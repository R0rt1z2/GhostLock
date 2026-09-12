#define _GNU_SOURCE
#include "gl_profile.h"
#include "gl_profiles_embedded.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/system_properties.h>
#include <sys/utsname.h>

enum profile_field {
    PF_VERSION,
    PF_TARGET,
    PF_VERIFICATION,
    PF_PRODUCT,
    PF_MODEL,
    PF_FINGERPRINT,
    PF_INCREMENTAL,
    PF_KERNEL_RELEASE,
    PF_MACHINE,
    PF_EXPLOIT_PATH,
    PF_WAITER_CORE,
    PF_CONSUMER_CORE,
    PF_CONSUMER_DELAY,
    PF_INIT_TASK,
    PF_PI_TASK,
    PF_ASHMEM_FOPS,
    PF_ASHMEM_MISC_FOPS,
    PF_ASHMEM_OPEN,
    PF_ASHMEM_RELEASE,
    PF_ASHMEM_IOCTL,
    PF_ASHMEM_MMAP,
    PF_CONFIGFS_READ,
    PF_CONFIGFS_WRITE,
    PF_TASK_TASKS,
    PF_TASK_REAL_CRED,
    PF_TASK_CRED,
    PF_TASK_COMM,
    PF_FOPS_READ,
    PF_FOPS_IOCTL,
    PF_FOPS_MMAP,
    PF_FOPS_OPEN,
    PF_FOPS_RELEASE,
    PF_CONFIGFS_PAGE,
    PF_CONFIGFS_MUTEX,
    PF_CONFIGFS_NRF,
    PF_FUTEX_HASH_LEN,
    PF_FUTEX_HASH_SEED,
    PF_FUTEX_HASH_BUCKETS,
    PF_STAMP_WAITER_DEPTH,
    PF_STAMP_COPY_DEPTH,
    PF_STAMP_SOCKET_KIND,
    PF_SELINUX_ENFORCING,
    PF_CRED_CAP_PERMITTED,
    PF_CRED_CAP_EFFECTIVE,
    PF_CRED_CAP_BSET,
    PF_TASK_PRIO,
    PF_TASK_STATIC_PRIO,
    PF_TASK_NORMAL_PRIO,
    PF_TASK_RT_PRIORITY,
    PF_TASK_SCHED_CLASS,
    PF_TASK_PI_LOCK,
    PF_TASK_PI_WAITERS,
    PF_TASK_PI_TOP_TASK,
    PF_TASK_PI_BLOCKED_ON,
    PF_SELINUX_STATUS_PAGE,
    PF_MEM_MAP,
    PF_PFN_OFFSET,
    PF_PAGE_STRUCT_SIZE,
    PF_LOWMEM_VA_SUB,
    PF_OTA_SHA256,
    PF_BOOT_SHA256,
    PF_KERNEL_SHA256,
    PF_CONFIG_SHA256,
    PF_SECOND_SHA256,
    PF_ANCHORS_SHA256,
    PF_REFERENCE,
    PF_KERNEL_BASE,
    PF_KERNEL_SIZE,
    PF_SIMILARITY_PPM,
    PF_COUNT
};

static void set_error(char *error, size_t error_size, const char *format, ...) {
    if (!error || !error_size)
        return;
    va_list ap;
    va_start(ap, format);
    vsnprintf(error, error_size, format, ap);
    va_end(ap);
}

static char *trim(char *text) {
    while (isspace((unsigned char)*text))
        text++;
    char *end = text + strlen(text);
    while (end > text && isspace((unsigned char)end[-1]))
        *--end = 0;
    return text;
}

#define PF_WORDS ((PF_COUNT + 63u) / 64u)

static int field_seen(const uint64_t *present, enum profile_field field) {
    return (present[field / 64u] >> (field % 64u)) & 1u;
}

static void field_mark(uint64_t *present, enum profile_field field) {
    present[field / 64u] |= UINT64_C(1) << (field % 64u);
}

static int assign_string(char *destination, size_t destination_size, const char *value,
                         uint64_t *present, enum profile_field field, const char *key, char *error,
                         size_t error_size) {
    if (field_seen(present, field)) {
        set_error(error, error_size, "duplicate profile key: %s", key);
        return 0;
    }
    if (!*value || strlen(value) >= destination_size) {
        set_error(error, error_size, "invalid value for profile key: %s", key);
        return 0;
    }
    strcpy(destination, value);
    field_mark(present, field);
    return 1;
}

static int assign_u32(uint32_t *destination, const char *value, uint64_t *present,
                      enum profile_field field, const char *key, char *error, size_t error_size) {
    if (field_seen(present, field)) {
        set_error(error, error_size, "duplicate profile key: %s", key);
        return 0;
    }
    errno = 0;
    char *end = NULL;
    unsigned long parsed = strtoul(value, &end, 0);
    if (errno || !end || *end || end == value || parsed > UINT32_MAX) {
        set_error(error, error_size, "invalid integer for profile key: %s", key);
        return 0;
    }
    *destination = (uint32_t)parsed;
    field_mark(present, field);
    return 1;
}

static int valid_sha256(const char *value) {
    if (strlen(value) != 64)
        return 0;
    for (const char *p = value; *p; p++)
        if (!isxdigit((unsigned char)*p))
            return 0;
    return 1;
}

static int parse_entry(struct gl_profile *profile, uint64_t *present, const char *key,
                       const char *value, char *error, size_t error_size) {
#define STRING_ENTRY(name, member, field)                                                          \
    if (!strcmp(key, name))                                                                        \
    return assign_string(profile->member, sizeof(profile->member), value, present, field, key,     \
                         error, error_size)
#define U32_ENTRY(name, member, field)                                                             \
    if (!strcmp(key, name))                                                                        \
    return assign_u32(&profile->member, value, present, field, key, error, error_size)

    U32_ENTRY("profile.version", profile_version, PF_VERSION);
    STRING_ENTRY("profile.target", target, PF_TARGET);
    STRING_ENTRY("profile.verification", verification, PF_VERIFICATION);
    STRING_ENTRY("match.ro.product.device", match_product, PF_PRODUCT);
    STRING_ENTRY("match.ro.product.model", match_model, PF_MODEL);
    STRING_ENTRY("match.ro.build.fingerprint", match_fingerprint, PF_FINGERPRINT);
    STRING_ENTRY("match.ro.build.version.incremental", match_incremental, PF_INCREMENTAL);
    STRING_ENTRY("match.uname.release", match_kernel_release, PF_KERNEL_RELEASE);
    STRING_ENTRY("match.uname.machine", match_machine, PF_MACHINE);
    STRING_ENTRY("exploit.path", exploit_path, PF_EXPLOIT_PATH);
    U32_ENTRY("exploit.waiter_core", waiter_core, PF_WAITER_CORE);
    U32_ENTRY("exploit.consumer_core", consumer_core, PF_CONSUMER_CORE);
    U32_ENTRY("exploit.consumer_delay_us", consumer_delay_us, PF_CONSUMER_DELAY);
    U32_ENTRY("address.init_task", init_task, PF_INIT_TASK);
    U32_ENTRY("address.pi_task", pi_task, PF_PI_TASK);
    U32_ENTRY("address.ashmem_fops", ashmem_fops, PF_ASHMEM_FOPS);
    U32_ENTRY("address.ashmem_misc_fops", ashmem_misc_fops, PF_ASHMEM_MISC_FOPS);
    U32_ENTRY("address.ashmem_open", ashmem_open, PF_ASHMEM_OPEN);
    U32_ENTRY("address.ashmem_release", ashmem_release, PF_ASHMEM_RELEASE);
    U32_ENTRY("address.ashmem_ioctl", ashmem_ioctl, PF_ASHMEM_IOCTL);
    U32_ENTRY("address.ashmem_mmap", ashmem_mmap, PF_ASHMEM_MMAP);
    U32_ENTRY("address.configfs_read_file", configfs_read_file, PF_CONFIGFS_READ);
    U32_ENTRY("address.configfs_write_file", configfs_write_file, PF_CONFIGFS_WRITE);
    U32_ENTRY("offset.task_tasks", task_tasks_off, PF_TASK_TASKS);
    U32_ENTRY("offset.task_real_cred", task_real_cred_off, PF_TASK_REAL_CRED);
    U32_ENTRY("offset.task_cred", task_cred_off, PF_TASK_CRED);
    U32_ENTRY("offset.task_comm", task_comm_off, PF_TASK_COMM);
    U32_ENTRY("offset.fops_read", fops_read_off, PF_FOPS_READ);
    U32_ENTRY("offset.fops_ioctl", fops_ioctl_off, PF_FOPS_IOCTL);
    U32_ENTRY("offset.fops_mmap", fops_mmap_off, PF_FOPS_MMAP);
    U32_ENTRY("offset.fops_open", fops_open_off, PF_FOPS_OPEN);
    U32_ENTRY("offset.fops_release", fops_release_off, PF_FOPS_RELEASE);
    U32_ENTRY("offset.configfs_page", configfs_page_off, PF_CONFIGFS_PAGE);
    U32_ENTRY("offset.configfs_mutex", configfs_mutex_off, PF_CONFIGFS_MUTEX);
    U32_ENTRY("offset.configfs_needs_read_fill", configfs_needs_read_fill_off, PF_CONFIGFS_NRF);
    U32_ENTRY("futex.hash_len", futex_hash_len, PF_FUTEX_HASH_LEN);
    U32_ENTRY("futex.hash_seed", futex_hash_seed, PF_FUTEX_HASH_SEED);
    U32_ENTRY("futex.hash_buckets", futex_hash_buckets, PF_FUTEX_HASH_BUCKETS);
    U32_ENTRY("stamp.waiter_depth", stamp_waiter_depth, PF_STAMP_WAITER_DEPTH);
    U32_ENTRY("stamp.copy_depth", stamp_copy_depth, PF_STAMP_COPY_DEPTH);
    U32_ENTRY("stamp.socket_kind", stamp_socket_kind, PF_STAMP_SOCKET_KIND);
    U32_ENTRY("offset.task_prio", task_prio_off, PF_TASK_PRIO);
    U32_ENTRY("offset.task_static_prio", task_static_prio_off, PF_TASK_STATIC_PRIO);
    U32_ENTRY("offset.task_normal_prio", task_normal_prio_off, PF_TASK_NORMAL_PRIO);
    U32_ENTRY("offset.task_rt_priority", task_rt_priority_off, PF_TASK_RT_PRIORITY);
    U32_ENTRY("offset.task_sched_class", task_sched_class_off, PF_TASK_SCHED_CLASS);
    U32_ENTRY("offset.task_pi_lock", task_pi_lock_off, PF_TASK_PI_LOCK);
    U32_ENTRY("offset.task_pi_waiters", task_pi_waiters_off, PF_TASK_PI_WAITERS);
    U32_ENTRY("offset.task_pi_top_task", task_pi_top_task_off, PF_TASK_PI_TOP_TASK);
    U32_ENTRY("offset.task_pi_blocked_on", task_pi_blocked_on_off, PF_TASK_PI_BLOCKED_ON);
    U32_ENTRY("address.selinux_status_page", selinux_status_page, PF_SELINUX_STATUS_PAGE);
    U32_ENTRY("address.mem_map", mem_map, PF_MEM_MAP);
    U32_ENTRY("address.pfn_offset", pfn_offset, PF_PFN_OFFSET);
    U32_ENTRY("layout.page_struct_size", page_struct_size, PF_PAGE_STRUCT_SIZE);
    U32_ENTRY("layout.lowmem_va_sub", lowmem_va_sub, PF_LOWMEM_VA_SUB);
    U32_ENTRY("address.selinux_enforcing", selinux_enforcing, PF_SELINUX_ENFORCING);
    U32_ENTRY("offset.cred_cap_permitted", cred_cap_permitted_off, PF_CRED_CAP_PERMITTED);
    U32_ENTRY("offset.cred_cap_effective", cred_cap_effective_off, PF_CRED_CAP_EFFECTIVE);
    U32_ENTRY("offset.cred_cap_bset", cred_cap_bset_off, PF_CRED_CAP_BSET);
    STRING_ENTRY("analysis.ota_sha256", ota_sha256, PF_OTA_SHA256);
    STRING_ENTRY("analysis.boot_sha256", boot_sha256, PF_BOOT_SHA256);
    STRING_ENTRY("analysis.kernel_sha256", kernel_sha256, PF_KERNEL_SHA256);
    STRING_ENTRY("analysis.kernel_config_sha256", kernel_config_sha256, PF_CONFIG_SHA256);
    STRING_ENTRY("analysis.second_sha256", second_sha256, PF_SECOND_SHA256);
    STRING_ENTRY("analysis.critical_anchors_sha256", critical_anchors_sha256, PF_ANCHORS_SHA256);
    STRING_ENTRY("analysis.reference", reference, PF_REFERENCE);
    U32_ENTRY("analysis.kernel_image_base", kernel_image_base, PF_KERNEL_BASE);
    U32_ENTRY("analysis.kernel_size", kernel_size, PF_KERNEL_SIZE);
    U32_ENTRY("analysis.kernel_similarity_ppm", kernel_similarity_ppm, PF_SIMILARITY_PPM);

#undef STRING_ENTRY
#undef U32_ENTRY
    set_error(error, error_size, "unknown profile key: %s", key);
    return 0;
}

static int parse_profile_stream(FILE *file, const char *label, struct gl_profile *profile,
                                char *error, size_t error_size) {
    memset(profile, 0, sizeof(*profile));
    uint64_t present[PF_WORDS] = {0};
    char line[1024];
    unsigned line_number = 0;
    while (fgets(line, sizeof(line), file)) {
        line_number++;
        if (!strchr(line, '\n') && !feof(file)) {
            set_error(error, error_size, "%s:%u: line is too long", label, line_number);
            return 0;
        }
        char *entry = trim(line);
        if (!*entry || *entry == '#')
            continue;
        char *separator = strchr(entry, '=');
        if (!separator) {
            set_error(error, error_size, "%s:%u: expected key=value", label, line_number);
            return 0;
        }
        *separator = 0;
        char *key = trim(entry);
        char *value = trim(separator + 1);
        char detail[256] = {0};
        if (!*key || !*value || !parse_entry(profile, present, key, value, detail, sizeof(detail))) {
            set_error(error, error_size, "%s:%u: %s", label, line_number,
                      detail[0] ? detail : "invalid entry");
            return 0;
        }
    }
    if (ferror(file)) {
        set_error(error, error_size, "cannot read %s: %s", label, strerror(errno));
        return 0;
    }

    for (unsigned field = 0; field < PF_COUNT; field++)
        if (!field_seen(present, (enum profile_field)field)) {
            set_error(error, error_size, "%s: missing required profile field %u", label, field);
            return 0;
        }
    if (profile->profile_version != GL_PROFILE_SCHEMA) {
        set_error(error, error_size, "%s: unsupported profile version %u", label,
                  profile->profile_version);
        return 0;
    }
    if (!strcmp(profile->match_kernel_release, "*") || !strcmp(profile->match_machine, "*")) {
        set_error(error, error_size, "%s: match.uname.release and match.uname.machine are required",
                  label);
        return 0;
    }
    const char *hashes[] = {profile->ota_sha256,
                            profile->boot_sha256,
                            profile->kernel_sha256,
                            profile->kernel_config_sha256,
                            profile->second_sha256,
                            profile->critical_anchors_sha256};
    for (unsigned i = 0; i < sizeof(hashes) / sizeof(hashes[0]); i++)
        if (!valid_sha256(hashes[i])) {
            set_error(error, error_size, "%s: malformed SHA-256 field", label);
            return 0;
        }
    return 1;
}

static int parse_profile(const char *path, struct gl_profile *profile, char *error,
                         size_t error_size) {
    FILE *file = fopen(path, "re");
    if (!file) {
        set_error(error, error_size, "cannot open %s: %s", path, strerror(errno));
        return 0;
    }
    int ok = parse_profile_stream(file, path, profile, error, error_size);
    fclose(file);
    return ok;
}

static int read_property(const char *name, char *destination, size_t destination_size) {
    char value[PROP_VALUE_MAX] = {0};
    int length = __system_property_get(name, value);
    if (length <= 0 || (size_t)length >= destination_size)
        return 0;
    memcpy(destination, value, (size_t)length + 1);
    return 1;
}

int gl_runtime_read(struct gl_runtime *runtime, char *error, size_t error_size) {
    memset(runtime, 0, sizeof(*runtime));
    if (!read_property("ro.product.device", runtime->product, sizeof(runtime->product)) ||
        !read_property("ro.product.model", runtime->model, sizeof(runtime->model)) ||
        !read_property("ro.build.fingerprint", runtime->fingerprint,
                       sizeof(runtime->fingerprint)) ||
        !read_property("ro.build.version.incremental", runtime->incremental,
                       sizeof(runtime->incremental))) {
        set_error(error, error_size, "required Android build properties are unavailable");
        return 0;
    }
    struct utsname uts;
    if (uname(&uts)) {
        set_error(error, error_size, "uname failed: %s", strerror(errno));
        return 0;
    }
    snprintf(runtime->kernel_release, sizeof(runtime->kernel_release), "%s", uts.release);
    snprintf(runtime->machine, sizeof(runtime->machine), "%s", uts.machine);
    return 1;
}

static int value_matches(const char *expected, const char *actual) {
    if (!strcmp(expected, "*"))
        return 1;
    const char *p = expected;
    while (*p) {
        const char *sep = p;
        while (*sep && *sep != ',' && *sep != '|')
            sep++;
        size_t len = (size_t)(sep - p);
        if (len == strlen(actual) && !strncmp(p, actual, len))
            return 1;
        p = *sep ? sep + 1 : sep;
    }
    return 0;
}

static int profile_matches(const struct gl_profile *profile, const struct gl_runtime *runtime) {
    return value_matches(profile->match_product, runtime->product) &&
           value_matches(profile->match_model, runtime->model) &&
           value_matches(profile->match_fingerprint, runtime->fingerprint) &&
           value_matches(profile->match_incremental, runtime->incremental) &&
           value_matches(profile->match_kernel_release, runtime->kernel_release) &&
           value_matches(profile->match_machine, runtime->machine);
}

static int has_conf_suffix(const char *name) {
    size_t length = strlen(name);
    return length > 5 && !strcmp(name + length - 5, ".conf");
}

int gl_profile_select(const char *directory, const char *explicit_path,
                      const struct gl_runtime *runtime, struct gl_profile *profile,
                      char *selected_path, size_t selected_path_size, char *error,
                      size_t error_size) {
    if (explicit_path) {
        if (!parse_profile(explicit_path, profile, error, error_size))
            return 0;
        if (!profile_matches(profile, runtime)) {
            set_error(error, error_size, "profile %s does not match this device", explicit_path);
            return 0;
        }
        snprintf(selected_path, selected_path_size, "%s", explicit_path);
        return 1;
    }

    DIR *dir = opendir(directory);
    if (!dir) {
        set_error(error, error_size, "cannot open profile directory %s: %s", directory,
                  strerror(errno));
        return 0;
    }
    unsigned matches = 0;
    struct dirent *entry;
    while ((entry = readdir(dir))) {
        if (entry->d_name[0] == '.' || !has_conf_suffix(entry->d_name))
            continue;
        char path[1024];
        int length = snprintf(path, sizeof(path), "%s/%s", directory, entry->d_name);
        if (length < 0 || (size_t)length >= sizeof(path)) {
            set_error(error, error_size, "profile path is too long");
            closedir(dir);
            return 0;
        }
        struct gl_profile candidate;
        if (!parse_profile(path, &candidate, error, error_size)) {
            closedir(dir);
            return 0;
        }
        if (!profile_matches(&candidate, runtime))
            continue;
        matches++;
        *profile = candidate;
        snprintf(selected_path, selected_path_size, "%s", path);
    }
    closedir(dir);
    if (matches == 0) {
        set_error(error, error_size, "no profile in %s matches this device", directory);
        return 0;
    }
    if (matches != 1) {
        set_error(error, error_size, "%u profiles in %s match this device; refusing ambiguity",
                  matches, directory);
        return 0;
    }
    return 1;
}

int gl_profile_select_embedded(const struct gl_runtime *runtime, struct gl_profile *profile,
                               char *error, size_t error_size) {
    unsigned matches = 0;
    for (unsigned i = 0; i < GL_EMBEDDED_PROFILE_COUNT; i++) {
        if (!profile_matches(&gl_embedded_profiles[i], runtime))
            continue;
        matches++;
        *profile = gl_embedded_profiles[i];
    }
    if (matches == 0) {
        set_error(error, error_size, "no built-in profile matches this device");
        return 0;
    }
    if (matches != 1) {
        set_error(error, error_size, "%u built-in profiles match this device; refusing ambiguity",
                  matches);
        return 0;
    }
    return 1;
}

void gl_profile_print(const struct gl_profile *profile, const struct gl_runtime *runtime,
                      const char *selected_path) {
    printf("profile=%s\n", selected_path);
    printf("target=%s\n", profile->target);
    printf("verification=%s\n", profile->verification);
    printf("exploit_path=%s\n", profile->exploit_path);
    printf("product=%s\n", runtime->product);
    printf("model=%s\n", runtime->model);
    printf("fingerprint=%s\n", runtime->fingerprint);
    printf("incremental=%s\n", runtime->incremental);
    printf("kernel=%s %s\n", runtime->kernel_release, runtime->machine);
    fflush(stdout);
}
