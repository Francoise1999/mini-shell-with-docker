CC = gcc
OBJ = minishell_plus.o
CFLAGS = -Wall -pthread
LIBS = -lreadline  
EXEC = minishell_plus
SRC = minishell_plus.c

all : $(EXEC)

$(EXEC): $(OBJ)
	$(CC) $(OBJ) -o $(EXEC) $(CFLAGS) $(LIBS)

clean:
	rm -f $(OBJ) $(EXEC)