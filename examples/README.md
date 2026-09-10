# EEL Example Programs

This directory contains practical, real-world examples written in **EEL (eBPF Language)**.

---

### **Overview of Examples**

| File | Description | Target Probe Point |
|---|---|---|
| [`01_hello_world.eel`](01_hello_world.eel) | Minimal probe: intercepts process execution and prints a message | `sys_execve` |
| [`02_security_root_exec.eel`](02_security_root_exec.eel) | Security audit: alerts whenever a binary is executed as root (`uid == 0`) | `sys_execve` |
| [`03_process_killer.eel`](03_process_killer.eel) | Signal monitoring: detects when processes send kill/termination signals | `sys_enter_kill` |
| [`04_tcp_connect.eel`](04_tcp_connect.eel) | Network observability: monitors outbound IPv4 TCP connections | `tcp_v4_connect` |
| [`05_file_open_filter.eel`](05_file_open_filter.eel) | Filesystem tracking: monitors file opens filtered by PID | `sys_enter_openat` |
| [`06_latency_profiler.eel`](06_latency_profiler.eel) | Performance profiling: captures kernel timestamps with `ktime()` | `sys_enter_sync` |
| [`07_nested_logic.eel`](07_nested_logic.eel) | Complex control flow: multi-branch `if`/`elif`/`else` and arithmetic | `sys_execve` |
| [`08_repeat_loop.eel`](08_repeat_loop.eel) | Bounded loops: executes counted repeat iterations in kernel space | `sys_execve` |

---

### **How to Compile Any Example**

From the repository root:

```bash
# Print disassembled bytecode to stdout
./eel examples/01_hello_world.eel

# Compile to a 64-bit eBPF ELF object file (.o)
./eel examples/02_security_root_exec.eel -o root_exec.o

# Compile to raw instruction bytecode (.bin)
./eel examples/02_security_root_exec.eel -o root_exec.bin

# Compile to a C header file (.h)
./eel examples/02_security_root_exec.eel -o root_exec.h
```

---

### **How to Hook and Run Live in Kernel**

```bash
# 1. Compile the script to raw bytecode
./eel examples/02_security_root_exec.eel -o root_exec.bin

# 2. Load into kernel with standalone loader (requires sudo)
sudo ./eel-loader root_exec.bin

# 3. In another terminal, trigger events:
sudo whoami
```
