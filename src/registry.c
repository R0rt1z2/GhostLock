#include "common.h"

#include <sys/system_properties.h>

struct target_entry {
  const char *label;
  const char *device_match;
  const char *banner_match;
  const char *fp_match;
  struct ksym_config ksym;
};

static const struct target_entry g_targets[] = {
  {
    .label = "TCL Smart TV Pro Android 14 (UTT2.250416.001 / AU14)",
    .device_match = "G08",
    .banner_match = "5.15.180-android14-11",
    .fp_match = "UTT2.250416.001/AU14",
    .ksym = {
      .kimage_text_base = 0xffffffc008000000ULL,

      .random_misc_fops_off = 0x01a457a0ULL,
      .ashmem_misc_fops_off = 0x0284da20ULL,
      .ashmem_fops_off = 0x01aa8cc0ULL,
      .ashmem_ioctl_off = 0x014617dcULL,
      .ashmem_compat_ioctl_off = 0x014617e0ULL,
      .ashmem_mmap_off = 0x01456fc8ULL,
      .ashmem_open_off = 0x01457690ULL,
      .ashmem_release_off = 0x01457694ULL,
      .ashmem_show_fdinfo_off = 0x014577f4ULL,
      .ashmem_read_iter_off = 0x01456df4ULL,
      .configfs_read_file_off = 0x005c0a48ULL,
      .configfs_write_bin_file_off = 0x005c12bcULL,
      .copy_splice_read_off = 0x00510620ULL,
      .noop_llseek_off = 0x0049d9a8ULL,

      .init_task_off = 0x028028c0ULL,
      .init_uts_ns_off = 0x028574c0ULL,
      .empty_zero_page_off = 0x028ea000ULL,
      .root_task_group_off = 0x028ee8c0ULL,
      .selinux_blob_sizes_off = 0x01af97a8ULL,
      .selinux_enforcing_off = 0x02940460ULL,
      .security_hook_heads_off = 0x01af90d0ULL,
      .kmalloc_caches_off = 0x01afa330ULL,
      .anon_pipe_buf_ops_off = 0x01938630ULL,

      .slide_random_boot_id_data_off = 0x027f6540ULL,
      .slide_sysctl_bootid_off = 0x0295bce1ULL,
      .slide_loggers_0_1_off = 0x026d18f0ULL,
      .slide_nfulnl_logger_off = 0x026d19c0ULL,

      .init_cred_off = 0x027c4a88ULL,
      .panic_scratch_off = 0x0294c800ULL,
      .dump_skip_zeroes_off = 0x0294c800ULL,
    },
  },
  {
    .label = "TCL Smart TV Pro Android 14 (UTT2.250416.001 / AU22)",
    .device_match = "G08",
    .banner_match = "5.15.192-android14-11",
    .fp_match = "UTT2.250416.001/AU22",
    .ksym = {
      .kimage_text_base = 0xffffffc008000000ULL,

      .random_misc_fops_off = 0x01a478e0ULL,
      .ashmem_misc_fops_off = 0x0284d920ULL,
      .ashmem_fops_off = 0x01aab080ULL,
      .ashmem_ioctl_off = 0x01465f30ULL,
      .ashmem_compat_ioctl_off = 0x01465f34ULL,
      .ashmem_mmap_off = 0x0145b6f0ULL,
      .ashmem_open_off = 0x0145bdb8ULL,
      .ashmem_release_off = 0x0145bdbcULL,
      .ashmem_show_fdinfo_off = 0x0145bf24ULL,
      .ashmem_read_iter_off = 0x0145b51cULL,
      .configfs_read_file_off = 0x005c1124ULL,
      .configfs_write_bin_file_off = 0x005c1998ULL,
      .copy_splice_read_off = 0x005107bcULL,
      .noop_llseek_off = 0x0049dad0ULL,

      .init_task_off = 0x028026c0ULL,
      .init_uts_ns_off = 0x028573b0ULL,
      .empty_zero_page_off = 0x028ea000ULL,
      .root_task_group_off = 0x028ee8c0ULL,
      .selinux_blob_sizes_off = 0x01afbf08ULL,
      .selinux_enforcing_off = 0x029404b0ULL,
      .security_hook_heads_off = 0x01afb830ULL,
      .kmalloc_caches_off = 0x01afca90ULL,
      .anon_pipe_buf_ops_off = 0x0193a630ULL,

      .slide_random_boot_id_data_off = 0x027f62d0ULL,
      .slide_sysctl_bootid_off = 0x0295bd21ULL,
      .slide_loggers_0_1_off = 0x026d18f8ULL,
      .slide_nfulnl_logger_off = 0x026d19c8ULL,

      .init_cred_off = 0x027c4978ULL,
      .panic_scratch_off = 0x0294c800ULL,
      .dump_skip_zeroes_off = 0x0294c800ULL,
    },
  },
};

#define N_TARGETS (sizeof(g_targets) / sizeof(g_targets[0]))

static void read_proc_version(char *buf, size_t len) {
  buf[0] = 0;
  int fd = open("/proc/version", O_RDONLY | O_CLOEXEC);
  if (fd < 0) {
    return;
  }
  ssize_t n = read(fd, buf, len - 1);
  close(fd);
  if (n > 0) {
    buf[n] = 0;
  }
}

static void read_fingerprint(char *buf, size_t len) {
  buf[0] = 0;
  char prop[PROP_VALUE_MAX];
  if (__system_property_get("ro.bootimage.build.fingerprint", prop) > 0) {
    snprintf(buf, len, "%s", prop);
  }
  size_t used = strlen(buf);
  if (__system_property_get("ro.build.fingerprint", prop) > 0) {
    snprintf(buf + used, len - used, "|%s", prop);
  }
}

static int device_ok(const struct target_entry *t, const char *dev) {
  if (!t->device_match) return 1;
  return dev[0] && strcmp(dev, t->device_match) == 0;
}

const char *ksym_config_select_for_device(void) {
  char banner[512];
  char fp[2 * PROP_VALUE_MAX + 2];
  char dev[PROP_VALUE_MAX];
  read_proc_version(banner, sizeof(banner));
  read_fingerprint(fp, sizeof(fp));
  if (__system_property_get("ro.product.device", dev) <= 0) {
    dev[0] = 0;
  }

  for (size_t i = 0; i < N_TARGETS; i++) {
    if (g_targets[i].fp_match && fp[0] &&
        strstr(fp, g_targets[i].fp_match)) {
      if (!device_ok(&g_targets[i], dev)) {
        continue;
      }
      g_ksym = g_targets[i].ksym;
      return g_targets[i].label;
    }
  }

  for (size_t i = 0; i < N_TARGETS; i++) {
    if (g_targets[i].banner_match && banner[0] &&
        strstr(banner, g_targets[i].banner_match)) {
      if (!device_ok(&g_targets[i], dev)) {
        pr_warning("Target: banner matches %s but device is \"%s\", not \"%s\"; "
                   "refusing\n", g_targets[i].label, dev,
                   g_targets[i].device_match);
        continue;
      }
      g_ksym = g_targets[i].ksym;
      pr_warning("Target: matched %s by kernel banner, not build fingerprint; "
                 "verify the incremental if the run fails\n",
                 g_targets[i].label);
      return g_targets[i].label;
    }
  }

  pr_warning("Target: no registry entry matched this build "
             "(banner=\"%.60s\"); using compiled-in defaults\n", banner);
  return NULL;
}
