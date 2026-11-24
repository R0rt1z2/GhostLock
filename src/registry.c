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
    .label = "Fire OS 8.3.3.8 (RS8338.3358N / 0030132739716)",
    .device_match = "quartz",
    .banner_match = "Thu Nov 5 01:49:48 UTC 2026",
    .fp_match = "RS8338.3358N",
    .ksym = {
      .kimage_text_base = 0xffffff8008080000ULL,

      .random_misc_fops_off = 0x00dea2a0ULL,
      .ashmem_misc_fops_off = 0x0136d3f0ULL,
      .ashmem_fops_off = 0x00e6b700ULL,
      .ashmem_ioctl_off = 0x00a51510ULL,
      .ashmem_compat_ioctl_off = 0x00a51decULL,
      .ashmem_mmap_off = 0x00a51e38ULL,
      .ashmem_open_off = 0x00a51fa0ULL,
      .ashmem_release_off = 0x00a52020ULL,
      .ashmem_show_fdinfo_off = 0x00000000ULL,
      .ashmem_read_iter_off = 0x00a5145cULL,
      .configfs_read_file_off = 0x00282de4ULL,
      .configfs_write_bin_file_off = 0x002832ccULL,
      .copy_splice_read_off = 0x0021cf14ULL,
      .noop_llseek_off = 0x001ddbd8ULL,

      .init_task_off = 0x0129bc00ULL,
      .init_uts_ns_off = 0x00000000ULL,
      .empty_zero_page_off = 0x013fe000ULL,
      .root_task_group_off = 0x01404740ULL,
      .selinux_blob_sizes_off = 0x00000000ULL,
      .selinux_enforcing_off = 0x014a0111ULL,
      .security_hook_heads_off = 0x011126c8ULL,
      .kmalloc_caches_off = 0x011121b0ULL,
      .anon_pipe_buf_ops_off = 0x00d92080ULL,

      .slide_random_boot_id_data_off = 0x012f79c8ULL,
      .slide_sysctl_bootid_off = 0x014ba010ULL,
      .slide_loggers_0_1_off = 0x00000000ULL,
      .slide_nfulnl_logger_off = 0x00000000ULL,

      .init_cred_off = 0x012a9910ULL,
      .panic_scratch_off = 0x01531800ULL,
      .dump_skip_zeroes_off = 0x01530000ULL,
    },
  },
  {
    .label = "Fire OS 8.3.3.8 (RS8338.3339N / 0030132734852)",
    .device_match = "quartz",
    .banner_match = "Mon Dec 15 19:45:00 UTC 2025",
    .fp_match = "RS8338.3339N",
    .ksym = {
      .kimage_text_base = 0xffffff8008080000ULL,

      .random_misc_fops_off = 0x00dea2a0ULL,
      .ashmem_misc_fops_off = 0x0136d3f0ULL,
      .ashmem_fops_off = 0x00e6b700ULL,
      .ashmem_ioctl_off = 0x00a51510ULL,
      .ashmem_compat_ioctl_off = 0x00a51decULL,
      .ashmem_mmap_off = 0x00a51e38ULL,
      .ashmem_open_off = 0x00a51fa0ULL,
      .ashmem_release_off = 0x00a52020ULL,
      .ashmem_show_fdinfo_off = 0x00000000ULL,
      .ashmem_read_iter_off = 0x00a5145cULL,
      .configfs_read_file_off = 0x00282de4ULL,
      .configfs_write_bin_file_off = 0x002832ccULL,
      .copy_splice_read_off = 0x0021cf14ULL,
      .noop_llseek_off = 0x001ddbd8ULL,

      .init_task_off = 0x0129bc00ULL,
      .init_uts_ns_off = 0x00000000ULL,
      .empty_zero_page_off = 0x013fe000ULL,
      .root_task_group_off = 0x01404740ULL,
      .selinux_blob_sizes_off = 0x00000000ULL,
      .selinux_enforcing_off = 0x014a0111ULL,
      .security_hook_heads_off = 0x011126c8ULL,
      .kmalloc_caches_off = 0x011121b0ULL,
      .anon_pipe_buf_ops_off = 0x00d92080ULL,

      .slide_random_boot_id_data_off = 0x012f79c8ULL,
      .slide_sysctl_bootid_off = 0x014ba010ULL,
      .slide_loggers_0_1_off = 0x00000000ULL,
      .slide_nfulnl_logger_off = 0x00000000ULL,

      .init_cred_off = 0x012a9910ULL,
      .panic_scratch_off = 0x01531800ULL,
      .dump_skip_zeroes_off = 0x01530000ULL,
    },
  },
  {
    .label = "Fire OS 8.3.3.7 (RS8337.3186N / 0029461607044)",
    .device_match = "quartz",
    .banner_match = "Sat Sep 20 23:23:03 UTC 2025",
    .fp_match = "RS8337.3186N",
    .ksym = {
      .kimage_text_base = 0xffffff8008080000ULL,

      .random_misc_fops_off = 0x00dea2a0ULL,
      .ashmem_misc_fops_off = 0x0136d3f0ULL,
      .ashmem_fops_off = 0x00e6b700ULL,
      .ashmem_ioctl_off = 0x00a51510ULL,
      .ashmem_compat_ioctl_off = 0x00a51decULL,
      .ashmem_mmap_off = 0x00a51e38ULL,
      .ashmem_open_off = 0x00a51fa0ULL,
      .ashmem_release_off = 0x00a52020ULL,
      .ashmem_show_fdinfo_off = 0x00000000ULL,
      .ashmem_read_iter_off = 0x00a5145cULL,
      .configfs_read_file_off = 0x00282de4ULL,
      .configfs_write_bin_file_off = 0x002832ccULL,
      .copy_splice_read_off = 0x0021cf14ULL,
      .noop_llseek_off = 0x001ddbd8ULL,

      .init_task_off = 0x0129bc00ULL,
      .init_uts_ns_off = 0x00000000ULL,
      .empty_zero_page_off = 0x013fe000ULL,
      .root_task_group_off = 0x01404740ULL,
      .selinux_blob_sizes_off = 0x00000000ULL,
      .selinux_enforcing_off = 0x014a0111ULL,
      .security_hook_heads_off = 0x011126c8ULL,
      .kmalloc_caches_off = 0x011121b0ULL,
      .anon_pipe_buf_ops_off = 0x00d92080ULL,

      .slide_random_boot_id_data_off = 0x012f79c8ULL,
      .slide_sysctl_bootid_off = 0x014ba010ULL,
      .slide_loggers_0_1_off = 0x00000000ULL,
      .slide_nfulnl_logger_off = 0x00000000ULL,

      .init_cred_off = 0x012a9910ULL,
      .panic_scratch_off = 0x01531800ULL,
      .dump_skip_zeroes_off = 0x01530000ULL,
    },
  },
  {
    .label = "Fire OS 8.3.3.6 (RS8336.3103N / 0028656279428)",
    .device_match = "quartz",
    .banner_match = "Thu Jul 31 00:13:31 UTC 2025",
    .fp_match = "RS8336.3103N",
    .ksym = {
      .kimage_text_base = 0xffffff8008080000ULL,

      .random_misc_fops_off = 0x00dea2a0ULL,
      .ashmem_misc_fops_off = 0x0136d3f0ULL,
      .ashmem_fops_off = 0x00e6b700ULL,
      .ashmem_ioctl_off = 0x00a51510ULL,
      .ashmem_compat_ioctl_off = 0x00a51decULL,
      .ashmem_mmap_off = 0x00a51e38ULL,
      .ashmem_open_off = 0x00a51fa0ULL,
      .ashmem_release_off = 0x00a52020ULL,
      .ashmem_show_fdinfo_off = 0x00000000ULL,
      .ashmem_read_iter_off = 0x00a5145cULL,
      .configfs_read_file_off = 0x00282de4ULL,
      .configfs_write_bin_file_off = 0x002832ccULL,
      .copy_splice_read_off = 0x0021cf14ULL,
      .noop_llseek_off = 0x001ddbd8ULL,

      .init_task_off = 0x0129bc00ULL,
      .init_uts_ns_off = 0x00000000ULL,
      .empty_zero_page_off = 0x013fe000ULL,
      .root_task_group_off = 0x01404740ULL,
      .selinux_blob_sizes_off = 0x00000000ULL,
      .selinux_enforcing_off = 0x014a0111ULL,
      .security_hook_heads_off = 0x011126c8ULL,
      .kmalloc_caches_off = 0x011121b0ULL,
      .anon_pipe_buf_ops_off = 0x00d92080ULL,

      .slide_random_boot_id_data_off = 0x012f79c8ULL,
      .slide_sysctl_bootid_off = 0x014ba010ULL,
      .slide_loggers_0_1_off = 0x00000000ULL,
      .slide_nfulnl_logger_off = 0x00000000ULL,

      .init_cred_off = 0x012a9910ULL,
      .panic_scratch_off = 0x01531800ULL,
      .dump_skip_zeroes_off = 0x01530000ULL,
    },
  },
  {
    .label = "Fire OS 8.3.3.5 (RS8335.2972N / 0027716721796)",
    .device_match = "quartz",
    .banner_match = "Thu May 15 01:35:07 UTC 2025",
    .fp_match = "RS8335.2972N",
    .ksym = {
      .kimage_text_base = 0xffffff8008080000ULL,

      .random_misc_fops_off = 0x00dea2a0ULL,
      .ashmem_misc_fops_off = 0x0136d3f0ULL,
      .ashmem_fops_off = 0x00e6b700ULL,
      .ashmem_ioctl_off = 0x00a51510ULL,
      .ashmem_compat_ioctl_off = 0x00a51decULL,
      .ashmem_mmap_off = 0x00a51e38ULL,
      .ashmem_open_off = 0x00a51fa0ULL,
      .ashmem_release_off = 0x00a52020ULL,
      .ashmem_show_fdinfo_off = 0x00000000ULL,
      .ashmem_read_iter_off = 0x00a5145cULL,
      .configfs_read_file_off = 0x00282de4ULL,
      .configfs_write_bin_file_off = 0x002832ccULL,
      .copy_splice_read_off = 0x0021cf14ULL,
      .noop_llseek_off = 0x001ddbd8ULL,

      .init_task_off = 0x0129bc00ULL,
      .init_uts_ns_off = 0x00000000ULL,
      .empty_zero_page_off = 0x013fe000ULL,
      .root_task_group_off = 0x01404740ULL,
      .selinux_blob_sizes_off = 0x00000000ULL,
      .selinux_enforcing_off = 0x014a0111ULL,
      .security_hook_heads_off = 0x011126c8ULL,
      .kmalloc_caches_off = 0x011121b0ULL,
      .anon_pipe_buf_ops_off = 0x00d92080ULL,

      .slide_random_boot_id_data_off = 0x012f79c8ULL,
      .slide_sysctl_bootid_off = 0x014ba010ULL,
      .slide_loggers_0_1_off = 0x00000000ULL,
      .slide_nfulnl_logger_off = 0x00000000ULL,

      .init_cred_off = 0x012a9910ULL,
      .panic_scratch_off = 0x01531800ULL,
      .dump_skip_zeroes_off = 0x01530000ULL,
    },
  },
  {
    .label = "Fire OS 8.3.3.3 (RS8333.2734N / 0026441592452)",
    .device_match = "quartz",
    .banner_match = "Thu Jan 23 19:07:48 UTC 2025",
    .fp_match = "RS8333.2734N",
    .ksym = {
      .kimage_text_base = 0xffffff8008080000ULL,

      .random_misc_fops_off = 0x00dea2a0ULL,
      .ashmem_misc_fops_off = 0x0136d3f0ULL,
      .ashmem_fops_off = 0x00e6b700ULL,
      .ashmem_ioctl_off = 0x00a51510ULL,
      .ashmem_compat_ioctl_off = 0x00a51decULL,
      .ashmem_mmap_off = 0x00a51e38ULL,
      .ashmem_open_off = 0x00a51fa0ULL,
      .ashmem_release_off = 0x00a52020ULL,
      .ashmem_show_fdinfo_off = 0x00000000ULL,
      .ashmem_read_iter_off = 0x00a5145cULL,
      .configfs_read_file_off = 0x00282de4ULL,
      .configfs_write_bin_file_off = 0x002832ccULL,
      .copy_splice_read_off = 0x0021cf14ULL,
      .noop_llseek_off = 0x001ddbd8ULL,

      .init_task_off = 0x0129bc00ULL,
      .init_uts_ns_off = 0x00000000ULL,
      .empty_zero_page_off = 0x013fe000ULL,
      .root_task_group_off = 0x01404740ULL,
      .selinux_blob_sizes_off = 0x00000000ULL,
      .selinux_enforcing_off = 0x014a0111ULL,
      .security_hook_heads_off = 0x011126c8ULL,
      .kmalloc_caches_off = 0x011121b0ULL,
      .anon_pipe_buf_ops_off = 0x00d92080ULL,

      .slide_random_boot_id_data_off = 0x012f79c8ULL,
      .slide_sysctl_bootid_off = 0x014ba010ULL,
      .slide_loggers_0_1_off = 0x00000000ULL,
      .slide_nfulnl_logger_off = 0x00000000ULL,

      .init_cred_off = 0x012a9910ULL,
      .panic_scratch_off = 0x01531800ULL,
      .dump_skip_zeroes_off = 0x01530000ULL,
    },
  },
  {
    .label = "Fire OS 8.3.3.2 (RS8332.3115N / 0025837710212)",
    .device_match = "quartz",
    .banner_match = "Fri Nov 8 02:06:01 UTC 2024",
    .fp_match = "RS8332.3115N",
    .ksym = {
      .kimage_text_base = 0xffffff8008080000ULL,

      .random_misc_fops_off = 0x00dea2a0ULL,
      .ashmem_misc_fops_off = 0x0136d3f0ULL,
      .ashmem_fops_off = 0x00e6b700ULL,
      .ashmem_ioctl_off = 0x00a51510ULL,
      .ashmem_compat_ioctl_off = 0x00a51decULL,
      .ashmem_mmap_off = 0x00a51e38ULL,
      .ashmem_open_off = 0x00a51fa0ULL,
      .ashmem_release_off = 0x00a52020ULL,
      .ashmem_show_fdinfo_off = 0x00000000ULL,
      .ashmem_read_iter_off = 0x00a5145cULL,
      .configfs_read_file_off = 0x00282de4ULL,
      .configfs_write_bin_file_off = 0x002832ccULL,
      .copy_splice_read_off = 0x0021cf14ULL,
      .noop_llseek_off = 0x001ddbd8ULL,

      .init_task_off = 0x0129bc00ULL,
      .init_uts_ns_off = 0x00000000ULL,
      .empty_zero_page_off = 0x013fe000ULL,
      .root_task_group_off = 0x01404740ULL,
      .selinux_blob_sizes_off = 0x00000000ULL,
      .selinux_enforcing_off = 0x014a0111ULL,
      .security_hook_heads_off = 0x011126c8ULL,
      .kmalloc_caches_off = 0x011121b0ULL,
      .anon_pipe_buf_ops_off = 0x00d92080ULL,

      .slide_random_boot_id_data_off = 0x012f79c8ULL,
      .slide_sysctl_bootid_off = 0x014ba010ULL,
      .slide_loggers_0_1_off = 0x00000000ULL,
      .slide_nfulnl_logger_off = 0x00000000ULL,

      .init_cred_off = 0x012a9910ULL,
      .panic_scratch_off = 0x01531800ULL,
      .dump_skip_zeroes_off = 0x01530000ULL,
    },
  },
  {
    .label = "Fire OS 8.3.3.1 (RS8331.2608N / 0025367818372)",
    .device_match = "quartz",
    .banner_match = "Sat Oct 26 01:57:14 UTC 2024",
    .fp_match = "RS8331.2608N",
    .ksym = {
      .kimage_text_base = 0xffffff8008080000ULL,

      .random_misc_fops_off = 0x00dea2a0ULL,
      .ashmem_misc_fops_off = 0x0136d3f0ULL,
      .ashmem_fops_off = 0x00e6b700ULL,
      .ashmem_ioctl_off = 0x00a51510ULL,
      .ashmem_compat_ioctl_off = 0x00a51decULL,
      .ashmem_mmap_off = 0x00a51e38ULL,
      .ashmem_open_off = 0x00a51fa0ULL,
      .ashmem_release_off = 0x00a52020ULL,
      .ashmem_show_fdinfo_off = 0x00000000ULL,
      .ashmem_read_iter_off = 0x00a5145cULL,
      .configfs_read_file_off = 0x00282de4ULL,
      .configfs_write_bin_file_off = 0x002832ccULL,
      .copy_splice_read_off = 0x0021cf14ULL,
      .noop_llseek_off = 0x001ddbd8ULL,

      .init_task_off = 0x0129bc00ULL,
      .init_uts_ns_off = 0x00000000ULL,
      .empty_zero_page_off = 0x013fe000ULL,
      .root_task_group_off = 0x01404740ULL,
      .selinux_blob_sizes_off = 0x00000000ULL,
      .selinux_enforcing_off = 0x014a0111ULL,
      .security_hook_heads_off = 0x011126c8ULL,
      .kmalloc_caches_off = 0x011121b0ULL,
      .anon_pipe_buf_ops_off = 0x00d92080ULL,

      .slide_random_boot_id_data_off = 0x012f79c8ULL,
      .slide_sysctl_bootid_off = 0x014ba010ULL,
      .slide_loggers_0_1_off = 0x00000000ULL,
      .slide_nfulnl_logger_off = 0x00000000ULL,

      .init_cred_off = 0x012a9910ULL,
      .panic_scratch_off = 0x01531800ULL,
      .dump_skip_zeroes_off = 0x01530000ULL,
    },
  },
  {
    .label = "Fire OS 8.3.3.0 (RS8328.3033N / 0024998828420)",
    .device_match = "quartz",
    .banner_match = "Thu Oct 10 00:55:19 UTC 2024",
    .fp_match = "RS8328.3033N",
    .ksym = {
      .kimage_text_base = 0xffffff8008080000ULL,

      .random_misc_fops_off = 0x00dea2a0ULL,
      .ashmem_misc_fops_off = 0x0136d3f0ULL,
      .ashmem_fops_off = 0x00e6b700ULL,
      .ashmem_ioctl_off = 0x00a51510ULL,
      .ashmem_compat_ioctl_off = 0x00a51decULL,
      .ashmem_mmap_off = 0x00a51e38ULL,
      .ashmem_open_off = 0x00a51fa0ULL,
      .ashmem_release_off = 0x00a52020ULL,
      .ashmem_show_fdinfo_off = 0x00000000ULL,
      .ashmem_read_iter_off = 0x00a5145cULL,
      .configfs_read_file_off = 0x00282de4ULL,
      .configfs_write_bin_file_off = 0x002832ccULL,
      .copy_splice_read_off = 0x0021cf14ULL,
      .noop_llseek_off = 0x001ddbd8ULL,

      .init_task_off = 0x0129bc00ULL,
      .init_uts_ns_off = 0x00000000ULL,
      .empty_zero_page_off = 0x013fe000ULL,
      .root_task_group_off = 0x01404740ULL,
      .selinux_blob_sizes_off = 0x00000000ULL,
      .selinux_enforcing_off = 0x014a0111ULL,
      .security_hook_heads_off = 0x011126c8ULL,
      .kmalloc_caches_off = 0x011121b0ULL,
      .anon_pipe_buf_ops_off = 0x00d92080ULL,

      .slide_random_boot_id_data_off = 0x012f79c8ULL,
      .slide_sysctl_bootid_off = 0x014ba010ULL,
      .slide_loggers_0_1_off = 0x00000000ULL,
      .slide_nfulnl_logger_off = 0x00000000ULL,

      .init_cred_off = 0x012a9910ULL,
      .panic_scratch_off = 0x01531800ULL,
      .dump_skip_zeroes_off = 0x01530000ULL,
    },
  },
  {
    .label = "Fire OS 8.3.2.7 (RS8327.2526N / 0024696708740)",
    .device_match = "quartz",
    .banner_match = "Thu Oct 3 10:45:15 UTC 2024",
    .fp_match = "RS8327.2526N",
    .ksym = {
      .kimage_text_base = 0xffffff8008080000ULL,

      .random_misc_fops_off = 0x00dea2a0ULL,
      .ashmem_misc_fops_off = 0x0136d3f0ULL,
      .ashmem_fops_off = 0x00e6b700ULL,
      .ashmem_ioctl_off = 0x00a51510ULL,
      .ashmem_compat_ioctl_off = 0x00a51decULL,
      .ashmem_mmap_off = 0x00a51e38ULL,
      .ashmem_open_off = 0x00a51fa0ULL,
      .ashmem_release_off = 0x00a52020ULL,
      .ashmem_show_fdinfo_off = 0x00000000ULL,
      .ashmem_read_iter_off = 0x00a5145cULL,
      .configfs_read_file_off = 0x00282de4ULL,
      .configfs_write_bin_file_off = 0x002832ccULL,
      .copy_splice_read_off = 0x0021cf14ULL,
      .noop_llseek_off = 0x001ddbd8ULL,

      .init_task_off = 0x0129bc00ULL,
      .init_uts_ns_off = 0x00000000ULL,
      .empty_zero_page_off = 0x013fe000ULL,
      .root_task_group_off = 0x01404740ULL,
      .selinux_blob_sizes_off = 0x00000000ULL,
      .selinux_enforcing_off = 0x014a0111ULL,
      .security_hook_heads_off = 0x011126c8ULL,
      .kmalloc_caches_off = 0x011121b0ULL,
      .anon_pipe_buf_ops_off = 0x00d92080ULL,

      .slide_random_boot_id_data_off = 0x012f79c8ULL,
      .slide_sysctl_bootid_off = 0x014ba010ULL,
      .slide_loggers_0_1_off = 0x00000000ULL,
      .slide_nfulnl_logger_off = 0x00000000ULL,

      .init_cred_off = 0x012a9910ULL,
      .panic_scratch_off = 0x01531800ULL,
      .dump_skip_zeroes_off = 0x01530000ULL,
    },
  },
  {
    .label = "Fire OS 8.3.2.4 (RS8324.2314N / 0023153150596)",
    .device_match = "quartz",
    .banner_match = "Wed Jun 12 02:30:14 UTC 2024",
    .fp_match = "RS8324.2314N",
    .ksym = {
      .kimage_text_base = 0xffffff8008080000ULL,

      .random_misc_fops_off = 0x00dea2a0ULL,
      .ashmem_misc_fops_off = 0x0136d3f0ULL,
      .ashmem_fops_off = 0x00e6b700ULL,
      .ashmem_ioctl_off = 0x00a51508ULL,
      .ashmem_compat_ioctl_off = 0x00a51de4ULL,
      .ashmem_mmap_off = 0x00a51e30ULL,
      .ashmem_open_off = 0x00a51f98ULL,
      .ashmem_release_off = 0x00a52018ULL,
      .ashmem_show_fdinfo_off = 0x00000000ULL,
      .ashmem_read_iter_off = 0x00a51454ULL,
      .configfs_read_file_off = 0x00282de4ULL,
      .configfs_write_bin_file_off = 0x002832ccULL,
      .copy_splice_read_off = 0x0021cf14ULL,
      .noop_llseek_off = 0x001ddbd8ULL,

      .init_task_off = 0x0129bc00ULL,
      .init_uts_ns_off = 0x00000000ULL,
      .empty_zero_page_off = 0x013fe000ULL,
      .root_task_group_off = 0x01404740ULL,
      .selinux_blob_sizes_off = 0x00000000ULL,
      .selinux_enforcing_off = 0x014a0111ULL,
      .security_hook_heads_off = 0x01112290ULL,
      .kmalloc_caches_off = 0x01111d78ULL,
      .anon_pipe_buf_ops_off = 0x00d92080ULL,

      .slide_random_boot_id_data_off = 0x012f79c8ULL,
      .slide_sysctl_bootid_off = 0x014ba010ULL,
      .slide_loggers_0_1_off = 0x00000000ULL,
      .slide_nfulnl_logger_off = 0x00000000ULL,

      .init_cred_off = 0x012a9910ULL,
      .panic_scratch_off = 0x01531800ULL,
      .dump_skip_zeroes_off = 0x01530000ULL,
    },
  },
  {
    .label = "Fire OS 8.3.2.3 (RS8323.2196N / 0022549140612)",
    .device_match = "quartz",
    .banner_match = "Wed Apr 24 00:46:26 UTC 2024",
    .fp_match = "RS8323.2196N",
    .ksym = {
      .kimage_text_base = 0xffffff8008080000ULL,

      .random_misc_fops_off = 0x00dea2a0ULL,
      .ashmem_misc_fops_off = 0x0136d370ULL,
      .ashmem_fops_off = 0x00e6b700ULL,
      .ashmem_ioctl_off = 0x00a513c8ULL,
      .ashmem_compat_ioctl_off = 0x00a51ca4ULL,
      .ashmem_mmap_off = 0x00a51cf0ULL,
      .ashmem_open_off = 0x00a51e58ULL,
      .ashmem_release_off = 0x00a51ed8ULL,
      .ashmem_show_fdinfo_off = 0x00000000ULL,
      .ashmem_read_iter_off = 0x00a51314ULL,
      .configfs_read_file_off = 0x00282d54ULL,
      .configfs_write_bin_file_off = 0x0028323cULL,
      .copy_splice_read_off = 0x0021cea4ULL,
      .noop_llseek_off = 0x001ddb68ULL,

      .init_task_off = 0x0129bc00ULL,
      .init_uts_ns_off = 0x00000000ULL,
      .empty_zero_page_off = 0x013fe000ULL,
      .root_task_group_off = 0x01404740ULL,
      .selinux_blob_sizes_off = 0x00000000ULL,
      .selinux_enforcing_off = 0x014a0111ULL,
      .security_hook_heads_off = 0x01112290ULL,
      .kmalloc_caches_off = 0x01111d78ULL,
      .anon_pipe_buf_ops_off = 0x00d92080ULL,

      .slide_random_boot_id_data_off = 0x012f79c8ULL,
      .slide_sysctl_bootid_off = 0x014ba010ULL,
      .slide_loggers_0_1_off = 0x00000000ULL,
      .slide_nfulnl_logger_off = 0x00000000ULL,

      .init_cred_off = 0x012a9910ULL,
      .panic_scratch_off = 0x01531800ULL,
      .dump_skip_zeroes_off = 0x01530000ULL,
    },
  },
  {
    .label = "Fire OS 8.3.2.2 (RS8322.2053N / 0022045787524)",
    .device_match = "quartz",
    .banner_match = "Sat Jan 13 10:55:50 UTC 2024",
    .fp_match = "RS8322.2053N",
    .ksym = {
      .kimage_text_base = 0xffffff8008080000ULL,

      .random_misc_fops_off = 0x00dea2a0ULL,
      .ashmem_misc_fops_off = 0x0136d370ULL,
      .ashmem_fops_off = 0x00e6b700ULL,
      .ashmem_ioctl_off = 0x00a51254ULL,
      .ashmem_compat_ioctl_off = 0x00a51b30ULL,
      .ashmem_mmap_off = 0x00a51b7cULL,
      .ashmem_open_off = 0x00a51ce4ULL,
      .ashmem_release_off = 0x00a51d64ULL,
      .ashmem_show_fdinfo_off = 0x00000000ULL,
      .ashmem_read_iter_off = 0x00a511a0ULL,
      .configfs_read_file_off = 0x00282d54ULL,
      .configfs_write_bin_file_off = 0x0028323cULL,
      .copy_splice_read_off = 0x0021cea4ULL,
      .noop_llseek_off = 0x001ddb68ULL,

      .init_task_off = 0x0129bc00ULL,
      .init_uts_ns_off = 0x00000000ULL,
      .empty_zero_page_off = 0x013fe000ULL,
      .root_task_group_off = 0x01404740ULL,
      .selinux_blob_sizes_off = 0x00000000ULL,
      .selinux_enforcing_off = 0x014a0111ULL,
      .security_hook_heads_off = 0x01112148ULL,
      .kmalloc_caches_off = 0x01111c30ULL,
      .anon_pipe_buf_ops_off = 0x00d92080ULL,

      .slide_random_boot_id_data_off = 0x012f79c8ULL,
      .slide_sysctl_bootid_off = 0x014ba010ULL,
      .slide_loggers_0_1_off = 0x00000000ULL,
      .slide_nfulnl_logger_off = 0x00000000ULL,

      .init_cred_off = 0x012a9910ULL,
      .panic_scratch_off = 0x01531800ULL,
      .dump_skip_zeroes_off = 0x01530000ULL,
    },
  },
  {
    .label = "Fire OS 8.3.2.1 (RS8321.1924N / 0021911536772)",
    .device_match = "quartz",
    .banner_match = "Tue Dec 5 18:15:04 UTC 2023",
    .fp_match = "RS8321.1924N",
    .ksym = {
      .kimage_text_base = 0xffffff8008080000ULL,

      .random_misc_fops_off = 0x00dea2a0ULL,
      .ashmem_misc_fops_off = 0x0136d370ULL,
      .ashmem_fops_off = 0x00e6b780ULL,
      .ashmem_ioctl_off = 0x00a50d0cULL,
      .ashmem_compat_ioctl_off = 0x00a515e8ULL,
      .ashmem_mmap_off = 0x00a51634ULL,
      .ashmem_open_off = 0x00a5179cULL,
      .ashmem_release_off = 0x00a5181cULL,
      .ashmem_show_fdinfo_off = 0x00000000ULL,
      .ashmem_read_iter_off = 0x00a50c58ULL,
      .configfs_read_file_off = 0x00282c94ULL,
      .configfs_write_bin_file_off = 0x0028317cULL,
      .copy_splice_read_off = 0x0021cde4ULL,
      .noop_llseek_off = 0x001ddaa8ULL,

      .init_task_off = 0x0129bc00ULL,
      .init_uts_ns_off = 0x00000000ULL,
      .empty_zero_page_off = 0x013fe000ULL,
      .root_task_group_off = 0x01404740ULL,
      .selinux_blob_sizes_off = 0x00000000ULL,
      .selinux_enforcing_off = 0x014a0111ULL,
      .security_hook_heads_off = 0x011120d8ULL,
      .kmalloc_caches_off = 0x01111bc0ULL,
      .anon_pipe_buf_ops_off = 0x00d92080ULL,

      .slide_random_boot_id_data_off = 0x012f79c8ULL,
      .slide_sysctl_bootid_off = 0x014ba010ULL,
      .slide_loggers_0_1_off = 0x00000000ULL,
      .slide_nfulnl_logger_off = 0x00000000ULL,

      .init_cred_off = 0x012a9910ULL,
      .panic_scratch_off = 0x01531800ULL,
      .dump_skip_zeroes_off = 0x01530000ULL,
    },
  },
  {
    .label = "Fire OS 8.3.2.0 (RS8320.1807N / 0021777289092)",
    .device_match = "quartz",
    .banner_match = "Mon Oct 2 19:15:20 UTC 2023",
    .fp_match = "RS8320.1807N",
    .ksym = {
      .kimage_text_base = 0xffffff8008080000ULL,

      .random_misc_fops_off = 0x00dea2a0ULL,
      .ashmem_misc_fops_off = 0x0136d370ULL,
      .ashmem_fops_off = 0x00e6b780ULL,
      .ashmem_ioctl_off = 0x00a50c0cULL,
      .ashmem_compat_ioctl_off = 0x00a514e8ULL,
      .ashmem_mmap_off = 0x00a51534ULL,
      .ashmem_open_off = 0x00a5169cULL,
      .ashmem_release_off = 0x00a5171cULL,
      .ashmem_show_fdinfo_off = 0x00000000ULL,
      .ashmem_read_iter_off = 0x00a50b58ULL,
      .configfs_read_file_off = 0x00282b6cULL,
      .configfs_write_bin_file_off = 0x00283054ULL,
      .copy_splice_read_off = 0x0021ccbcULL,
      .noop_llseek_off = 0x001dd97cULL,

      .init_task_off = 0x0129bc00ULL,
      .init_uts_ns_off = 0x00000000ULL,
      .empty_zero_page_off = 0x013fe000ULL,
      .root_task_group_off = 0x01404740ULL,
      .selinux_blob_sizes_off = 0x00000000ULL,
      .selinux_enforcing_off = 0x014a0111ULL,
      .security_hook_heads_off = 0x01112090ULL,
      .kmalloc_caches_off = 0x01111b78ULL,
      .anon_pipe_buf_ops_off = 0x00d92080ULL,

      .slide_random_boot_id_data_off = 0x012f79c8ULL,
      .slide_sysctl_bootid_off = 0x014ba010ULL,
      .slide_loggers_0_1_off = 0x00000000ULL,
      .slide_nfulnl_logger_off = 0x00000000ULL,

      .init_cred_off = 0x012a9910ULL,
      .panic_scratch_off = 0x01531800ULL,
      .dump_skip_zeroes_off = 0x01530000ULL,
    },
  },
  {
    .label = "Fire OS 8.3.1.9 (RS8319.1664N / 0021508817028)",
    .device_match = "quartz",
    .banner_match = "Tue Jun 6 22:01:11 UTC 2023",
    .fp_match = "RS8319.1664N",
    .ksym = {
      .kimage_text_base = 0xffffff8008080000ULL,

      .random_misc_fops_off = 0x00dea2a0ULL,
      .ashmem_misc_fops_off = 0x0136d370ULL,
      .ashmem_fops_off = 0x00e6b700ULL,
      .ashmem_ioctl_off = 0x00a507b4ULL,
      .ashmem_compat_ioctl_off = 0x00a51090ULL,
      .ashmem_mmap_off = 0x00a510dcULL,
      .ashmem_open_off = 0x00a51244ULL,
      .ashmem_release_off = 0x00a512c4ULL,
      .ashmem_show_fdinfo_off = 0x00000000ULL,
      .ashmem_read_iter_off = 0x00a50700ULL,
      .configfs_read_file_off = 0x00282b3cULL,
      .configfs_write_bin_file_off = 0x00283024ULL,
      .copy_splice_read_off = 0x0021cc90ULL,
      .noop_llseek_off = 0x001dd950ULL,

      .init_task_off = 0x0129bc00ULL,
      .init_uts_ns_off = 0x00000000ULL,
      .empty_zero_page_off = 0x013fe000ULL,
      .root_task_group_off = 0x01404740ULL,
      .selinux_blob_sizes_off = 0x00000000ULL,
      .selinux_enforcing_off = 0x014a0111ULL,
      .security_hook_heads_off = 0x01111f90ULL,
      .kmalloc_caches_off = 0x01111a78ULL,
      .anon_pipe_buf_ops_off = 0x00d92080ULL,

      .slide_random_boot_id_data_off = 0x012f79c8ULL,
      .slide_sysctl_bootid_off = 0x014ba010ULL,
      .slide_loggers_0_1_off = 0x00000000ULL,
      .slide_nfulnl_logger_off = 0x00000000ULL,

      .init_cred_off = 0x012a9910ULL,
      .panic_scratch_off = 0x01531800ULL,
      .dump_skip_zeroes_off = 0x01530000ULL,
    },
  },
  {
    .label = "Fire OS 8.3.1.8 (RS8318.2029N / 0021441801604)",
    .device_match = "quartz",
    .banner_match = "Sun Apr 9 02:19:23 UTC 2023",
    .fp_match = "RS8318.2029N",
    .ksym = {
      .kimage_text_base = 0xffffff8008080000ULL,

      .random_misc_fops_off = 0x00dea260ULL,
      .ashmem_misc_fops_off = 0x0136d350ULL,
      .ashmem_fops_off = 0x00e6b6c0ULL,
      .ashmem_ioctl_off = 0x00a507f8ULL,
      .ashmem_compat_ioctl_off = 0x00a510d4ULL,
      .ashmem_mmap_off = 0x00a51120ULL,
      .ashmem_open_off = 0x00a51288ULL,
      .ashmem_release_off = 0x00a51308ULL,
      .ashmem_show_fdinfo_off = 0x00000000ULL,
      .ashmem_read_iter_off = 0x00a50744ULL,
      .configfs_read_file_off = 0x00282b20ULL,
      .configfs_write_bin_file_off = 0x00283008ULL,
      .copy_splice_read_off = 0x0021cc74ULL,
      .noop_llseek_off = 0x001dd950ULL,

      .init_task_off = 0x0129bc00ULL,
      .init_uts_ns_off = 0x00000000ULL,
      .empty_zero_page_off = 0x013fe000ULL,
      .root_task_group_off = 0x01404740ULL,
      .selinux_blob_sizes_off = 0x00000000ULL,
      .selinux_enforcing_off = 0x014a0111ULL,
      .security_hook_heads_off = 0x01111f10ULL,
      .kmalloc_caches_off = 0x011119f8ULL,
      .anon_pipe_buf_ops_off = 0x00d92040ULL,

      .slide_random_boot_id_data_off = 0x012f79c8ULL,
      .slide_sysctl_bootid_off = 0x014ba010ULL,
      .slide_loggers_0_1_off = 0x00000000ULL,
      .slide_nfulnl_logger_off = 0x00000000ULL,

      .init_cred_off = 0x012a9910ULL,
      .panic_scratch_off = 0x01531800ULL,
      .dump_skip_zeroes_off = 0x01530000ULL,
    },
  },
  {
    .label = "Fire OS 8.3.1.4 (RS8314.1401N / 0021139650948)",
    .device_match = "quartz",
    .banner_match = "Fri Jan 13 20:41:35 UTC 2023",
    .fp_match = "RS8314.1401N",
    .ksym = {
      .kimage_text_base = 0xffffff8008080000ULL,

      .random_misc_fops_off = 0x00dea260ULL,
      .ashmem_misc_fops_off = 0x0136d350ULL,
      .ashmem_fops_off = 0x00e6b6c0ULL,
      .ashmem_ioctl_off = 0x00a507c0ULL,
      .ashmem_compat_ioctl_off = 0x00a5109cULL,
      .ashmem_mmap_off = 0x00a510e8ULL,
      .ashmem_open_off = 0x00a51250ULL,
      .ashmem_release_off = 0x00a512d0ULL,
      .ashmem_show_fdinfo_off = 0x00000000ULL,
      .ashmem_read_iter_off = 0x00a5070cULL,
      .configfs_read_file_off = 0x00282b20ULL,
      .configfs_write_bin_file_off = 0x00283008ULL,
      .copy_splice_read_off = 0x0021cc74ULL,
      .noop_llseek_off = 0x001dd950ULL,

      .init_task_off = 0x0129bc00ULL,
      .init_uts_ns_off = 0x00000000ULL,
      .empty_zero_page_off = 0x013fe000ULL,
      .root_task_group_off = 0x01404740ULL,
      .selinux_blob_sizes_off = 0x00000000ULL,
      .selinux_enforcing_off = 0x014a0111ULL,
      .security_hook_heads_off = 0x01111f48ULL,
      .kmalloc_caches_off = 0x01111a30ULL,
      .anon_pipe_buf_ops_off = 0x00d92040ULL,

      .slide_random_boot_id_data_off = 0x012f79c8ULL,
      .slide_sysctl_bootid_off = 0x014ba010ULL,
      .slide_loggers_0_1_off = 0x00000000ULL,
      .slide_nfulnl_logger_off = 0x00000000ULL,

      .init_cred_off = 0x012a9910ULL,
      .panic_scratch_off = 0x01531800ULL,
      .dump_skip_zeroes_off = 0x01530000ULL,
    },
  },
  {
    .label = "Fire OS 8.3.1.3 (RS8313.1247N / 0021072502660)",
    .device_match = "quartz",
    .banner_match = "Mon Nov 21 19:31:14 UTC 2022",
    .fp_match = "RS8313.1247N",
    .ksym = {
      .kimage_text_base = 0xffffff8008080000ULL,

      .random_misc_fops_off = 0x00dea260ULL,
      .ashmem_misc_fops_off = 0x0136d350ULL,
      .ashmem_fops_off = 0x00e6b6c8ULL,
      .ashmem_ioctl_off = 0x00a4e388ULL,
      .ashmem_compat_ioctl_off = 0x00a4ec64ULL,
      .ashmem_mmap_off = 0x00a4ecb0ULL,
      .ashmem_open_off = 0x00a4ee18ULL,
      .ashmem_release_off = 0x00a4ee98ULL,
      .ashmem_show_fdinfo_off = 0x00000000ULL,
      .ashmem_read_iter_off = 0x00a4e2d4ULL,
      .configfs_read_file_off = 0x00282a04ULL,
      .configfs_write_bin_file_off = 0x00282eecULL,
      .copy_splice_read_off = 0x0021cb58ULL,
      .noop_llseek_off = 0x001dd834ULL,

      .init_task_off = 0x0129bc00ULL,
      .init_uts_ns_off = 0x00000000ULL,
      .empty_zero_page_off = 0x013fe000ULL,
      .root_task_group_off = 0x01404740ULL,
      .selinux_blob_sizes_off = 0x00000000ULL,
      .selinux_enforcing_off = 0x014a0111ULL,
      .security_hook_heads_off = 0x01110a38ULL,
      .kmalloc_caches_off = 0x01110520ULL,
      .anon_pipe_buf_ops_off = 0x00d92040ULL,

      .slide_random_boot_id_data_off = 0x012f79c8ULL,
      .slide_sysctl_bootid_off = 0x014ba010ULL,
      .slide_loggers_0_1_off = 0x00000000ULL,
      .slide_nfulnl_logger_off = 0x00000000ULL,

      .init_cred_off = 0x012a9910ULL,
      .panic_scratch_off = 0x01531800ULL,
      .dump_skip_zeroes_off = 0x01530000ULL,
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
