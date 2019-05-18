/* -*- Mode: C; tab-width: 8; c-basic-offset: 2; indent-tabs-mode: nil; -*- */

#include "util.h"

#include <asm/ldt.h>
#include <asm/prctl.h>
#include <sys/prctl.h>

// Only available in kernel patch
#ifndef ARCH_SET_XCR0
#define ARCH_SET_XCR0 0x1021
#endif

static uint64_t xgetbv(uint32_t which)
{
    uint32_t eax, edx;
	__asm__ volatile ("xgetbv" : "=a" (eax), "=d" (edx) : "c" (which));
	return (((uint64_t)edx) << 32) | eax;
}

int main(void) {
    uint64_t our_xcr0 = xgetbv(0);
    int ret = syscall(SYS_arch_prctl, ARCH_SET_XCR0, 0x7);
    test_assert(ret == 0);
    test_assert(xgetbv(0) == 0x7);
    ret = syscall(SYS_arch_prctl, ARCH_SET_XCR0, our_xcr0);
    test_assert(ret == 0);
    test_assert(xgetbv(0) == our_xcr0);
    return 0;
}
