#include "parser.h"

void parser_init(Parser *p, Lexer *l) {
  p->lexer = l;
  p->cur_token = next_token(l);
  p->peek_token = next_token(l);
}

void next_token_parser(Parser *p) {
  p->cur_token = p->peek_token;
  p->peek_token = next_token(p->lexer);
}

bool cur_token_is(Parser *p, TokenType t) {
  if (p->cur_token.type == t) {
    return true;
  }
  return false;
}

bool peek_token_is(Parser *p, TokenType t) {
  if (p->peek_token.type == t) {
    return true;
  }
  return false;
}
