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
