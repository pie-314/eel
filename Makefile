CC = gcc

CFLAGS = -Wall -Wextra -g

SRC = \
	main.c \
	lexer/lexer.c \
	parser/parser.c

TARGET = eel

all:
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET)

run: all
	./$(TARGET)

clean:
	rm -f $(TARGET)
