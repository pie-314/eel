CC = gcc

CFLAGS = -Wall -Wextra -g

SRC = \
	main.c \
	lexer/lexer.c \
	parser/parser.c \
	semantic/semantic.c \
	semantic/symbol_table.c \
	codegen/codegen.c \
	loader/loader.c

TARGET = eel

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET)

eel-loader: loader/loader.c
	$(CC) $(CFLAGS) -DLOADER_STANDALONE loader/loader.c -o eel-loader

clean:
	rm -f $(TARGET) eel-loader
