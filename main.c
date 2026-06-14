#include "lexer/lexer.h"
#include "parser/parser.h"

int main() {
  // char *source = "{\n"
  //                "  print(\"start\")\n"
  //                "  x = 6\n"
  //                "  repeat (x == 7){\n"
  //                "  print(\"end\")}\n"
  //                "}\n"
  //                "probe sys_exceve {\n"
  //                "print(\"hello world\")\n"
  //                "}\n";
  char *source = "probe sys_execve {\n"
                 "    pid = 1337\n"
                 "\n"
                 "x = -10\n"
                 "    if (pid > 1000) {\n"
                 "        repeat pid {\n"
                 "            print(\"large\",\"hello\",pid+1000)\n"
                 "        }\n"
                 "    }\n"
                 "}";
  Lexer lexer;
  Parser parser;

  init_lexer(&lexer, source);
  parser_init(&parser, &lexer);

  ASTNode *root = parse_program(&parser);

  print_ast(root, 0);

  return 0;
}
