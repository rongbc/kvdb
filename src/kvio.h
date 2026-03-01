#ifndef KVIO_H
#define KVIO_H

#include <errno.h>
#include <limits.h>
#include <sys/types.h>
#include <unistd.h>

static inline int kv_full_pread(int fd, void * buffer, size_t size, off_t offset)
{
    char * p = (char *) buffer;
    size_t remaining = size;
    while (remaining > 0) {
        size_t chunk = remaining;
        if (chunk > SSIZE_MAX) {
            chunk = SSIZE_MAX;
        }
        ssize_t count = pread(fd, p, chunk, offset);
        if (count < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (count == 0) {
            return -1;
        }
        p += count;
        remaining -= (size_t) count;
        offset += count;
    }
    return 0;
}

static inline int kv_full_pwrite(int fd, const void * buffer, size_t size, off_t offset)
{
    const char * p = (const char *) buffer;
    size_t remaining = size;
    while (remaining > 0) {
        size_t chunk = remaining;
        if (chunk > SSIZE_MAX) {
            chunk = SSIZE_MAX;
        }
        ssize_t count = pwrite(fd, p, chunk, offset);
        if (count < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (count == 0) {
            return -1;
        }
        p += count;
        remaining -= (size_t) count;
        offset += count;
    }
    return 0;
}

static inline int kv_full_write(int fd, const void * buffer, size_t size)
{
    const char * p = (const char *) buffer;
    size_t remaining = size;
    while (remaining > 0) {
        size_t chunk = remaining;
        if (chunk > SSIZE_MAX) {
            chunk = SSIZE_MAX;
        }
        ssize_t count = write(fd, p, chunk);
        if (count < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (count == 0) {
            return -1;
        }
        p += count;
        remaining -= (size_t) count;
    }
    return 0;
}

#endif
