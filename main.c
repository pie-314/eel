#include "codegen/codegen.h"
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "semantic/semantic.h"
#include "loader/loader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static const char *get_file_extension(const char *filename) {
  const char *dot = strrchr(filename, '.');
  if (!dot || dot == filename)
    return "";
  return dot + 1;
}

static void print_usage(const char *prog_name) {
  printf("Usage: %s <file.eel> [-o <output_file>]\n", prog_name);
  printf("       %s run <eBPF_bin_name> [probe_symbol]\n\n", prog_name);
  printf("Commands:\n");
  printf("  run <file>   Load eBPF binary into kernel and stream live trace events\n\n");
  printf("Options:\n");
  printf("  -o <file>    Output compiled bytecode to <file>\n");
  printf("               Supported formats (determined by extension):\n");
  printf("                 .bin, .raw  - Raw binary eBPF instruction bytes\n");
  printf("                 .h          - C header file with struct bpf_insn array\n");
  printf("                 .o          - Standard 64-bit eBPF ELF relocatable object\n");
  printf("  -h, --help   Display this help message\n\n");
  printf("If -o is omitted, disassembled bytecode is printed to stdout.\n");
}

int main(int argc, char **argv) {
  if (argc >= 2 && strcmp(argv[1], "run") == 0) {
    if (argc < 3) {
      fprintf(stderr, "Error: 'run' requires an eBPF binary file.\n");
      fprintf(stderr, "Usage: %s run <eBPF_bin_name> [probe_symbol]\n", argv[0]);
      return 1;
    }
    const char *target = argv[2];
    const char *kprobe_symbol = (argc > 3) ? argv[3] : NULL;

    /* If user passed a .eel source file, compile it on the fly first */
    const char *ext = get_file_extension(target);
    if (strcmp(ext, "eel") == 0) {
      char *source = read_file(target);
      if (!source) return 1;

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

      if (!semantic_analyze(root)) {
        fprintf(stderr, "Compilation aborted due to semantic errors.\n");
        free(source);
        return 1;
      }

      BytecodeBuffer *buf = compile_ast_to_bytecode(root, NULL);
      if (!buf) {
        fprintf(stderr, "Error: code generation failed.\n");
        free(source);
        return 1;
      }

      const char *tmp_bin = "/tmp/eel_temp.bin";
      if (!bytecode_write_bin(buf, tmp_bin)) {
        fprintf(stderr, "Error: failed to write temporary binary.\n");
        bytecode_free(buf);
        free(source);
        return 1;
      }

      if (!kprobe_symbol) {
        const char *pname = get_probe_name(root);
        if (pname && strlen(pname) > 0) {
          kprobe_symbol = pname;
        }
      }

      bytecode_free(buf);
      free(source);
      target = tmp_bin;
    }

    return run_loader(target, kprobe_symbol);
  }

  const char *input_file = NULL;
  const char *output_file = NULL;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
      print_usage(argv[0]);
      return 0;
    } else if (strcmp(argv[i], "-o") == 0) {
      if (i + 1 >= argc) {
        fprintf(stderr, "Error: -o requires an output filename\n");
        return 1;
      }
      output_file = argv[++i];
    } else if (argv[i][0] == '-') {
      fprintf(stderr, "Error: unknown option '%s'\n", argv[i]);
      print_usage(argv[0]);
      return 1;
    } else {
      if (input_file != NULL) {
        fprintf(stderr, "Error: multiple input files specified: '%s' and '%s'\n", input_file, argv[i]);
        return 1;
      }
      input_file = argv[i];
    }
  }

  if (!input_file) {
    print_usage(argv[0]);
    return 1;
  }

  char *source = read_file(input_file);
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
  if (!ok) {
    fprintf(stderr, "Compilation aborted due to semantic errors.\n");
    free(source);
    return 1;
  }

  BytecodeBuffer *buf = compile_ast_to_bytecode(root, NULL);
  if (!buf) {
    fprintf(stderr, "Error: code generation failed.\n");
    free(source);
    return 1;
  }

  if (output_file) {
    const char *ext = get_file_extension(output_file);
    const char *probe_name = get_probe_name(root);
    char array_name[128];
    if (probe_name && strlen(probe_name) > 0) {
      snprintf(array_name, sizeof(array_name), "%s_probe", probe_name);
    } else {
      snprintf(array_name, sizeof(array_name), "eel_bpf_prog");
    }

    bool success = false;
    if (strcmp(ext, "bin") == 0 || strcmp(ext, "raw") == 0) {
      success = bytecode_write_bin(buf, output_file);
      if (success) {
        printf("[EEL] Wrote %d raw eBPF instructions (%zu bytes) to %s\n",
               buf->count, buf->count * sizeof(struct bpf_insn), output_file);
      }
    } else if (strcmp(ext, "h") == 0) {
      success = bytecode_write_header(buf, output_file, array_name);
      if (success) {
        printf("[EEL] Wrote C header with %d eBPF instructions to %s\n",
               buf->count, output_file);
      }
    } else if (strcmp(ext, "o") == 0) {
      success = bytecode_write_elf(buf, output_file, probe_name);
      if (success) {
        printf("[EEL] Wrote 64-bit eBPF ELF object (%d instructions) to %s\n",
               buf->count, output_file);
      }
    } else {
      fprintf(stderr, "Error: unsupported output file extension '.%s'\n", ext);
      fprintf(stderr, "Supported formats: .bin, .raw, .h, .o\n");
      success = false;
    }

    bytecode_free(buf);
    free(source);
    return success ? 0 : 1;
  }

  /* Default behavior: print disassembled bytecode to stdout */
  bytecode_dump(buf);
  bytecode_free(buf);
  free(source);
  return 0;
}
