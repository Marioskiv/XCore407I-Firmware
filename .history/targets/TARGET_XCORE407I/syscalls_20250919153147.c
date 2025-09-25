#include <sys/stat.h>
#include <errno.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/times.h>

extern char _end; /* Provided by linker script: end of bss/data */
static char *heap_ptr = &_end;

int _close(int fd) {
    (void)fd; return -1;
}

int _fstat(int fd, struct stat *st) {
    (void)fd; if (st){ st->st_mode = S_IFCHR; } return 0;
}

int _isatty(int fd) {
    (void)fd; return 1;
}

off_t _lseek(int fd, off_t pos, int whence) {
    (void)fd; (void)pos; (void)whence; return -1;
}

ssize_t _read(int fd, void *buf, size_t len) {
    (void)fd; (void)buf; (void)len; return 0;
}

ssize_t _write(int fd, const void *buf, size_t len) {
    (void)fd; (void)buf; return (ssize_t)len; /* discard */
}

void* _sbrk(ptrdiff_t incr) {
    char *prev = heap_ptr;
    heap_ptr += incr;
    return prev;
}

/* Provide _exit so newlib's exit() reference during CMake try-compile resolves */
void _exit(int status) {
    (void)status;
    while (1) { /* hang */ }
}

int _kill(int pid, int sig) {
    (void)pid; (void)sig; errno = EINVAL; return -1;
}

int _getpid(void) {
    return 1;
}

int _stat(const char *path, struct stat *st) {
    (void)path; if (st) { st->st_mode = S_IFCHR; } return 0;
}

int _unlink(const char *path) {
    (void)path; errno = EIO; return -1;
}

int _gettimeofday(struct timeval *tv, void *tz) {
    (void)tz; if (tv) { tv->tv_sec = 0; tv->tv_usec = 0; } return 0;
}

clock_t _times(struct tms *buf) {
    if (buf) { buf->tms_utime = buf->tms_stime = buf->tms_cutime = buf->tms_cstime = 0; }
    return 0;
}
