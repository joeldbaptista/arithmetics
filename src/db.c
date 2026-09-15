/* db.c - performance history kept in a SQLite database. */
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "db.h"

#define PATHSZ 1024
#define TIMESZ 32

static const char schema[] =
	"PRAGMA foreign_keys = ON;"
	"CREATE TABLE IF NOT EXISTS sessions ("
	"  id            INTEGER PRIMARY KEY,"
	"  started_at    TEXT    NOT NULL,"
	"  finished_at   TEXT,"
	"  operation     TEXT    NOT NULL,"
	"  min_operand   INTEGER NOT NULL,"
	"  max_operand   INTEGER NOT NULL,"
	"  rounds        INTEGER NOT NULL,"
	"  rounds_played INTEGER NOT NULL DEFAULT 0,"
	"  correct       INTEGER NOT NULL DEFAULT 0,"
	"  total_ms      INTEGER NOT NULL DEFAULT 0"
	");"
	"CREATE TABLE IF NOT EXISTS answers ("
	"  id            INTEGER PRIMARY KEY,"
	"  session_id    INTEGER NOT NULL"
	"                REFERENCES sessions(id) ON DELETE CASCADE,"
	"  round_no      INTEGER NOT NULL,"
	"  asked_at      TEXT    NOT NULL,"
	"  operation     TEXT    NOT NULL,"
	"  left_operand  INTEGER NOT NULL,"
	"  right_operand INTEGER NOT NULL,"
	"  expected      INTEGER NOT NULL,"
	"  expected_rem  INTEGER NOT NULL DEFAULT 0,"
	"  given         INTEGER,"
	"  given_rem     INTEGER,"
	"  correct       INTEGER NOT NULL,"
	"  elapsed_ms    INTEGER NOT NULL"
	");"
	"CREATE INDEX IF NOT EXISTS answers_session_idx"
	" ON answers(session_id);";

/* Remainder columns are newer than the first schema, so add them to a
   database written before they existed. The error of an existing column is
   expected and therefore ignored. */
static const char migration[] =
	"ALTER TABLE answers ADD COLUMN expected_rem INTEGER NOT NULL"
	" DEFAULT 0;"
	"ALTER TABLE answers ADD COLUMN given_rem INTEGER;";

static void nowstamp(char *buf, size_t sz);
static int mkparents(const char *path);

/* Current UTC time as an ISO-8601 string. */
static void
nowstamp(char *buf, size_t sz)
{
	struct tm tm;
	time_t t;

	t = time(NULL);
	gmtime_r(&t, &tm);
	strftime(buf, sz, "%Y-%m-%dT%H:%M:%SZ", &tm);
}

/* Create every missing directory component of a file path. */
static int
mkparents(const char *path)
{
	char buf[PATHSZ];
	size_t i;

	if (strlen(path) >= sizeof(buf)) {
		fprintf(stderr, "arithmetics: database path is too long\n");
		return -1;
	}
	strcpy(buf, path);
	for (i = 1; buf[i]; i++) {
		if (buf[i] != '/')
			continue;
		buf[i] = '\0';
		if (mkdir(buf, 0755) < 0 && errno != EEXIST) {
			fprintf(stderr, "arithmetics: cannot create %s: %s\n",
			        buf, strerror(errno));
			return -1;
		}
		buf[i] = '/';
	}
	return 0;
}

int
opendb(Db *db, const char *path)
{
	char *err;

	db->conn = NULL;
	db->sid = 0;
	err = NULL;
	if (mkparents(path) < 0)
		return -1;
	if (sqlite3_open(path, &db->conn) != SQLITE_OK) {
		fprintf(stderr, "arithmetics: cannot open %s: %s\n", path,
		        sqlite3_errmsg(db->conn));
		return -1;
	}
	if (sqlite3_exec(db->conn, schema, NULL, NULL, &err) != SQLITE_OK) {
		fprintf(stderr, "arithmetics: cannot create schema: %s\n", err);
		sqlite3_free(err);
		return -1;
	}
	sqlite3_exec(db->conn, migration, NULL, NULL, NULL);
	return 0;
}

int
startsession(Db *db, char op, long min, long max, int rounds)
{
	static const char sql[] =
		"INSERT INTO sessions (started_at, operation, min_operand,"
		" max_operand, rounds) VALUES (?, ?, ?, ?, ?);";
	sqlite3_stmt *st;
	char stamp[TIMESZ];
	char ops[2];
	int rc;

	st = NULL;
	nowstamp(stamp, sizeof(stamp));
	ops[0] = op;
	ops[1] = '\0';
	if (sqlite3_prepare_v2(db->conn, sql, -1, &st, NULL) != SQLITE_OK) {
		fprintf(stderr, "arithmetics: %s\n", sqlite3_errmsg(db->conn));
		return -1;
	}
	sqlite3_bind_text(st, 1, stamp, -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(st, 2, ops, -1, SQLITE_TRANSIENT);
	sqlite3_bind_int64(st, 3, min);
	sqlite3_bind_int64(st, 4, max);
	sqlite3_bind_int(st, 5, rounds);
	rc = sqlite3_step(st);
	sqlite3_finalize(st);
	if (rc != SQLITE_DONE) {
		fprintf(stderr, "arithmetics: cannot start session: %s\n",
		        sqlite3_errmsg(db->conn));
		return -1;
	}
	db->sid = sqlite3_last_insert_rowid(db->conn);
	return 0;
}

int
addanswer(Db *db, int n, const Question *q, const Answer *a)
{
	static const char sql[] =
		"INSERT INTO answers (session_id, round_no, asked_at,"
		" operation, left_operand, right_operand, expected,"
		" expected_rem, given, given_rem, correct, elapsed_ms)"
		" VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";
	sqlite3_stmt *st;
	char stamp[TIMESZ];
	char ops[2];
	int rc;

	st = NULL;
	nowstamp(stamp, sizeof(stamp));
	ops[0] = q->op;
	ops[1] = '\0';
	if (sqlite3_prepare_v2(db->conn, sql, -1, &st, NULL) != SQLITE_OK) {
		fprintf(stderr, "arithmetics: %s\n", sqlite3_errmsg(db->conn));
		return -1;
	}
	sqlite3_bind_int64(st, 1, db->sid);
	sqlite3_bind_int(st, 2, n);
	sqlite3_bind_text(st, 3, stamp, -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(st, 4, ops, -1, SQLITE_TRANSIENT);
	sqlite3_bind_int64(st, 5, q->lhs);
	sqlite3_bind_int64(st, 6, q->rhs);
	sqlite3_bind_int64(st, 7, q->ans);
	sqlite3_bind_int64(st, 8, q->rem);
	if (a->given) {
		sqlite3_bind_int64(st, 9, a->ans);
		sqlite3_bind_int64(st, 10, a->rem);
	} else {
		sqlite3_bind_null(st, 9);
		sqlite3_bind_null(st, 10);
	}
	sqlite3_bind_int(st, 11, a->right);
	sqlite3_bind_int64(st, 12, a->ms);
	rc = sqlite3_step(st);
	sqlite3_finalize(st);
	if (rc != SQLITE_DONE) {
		fprintf(stderr, "arithmetics: cannot record answer: %s\n",
		        sqlite3_errmsg(db->conn));
		return -1;
	}
	return 0;
}

int
endsession(Db *db, int played, int right, long ms)
{
	static const char sql[] =
		"UPDATE sessions SET finished_at = ?, rounds_played = ?,"
		" correct = ?, total_ms = ? WHERE id = ?;";
	sqlite3_stmt *st;
	char stamp[TIMESZ];
	int rc;

	st = NULL;
	nowstamp(stamp, sizeof(stamp));
	if (sqlite3_prepare_v2(db->conn, sql, -1, &st, NULL) != SQLITE_OK) {
		fprintf(stderr, "arithmetics: %s\n", sqlite3_errmsg(db->conn));
		return -1;
	}
	sqlite3_bind_text(st, 1, stamp, -1, SQLITE_TRANSIENT);
	sqlite3_bind_int(st, 2, played);
	sqlite3_bind_int(st, 3, right);
	sqlite3_bind_int64(st, 4, ms);
	sqlite3_bind_int64(st, 5, db->sid);
	rc = sqlite3_step(st);
	sqlite3_finalize(st);
	if (rc != SQLITE_DONE) {
		fprintf(stderr, "arithmetics: cannot finish session: %s\n",
		        sqlite3_errmsg(db->conn));
		return -1;
	}
	return 0;
}

void
closedb(Db *db)
{
	if (db->conn) {
		sqlite3_close(db->conn);
		db->conn = NULL;
	}
}
