CC      ?= cc
CFLAGS  ?= -std=c11 -Wall -Wextra -O2
LDLIBS  := -lsqlite3

BIN := arithmetics
SRC := src/main.c src/db.c src/quiz.c
OBJ := $(SRC:.c=.o)

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ) $(LDLIBS)

src/main.o: src/main.c src/db.h src/quiz.h
src/db.o:   src/db.c   src/db.h src/quiz.h
src/quiz.o: src/quiz.c src/quiz.h

clean:
	rm -f $(OBJ) $(BIN)

.PHONY: all clean
