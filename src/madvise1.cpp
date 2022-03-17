#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <string.h>

size_t touch(char* base, off_t size) {
    size = (size + 4095) & ~4095;
    size_t sum = 0;
    for (off_t i = 0; i < size; i += 4096) sum += base[i];
    return sum;
}

double ms(timespec t0, timespec t1) {
    return (t1.tv_sec - t0.tv_sec) * 1e3 + (t1.tv_nsec - t0.tv_nsec) / 1e6;
}

void print_mincore(void* base, size_t len) {
    size_t num_pages = (len + 4096-1)/4096;
    unsigned char* is_incore = (unsigned char*)calloc(1, num_pages + 1); // set to zero
    mincore(base, len, is_incore);
    bool same = true;
    unsigned char first = is_incore[0];
    for (size_t i = 1; i < num_pages; ++i) if (is_incore[i] != first) { same = false; break; }
    if (same) {
        printf("mincore = %c, all same\n", 0x30 + first);
    }
    else {
        for (size_t i = 0; i < num_pages; ++i) is_incore[i] += 0x30;
        printf("mincore = %s\n", is_incore);
    }
    free(is_incore);
}

void print_touch(char* base, off_t len) {
    print_mincore(base, len);
    timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    size_t sum = touch(base, len);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    printf("touch mem time = %f ms, sum = %zd\n--\n", ms(t0, t1), sum);
}

int main() {
    struct stat st;
    fstat(STDIN_FILENO, &st);
    if (posix_fadvise(STDIN_FILENO, 0, st.st_size, POSIX_FADV_DONTNEED)) {
        fprintf(stderr, "ERROR: posix_fadvise(STDIN, 0, %zd, DONTNEED) = %m\n", size_t(st.st_size));
        return 1;
    }
    char* base = (char*)mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, STDIN_FILENO, 0);
    if (MAP_FAILED == base) {
        fprintf(stderr, "ERROR: mmap(STDIN, size=%zd) = %m\n", size_t(st.st_size));
        return 1;
    }
    print_touch(base, st.st_size);
    if (posix_fadvise(STDIN_FILENO, 0, st.st_size, POSIX_FADV_DONTNEED)) {
        fprintf(stderr, "ERROR: posix_fadvise(STDIN, 0, %zd, DONTNEED) = %m\n", size_t(st.st_size));
        return 1;
    }
    if (madvise(base, st.st_size, MADV_DONTNEED) < 0) {
        fprintf(stderr, "ERROR: madvise(STDIN, %zd, WILLNEED) = %m\n", size_t(st.st_size));
        return 1;
    }
    print_mincore(base, st.st_size);
    printf("--\n");
    timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    if (madvise(base, st.st_size, MADV_WILLNEED) < 0) {
        fprintf(stderr, "ERROR: madvise(STDIN, %zd, WILLNEED) = %m\n", size_t(st.st_size));
        return 1;
    }
    clock_gettime(CLOCK_MONOTONIC, &t1);
    printf("madvise(WILLNEED) time = %f ms\n", ms(t0, t1));
    print_touch(base, st.st_size);

    return 0;
}
