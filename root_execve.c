typedef unsigned int __u32;
typedef unsigned long long __u64;

static long (*bpf_trace_printk)(const char *fmt, __u32 fmt_size, ...) = (void *)6;
static __u64 (*bpf_get_current_pid_tgid)(void) = (void *)14;
static __u64 (*bpf_get_current_uid_gid)(void) = (void *)15;

#define SEC(NAME) __attribute__((section(NAME), used))

SEC("kprobe/sys_execve")
int kprobe_sys_execve(void *ctx) {
    __u32 uid = (__u32)bpf_get_current_uid_gid();

    if (uid == 0) {
        __u32 pid = bpf_get_current_pid_tgid() >> 32;
        char msg[] = "SECURITY ALERT: Root exec detected! PID: %ld\n";
        bpf_trace_printk(msg, sizeof(msg), pid);
    }

    return 0;
}

char _license[] SEC("license") = "GPL";
