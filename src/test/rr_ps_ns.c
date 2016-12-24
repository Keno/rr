/* -*- Mode: C; tab-width: 8; c-basic-offset: 2; indent-tabs-mode: nil; -*- */

#include "nsutils.h"
#include "rrutil.h"

int main(void) {
  pid_t pid;
  if (try_setup_ns(CLONE_NEWPID)) {
    atomic_printf("EXIT-SUCCESS");
    // 77 will indicate to driver script that this test was skipped
    return 77;
  }

  // This is the first child, therefore PID 1 in it's PID namespace
  if ((pid = fork()) == 0) {
    // We will check that rr ps includes `(1)` here
    return 78;
  }

  test_assert(pid == waitpid(pid, NULL, 0));
  atomic_printf("EXIT-SUCCESS");
  return 79;
}
