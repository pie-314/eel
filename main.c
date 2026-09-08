#include "codegen/codegen.h"
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "semantic/semantic.h"

int main(int argc, char **argv) {
  char *default_source = "probe sys_execve {\n"
                         "    pid = 1337\n"
                         "   // y = 10\n"
                         "\n"
                         "int x = \"hello\"\n"
                         "    if (pid == 1000) {\n"
                         "        a = 10; \n"
                         "        repeat pid {\n"
                         "            print(\"large\",\"hello\",pid+1000)\n"
                         "            b = a\n"
                         "        }\n"
                         "    }\n"
                         "}";
  char *source = (argc > 1) ? argv[1] : default_source;
  Lexer lexer;
  Parser parser;

  init_lexer(&lexer, source);
  parser_init(&parser, &lexer);

  ASTNode *root = parse_program(&parser);

  bool ok = semantic_analyze(root);
  if (ok) {
    BytecodeBuffer *buf = compile_ast_to_bytecode(root, NULL);
    if (buf) {
      bytecode_dump(buf);
      bytecode_free(buf);
    }
  }

  // print_ast(root, 0);
  return 0;
}
