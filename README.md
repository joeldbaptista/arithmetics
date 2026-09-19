# Arithmetics

A re-interpretation of the old BSD `arithmetic` game, for practising mental
arithmetic. The program asks a series of questions over a chosen operation and
operand range, marks each answer, and records the whole session in a SQLite
database, so past performance can be reviewed later.

This project is just an excuse to play with SQLite C API. 

## Requirements

* A C11 compiler.
* SQLite 3, both the header `sqlite3.h` and the library `-lsqlite3`.

## Build

    make            # builds ./arithmetics
    make clean      # removes the objects and the binary

There is no install target, so copy the binary wherever it is wanted.

## Usage

    arithmetics -o <a|s|m|d|A> -m <min> -M <max> -r <rounds>

All four options are required.

| Option | Meaning |
| ------ | ------- |
| `-o`   | Operation: `a` add, `s` subtract, `m` multiply, `d` divide, `A` one of those four drawn at random on every round. |
| `-m`   | Smallest operand. It must be greater than 0. |
| `-M`   | Largest operand. It must be greater than `min`. |
| `-r`   | Number of rounds. It must be greater than 0. |
| `-h`   | Print the usage text and exit. |

The exit status is 0 after a normal session, 1 on a database failure, and 2 on
a usage error.

### Example session

    $ arithmetics -o A -m 2 -M 12 -r 3
    all operations, operands from 2 to 12, 3 rounds.
    1/3  10 / 6 = 1 4/6
      right (3.1 s)
    2/3  6 - 4 = 3
      wrong, the answer is 2 (1.8 s)
    3/3  3 + 11 = 14
      right (2.2 s)

    2 of 3 correct (67%), 7.1 s total, 2.4 s per question.

## Answering

Answers are read from standard input, one per line.

* Addition, subtraction and multiplication take a plain integer.
* Division takes an integer division followed by its remainder, written as
  `quotient remainder/divisor`. For example, `12 / 5` is answered `2 2/5`.
  Any fraction of equal value is accepted, so `10 / 4` takes `2 1/2`, `2 2/4`
  and the improper form `5/2` alike. A denominator of 0 is always wrong.
  When the division is exact the quotient alone is enough, so `9 / 3` is
  answered `3`, although `3 0/3` is also accepted.
  The program shows the expected answer reduced to lowest terms, so a missed
  `9 / 6` is reported as `1 1/2`.
* An empty line, or any reply that cannot be read as a number, skips the round.
  The round still counts as played and as wrong.
* `q`, `quit`, or end of input ends the session early. The rounds already
  played are kept.

## Questions

Both operands are drawn uniformly from the range `[min, max]`. Subtraction and
division then put the larger operand first. So a difference is never negative,
a quotient is always at least 1, and the divisor is never 0, because `min` is
above 0. Addition and multiplication use the operands in the order drawn.

Under `-o A` the operation is drawn once per round, with the four operations
equally likely.

## Database

Past performance is stored in `database/db.sqlite3`, relative to the working
directory. The program creates the directory and the schema on first run, and
appends to them afterwards.

`sessions` holds one row per run:

| Column | Meaning |
| ------ | ------- |
| `id` | Row identifier. |
| `started_at`, `finished_at` | ISO-8601 timestamps in UTC. |
| `operation` | The `-o` argument, `A` included. |
| `min_operand`, `max_operand` | The `-m` and `-M` arguments. |
| `rounds` | The `-r` argument, that is, the rounds requested. |
| `rounds_played` | Rounds actually played, which is lower after an early quit. |
| `correct` | Number of correct answers. |
| `total_ms` | Time spent answering, in milliseconds. |

`answers` holds one row per question:

| Column | Meaning |
| ------ | ------- |
| `id` | Row identifier. |
| `session_id` | The session the question belongs to. |
| `round_no` | Position of the question within the session. |
| `asked_at` | ISO-8601 timestamp in UTC. |
| `operation` | The operation actually drawn, so never `A`. |
| `left_operand`, `right_operand` | The operands, as shown. |
| `expected`, `expected_rem` | The correct answer, and its remainder over `right_operand`, unreduced and 0 outside division. |
| `given`, `given_rem`, `given_den` | The answer entered, as its whole part, remainder and denominator. The denominator repeats the divisor outside division and whenever no fraction was typed. All three are `NULL` when the round was skipped. |
| `correct` | 1 when the answer matched, 0 otherwise. |
| `elapsed_ms` | Time taken to answer, in milliseconds. |

### Reviewing performance

Accuracy and speed per operation:

    SELECT operation,
           COUNT(*) AS asked,
           SUM(correct) AS correct_n,
           ROUND(100.0 * SUM(correct) / COUNT(*), 1) AS pct,
           ROUND(AVG(elapsed_ms) / 1000.0, 1) AS avg_s
    FROM answers
    GROUP BY operation;

The ten questions that took longest:

    SELECT operation, left_operand, right_operand, elapsed_ms
    FROM answers
    ORDER BY elapsed_ms DESC
    LIMIT 10;

Accuracy session by session:

    SELECT id, started_at, operation, rounds_played, correct
    FROM sessions
    ORDER BY id;

## Layout

    src/main.c   option parsing, the round loop, the summary
    src/quiz.c   question generation
    src/db.c     the SQLite schema and the writes
    STYLE.md     the C style the sources follow
