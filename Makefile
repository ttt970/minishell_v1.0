CC      = gcc
CFLAGS  = -Wall -Wextra -g -O0 -D_GNU_SOURCE -std=gnu11
OBJ     = minishell
SRC     = main.c show.c execcmd.c

$(OBJ): $(SRC) head.h show.h execcmd.h
	$(CC) $(CFLAGS) $(SRC) -o $@

.PHONY: clean
clean:
	rm -f $(OBJ)
