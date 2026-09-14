# EEL (eBPF Language)

EEL is a standalone domain-specific language (DSL) compiler and direct-ingestion runtime for extended Berkeley Packet Filter (eBPF) bytecode.

Unlike conventional eBPF development pipelines that depend on monolithic toolchains (LLVM/Clang, Python runtimes, BCC, or libbpf), EEL compiles tracing scripts directly into verified 64-bit eBPF instruction byte arrays (`struct bpf_insn`), outputs industry-standard relocatable ELF objects, and loads programs directly into the Linux kernel via bare `sys_bpf()` system calls.

---

## 1. System Architecture and Design Principles

Modern eBPF toolchains require multi-hundred megabyte compiler environments (LLVM/Clang) and kernel headers, prohibiting rapid execution in resource-constrained environments, distroless containers, and embedded appliances. EEL is implemented in pure ANSI C without external library dependencies.

```
+-------------------------------------------------------------+
|                      EEL Source (.eel)                      |
+-------------------------------------------------------------+
                               |
                               v
+-------------------------------------------------------------+
|  Lexical Scanner: lexer/lexer.c                             |
|  - Tokenizer, keyword match, literal scanning, line context |
+-------------------------------------------------------------+
                               | Token Stream
                               v
+-------------------------------------------------------------+
|  Recursive-Descent Parser: parser/parser.c                  |
|  - Operator precedence parsing, AST generation              |
+-------------------------------------------------------------+
                               | Abstract Syntax Tree (AST)
                               v
+-------------------------------------------------------------+
|  Semantic Analysis: semantic/semantic.c                     |
|  - Scoped symbol resolution (symbol_table.c)                |
|  - Type checking (int, string, map, void)                   |
|  - Verifier pre-checks (512-byte stack frame, loop bounds)  |
+-------------------------------------------------------------+
                               | Validated AST & Stack Layout
                               v
+-------------------------------------------------------------+
|  Bytecode Generator: codegen/codegen.c                      |
|  - Register allocation (R0-R10)                             |
|  - Single-pass jump backpatching (if/elif/else, repeat)     |
|  - Runtime stack string literal synthesis                   |
|  - Kernel helper function call lowering                     |
+-------------------------------------------------------------+
       |                       |                       |
       v                       v                       v
+---------------+      +---------------+      +---------------+
|  Raw Bytecode |      |   C Header    |      | 64-bit ELF    |
|  (.bin, .raw) |      |     (.h)      |      | Relocatable   |
|  Direct array |      |  Static array |      |     (.o)      |
+---------------+      +---------------+      +---------------+
                               |
                               v
+-------------------------------------------------------------+
|  Kernel Ingestion Engine: loader/loader.c (eel run)         |
|  - Direct sys_bpf(BPF_PROG_LOAD) syscall                    |
|  - Automated 64KB verifier diagnostic log extraction        |
|  - Dynamic tracefs kprobe registration                      |
|  - perf_event_open & PERF_EVENT_IOC_SET_BPF binding         |
|  - Real-time /sys/kernel/tracing/trace_pipe stream reader   |
+-------------------------------------------------------------+
```

### Core Design Constraints
* **Zero External Dependencies**: Direct compilation without LLVM, Clang, or libbpf.
* **Deterministic Single-Pass Lowering**: Direct translation from AST to target instruction stream with jump backpatching.
* **Kernel Verifier Safety**: Guaranteed adherence to eBPF limitations, including negative frame pointer indexing, 512-byte stack bounds, and bounded loop counts.
* **Multi-Format Export**: Production of raw byte streams, C headers with static instruction arrays, and standard 64-bit eBPF ELF objects (`EM_BPF`).

---

## 2. eBPF Virtual Machine Architecture and ABI

The Linux eBPF virtual machine is a 64-bit RISC register architecture operating inside the kernel address space.

### Register Allocation Map

| Register | Hardware Designation | Calling Convention / Semantics |
|---|---|---|
| `R0` | Function Return Code | Contains program return code upon exit (must be `0` for kprobes). Evaluates helper function return values. |
| `R1` | Argument 1 / Context | Receives context pointer (`struct pt_regs *`) on entry. Evaluates argument 1 during helper function calls. Expression scratchpad. |
| `R2` | Argument 2 / Scratch | Caller-saved scratchpad. Evaluates argument 2 for helper calls (e.g., string length). |
| `R3` | Argument 3 / Scratch | Caller-saved scratchpad. Evaluates argument 3 for helper calls (e.g., print format value 1). |
| `R4` | Argument 4 | Caller-saved scratchpad. Evaluates argument 4 for helper calls (e.g., print format value 2). |
| `R5` | Argument 5 | Caller-saved scratchpad. Evaluates argument 5 for helper calls (e.g., print format value 3). |
| `R6` | Callee-saved Register | Preserved across helper function invocations. Primary loop counter for outer `repeat` loops. |
| `R7` | Callee-saved Register | Preserved across helper function invocations. Loop boundary register for outer `repeat` loops. |
| `R8` | Callee-saved Register | Preserved across helper function invocations. Secondary loop counter for nested `repeat` loops. |
| `R9` | Callee-saved Register | Preserved across helper function invocations. Secondary loop boundary register for nested `repeat` loops. |
| `R10` | Frame Pointer | **Read-only** base pointer for the 512-byte stack frame. Absolute register modifications are illegal. |

### Stack Frame Memory Layout

The eBPF virtual machine provides a private 512-byte stack frame per program instance. The stack grows downwards from the frame pointer `R10`. Memory access is restricted to negative offsets relative to `R10`:

$$\text{Address} = R_{10} - \text{offset}, \quad 0 < \text{offset} \le 512$$

Memory layout generated by the EEL compiler:

```
Address               Offset       Slot Contents
------------------------------------------------------------------------
[R10 + 0]             0            Stack Top (Read-Only Base Pointer)
[R10 - 8]             -8           User Variable 1 (64-bit Doubleword)
[R10 - 16]            -16          User Variable 2 (64-bit Doubleword)
...                   ...          ...
[R10 - 256]           -256         Base of Synthesized String Buffer
[R10 - (256 - len)]   ...          String literal data (padded to 8 bytes)
...                   ...          ...
[R10 - 512]           -512         Kernel Verifier Hard Stack Boundary
------------------------------------------------------------------------
```

* **Variable Alignment**: All user stack variables are allocated on 8-byte boundaries and accessed using 64-bit doubleword loads and stores (`BPF_LDX_MEM_DW`, `BPF_STX_MEM_DW`).
* **Verifier Enforcement**: If a script declares variables or stack operations that exceed the 512-byte threshold, compilation is aborted during semantic analysis prior to code generation.

---

## 3. Instruction Set Architecture (ISA) Encoding

Every standard eBPF instruction is an 8-byte (64-bit) fixed-width structure conforming to the Linux kernel UAPI layout (`struct bpf_insn`).

```
struct bpf_insn {
    uint8_t  code;         /* Opcode bitfield: Class | Source | Operation */
    uint8_t  dst_reg:4;    /* Destination register nibble (bits 0-3) */
    uint8_t  src_reg:4;    /* Source register nibble (bits 4-7) */
    int16_t  off;          /* Signed 16-bit offset / jump target relative offset */
    int32_t  imm;          /* Signed 32-bit immediate constant value */
};
```

### Physical Bitfield Decomposition

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|    opcode     |src_reg|dst_reg|            offset             |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                           immediate                           |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

### Opcode Encoding (`code`)

The 8-bit `code` field is divided into three components:

| Bit Field | Mask | Name | Semantics |
|---|---|---|---|
| `bits [2:0]` | `0x07` | `BPF_CLASS` | Instruction class (LD, ST, ALU, JMP, etc.) |
| `bit 3` | `0x08` | `BPF_SRC` | Source operand selector: `BPF_K` (immediate) or `BPF_X` (register) |
| `bits [7:4]` | `0xF0` | `BPF_OP` / `BPF_SIZE` | Specific operation opcode or memory transfer width |

### Instruction Classes (`BPF_CLASS`)

| Class | Value | Description |
|---|---|---|
| `BPF_LD` | `0x00` | Non-standard load operation (used for 64-bit wide immediate loads) |
| `BPF_LDX` | `0x01` | Load into register from memory: `dst = *(type *)(src + off)` |
| `BPF_ST` | `0x02` | Store immediate constant into memory: `*(type *)(dst + off) = imm` |
| `BPF_STX` | `0x03` | Store register into memory: `*(type *)(dst + off) = src` |
| `BPF_ALU` | `0x04` | 32-bit arithmetic operations |
| `BPF_JMP` | `0x05` | 64-bit jump, branch, call, and exit operations |
| `BPF_ALU64` | `0x07` | 64-bit arithmetic operations |

### 64-Bit Arithmetic Operations (`BPF_ALU64`)

| Operation | Code | Assembly Notation | Formal Operation |
|---|---|---|---|
| `BPF_ADD` | `0x00` | `dst += src` | Addition |
| `BPF_SUB` | `0x10` | `dst -= src` | Subtraction |
| `BPF_MUL` | `0x20` | `dst *= src` | Unsigned multiplication |
| `BPF_DIV` | `0x30` | `dst /= src` | Unsigned integer division |
| `BPF_OR` | `0x40` | `dst \|= src` | Bitwise logical OR |
| `BPF_AND` | `0x50` | `dst &= src` | Bitwise logical AND |
| `BPF_LSH` | `0x60` | `dst <<= src` | Logical shift left |
| `BPF_RSH` | `0x70` | `dst >>= src` | Logical shift right |
| `BPF_NEG` | `0x80` | `dst = -dst` | Two's complement negation |
| `BPF_MOD` | `0x90` | `dst %= src` | Unsigned modulo remainder |
| `BPF_XOR` | `0xA0` | `dst ^= src` | Bitwise logical XOR |
| `BPF_MOV` | `0xB0` | `dst = src` | Register move / 32-bit immediate load |

### Branching and Control Flow Instructions (`BPF_JMP`)

| Operation | Code | Instruction Mnemonic | Branch Condition |
|---|---|---|---|
| `BPF_JA` | `0x00` | `goto +off` | Unconditional jump: `PC += off` |
| `BPF_JEQ` | `0x10` | `if dst == src goto +off` | Jump if equal |
| `BPF_JGT` | `0x20` | `if dst > src goto +off` | Jump if unsigned greater than |
| `BPF_JGE` | `0x30` | `if dst >= src goto +off` | Jump if unsigned greater than or equal |
| `BPF_JNE` | `0x50` | `if dst != src goto +off` | Jump if not equal |
| `BPF_JSGT` | `0x60` | `if dst > src goto +off` | Jump if signed greater than |
| `BPF_JSGE` | `0x70` | `if dst >= src goto +off` | Jump if signed greater than or equal |
| `BPF_CALL` | `0x80` | `call imm` | Invoke kernel helper function (`imm` = Helper ID) |
| `BPF_EXIT` | `0x90` | `exit` | Terminate program execution (`R0` return value) |
| `BPF_JLT` | `0xA0` | `if dst < src goto +off` | Jump if unsigned less than |
| `BPF_JLE` | `0xB0` | `if dst <= src goto +off` | Jump if unsigned less than or equal |
| `BPF_JSLT` | `0xC0` | `if dst < src goto +off` | Jump if signed less than |
| `BPF_JSLE` | `0xD0` | `if dst <= src goto +off` | Jump if signed less than or equal |

#### Relative Jump Offset Formula

eBPF branch instructions encode relative displacement calculated from the start of the instruction following the jump:

$$\text{off} = \text{Target\_Index} - (\text{Current\_Index} + 1)$$

---

## 4. Language Syntax and Grammar

EEL uses an imperative, block-scoped grammar tailored for kernel tracepoints and kprobes.

### Formal Grammar (EBNF)

```ebnf
program         ::= probe_decl*
probe_decl      ::= "probe" IDENTIFIER "{" statement* "}"

statement       ::= var_assign
                  | if_stmt
                  | repeat_stmt
                  | call_stmt

var_assign      ::= IDENTIFIER "=" expression
if_stmt         ::= "if" "(" expression ")" block
                    ("elif" "(" expression ")" block)*
                    ("else" block)?
repeat_stmt     ::= "repeat" "(" (NUMBER | IDENTIFIER) ")" block
call_stmt       ::= IDENTIFIER "(" (expression ("," expression)*)? ")"
block           ::= "{" statement* "}"

expression      ::= logical_expr
logical_expr    ::= rel_expr (("==" | "!=") rel_expr)*
rel_expr        ::= add_expr (("<" | "<=" | ">" | ">=") add_expr)*
add_expr        ::= mul_expr (("+" | "-") mul_expr)*
mul_expr        ::= unary_expr (("*" | "/" | "%") unary_expr)*
unary_expr      ::= ("-" | "!")? primary_expr
primary_expr    ::= NUMBER | STRING | IDENTIFIER | call_expr | "(" expression ")"
call_expr       ::= IDENTIFIER "(" (expression ("," expression)*)? ")"
```

### Keywords and Data Types
* **Keywords**: `probe`, `if`, `elif`, `else`, `repeat`, `int`, `string`, `map`.
* **Primitive Types**: 64-bit Signed Integer (`int`), Constant String (`string`).
* **Built-in System Variables**:
  * `pid`: Lower 32 bits of current thread/process group identifier.
  * `tgid`: Upper 32 bits of process group identifier.
  * `uid`: User ID of executing process context.
  * `gid`: Group ID of executing process context.
* **Kernel Helpers**:
  * `print(format_string, ...)`: Formats and streams trace log messages to kernel tracefs.
  * `ktime()`: Monotonic system clock timestamp in nanoseconds.

---

## 5. Compiler Internals and Lowering Mechanisms

### 5.1 Local Variable Stack Allocation
Variable declarations are inferred upon first assignment. The semantic analyzer registers the variable symbol in the current scope table and assigns it an explicit 8-byte stack offset:

$$\text{Offset}_{new} = -(\text{current\_stack\_offset} + 8)$$

Accessing the variable generates memory load and store instructions directly against `R10`:
```c
/* Write: var = R1 */
BPF_STX_MEM_DW(BPF_REG_10, BPF_REG_1, sym->stack_offset);

/* Read: Rtarget = var */
BPF_LDX_MEM_DW(Rtarget, BPF_REG_10, sym->stack_offset);
```

### 5.2 Single-Pass Jump Backpatching
The compiler lowers conditional branches and loops using forward-referencing relative jump backpatching:

1. **Condition Evaluation**: The condition expression is evaluated into `R1`.
2. **Placeholder Branch**: A conditional jump instruction (`BPF_JMP_IMM(BPF_JEQ, BPF_REG_1, 0, 0)`) is emitted with a placeholder offset of `0`. The instruction index $idx_{cond}$ is recorded.
3. **Branch Emission**: The body block instructions are emitted into the buffer.
4. **Exit Jump**: If subsequent branches (`elif`/`else`) exist, an unconditional exit jump (`BPF_JA`) is emitted with a placeholder offset and registered into an exit tracking array.
5. **Backpatching**: The target displacement is computed and written into instruction $idx_{cond}$:
   $$\text{insn}[idx_{cond}].\text{off} = \text{buf}\to\text{count} - (idx_{cond} + 1)$$
6. **Exit Convergence**: After all branches are emitted, all deferred exit jumps are patched to land at the current instruction boundary.

### 5.3 Bounded Loop Lowering (`repeat`)
The Linux kernel verifier prohibits unbounded loops. Bounded `repeat (N)` constructs are emitted with explicit induction variables stored in callee-saved registers (`R6` or `R8`):

1. **Counter Initialization**: Emit `BPF_MOV64_IMM(R6, 0)`.
2. **Loop Boundary Test**: Emit `BPF_JMP_IMM(BPF_JSGE, R6, N, 0)` with a placeholder offset.
3. **Body Generation**: Recursively compile statements contained in the block.
4. **Induction Step**: Emit `BPF_ALU64_IMM(BPF_ADD, R6, 1)`.
5. **Loop Backedge**: Emit backward jump `BPF_JMP_IMM(BPF_JA, 0, 0, back_offset)`.
6. **Exit Patch**: Backpatch the exit comparison from Step 2 to target the instruction following the backedge.

### 5.4 String Literal Stack Synthesis
Zero-dependency compilation cannot rely on runtime ELF relocators to resolve global `.rodata` string pointers. EEL solves this by synthesizing string constants onto the stack frame at runtime:

1. Format strings are aligned and padded to 8-byte boundaries ($L_{padded} = \lceil (\text{strlen}(s) + 1) / 8 \rceil \times 8$).
2. The string is sliced into 64-bit integer chunks.
3. For each 8-byte chunk, a 64-bit immediate load (`emit_ld_imm64`) stores the chunk into scratch register `R1`.
4. The register contents are stored onto the stack at `[R10 - 256 + offset]` via `BPF_STX_MEM_DW`.
5. `R1` is set to point to the base of the synthesized buffer: `R1 = R10 - 256`.
6. `R2` is assigned the length of the string: `R2 = strlen(s) + 1`.

### 5.5 Kernel Helper Lowering Specifications

#### `print(...)` -> `bpf_trace_printk` (Helper ID 6)
* **Kernel Signature**: `long bpf_trace_printk(const char *fmt, u32 fmt_size, ...)`
* **Register Calling Convention**:
  * `R1`: Memory address of format string buffer (`R10 - 256`).
  * `R2`: Size of format string in bytes (including null terminator).
  * `R3`: Evaluated 64-bit parameter 1 (if present).
  * `R4`: Evaluated 64-bit parameter 2 (if present).
  * `R5`: Evaluated 64-bit parameter 3 (if present).
* **Emission**: Emits `BPF_CALL_FUNC(BPF_FUNC_trace_printk)`. Output is written by the kernel to `/sys/kernel/tracing/trace_pipe`.

#### `ktime()` -> `bpf_ktime_get_ns` (Helper ID 5)
* **Kernel Signature**: `u64 bpf_ktime_get_ns(void)`
* **Emission**: Emits `BPF_CALL_FUNC(BPF_FUNC_ktime_get_ns)`.
* **Output**: Monotonic nanosecond timestamp stored in return register `R0`, moved to target evaluation register.

#### Process Identifiers -> `bpf_get_current_pid_tgid` (Helper ID 14)
* **Kernel Signature**: `u64 bpf_get_current_pid_tgid(void)`
* **Returns**: 64-bit integer where `upper 32 bits = TGID`, `lower 32 bits = PID`.
* **Lowering**:
  * `pid`: `call 14` followed by `BPF_ALU64_IMM(BPF_RSH, R0, 32)`.
  * `tgid`: `call 14` followed by `BPF_ALU64_IMM(BPF_AND, R0, 0xFFFFFFFF)`.

#### User Identifiers -> `bpf_get_current_uid_gid` (Helper ID 15)
* **Kernel Signature**: `u64 bpf_get_current_uid_gid(void)`
* **Returns**: 64-bit integer where `upper 32 bits = GID`, `lower 32 bits = UID`.
* **Lowering**:
  * `uid`: `call 15` followed by `BPF_ALU64_IMM(BPF_AND, R0, 0xFFFFFFFF)`.
  * `gid`: `call 15` followed by `BPF_ALU64_IMM(BPF_RSH, R0, 32)`.

---

## 6. Output Binary Formats and Object Model

The EEL compiler supports four distinct emission targets:

### 6.1 Disassembled Bytecode Stream (Default, stdout)
Human-readable instruction listing including instruction index, raw 8-byte hexadecimal encoding, register assignments, offsets, and kernel helper symbolic names:

```
[0000] 85 00 00 00 0f 00 00 00   call bpf_get_current_uid_gid#15
[0001] 57 00 00 00 ff ff ff ff   r0 &= -1
[0002] bf 01 00 00 00 00 00 00   r1 = r0
[0003] b7 02 00 00 00 00 00 00   r2 = 0
[0004] 1d 21 02 00 00 00 00 00   if r1 == r2 goto +2 (PC -> 0007)
```

### 6.2 Raw Bytecode (`.bin`, `.raw`)
Direct sequence of 8-byte `struct bpf_insn` instructions. The file size is strictly a multiple of 8 ($N \times 8 \text{ bytes}$). Suitable for low-overhead embedded execution or direct ingestion into bare `sys_bpf` syscalls.

### 6.3 C Header (`.h`)
Produces a C header file containing a static constant array of `struct bpf_insn` elements and preprocessor definitions for direct inclusion into C/C++ projects:

```c
/* Auto-generated by EEL (eBPF Language) Compiler */
#ifndef SYS_EXECVE_PROBE_BPF_H
#define SYS_EXECVE_PROBE_BPF_H

#include <stdint.h>

struct bpf_insn {
    uint8_t  code;
    uint8_t  dst_reg:4;
    uint8_t  src_reg:4;
    int16_t  off;
    int32_t  imm;
};

static const struct bpf_insn sys_execve_probe[24] = {
    { .code = 0x18, .dst_reg = 1, .src_reg = 0, .off = 0, .imm = 1852402248 },
    ...
};

#define SYS_EXECVE_PROBE_BPF_H_INSN_CNT 24

#endif
```

### 6.4 64-Bit eBPF Relocatable ELF Object (`.o`)
Generates standard relocatable ELF files (`ET_REL`) targeting the eBPF machine type (`EM_BPF = 247`):

```
ELF Header:
  Class:                             ELF64
  Data:                              2's complement, little endian
  Type:                              REL (Relocatable file)
  Machine:                           Linux BPF (247)

Section Headers:
  [Nr] Name              Type             Address           Offset
  [ 0]                   NULL             0000000000000000  00000000
  [ 1] kprobe/<symbol>   PROGBITS         0000000000000000  00000040 (Instructions)
  [ 2] license           PROGBITS         0000000000000000  ...      ("GPL\0")
  [ 3] .shstrtab         STRTAB           0000000000000000  ...      (Section Names)
```

Generated ELF files can be loaded with standard tooling, including `bpftool`, `iproute2`, and `libbpf`.

---

## 7. Kernel Ingestion and Standalone Loader Subsystem

The runtime ingestion engine (`loader/loader.c`) loads and attaches eBPF bytecode into the running Linux kernel without requiring `libbpf` or external userspace daemons.

### Ingestion Lifecycle

```
[ Step 1: Read Bytecode ]
       |
       v
[ Step 2: sys_bpf(BPF_PROG_LOAD) ] ---> Verification Failure?
       |                                      |
       | Verification Success                 v
       v                                [ Read & Dump 64KB Verifier Log ]
[ Step 3: Register kprobe ]
       | Open /sys/kernel/tracing/kprobe_events
       | Write: "p:kprobes/eel_execve <symbol>"
       v
[ Step 4: Resolve Tracepoint Event ID ]
       | Read /sys/kernel/tracing/events/kprobes/eel_execve/id
       v
[ Step 5: perf_event_open() ]
       | Type = PERF_TYPE_TRACEPOINT, Config = Event ID
       v
[ Step 6: ioctl() Attachment ]
       | ioctl(pfd, PERF_EVENT_IOC_SET_BPF, prog_fd)
       | ioctl(pfd, PERF_EVENT_IOC_ENABLE, 0)
       v
[ Step 7: Stream Kernel Events ]
       | Read /sys/kernel/tracing/trace_pipe
       v
[ Step 8: Signal Interruption (SIGINT/SIGTERM) ]
       | ioctl(pfd, PERF_EVENT_IOC_DISABLE)
       | Close pfd & prog_fd
       | Unregister kprobe ("-:kprobes/eel_execve")
```

### Syscall Interface (`BPF_PROG_LOAD`)
Ingestion issues a direct syscall to `__NR_bpf` ($321$ on x86-64):

```c
union bpf_attr attr;
memset(&attr, 0, sizeof(attr));
attr.prog_type = BPF_PROG_TYPE_KPROBE;
attr.insns     = (unsigned long)insns;
attr.insn_cnt  = insn_cnt;
attr.license   = (unsigned long)"GPL";
attr.log_buf   = (unsigned long)log_buf;
attr.log_size  = sizeof(log_buf);
attr.log_level = 1;

int prog_fd = syscall(__NR_bpf, BPF_PROG_LOAD, &attr, sizeof(attr));
```

If the kernel verifier rejects the bytecode, the loader captures and dumps the verifier log buffer (`log_buf`) to stderr, pinpointing the failing instruction.

---

## 8. Build System and CLI Reference

### Prerequisites
* GCC (9.0+) or Clang
* GNU Make
* Linux Kernel 4.18+ with eBPF and tracing subsystems enabled:
  * `CONFIG_BPF=y`
  * `CONFIG_BPF_SYSCALL=y`
  * `CONFIG_KPROBES=y`
  * `CONFIG_BPF_EVENTS=y`
  * `CONFIG_TRACING=y`

### Compilation

```bash
# Build the unified compiler and runner (./eel)
make

# Build the standalone binary loader utility (./eel-loader)
make eel-loader

# Clean build artifacts
make clean
```

### Command Line Interface

```
Usage: ./eel <file.eel> [-o <output_file>]
       ./eel run <eBPF_bin_name | source.eel> [probe_symbol]

Commands:
  run <file>   Compile/load eBPF binary into kernel and stream trace events

Options:
  -o <file>    Output compiled bytecode to <file>
               Supported formats (determined by extension):
                 .bin, .raw  - Raw binary eBPF instruction bytes
                 .h          - C header file with struct bpf_insn array
                 .o          - Standard 64-bit eBPF ELF relocatable object
  -h, --help   Display command help
```

---

## 9. Disassembly Walkthrough

The following walkthrough demonstrates how an EEL security probe compiles down to bare-metal eBPF bytecode.

### Source Program (`security_root_exec.eel`)

```c
probe sys_execve {
    if (uid == 0) {
        print("SECURITY ALERT: Root execution detected! PID: %ld", pid)
    }
}
```

### Generated Disassembly (`./eel security_root_exec.eel`)

```
 Generated eBPF Bytecode (40 instructions) 
[0000] 85 00 00 00 0f 00 00 00   call bpf_get_current_uid_gid#15
[0001] 57 00 00 00 ff ff ff ff   r0 &= -1                           ; Extract UID (lower 32 bits)
[0002] bf 01 00 00 00 00 00 00   r1 = r0                            ; Load UID into R1
[0003] b7 02 00 00 00 00 00 00   r2 = 0                             ; Load 0 into R2
[0004] 1d 21 02 00 00 00 00 00   if r1 == r2 goto +2 (PC -> 0007)   ; Compare uid == 0
[0005] b7 01 00 00 00 00 00 00   r1 = 0                             ; Condition false
[0006] 05 00 01 00 00 00 00 00   goto +1 (PC -> 0008)
[0007] b7 01 00 00 01 00 00 00   r1 = 1                             ; Condition true
[0008] 15 01 1d 00 00 00 00 00   if r1 == 0 goto +29 (PC -> 0038)   ; Branch on condition
[0009] 85 00 00 00 0e 00 00 00   call bpf_get_current_pid_tgid#14   ; Fetch process PID/TGID
[0010] 77 00 00 00 20 00 00 00   r0 >>= 32                          ; Extract PID
[0011] bf 03 00 00 00 00 00 00   r3 = r0                            ; R3 = print parameter 1
[0012] 18 01 00 00 53 45 43 55   r1 = 0x5954495255434553 (ld_imm64) ; Synthesize format string
[0013] 00 00 00 00 52 49 54 59   (ld_imm64 high 32 bits)
[0014] 7b 1a 00 ff 00 00 00 00   *(u64 *)(r10 - 256) = r1           ; Store chunk 0 on stack
[0015] 18 01 00 00 20 41 4c 45   r1 = 0x203a5452454c4120 (ld_imm64)
[0016] 00 00 00 00 52 54 3a 20   (ld_imm64 high 32 bits)
[0017] 7b 1a 08 ff 00 00 00 00   *(u64 *)(r10 - 248) = r1           ; Store chunk 1 on stack
[0018] 18 01 00 00 52 6f 6f 74   r1 = 0x65786520746f6f52 (ld_imm64)
[0019] 00 00 00 00 20 65 78 65   (ld_imm64 high 32 bits)
[0020] 7b 1a 10 ff 00 00 00 00   *(u64 *)(r10 - 240) = r1           ; Store chunk 2 on stack
[0021] 18 01 00 00 63 75 74 69   r1 = 0x64206e6f69747563 (ld_imm64)
[0022] 00 00 00 00 6f 6e 20 64   (ld_imm64 high 32 bits)
[0023] 7b 1a 18 ff 00 00 00 00   *(u64 *)(r10 - 232) = r1           ; Store chunk 3 on stack
[0024] 18 01 00 00 65 74 65 63   r1 = 0x2164657463657465 (ld_imm64)
[0025] 00 00 00 00 74 65 64 21   (ld_imm64 high 32 bits)
[0026] 7b 1a 20 ff 00 00 00 00   *(u64 *)(r10 - 224) = r1           ; Store chunk 4 on stack
[0027] 18 01 00 00 20 50 49 44   r1 = 0x6c25203a44495020 (ld_imm64)
[0028] 00 00 00 00 3a 20 25 6c   (ld_imm64 high 32 bits)
[0029] 7b 1a 28 ff 00 00 00 00   *(u64 *)(r10 - 216) = r1           ; Store chunk 5 on stack
[0030] 18 01 00 00 64 0a 00 00   r1 = 0xa64 (ld_imm64)
[0031] 00 00 00 00 00 00 00 00   (ld_imm64 high 32 bits)
[0032] 7b 1a 30 ff 00 00 00 00   *(u64 *)(r10 - 208) = r1           ; Store chunk 6 on stack
[0033] bf a1 00 00 00 00 00 00   r1 = r10                           ; R1 = R10
[0034] 07 01 00 00 00 ff ff ff   r1 += -256                         ; R1 = pointer to string
[0035] b7 02 00 00 33 00 00 00   r2 = 51                            ; R2 = string length (51 bytes)
[0036] 85 00 00 00 06 00 00 00   call bpf_trace_printk#6            ; Trigger kernel log write
[0037] 05 00 00 00 00 00 00 00   goto +0 (PC -> 0038)               ; Exit jump backpatched
[0038] b7 00 00 00 00 00 00 00   r0 = 0                             ; Return 0
[0039] 95 00 00 00 00 00 00 00   exit                               ; Terminate probe
```

---

## 10. Verification and Live Ingestion Walkthrough

### Compiling and Running Live in One Step
The compiler includes a unified execution pipeline that compiles source files on the fly and streams live kernel events:

```bash
sudo ./eel run examples/02_security_root_exec.eel
```

### Manual Compilation and Detached Loading

```bash
# 1. Compile EEL source to raw eBPF instruction bytes
./eel examples/02_security_root_exec.eel -o prog.bin

# 2. Compile to standard 64-bit eBPF ELF relocatable object
./eel examples/02_security_root_exec.eel -o prog.o

# 3. Compile to C header
./eel examples/02_security_root_exec.eel -o prog.h

# 4. Load binary into kernel and attach kprobe (requires root privileges)
sudo ./eel-loader prog.bin __x64_sys_execve
```

### Live Output Stream

In another terminal, executing commands as root (`sudo whoami`) triggers the probe, producing real-time trace events from `/sys/kernel/tracing/trace_pipe`:

```
           <...>-34190 [002] d... 24194.882190: bpf_trace_printk: SECURITY ALERT: Root execution detected! PID: 34190
```

---

## 11. Reference Programs

The repository provides production test probes demonstrating various language features:

| File | Description | Target Probe |
|---|---|---|
| `examples/01_hello_world.eel` | Intercepts system executions and emits trace log | `sys_execve` |
| `examples/02_security_root_exec.eel` | Security audit monitoring execution under `uid == 0` | `sys_execve` |
| `examples/03_process_killer.eel` | Intercepts signal transmission events | `sys_enter_kill` |
| `examples/04_tcp_connect.eel` | Observes outbound IPv4 TCP connection establishments | `tcp_v4_connect` |
| `examples/05_file_open_filter.eel` | Monitors filesystem file opens filtered by PID threshold | `sys_enter_openat` |
| `examples/06_latency_profiler.eel` | Captures monotonic timestamps using `ktime()` | `sys_enter_sync` |
| `examples/07_nested_logic.eel` | Multi-branch `if`/`elif`/`else` control flow and arithmetic | `sys_execve` |
| `examples/08_repeat_loop.eel` | Bounded counted loops via callee-saved registers | `sys_execve` |

---

## 12. Technical Specifications and Constraints

| Parameter | Limit / Specification | Enforcement Mechanism |
|---|---|---|
| Max Stack Frame Size | 512 bytes | Semantic analyzer verifies total stack allocation <= 512 bytes |
| Register Count | 11 registers ($R_0 - R_{10}$) | Enforced by eBPF VM ISA |
| Max Loop Iterations | 256 iterations | Semantic analyzer validates static loop count 0 < N <= 256 |
| Nested Loop Depth | 2 levels | Callee-saved register pool ($R_6/R_7$ depth 0, $R_8/R_9$ depth 1) |
| Max `print()` Arguments | Format string + 3 values | eBPF ABI parameter register limit ($R_3, R_4, R_5$) |
| String Literal Buffer | Stack offset `[R10 - 256]` | 64-bit doubleword padded chunking mechanism |
| Target Machine Architecture | 64-bit little-endian (`EM_BPF = 247`) | ELF generator and instruction constructor |
| Supported Output Formats | `.bin`, `.raw`, `.h`, `.o`, stdout disassembly | Output file extension inspection in CLI frontend |

---

## 13. Development Roadmap

### Phase 1: Lexical Analysis
- [x] Core token scanner (`lexer/lexer.c`)
- [x] Keyword, operator, string, and integer literal recognition
- [x] Line and column tracking for diagnostic errors

### Phase 2: Syntactic Analysis
- [x] Recursive-descent parser (`parser/parser.c`)
- [x] Abstract Syntax Tree construction (`ASTNode`)
- [x] Precedence climbing for binary arithmetic and comparison operations

### Phase 3: Semantic Analysis and Type Checking
- [x] Scoped symbol tables (`semantic/symbol_table.c`)
- [x] Type verification (integers, strings, maps)
- [x] Static verifier bounds validation (512-byte stack frame check, bounded loop checks)

### Phase 4: Bytecode Generation
- [x] Direct 64-bit eBPF instruction emission (`codegen/codegen.c`)
- [x] Deterministic register allocation
- [x] Jump displacement backpatching for `if`/`elif`/`else` and `repeat`
- [x] Runtime stack string synthesis
- [x] Helper call lowering (`bpf_trace_printk`, `bpf_ktime_get_ns`, `bpf_get_current_pid_tgid`, `bpf_get_current_uid_gid`)
- [x] Multi-format file writers: Raw binary (`.bin`/`.raw`), C header (`.h`), relocatable ELF (`.o`), and human-readable disassembler

### Phase 5: Kernel Integration and Loader
- [x] Direct `sys_bpf(BPF_PROG_LOAD)` syscall ingestion (`loader/loader.c`)
- [x] Automated 64KB kernel verifier log retrieval on rejection
- [x] Tracefs dynamic kprobe registration and tracepoint event attachment
- [x] Perf event binding via `perf_event_open` and `PERF_EVENT_IOC_SET_BPF`
- [x] Signal-safe detachment (`SIGINT`, `SIGTERM`)
- [x] Unified `./eel run` one-step compilation and streaming interface

### Phase 6: Future Architecture
- [ ] Kernel tracepoint probe declarations (`tracepoint:sys_enter_*`)
- [ ] Return probes (`kretprobe`, `uretprobe`)
- [ ] eBPF Map storage primitives (`BPF_MAP_TYPE_HASH`, `BPF_MAP_TYPE_ARRAY`)
- [ ] Map helper integration (`bpf_map_lookup_elem`, `bpf_map_update_elem`, `bpf_map_delete_elem`)
- [ ] Ring buffer event transport (`BPF_MAP_TYPE_RINGBUF`)
- [ ] XDP (eXpress Data Path) high-throughput packet filtering target
