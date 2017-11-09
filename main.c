/**
 * \file main.c
 *
 * \brief Parse command-line arguments for the diceware module.
 *
 * \author Brian Kubisiak
 */

#include <err.h>
#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "diceware.h"

#define USAGE_STRING \
	"usage: %s [-d <dbfile>] [-h] [-n <num>] [-v] [-w <wordlist>]\n"
#define VSN_STRING   "Diceware v%d.%d, Copyright (C) 2017 Brian Kubisiak\n"

int main(int argc, char *argv[])
{
    struct diceware dw;
    int arg, rc;
    unsigned long len;
    char *db_file, *word_file;
    char *endptr;
    char default_path[128];
    char *home;

    /* Generate the default path in the user's home directory. If the
     * environment is not set correctly, use the current directory.
     */
    home = getenv("HOME");
    if (home == NULL)
    {
        home = ".";
    }
    snprintf(default_path, sizeof(default_path), "%s/.diceware.db", home);

    /* Set defaults */
    len = 4ul;
    db_file = default_path;
    word_file = NULL;

    /* Turn off automatic logging; we will print errors on our own. */
    opterr = 0;
    while ((arg = getopt(argc, argv, "d:hn:vw:")) != -1)
    {
        switch (arg)
        {
        /* Set the path to the database file */
        case 'd':
            db_file = optarg;
            break;
	/* Print usage message and exit. */
	case 'h':
	    fprintf(stderr, USAGE_STRING, argv[0]);
	    exit(EXIT_SUCCESS);
	    break;
        /* Set the number of words to use in the passphrase. */
        case 'n':
            len = strtoul(optarg, &endptr, 10);
            if (*endptr != '\0')
            {
                fprintf(stderr, USAGE_STRING, argv[0]);
		exit(EXIT_FAILURE);
            }
            break;
        /* Print version info and exit. */
        case 'v':
	    fprintf(stderr, VSN_STRING, DICEWARE_VSN_MAJOR, DICEWARE_VSN_MINOR);
	    exit(EXIT_SUCCESS);
            break;
        /* Set the path to the wordlist for setting up a new database. */
        case 'w':
            word_file = optarg;
            break;
        default:
            fprintf(stderr, USAGE_STRING, argv[0]);
	    exit(EXIT_FAILURE);
            break;
        }
    }

    /* Create a new database if a word list was specified; otherwise, open a
     * connection to an existing database.
     */
    if (word_file == NULL)
    {
        rc = dw_open(&dw, db_file);
    }
    else
    {
        rc = dw_create(&dw, db_file, word_file);
    }

    if (rc < 0)
    {
        rc = EXIT_FAILURE;
        goto main_exit;
    }

    rc = dw_generate(&dw, stdout, len);
    if (rc < 0)
    {
        rc = EXIT_FAILURE;
        goto main_cleanup;
    }

main_cleanup:
    dw_close(&dw);
main_exit:
    return EXIT_SUCCESS;
}

