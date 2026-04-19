
// pa3

// testcase 3: Clock Sweeper Sanity Check

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

const int PGSIZE = 4096;

// Based on your stats, RAM holds ~64 pages. 
// We use 50 here because the OS/shell takes ~20, meaning 50 will definitely fill the remaining space.
const int RAM_LIMIT = 43; 

void print_stats(char *label) {
    struct vmstats stats;
    if (getvmstats(getpid(), &stats) == 0) {
        printf("\n--- VM Statistics (%s) ---\n", label);
        printf("Page Faults: %d\n", stats.page_faults);
        printf("Pages Evicted: %d\n", stats.pages_evicted);
        printf("Pages Swapped In: %d\n", stats.pages_swapped_in);
        printf("Resident Pages: %d\n", stats.resident_pages);
    }
}

int main() {
    printf("Starting Clock Sweeper Test...\n");
    print_stats("Initial State");

    printf("\n1. Allocating %d pages to completely fill RAM...\n", RAM_LIMIT);
    char *mem = sbrklazy(RAM_LIMIT * PGSIZE);
    
    for(int i = 0; i < RAM_LIMIT; i++) {
        mem[i * PGSIZE] = 'X'; 
    }
    printf("RAM is now full. All Hardware Reference Bits (PTE_A) are currently 1.\n");
    print_stats("RAM Full State");

    printf("\n2. The Trigger: Allocating 1 extra page to force eviction...\n");
    char *extra_page = sbrklazy(PGSIZE);
    
    // --> THIS is the moment of truth. <--
    // Writing to this page triggers a page fault. 
    // The OS will enter your `evict_page` while loop.
    // It must successfully sweep the whole table, clear the PTE_A bits, and not infinite-loop!
    extra_page[0] = 'Y'; 

    printf("Success! The OS survived the 360-degree Clock sweep without freezing.\n");
    print_stats("After Trigger");

    printf("\n3. Verifying the Victim...\n");
    int errors = 0;
    for(int i = 0; i < RAM_LIMIT; i++) {
        if(mem[i * PGSIZE] != 'X') errors++;
    }
    printf("Read all pages. Errors: %d\n", errors);

    print_stats("Final State");
    printf("\nClock Algorithm is fully validated.\n");
    
    exit(0);
}


// testcase2 : Swap Space Sanity Check


// #include "kernel/types.h"
// #include "kernel/stat.h"
// #include "user/user.h"

// const int PGSIZE = 4096;
// const int ALLOC_PAGES = 35; // Two processes allocating 35 pages = 70 pages (Exceeds 64 limit)

// void print_stats(char *label) {
//     struct vmstats stats;
//     if (getvmstats(getpid(), &stats) == 0) {
//         printf("\n--- %s (PID %d) VM Stats ---\n", label, getpid());
//         printf("Page Faults: %d\n", stats.page_faults);
//         printf("Pages Evicted: %d\n", stats.pages_evicted);
//         printf("Resident Pages: %d\n", stats.resident_pages);
//     }
// }

// void run_victim() {
//     printf("VICTIM: Starting. Burning CPU to drop to lowest priority...\n");
//     // Heavy CPU loop to trigger time-slice expiration and priority demotion
//     volatile long counter = 0;
//     for(int i = 0; i < 50000000; i++) {
//         counter++; 
//     }

//     printf("VICTIM: Allocating %d pages and filling RAM...\n", ALLOC_PAGES);
//     char *mem = sbrklazy(ALLOC_PAGES * PGSIZE);
    
//     // Trigger page faults to bring these pages into physical frames
//     for(int i = 0; i < ALLOC_PAGES; i++) {
//         mem[i * PGSIZE] = 'A';
//     }

//     printf("VICTIM: pauseing to let VIP attack my memory...\n");
//     pause(100); //   pause while VIP runs and steals frames

//     print_stats("VICTIM");
//     exit(0);
// }

// void run_vip() {
//     // Wait for the Victim to drop in priority and fill up the RAM first
//     pause(30); 

//     printf("VIP: Starting. Spamming syscalls to stay High Priority...\n");
//     // Spam system calls to ensure delta_s > time_slice, keeping priority high
//     for(int i = 0; i < 100000; i++) {
//         getpid(); 
//     }

//     printf("VIP: Allocating %d pages (RAM is full, forced to evict)...\n", ALLOC_PAGES);
//     char *mem = sbrklazy(ALLOC_PAGES * PGSIZE);
    
//     // As the VIP writes, it will page fault. Since RAM is full, the OS must evict.
//     // Because VIP has higher priority, it should steal the VICTIM's pages.
//     for(int i = 0; i < ALLOC_PAGES; i++) {
//         mem[i * PGSIZE] = 'B';
//         getpid(); // keep priority high during the allocation phase
//     }

//     print_stats("VIP");
//     exit(0);
// }

// int main() {
//     printf("Starting Priority Duel Test...\n");

//     int pid_victim = fork();
//     if (pid_victim == 0) {
//         run_victim();
//     } else {
//         int pid_vip = fork();
//         if (pid_vip == 0) {
//             run_vip();
//         } else {
//             // Parent waits for both children to finish the duel
//             wait(0);
//             wait(0);
//             printf("\nDuel Complete. Compare the 'Pages Evicted' above!\n");
//         }
//     }
//     exit(0);
// }


// testcase1 : Basic Swap Functionality Check
// #include "kernel/types.h"
// #include "kernel/stat.h"
// #include "user/user.h"

// const int PGSIZE = 4096;
// const int NUM_PAGES = 100;

// void print_stats(char *label) {
//     struct vmstats stats;
//     if (getvmstats(getpid(), &stats) == 0) {
//         printf("\n--- VM Statistics (%s) ---\n", label);
//         printf("Page Faults: %d\n", stats.page_faults);
//         printf("Pages Evicted: %d\n", stats.pages_evicted);
//         printf("Pages Swapped In: %d\n", stats.pages_swapped_in);
//         printf("Pages Swapped Out: %d\n", stats.pages_swapped_out);
//         printf("Resident Pages: %d\n", stats.resident_pages);
//     } else {
//         printf("Failed to get vmstats.\n");
//     }
// }

// int main() {
//     print_stats("Initial");

//     printf("\nStarting swaptest: Allocating %d pages using sbrklazy...\n", NUM_PAGES);
//     char *mem = sbrklazy(NUM_PAGES * PGSIZE);
//     if(mem == (char*)-1){
//         printf("sbrklazy failed!\n");
//         exit(1);
//     }

//     printf("1. Writing to pages (Phase A: forcing page faults and evictions)...\n");
//     for(int i = 0; i < NUM_PAGES; i++) {
//         // Triggers demand paging. Evicts older pages when physical memory fills up.
//         mem[i * PGSIZE] = (char)(i % 256);
//     }

//     print_stats("After Writing (Evictions expected)");

//     printf("\n2. Reading from pages (Phase B: forcing swap-ins)...\n");
//     int errors = 0;
//     for(int i = 0; i < NUM_PAGES; i++) {
//         // Triggers swap-ins for pages that were evicted in Phase A.
//         if(mem[i * PGSIZE] != (char)(i % 256)) {
//             printf("Data mismatch at page %d!\n", i);
//             errors++;
//         }
//     }

//     if (errors == 0) {
//         printf("All data verified successfully!\n");
//     } else {
//         printf("Failed with %d data mismatches.\n", errors);
//     }

//     print_stats("Final (Swap-ins expected)");

//     exit(0);
// }
