/* arithmetics - mental arithmetic practice, after the BSD game. */
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "db.h"
#include "quiz.h"

#define DBPATH "database/db.sqlite3"
#define LINESZ 256
#define ANSSZ  64

static void usage(FILE *out);
static long nowms(void);
static char *trim(char *s);
static int parsenum(const char *s, long *v);
static int parseans(const char *s, const Question *q, Answer *a);
static void fmtans(char *buf, size_t sz, const Question *q);
static int ask(const Question *q, int n, long rounds, Answer *a);

static void
usage(FILE *out)
{
	fprintf(out,
	        "usage: arithmetics -o <a|s|m|d|A> -m <min> -M <max>"
	        " -r <rounds>\n"
	        "  -o  operation: a add, s subtract, m multiply, d divide,\n"
	        "      A one of the four drawn at random each round\n"
	        "  -m  smallest operand, greater than 0\n"
	        "  -M  largest operand, greater than min\n"
	        "  -r  number of rounds, greater than 0\n"
	        "A division is answered as quotient and remainder, so 12 / 5"
	        " is \"2 2/5\".\n"
	        "Answers are read from standard input; an empty line skips a"
	        " round and\n\"q\" ends the session early.\n");
}

/* Monotonic clock in milliseconds. */
static long
nowms(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

/* Strip leading and trailing white space, in place. */
static char *
trim(char *s)
{
	char *p;

	while (*s && isspace((unsigned char)*s))
		s++;
	p = s + strlen(s);
	while (p > s && isspace((unsigned char)p[-1]))
		p--;
	*p = '\0';
	return s;
}

/* Read the whole string as a decimal integer. */
static int
parsenum(const char *s, long *v)
{
	char *end;
	long n;

	errno = 0;
	n = strtol(s, &end, 10);
	if (end == s || *end || errno == ERANGE)
		return -1;
	*v = n;
	return 0;
}

/* Read the player's reply. A division takes a mixed number, written as
   "quotient remainder/divisor"; the whole part alone is accepted when the
   remainder is 0, and the fraction alone when the quotient is 0. Every
   other operation takes a plain integer. */
static int
parseans(const char *s, const Question *q, Answer *a)
{
	long w, r, d;
	int n;

	a->ans = 0;
	a->rem = 0;
	a->den = 0;
	if (q->op != OPDIV)
		return parsenum(s, &a->ans);
	n = 0;
	if (sscanf(s, "%ld %ld / %ld %n", &w, &r, &d, &n) == 3 && !s[n]) {
		a->ans = w;
		a->rem = r;
		a->den = d;
		return 0;
	}
	n = 0;
	if (sscanf(s, "%ld / %ld %n", &r, &d, &n) == 2 && !s[n]) {
		a->rem = r;
		a->den = d;
		return 0;
	}
	if (parsenum(s, &a->ans) < 0)
		return -1;
	a->den = q->rhs;	/* no fraction written means no remainder */
	return 0;
}

/* Write the expected answer of a question in the form the player types. */
static void
fmtans(char *buf, size_t sz, const Question *q)
{
	if (q->op == OPDIV && q->rem)
		snprintf(buf, sz, "%ld %ld/%ld", q->ans, q->rem, q->rhs);
	else
		snprintf(buf, sz, "%ld", q->ans);
}

/* Put one question and read the reply. Returns 1 when the round was
   played, and 0 when the player ended the session. */
static int
ask(const Question *q, int n, long rounds, Answer *a)
{
	char line[LINESZ];
	char buf[ANSSZ];
	char *s;
	long t;

	printf("%d/%ld  %ld %c %ld = ", n, rounds, q->lhs, opsymbol(q->op),
	       q->rhs);
	fflush(stdout);
	t = nowms();
	if (!fgets(line, sizeof(line), stdin)) {
		putchar('\n');
		return 0;
	}
	a->ms = nowms() - t;
	s = trim(line);
	if (!strcmp(s, "q") || !strcmp(s, "quit"))
		return 0;
	a->given = parseans(s, q, a) == 0;
	a->right = a->given && a->ans == q->ans && a->rem == q->rem &&
	           (q->op != OPDIV || a->den == q->rhs);
	if (a->right) {
		printf("  right (%.1f s)\n", a->ms / 1000.0);
	} else {
		fmtans(buf, sizeof(buf), q);
		printf("  wrong, the answer is %s (%.1f s)\n", buf,
		       a->ms / 1000.0);
	}
	return 1;
}

int
main(int argc, char *argv[])
{
	Question q;
	Answer a;
	Db db;
	long min, max, rounds, ms;
	int i, opt, played, right, ret;
	char op;

	op = '\0';
	min = max = rounds = 0;
	ms = 0;
	played = right = ret = 0;
	while ((opt = getopt(argc, argv, "o:m:M:r:h")) != -1) {
		switch (opt) {
		case 'o':
			if (strlen(optarg) != 1 || !validop(optarg[0])) {
				fprintf(stderr, "arithmetics: operation must be"
				        " one of a, s, m, d, A\n");
				return 2;
			}
			op = optarg[0];
			break;
		case 'm':
			if (parsenum(optarg, &min) < 0) {
				fprintf(stderr,
				        "arithmetics: -m needs an integer\n");
				return 2;
			}
			break;
		case 'M':
			if (parsenum(optarg, &max) < 0) {
				fprintf(stderr,
				        "arithmetics: -M needs an integer\n");
				return 2;
			}
			break;
		case 'r':
			if (parsenum(optarg, &rounds) < 0) {
				fprintf(stderr,
				        "arithmetics: -r needs an integer\n");
				return 2;
			}
			break;
		case 'h':
			usage(stdout);
			return 0;
		default:
			usage(stderr);
			return 2;
		}
	}
	if (!op || !min || !max || !rounds) {
		fprintf(stderr, "arithmetics: -o, -m, -M and -r are all"
		        " required\n");
		usage(stderr);
		return 2;
	}
	if (min <= 0) {
		fprintf(stderr, "arithmetics: min must be greater than 0\n");
		return 2;
	}
	if (max <= min) {
		fprintf(stderr, "arithmetics: max must be greater than min\n");
		return 2;
	}
	if (rounds <= 0 || rounds > INT_MAX) {
		fprintf(stderr, "arithmetics: rounds must be greater than 0\n");
		return 2;
	}
	if (opendb(&db, DBPATH) < 0)
		return 1;
	if (startsession(&db, op, min, max, (int)rounds) < 0) {
		ret = 1;
		goto done;
	}
	seedquiz();
	printf("%s, operands from %ld to %ld, %ld rounds.\n", opname(op), min,
	       max, rounds);
	for (i = 1; i <= (int)rounds; i++) {
		q = mkquestion(op, min, max);
		if (!ask(&q, i, rounds, &a))
			break;
		played++;
		right += a.right;
		ms += a.ms;
		if (addanswer(&db, i, &q, &a) < 0) {
			ret = 1;
			break;
		}
	}
	if (endsession(&db, played, right, ms) < 0)
		ret = 1;
done:
	closedb(&db);
	printf("\n%d of %d correct", right, played);
	if (played)
		printf(" (%.0f%%), %.1f s total, %.1f s per question",
		       100.0 * right / played, ms / 1000.0,
		       ms / 1000.0 / played);
	printf(".\n");
	return ret;
}
