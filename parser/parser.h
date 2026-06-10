#include "../lexer/lexer.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
typedef struct ASTNode ASTNode;

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

struct ASTNode {
  NodeType type;

  Token token;

  ASTNode *left;
  ASTNode *right;

  ASTNode **children;
  int child_count;
};

typedef struct {
  Lexer *lexer;

  Token cur_token;
  Token peek_token;
} Parser;

typedef enum {
  PREC_LOWEST,
  PREC_EQUALS,
  PREC_COMPARE,
  PREC_SUM,     // + -
  PREC_PRODUCT, // * / %
} Precedence;

void parser_init(Parser *p, Lexer *l);
void next_token_parser(Parser *p);
void parser_error(Parser *p, const char *msg);
bool cur_token_is(Parser *p, TokenType t);
bool peek_token_is(Parser *p, TokenType t);
bool expect_peek(Parser *p, TokenType t);
ASTNode *new_node(NodeType type);
ASTNode *new_number(Token tok);
ASTNode *new_identifier(Token tok);
ASTNode *parse_program(Parser *p);
ASTNode *parse_statement(Parser *p);
ASTNode *parse_assignment(Parser *p);
ASTNode *parse_expression(Parser *p, Precedence prec);
ASTNode *parse_identifier(Parser *p);
ASTNode *parse_string(Parser *p);
ASTNode *parse_number(Parser *p);
ASTNode *parse_call_expression(Parser *p);
ASTNode *parse_blocks(Parser *p);

Precedence get_precedence(TokenType t);
void print_ast(ASTNode *node, int depth);
