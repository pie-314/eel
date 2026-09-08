#ifndef CODEGEN_H
#define CODEGEN_H

#include "../parser/parser.h"
#include "../semantic/symbol_table.h"
#include <stdint.h>
#include <stdbool.h>

#if defined(__linux__) && __has_include(<linux/bpf.h>)
#include <linux/bpf.h>
#else

/* eBPF 8-byte Instruction Layout (standard Linux kernel ABI) */
struct bpf_insn {
    uint8_t  code;         /* Opcode: Class | Source | Operation */
    uint8_t  dst_reg:4;    /* Destination register nibble (0-10) */
    uint8_t  src_reg:4;    /* Source register nibble (0-10) */
    int16_t  off;          /* Signed 16-bit offset */
    int32_t  imm;          /* Signed 32-bit immediate constant / helper ID */
};

#endif

/* Register Definitions */
#ifndef BPF_REG_0
#define BPF_REG_0  0   /* Return value / Exit code */
#define BPF_REG_1  1   /* Arg 1 / Context pointer / Scratch */
#define BPF_REG_2  2   /* Arg 2 / Scratch */
#define BPF_REG_3  3   /* Arg 3 / Scratch */
#define BPF_REG_4  4   /* Arg 4 */
#define BPF_REG_5  5   /* Arg 5 */
#define BPF_REG_6  6   /* Callee-saved (preserved across helper calls) */
#define BPF_REG_7  7   /* Callee-saved */
#define BPF_REG_8  8   /* Callee-saved */
#define BPF_REG_9  9   /* Callee-saved */
#define BPF_REG_10 10  /* Read-Only Frame Pointer (Stack top) */
#endif

/* Instruction Classes (code[2:0]) */
#ifndef BPF_CLASS
#define BPF_CLASS(code) ((code) & 0x07)
#endif
#ifndef BPF_LD
#define BPF_LD    0x00
#endif
#ifndef BPF_LDX
#define BPF_LDX   0x01
#endif
#ifndef BPF_ST
#define BPF_ST    0x02
#endif
#ifndef BPF_STX
#define BPF_STX   0x03
#endif
#ifndef BPF_ALU
#define BPF_ALU   0x04
#endif
#ifndef BPF_JMP
#define BPF_JMP   0x05
#endif
#ifndef BPF_ALU64
#define BPF_ALU64 0x07
#endif

/* Source Operand Flags (code[3]) */
#ifndef BPF_K
#define BPF_K     0x00    /* 32-bit immediate field (imm) */
#endif
#ifndef BPF_X
#define BPF_X     0x08    /* Source register (src_reg) */
#endif

/* ALU Operations (code[7:4]) */
#ifndef BPF_OP
#define BPF_OP(code)    ((code) & 0xf0)
#endif
#ifndef BPF_ADD
#define BPF_ADD   0x00
#endif
#ifndef BPF_SUB
#define BPF_SUB   0x10
#endif
#ifndef BPF_MUL
#define BPF_MUL   0x20
#endif
#ifndef BPF_DIV
#define BPF_DIV   0x30
#endif
#ifndef BPF_OR
#define BPF_OR    0x40
#endif
#ifndef BPF_AND
#define BPF_AND   0x50
#endif
#ifndef BPF_LSH
#define BPF_LSH   0x60
#endif
#ifndef BPF_RSH
#define BPF_RSH   0x70
#endif
#ifndef BPF_NEG
#define BPF_NEG   0x80
#endif
#ifndef BPF_MOD
#define BPF_MOD   0x90
#endif
#ifndef BPF_XOR
#define BPF_XOR   0xa0
#endif
#ifndef BPF_MOV
#define BPF_MOV   0xb0
#endif
#ifndef BPF_ARSH
#define BPF_ARSH  0xc0
#endif

/* Memory Size Flags */
#ifndef BPF_SIZE
#define BPF_SIZE(code)  ((code) & 0x18)
#endif
#ifndef BPF_W
#define BPF_W     0x00    /* 32-bit word */
#endif
#ifndef BPF_H
#define BPF_H     0x08    /* 16-bit halfword */
#endif
#ifndef BPF_B
#define BPF_B     0x10    /* 8-bit byte */
#endif
#ifndef BPF_DW
#define BPF_DW    0x18    /* 64-bit doubleword */
#endif
#ifndef BPF_MEM
#define BPF_MEM   0x60    /* Plain memory mode */
#endif
#ifndef BPF_IMM
#define BPF_IMM   0x00
#endif

/* Jump Operations (code[7:4]) */
#ifndef BPF_JA
#define BPF_JA    0x00    /* Unconditional jump */
#endif
#ifndef BPF_JEQ
#define BPF_JEQ   0x10    /* == */
#endif
#ifndef BPF_JGT
#define BPF_JGT   0x20    /* > (unsigned) */
#endif
#ifndef BPF_JGE
#define BPF_JGE   0x30    /* >= (unsigned) */
#endif
#ifndef BPF_JNE
#define BPF_JNE   0x50    /* != */
#endif
#ifndef BPF_JSGT
#define BPF_JSGT  0x60    /* > (signed) */
#endif
#ifndef BPF_JSGE
#define BPF_JSGE  0x70    /* >= (signed) */
#endif
#ifndef BPF_CALL
#define BPF_CALL  0x80    /* Helper call */
#endif
#ifndef BPF_EXIT
#define BPF_EXIT  0x90    /* Terminate */
#endif
#ifndef BPF_JLT
#define BPF_JLT   0xa0    /* < (unsigned) */
#endif
#ifndef BPF_JLE
#define BPF_JLE   0xb0    /* <= (unsigned) */
#endif
#ifndef BPF_JSLT
#define BPF_JSLT  0xc0    /* < (signed) */
#endif
#ifndef BPF_JSLE
#define BPF_JSLE  0xd0    /* <= (signed) */
#endif

/* Kernel Helper Function IDs */
#ifndef BPF_FUNC_trace_printk
#define BPF_FUNC_trace_printk 6
#endif
#ifndef BPF_FUNC_ktime_get_ns
#define BPF_FUNC_ktime_get_ns 5
#endif
#ifndef BPF_FUNC_get_current_pid_tgid
#define BPF_FUNC_get_current_pid_tgid 14
#endif
#ifndef BPF_FUNC_get_current_uid_gid
#define BPF_FUNC_get_current_uid_gid  15
#endif

/* Instruction Constructor Macros */
#define BPF_ALU64_IMM(op, dst, immediate) \
    ((struct bpf_insn){ .code = BPF_ALU64 | (op) | BPF_K, .dst_reg = (uint8_t)(dst), .src_reg = 0, .off = 0, .imm = (int32_t)(immediate) })

#define BPF_ALU64_REG(op, dst, src) \
    ((struct bpf_insn){ .code = BPF_ALU64 | (op) | BPF_X, .dst_reg = (uint8_t)(dst), .src_reg = (uint8_t)(src), .off = 0, .imm = 0 })

#define BPF_MOV64_IMM(dst, immediate) \
    BPF_ALU64_IMM(BPF_MOV, (dst), (immediate))

#define BPF_MOV64_REG(dst, src) \
    BPF_ALU64_REG(BPF_MOV, (dst), (src))

#define BPF_LDX_MEM_DW(dst, base, offset) \
    ((struct bpf_insn){ .code = BPF_LDX | BPF_MEM | BPF_DW, .dst_reg = (uint8_t)(dst), .src_reg = (uint8_t)(base), .off = (int16_t)(offset), .imm = 0 })

#define BPF_STX_MEM_DW(base, src, offset) \
    ((struct bpf_insn){ .code = BPF_STX | BPF_MEM | BPF_DW, .dst_reg = (uint8_t)(base), .src_reg = (uint8_t)(src), .off = (int16_t)(offset), .imm = 0 })

#define BPF_ST_MEM_DW(base, offset, immediate) \
    ((struct bpf_insn){ .code = BPF_ST | BPF_MEM | BPF_DW, .dst_reg = (uint8_t)(base), .src_reg = 0, .off = (int16_t)(offset), .imm = (int32_t)(immediate) })

#define BPF_JMP_IMM(op, dst, immediate, offset) \
    ((struct bpf_insn){ .code = BPF_JMP | (op) | BPF_K, .dst_reg = (uint8_t)(dst), .src_reg = 0, .off = (int16_t)(offset), .imm = (int32_t)(immediate) })

#define BPF_JMP_REG(op, dst, src, offset) \
    ((struct bpf_insn){ .code = BPF_JMP | (op) | BPF_X, .dst_reg = (uint8_t)(dst), .src_reg = (uint8_t)(src), .off = (int16_t)(offset), .imm = 0 })

#define BPF_CALL_FUNC(func_id) \
    ((struct bpf_insn){ .code = BPF_JMP | BPF_CALL, .dst_reg = 0, .src_reg = 0, .off = 0, .imm = (int32_t)(func_id) })

#define BPF_EXIT_INSN() \
    ((struct bpf_insn){ .code = BPF_JMP | BPF_EXIT, .dst_reg = 0, .src_reg = 0, .off = 0, .imm = 0 })

/* Dynamic Bytecode Instruction Buffer */
typedef struct {
    struct bpf_insn *insns;
    int count;
    int capacity;
} BytecodeBuffer;

/* CodeGen Context */
typedef struct {
    BytecodeBuffer *buf;
    SymbolTable *table;
    int current_stack_offset;
    int loop_depth;
    bool own_table;
} CodeGenContext;

/* Core CodeGen API */
BytecodeBuffer *bytecode_create(void);
void bytecode_free(BytecodeBuffer *buf);
void emit(BytecodeBuffer *buf, struct bpf_insn insn);
void emit_ld_imm64(BytecodeBuffer *buf, int dst, uint64_t val);

void compile_node(ASTNode *node, CodeGenContext *ctx);
void compile_expression(ASTNode *node, CodeGenContext *ctx, int target_reg);
BytecodeBuffer *compile_ast_to_bytecode(ASTNode *root, SymbolTable *table);

/* Inspection and Export Utilities */
void bytecode_dump(const BytecodeBuffer *buf);
void bytecode_print_c_array(const BytecodeBuffer *buf, const char *array_name);

#endif /* CODEGEN_H */
