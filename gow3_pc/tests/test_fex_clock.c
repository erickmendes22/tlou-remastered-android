/* Exercise the emulated CPU contract with a non-GHz counter and absolute epoch. */
#define _GNU_SOURCE
#define GUEST_CPU_TEST_FEX
#include <assert.h>
#include <stdio.h>
#include "../src/runtime_kernel.c"
int32_t *runtime_errno(void) { static int32_t value; return &value; }
void runtime_report(void) {}
uintptr_t runtime_lookup(const RuntimeExport *table,size_t count,const char *name) {
    (void)table; (void)count; (void)name; return 0;
}
uint64_t guest_cpu_tsc_frequency(void) { return 1228800000; }
uint64_t guest_cpu_tsc(void) {
    struct timespec t; clock_gettime(CLOCK_MONOTONIC,&t);
    return 9000000000000ULL+(uint64_t)(((__uint128_t)t.tv_sec*1000000000+t.tv_nsec)*guest_cpu_tsc_frequency()/1000000000);
}
int main(void) {
    assert(tsc_frequency()==1228800000);
    assert(process_time_frequency()==tsc_frequency());
    uint64_t first=guest_cpu_tsc(), kernel=read_tsc(), last=guest_cpu_tsc();
    assert(kernel>=first && kernel<=last);
    assert(process_time_counter()<tsc_frequency()*60); /* process-relative, not absolute */
    uint64_t tc=read_tsc(), pc=process_time_counter(), us=process_time();
    sleep_ns(50000000);
    double a=(double)(read_tsc()-tc)/tsc_frequency();
    double b=(double)(process_time_counter()-pc)/process_time_frequency();
    double c=(double)(process_time()-us)/1000000;
    assert(a>0.04 && a<0.5);
    assert(b>0.04 && b<0.5);
    assert(c>0.04 && c<0.5);
    assert(a-b<0.01 && b-a<0.01 && b-c<0.01 && c-b<0.01);
    puts("fex-clock-test ok");
}
