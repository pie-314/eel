#include <cstdint>
#include <stdint.h>

// eBPF 8-byte instruction
struct bpf_insn {
  uint8_t code;
  uint8_t dst_reg : 4;
  uint8_t src_reg : 4;
  int16_t off;
  int16_t imm;
};

// eBPF registers used by the generated code
#define BPF_REG_0 0   // return value return
#define BPF_REG_1 1   // arg 1 helper/function arg
#define BPF_REG_2 2   // arg 2
#define BPF_REG_3 3   // arg 3
#define BPF_REG_4 4   // arg 4
#define BPF_REG_5 5   // arg 5
#define BPF_REG_6 6   // callee-saved
#define BPF_REG_7 7   // callee-saved
#define BPF_REG_8 8   // callee-saved
#define BPF_REG_9 9   // callee-saved
#define BPF_REG_10 10 // read only frame pointer (stack top)
