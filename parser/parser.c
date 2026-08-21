#include "parser.h"
#include <stdlib.h>

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

// check for next token
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

Precedence get_precedence(TokenType t) {
  switch (t) {
  case TOKEN_EQ:
    return PREC_EQUALS;

  case TOKEN_GT:
  case TOKEN_LT:
  case TOKEN_NOTEQ:
  case TOKEN_GTE:
  case TOKEN_LTE:
    return PREC_COMPARE;

  case TOKEN_ADD:
  case TOKEN_SUBTRACT:
    return PREC_SUM;

  case TOKEN_MULTI:
  case TOKEN_DIV:
  case TOKEN_MOD:
    return PREC_PRODUCT;

  default:
    return PREC_LOWEST;
  }
}

ASTNode *parse_string(Parser *p) {
  ASTNode *node = new_node(NODE_STRING);
  node->token = p->cur_token;
  return node;
}

// parse probes
ASTNode *parse_probe(Parser *p) {

  ASTNode *node = new_node(NODE_PROBE);

  if (!expect_peek(p, TOKEN_IDENTIFIER)) {
    return NULL;
  }

  node->left = parse_primary(p);

  if (!expect_peek(p, TOKEN_LBRACE))
    return NULL;

  node->right = parse_blocks(p);

  return node;
}

// parse loops
ASTNode *parse_repeat(Parser *p) {

  ASTNode *node = new_node(NODE_REPEAT);

  // repeat 5 { ... }
  if (peek_token_is(p, TOKEN_NUMBER)) {

    next_token_parser(p);
    node->left = parse_number(p);
  }

  // repeat x
  if (peek_token_is(p, TOKEN_IDENTIFIER)) {
    next_token_parser(p);
    node->left = parse_identifier(p);

  }

  // repeat (x + 1) { ... }
  else if (peek_token_is(p, TOKEN_LPAREN)) {

    expect_peek(p, TOKEN_LPAREN);

    next_token_parser(p);

    node->left = parse_expression(p, PREC_LOWEST);

    if (!expect_peek(p, TOKEN_RPAREN))
      return NULL;
  }

  else {
    parser_error(p, "expected repeat count");
    return NULL;
  }

  if (!expect_peek(p, TOKEN_LBRACE))
    return NULL;

  node->right = parse_blocks(p);

  return node;
}

// handle if / elif / else
ASTNode *parse_conditionals(Parser *p) {

  ASTNode *node = new_node(NODE_IF);

  if (!expect_peek(p, TOKEN_LPAREN))
    return NULL;

  next_token_parser(p);

  node->left = parse_expression(p, PREC_LOWEST);

  if (!expect_peek(p, TOKEN_RPAREN))
    return NULL;

  if (!expect_peek(p, TOKEN_LBRACE))
    return NULL;

  node->right = parse_blocks(p);

  while (peek_token_is(p, TOKEN_ELIF)) {
    next_token_parser(p);
    ASTNode *elif_node = new_node(NODE_ELIF);
    if (!expect_peek(p, TOKEN_LPAREN))
      return NULL;

    next_token_parser(p);

    elif_node->left = parse_expression(p, PREC_LOWEST);

    if (!expect_peek(p, TOKEN_RPAREN))
      return NULL;

    if (!expect_peek(p, TOKEN_LBRACE))
      return NULL;

    elif_node->right = parse_blocks(p);

    node->children =
        realloc(node->children, sizeof(ASTNode *) * (node->child_count + 1));

    node->children[node->child_count++] = elif_node;
  }

  if (peek_token_is(p, TOKEN_ELSE)) {

    next_token_parser(p);

    ASTNode *else_node = new_node(NODE_ELSE);

    if (!expect_peek(p, TOKEN_LBRACE))
      return NULL;

    else_node->right = parse_blocks(p);

    node->children =
        realloc(node->children, sizeof(ASTNode *) * (node->child_count + 1));

    node->children[node->child_count++] = else_node;
  }

  return node;
}

// handle blocks
ASTNode *parse_blocks(Parser *p) {
  ASTNode *block = new_node(NODE_BLOCK);

  next_token_parser(p);

  while (!cur_token_is(p, TOKEN_RBRACE) && !cur_token_is(p, TOKEN_EOF)) {

    ASTNode *stmt = parse_statement(p);

    if (stmt) {
      block->children = realloc(block->children,
                                sizeof(ASTNode *) * (block->child_count + 1));

      block->children[block->child_count++] = stmt;
    }

    next_token_parser(p);
  }

  return block;
}

ASTNode *parse_primary(Parser *p) {
  if (cur_token_is(p, TOKEN_NUMBER))
    return parse_number(p);

  if (cur_token_is(p, TOKEN_SUBTRACT)) {
    return parse_prefix(p);
  }

  if (cur_token_is(p, TOKEN_NOT))
    return parse_prefix(p);

  if (cur_token_is(p, TOKEN_IDENTIFIER)) {
    if (peek_token_is(p, TOKEN_LPAREN))
      return parse_call_expression(p);

    return parse_identifier(p);
  }
  if (cur_token_is(p, TOKEN_STRING))
    return parse_string(p);

  if (cur_token_is(p, TOKEN_LPAREN)) {
    next_token_parser(p);

    ASTNode *expr = parse_expression(p, PREC_LOWEST);

    if (!expect_peek(p, TOKEN_RPAREN)) {
      return NULL;
    }

    return expr;
  }
  // if (cur_token_is(p, TOKEN_LBRACE)) {
  //   return parse_blocks(p);
  // }

  return NULL;
}

ASTNode *parse_call_expression(Parser *p) {

  ASTNode *call = new_node(NODE_CALL);

  call->left = parse_identifier(p);

  if (!expect_peek(p, TOKEN_LPAREN)) {
    return NULL;
  }

  next_token_parser(p);

  while (!cur_token_is(p, TOKEN_RPAREN)) {
    ASTNode *arg = parse_expression(p, PREC_LOWEST);

    call->children =
        realloc(call->children, sizeof(ASTNode *) * (call->child_count + 1));
    call->children[call->child_count++] = arg;

    if (peek_token_is(p, TOKEN_COMMA)) {
      next_token_parser(p);
      next_token_parser(p);
    } else {
      break;
    }
  }

  expect_peek(p, TOKEN_RPAREN);

  return call;
}

ASTNode *parse_number(Parser *p) {
  ASTNode *node = new_node(NODE_NUMBER);
  node->token = p->cur_token;

  return node;
}

NodeType token_to_node(TokenType t) {
  switch (t) {
  case TOKEN_ADD:
    return NODE_ADD;

  case TOKEN_SUBTRACT:
    return NODE_SUB;

  case TOKEN_MULTI:
    return NODE_MUL;

  case TOKEN_DIV:
    return NODE_DIV;

  case TOKEN_MOD:
    return NODE_MOD;

  case TOKEN_EQ:
    return NODE_EQ;

  case TOKEN_NOTEQ:
    return NODE_NEQ;

  case TOKEN_GT:
    return NODE_GT;

  case TOKEN_LT:
    return NODE_LT;

  case TOKEN_GTE:
    return NODE_GTE;

  case TOKEN_LTE:
    return NODE_LTE;

  default:
    return NODE_EXPR_STMT;
  }
}

// ASTNode *parse_expression(Parser *p) {
//   ASTNode *left = NULL;
//
//   /*
//    suppose stream is x = 20 + 10;
//    every ast node has tow sides left and right
//    exp
//      |_ left
//      |_ right
//
//     so for now we have left = NULL
//     which means
//     IDENT(x)
//      |_ NULL
//
//    */
//   if (cur_token_is(p, TOKEN_IDENTIFIER)) {
//     left = parse_identifier(p);
//   } else if (cur_token_is(p, TOKEN_NUMBER)) {
//     left = parse_number(p);
//     /*
//      this codition is true so it becomes
//      left
//        |
//     NUMBER(20)
//     */
//
//   } else {
//     return NULL;
//   }
//
//   if (!peek_token_is(p, TOKEN_ADD)) {
//     return left;
//     /*
//      if this condition was true
//      and expression was true and expression was x = 20 then it will return
//      left
//        |
//     NUMBER(20)
//     */
//   }
//
//   /* since the next thing after TOKEN_NUMBER is TOKEN_ADD
//      it moves to next token
//    */
//   next_token_parser(p);
//
//   ASTNode *add = new_node(NODE_ADD);
//   /*
//    a new node is added and it becomes
//    ASSIGN
//       |_IDENT(x)
//       |_ADD
//          |_ NULL
//          |_ NULL
//    */
//
//   add->left = left;
//   /*
//    ASSIGN
//       |_IDENT(x)
//       |_ADD
//          |_ NUMBER(20)
//          |_ NULL
//    */
//
//   next_token_parser(p);
//   if (cur_token_is(p, TOKEN_IDENTIFIER)) {
//     add->right = parse_identifier(p);
//   } else if (cur_token_is(p, TOKEN_NUMBER)) {
//     add->right = parse_number(p);
//     /* so finally it becomes
//     ASSIGN
//        |_IDENT(x)
//        |_ADD
//           |_ NUMBER(20)
//           |_ NUMBER(10)
//     */
//   }
//   return add;
// }

ASTNode *parse_prefix(Parser *p) {
  ASTNode *node = NULL;

  if (cur_token_is(p, TOKEN_SUBTRACT))
    node = new_node(NODE_NEG);
  else if (cur_token_is(p, TOKEN_NOT))
    node = new_node(NODE_NOT);
  else
    return NULL;

  next_token_parser(p);

  node->left = parse_expression(p, PREC_PREFIX);

  return node;
}

ASTNode *parse_expression(Parser *p, Precedence prec) {

  ASTNode *left = parse_primary(p);

  while (get_precedence(p->peek_token.type) > prec) {

    next_token_parser(p);

    TokenType op = p->cur_token.type;

    ASTNode *expr = new_node(token_to_node(op));

    expr->left = left;

    Precedence current_prec = get_precedence(op);

    next_token_parser(p);

    expr->right = parse_expression(p, current_prec);

    left = expr;
  }

  return left;
}
ASTNode *parse_statement(Parser *p) {
  if (cur_token_is(p, TOKEN_LBRACE)) {
    return parse_blocks(p);
  }

  else if (cur_token_is(p, TOKEN_PROBE)) {
    return parse_probe(p);
  }

  else if (cur_token_is(p, TOKEN_REPEAT)) {
    return parse_repeat(p);
  }

  else if (cur_token_is(p, TOKEN_IF)) {
    return parse_conditionals(p);
  }

  else if ((cur_token_is(p, TOKEN_INT) || cur_token_is(p, TOKEN_STRING_TYPE)) &&
           peek_token_is(p, TOKEN_IDENTIFIER)) {
    next_token_parser(p);
    return parse_assignment(p);
  }

  else if (cur_token_is(p, TOKEN_IDENTIFIER) &&
           peek_token_is(p, TOKEN_ASSIGN)) {
    return parse_assignment(p);
  }

  else if (cur_token_is(p, TOKEN_IDENTIFIER) &&
           peek_token_is(p, TOKEN_LPAREN)) {
    return parse_call_expression(p);
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
  node->right = parse_expression(p, PREC_LOWEST);

  return node;
}

ASTNode *parse_program(Parser *p) {
  ASTNode *program = new_node(NODE_PROGRAM);

  while (!cur_token_is(p, TOKEN_EOF)) {
    ASTNode *stmt = parse_statement(p);

    if (stmt != NULL) {
      program->children = realloc(
          program->children, sizeof(ASTNode *) * (program->child_count + 1));

      program->children[program->child_count++] = stmt;
    }
    next_token_parser(p);
  }
  return program;
}

void parser_error(Parser *p, const char *msg) {
  fprintf(stderr, "Parser Error: %s near '%s'\n", msg, p->cur_token.literal);
}

const char *node_type_to_string[] = {
    "PROGRAM", "IDENT", "NUMBER", "STRING", "ASSIGN",    "ADD",   "SUB",
    "MUL",     "DIV",   "MOD",    "EQ",     "NEQ",       "GT",    "LT",
    "GTE",     "LTE",   "NEG",    "NOT",    "EXPR_STMT", "BLOCK", "IF",
    "ELIF",    "ELSE",  "REPEAT", "PROBE",  "CALL"};

void print_ast(ASTNode *node, int depth) {
  if (node == NULL)
    return;

  for (int i = 0; i < depth; i++)
    printf("  ");

  printf("%s", node_type_to_string[node->type]);

  if (node->type == NODE_IDENT || node->type == NODE_NUMBER ||
      node->type == NODE_STRING) {
    printf(" (%s)", node->token.literal);
  }

  printf("\n");

  print_ast(node->left, depth + 1);
  print_ast(node->right, depth + 1);

  for (int i = 0; i < node->child_count; i++) {
    print_ast(node->children[i], depth + 1);
  }
}
