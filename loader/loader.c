#define _GNU_SOURCE
#include "loader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <sys/syscall.h>
#include <sys/ioctl.h>
#include <linux/bpf.h>
#include <linux/perf_event.h>

static int g_pfd = -1;
static int g_prog_fd = -1;

static void cleanup_probe(int sig) {
    (void)sig;
    printf("\n[EEL Loader] Detaching probe and exiting...\n");

    if (g_pfd >= 0) {
        ioctl(g_pfd, PERF_EVENT_IOC_DISABLE, 0);
        close(g_pfd);
    }
    if (g_prog_fd >= 0) {
        close(g_prog_fd);
    }

    /* Unregister kprobe from tracefs */
    int fd = open("/sys/kernel/tracing/kprobe_events", O_WRONLY | O_APPEND);
    if (fd < 0) {
        fd = open("/sys/kernel/debug/tracing/kprobe_events", O_WRONLY | O_APPEND);
    }
    if (fd >= 0) {
        write(fd, "-:kprobes/eel_execve\n", 21);
        close(fd);
    }

    exit(0);
}

int run_loader(const char *bin_path, const char *kprobe_symbol) {
    if (!bin_path) {
        bin_path = "prog.bin";
    }
    if (!kprobe_symbol || strlen(kprobe_symbol) == 0) {
        kprobe_symbol = "__x64_sys_execve";
    }

    printf("========================================\n");
    printf("     EEL Standalone Kernel Loader       \n");
    printf("========================================\n");

    /* 1. Read the compiled instruction bytecode */
    FILE *f = fopen(bin_path, "rb");
    if (!f) {
        fprintf(stderr, "Error: cannot open binary file '%s'\n", bin_path);
        fprintf(stderr, "Hint: compile your script first with: ./eel <script.eel> -o prog.bin\n");
        return 1;
    }

    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (file_size <= 0 || file_size % 8 != 0) {
        fprintf(stderr, "Error: invalid instruction file size (%ld bytes)\n", file_size);
        fclose(f);
        return 1;
    }

    int insn_cnt = (int)(file_size / 8);
    struct bpf_insn *insns = malloc(file_size);
    if (!insns) {
        fclose(f);
        return 1;
    }

    size_t n = fread(insns, 1, file_size, f);
    fclose(f);
    if ((long)n != file_size) {
        fprintf(stderr, "Error: failed to read complete bytecode\n");
        free(insns);
        return 1;
    }

    printf("[1/4] Loaded %d instructions (%ld bytes) from '%s'\n", insn_cnt, file_size, bin_path);

    /* 2. Load into kernel via BPF_PROG_LOAD syscall */
    char log_buf[65536];
    memset(log_buf, 0, sizeof(log_buf));

    union bpf_attr attr;
    memset(&attr, 0, sizeof(attr));
    attr.prog_type = BPF_PROG_TYPE_KPROBE;
    attr.insns     = (unsigned long)insns;
    attr.insn_cnt  = insn_cnt;
    attr.license   = (unsigned long)"GPL";
    attr.log_buf   = (unsigned long)log_buf;
    attr.log_size  = sizeof(log_buf);
    attr.log_level = 1;

    g_prog_fd = syscall(__NR_bpf, BPF_PROG_LOAD, &attr, sizeof(attr));
    free(insns);

    if (g_prog_fd < 0) {
        fprintf(stderr, "\n[FATAL] Kernel Verifier Rejected Program (errno: %d: %s)\n", errno, strerror(errno));
        if (errno == EPERM) {
            fprintf(stderr, "Hint: root privileges are required to load eBPF programs. Run with sudo!\n");
        }
        if (strlen(log_buf) > 0) {
            fprintf(stderr, "\n--- Verifier Log ---\n%s\n", log_buf);
        }
        return 1;
    }

    printf("[2/4] Kernel verification PASSED! Program loaded (prog_fd: %d)\n", g_prog_fd);

    /* Setup clean shutdown */
    signal(SIGINT, cleanup_probe);
    signal(SIGTERM, cleanup_probe);

    /* 3. Register kprobe via tracefs */
    int kfd = open("/sys/kernel/tracing/kprobe_events", O_WRONLY | O_APPEND);
    if (kfd < 0) {
        kfd = open("/sys/kernel/debug/tracing/kprobe_events", O_WRONLY | O_APPEND);
    }
    if (kfd < 0) {
        fprintf(stderr, "Error opening kprobe_events: %s (Did you run with sudo?)\n", strerror(errno));
        close(g_prog_fd);
        return 1;
    }

    /* Clear old probe if exists */
    write(kfd, "-:kprobes/eel_execve\n", 21);

    char cmd[256];
    snprintf(cmd, sizeof(cmd), "p:kprobes/eel_execve %s\n", kprobe_symbol);
    if (write(kfd, cmd, strlen(cmd)) < 0) {
        /* If registration failed, try with __x64_ prefix on x86_64 */
        if (strncmp(kprobe_symbol, "__x64_", 6) != 0) {
            snprintf(cmd, sizeof(cmd), "p:kprobes/eel_execve __x64_%s\n", kprobe_symbol);
            write(kfd, cmd, strlen(cmd));
        } else {
            snprintf(cmd, sizeof(cmd), "p:kprobes/eel_execve sys_execve\n");
            write(kfd, cmd, strlen(cmd));
        }
    }
    close(kfd);

    /* Read event ID */
    FILE *id_file = fopen("/sys/kernel/tracing/events/kprobes/eel_execve/id", "r");
    if (!id_file) {
        id_file = fopen("/sys/kernel/debug/tracing/events/kprobes/eel_execve/id", "r");
    }
    if (!id_file) {
        fprintf(stderr, "Error: cannot read kprobe event ID: %s\n", strerror(errno));
        cleanup_probe(0);
        return 1;
    }

    int event_id = 0;
    if (fscanf(id_file, "%d", &event_id) != 1) {
        fprintf(stderr, "Error: failed to parse kprobe event ID\n");
        fclose(id_file);
        cleanup_probe(0);
        return 1;
    }
    fclose(id_file);

    printf("[3/4] Registered kprobe on '%s' (Event ID: %d)\n", kprobe_symbol, event_id);

    /* 4. Open perf event and attach BPF program */
    struct perf_event_attr pattr;
    memset(&pattr, 0, sizeof(pattr));
    pattr.type = PERF_TYPE_TRACEPOINT;
    pattr.size = sizeof(pattr);
    pattr.config = event_id;

    g_pfd = syscall(__NR_perf_event_open, &pattr, -1, 0, -1, 0);
    if (g_pfd < 0) {
        fprintf(stderr, "perf_event_open failed: %s\n", strerror(errno));
        cleanup_probe(0);
        return 1;
    }

    if (ioctl(g_pfd, PERF_EVENT_IOC_SET_BPF, g_prog_fd) < 0) {
        fprintf(stderr, "ioctl PERF_EVENT_IOC_SET_BPF failed: %s\n", strerror(errno));
        cleanup_probe(0);
        return 1;
    }

    if (ioctl(g_pfd, PERF_EVENT_IOC_ENABLE, 0) < 0) {
        fprintf(stderr, "ioctl PERF_EVENT_IOC_ENABLE failed: %s\n", strerror(errno));
        cleanup_probe(0);
        return 1;
    }

    printf("[4/4] Probe attached and enabled successfully!\n");
    printf("\n>>> Now streaming live kernel events (trace_pipe)...\n");
    printf(">>> (Open another terminal and run commands like 'ls', 'whoami', 'curl')\n");
    printf(">>> Press Ctrl+C to stop and detach probe.\n\n");

    /* 5. Stream trace_pipe */
    int pipe_fd = open("/sys/kernel/tracing/trace_pipe", O_RDONLY);
    if (pipe_fd < 0) {
        pipe_fd = open("/sys/kernel/debug/tracing/trace_pipe", O_RDONLY);
    }
    if (pipe_fd < 0) {
        fprintf(stderr, "Error opening trace_pipe: %s\n", strerror(errno));
        cleanup_probe(0);
        return 1;
    }

    char stream_buf[1024];
    while (1) {
        ssize_t bytes = read(pipe_fd, stream_buf, sizeof(stream_buf) - 1);
        if (bytes > 0) {
            stream_buf[bytes] = '\0';
            printf("%s", stream_buf);
            fflush(stdout);
        }
    }

    cleanup_probe(0);
    return 0;
}

#ifdef LOADER_STANDALONE
int main(int argc, char **argv) {
    const char *bin_path = (argc > 1) ? argv[1] : "prog.bin";
    const char *kprobe_symbol = (argc > 2) ? argv[2] : "__x64_sys_execve";
    return run_loader(bin_path, kprobe_symbol);
}
#endif

