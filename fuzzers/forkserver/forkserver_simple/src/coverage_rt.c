#include <stdint.h>
#include <stdlib.h>
#include <sys/shm.h>

#define MAP_SIZE 65536

static uint8_t *__coverage_map;
static __thread uint32_t __prev_loc;

__attribute__((constructor))
static void __coverage_shm_init(void) {
    const char *id_str = getenv("__AFL_SHM_ID");
    if (!id_str) return;

    // adresse fixe via AFL_MAP_ADDR si définie
    void *hint = NULL;
    const char *addr_str = getenv("AFL_MAP_ADDR");
    if (addr_str) hint = (void *)strtoull(addr_str, NULL, 0);

    void *map = shmat(atoi(id_str), hint, 0);
    if (map == (void *)-1) return;
    __coverage_map = (uint8_t *)map;
}

void __sanitizer_cov_trace_pc_guard_init(uint32_t *start, uint32_t *stop) {
    static uint32_t N;
    if (start == stop || *start) return;
    for (uint32_t *x = start; x < stop; x++)
        *x = ++N;
}

void __sanitizer_cov_trace_pc_guard(uint32_t *guard) {
    if (!__coverage_map) return;
    uint32_t cur_loc = *guard;
    __coverage_map[(cur_loc ^ __prev_loc) % MAP_SIZE]++;
    __prev_loc = cur_loc >> 1;
}
