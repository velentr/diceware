/**
 * \file diceware.h
 */

#ifndef _DICEWARE_H_
#define _DICEWARE_H_


#include <sqlite3.h>

#define DICEWARE_VSN_MAJOR 0
#define DICEWARE_VSN_MINOR 0

/**
 * Handle for the diceware word database.
 */
struct diceware
{
    sqlite3 *db;            /**< Active connection to the database file. */
    sqlite3_stmt *insert;   /**< Statement for inserting words. */
    sqlite3_stmt *query;    /**< Statement for retrieving words. */
};

int dw_open(struct diceware *dw, const char *path);
void dw_close(struct diceware *dw);
int dw_create(struct diceware *dw, const char *db_path, const char *word_path);
int dw_generate(struct diceware *dw, FILE *output, size_t nwords);


#endif /* end of include guard: _DICEWARE_H_ */

