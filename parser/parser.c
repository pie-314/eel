#include "../lexer/lexer.h"

typedef enum {
  NODE_PROGRAM,

  // literals
  NODE_IDENT,
  NODE_NUMBER,
  NODE_STRING,

  // expressions
  NODE_ASSIGN,

  NODE_ADD,
  NODE_SUB,
  NODE_MUL,
  NODE_DIV,
  NODE_MOD,

  NODE_EQ,
  NODE_NEQ,
  NODE_GT,
  NODE_LT,
  NODE_GTE,
  NODE_LTE,

  // statements
  NODE_EXPR_STMT,
  NODE_BLOCK,

  NODE_IF,
  NODE_REPEAT,

  // eel
  NODE_PROBE,

  // function call
  NODE_CALL
} NodeType;
