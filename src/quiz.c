/* quiz.c - question generation for the arithmetics game. */
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "quiz.h"

#define LEN(a) (sizeof(a) / sizeof((a)[0]))

static long pick(long min, long max);

static const char ops[] = { OPADD, OPSUB, OPMUL, OPDIV };

/* Uniform integer in [min, max]. */
static long
pick(long min, long max)
{
	return min + (long)(random() % (max - min + 1));
}

void
seedquiz(void)
{
	srandom((unsigned int)(time(NULL) ^ (getpid() << 16)));
}

int
validop(char op)
{
	return op == OPADD || op == OPSUB || op == OPMUL ||
	       op == OPDIV || op == OPALL;
}

char
opsymbol(char op)
{
	switch (op) {
	case OPADD:
		return '+';
	case OPSUB:
		return '-';
	case OPMUL:
		return '*';
	case OPDIV:
		return '/';
	}
	return '?';
}

const char *
opname(char op)
{
	switch (op) {
	case OPADD:
		return "addition";
	case OPSUB:
		return "subtraction";
	case OPMUL:
		return "multiplication";
	case OPDIV:
		return "division";
	case OPALL:
		return "all operations";
	}
	return "unknown";
}

Question
mkquestion(char op, long min, long max)
{
	Question q;
	long a, b;

	if (op == OPALL)
		op = ops[random() % (long)LEN(ops)];
	a = pick(min, max);
	b = pick(min, max);
	q.op = op;
	q.rem = 0;
	switch (op) {
	case OPADD:
		q.lhs = a;
		q.rhs = b;
		q.ans = a + b;
		break;
	case OPSUB:
		/* Larger operand first, so the difference is never negative. */
		q.lhs = a > b ? a : b;
		q.rhs = a > b ? b : a;
		q.ans = q.lhs - q.rhs;
		break;
	case OPMUL:
		q.lhs = a;
		q.rhs = b;
		q.ans = a * b;
		break;
	default:	/* OPDIV */
		/* Larger operand first, so the quotient is at least 1; min is
		   above 0, so the divisor is never 0. */
		q.lhs = a > b ? a : b;
		q.rhs = a > b ? b : a;
		q.ans = q.lhs / q.rhs;
		q.rem = q.lhs % q.rhs;
		break;
	}
	return q;
}
