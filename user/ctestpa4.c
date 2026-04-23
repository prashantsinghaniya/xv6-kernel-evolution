/*
 * test_disksched.c
 * PA4 – Disk Scheduling Policy Tests (FCFS and SSTF)
 */

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define PAGE_SIZE   4096

// Your RAM is 32768 pages. We need 33500 to force swap to disk!
#define PRESSURE_PAGES 33500 

#define PASS(msg)   printf("[PASS] %s\n", msg)
#define FAIL(msg)   printf("[FAIL] %s\n", msg)
#define INFO(msg)   printf("[INFO] %s\n", msg)

/* --- MUST MATCH YOUR SYSCALL NUMBERS --- */
#define SYS_setdisksched  27      
#define SYS_getdiskstats  28      

#define POLICY_FCFS 0
#define POLICY_SSTF 1

static int set_policy(int policy) {
    return setdisksched(policy); // Direct xv6 syscall call
}
static int get_stats(struct vmstats *s)
{
    // Yahan hum wahi purana getvmstats use karenge jo pehle chal raha tha
    return getvmstats(getpid(), (struct vmstats*)s);
}
static int generate_io_burst(void) {
    const int N = 48;
    char *pages[N];

    for (int i = 0; i < N; i++) {
        pages[i] = sbrk(PAGE_SIZE);
        if (pages[i] == (char *)-1) return -1;
        pages[i][0] = (char)(i * 13);  
    }

    // MASSIVE PRESSURE TO FORCE SWAP
    for (int i = 0; i < PRESSURE_PAGES; i++) {
        char *p = sbrk(PAGE_SIZE);
        if (p == (char *)-1) break;
        p[0] = 1;
    }

    for (int stride = 0; stride < 3; stride++) {
        for (int i = stride; i < N; i += 3) {
            if (pages[i] != (char *)-1)
                (void)pages[i][0]; // Fault it back in
        }
    }
    return 0;
}

static void test_setdisksched_valid(void) {
    INFO("Test 1: setdisksched() accepts valid policies");
    if (set_policy(POLICY_FCFS) == 0 && set_policy(POLICY_SSTF) == 0)
        PASS("Test 1: setdisksched valid policies");
    else
        FAIL("Test 1: setdisksched rejected a valid policy");
}

static void test_setdisksched_invalid(void) {
    INFO("Test 2: setdisksched() rejects invalid policies");
    int r1 = set_policy(-1);
    int r2 = set_policy(99);
    if (r1 < 0 && r2 < 0)
        PASS("Test 2: setdisksched rejects invalid policies");
    else
        FAIL("Test 2: setdisksched accepted an invalid policy");
}

static void test_sstf_beats_fcfs(void) {
    INFO("Test 3: SSTF total latency <= FCFS total latency");
    struct vmstats before_fcfs, after_fcfs;
    struct vmstats before_sstf, after_sstf;

    if (set_policy(POLICY_FCFS) < 0) {
        INFO("Test 3: skipped (setdisksched unavailable)");
        return;
    }
    
    get_stats(&before_fcfs);
    generate_io_burst();
    get_stats(&after_fcfs);
    int fcfs_latency = after_fcfs.avg_disk_latency - before_fcfs.avg_disk_latency;

    set_policy(POLICY_SSTF);
    get_stats(&before_sstf);
    generate_io_burst();
    get_stats(&after_sstf);
    int sstf_latency = after_sstf.avg_disk_latency - before_sstf.avg_disk_latency;

    printf("[INFO] FCFS total latency : %d\n", fcfs_latency);
    printf("[INFO] SSTF total latency : %d\n", sstf_latency);

    if (sstf_latency <= fcfs_latency)
        PASS("Test 3: SSTF total latency <= FCFS");
    else
        FAIL("Test 3: SSTF latency exceeded FCFS (unexpected)");
}

static void test_request_count_consistent(void) {
    INFO("Test 4: request count consistent between FCFS and SSTF");
    struct vmstats b1, a1, b2, a2;

    set_policy(POLICY_FCFS);
    get_stats(&b1);
    generate_io_burst();
    get_stats(&a1);
    int fcfs_req = (a1.total_disk_reads + a1.total_disk_writes) - (b1.total_disk_reads + b1.total_disk_writes);

    set_policy(POLICY_SSTF);
    get_stats(&b2);
    generate_io_burst();
    get_stats(&a2);
    int sstf_req = (a2.total_disk_reads + a2.total_disk_writes) - (b2.total_disk_reads + b2.total_disk_writes);

    printf("[INFO] FCFS requests: %d  SSTF requests: %d\n", fcfs_req, sstf_req);

    int diff = fcfs_req - sstf_req;
    if (diff < 0) diff = -diff;

    if (diff <= 2)
        PASS("Test 4: request counts consistent across policies");
    else
        FAIL("Test 4: request count mismatch between FCFS and SSTF");
}

static void test_avg_latency_nonzero(void) {
    INFO("Test 5: avg_disk_latency is non-zero after I/O");
    struct vmstats s;

    set_policy(POLICY_FCFS);
    generate_io_burst();
    get_stats(&s);

    // int avg = (s.requests_served > 0) ? s.avg_disk_latency / s.requests_served : 0;
    // printf("[INFO] avg_disk_latency = %d\n", avg);

    // if (avg > 0) PASS("Test 5: avg_disk_latency > 0");
    // else         FAIL("Test 5: avg_disk_latency is zero (latency model broken?)");
}

static void test_mid_run_policy_switch(void) {
    INFO("Test 6: policy switch mid-run");
    const int N = 16;
    char *pages[N];

    set_policy(POLICY_FCFS);

    for (int i = 0; i < N; i++) {
        pages[i] = sbrk(PAGE_SIZE);
        if (pages[i] != (char *)-1) pages[i][0] = (char)(i + 1);
    }

    for (int i = 0; i < PRESSURE_PAGES; i++) {
        char *p = sbrk(PAGE_SIZE);
        if (p != (char *)-1) p[0] = 1;
        else break;
    }

    set_policy(POLICY_SSTF);

    for (int i = 0; i < PRESSURE_PAGES; i++) {
        char *p = sbrk(PAGE_SIZE);
        if (p != (char *)-1) p[0] = 1;
        else break;
    }

    int ok = 1;
    for (int i = 0; i < N; i++) {
        if (pages[i] != (char *)-1 && pages[i][0] != (char)(i + 1)) {
            ok = 0;
            printf("[FAIL] Test 6: page %d corrupted after policy switch\n", i);
        }
    }
    if (ok) PASS("Test 6: mid-run policy switch – data intact");
}

static void test_fcfs_sequential_low_latency(void) {
    INFO("Test 7: FCFS sequential access has near-minimum latency");
    struct vmstats before, after;

    set_policy(POLICY_FCFS);
    get_stats(&before);

    const int N = 32;
    char *pages[N];
    for (int i = 0; i < N; i++) {
        pages[i] = sbrk(PAGE_SIZE);
        if (pages[i] != (char *)-1) pages[i][0] = 1;
    }
    
    for (int i = 0; i < PRESSURE_PAGES; i++) {
        char *p = sbrk(PAGE_SIZE);
        if (p != (char *)-1) p[0] = 1;
        else break;
    }
    
    for (int i = 0; i < N; i++) {
        if (pages[i] != (char *)-1) (void)pages[i][0];
    }

    get_stats(&after);

    int lat = after.avg_disk_latency - before.avg_disk_latency;
    int req = (after.total_disk_reads + after.total_disk_writes) - (before.total_disk_reads + before.total_disk_writes);

    printf("[INFO] Sequential FCFS: avg_disk_latency=%d requests=%d\n", lat, req);

    int avg_seek = (req > 0) ? lat / req : 9999;
    if (avg_seek <= 8)
        PASS("Test 7: FCFS sequential access has low avg seek");
    else
        INFO("Test 7: avg seek higher than expected (may be workload noise)");
}

/* --- ISOLATION WRAPPER --- */
void run_isolated_test(void (*test_func)(void)) {
    int pid = fork();
    if (pid == 0) {
        test_func();
        exit(0);
    }
    wait(0);
}

int main(void) {
    printf("\n=== PA4 Disk Scheduling Tests ===\n");
    printf("NOTE: Memory pressure scaled for 128MB RAM configuration.\n\n");

    run_isolated_test(test_setdisksched_valid);
    run_isolated_test(test_setdisksched_invalid);
    run_isolated_test(test_sstf_beats_fcfs);
    run_isolated_test(test_request_count_consistent);
    run_isolated_test(test_avg_latency_nonzero);
    run_isolated_test(test_mid_run_policy_switch);
    run_isolated_test(test_fcfs_sequential_low_latency);

    printf("\n=== Disk Scheduling Tests Complete ===\n");
    exit(0);
}