#include "lexer/lexer.h"
#include "parser/parser.h"
#include "semantic/semantic.h"

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
                 "   // y = 10\n"
                 "\n"
                 "int x = \"hello\"\n"
                 "int z = y + 10\n"
                 "    if (pid == \"1000\") {\n"
                 "        a = 10; \n"
                 "        repeat pid {\n"
                 "            print(\"large\",\"hello\",pid+1000)\n"
                 "            b = a\n"
                 "        }\n"
                 "    }\n"
                 "}";
  Lexer lexer;
  Parser parser;

  init_lexer(&lexer, source);
  parser_init(&parser, &lexer);

  ASTNode *root = parse_program(&parser);

  semantic_analyze(root);

  // print_ast(root, 0);
}
