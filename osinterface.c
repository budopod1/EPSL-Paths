#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>

#ifdef _WIN32

#else
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <pwd.h>
#endif

#include "builtins.h"

#define EPSL_COMMON_PREFIX "epsl_paths_"

typedef struct {
    uint64_t ref_counter;
    uint64_t capacity;
    uint64_t length;
    unsigned char *content;
} ARRAY_Byte, NULLABLE_ARRAY_Byte;

typedef struct {
    uint64_t ref_counter;
    uint64_t capacity;
    uint64_t length;
    ARRAY_Byte **content;
} ARRAY_ARRAY_Byte, NULLABLE_ARRAY_ARRAY_Byte;

#ifdef _WIN32
static unsigned char path_separator_content[] = "\\";
#else
static unsigned char path_separator_content[] = "/";
#endif
ARRAY_Byte path_separator_str = {1, 0, 1, path_separator_content};

#define EPSL_STR_TO_C_STR(epsl_str, new_name)\
    char *new_name;\
    bool new_name##_is_new_str = epsl_str->capacity <= epsl_str->length;\
    if (new_name##_is_new_str) {\
        new_name = (char*)epsl_str->content;\
    } else {\
        new_name = malloc(epsl_str->length+1);\
        memcpy(new_name, epsl_str->content, epsl_str->length);\
    }\
    new_name[epsl_str->length] = '\0';

#define CLEANUP_C_STR(str_name)\
    if (str_name##_is_new_str) free(str_name);

#ifdef _WIN32
#warning "EPSL-Paths does not yet fully support windows"

static void windows_abort() {
    epsl_panicf("EPSL-Paths does not yet support this operation on windows");
}
#endif

char *_strdup(const char *src) {
    size_t len = strlen(src);
    char *copy = malloc(len+1);
    memcpy(copy, src, len+1);
    return copy;
}

static ARRAY_Byte *C_str_with_cap_to_epsl_str(uint64_t ref_counter, char *src, uint64_t cap) {
    ARRAY_Byte *result = malloc(sizeof(*result));
    result->ref_counter = ref_counter;
    result->capacity = cap;
    result->length = strlen(src);
    result->content = (unsigned char*)src;
    return result;
}

static ARRAY_Byte *C_str_to_epsl_str(uint64_t ref_counter, char *src) {
    ARRAY_Byte *result = malloc(sizeof(*result));
    result->ref_counter = ref_counter;
    uint64_t length = strlen(src);
    result->capacity = length + 1;
    result->length = length;
    result->content = (unsigned char*)src;
    return result;
}

ARRAY_Byte *epsl_paths_path_sep_str() {
    return &path_separator_str;
}

unsigned char epsl_paths_path_sep_chr() {
    return *path_separator_content;
}

bool epsl_paths_paths_have_drive() {
#ifdef _WIN32
    return true;
#else
    return false;
#endif
}

ARRAY_Byte *epsl_paths_get_home_path() {
#ifdef _WIN32
    windows_abort();
#else
    char *home_dir = getenv("HOME");
    if (home_dir == NULL) {
        home_dir = getpwuid(getuid())->pw_dir;
    }
    return C_str_to_epsl_str(0, _strdup(home_dir));
#endif
}

NULLABLE_ARRAY_Byte *epsl_paths_get_cwd() {
#ifdef _WIN32
    windows_abort();
#else
#ifdef _GNU_SOURCE
    char *cwd = getcwd(NULL, 0);
    return cwd ? C_str_to_epsl_str(0, cwd) : NULL;
#else
    
#endif
    size_t path_cap = 256;
    char *path = malloc(path_cap);
    while (getcwd(path, path_cap) == NULL) {
        if (errno != ERANGE) return NULL;
        path_cap *= 2;
        path = realloc(path, path_cap);
    }
    return C_str_with_cap_to_epsl_str(0, path, (uint64_t)path_cap);
#endif
}

NULLABLE_ARRAY_Byte *epsl_paths_resolve_real_path(ARRAY_Byte *path) {
#ifdef _WIN32
    windows_abort();
#else
    EPSL_STR_TO_C_STR(path, c_path);
    char *resolved = realpath(c_path, NULL);
    CLEANUP_C_STR(c_path);
    return resolved ? C_str_to_epsl_str(0, resolved) : NULL;
#endif
}

NULLABLE_ARRAY_Byte *epsl_paths_read_symlink(ARRAY_Byte *path) {
#ifdef _WIN32
    windows_abort();
#else
    ARRAY_Byte *result = NULL;
    EPSL_STR_TO_C_STR(path, c_path);
    
    struct stat sb;
    if (lstat(c_path, &sb) == -1) goto cleanup;
    size_t max_link_len = sb.st_size + 1;
    
    char *link_content = malloc(max_link_len);
    if (readlink(c_path, link_content, max_link_len) == -1) goto cleanup;

    result = C_str_to_epsl_str(0, link_content);
    
cleanup:
    CLEANUP_C_STR(c_path);
    return result;
#endif
}

bool epsl_paths_check_path_exists(ARRAY_Byte *path) {
#ifdef _WIN32
    windows_abort();
#else
    struct stat sb;
    EPSL_STR_TO_C_STR(path, c_path);
    int status = stat(c_path, &sb);
    CLEANUP_C_STR(c_path);
    return status != -1;
#endif
}

bool epsl_paths_check_path_is_file(ARRAY_Byte *path) {
#ifdef _WIN32
    windows_abort();
#else
    struct stat sb;
    EPSL_STR_TO_C_STR(path, c_path);
    int status = stat(c_path, &sb);
    CLEANUP_C_STR(c_path);
    if (status == -1) return false;
    return S_ISREG(sb.st_mode);
#endif
}

bool epsl_paths_check_path_is_dir(ARRAY_Byte *path) {
#ifdef _WIN32
    windows_abort();
#else
    struct stat sb;
    EPSL_STR_TO_C_STR(path, c_path);
    int status = stat(c_path, &sb);
    CLEANUP_C_STR(c_path);
    if (status == -1) return false;
    return S_ISDIR(sb.st_mode);
#endif
}

bool epsl_paths_check_path_is_symlink(ARRAY_Byte *path) {
#ifdef _WIN32
    windows_abort();
#else
    struct stat sb;
    EPSL_STR_TO_C_STR(path, c_path);
    int status = lstat(c_path, &sb);
    CLEANUP_C_STR(c_path);
    if (status == -1) return false;
    return S_ISLNK(sb.st_mode);
#endif
}

NULLABLE_ARRAY_ARRAY_Byte *epsl_paths_read_directory_contents(ARRAY_Byte *path) {
#ifdef _WIN32
    windows_abort();
#else
    EPSL_STR_TO_C_STR(path, c_path);
    DIR *dir = opendir(c_path);
    CLEANUP_C_STR(c_path);
    if (dir == NULL) return NULL;
    
    ARRAY_ARRAY_Byte *result = (ARRAY_ARRAY_Byte*)epsl_blank_array(sizeof(ARRAY_Byte*));
    
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        ARRAY_Byte *entry_name = C_str_to_epsl_str(1, _strdup(entry->d_name));
        epsl_increment_length((struct Array*)result, sizeof(ARRAY_Byte*));
        result->content[result->length - 1] = entry_name;
    }
    
    closedir(dir);
    return result;
#endif
}

bool epsl_paths_make_file(ARRAY_Byte *path) {
#ifdef _WIN32
    windows_abort();
#else
    EPSL_STR_TO_C_STR(path, c_path);
    int status = creat(c_path, 0666); // note: octal
    CLEANUP_C_STR(c_path);
    return status != -1;
#endif
}

bool epsl_paths_make_directory(ARRAY_Byte *path) {
#ifdef _WIN32
    windows_abort();
#else
    EPSL_STR_TO_C_STR(path, c_path);
    int status = mkdir(c_path, 0777); // note: octal
    CLEANUP_C_STR(c_path);
    return status != -1;
#endif
}

bool epsl_paths_make_symlink(ARRAY_Byte *from, ARRAY_Byte *to) {
#ifdef _WIN32
    windows_abort();
#else
    EPSL_STR_TO_C_STR(from, c_from);
    EPSL_STR_TO_C_STR(to, c_to);
    int status = symlink(c_to, c_from);
    CLEANUP_C_STR(c_from);
    CLEANUP_C_STR(c_to);
    return status != -1;
#endif
}

bool epsl_paths_make_hardlink(ARRAY_Byte *from, ARRAY_Byte *to) {
#ifdef _WIN32
    windows_abort();
#else
    EPSL_STR_TO_C_STR(from, c_from);
    EPSL_STR_TO_C_STR(to, c_to);
    int status = link(c_to, c_from);
    CLEANUP_C_STR(c_from);
    CLEANUP_C_STR(c_to);
    return status != -1;
#endif
}

bool epsl_paths_rename_file(ARRAY_Byte *from, ARRAY_Byte *to) {
#ifdef _WIN32
    windows_abort();
#else
    EPSL_STR_TO_C_STR(from, c_from);
    EPSL_STR_TO_C_STR(to, c_to);
    int status = rename(c_to, c_from);
    CLEANUP_C_STR(c_from);
    CLEANUP_C_STR(c_to);
    return status != -1;
#endif
}

bool epsl_paths_unlink_file(ARRAY_Byte *path) {
#ifdef _WIN32
    windows_abort();
#else
    EPSL_STR_TO_C_STR(path, c_path);
    int status = unlink(c_path);
    CLEANUP_C_STR(c_path);
    return status != -1;
#endif
}

bool epsl_paths_rmdir(ARRAY_Byte *path) {
#ifdef _WIN32
    windows_abort();
#else
    EPSL_STR_TO_C_STR(path, c_path);
    int status = rmdir(c_path);
    CLEANUP_C_STR(c_path);
    return status != -1;
#endif
}

bool epsl_paths_chdir(ARRAY_Byte *path) {
#ifdef _WIN32
    windows_abort();
#else
    EPSL_STR_TO_C_STR(path, c_path);
    int status = chdir(c_path);
    CLEANUP_C_STR(c_path);
    return status != -1;
#endif
}
