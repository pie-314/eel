#include <ctype.h>
#include <stdio.h>
#include <string.h>

// TODO
// * Add comparison operators support
// * Add loops
// * Add lex
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

  TOKEN_COUNT
} TokenType;

const char *token_type_to_string[TOKEN_COUNT] = {[TOKEN_ILLEGAL] = "ILLEGAL",
                                                 [TOKEN_EOF] = "EOF",
                                                 [TOKEN_PROBE] = "PROBE",
                                                 [TOKEN_IDENTIFIER] =
                                                     "IDENTIFIER",
                                                 // brances
                                                 [TOKEN_LBRACE] = "{",
                                                 [TOKEN_RBRACE] = "}",

                                                 // parenthesis
                                                 [TOKEN_LPAREN] = "(",
                                                 [TOKEN_RPAREN] = ")",

                                                 // literals
                                                 [TOKEN_STRING] = "STRING",
                                                 [TOKEN_NUMBER] = "NUMBER",

                                                 // arthmetic
                                                 [TOKEN_ADD] = "+",
                                                 [TOKEN_SUBTRACT] = "-",
                                                 [TOKEN_MULTI] = "*",
                                                 [TOKEN_DIV] = "/",
                                                 [TOKEN_MOD] = "%",

                                                 // comparison
                                                 [TOKEN_GT] = ">",
                                                 [TOKEN_LT] = "<",
                                                 [TOKEN_EQ] = "==",
                                                 [TOKEN_NOTEQ] = "!=",
                                                 [TOKEN_GTE] = ">=",
                                                 [TOKEN_LTE] = "<=",
                                                 [TOKEN_SEMICOLON] = ";",

                                                 // conditinals
                                                 [TOKEN_IF] = "IF",
                                                 [TOKEN_ELIF] = "ELIF",
                                                 [TOKEN_ELSE] = "ELSE",

                                                 // Loop
                                                 [TOKEN_REPEAT] = "REPEAT",

                                                 [TOKEN_ASSIGN] = "="};

typedef struct {
  TokenType type;
  char literal[256];
} Token;

typedef struct {
  char *input;
  int position;
  int next_position;
  char ch;
} Lexer;

char *source = "probe sys_execve {\n"
               "    print(\"exec called\")\n"
               "    x = 10\n"
               "}";

void read_char(Lexer *l) {
  if (l->input[l->position] == '\0') {
    l->ch = 0;
  } else {
    l->ch = l->input[l->position];
  }
  l->position++;
  l->next_position = l->position++;
}

void init_lexer(Lexer *l, char *input) {
  l->input = input;
  l->position = 0;
  l->ch = 0;

  read_char(l);
}

// skip whitespaces since they don't add any value until it's a string
void skip_whitespace(Lexer *l) {
  while (isspace(l->ch)) {
    read_char(l);
  }
}

char peek_char(Lexer *l) { return l->input[l->position]; }

Token next_token(Lexer *l) {
  Token tok;

  skip_whitespace(l);

  switch (l->ch) {
  case '{':
    tok.type = TOKEN_LBRACE;
    strcpy(tok.literal, "{");
    read_char(l);
    break;

  case '}':
    tok.type = TOKEN_RBRACE;
    strcpy(tok.literal, "}");
    read_char(l);
    break;

  case '(':
    tok.type = TOKEN_LPAREN;
    strcpy(tok.literal, "(");
    read_char(l);
    break;

  case ')':
    tok.type = TOKEN_RPAREN;
    strcpy(tok.literal, ")");
    read_char(l);
    break;

  case '"': {
    read_char(l);

    int i = 0;

    while (l->ch != '"' && l->ch != 0) {
      tok.literal[i++] = l->ch;
      read_char(l);
    }

    tok.literal[i] = '\0';
    tok.type = TOKEN_STRING;

    read_char(l);
    break;
  }

  case 0:
    tok.type = TOKEN_EOF;
    strcpy(tok.literal, "EOF");
    break;

  default:
    if (isdigit(l->ch)) {
      int j = 0;

      while (isalnum(l->ch)) {
        tok.literal[j++] = l->ch;
        read_char(l);
      }
      tok.literal[j] = '\0';
      tok.type = TOKEN_NUMBER;
      return tok;
    }

    if (isalpha(l->ch) || l->ch == '_') {
      int i = 0;

      while (isalnum(l->ch) || l->ch == '_') {
        tok.literal[i++] = l->ch;
        read_char(l);
      }

      tok.literal[i] = '\0';

      if (strcmp(tok.literal, "probe") == 0) {
        tok.type = TOKEN_PROBE;
      } else {
        tok.type = TOKEN_IDENTIFIER;
      }

      return tok;
    }

    tok.type = TOKEN_ILLEGAL;
    tok.literal[0] = l->ch;
    tok.literal[1] = '\0';

    read_char(l);
  }

  return tok;
}

int main() {
  Lexer lexer;

  init_lexer(&lexer, source);

  Token tok;

  do {
    tok = next_token(&lexer);

    printf("Type: %-12s Literal: %s\n", token_type_to_string[tok.type],
           tok.literal);

  } while (tok.type != TOKEN_EOF);

  return 0;
}
