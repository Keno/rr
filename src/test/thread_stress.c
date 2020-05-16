/* -*- Mode: C; tab-width: 8; c-basic-offset: 2; indent-tabs-mode: nil; -*- */

#include <pthread.h>
#include <stdarg.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/time.h>
#include <string.h>

/**
 * Print the printf-like arguments to stdout as atomic-ly as we can
 * manage.  Async-signal-safe.  Does not flush stdio buffers (doing so
 * isn't signal safe).
 */
__attribute__((format(printf, 1, 2))) inline static int atomic_printf(
    const char* fmt, ...) {
  va_list args;
  char buf[1024];
  int len;

  va_start(args, fmt);
  len = vsnprintf(buf, sizeof(buf) - 1, fmt, args);
  va_end(args);
  return write(1, buf, len);
}

/**
 * Write |str| on its own line to stdout as atomic-ly as we can
 * manage.  Async-signal-safe.  Does not flush stdio buffers (doing so
 * isn't signal safe).
 */
inline static int atomic_puts(const char* str) {
  return atomic_printf("%s\n", str);
}

#define fprintf(...) USE_dont_write_stderr
#define printf(...) USE_atomic_printf_INSTEAD
#define puts(...) USE_atomic_puts_INSTEAD

inline static int check_cond(int cond) {
  if (!cond) {
    atomic_printf("FAILED: errno=%d (%s)\n", errno, strerror(errno));
  }
  return cond;
}

#define test_assert(cond) assert("FAILED: !" && check_cond(cond))

/* Chosen so that |3MB * THREAD_GROUPS * THREADS_PER_GROUP| exhausts a
 * 32-bit address space. */
#define THREAD_GROUPS 150
#define THREADS_PER_GROUP 10

static void* thread(__attribute__((unused)) void* unused) {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return NULL;
}

int main(void) {
  int i;

  for (i = 0; i < THREAD_GROUPS; ++i) {
    pthread_t threads[THREADS_PER_GROUP];
    int j;
    for (j = 0; j < THREADS_PER_GROUP; ++j) {
      test_assert(0 == pthread_create(&threads[j], NULL, thread, NULL));
    }
    for (j = 0; j < THREADS_PER_GROUP; ++j) {
      test_assert(0 == pthread_join(threads[j], NULL));
    }
  }

  atomic_puts("EXIT-SUCCESS");
  return 0;
}
