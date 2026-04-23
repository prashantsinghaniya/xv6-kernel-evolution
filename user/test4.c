// #include "kernel/types.h"
// #include "kernel/stat.h"
// #include "user/user.h"

// #define PGSIZE 4096
// #define NUM_PAGES 100 // 100 pages = 400 KB (This will force evictions since RAM is 256 KB)

// int main(int argc, char *argv[]) {
//     printf("Starting Disk-Backed Swap Test...\n");

//     // 1. Allocate a large chunk of memory
//     char *mem = (char *)sbrklazy(NUM_PAGES * PGSIZE);
//     if(mem == (char*)-1){
//         printf("Error: sbrklazy failed to allocate memory.\n");
//         exit(1);
//     }
//     printf("Allocated %d pages (%d bytes).\n", NUM_PAGES, NUM_PAGES * PGSIZE);

//     // 2. Write to memory (Forces allocation and eventually swap-out)
//     printf("Writing data to memory (forcing swap-outs)...\n");
//     for(int i = 0; i < NUM_PAGES; i++) {
//         // Write a specific marker (like a signature) to the start of each page
//         mem[i * PGSIZE] = (char)(i % 256);
//     }

//     // 3. Read back from memory (Forces swap-in)
//     printf("Reading data back (forcing swap-ins)...\n");
//     int errors = 0;
//     for(int i = 0; i < NUM_PAGES; i++) {
//         char expected = (char)(i % 256);
//         char actual = mem[i * PGSIZE];
        
//         if(actual != expected) {
//             printf("DATA CORRUPTION at page %d: Expected %d, got %d\n", i, expected, actual);
//             errors++;
//         }
//     }

//     // 4. Print VM Stats to verify kernel recorded the activity
//     // (Assuming your sys_getvmstats works from PA3)
//     struct vmstats stats;
//     if(getvmstats(getpid(), &stats) == 0) {
//         printf("\n--- VM Statistics ---\n");
//         printf("Page Faults: %d\n", stats.page_faults);
//         printf("Pages Evicted: %d\n", stats.pages_evicted);
//         printf("Pages Swapped In: %d\n", stats.pages_swapped_in);
//         printf("Pages Swapped Out: %d\n", stats.pages_swapped_out);
//     }

//     if(errors == 0) {
//         printf("\n>>> SWAP TEST PASSED! All data matched perfectly. <<<\n");
//     } else {
//         printf("\n>>> SWAP TEST FAILED with %d errors. <<<\n", errors);
//     }

//     exit(0);
// }



// #include "kernel/types.h"
// #include "kernel/stat.h"
// #include "user/user.h"

// #define PGSIZE 4096
// #define NUM_PAGES 100 // Chota test taki console par padhna aasan ho

// void run_access_pattern() {
//     char *mem = (char *)sbrk(NUM_PAGES * PGSIZE);
    
//     // Write data to force swap out
//     for(int i = 0; i < NUM_PAGES; i++) {
//         mem[i * PGSIZE] = 'A'; 
//     }

//     // Now we access pages out of order: 0, 9, 1, 8, 2, 7...
//     // This creates a zig-zag disk request pattern
//     printf("Triggering Zig-Zag Page Faults...\n");
//     int left = 0;
//     int right = NUM_PAGES - 1;
    
//     while(left <= right) {
//         char val;
//         val = mem[left * PGSIZE]; // Access left
//         left++;
//         if(left <= right) {
//             val = mem[right * PGSIZE]; // Access right
//             right--;
//         }
//         printf("%d ", val); // Just to prevent compiler optimizations   
//     }
//     sbrklazy(-(NUM_PAGES * PGSIZE)); // Clean up
// }

// int main(void) {
//     printf("=== Starting Disk Scheduling Test ===\n");

//     // TEST 1: FCFS
//     printf("\n--- Setting Policy to FCFS (0) ---\n");
//     setdisksched(0);
//     run_access_pattern();

//     // TEST 2: SSTF
//     printf("\n--- Setting Policy to SSTF (1) ---\n");
//     setdisksched(1);
//     run_access_pattern();

//     printf("\n=== Test Complete ===\n");
//     exit(0);
// }


// #include "kernel/types.h"
// #include "kernel/stat.h"
// #include "user/user.h"

// #define PGSIZE 4096
// #define NUM_PAGES 100 // Force massive swapping

// void run_raid_test(int raid_level) {
//     printf("\n==================================\n");
//     printf("=== Testing RAID Level %d ========\n", raid_level);
//     printf("==================================\n");
    
//     if (setraid(raid_level) < 0) {
//         printf("Error: setraid system call failed. Did you add it?\n");
//         return;
//     }

//     // Allocate memory to trigger lazy allocation
//     char *mem = (char *)sbrk(NUM_PAGES * PGSIZE);
//     if(mem == (char*)-1) {
//         printf("Error: sbrk failed.\n");
//         return;
//     }

//     printf("1. Writing %d pages to force swap-out (RAID Writes)...\n", NUM_PAGES);
//     for(int i = 0; i < NUM_PAGES; i++) {
//         // Write a specific pattern
//         mem[i * PGSIZE] = (char)((i * 3) % 256); 
//     }

//     printf("2. Reading %d pages to force swap-in (RAID Reads)...\n", NUM_PAGES);
//     int errors = 0;
//     for(int i = 0; i < NUM_PAGES; i++) {
//         char expected = (char)((i * 3) % 256);
//         char actual = mem[i * PGSIZE];
//         if(actual != expected) {
//             errors++;
//         }
//     }

//     if(errors == 0) {
//         printf(">>> RAID %d TEST PASSED: Zero Data Corruption! <<<\n", raid_level);
//     } else {
//         printf(">>> RAID %d TEST FAILED: %d errors found! <<<\n", raid_level, errors);
//     }

//     // Free memory so the next test starts fresh
//     sbrk(-(NUM_PAGES * PGSIZE)); 
// }

// int main(void) {
//     printf("Starting Complete RAID Storage Tests...\n");
    
//     // Set scheduler to FCFS so our logs are easy to read in order
//     setdisksched(0);

//     run_raid_test(0); // Test Striping
//     run_raid_test(1); // Test Mirroring
//     run_raid_test(5); // Test Parity

//     printf("\nAll RAID Storage Tests Completed.\n");
//     // --- NAYA CODE (Fix for the init crash) ---
//     printf("Restoring default RAID mapping for OS stability...\n");
//     setraid(0); 
//     // ------------------------------------------
//     exit(0);
// }


#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define PGSIZE 4096
#define NUM_PAGES 40 // Each child asks for 40 pages. 3 children = 120 pages total!

// Must match your kernel's structure
struct vmstats {
  int page_faults;
  int pages_evicted;
  int pages_swapped_in;
  int pages_swapped_out;
  int resident_pages;
  uint total_disk_reads;
  uint total_disk_writes;
  uint avg_disk_latency;
};

void print_stats(char *stage) {
    struct vmstats st;
    if(getvmstats(getpid(), &st) == 0) {
        printf("\n--- %s [PID %d] ---\n", stage, getpid());
        printf("Page Faults: %d | Evicted: %d\n", st.page_faults, st.pages_evicted);
        printf("Disk Reads: %d | Disk Writes: %d\n", st.total_disk_reads, st.total_disk_writes);
        printf("Avg Disk Latency: %d ticks\n", st.avg_disk_latency);
        printf("--------------------------------\n");
    }
}

void child_workload(int child_num) {
    printf("Child %d (PID %d) starting heavy workload...\n", child_num, getpid());

    // Allocate memory
    char *mem = (char *)sbrklazy(NUM_PAGES * PGSIZE);
    if(mem == (char*)-1){
        printf("Child %d: sbrk failed!\n", child_num);
        exit(1);
    }

    // Write Phase (Forces Swap-Out + RAID Writes)
    for(int i = 0; i < NUM_PAGES; i++) {
        mem[i * PGSIZE] = (char)(child_num * 10); // Unique data per child
    }

    // Read & Verify Phase (Forces Swap-In + RAID Reads)
    int errors = 0;
    for(int i = 0; i < NUM_PAGES; i++) {
        char expected = (char)(child_num * 10);
        char actual = mem[i * PGSIZE];
        if(actual != expected) {
            errors++;
        }
    }

    if(errors == 0) {
        printf(">>> Child %d (PID %d) PASSED: Zero Data Corruption! <<<\n", child_num, getpid());
    } else {
        printf(">>> Child %d (PID %d) FAILED with %d errors! <<<\n", child_num, getpid(), errors);
    }

    print_stats("Final Stats Before Exit");
    exit(0);
}

int main(void) {
    printf("===========================================\n");
    printf("=== CS3523 PA4 ULTIMATE SYSTEM CHECKUP ===\n");
    printf("===========================================\n\n");

    // 1. Setup the most complex environment
    setraid(5);       // Enable Parity Multi-Disk Writes
    setdisksched(1);  // Enable SSTF Queue Sorting
    
    // Optional: If you have a priority system call from PA2, set parent to highest
    // setpriority(getpid(), 10); 

    print_stats("Parent Initial Stats");

    // 2. Launch Concurrent Children
    printf("\n[Spawning 3 Concurrent Processes to flood the Disk Queue...]\n");
    for(int i = 1; i <= 3; i++) {
        int pid = fork();
        if(pid == 0) {
            // Child Process
            // Optional: setpriority(getpid(), i); // Give different priorities
            child_workload(i);
        }
    }

    // 3. Parent waits for all children
    for(int i = 0; i < 3; i++) {
        wait(0);
    }

    printf("\n===========================================\n");
    printf("=== ALL TESTS COMPLETED SUCCESSFULLY! ===\n");
    printf("==========================================\n");
    // Restore defaults to prevent sh trap bug
    // setraid(0);
    // setdisksched(0);
    
    exit(0);
}
// #include "kernel/types.h"
// #include "kernel/stat.h"
// #include "user/user.h"

// #define PGSIZE 4096
// #define NUM_PAGES 40 // Each child asks for 40 pages. 3 children = 120 pages total!

// // Must match your kernel's structure
// struct vmstats {
//   int page_faults;
//   int pages_evicted;
//   int pages_swapped_in;
//   int pages_swapped_out;
//   int resident_pages;
//   uint total_disk_reads;
//   uint total_disk_writes;
//   uint avg_disk_latency;
// };

// void print_stats(char *stage) {
//     struct vmstats st;
//     if(getvmstats(getpid(), &st) == 0) {
//         printf("\n--- %s [PID %d] ---\n", stage, getpid());
//         printf("Page Faults: %d | Evicted: %d\n", st.page_faults, st.pages_evicted);
//         printf("Disk Reads: %d | Disk Writes: %d\n", st.total_disk_reads, st.total_disk_writes);
//         printf("Avg Disk Latency: %d ticks\n", st.avg_disk_latency);
//         printf("--------------------------------\n");
//     }
// }

// void child_workload(int child_num) {
//     printf("Child %d (PID %d) starting heavy workload...\n", child_num, getpid());

//     // Allocate memory
//     char *mem = (char *)sbrklazy(NUM_PAGES * PGSIZE);
//     if(mem == (char*)-1){
//         printf("Child %d: sbrk failed!\n", child_num);
//         exit(1);
//     }

//     // Write Phase (Forces Swap-Out + RAID Writes)
//     for(int i = 0; i < NUM_PAGES; i++) {
//         mem[i * PGSIZE] = (char)(child_num * 10); // Unique data per child
//     }

//     // Read & Verify Phase (Forces Swap-In + RAID Reads)
//     int errors = 0;
//     for(int i = 0; i < NUM_PAGES; i++) {
//         char expected = (char)(child_num * 10);
//         char actual = mem[i * PGSIZE];
//         if(actual != expected) {
//             errors++;
//         }
//     }

//     if(errors == 0) {
//         printf(">>> Child %d (PID %d) PASSED: Zero Data Corruption! <<<\n", child_num, getpid());
//     } else {
//         printf(">>> Child %d (PID %d) FAILED with %d errors! <<<\n", child_num, getpid(), errors);
//     }

//     print_stats("Final Stats Before Exit");
//     exit(0);
// }

// int main(void) {
//     printf("===========================================\n");
//     printf("=== CS3523 PA4 ULTIMATE SYSTEM CHECKUP ===\n");
//     printf("===========================================\n\n");

//     // 1. Setup the most complex environment
//     setraid(5);       // Enable Parity Multi-Disk Writes
//     setdisksched(1);  // Enable SSTF Queue Sorting
    
//     // Optional: If you have a priority system call from PA2, set parent to highest
//     // setpriority(getpid(), 10); 

//     print_stats("Parent Initial Stats");

//     // 2. Launch Concurrent Children
//     printf("\n[Spawning 3 Concurrent Processes to flood the Disk Queue...]\n");
//     for(int i = 1; i <= 3; i++) {
//         int pid = fork();
//         if(pid == 0) {
//             // Child Process
//             // Optional: setpriority(getpid(), i); // Give different priorities
//             child_workload(i);
//         }
//     }

//     // 3. Parent waits for all children
//     for(int i = 0; i < 3; i++) {
//         wait(0);
//     }

//     printf("\n===========================================\n");
//     printf("=== ALL TESTS COMPLETED SUCCESSFULLY! ===\n");
//     printf("==========================================\n");
//     // Restore defaults to prevent sh trap bug
//     // setraid(0);
//     // setdisksched(0);
    
//     exit(0);
// }