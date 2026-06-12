#ifndef LEXER_H
#define LEXER_H

typedef enum {
  TOKEN_ILLEGAL,
  TOKEN_EOF,

  // Keywords
  TOKEN_PROBE,
  TOKEN_IDENTIFIER,

  // braces
  TOKEN_LBRACE,
  TOKEN_RBRACE,

  // parenthesis
  TOKEN_LPAREN,
  TOKEN_RPAREN,

  // Literals
  TOKEN_STRING,
  TOKEN_NUMBER,

  // arthmetic
  TOKEN_ADD,
  TOKEN_SUBTRACT,
  TOKEN_MULTI,
  TOKEN_DIV,
  TOKEN_MOD,

  // assignment
  TOKEN_ASSIGN,

  // comparison
  TOKEN_GT,    // >
  TOKEN_LT,    // <
  TOKEN_EQ,    // ==
  TOKEN_NOTEQ, // !=
  TOKEN_GTE,   // >=
  TOKEN_LTE,   // <=
  TOKEN_SEMICOLON,

  // conditinals
  TOKEN_IF,
  TOKEN_ELIF,
  TOKEN_ELSE,

  // Loop
  TOKEN_REPEAT,

  // types
  TOKEN_INT,
  TOKEN_STRING_TYPE,
  TOKEN_MAP,
  TOKEN_COMMA,

  TOKEN_COUNT
} TokenType;

typedef struct {
  TokenType type;
  char literal[256];
} Token;

typedef struct {
  char *input;
  int position;
  int next_position;
  char ch;
  int line;
  int column;
} Lexer;

extern const char *token_type_to_string[TOKEN_COUNT];

void init_lexer(Lexer *l, char *input);
Token next_token(Lexer *l);
void report_error(Lexer *l, const char *msg);

#endif
