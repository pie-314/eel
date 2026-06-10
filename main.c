#include "lexer/lexer.h"
#include "parser/parser.h"

int main() {
  char *source = "{\n"
                 "  print(\"start\")\n"
                 "  x = 42{\n"
                 "  print(\"end\")}\n"
                 "}";
  Lexer lexer;
  Parser parser;

  init_lexer(&lexer, source);
  parser_init(&parser, &lexer);

  ASTNode *root = parse_program(&parser);

  print_ast(root, 0);

  return 0;
}
