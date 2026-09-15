/* db.h - performance history kept in a SQLite database. */
#ifndef DB_H
#define DB_H

#include <sqlite3.h>

#include "quiz.h"

typedef struct db Db;
struct db {
	sqlite3 *conn;
	sqlite3_int64 sid;	/* row id of the session in progress */
};

/* Each function returns 0 on success and -1 on failure, after writing a
   diagnostic to stderr. */
int opendb(Db *db, const char *path);
int startsession(Db *db, char op, long min, long max, int rounds);
int addanswer(Db *db, int n, const Question *q, const Answer *a);
int endsession(Db *db, int played, int right, long ms);
void closedb(Db *db);

#endif /* DB_H */
