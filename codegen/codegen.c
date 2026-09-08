#include "codegen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

BytecodeBuffer *bytecode_create(void) {
  BytecodeBuffer *buf = malloc(sizeof(BytecodeBuffer));
  if (!buf)
    return NULL;
  buf->capacity = 64;
  buf->count = 0;
  buf->insns = malloc(buf->capacity * sizeof(struct bpf_insn));
  if (!buf->insns) {
    free(buf);
    return NULL;
  }
  return buf;
}

void bytecode_free(BytecodeBuffer *buf) {
  if (!buf)
    return;
  if (buf->insns) {
    free(buf->insns);
  }
  free(buf);
}

void emit(BytecodeBuffer *buf, struct bpf_insn insn) {
  if (buf->count >= buf->capacity) {
    buf->capacity *= 2;
    buf->insns = realloc(buf->insns, buf->capacity * sizeof(struct bpf_insn));
  }
  buf->insns[buf->count++] = insn;
}

/* 64-bit immediate load pseudo-instruction (requires 2 instruction slots) */
void emit_ld_imm64(BytecodeBuffer *buf, int dst, uint64_t val) {
  emit(buf, (struct bpf_insn){.code = BPF_LD | BPF_DW | BPF_IMM,
                              .dst_reg = (uint8_t)dst,
                              .src_reg = 0,
                              .off = 0,
                              .imm = (int32_t)(val & 0xFFFFFFFF)});
  emit(buf, (struct bpf_insn){.code = 0,
                              .dst_reg = 0,
                              .src_reg = 0,
                              .off = 0,
                              .imm = (int32_t)(val >> 32)});
}

/* Synthesise string onto stack and invoke bpf_trace_printk helper */
static void emit_print_helper(ASTNode *call_node, CodeGenContext *ctx) {
  if (!call_node || call_node->child_count == 0)
    return;

  ASTNode *arg0 = call_node->children[0];
  char fmt_str[256] = {0};
  int val_args_start = 0;

  if (arg0 && arg0->type == NODE_STRING) {
    val_args_start = 1;
    const char *lit = arg0->token.literal;
    int existing_specifiers = 0;
    for (int i = 0; lit[i] != '\0'; i++) {
      if (lit[i] == '%' && lit[i + 1] != '%') {
        existing_specifiers++;
      }
    }
    int total_args = call_node->child_count - 1;
    int needed = total_args - existing_specifiers;
    snprintf(fmt_str, sizeof(fmt_str), "%.200s", lit);
    for (int i = 0;
         i < needed && (int)strlen(fmt_str) + 6 < (int)sizeof(fmt_str); i++) {
      strcat(fmt_str, " %ld");
    }
    if (strlen(fmt_str) > 0 && fmt_str[strlen(fmt_str) - 1] != '\n') {
      strcat(fmt_str, "\n");
    }
  } else {
    val_args_start = 0;
    if (call_node->child_count == 1) {
      snprintf(fmt_str, sizeof(fmt_str), "val: %%ld\n");
    } else if (call_node->child_count == 2) {
      snprintf(fmt_str, sizeof(fmt_str), "val: %%ld %%ld\n");
    } else {
      snprintf(fmt_str, sizeof(fmt_str), "val: %%ld %%ld %%ld\n");
    }
  }

  /* Evaluate value arguments into registers R3, R4, R5 (up to 3 values) */
  int arg_regs[] = {BPF_REG_3, BPF_REG_4, BPF_REG_5};
  int val_idx = 0;
  for (int i = val_args_start; i < call_node->child_count && val_idx < 3; i++) {
    compile_expression(call_node->children[i], ctx, arg_regs[val_idx]);
    val_idx++;
  }

  /* Stack layout: write string buffer at [R10 - 256] */
  int str_len = (int)strlen(fmt_str) + 1;
  int padded_len = ((str_len + 7) / 8) * 8;
  int stack_base_offset = -256;

  char chunk_buf[128] = {0};
  memcpy(chunk_buf, fmt_str, str_len);

  for (int i = 0; i < padded_len; i += 8) {
    uint64_t chunk = 0;
    memcpy(&chunk, chunk_buf + i, 8);

    emit_ld_imm64(ctx->buf, BPF_REG_1, chunk);
    emit(ctx->buf,
         BPF_STX_MEM_DW(BPF_REG_10, BPF_REG_1, stack_base_offset + i));
  }

  /* R1 = pointer to format string buffer: R1 = R10 + stack_base_offset */
  emit(ctx->buf, BPF_MOV64_REG(BPF_REG_1, BPF_REG_10));
  emit(ctx->buf, BPF_ALU64_IMM(BPF_ADD, BPF_REG_1, stack_base_offset));

  /* R2 = format string length */
  emit(ctx->buf, BPF_MOV64_IMM(BPF_REG_2, str_len));

  /* Call bpf_trace_printk (Helper ID 6) */
  emit(ctx->buf, BPF_CALL_FUNC(BPF_FUNC_trace_printk));
}

void compile_expression(ASTNode *node, CodeGenContext *ctx, int target_reg) {
  if (!node)
    return;

  switch (node->type) {
  case NODE_NUMBER: {
    int64_t val = atoll(node->token.literal);
    if (val >= INT32_MIN && val <= INT32_MAX) {
      emit(ctx->buf, BPF_MOV64_IMM(target_reg, (int32_t)val));
    } else {
      emit_ld_imm64(ctx->buf, target_reg, (uint64_t)val);
    }
    break;
  }

  case NODE_IDENT: {
    const char *name = node->token.literal;
    Symbol *sym = symbol_lookup(ctx->table, name);
    if (sym && sym->stack_offset < 0) {
      emit(ctx->buf, BPF_LDX_MEM_DW(target_reg, BPF_REG_10, sym->stack_offset));
      break;
    }

    if (strcmp(name, "pid") == 0) {
      emit(ctx->buf, BPF_CALL_FUNC(BPF_FUNC_get_current_pid_tgid));
      emit(ctx->buf, BPF_ALU64_IMM(BPF_RSH, BPF_REG_0, 32));
      emit(ctx->buf, BPF_MOV64_REG(target_reg, BPF_REG_0));
      break;
    }
    if (strcmp(name, "tgid") == 0) {
      emit(ctx->buf, BPF_CALL_FUNC(BPF_FUNC_get_current_pid_tgid));
      emit(ctx->buf, BPF_ALU64_IMM(BPF_AND, BPF_REG_0, 0xFFFFFFFF));
      emit(ctx->buf, BPF_MOV64_REG(target_reg, BPF_REG_0));
      break;
    }
    if (strcmp(name, "uid") == 0) {
      emit(ctx->buf, BPF_CALL_FUNC(BPF_FUNC_get_current_uid_gid));
      emit(ctx->buf, BPF_ALU64_IMM(BPF_AND, BPF_REG_0, 0xFFFFFFFF));
      emit(ctx->buf, BPF_MOV64_REG(target_reg, BPF_REG_0));
      break;
    }
    if (strcmp(name, "gid") == 0) {
      emit(ctx->buf, BPF_CALL_FUNC(BPF_FUNC_get_current_uid_gid));
      emit(ctx->buf, BPF_ALU64_IMM(BPF_RSH, BPF_REG_0, 32));
      emit(ctx->buf, BPF_MOV64_REG(target_reg, BPF_REG_0));
      break;
    }

    if (!sym) {
      sym =
          symbol_insert(ctx->table, name, TYPE_INT, -ctx->current_stack_offset);
      ctx->current_stack_offset += 8;
    }
    emit(ctx->buf, BPF_LDX_MEM_DW(target_reg, BPF_REG_10, sym->stack_offset));
    break;
  }

  case NODE_ADD:
  case NODE_SUB:
  case NODE_MUL:
  case NODE_DIV:
  case NODE_MOD: {
    compile_expression(node->left, ctx, target_reg);
    int scratch = (target_reg < BPF_REG_5) ? target_reg + 1 : BPF_REG_2;
    compile_expression(node->right, ctx, scratch);

    uint8_t op = BPF_ADD;
    if (node->type == NODE_SUB)
      op = BPF_SUB;
    else if (node->type == NODE_MUL)
      op = BPF_MUL;
    else if (node->type == NODE_DIV)
      op = BPF_DIV;
    else if (node->type == NODE_MOD)
      op = BPF_MOD;

    emit(ctx->buf, BPF_ALU64_REG(op, target_reg, scratch));
    break;
  }

  case NODE_EQ:
  case NODE_NEQ:
  case NODE_GT:
  case NODE_LT:
  case NODE_GTE:
  case NODE_LTE: {
    compile_expression(node->left, ctx, target_reg);
    int scratch = (target_reg < BPF_REG_5) ? target_reg + 1 : BPF_REG_2;
    compile_expression(node->right, ctx, scratch);

    uint8_t jmp_op = BPF_JEQ;
    if (node->type == NODE_EQ)
      jmp_op = BPF_JEQ;
    else if (node->type == NODE_NEQ)
      jmp_op = BPF_JNE;
    else if (node->type == NODE_GT)
      jmp_op = BPF_JSGT;
    else if (node->type == NODE_LT)
      jmp_op = BPF_JSLT;
    else if (node->type == NODE_GTE)
      jmp_op = BPF_JSGE;
    else if (node->type == NODE_LTE)
      jmp_op = BPF_JSLE;

    /* If condition met, jump forward 2 instructions to load 1 */
    emit(ctx->buf, BPF_JMP_REG(jmp_op, target_reg, scratch, 2));
    emit(ctx->buf, BPF_MOV64_IMM(target_reg, 0));
    emit(ctx->buf, BPF_JMP_IMM(BPF_JA, 0, 0, 1));
    emit(ctx->buf, BPF_MOV64_IMM(target_reg, 1));
    break;
  }

  case NODE_NEG: {
    compile_expression(node->left, ctx, target_reg);
    emit(ctx->buf, BPF_ALU64_IMM(BPF_NEG, target_reg, 0));
    break;
  }

  case NODE_NOT: {
    compile_expression(node->left, ctx, target_reg);
    emit(ctx->buf, BPF_JMP_IMM(BPF_JEQ, target_reg, 0, 2));
    emit(ctx->buf, BPF_MOV64_IMM(target_reg, 0));
    emit(ctx->buf, BPF_JMP_IMM(BPF_JA, 0, 0, 1));
    emit(ctx->buf, BPF_MOV64_IMM(target_reg, 1));
    break;
  }

  case NODE_CALL: {
    const char *fn = node->left->token.literal;
    if (strcmp(fn, "ktime") == 0) {
      emit(ctx->buf, BPF_CALL_FUNC(BPF_FUNC_ktime_get_ns));
      emit(ctx->buf, BPF_MOV64_REG(target_reg, BPF_REG_0));
    } else if (strcmp(fn, "print") == 0) {
      emit_print_helper(node, ctx);
      emit(ctx->buf, BPF_MOV64_IMM(target_reg, 0));
    }
    break;
  }

  default:
    break;
  }
}

void compile_node(ASTNode *node, CodeGenContext *ctx) {
  if (!node)
    return;

  switch (node->type) {
  case NODE_PROGRAM:
    for (int i = 0; i < node->child_count; i++) {
      compile_node(node->children[i], ctx);
    }
    break;

  case NODE_PROBE: {
    /* Reset stack layout for probe entry */
    ctx->current_stack_offset = 8;
    ctx->loop_depth = 0;

    /* Insert built-in variables */
    Symbol *builtin_pid = symbol_insert(ctx->table, "pid", TYPE_INT, 0);
    if (builtin_pid)
      builtin_pid->is_builtin = true;

    /* Compile probe statements */
    compile_node(node->right, ctx);

    /* Exit: return 0 in R0 */
    emit(ctx->buf, BPF_MOV64_IMM(BPF_REG_0, 0));
    emit(ctx->buf, BPF_EXIT_INSN());
    break;
  }

  case NODE_BLOCK:
    for (int i = 0; i < node->child_count; i++) {
      compile_node(node->children[i], ctx);
    }
    break;

  case NODE_ASSIGN: {
    /* Evaluate RHS into R1 */
    compile_expression(node->right, ctx, BPF_REG_1);

    /* Find or create variable stack slot */
    const char *var_name = node->left->token.literal;
    Symbol *sym = symbol_lookup(ctx->table, var_name);
    if (!sym || sym->stack_offset >= 0) {
      if (!sym) {
        sym = symbol_insert(ctx->table, var_name, TYPE_INT,
                            -ctx->current_stack_offset);
      } else {
        sym->stack_offset = -ctx->current_stack_offset;
      }
      ctx->current_stack_offset += 8;
    }
    emit(ctx->buf, BPF_STX_MEM_DW(BPF_REG_10, BPF_REG_1, sym->stack_offset));
    break;
  }

  case NODE_EXPR_STMT:
    if (node->left) {
      if (node->left->type == NODE_CALL) {
        compile_node(node->left, ctx);
      } else {
        compile_expression(node->left, ctx, BPF_REG_1);
      }
    }
    break;

  case NODE_IF: {
    /* 1. Evaluate condition into R1 */
    compile_expression(node->left, ctx, BPF_REG_1);

    /* 2. Jump to false branch if condition is 0 */
    int jump_false_idx = ctx->buf->count;
    emit(ctx->buf, BPF_JMP_IMM(BPF_JEQ, BPF_REG_1, 0, 0 /* to patch */));

    /* 3. Emit then-block */
    compile_node(node->right, ctx);

    int exit_jumps[64];
    int exit_count = 0;

    bool has_branches = (node->child_count > 0);
    if (has_branches) {
      exit_jumps[exit_count++] = ctx->buf->count;
      emit(ctx->buf, BPF_JMP_IMM(BPF_JA, 0, 0, 0 /* to patch */));
    }

    /* Backpatch false branch */
    ctx->buf->insns[jump_false_idx].off =
        ctx->buf->count - (jump_false_idx + 1);

    /* 4. Compile ELIF and ELSE branches */
    for (int i = 0; i < node->child_count; i++) {
      ASTNode *branch = node->children[i];
      if (branch->type == NODE_ELIF) {
        compile_expression(branch->left, ctx, BPF_REG_1);

        int elif_false_idx = ctx->buf->count;
        emit(ctx->buf, BPF_JMP_IMM(BPF_JEQ, BPF_REG_1, 0, 0 /* to patch */));

        compile_node(branch->right, ctx);

        exit_jumps[exit_count++] = ctx->buf->count;
        emit(ctx->buf, BPF_JMP_IMM(BPF_JA, 0, 0, 0 /* to patch */));

        ctx->buf->insns[elif_false_idx].off =
            ctx->buf->count - (elif_false_idx + 1);
      } else if (branch->type == NODE_ELSE) {
        compile_node(branch->right, ctx);
      }
    }

    /* 5. Backpatch all branch exit jumps to land past the whole if block */
    for (int j = 0; j < exit_count; j++) {
      int idx = exit_jumps[j];
      ctx->buf->insns[idx].off = ctx->buf->count - (idx + 1);
    }
    break;
  }

  case NODE_REPEAT: {
    /* Callee-saved register R6/R7 for outer loop, R8/R9 for nested loop */
    int loop_reg = (ctx->loop_depth == 0) ? BPF_REG_6 : BPF_REG_8;
    int bound_reg = (ctx->loop_depth == 0) ? BPF_REG_7 : BPF_REG_9;
    ctx->loop_depth++;

    /* R[loop_reg] = 0 */
    emit(ctx->buf, BPF_MOV64_IMM(loop_reg, 0));

    bool is_const = (node->left && node->left->type == NODE_NUMBER);
    int32_t const_bound = 0;
    if (is_const) {
      const_bound = (int32_t)atoi(node->left->token.literal);
    } else {
      compile_expression(node->left, ctx, bound_reg);
    }

    int loop_start_pc = ctx->buf->count;

    /* Exit check: if loop_reg >= bound, jump to loop_exit */
    int exit_jump_idx = ctx->buf->count;
    if (is_const) {
      emit(ctx->buf,
           BPF_JMP_IMM(BPF_JSGE, loop_reg, const_bound, 0 /* to patch */));
    } else {
      emit(ctx->buf,
           BPF_JMP_REG(BPF_JSGE, loop_reg, bound_reg, 0 /* to patch */));
    }

    /* Loop body */
    compile_node(node->right, ctx);

    /* Increment counter: loop_reg += 1 */
    emit(ctx->buf, BPF_ALU64_IMM(BPF_ADD, loop_reg, 1));

    /* Jump backwards to loop_start_pc */
    int back_offset = loop_start_pc - (ctx->buf->count + 1);
    emit(ctx->buf, BPF_JMP_IMM(BPF_JA, 0, 0, back_offset));

    /* Backpatch exit jump */
    ctx->buf->insns[exit_jump_idx].off = ctx->buf->count - (exit_jump_idx + 1);

    ctx->loop_depth--;
    break;
  }

  case NODE_CALL:
    emit_print_helper(node, ctx);
    break;

  default:
    break;
  }
}

BytecodeBuffer *compile_ast_to_bytecode(ASTNode *root, SymbolTable *table) {
  if (!root)
    return NULL;

  BytecodeBuffer *buf = bytecode_create();
  if (!buf)
    return NULL;

  CodeGenContext ctx;
  ctx.buf = buf;
  ctx.current_stack_offset = 8;
  ctx.loop_depth = 0;

  if (table) {
    ctx.table = table;
    ctx.own_table = false;
  } else {
    ctx.table = st_create(128);
    ctx.own_table = true;
  }

  compile_node(root, &ctx);

  if (ctx.own_table) {
    st_free(ctx.table);
  }

  return buf;
}

void bytecode_dump(const BytecodeBuffer *buf) {
  if (!buf)
    return;

  printf("\n Generated eBPF Bytecode (%d instructions) \n", buf->count);
  for (int i = 0; i < buf->count; i++) {
    struct bpf_insn insn = buf->insns[i];
    unsigned char *b = (unsigned char *)&insn;

    printf("[%04d] %02x %02x %02x %02x %02x %02x %02x %02x   ", i, b[0], b[1],
           b[2], b[3], b[4], b[5], b[6], b[7]);

    uint8_t cls = BPF_CLASS(insn.code);
    uint8_t op = BPF_OP(insn.code);
    bool is_reg = (insn.code & BPF_X) != 0;

    if (insn.code == 0 && insn.dst_reg == 0 && insn.src_reg == 0 &&
        insn.off == 0) {
      printf("(ld_imm64 high 32 bits: 0x%08x)\n", (uint32_t)insn.imm);
      continue;
    }

    switch (cls) {
    case BPF_ALU64:
    case BPF_ALU: {
      const char *op_name = "unknown";
      switch (op) {
      case BPF_MOV:
        op_name = "=";
        break;
      case BPF_ADD:
        op_name = "+=";
        break;
      case BPF_SUB:
        op_name = "-=";
        break;
      case BPF_MUL:
        op_name = "*=";
        break;
      case BPF_DIV:
        op_name = "/=";
        break;
      case BPF_MOD:
        op_name = "%=";
        break;
      case BPF_AND:
        op_name = "&=";
        break;
      case BPF_OR:
        op_name = "|=";
        break;
      case BPF_XOR:
        op_name = "^=";
        break;
      case BPF_LSH:
        op_name = "<<=";
        break;
      case BPF_RSH:
        op_name = ">>=";
        break;
      case BPF_NEG:
        printf("r%d = -r%d\n", insn.dst_reg, insn.dst_reg);
        continue;
      }
      if (is_reg) {
        printf("r%d %s r%d\n", insn.dst_reg, op_name, insn.src_reg);
      } else {
        printf("r%d %s %d\n", insn.dst_reg, op_name, insn.imm);
      }
      break;
    }

    case BPF_LDX:
      printf("r%d = *(u64 *)(r%d %s %d)\n", insn.dst_reg, insn.src_reg,
             (insn.off >= 0) ? "+" : "-", abs(insn.off));
      break;

    case BPF_STX:
      printf("*(u64 *)(r%d %s %d) = r%d\n", insn.dst_reg,
             (insn.off >= 0) ? "+" : "-", abs(insn.off), insn.src_reg);
      break;

    case BPF_ST:
      printf("*(u64 *)(r%d %s %d) = %d\n", insn.dst_reg,
             (insn.off >= 0) ? "+" : "-", abs(insn.off), insn.imm);
      break;

    case BPF_LD:
      if (i + 1 < buf->count && buf->insns[i + 1].code == 0) {
        uint64_t full = ((uint64_t)(uint32_t)buf->insns[i + 1].imm << 32) |
                        ((uint64_t)(uint32_t)insn.imm);
        printf("r%d = 0x%lx (ld_imm64)\n", insn.dst_reg, (unsigned long)full);
      } else {
        printf("r%d = %d (ld)\n", insn.dst_reg, insn.imm);
      }
      break;

    case BPF_JMP:
      if (op == BPF_EXIT) {
        printf("exit\n");
      } else if (op == BPF_CALL) {
        const char *helper = "unknown";
        if (insn.imm == BPF_FUNC_trace_printk)
          helper = "bpf_trace_printk#6";
        else if (insn.imm == BPF_FUNC_ktime_get_ns)
          helper = "bpf_ktime_get_ns#5";
        else if (insn.imm == BPF_FUNC_get_current_pid_tgid)
          helper = "bpf_get_current_pid_tgid#14";
        else if (insn.imm == BPF_FUNC_get_current_uid_gid)
          helper = "bpf_get_current_uid_gid#15";
        printf("call %s\n", helper);
      } else if (op == BPF_JA) {
        printf("goto +%d (PC -> %04d)\n", insn.off, i + 1 + insn.off);
      } else {
        const char *cond = "==";
        if (op == BPF_JNE)
          cond = "!=";
        else if (op == BPF_JGT || op == BPF_JSGT)
          cond = ">";
        else if (op == BPF_JGE || op == BPF_JSGE)
          cond = ">=";
        else if (op == BPF_JLT || op == BPF_JSLT)
          cond = "<";
        else if (op == BPF_JLE || op == BPF_JSLE)
          cond = "<=";

        if (is_reg) {
          printf("if r%d %s r%d goto +%d (PC -> %04d)\n", insn.dst_reg, cond,
                 insn.src_reg, insn.off, i + 1 + insn.off);
        } else {
          printf("if r%d %s %d goto +%d (PC -> %04d)\n", insn.dst_reg, cond,
                 insn.imm, insn.off, i + 1 + insn.off);
        }
      }
      break;

    default:
      printf("opcode 0x%02x dst=%d src=%d off=%d imm=%d\n", insn.code,
             insn.dst_reg, insn.src_reg, insn.off, insn.imm);
      break;
    }
  }
}

void bytecode_print_c_array(const BytecodeBuffer *buf, const char *array_name) {
  if (!buf)
    return;
  printf("/* Auto-generated by EEL compiler */\n");
  printf("struct bpf_insn %s[] = {\n", array_name ? array_name : "prog");
  for (int i = 0; i < buf->count; i++) {
    struct bpf_insn insn = buf->insns[i];
    printf("    { .code = 0x%02x, .dst_reg = %d, .src_reg = %d, .off = %d, "
           ".imm = %d },\n",
           insn.code, insn.dst_reg, insn.src_reg, insn.off, insn.imm);
  }
  printf("};\n");
}
