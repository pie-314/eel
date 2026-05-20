# EEL - EEL is eBPF Language

EEL is standalone DSL compiler which will be used for compiling code to eBPF bytecode, without third party tool chains.

## Minimum criteria
 * stand-alone
 * Simple syntax
 * Light-Weight
 * Minimum abstraction 
 * Fast compilation

## Architecture
 ```
 Source Code
    |
  Lexer
    |
  Parser
    |
   AST
    |
Verifier-safe IR
    |
eBPF Bytecode Generator
    |
Kernel Loader
 ```

## Example
```
probe sys_execve {
    print("exec called")
}
```

## Initial Targets
* kprobes
* trace logging
* helper calls
* integer variables
* maps
* conditionals

## Future Scope
* tracepoints
* XDP support
* perf events
* userspace tooling
* verifier-aware optimizations

