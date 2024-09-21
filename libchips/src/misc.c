#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

// `malloc`, but aborts on failure
void* xmalloc(size_t size) {
  void* ptr = malloc(size);
  if (ptr == NULL && size != 0) {
    abort();
  }
  return ptr;
}

void* xcalloc(size_t memb_size, size_t memb_n) {
  void* ptr = calloc(memb_size, memb_n);
  if (ptr == NULL && memb_size * memb_n != 0) {
    abort();
  }
  return ptr;
}

// `realloc`, but aborts on failure and handles zero size correctly
void* xrealloc(void* old_ptr, size_t size) {
  if (size == 0) {
    if (old_ptr != NULL) {
      free(old_ptr);
    }
    return NULL;
  }
  void* ptr = realloc(old_ptr, size);
  if (ptr == NULL) {
    abort();
  }
  return ptr;
}


// `sprintf` but just measures and returns a string by itself
char* stringf(const char* msg, ...) {
  va_list list1;
  va_list list2;
  va_start(list1, msg);
  va_copy(list2, list1);
  int string_size = vsnprintf(NULL, 0, msg, list1) + 1;
  va_end(list1);
  char* formatted_msg = xmalloc(string_size);
  vsnprintf(formatted_msg, string_size, msg, list2);
  va_end(list2);
  return formatted_msg;
};

#ifndef NDEBUG
void fprintfnl(FILE* stream, const char* fmt, ...) {
  va_list list;
  va_start(list, fmt);
  vfprintf(stream, fmt, list);
  fputs("\n", stream);
  va_end(list);
}
#endif
