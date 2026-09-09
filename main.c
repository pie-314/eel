#include "codegen/codegen.h"
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "semantic/semantic.h"
#include <stdio.h>
#include <stdlib.h>

static char *read_file(const char *filepath) {
  FILE *file = fopen(filepath, "rb");
  if (!file) {
    fprintf(stderr, "Error: could not open file '%s'\n", filepath);
    return NULL;
  }

  fseek(file, 0, SEEK_END);
  long length = ftell(file);
  fseek(file, 0, SEEK_SET);

  char *buffer = malloc(length + 1);
  if (!buffer) {
    fprintf(stderr, "Error: failed to allocate memory for '%s'\n", filepath);
    fclose(file);
    return NULL;
  }

  size_t read_bytes = fread(buffer, 1, length, file);
  buffer[read_bytes] = '\0';
  fclose(file);
  return buffer;
}

int main(int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr, "Usage: %s <file.eel>\n", argv[0]);
    return 1;
  }

  char *source = read_file(argv[1]);
  if (!source) {
    return 1;
  }

  Lexer lexer;
  Parser parser;

  init_lexer(&lexer, source);
  parser_init(&parser, &lexer);

  ASTNode *root = parse_program(&parser);
  if (!root) {
    fprintf(stderr, "Parsing failed.\n");
    free(source);
    return 1;
  }

  bool ok = semantic_analyze(root);
  if (ok) {
    BytecodeBuffer *buf = compile_ast_to_bytecode(root, NULL);
    if (buf) {
      bytecode_dump(buf);
      bytecode_free(buf);
    }
  }

  free(source);
  return ok ? 0 : 1;
}
