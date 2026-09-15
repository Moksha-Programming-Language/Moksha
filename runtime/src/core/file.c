#include "../../include/moksha_rt.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* @brief Uncomment the debugger statements when needed */

#if defined(__MOKSHA_BAREMETAL__)

// Minimal stubs for freestanding environments without a filesystem
void moksha_file_open(MokshaAny *out_any, char *path, int32_t mode) {
  (void)path;
  (void)mode;
  if (out_any) {
    out_any->data = NULL;
    out_any->vtable = NULL;
  }
}
void moksha_file_close(MokshaAny *file_any) { (void)file_any; }
void moksha_file_write(MokshaAny *file_any, MokshaAny *data_any) {
  (void)file_any;
  (void)data_any;
}
void moksha_file_read(MokshaAny *out_any, MokshaAny *file_any) {
  (void)file_any;
  if (out_any) {
    out_any->data = NULL;
    out_any->vtable = NULL;
  }
}
int64_t moksha_file_size(MokshaAny *file_any) {
  (void)file_any;
  return 0;
}
void moksha_file_seek(MokshaAny *file_any, int64_t pos) {
  (void)file_any;
  (void)pos;
}
int64_t moksha_file_tell(MokshaAny *file_any) {
  (void)file_any;
  return -1;
}
void moksha_file_flush(MokshaAny *file_any) { (void)file_any; }
bool moksha_file_eof(MokshaAny *file_any) {
  (void)file_any;
  return true;
}
bool moksha_file_exists(char *path) {
  (void)path;
  return false;
}
void moksha_file_truncate(MokshaAny *file_any, int64_t size) {
  (void)file_any;
  (void)size;
}
void moksha_file_writeLine(MokshaAny *file_any, char *text) {
  (void)file_any;
  (void)text;
}
char *moksha_file_readLine(MokshaAny *file_any) {
  (void)file_any;
  return NULL;
}
void moksha_file_readLines(MokshaAny *out_any, MokshaAny *file_any) {
  (void)file_any;
  if (out_any) {
    out_any->data = NULL;
    out_any->vtable = NULL;
  }
}
char *moksha_file_readText(char *path) {
  (void)path;
  return NULL;
}
void moksha_file_writeText(char *path, char *text) {
  (void)path;
  (void)text;
}
void moksha_file_appendText(char *path, char *text) {
  (void)path;
  (void)text;
}
void moksha_file_writeBytes(char *path, MokshaAny *data_any) {
  (void)path;
  (void)data_any;
}
void moksha_file_appendBytes(char *path, MokshaAny *data_any) {
  (void)path;
  (void)data_any;
}
void moksha_file_readBytes(MokshaAny *out_any, char *path) {
  (void)path;
  if (out_any) {
    out_any->data = NULL;
    out_any->vtable = NULL;
  }
}
void moksha_file_writeJson(char *path, MokshaAny *data_any) {
  (void)path;
  (void)data_any;
}
void moksha_file_readJson(MokshaAny *out_any, char *path) {
  (void)path;
  if (out_any) {
    out_any->data = NULL;
    out_any->vtable = NULL;
  }
}
void moksha_file_writeYaml(char *path, MokshaAny *data_any) {
  (void)path;
  (void)data_any;
}
void moksha_file_readYaml(MokshaAny *out_any, char *path) {
  (void)path;
  if (out_any) {
    out_any->data = NULL;
    out_any->vtable = NULL;
  }
}
void moksha_file_writeCsv(char *path, MokshaAny *data_any) {
  (void)path;
  (void)data_any;
}
void moksha_file_readCsv(MokshaAny *out_any, char *path) {
  (void)path;
  if (out_any) {
    out_any->data = NULL;
    out_any->vtable = NULL;
  }
}
void moksha_file_createPdf(MokshaAny *out_any, char *path) {
  (void)path;
  if (out_any) {
    out_any->data = NULL;
    out_any->vtable = NULL;
  }
}
void moksha_file_writePdfText(MokshaAny *pdf_any, char *text) {
  (void)pdf_any;
  (void)text;
}
void moksha_file_savePdf(MokshaAny *pdf_any) { (void)pdf_any; }
void moksha_file_openPdf(MokshaAny *out_any, char *path) {
  (void)path;
  if (out_any) {
    out_any->data = NULL;
    out_any->vtable = NULL;
  }
}
char *moksha_file_extractText(MokshaAny *pdf_any) {
  (void)pdf_any;
  return NULL;
}
bool moksha_file_createDir(char *path) {
  (void)path;
  return false;
}
bool moksha_file_isDir(char *path) {
  (void)path;
  return false;
}
bool moksha_file_isFile(char *path) {
  (void)path;
  return false;
}
bool moksha_file_copy(char *src, char *dst) {
  (void)src;
  (void)dst;
  return false;
}
bool moksha_file_move(char *src, char *dst) {
  (void)src;
  (void)dst;
  return false;
}
bool moksha_file_remove(char *path) {
  (void)path;
  return false;
}
bool moksha_file_removeDir(char *path) {
  (void)path;
  return false;
}
void moksha_file_listDir(MokshaAny *out_any, char *path) {
  (void)path;
  if (out_any) {
    out_any->data = NULL;
    out_any->vtable = NULL;
  }
}

// Keep the dynamic dispatch length utility functional for strings/arrays/maps
static size_t internal_strlen_file(const char *s) {
  size_t len = 0;
  while (s && s[len])
    len++;
  return len;
}

extern int32_t moksha_rt_map_len(void *map);
int32_t moksha_rt_any_len(MokshaAny *any_val) {
  if (!any_val || !any_val->vtable || !any_val->data)
    return 0;
  if (any_val->vtable->type_id == MOKSHA_TYPE_STRING) {
    return (int32_t)internal_strlen_file((char *)any_val->data);
  } else if (any_val->vtable->type_id == MOKSHA_TYPE_ARRAY) {
    MokshaSlice *slice = (MokshaSlice *)any_val->data;
    return (int32_t)slice->length;
  } else if (any_val->vtable->type_id == MOKSHA_TYPE_TABLE) {
    return moksha_rt_map_len(any_val->data);
  }
  return 0;
}

#else

// ---------------------------------------------------------
// Original Host OS Implementations (Linux / Windows / MacOS)
// ---------------------------------------------------------
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

extern const AnyVTable vtable_string;
extern const AnyVTable vtable_array;
extern const AnyVTable vtable_map;

extern void *moksha_mem_alloc(size_t size);
extern void moksha_mem_free(void *ptr);

#ifdef _WIN32
#include <io.h>
#define fsync _commit
#endif

#ifndef O_BINARY
#define O_BINARY 0
#endif

#include <stdarg.h>
#include <stdio.h>

// static volatile long _moksha_open_files_count = 0;
// static volatile long _moksha_file_ops = 0;

// static inline void trigger_file_debug() {
//   long ops = __atomic_add_fetch(&_moksha_file_ops, 1, __ATOMIC_RELAXED);
//   // Print every 10,000 file operations
//   if (ops % 10000 == 0) {
//     printf("[FILE DEBUG] Open File Descriptors: %ld\n",
//            _moksha_open_files_count);
//   }
// }

static inline int tracked_open(const char *pathname, int flags, ...) {
  int fd;
  if (flags & O_CREAT) {
    va_list args;
    va_start(args, flags);
    int mode = va_arg(args, int);
    va_end(args);
    fd = open(pathname, flags, mode);
  } else {
    fd = open(pathname, flags);
  }

  // if (fd >= 0) {
  //   __atomic_add_fetch(&_moksha_open_files_count, 1, __ATOMIC_RELAXED);
  //   trigger_file_debug();
  // }
  return fd;
}

static inline int tracked_close(int fd) {
  int res = close(fd);
  // if (res == 0) {
  //   __atomic_sub_fetch(&_moksha_open_files_count, 1, __ATOMIC_RELAXED);
  //   trigger_file_debug();
  // }
  return res;
}

// Override POSIX calls for the rest of this file
#define open(path, ...) tracked_open(path, __VA_ARGS__)
#define close(fd) tracked_close(fd)

// Internal Utilities

static void moksha_array_string_dtor(void *ptr) {
  MokshaSlice *slice = (MokshaSlice *)ptr;
  if (!slice || !slice->data)
    return;
  char **strings = (char **)slice->data;
  for (uint64_t i = 0; i < slice->length; i++) {
    if (strings[i])
      moksha_rt_release(strings[i]);
  }
}

static const AnyVTable vtable_array_string = {18, NULL, moksha_rt_retain,
                                              moksha_array_string_dtor};

static void moksha_array_any_dtor(void *ptr) {
  MokshaSlice *slice = (MokshaSlice *)ptr;
  if (!slice || !slice->data)
    return;
  MokshaAny *anys = (MokshaAny *)slice->data;
  for (uint64_t i = 0; i < slice->length; i++) {
    if (anys[i].data) {
      if (anys[i].vtable && anys[i].vtable->drop) {
        moksha_rt_release_with_dtor(anys[i].data, anys[i].vtable->drop);
      } else {
        moksha_rt_release(anys[i].data);
      }
    }
  }
}

static const AnyVTable vtable_array_any = {18, NULL, moksha_rt_retain,
                                           moksha_array_any_dtor};

static char *make_mstring(const char *cstr, size_t len) {
  char *str = (char *)moksha_rt_alloc(len + 1, 16);
  memcpy(str, cstr, len);
  str[len] = '\0';
  return str;
}

static inline int unbox_fd(MokshaAny *file_any) {
  if (!file_any || !file_any->data)
    return -1;
  return (int)*(intptr_t *)file_any->data;
}

int32_t length(void *arr_ptr) {
  if (!arr_ptr)
    return 0;
  MokshaAny *any_val = (MokshaAny *)arr_ptr;
  if (!any_val->data)
    return 0;

  // Single unbox ABI matching the compiler
  MokshaSlice *slice = (MokshaSlice *)any_val->data;
  return (int32_t)slice->length;
}

int32_t moksha_rt_any_len(MokshaAny *any_val) {
  if (!any_val || !any_val->vtable || !any_val->data)
    return 0;

  if (any_val->vtable->type_id == MOKSHA_TYPE_STRING) {
    return strlen((char *)any_val->data);
  } else if (any_val->vtable->type_id == MOKSHA_TYPE_ARRAY) {
    // Single unbox ABI matching the compiler
    MokshaSlice *slice = (MokshaSlice *)any_val->data;
    return (int32_t)slice->length;
  } else if (any_val->vtable->type_id == MOKSHA_TYPE_TABLE) {
    return moksha_rt_map_len(any_val->data);
  }
  return 0;
}

/** @brief Internal Literal Parser for Structured Data */
static MokshaAny *parse_and_box_literal(const char *val_start, int val_len,
                                        bool is_string) {
  MokshaAny *box = (MokshaAny *)moksha_rt_alloc(sizeof(MokshaAny), 19);

  if (is_string) {
    char *val_str = moksha_rt_alloc(val_len + 1, 16); // MOKSHA_TYPE_STRING
    memcpy(val_str, val_start, val_len);
    val_str[val_len] = '\0';
    box->data = val_str;
    box->vtable = &vtable_string;
    return box;
  }

  while (val_len > 0 &&
         (val_start[val_len - 1] == ' ' || val_start[val_len - 1] == '\r')) {
    val_len--;
  }

  char tmp[128];
  int copy_len = val_len < 127 ? val_len : 127;
  memcpy(tmp, val_start, copy_len);
  tmp[copy_len] = '\0';

  if (strncmp(tmp, "true", 4) == 0) {
    bool *b = moksha_rt_alloc(sizeof(bool), 0);
    *b = true;
    box->data = b;
    box->vtable = NULL;
    return box;
  }
  if (strncmp(tmp, "false", 5) == 0) {
    bool *b = moksha_rt_alloc(sizeof(bool), 0);
    *b = false;
    box->data = b;
    box->vtable = NULL;
    return box;
  }
  if (strncmp(tmp, "null", 4) == 0) {
    box->data = NULL;
    box->vtable = NULL;
    return box;
  }

  bool is_float = false;
  for (int i = 0; i < copy_len; i++) {
    if (tmp[i] == '.') {
      is_float = true;
      break;
    }
  }

  if (is_float) {
    char *endptr;
    double val = strtod(tmp, &endptr);
    if (endptr != tmp) {
      double *d = moksha_rt_alloc(sizeof(double), 14);
      *d = val;
      box->data = d;
      box->vtable = NULL;
      return box;
    }
  } else {
    char *endptr;
    long val = strtol(tmp, &endptr, 10);
    if (endptr != tmp) {
      int32_t *i = moksha_rt_alloc(sizeof(int32_t), 5);
      *i = (int32_t)val;
      box->data = i;
      box->vtable = NULL;
      return box;
    }
  }

  // Fallback to string
  return parse_and_box_literal(val_start, val_len, true);
}

/** @brief Raw File Descriptor Builtins */

void moksha_file_open(MokshaAny *out_any, char *path, int32_t mode) {
  if (!out_any)
    return;
  if (!path) {
    out_any->data = NULL;
    out_any->vtable = NULL;
    return;
  }

  int flags = 0;
  if ((mode & 1) && (mode & 2))
    flags = O_RDWR;
  else if (mode & 2)
    flags = O_WRONLY;
  else
    flags = O_RDONLY;

  if (mode & 4)
    flags |= O_APPEND;
  if (mode & 8)
    flags |= O_BINARY;
  if (mode & 16)
    flags |= O_CREAT;
  if (mode & 32)
    flags |= O_TRUNC;

  int fd = open(path, flags, 0666);
  if (fd == -1) {
    out_any->data = NULL;
    out_any->vtable = NULL;
    return;
  }

  intptr_t *fd_box = (intptr_t *)moksha_rt_alloc(sizeof(intptr_t), 19);
  *fd_box = fd;

  out_any->data = fd_box;
  out_any->vtable = NULL;
}

void moksha_file_close(MokshaAny *file_any) {
  int fd = unbox_fd(file_any);
  if (fd >= 0)
    close(fd);
}

void moksha_file_write(MokshaAny *file_any, MokshaAny *data_any) {
  int fd = unbox_fd(file_any);
  if (fd < 0 || !data_any || !data_any->data)
    return;
  char *data = (char *)data_any->data;
  write(fd, data, strlen(data));
}

void moksha_file_read(MokshaAny *out_any, MokshaAny *file_any) {
  if (!out_any)
    return;
  int fd = unbox_fd(file_any);
  if (fd < 0) {
    out_any->data = NULL;
    out_any->vtable = NULL;
    return;
  }

  struct stat st;
  if (fstat(fd, &st) < 0) {
    out_any->data = NULL;
    out_any->vtable = NULL;
    return;
  }

  size_t size = st.st_size;
  char *buf = (char *)moksha_rt_alloc(size + 1, 16);

  ssize_t bytes_read = read(fd, buf, size);
  if (bytes_read < 0)
    bytes_read = 0;
  buf[bytes_read] = '\0';

  // Attach string vtable so structural equality works
  out_any->data = buf;
  out_any->vtable = &vtable_string;
}

int64_t moksha_file_size(MokshaAny *file_any) {
  int fd = unbox_fd(file_any);
  if (fd < 0)
    return 0;
  struct stat st;
  if (fstat(fd, &st) == 0)
    return (int64_t)st.st_size;
  return 0;
}

void moksha_file_seek(MokshaAny *file_any, int64_t pos) {
  int fd = unbox_fd(file_any);
  if (fd >= 0)
    lseek(fd, (off_t)pos, SEEK_SET);
}

int64_t moksha_file_tell(MokshaAny *file_any) {
  int fd = unbox_fd(file_any);
  if (fd < 0)
    return -1;
  return (int64_t)lseek(fd, 0, SEEK_CUR);
}

void moksha_file_flush(MokshaAny *file_any) {
  int fd = unbox_fd(file_any);
  if (fd >= 0)
    fsync(fd);
}

bool moksha_file_eof(MokshaAny *file_any) {
  int fd = unbox_fd(file_any);
  if (fd < 0)
    return true;
  off_t current = lseek(fd, 0, SEEK_CUR);
  struct stat st;
  fstat(fd, &st);
  return current >= st.st_size;
}

bool moksha_file_exists(char *path) {
  if (!path)
    return false;
#ifdef _WIN32
  // Windows _access: 0 means it exists
  return _access(path, 0) == 0;
#else
  // POSIX access: F_OK means file exists
  return access(path, F_OK) == 0;
#endif
}

void moksha_file_truncate(MokshaAny *file_any, int64_t size) {
  int fd = unbox_fd(file_any);
  if (fd >= 0)
    ftruncate(fd, (off_t)size);
}

/** @brief High-Level Stream IO */

void moksha_file_writeLine(MokshaAny *file_any, char *text) {
  int fd = unbox_fd(file_any);
  if (fd < 0 || !text)
    return;
  write(fd, text, strlen(text));
  write(fd, "\n", 1);
}

char *moksha_file_readLine(MokshaAny *file_any) {
  int fd = unbox_fd(file_any);
  if (fd < 0)
    return NULL;

  size_t cap = 128;
  size_t len = 0;
  char *buf = (char *)moksha_rt_alloc(cap, 16);
  char c;
  bool read_any = false;

  while (read(fd, &c, 1) == 1) {
    read_any = true;
    if (c == '\n')
      break;
    if (c == '\r')
      continue;

    if (len + 1 >= cap) {
      cap *= 2;
      char *new_buf = (char *)moksha_rt_alloc(cap, 16);
      memcpy(new_buf, buf, len);
      moksha_rt_release(buf);
      buf = new_buf;
    }
    buf[len++] = c;
  }

  if (!read_any) {
    moksha_rt_release(buf);
    return NULL;
  }

  buf[len] = '\0';
  return buf;
}

void moksha_file_readLines(MokshaAny *out_any, MokshaAny *file_or_path_any) {
  if (!out_any)
    return;

  if (!file_or_path_any) {
    out_any->data = NULL;
    out_any->vtable = NULL;
    return;
  }

  int fd = -1;
  bool should_close = false;
  MokshaAny temp = {NULL, NULL};

  if (file_or_path_any->vtable && file_or_path_any->vtable->type_id == 16) {
    char *path = (char *)file_or_path_any->data;
    moksha_file_open(&temp, path, 0);
    fd = unbox_fd(&temp);
    should_close = true;
  } else {
    fd = unbox_fd(file_or_path_any);
  }

  if (fd < 0) {
    if (temp.data)
      moksha_rt_release(temp.data);
    out_any->data = NULL;
    out_any->vtable = NULL;
    return;
  }

  intptr_t fd_box = fd;
  MokshaAny temp_any = {&fd_box, NULL};

  size_t cap = 16;
  size_t count = 0;
  char **arr = (char **)moksha_mem_alloc(cap * sizeof(char *));

  while (true) {
    char *line = moksha_file_readLine(&temp_any);
    if (!line)
      break;

    if (count >= cap) {
      cap *= 2;
      char **new_arr = (char **)moksha_mem_alloc(cap * sizeof(char *));
      memcpy(new_arr, arr, count * sizeof(char *));
      // Fix: Free the raw memory, do not ARC release it
      moksha_mem_free(arr);
      arr = new_arr;
    }
    arr[count++] = line;
  }

  if (should_close) {
    close(fd);
    // Fix: Clean up the temporary ARC fd_box
    moksha_rt_release(temp.data);
  }

  MokshaSlice *slice = (MokshaSlice *)moksha_rt_alloc(sizeof(MokshaSlice), 18);
  slice->data = arr;
  slice->length = count;

  out_any->data = slice;
  out_any->vtable = &vtable_array_string;
}

char *moksha_file_readText(char *path) {
  if (!path)
    return NULL;

  MokshaAny file_any;
  moksha_file_open(&file_any, path, 0);

  if (unbox_fd(&file_any) < 0)
    return NULL;

  MokshaAny result_any;
  moksha_file_read(&result_any, &file_any);
  moksha_file_close(&file_any);

  moksha_rt_release(file_any.data);
  return (char *)result_any.data;
}

void moksha_file_writeText(char *path, char *text) {
  if (!path || !text)
    return;
  int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
  if (fd >= 0) {
    write(fd, text, strlen(text));
    close(fd);
  }
}

void moksha_file_appendText(char *path, char *text) {
  if (!path || !text)
    return;
  int fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0666);
  if (fd >= 0) {
    write(fd, text, strlen(text));
    close(fd);
  }
}

/** @brief High-Level Binary IO */

void moksha_file_writeBytes(char *path, MokshaAny *data_any) {
  if (!path || !data_any || !data_any->data)
    return;

  MokshaSlice *slice = (MokshaSlice *)data_any->data;
  int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0666);

  if (fd >= 0) {
    uint8_t *bytes = malloc(slice->length);
    int32_t *ints = (int32_t *)slice->data;
    for (uint64_t i = 0; i < slice->length; i++) {
      bytes[i] = (uint8_t)ints[i];
    }
    write(fd, bytes, slice->length);
    free(bytes);
    close(fd);
  }
}

void moksha_file_appendBytes(char *path, MokshaAny *data_any) {
  if (!path || !data_any || !data_any->data)
    return;

  MokshaSlice *slice = (MokshaSlice *)data_any->data;
  int fd = open(path, O_WRONLY | O_CREAT | O_APPEND | O_BINARY, 0666);

  if (fd >= 0) {
    uint8_t *bytes = malloc(slice->length);
    int32_t *ints = (int32_t *)slice->data;
    for (uint64_t i = 0; i < slice->length; i++) {
      bytes[i] = (uint8_t)ints[i];
    }
    write(fd, bytes, slice->length);
    free(bytes);
    close(fd);
  }
}

void moksha_file_readBytes(MokshaAny *out_any, char *path) {
  if (!out_any)
    return;
  if (!path) {
    out_any->data = NULL;
    out_any->vtable = NULL;
    return;
  }

  int fd = open(path, O_RDONLY | O_BINARY);
  if (fd < 0) {
    out_any->data = NULL;
    out_any->vtable = NULL;
    return;
  }

  struct stat st;
  if (fstat(fd, &st) < 0) {
    close(fd);
    out_any->data = NULL;
    out_any->vtable = NULL;
    return;
  }

  size_t size = st.st_size;
  char *buf = (char *)moksha_mem_alloc(size);

  ssize_t bytes_read = read(fd, buf, size);
  if (bytes_read < 0)
    bytes_read = 0;
  close(fd);

  MokshaSlice *slice = (MokshaSlice *)moksha_rt_alloc(sizeof(MokshaSlice), 18);
  slice->data = buf;
  slice->length = bytes_read;

  out_any->data = slice;
  out_any->vtable = &vtable_array;
}

/** @brief Structured Data (JSON / YAML / PDF File Streamers) */

void moksha_file_writeJson(char *path, MokshaAny *data_any) {
  void *map_ptr = data_any ? data_any->data : NULL;
  if (!map_ptr)
    return;

  int len = moksha_rt_map_len(map_ptr);

  int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0666);
  if (fd < 0)
    return;

  write(fd, "{\n", 2);
  for (int i = 0; i < len; i++) {
    MokshaAny *k = moksha_rt_map_get_key_at(map_ptr, i);
    MokshaAny *v = moksha_rt_map_get_val_at(map_ptr, i);

    const char *k_str = (k && k->data) ? (const char *)k->data : "";
    bool is_str = (v && v->vtable && v->vtable->type_id == 16);
    const char *v_str =
        is_str ? (const char *)v->data : (v ? __moksha_any_to_string(v) : "");

    write(fd, "  \"", 3);
    write(fd, k_str, strlen(k_str));
    write(fd, "\": ", 3);

    if (is_str)
      write(fd, "\"", 1);
    write(fd, v_str, strlen(v_str));
    if (is_str)
      write(fd, "\"", 1);

    if (!is_str && v) {
      moksha_rt_release((void *)(uintptr_t)v_str);
    }

    if (i < len - 1)
      write(fd, ",", 1);
    write(fd, "\n", 1);
  }
  write(fd, "}\n", 2);
  close(fd);
}

void moksha_file_readJson(MokshaAny *out_any, char *path) {
  if (!out_any)
    return;

  char *text = moksha_file_readText(path);
  void *map = moksha_rt_map_new();
  if (!text) {
    out_any->data = map;
    out_any->vtable = &vtable_map;
    return;
  }

  char *p = text;
  while (*p && *p != '{')
    p++;
  if (*p == '{')
    p++;

  while (*p && *p != '}') {
    while (*p == ' ' || *p == '\n' || *p == '\r' || *p == ',')
      p++;
    if (*p == '}')
      break;

    if (*p == '"')
      p++;
    char *key_start = p;
    while (*p && *p != '"')
      p++;
    int key_len = p - key_start;
    if (*p == '"')
      p++;

    while (*p && *p != ':')
      p++;
    if (*p == ':')
      p++;
    while (*p == ' ' || *p == '\n' || *p == '\r')
      p++;

    bool is_string = (*p == '"');
    if (is_string)
      p++;

    char *val_start = p;
    if (is_string) {
      while (*p && *p != '"')
        p++;
    } else {
      while (*p && *p != ',' && *p != '}' && *p != ' ' && *p != '\n' &&
             *p != '\r')
        p++;
    }
    int val_len = p - val_start;
    if (is_string && *p == '"')
      p++;

    char *key_str = moksha_rt_alloc(key_len + 1, 16); // MOKSHA_TYPE_STRING
    memcpy(key_str, key_start, key_len);
    key_str[key_len] = '\0';

    MokshaAny *kp = (MokshaAny *)moksha_rt_alloc(sizeof(MokshaAny), 19);
    kp->data = key_str;
    kp->vtable = &vtable_string;

    MokshaAny *vp = parse_and_box_literal(val_start, val_len, is_string);
    moksha_rt_map_insert(map, kp, vp);
    moksha_rt_release(kp);
    moksha_rt_release(vp);
  }
  if (text)
    moksha_rt_release(text);
  out_any->data = map;
  out_any->vtable = &vtable_map;
}

void moksha_file_writeYaml(char *path, MokshaAny *data_any) {
  void *map_ptr = data_any ? data_any->data : NULL;
  if (!map_ptr)
    return;

  int len = moksha_rt_map_len(map_ptr);

  int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0666);
  if (fd < 0)
    return;

  for (int i = 0; i < len; i++) {
    MokshaAny *k = moksha_rt_map_get_key_at(map_ptr, i);
    MokshaAny *v = moksha_rt_map_get_val_at(map_ptr, i);

    const char *k_str = (k && k->data) ? (const char *)k->data : "";
    bool is_str = (v && v->vtable && v->vtable->type_id == 16);
    const char *v_str =
        is_str ? (const char *)v->data : (v ? __moksha_any_to_string(v) : "");

    write(fd, k_str, strlen(k_str));
    write(fd, ": ", 2);
    write(fd, v_str, strlen(v_str));
    write(fd, "\n", 1);

    if (!is_str && v) {
      moksha_rt_release((void *)(uintptr_t)v_str);
    }
  }
  close(fd);
}

void moksha_file_readYaml(MokshaAny *out_any, char *path) {
  if (!out_any)
    return;

  char *text = moksha_file_readText(path);
  void *map = moksha_rt_map_new();
  if (!text) {
    out_any->data = map;
    out_any->vtable = &vtable_map;
    return;
  }

  char *p = text;
  while (*p) {
    while (*p == ' ' || *p == '\n' || *p == '\r')
      p++;
    if (!*p)
      break;

    char *key_start = p;
    while (*p && *p != ':')
      p++;
    int key_len = p - key_start;
    if (*p == ':')
      p++;
    while (*p == ' ')
      p++;

    char *val_start = p;
    while (*p && *p != '\n' && *p != '\r')
      p++;
    int val_len = p - val_start;

    bool is_string = false;
    if (val_len >= 2 && val_start[0] == '"' && val_start[val_len - 1] == '"') {
      is_string = true;
      val_start++;
      val_len -= 2;
    }

    char *key_str = moksha_rt_alloc(key_len + 1, 16); // MOKSHA_TYPE_STRING
    memcpy(key_str, key_start, key_len);
    key_str[key_len] = '\0';

    MokshaAny *kp = (MokshaAny *)moksha_rt_alloc(sizeof(MokshaAny), 19);
    kp->data = key_str;
    kp->vtable = &vtable_string;

    MokshaAny *vp = parse_and_box_literal(val_start, val_len, is_string);
    moksha_rt_map_insert(map, kp, vp);
    moksha_rt_release(kp);
    moksha_rt_release(vp);
  }

  if (text)
    moksha_rt_release(text);
  out_any->data = map;
  out_any->vtable = &vtable_map;
}

/** @brief CSV File Streamers (Array of Tables) */

void moksha_file_writeCsv(char *path, MokshaAny *data_any) {
  if (!path || !data_any || !data_any->data)
    return;

  // Single unbox ABI matching the compiler
  MokshaSlice *slice = (MokshaSlice *)data_any->data;
  if (slice->length == 0)
    return;

  int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0666);
  if (fd < 0)
    return;

  // Assume data_any is an array of maps. Get the first row to extract headers.
  MokshaAny *first_row = &((MokshaAny *)slice->data)[0];
  void *map_ptr = first_row->data;
  if (!map_ptr) {
    close(fd);
    return;
  }

  int cols = moksha_rt_map_len(map_ptr);

  // Write Headers
  for (int i = 0; i < cols; i++) {
    MokshaAny *k = moksha_rt_map_get_key_at(map_ptr, i);
    const char *k_str = "";
    if (k && k->vtable && k->vtable->type_id == 16) {
      k_str = (const char *)k->data; // Directly unbox C-string
    }

    bool needs_quotes = strchr(k_str, ',') != NULL;
    if (needs_quotes)
      write(fd, "\"", 1);
    write(fd, k_str, strlen(k_str));
    if (needs_quotes)
      write(fd, "\"", 1);

    if (i < cols - 1)
      write(fd, ",", 1);
  }
  write(fd, "\n", 1);

  // Write Rows
  for (uint64_t r = 0; r < slice->length; r++) {
    MokshaAny *row_any = &((MokshaAny *)slice->data)[r];
    void *row_map = row_any->data;
    if (!row_map)
      continue;

    for (int i = 0; i < cols; i++) {
      MokshaAny *v = moksha_rt_map_get_val_at(row_map, i);

      // Match the JSON/YAML implementation for string extraction
      bool is_str = (v && v->vtable && v->vtable->type_id == 16);
      const char *v_str =
          is_str ? (const char *)v->data : (v ? __moksha_any_to_string(v) : "");

      bool needs_quotes = strchr(v_str, ',') != NULL;
      if (needs_quotes)
        write(fd, "\"", 1);
      write(fd, v_str, strlen(v_str));
      if (needs_quotes)
        write(fd, "\"", 1);

      if (!is_str && v) {
        moksha_rt_release((void *)(uintptr_t)v_str);
      }

      if (i < cols - 1)
        write(fd, ",", 1);
    }
    write(fd, "\n", 1);
  }
  close(fd);
}

void moksha_file_readCsv(MokshaAny *out_any, char *path) {
  if (!out_any)
    return;

  MokshaSlice *empty = (MokshaSlice *)moksha_rt_alloc(sizeof(MokshaSlice), 18);
  empty->data = NULL;
  empty->length = 0;

  if (!path) {
    out_any->data = empty;
    out_any->vtable = &vtable_array;
    return;
  }

  int fd = open(path, O_RDONLY | O_BINARY);
  if (fd < 0) {
    out_any->data = empty;
    out_any->vtable = &vtable_array;
    return;
  }

  off_t size = lseek(fd, 0, SEEK_END);
  lseek(fd, 0, SEEK_SET);

  char *text = (char *)malloc(size + 1);
  int bytes_read = read(fd, text, size);
  close(fd);

  if (bytes_read < 0) {
    free(text);
    out_any->data = empty;
    out_any->vtable = &vtable_array;
    return;
  }
  text[bytes_read] = '\0';

  size_t cap = 16;
  size_t count = 0;
  MokshaAny *arr = (MokshaAny *)moksha_mem_alloc(cap * sizeof(MokshaAny));

  char *p = text;
  char *headers[256];
  int cols = 0;

  while (*p == ' ' || *p == '\r' || *p == '\n')
    p++;

  // 1. Parse Headers
  while (*p && *p != '\n' && *p != '\r' && cols < 256) {
    char *start = p;
    if (*p == '"') {
      p++;
      start = p;
      while (*p && *p != '"')
        p++;
      int len = p - start;
      headers[cols] = make_mstring(start, len);
      if (*p == '"')
        p++;
    } else {
      while (*p && *p != ',' && *p != '\n' && *p != '\r')
        p++;
      int len = p - start;
      headers[cols] = make_mstring(start, len);
    }
    cols++;
    if (*p == ',')
      p++;
  }

  if (*p == '\r')
    p++;
  if (*p == '\n')
    p++;

  // 2. Parse Rows
  while (*p) {
    if (*p == '\n' || *p == '\r') {
      p++;
      continue;
    }

    void *map = moksha_rt_map_new();

    for (int i = 0; i < cols; i++) {
      if (!*p || *p == '\n' || *p == '\r')
        break;

      char *start = p;
      if (*p == '"') {
        p++;
        start = p;
        while (*p && *p != '"')
          p++;
        int len = p - start;

        moksha_rt_retain(headers[i]);
        MokshaAny *kp = (MokshaAny *)moksha_rt_alloc(sizeof(MokshaAny), 19);
        kp->data = headers[i];
        kp->vtable = &vtable_string;

        MokshaAny *vp = parse_and_box_literal(start, len, true);
        moksha_rt_map_insert(map, kp, vp);
        moksha_rt_release(kp);
        moksha_rt_release(vp);

        if (*p == '"')
          p++;
      } else {
        while (*p && *p != ',' && *p != '\n' && *p != '\r')
          p++;
        int len = p - start;

        moksha_rt_retain(headers[i]);
        MokshaAny *kp = (MokshaAny *)moksha_rt_alloc(sizeof(MokshaAny), 19);
        kp->data = headers[i];
        kp->vtable = &vtable_string;

        MokshaAny *vp = parse_and_box_literal(start, len, false);
        moksha_rt_map_insert(map, kp, vp);
        moksha_rt_release(kp);
        moksha_rt_release(vp);
      }

      if (*p == ',')
        p++;
    }

    if (count >= cap) {
      cap *= 2;
      MokshaAny *new_arr =
          (MokshaAny *)moksha_mem_alloc(cap * sizeof(MokshaAny));
      memcpy(new_arr, arr, count * sizeof(MokshaAny));
      moksha_mem_free(arr);
      arr = new_arr;
    }

    arr[count].data = map;
    arr[count].vtable = &vtable_map;
    count++;

    while (*p && *p != '\n' && *p != '\r')
      p++;
    if (*p == '\r')
      p++;
    if (*p == '\n')
      p++;
  }

  for (int i = 0; i < cols; i++) {
    moksha_rt_release(headers[i]);
  }

  free(text);

  MokshaSlice *slice = (MokshaSlice *)moksha_rt_alloc(sizeof(MokshaSlice), 18);
  slice->data = arr;
  slice->length = count;

  out_any->data = slice;
  out_any->vtable = &vtable_array_any;
}

/** @brief PDF endpoints (Mock for demo) */

void moksha_file_createPdf(MokshaAny *out_any, char *path) {
  if (!out_any)
    return;
  moksha_file_open(out_any, path, 2 | 16 | 32);
  int fd = unbox_fd(out_any);

  if (fd >= 0) {
    const char *magic = "%PDF-1.4\n%\xE2\xE3\xCF\xD3\n";
    write(fd, magic, strlen(magic));
  }
}

void moksha_file_writePdfText(MokshaAny *pdf_any, char *text) {
  moksha_file_writeLine(pdf_any, text);
}

void moksha_file_savePdf(MokshaAny *pdf_any) { moksha_file_close(pdf_any); }

void moksha_file_openPdf(MokshaAny *out_any, char *path) {
  moksha_file_open(out_any, path, 1);
}

char *moksha_file_extractText(MokshaAny *pdf_any) {
  MokshaAny result_any;
  moksha_file_read(&result_any, pdf_any);
  char *raw_buffer = (char *)result_any.data;

  if (!raw_buffer)
    return NULL;

  const char *magic = "%PDF-1.4\n%\xE2\xE3\xCF\xD3\n";
  size_t magic_len = strlen(magic);

  if (strncmp(raw_buffer, magic, magic_len) == 0) {
    char *text_start = raw_buffer + magic_len;
    size_t text_len = strlen(text_start);

    if (text_len > 0 && text_start[text_len - 1] == '\n') {
      text_len--;
    }

    char *extracted = (char *)moksha_rt_alloc(text_len + 1, 16);
    memcpy(extracted, text_start, text_len);
    extracted[text_len] = '\0';
    return extracted;
  }

  return raw_buffer;
}

/** @brief Directory Operations */

bool moksha_file_createDir(char *path) {
  if (!path)
    return false;
#ifdef _WIN32
  return mkdir(path) == 0;
#else
  return mkdir(path, 0777) == 0;
#endif
}

bool moksha_file_isDir(char *path) {
  struct stat buffer;
  if (stat(path, &buffer) != 0)
    return false;
  return S_ISDIR(buffer.st_mode);
}

bool moksha_file_isFile(char *path) {
  struct stat buffer;
  if (stat(path, &buffer) != 0)
    return false;
  return S_ISREG(buffer.st_mode);
}

bool moksha_file_copy(char *src, char *dst) {
  if (!src || !dst)
    return false;
  int source = open(src, O_RDONLY);
  int target = open(dst, O_WRONLY | O_CREAT | O_TRUNC, 0666);

  if (source < 0 || target < 0) {
    if (source >= 0)
      close(source);
    if (target >= 0)
      close(target);
    return false;
  }

  char buf[4096];
  ssize_t n;
  while ((n = read(source, buf, sizeof(buf))) > 0) {
    write(target, buf, n);
  }

  close(source);
  close(target);
  return true;
}

bool moksha_file_move(char *src, char *dst) {
  if (!src || !dst)
    return false;
  return rename(src, dst) == 0;
}

bool moksha_file_remove(char *path) {
  if (!path)
    return false;
  return unlink(path) == 0;
}

bool moksha_file_removeDir(char *path) {
  if (!path)
    return false;
  return rmdir(path) == 0;
}

void moksha_file_listDir(MokshaAny *out_any, char *path) {
  if (!out_any)
    return;
  if (!path) {
    out_any->data = NULL;
    out_any->vtable = NULL;
    return;
  }

  DIR *dir = opendir(path);
  if (!dir) {
    MokshaSlice *empty = (MokshaSlice *)moksha_rt_alloc(sizeof(MokshaSlice), 2);
    empty->data = NULL;
    empty->length = 0;
    out_any->data = empty;
    out_any->vtable = &vtable_array;
    return;
  }

  size_t cap = 16;
  size_t len = 0;
  char **arr = (char **)moksha_mem_alloc(cap * sizeof(char *));

  struct dirent *ent;
  while ((ent = readdir(dir)) != NULL) {
    if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) {
      continue;
    }
    if (len >= cap) {
      cap *= 2;
      char **new_arr = (char **)moksha_mem_alloc(cap * sizeof(char *));
      memcpy(new_arr, arr, len * sizeof(char *));
      moksha_mem_free(arr);
      arr = new_arr;
    }
    size_t name_len = strlen(ent->d_name);
    arr[len++] = make_mstring(ent->d_name, name_len);
  }
  closedir(dir);

  MokshaSlice *slice = (MokshaSlice *)moksha_rt_alloc(sizeof(MokshaSlice), 2);
  slice->data = arr;
  slice->length = len;

  out_any->data = slice;
  out_any->vtable = &vtable_array_string;
}

#endif
