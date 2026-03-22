CC = gcc
CFLAGS = -O2 -Wall -Wextra
LDFLAGS = -lraylib -lm

minesweeper: main.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f minesweeper

.PHONY: clean
