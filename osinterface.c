#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <stdio.h>

#include "epsilon.h"

#ifdef _WIN32
#define UNICODE
#include <limits.h>
#include <windows.h>
#include <Lmcons.h>
#include <wchar.h>
#else
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <pwd.h>
#endif

#define EPSL_COMMON_PREFIX "epsl_paths_"

typedef struct ARRAY_Byte {
    uint64_t ref_counter;
    uint64_t capacity;
    uint64_t length;
    unsigned char *content;
} ARRAY_Byte, NULLABLE_ARRAY_Byte;

typedef struct ARRAY_ARRAY_Byte {
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
#define PATH_SEP_CHAR (*path_separator_content)
ARRAY_Byte path_separator_str = {1, 0, 1, path_separator_content};

#ifdef _WIN32
#warning "EPSL-Paths does not yet fully support windows"
#endif

#define Cstr_to_Estr(a, b) ((ARRAY_Byte*)epsl_Cstr_to_Estr(a, b))
#define dup_Cstr_to_Estr(a, b) ((ARRAY_Byte*)epsl_dup_Cstr_to_Estr(a, b))

ARRAY_Byte *epsl_paths_path_sep_str(void) {
    return &path_separator_str;
}

unsigned char epsl_paths_path_sep_chr(void) {
    return *path_separator_content;
}

bool epsl_paths_paths_have_drive(void) {
#ifdef _WIN32
    return true;
#else
    return false;
#endif
}

NULLABLE_ARRAY_Byte *epsl_paths_get_home_path(void) {
#ifdef _WIN32
    char *user_profile = getenv("USERPROFILE");
    if (user_profile != NULL) {
        return dup_Cstr_to_Estr(0, user_profile);
    }

    char *home_drive = getenv("HOMEDRIVE");
    bool found_home_drive = home_drive != NULL;
    if (found_home_drive) {
        // we need to strdup it because getenv can overwrite
        // the buffer returned by previous calls
        home_drive = _strdup(home_drive);
    } else {
        home_drive = "";
    }
    uint64_t home_drive_len = strlen(home_drive);

    char *home_path = getenv("HOMEPATH");
    if (home_path == NULL) return NULL;
    uint64_t home_path_len = strlen(home_path);

    uint64_t path_sep_len = strlen(strlen(path_separator_content));
    uint64_t total_len = home_drive_len + path_sep_len + home_path_len;
    char *content = epsl_malloc(total_len);
    memcpy(content, home_drive, home_drive_len);
    memcpy(content + home_drive_len, path_separator_content, path_sep_len);
    memcpy(content + home_drive_len + path_sep_len, home_path, home_path_len);

    if (found_home_drive) free(home_drive);
    
    ARRAY_Byte *result = epsl_malloc(sizeof(*result));
    result->ref_counter = 0;
    result->capacity = total_len;
    result->length = total_len;
    result->content = (unsigned char*)content;
    return result;
#else
    char *home_dir = getenv("HOME");
    if (home_dir == NULL) {
        struct passwd *pwd = getpwuid(getuid());
        if (pwd == NULL) return NULL;
        home_dir = pwd->pw_dir;
    }
    return dup_Cstr_to_Estr(0, home_dir);
#endif
}

NULLABLE_ARRAY_Byte *epsl_paths_get_cwd(void) {
#ifdef _WIN32
    // UNICODE was #defined, so TCHARs are wchar_ts
    DWORD wchar_cap = GetCurrentDirectory(0, NULL);
    if (wchar_cap == 0) return NULL;
    wchar_t *wchar_buf = epsl_malloc(wchar_cap);
    DWORD status = GetCurrentDirectory(wchar_cap, wchar_buf);
    if (status == 0) {
        free(wchar_buf);
        return NULL;
    }
    struct Array *result = epsl_Wstr_to_Estr(0, wchar_buf);
    free(wchar_buf);
    return result;
#else
#ifdef _GNU_SOURCE
    char *cwd = getcwd(NULL, 0);
    return cwd ? Cstr_to_Estr(0, cwd) : NULL;
#else
    size_t path_cap = 256;
    char *path = epsl_malloc(path_cap);
    while (getcwd(path, path_cap) == NULL) {
        if (errno != ERANGE) {
            free(path);
            return NULL;
        }
        path_cap *= 2;
        path = epsl_realloc(path, path_cap);
    }
    ARRAY_Byte *result = Cstr_to_Estr(0, path);
    result->capacity = (uint64_t)path_cap;
    return result;
#endif
#endif
}

NULLABLE_ARRAY_Byte *epsl_paths_resolve_real_path(ARRAY_Byte *path) {
    if (path->length == 0) {
        return epsl_paths_get_cwd();
    }
#ifdef _WIN32
    wchar_t *rel_wchar_str = epsl_Estr_to_Wstr(path);
    if (rel_wchar_str == NULL) return NULL;
    wchar_t *abs_wchar_str = _wfullpath(NULL, rel_wchar_str, SIZE_MAX);
    free(rel_wchar_str);
    struct Array *result = abs_wchar_str == NULL ? NULL
        : epsl_Wstr_to_Estr(0, abs_wchar_str);
    free(abs_wchar_str);
    return result;
#else
    EPSL_STR_TO_C_STR(path, c_path);
    char *resolved = realpath(c_path, NULL);
    CLEANUP_CONV_C_STR(c_path);
    return resolved ? Cstr_to_Estr(0, resolved) : NULL;
#endif
}

NULLABLE_ARRAY_Byte *epsl_paths_os_user_name(void) {
#ifdef _WIN32
    wchar_t wstr_name[UNLEN + 1];
    if (!GetUserNameW(wstr_name, UNLEN + 1)) {
        return NULL;
    }
    return epsl_Wstr_to_Estr(0, wstr_name);
#else
    struct passwd *pwd = getpwuid(geteuid());
    if (pwd == NULL) return NULL;
    return dup_Cstr_to_Estr(0, pwd->pw_name);
#endif
}

NULLABLE_ARRAY_Byte *epsl_paths_read_symlink(ARRAY_Byte *path) {
#ifdef _WIN32
    epsl_panicf("Readlink operation not supported on Windows :/");
#else
    ARRAY_Byte *result = NULL;
    EPSL_STR_TO_C_STR(path, c_path);
    
    struct stat sb;
    if (lstat(c_path, &sb) == -1) goto cleanup;
    size_t link_cap = sb.st_size + 1;
    if (link_cap <= 2) link_cap = 256;
    
    char *link_content = NULL;
    while (1) {
        link_content = epsl_realloc(link_content, link_cap);
        ssize_t status = readlink(c_path, link_content, link_cap);
        if (status == -1) {
            free(link_content);
            goto cleanup;
        } else if (status == link_cap) {
            link_cap *= 2;
        } else {
            result = epsl_malloc(sizeof(*result));
            result->ref_counter = 0;
            result->capacity = (uint64_t)link_cap;
            result->length = (uint64_t)status;
            result->content = (unsigned char*)link_content;
            break;
        }
    }
    
cleanup:
    CLEANUP_CONV_C_STR(c_path);
    return result;
#endif
}

bool epsl_paths_check_path_exists(ARRAY_Byte *path) {
#ifdef _WIN32
    wchar_t *wstr_path = epsl_Estr_to_Wstr(path);
    if (wstr_path == NULL) return false;
    DWORD attrs = GetFileAttributesW(wstr_path);
    free(wstr_path);
    return attrs != INVALID_FILE_ATTRIBUTES;
#else
    struct stat sb;
    EPSL_STR_TO_C_STR(path, c_path);
    int status = stat(c_path, &sb);
    CLEANUP_CONV_C_STR(c_path);
    return status != -1;
#endif
}

bool epsl_paths_check_path_is_file(ARRAY_Byte *path) {
#ifdef _WIN32
    wchar_t *wstr_path = epsl_Estr_to_Wstr(path);
    if (wstr_path == NULL) return false;
    DWORD attrs = GetFileAttributesW(wstr_path);
    free(wstr_path);
    return attrs != INVALID_FILE_ATTRIBUTES
        && !(attrs & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)); 
#else
    struct stat sb;
    EPSL_STR_TO_C_STR(path, c_path);
    int status = stat(c_path, &sb);
    CLEANUP_CONV_C_STR(c_path);
    if (status == -1) return false;
    return S_ISREG(sb.st_mode);
#endif
}

bool epsl_paths_check_path_is_dir(ARRAY_Byte *path) {
#ifdef _WIN32
    wchar_t *wstr_path = epsl_Estr_to_Wstr(path);
    if (wstr_path == NULL) return false;
    DWORD attrs = GetFileAttributesW(wstr_path);
    free(wstr_path);
    return attrs != INVALID_FILE_ATTRIBUTES
        && (attrs & FILE_ATTRIBUTE_DIRECTORY);
#else
    struct stat sb;
    EPSL_STR_TO_C_STR(path, c_path);
    int status = stat(c_path, &sb);
    CLEANUP_CONV_C_STR(c_path);
    if (status == -1) return false;
    return S_ISDIR(sb.st_mode);
#endif
}

bool epsl_paths_check_path_is_symlink(ARRAY_Byte *path) {
#ifdef _WIN32
    wchar_t *wstr_path = epsl_Estr_to_Wstr(path);
    if (wstr_path == NULL) return false;
    DWORD attrs = GetFileAttributesW(wstr_path);
    free(wstr_path);
    return attrs != INVALID_FILE_ATTRIBUTES
        && (attrs & FILE_ATTRIBUTE_REPARSE_POINT);
#else
    struct stat sb;
    EPSL_STR_TO_C_STR(path, c_path);
    int status = lstat(c_path, &sb);
    CLEANUP_CONV_C_STR(c_path);
    if (status == -1) return false;
    return S_ISLNK(sb.st_mode);
#endif
}

NULLABLE_ARRAY_ARRAY_Byte *epsl_paths_read_directory_contents(ARRAY_Byte *path) {
#ifdef _WIN32
    // https://github.com/python/cpython/blob/9609574e7fd36edfaa8b575558a82cc14e65bfbc/Modules/posixmodule.c#L4887

    if (path->length == 0) return NULL;

    for (uint64_t i = 0; i < path->length; i++) {
        unsigned char c = path->content[i];
        if (c == '*' || c == '?') return NULL;
    }

    bool trailing_sep = path->content[path->length - 1] == PATH_SEP_CHAR;
    
    wchar_t *base_wstr_path = epsl_Estr_to_Wstr(path);
    if (base_wstr_path == NULL) return NULL;

    size_t base_wstr_len = wcslen(base_wstr_path);
    size_t pattern_len = base_wstr_len
        + /*'\\' if applicable*/ !trailing_sep
        + /*'*'*/ 1    
        + /*'\0'*/ 1;
    
    wchar_t *wstr_pattern = epsl_malloc(sizeof(wchar_t) * pattern_len);
    memcpy(wstr_pattern, base_wstr_path, sizeof(wchar_t) * base_wstr_len);
    free(base_wstr_path);
    wchar_t *suffix_ptr = wstr_pattern + base_wstr_len;
    if (!trailing_sep) *(suffix_ptr++) = PATH_SEP_CHAR;
    *(suffix_ptr++) = '*';
    *(suffix_ptr++) = '\0';

    WIN32_FIND_DATAW find_data;
    HANDLE find_handle = FindFirstFileW(wstr_pattern, &find_data);
    free(wstr_pattern);

    if (find_handle == INVALID_HANDLE_VALUE) {
        return NULL;
    }
    
    ARRAY_ARRAY_Byte *result = (ARRAY_ARRAY_Byte*)epsl_blank_array(sizeof(ARRAY_Byte*));

    do {
        wchar_t *wstr_filename = file_data.cFileName;

        if (wcscmp(wstr_filename, L".") == 0
            || wcscmp(wstr_filename, L"..") == 0) {
            continue;
        }

        struct Array *filename = epsl_Wstr_to_Estr(0, wstr_filename);

        epsl_increment_length((struct Array*)result, sizeof(ARRAY_Byte*));
        result->content[result->length - 1] = (ARRAY_Byte*)filename;
    } while (FindNextFileW(find_handle, &find_data));

    DWORD error = GetLastError();

    FindClose(find_handle);
    
    if (error == ERROR_NO_MORE_FILES) {
        return result;
    } else {
        for (uint64_t i = 0; i < result->length; i++) {
            ARRAY_Byte *str = result->content[i];
            free(str->content);
            free(str);
        }
        free(result->content);
        free(result);
        return NULL;
    }
#else
    EPSL_STR_TO_C_STR(path, c_path);
    DIR *dir = opendir(c_path);
    CLEANUP_CONV_C_STR(c_path);
    if (dir == NULL) return NULL;
    
    ARRAY_ARRAY_Byte *result = (ARRAY_ARRAY_Byte*)epsl_blank_array(sizeof(ARRAY_Byte*));
    
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        ARRAY_Byte *entry_name = dup_Cstr_to_Estr(1, entry->d_name);
        epsl_increment_length((struct Array*)result, sizeof(ARRAY_Byte*));
        result->content[result->length - 1] = entry_name;
    }
    
    closedir(dir);
    return result;
#endif
}

bool epsl_paths_make_file(ARRAY_Byte *path) {
#ifdef _WIN32
    wchar_t *wstr_path = epsl_Estr_to_Wstr(path);
    HANDLE handle = CreateFileW(
        wstr_path, // filename
        0, // desired access
        0, // sharing mode
        NULL, // security attributes
        CREATE_ALWAYS, // creation disposition
        FILE_ATTRIBUTE_NORMAL, // file flags and attributes
        NULL // template file
    );
    free(wstr_path);
    if (handle == INVALID_HANDLE_VALUE) {
        return false;
    } else {
        CloseHandle(handle);
        return true;
    }
#else
    EPSL_STR_TO_C_STR(path, c_path);
    int status = creat(c_path, 0666); // note: octal
    CLEANUP_CONV_C_STR(c_path);
    return status != -1;
#endif
}

bool epsl_paths_make_directory(ARRAY_Byte *path) {
#ifdef _WIN32
    wchar_t *wstr_path = epsl_Estr_to_Wstr(path);
    bool success = CreateDirectoryW(wstr_path, NULL) != 0;
    free(wstr_path);
    return success;
#else
    EPSL_STR_TO_C_STR(path, c_path);
    int status = mkdir(c_path, 0777); // note: octal
    CLEANUP_CONV_C_STR(c_path);
    return status != -1;
#endif
}

bool epsl_paths_make_symlink(ARRAY_Byte *from, ARRAY_Byte *to) {
#ifdef _WIN32
    wchar_t *wstr_to = epsl_Estr_to_Wstr(to);
    if (wstr_to == NULL) {
        return false;
    }
    DWORD target_attrs = GetFileAttributesW(wstr_to);
    if (target_attrs == INVALID_FILE_ATTRIBUTES) {
        free(wstr_to);
        return false;
    }
    wchar_t *wstr_from = epsl_Estr_to_Wstr(from);
    if (wstr_from == NULL) {
        free(wstr_to);
        return false;
    }
    DWORD flags = SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE;
    if (target_attrs & FILE_ATTRIBUTE_DIRECTORY) {
        flags |= SYMBOLIC_LINK_FLAG_DIRECTORY;
    }
    DWORD status = CreateSymbolicLinkW(wstr_from, wstr_to, flags);
    free(wstr_to);
    free(wstr_from);
    return status != 0;
#else
    EPSL_STR_TO_C_STR(from, c_from);
    EPSL_STR_TO_C_STR(to, c_to);
    int status = symlink(c_to, c_from);
    CLEANUP_CONV_C_STR(c_from);
    CLEANUP_CONV_C_STR(c_to);
    return status != -1;
#endif
}

bool epsl_paths_make_hardlink(ARRAY_Byte *from, ARRAY_Byte *to) {
#ifdef _WIN32
    wchar_t *wstr_from = epsl_Estr_to_Wstr(from);
    if (wstr_from == NULL) return false;
    wchar_t *wstr_to = epsl_Estr_to_Wstr(to);
    if (wstr_to == NULL) {
        free(wstr_from);
        return false;
    }
    BOOL status = CreateHardLinkW(wstr_to, wstr_from, NULL);
    free(wstr_from);
    free(wstr_to);
    return status != 0;
#else
    EPSL_STR_TO_C_STR(from, c_from);
    EPSL_STR_TO_C_STR(to, c_to);
    int status = link(c_to, c_from);
    CLEANUP_CONV_C_STR(c_from);
    CLEANUP_CONV_C_STR(c_to);
    return status != -1;
#endif
}

bool epsl_paths_rename_file(ARRAY_Byte *from, ARRAY_Byte *to) {
#ifdef _WIN32
    wchar_t *wstr_old_name = epsl_Estr_to_Wstr(from);
    wchar_t *wstr_new_name = epsl_Estr_to_Wstr(to);
    int status = _wrename(wstr_old_name, wstr_new_name);
    free(wstr_old_name);
    free(wstr_new_name);
    return status == 0;
#else
    EPSL_STR_TO_C_STR(from, c_from);
    EPSL_STR_TO_C_STR(to, c_to);
    int status = rename(c_to, c_from);
    CLEANUP_CONV_C_STR(c_from);
    CLEANUP_CONV_C_STR(c_to);
    return status != -1;
#endif
}

bool epsl_paths_unlink_file(ARRAY_Byte *path) {
#ifdef _WIN32
    wchar_t *wstr_path = epsl_Estr_to_Wstr(path);
    int status = _wunlink(wstr_path);
    free(wstr_path);
    return status != -1;
#else
    EPSL_STR_TO_C_STR(path, c_path);
    int status = unlink(c_path);
    CLEANUP_CONV_C_STR(c_path);
    return status != -1;
#endif
}

bool epsl_paths_rmdir(ARRAY_Byte *path) {
#ifdef _WIN32
    wchar_t *wstr_path = epsl_Estr_to_Wstr(path);
    int status = _wrmdir(wstr_path);
    free(wstr_path);
    return status != -1;
#else
    EPSL_STR_TO_C_STR(path, c_path);
    int status = rmdir(c_path);
    CLEANUP_CONV_C_STR(c_path);
    return status != -1;
#endif
}

bool epsl_paths_chdir(ARRAY_Byte *path) {
#ifdef _WIN32
    wchar_t *wstr_path = epsl_Estr_to_Wstr(path);
    int status = _wchdir(wstr_path);
    free(wstr_path);
    return status != -1;
#else
    EPSL_STR_TO_C_STR(path, c_path);
    int status = chdir(c_path);
    CLEANUP_CONV_C_STR(c_path);
    return status != -1;
#endif
}
