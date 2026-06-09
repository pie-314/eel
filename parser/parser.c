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

<<<<<<< HEAD
=======
Precedence get_precedence(TokenType t) {
  switch (t) {
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

ASTNode *parse_primary(Parser *p) {
  if (cur_token_is(p, TOKEN_NUMBER))
    return parse_number(p);

  if (cur_token_is(p, TOKEN_IDENTIFIER))
    return parse_identifier(p);

  return NULL;
}

>>>>>>> 5207145 (pratt parser for expressions implemented)
ASTNode *parse_number(Parser *p) {
  ASTNode *node = new_node(NODE_NUMBER);
  node->token = p->cur_token;

  return node;
}

<<<<<<< HEAD
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

=======
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
>>>>>>> 5207145 (pratt parser for expressions implemented)
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
<<<<<<< HEAD
  node->right = parse_number(p);
=======
  node->right = parse_expression(p, PREC_LOWEST);
>>>>>>> 5207145 (pratt parser for expressions implemented)

  return node;
}

ASTNode *parse_program(Parser *p) {
  ASTNode *program = new_node(NODE_PROGRAM);

  while (!cur_token_is(p, TOKEN_EOF)) {
<<<<<<< HEAD
    ASTNode *stmt = parse_program(p);
=======
    ASTNode *stmt = parse_statement(p);
>>>>>>> 5207145 (pratt parser for expressions implemented)

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
<<<<<<< HEAD
=======

const char *node_type_to_string[] = {
    "PROGRAM",   "IDENT",  "NUMBER", "STRING",

    "ASSIGN",

    "ADD",       "SUB",    "MUL",    "DIV",    "MOD",

    "EQ",        "NEQ",    "GT",     "LT",     "GTE", "LTE",

    "EXPR_STMT", "BLOCK",

    "IF",        "REPEAT",

    "PROBE",

    "CALL"};

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
>>>>>>> 5207145 (pratt parser for expressions implemented)
