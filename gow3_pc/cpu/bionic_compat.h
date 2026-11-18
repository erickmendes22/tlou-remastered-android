// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#ifdef __ANDROID__
#include <stddef.h>
#include <sys/syscall.h>
#include <unistd.h>
// Android exposes the kernel operation but no public getdents64 libc entry.
// Keep the pinned FEX FileUtils implementation; substitute only this libc call.
static inline ssize_t gow3_bionic_getdents64(int fd, void* entries, size_t bytes) {
    return (ssize_t)syscall(__NR_getdents64, fd, entries, bytes);
}
#define getdents64 gow3_bionic_getdents64
#endif
