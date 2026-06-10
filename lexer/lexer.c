#include "lexer.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

const char *token_type_to_string[TOKEN_COUNT] = {
    [TOKEN_ILLEGAL] = "ILLEGAL",
    [TOKEN_EOF] = "EOF",
    [TOKEN_PROBE] = "PROBE",
    [TOKEN_IDENTIFIER] = "IDENTIFIER",
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

    // types
    [TOKEN_INT] = "INT",
    [TOKEN_STRING_TYPE] = "STR_TYPE",
    [TOKEN_MAP] = "MAP",

    [TOKEN_ASSIGN] = "="};

char *source = "probe sys_execve {\n"
               "    print(\"exec called\")\n"
               "\n"
               "    pid = 1337\n"
               "    counter = counter + 1\n"
               "\n"
               "// modi\n"
               "    if (pid >= 1000) {\n"
               "        print(\"large pid\")\n"
               "    }\n"
               "\n"
               "    if (counter == 10) {\n"
               "        print(\"counter reached\")\n"
               "    }\n"
               "\n"
               "/* checking for \n"
               "multi-line comment*/ \n"
               "    if (pid != 1) {\n"
               "        print(\"not init process\")\n"
               "    }\n"
               "\n"
               "    total = (10 + 20) * 5\n"
               "    mod = total % 3\n"
               "\n"
               "    repeat 3 {\n"
               "        print(\"looping\")\n"
               "    }\n"
               "}";

void read_char(Lexer *l) {
  if (l->input[l->position] == '\0') {
    l->ch = 0;
  } else {
    l->ch = l->input[l->position];
  }
  l->position++;
  l->next_position = l->position + 1;

  // track next_position
  if (l->ch == '\n') {
    l->line++;
    l->column = 0;
  } else {
    l->column++;
  }
}

void init_lexer(Lexer *l, char *input) {
  l->input = input;
  l->position = 0;
  l->ch = 0;
  l->line = 1;
  l->column = 0;

  read_char(l);
}

void report_error(Lexer *l, const char *msg) {
  fprintf(stderr, "Lexer Error at line %d, col %d: %s\n", l->line, l->column,
          msg);
}

// skip whitespaces since they don't add any value until it's a string
void skip_whitespace(Lexer *l) {
  while (isspace(l->ch)) {
    read_char(l);
  }
}
TokenType lookup_identifier(char *ident) {
  if (strcmp(ident, "probe") == 0)
    return TOKEN_PROBE;

  if (strcmp(ident, "if") == 0)
    return TOKEN_IF;

  if (strcmp(ident, "elif") == 0)
    return TOKEN_ELIF;

  if (strcmp(ident, "else") == 0)
    return TOKEN_ELSE;

  if (strcmp(ident, "repeat") == 0)
    return TOKEN_REPEAT;
  if (strcmp(ident, "int") == 0)
    return TOKEN_INT;

  if (strcmp(ident, "string") == 0)
    return TOKEN_STRING_TYPE;

  if (strcmp(ident, "map") == 0)
    return TOKEN_MAP;

  return TOKEN_IDENTIFIER;
}

char peek_char(Lexer *l) { return l->input[l->position]; }

void skip_comments(Lexer *l) {
  // single-line
  if (l->ch == '/' && peek_char(l) == '/') {
    while (l->ch != '\n' && l->ch != 0) {
      read_char(l);
    }
  }

  // multi-line
  if (l->ch == '/' && peek_char(l) == '*') {
    read_char(l);
    read_char(l);

    while (!(l->ch == '*' && peek_char(l) == '/') && l->ch != 0) {
      read_char(l);
    }

    if (l->ch != 0) {
      read_char(l);
      read_char(l);
    } else {
      report_error(l, "Unterminated multi-line comment");
    }
  }
}

Token next_token(Lexer *l) {
  Token tok;

  while (1) {
    skip_whitespace(l);

    if (l->ch == '/' && peek_char(l) == '/') {
      skip_comments(l);
    } else if (l->ch == '/' && peek_char(l) == '*') {
      skip_comments(l);
    } else {
      break;
    }
  }

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

  case ';':
    tok.type = TOKEN_SEMICOLON;
    strcpy(tok.literal, ";");
    read_char(l);
    break;

  case '"': {
    read_char(l);

    int i = 0;

    while (l->ch != '"' && l->ch != 0) {
      tok.literal[i++] = l->ch;
      read_char(l);
    }

    if (l->ch == 0) {
      report_error(l, "Unterminated string literal");
      tok.type = TOKEN_ILLEGAL;
      return tok;
    }

    tok.literal[i] = '\0';
    tok.type = TOKEN_STRING;

    read_char(l);
    break;
  }
  case '+':
    read_char(l);
    tok.type = TOKEN_ADD;
    strcpy(tok.literal, "+");
    break;

  case '-':
    read_char(l);
    tok.type = TOKEN_SUBTRACT;
    strcpy(tok.literal, "-");
    break;

  case '*':
    read_char(l);
    tok.type = TOKEN_MULTI;
    strcpy(tok.literal, "*");
    break;

  case '/':
    read_char(l);
    tok.type = TOKEN_DIV;
    strcpy(tok.literal, "/");
    break;

  case '%':
    read_char(l);
    tok.type = TOKEN_MOD;
    strcpy(tok.literal, "%");
    break;

  case '=':
    if (peek_char(l) == '=') {
      read_char(l);
      tok.type = TOKEN_EQ;
      strcpy(tok.literal, "==");
    } else {
      tok.type = TOKEN_ASSIGN;
      strcpy(tok.literal, "=");
    }
    read_char(l);
    break;

  case '!':
    if (peek_char(l) == '=') {
      read_char(l);
      tok.type = TOKEN_NOTEQ;
      strcpy(tok.literal, "!=");
      read_char(l);
    } else {
      tok.type = TOKEN_ILLEGAL;
      strcpy(tok.literal, "!");
      read_char(l);
    }
    break;

  case '>':
    if (peek_char(l) == '=') {
      read_char(l);
      tok.type = TOKEN_GTE;
      strcpy(tok.literal, ">=");

    } else {
      tok.type = TOKEN_GT;
      strcpy(tok.literal, ">");
    }
    read_char(l);
    break;

  case '<':
    if (peek_char(l) == '=') {
      read_char(l);
      tok.type = TOKEN_LTE;
      strcpy(tok.literal, "<=");
    } else {
      tok.type = TOKEN_LT;
      strcpy(tok.literal, "<");
    }
    read_char(l);
    break;

  case 0:
    tok.type = TOKEN_EOF;
    strcpy(tok.literal, "EOF");
    break;

  default:
    if (isdigit(l->ch)) {
      int j = 0;

      while (isdigit(l->ch)) {
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
      tok.type = lookup_identifier(tok.literal);

      return tok;
    }

    tok.type = TOKEN_ILLEGAL;
    tok.literal[0] = l->ch;
    tok.literal[1] = '\0';

    report_error(l, "Illegal character found");

    read_char(l);
  }

  return tok;
}
//
// int main() {
//   Lexer lexer;
//
//   init_lexer(&lexer, source);
//
//   Token tok;
//
//   do {
//     tok = next_token(&lexer);
//
//     printf("Type: %-12s Literal: %s\n", token_type_to_string[tok.type],
//            tok.literal);
//
//   } while (tok.type != TOKEN_EOF);
//
//   return 0;
// }
