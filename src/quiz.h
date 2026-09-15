/* quiz.h - question generation for the arithmetics game. */
#ifndef QUIZ_H
#define QUIZ_H

enum {
	OPADD = 'a',
	OPSUB = 's',
	OPMUL = 'm',
	OPDIV = 'd',
	OPALL = 'A'
};

typedef struct question Question;
struct question {
	char op;	/* never OPALL: a round has one concrete operation */
	long lhs;
	long rhs;
	long ans;	/* sum, difference, product, or quotient */
	long rem;	/* remainder, always 0 unless op is OPDIV */
};

typedef struct answer Answer;
struct answer {
	int given;	/* 1 when the player entered a parsable answer */
	int right;	/* 1 when the answer matches the question */
	long ans;
	long rem;
	long den;	/* denominator entered, 0 unless op is OPDIV */
	long ms;	/* time taken to answer */
};

void seedquiz(void);
int validop(char op);
char opsymbol(char op);
const char *opname(char op);
Question mkquestion(char op, long min, long max);

#endif /* QUIZ_H */
