#include "parser.h"

void parser_init(Parser *p, Lexer *l) {
  p->lexer = l;
  p->cur_token = next_token(l);
  p->peek_token = next_token(l);
}

// get next token
void next_token_parser(Parser *p) {
  p->cur_token = p->peek_token;
  p->peek_token = next_token(p->lexer);
}

// check for current token
bool cur_token_is(Parser *p, TokenType t) {
  if (p->cur_token.type == t) {
    return true;
  }
  return false;
}

// chekc for next token
bool peek_token_is(Parser *p, TokenType t) {
  if (p->peek_token.type == t) {
    return true;
  }
  return false;
}

// check for unexpected tokens
bool expect_peek(Parser *p, TokenType t) {
  if (peek_token_is(p, t)) {
    next_token_parser(p);
    return true;
  }

  parser_error(p, "unexpected token");
  return false;
}

ASTNode *new_node(NodeType type) {
  ASTNode *node = malloc(sizeof(ASTNode));

  node->type = type;

  node->left = NULL;
  node->right = NULL;

  node->children = NULL;
  node->child_count = 0;

  return node;
}

ASTNode *parse_identifier(Parser *p) {
  ASTNode *node = new_node(NODE_IDENT);
  node->token = p->cur_token;

  return node;
}

ASTNode *parse_number(Parser *p) {
  ASTNode *node = new_node(NODE_NUMBER);
  node->token = p->cur_token;

  return node;
}

ASTNode *parse_expression(Parser *p) {
  ASTNode *node = new_node(NODE_EXPR_STMT);
  node->token = p->cur_token;
  node->left = parse_identifier(p);

  if (!expect_peek(p, TOKEN_ADD))
    return NULL;

  next_token_parser(p);
  node->right = parse_number(p);

  return node;
}

ASTNode *parse_statement(Parser *p) {
  if (cur_token_is(p, TOKEN_IDENTIFIER) && peek_token_is(p, TOKEN_ASSIGN)) {
    return parse_assignment(p);
  }

  return NULL;
}

ASTNode *parse_assignment(Parser *p) {
  ASTNode *node = new_node(NODE_ASSIGN);
  node->token = p->cur_token;
  node->left = parse_identifier(p);

  if (!expect_peek(p, TOKEN_ASSIGN))
    return NULL;

  next_token_parser(p);
  node->right = parse_number(p);

  return node;
}
