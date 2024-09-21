#ifndef MISC_H
#define MISC_H

#include <stddef.h>

#define DEFINE_RESULT(type)      \
  typedef struct Result_##type { \
    bool success;                \
    union {                      \
      type value;                \
      char* error;               \
    };                           \
  } Result_##type;
#define res_err(type, ...) \
  ((Result_##type){.success = false, .error = stringf(__VA_ARGS__)})

#define res_val(type, val) ((Result_##type){.success = true, .value = (val)})

#if defined(__has_attribute) && __has_attribute(__malloc__)
#define attr_malloc __attribute__((__malloc__))
#else
#define attr_malloc
#endif
#if defined(__has_attribute) && __has_attribute(__alloc_size__)
#define attr_alloc_size(params) __attribute__((__alloc_size__ params))
#else
#define attr_alloc_size(params)
#endif

#if defined(__has_attribute) && __has_attribute(__printf__)
#define attr_printf(params) __attribute__((__format__(__printf__, params)))
#else
#define attr_printf(...)
#endif

#if defined(__has_builtin) &&                           \
    __has_builtin(__builtin_expect_with_probability) && \
    __has_builtin(__builtin_expect)
#define compiler_expect_prob(a, b, prob) \
  __builtin_expect_with_probability(a, b, prob)
#define compiler_expect(a, b) __builtin_expect(a, b)
#else
#define compiler_expect_prob(a, b, prob) (a)
#define compiler_expect(a, b) (a)
#endif

#define lengthof(arr) (sizeof(arr) / sizeof(arr[0]))

void* xmalloc(size_t size) attr_malloc attr_alloc_size((1));
void* xcalloc(size_t memb_size, size_t memb_n) attr_malloc
    attr_alloc_size((1, 2));
void* xrealloc(void* old_ptr, size_t size) attr_alloc_size((2));
char* stringf(char const* msg, ...) attr_printf(1, 2);

#ifndef NDEBUG
#include <stdio.h>
void fprintfnl(FILE* stream, char const* fmt, ...) attr_printf(2, 3);
#define warn(...) fprintfnl(stderr, __VA_ARGS__)
#else
#define warn(...)
#endif

#endif  // MISC_H
