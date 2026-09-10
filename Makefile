CC = gcc

CFLAGS = -Wall -Wextra -g

SRC = \
	main.c \
	lexer/lexer.c \
	parser/parser.c \
	semantic/semantic.c \
	semantic/symbol_table.c \
	codegen/codegen.c

TARGET = eel

all: $(TARGET) eel-loader

$(TARGET):
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET)

eel-loader: loader/loader.c
	$(CC) $(CFLAGS) loader/loader.c -o eel-loader

run: all
	./$(TARGET) example.eel

clean:
	rm -f $(TARGET) eel-loader
