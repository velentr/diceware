/**
 * \file diceware.c
 *
 * \brief Module for generating diceware passphrases.
 *
 * A diceware word list is constructed using an SQLite database from a given
 * wordlist, and is stored at a given location. The word list from which the
 * database is constructed should be a sequence of tuples <tt>number word</tt>
 * separated by whitespace, e.g.:
 *
 * 11111 word
 * 11112 other
 * 11113 ...
 *
 * Each number should be unique and should only contain the digits 1-6. Words
 * should also be unique.
 *
 * To use this module, begin by calling either #dw_create() to construct a new
 * database or #dw_open() to open a connection to an existing database. Then,
 * call #dw_generate() to generate a passphrase and print it to the file of your
 * choice. Last, call #dw_close() to cleanup the database. Note that #dw_close()
 * must always be called to cleanup memory whenever #dw_open() or #dw_create()
 * succeed.
 *
 * For any of these functions, if an error is encountered, a message describing
 * the issue is printed to \c stderr (along with some diagnostic information),
 * and a negative (or \c NULL) value is returned.
 *
 * \author Brian Kubisiak
 */

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* If we are linux, need to include BSD stdlib for arc4random functions. On
 * other unixen, these functions should already be in stdlib.h. If you are using
 * windows, you should switch to linux.
 */
#ifdef __linux__
typedef unsigned char u_char;
#include <bsd/stdlib.h>
#endif

#include <sqlite3.h>

#include "diceware.h"

/**
 * Number of dice used for generating indices.
 */
#define NDICE 5

/**
 * Maximum value a die can roll.
 */
#define MAX_DIE_ROLL 6

/* SQL for interacting with the database. */
#define CREATE_TABLES       "CREATE TABLE diceware (id INTEGER PRIMARY KEY, " \
                            "word TEXT);"
#define BEGIN_TRANSACTION   "BEGIN TRANSACTION;"
#define END_TRANSACTION     "END TRANSACTION;"
#define UNDO_TRANSACTION    "ROLLBACK TRANSACTION;"
#define GET_WORD            "SELECT word FROM diceware WHERE id = ?;"
#define INSERT_WORD         "INSERT INTO diceware (id, word) VALUES (?, ?);"

static int _dw_get_word(struct diceware *dw, unsigned idx, char *out,
        size_t len)
{
    int rc;
    const char *word;

    if (dw->query == NULL)
    {
        rc = sqlite3_prepare_v2(dw->db, GET_WORD, -1, &dw->query, NULL);
        if (rc != SQLITE_OK)
        {
            fprintf(stderr, "sqlite3_prepare_v2(%s): %s\n", GET_WORD,
                    sqlite3_errmsg(dw->db));
            return -1;
        }
    }
    else
    {
        sqlite3_reset(dw->query);
    }

    rc = sqlite3_bind_int(dw->query, 1, idx);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "sqlite3_bind_int: %s\n", sqlite3_errstr(rc));
        return -1;
    }

    do
    {
        rc = sqlite3_step(dw->query);
    } while (rc == SQLITE_BUSY);

    if (rc != SQLITE_ROW)
    {
        if (rc == SQLITE_DONE)
        {
            fprintf(stderr, "incomplete database\n");
        }
        else
        {
            fprintf(stderr, "sqlite3_step(%s): %s\n", GET_WORD,
                    sqlite3_errmsg(dw->db));
        }
        return -1;
    }

    word = (const char *)sqlite3_column_text(dw->query, 0);
    if (word == NULL)
    {
        fprintf(stderr, "sqlite3_column_text(%s): %s\n", GET_WORD,
                sqlite3_errmsg(dw->db));
        return -1;
    }

    strncpy(out, word, len);

    return 0;
}

static int _dw_insert(struct diceware *dw, int index, const char *word)
{
    int rc;

    if (dw->insert == NULL)
    {
        rc = sqlite3_prepare_v2(dw->db, INSERT_WORD, -1, &dw->insert, NULL);
        if (rc != SQLITE_OK)
        {
            fprintf(stderr, "sqlite3_prepare_v2(%s): %s\n", INSERT_WORD,
                    sqlite3_errmsg(dw->db));
            return -1;
        }
    }
    else
    {
        sqlite3_reset(dw->insert);
    }

    rc = sqlite3_bind_int(dw->insert, 1, index);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "sqlite3_bind_int: %s\n", sqlite3_errstr(rc));
        return -1;
    }

    rc = sqlite3_bind_text(dw->insert, 2, word, -1, SQLITE_STATIC);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "sqlite3_bind_text: %s\n", sqlite3_errstr(rc));
        return -1;
    }

    do
    {
        rc = sqlite3_step(dw->insert);
    } while (rc == SQLITE_BUSY);

    if (rc != SQLITE_DONE)
    {
        fprintf(stderr, "sqlite3_step(%s): %s\n", GET_WORD,
                sqlite3_errmsg(dw->db));
        return -1;
    }

    return 0;
}

static int _dw_populate(struct diceware *dw, const char *path)
{
    FILE *input;
    int index;
    char word[64];
    int rc, count;

    /* Open the input file, checking for errors. */
    input = fopen(path, "r");
    if (input == NULL)
    {
        fprintf(stderr, "fopen(%s): %s\n", path, strerror(errno));
        return -1;
    }

    count = 0;
    for (;;)
    {
        /* If can't scan the correct input, then the parser has failed. */
        rc = fscanf(input, "%d %63s", &index, word);
        if (rc == 2)
        {
            /* Attempt to insert the new database entry; break on failure. */
            rc = _dw_insert(dw, index, word);
            if (rc == 0)
            {
                count++;
            }
            else
            {
                break;
            }
        }
        else
        {
            break;
        }
    }

    /* Input file was not complete/some other error occurred. */
    if (count < 7776)
    {
        /* Figure out which error occurred and log the appropriate message. */
        if (ferror(input))
        {
            fprintf(stderr, "fscanf(%s): %s\n", path, strerror(errno));
        }
        else if (feof(input))
        {
            fprintf(stderr, "too few diceware entries: %s\n", path);
        }
        else
        {
            fprintf(stderr, "invalid diceware file: %s\n", path);
        }

        return -1;
    }
    return 0;
}

void dw_close(struct diceware *dw)
{
    sqlite3_close(dw->db);

    if (dw->insert != NULL)
    {
        sqlite3_finalize(dw->insert);
    }

    if (dw->query != NULL)
    {
        sqlite3_finalize(dw->query);
    }
}

int dw_create(struct diceware *dw, const char *db_path, const char *word_path)
{
    char *errmsg;
    int rc;

    rc = dw_open(dw, db_path);
    if (rc < 0)
    {
        return rc;
    }

    /* Start a new transaction that adds all tables and entries at once. These
     * must be atomic since the generator expects exactly 6^5 entries.
     */
    rc = sqlite3_exec(dw->db, BEGIN_TRANSACTION, NULL, NULL, &errmsg);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "sqlite3_exec(%s): %s\n", BEGIN_TRANSACTION, errmsg);
        sqlite3_free(errmsg);
        return -1;
    }

    rc = sqlite3_exec(dw->db, CREATE_TABLES, NULL, NULL, &errmsg);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "sqlite3_exec(%s): %s\n", CREATE_TABLES, errmsg);
        sqlite3_free(errmsg);
        dw_close(dw);
        return -1;
    }

    rc = _dw_populate(dw, word_path);
    if (rc < 0)
    {
        /* Rollback the transaction; we don't want an incomplete database. */
        do
        {
            rc = sqlite3_exec(dw->db, UNDO_TRANSACTION, NULL, NULL, &errmsg);
        } while (rc == SQLITE_BUSY);

        /* Rollback failed; something has gone horribly wrong. */
        if (rc != SQLITE_OK)
        {
            fprintf(stderr, "sqlite3_exec(%s): %s\n", UNDO_TRANSACTION, errmsg);
            sqlite3_free(errmsg);
            return -1;
        }

        dw_close(dw);
        return -1;
    }
    /* Finished all diceware entries; commit to the database. */
    else
    {
        /* Attempt to commit the transaction. */
        do
        {
            rc = sqlite3_exec(dw->db, END_TRANSACTION, NULL, NULL, &errmsg);
        } while (rc == SQLITE_BUSY);

        /* Commit to DB failed; return the error and let the higher layer handle
         * it. */
        if (rc != SQLITE_OK)
        {
            fprintf(stderr, "sqlite3_exec(%s): %s\n", END_TRANSACTION, errmsg);
            sqlite3_free(errmsg);

            dw_close(dw);
            return -1;
        }
    }

    return 0;
}

int dw_open(struct diceware *dw, const char *path)
{
    int rc;
    sqlite3 *db;

    rc = sqlite3_open(path, &db);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "sqlite3_open(%s): %s\n", path, sqlite3_errmsg(db));
        return -1;
    }

    dw->db = db;
    dw->insert = NULL;
    dw->query = NULL;

    return 0;
}

/**
 * \brief Generate a diceware passphrase.
 *
 * Using the diceware database \p dw, generate a passphrase using \p nwords
 * words, printing the result top output. If any errors occurs, returns -1.
 * Else, returns 0. Note that the underlying RNG is the cryptographically-secure
 * BSD \c arc4random_uniform function.
 *
 * \param dw Diceware database to use for words.
 * \param output File stream to which the result is written.
 * \param nwords Number of words to use for the passphrase.
 *
 * \return Returns 0 on successful generation. On failure, prints an error
 * message to stderr and returns -1.
 */
int dw_generate(struct diceware *dw, FILE *output, size_t nwords)
{
    size_t i, j;
    uint32_t n;
    char word[32];
    int rc;

    /* Generate each word separately. */
    for (i = 0; i < nwords; i++)
    {
        /* Generate the dice rolls needed for selecting a single word; store the
         * dice rolls in a base-10 number, with each decimal digit representing
         * a single die.
         */
        n = 0;
        for (j = 0; j < NDICE; j++)
        {
            n *= 10;
            n += arc4random_uniform(MAX_DIE_ROLL) + 1;
        }

        /* Get the random word and print it to the given stream. */
        rc = _dw_get_word(dw, n, word, sizeof(word));
        if (rc != 0)
        {
            return -1;
        }

        rc = fprintf(output, "%s ", word);
        if (rc < 0)
        {
            fprintf(stderr, "fprintf: %s\n", strerror(errno));
            return -1;
        }
    }

    rc = fprintf(output, "\n");
    if (rc < 0)
    {
        fprintf(stderr, "fprintf: %s\n", strerror(errno));
        return -1;
    }

    return 0;
}

