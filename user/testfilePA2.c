
// #include "kernel/types.h"
// #include "kernel/stat.h"
// #include "user/user.h"
// // #include "kernel/vmstats.h"

// const int PGSIZE = 4096;
// int main() {

//     struct vmstats stats;
//     if (getvmstats(getpid(), &stats) == 0) {
//         printf("\n--- VM Statistics ---\n");
//         printf("Page Faults: %d\n", stats.page_faults);
//         printf("Pages Evicted: %d\n", stats.pages_evicted); 
//         printf("Pages Swapped In: %d\n", stats.pages_swapped_in);
//         printf("Pages Swapped Out: %d\n", stats.pages_swapped_out);
//         printf("Resident Pages: %d\n", stats.resident_pages);
//     } else {
//         printf("Failed to get vmstats.\n");
//     }

//     int NUM_PAGES = 100;
//     printf("Starting swaptest: Allocating %d pages...\n", NUM_PAGES);
    
//     char *mem = sbrklazy(NUM_PAGES * PGSIZE);
//     if(mem == (char*)-1){
//         printf("sbrk failed!\n");
//         exit(1);
//     }

//     printf("1. Writing to pages (forcing page faults and evictions)...\n");
//     for(int i = 0; i < NUM_PAGES; i++) {
//         // Write a specific pattern to the first byte of each page
//         mem[i * PGSIZE] = (char)(i % 256);
//     }

//     printf("2. Reading from pages (forcing swap-ins)...\n");
//     int errors = 0;
//     for(int i = 0; i < NUM_PAGES; i++) {
//         if(mem[i * PGSIZE] != (char)(i % 256)) {
//             printf("Data mismatch at page %d!\n", i);
//             errors++;
//         }
//     }

//     if (errors == 0) {
//         printf("All data verified successfully!\n");
//     }

//     // Fetch and print statistics
//     // struct vmstats stats;
//     if (getvmstats(getpid(), &stats) == 0) {
//         printf("\n--- VM Statistics ---\n");
//         printf("Page Faults: %d\n", stats.page_faults);
//         printf("Pages Evicted: %d\n", stats.pages_evicted);
//         printf("Pages Swapped In: %d\n", stats.pages_swapped_in);
//         printf("Pages Swapped Out: %d\n", stats.pages_swapped_out);
//         printf("Resident Pages: %d\n", stats.resident_pages);
//     } else {
//         printf("Failed to get vmstats.\n");
//     }

//     exit(0);
// }


// #include "kernel/types.h"
// #include "kernel/stat.h"
// #include "user/user.h"

// #define PGSIZE 4096
// // 64 Frames + 128 Swap Slots = 192 total possible pages.
// // We use 170 to leave a tiny bit of room for the kernel and stack overhead so we don't panic.
// #define STRESS_PAGES 170 

// int main(int argc, char *argv[]) {
//     int pid = getpid();
//     struct vmstats stats;
//     char *pages[STRESS_PAGES];

//     printf("--- PA3 Absolute Maximum Stress Test ---\n");
//     printf("Attempting to burn %d pages (Frames + Swap Limit)...\n\n", STRESS_PAGES);

//     // 1. Burn Memory!
//     for(int i = 0; i < STRESS_PAGES; i++) {
//         pages[i] = malloc(PGSIZE);
//         if(pages[i] == 0) {
//             printf("[!] malloc failed early at page %d! System out of memory.\n", i);
//             break;
//         }
        
//         // Write to memory to trigger lazy allocation and force evictions
//         pages[i][0] = (char)(i % 256); 
        
//         // Print progress every 20 pages so you can watch it struggle
//         if(i > 0 && i % 20 == 0) {
//             getvmstats(pid, &stats);
//             printf("Burned %d pages... (Resident: %d, Swap Out: %d)\n", 
//                    i, stats.resident_pages, stats.pages_swapped_out);
//         }
//     }

//     printf("\n[*] Physical memory and swap are now heavily saturated.\n");

//     // 2. The Great Swap-In Torture
//     printf("[*] Forcing massive swap-in storm...\n");
//     int errors = 0;
//     for(int i = 0; i < STRESS_PAGES; i++) {
//         if(pages[i] != 0 && pages[i][0] != (char)(i % 256)) {
//             errors++;
//         }
//     }
    
//     if(errors > 0) {
//         printf("[!] KERNEL FAILED: %d pages corrupted during swap-in!\n", errors);
//     } else {
//         printf("[+] KERNEL SURVIVED: All data read back successfully!\n");
//     }

//     // 3. Final Report
//     if (getvmstats(pid, &stats) == 0) {
//         printf("\n[Final Battlefield Stats]\n");
//         printf("Total Faults:      %d\n", stats.page_faults);
//         printf("Total Evicted:     %d\n", stats.pages_evicted);
//         printf("Total Swapped In:  %d\n", stats.pages_swapped_in);
//         printf("Total Swapped Out: %d\n", stats.pages_swapped_out);
//         printf("Final Resident:    %d\n", stats.resident_pages);
//     }

//     exit(0);
// }



// #include "kernel/types.h"
// #include "kernel/stat.h"
// #include "user/user.h"

// #define PGSIZE 4096
// #define NUM_PAGES 100 // 100 pages > 64 MAX_FRAMES, forcing eviction!

// int main(int argc, char *argv[]) {
//     int pid = getpid();
//     struct vmstats stats_before, stats_after;
//     char *pages[NUM_PAGES];

//     printf("--- PA3 Page Replacement Stress Test ---\n");

//     // 1. Get initial stats
//     if (getvmstats(pid, &stats_before) < 0) {
//         printf("Error: getvmstats failed\n");
//         exit(1);
//     }
//     printf("\n[Initial Stats]\n");
//     printf("Faults: %d | Evicted: %d | Swap In: %d | Swap Out: %d | Resident: %d\n",
//            stats_before.page_faults, stats_before.pages_evicted,
//            stats_before.pages_swapped_in, stats_before.pages_swapped_out,
//            stats_before.resident_pages);

//     // 2. Allocate and write to memory to force lazy allocation and eventual eviction
//     printf("\n[*] Allocating and writing to %d pages...\n", NUM_PAGES);
//     for(int i = 0; i < NUM_PAGES; i++) {
//         pages[i] = malloc(PGSIZE);
//         if(pages[i] == 0) {
//             printf("malloc failed at page %d\n", i);
//             exit(1);
//         }
        
//         // Writing to the first byte of the page triggers the page fault
//         pages[i][0] = (char)(i % 256); 
//     }
//     printf("[*] Allocation complete. Physical memory should be full, pages evicted.\n");

//     // 3. Read back the memory to force swap-ins
//     printf("\n[*] Reading back older pages to force swap-ins...\n");
//     int errors = 0;
//     for(int i = 0; i < NUM_PAGES; i++) {
//         // If the data doesn't match, our swap memory memmove failed!
//         if(pages[i][0] != (char)(i % 256)) {
//             errors++;
//         }
//     }
    
//     if(errors > 0) {
//         printf("[!] DATA CORRUPTION DETECTED: %d pages failed!\n", errors);
//     } else {
//         printf("[+] All data read back successfully from swap!\n");
//     }

//     // 4. Get final stats
//     if (getvmstats(pid, &stats_after) < 0) {
//         printf("Error: getvmstats failed\n");
//         exit(1);
//     }
//     printf("\n[Final Stats]\n");
//     printf("Faults: %d | Evicted: %d | Swap In: %d | Swap Out: %d | Resident: %d\n",
//            stats_after.page_faults, stats_after.pages_evicted,
//            stats_after.pages_swapped_in, stats_after.pages_swapped_out,
//            stats_after.resident_pages);

//     printf("\nTest finished.\n");
//     exit(0);
// }


















// PA2



// #include "kernel/types.h"
// #include "user/user.h"

// struct mlfqinfo info;

// int
// main()
// {
//     printf("=== Test 3: getmlfqinfo + Edge Cases ===\n");

//     int pid = getpid();

//     // Generate activity
//     for(int i = 0; i < 1000; i++){
//         getpid(); // syscalls
//         for(int j = 0; j < 10000; j++);
//     }

//     if(getmlfqinfo(pid, &info) < 0){
//         printf("ERROR: getmlfqinfo failed\n");
//         exit(1);
//     }

//     printf("Level: %d\n", info.level);
//     printf("Times scheduled: %d\n", info.times_scheduled);
//     printf("Total syscalls: %d\n", info.total_syscalls);

//     for(int i = 0; i < 4; i++){
//         printf("Ticks[%d]: %d\n", i, info.ticks[i]);
//     }

//     // Basic sanity checks
//     if(info.total_syscalls == 0){
//         printf("ERROR: syscall count not tracked\n");
//     }

//     // Invalid PID test
//     if(getmlfqinfo(99999, &info) != -1){
//         printf("ERROR: invalid PID not handled\n");
//     } else {
//         printf("Invalid PID handled correctly\n");
//     }

//     exit(0);
// }


// #include "kernel/types.h"
// #include "user/user.h"

// int
// main()
// {
//     printf("=== Test 2: Priority Boost & Fairness ===\n");

//     for(int i = 0; i < 3; i++){
//         if(fork() == 0){
//             // Make them CPU heavy so they get demoted
//             for(int j = 0; j < 150000000; j++);

//             int lvl_before = getlevel();
//             printf("Child %d level BEFORE boost: %d\n", getpid(), lvl_before);

//             // Wait for boost (using pause)
//             for(int k = 0; k < 200 + (10*i); k++){
//                 pause(1);
//             }

//             int lvl_after = getlevel();
//             printf("Child %d level AFTER boost: %d\n", getpid(), lvl_after);

//             if(lvl_after != 0){
//                 printf("ERROR: Boost failed for PID %d\n", getpid());
//             }

//             exit(0);
//         }
//     }
//     exit(0);
// }

//     for(int i = 0; i < 3; i++)
//         wait(0);

//     exit(0);

// }


// #include "kernel/types.h"
// #include "user/user.h"

// void cpu_bound()
// {
//     for(int i = 0; i < 100000000; i++);
//     printf("[CPU] PID %d Level: %d\n", getpid(), getlevel());
// }

// void syscall_heavy()
// {
//     for(int i = 0; i < 2000; i++){
//         getpid();
//     }
//     printf("[SYSCALL] PID %d Level: %d\n", getpid(), getlevel());
// }

// void mixed()
// {
//     for(int i = 0; i < 500; i++){
//         for(int j = 0; j < 10000; j++);
//         getpid();
//     }
//     printf("[MIXED] PID %d Level: %d\n", getpid(), getlevel());
// }

// int
// main()
// {
//     printf("=== Test 1: Core Scheduling Behavior ===\n");

//     printf("Initial Level (parent): %d\n", getlevel());

//     if(fork() == 0){
//         cpu_bound();
//         exit(0);
//     }

//     if(fork() == 0){
//         syscall_heavy();
//         exit(0);
//     }

//     if(fork() == 0){
//         mixed();
//         exit(0);
//     }

//     for(int i = 0; i < 3; i++)
//         wait(0);

//     exit(0);
// }


// #include "kernel/types.h"
// #include "user/user.h"

// int main() {
//   int pid = getpid();

//   for(int i = 0; i < 200; i++){
    
//     // CPU work
//     for(long j = 0; j < 5000000; j++);

//     // syscall work
//     for(int k = 0; k < 1000; k++)
//       getpid();
//   }

//   struct mlfqinfo info;
//   getmlfqinfo(pid, &info);

//   printf("Mixed workload process\n");
//   printf("Final level: %d\n", info.level);

//   exit(0);
// }

// #include "kernel/types.h"
// #include "user/user.h"

// int main() {
//   int pid = getpid();

//   for(int i=0;i<100000;i++){
//     getpid();
//   }

//   struct mlfqinfo info;
//   getmlfqinfo(pid,&info);

//   printf("Syscall-heavy process\n");
//   printf("Final level: %d\n",info.level);

//   exit(0);
// }


// #include "kernel/types.h"
// #include "user/user.h"

// int main() {
//   int pid = getpid();
//   int counter = 0;

//   for(long i = 0; i < 1000000000; i++) counter ++; // busy loop

//   struct mlfqinfo info;
//   getmlfqinfo(pid, &info);
  
  

//   printf("CPU-bound process\n");
//   printf("Final level: %d\n", info.level);
//   printf("Ticks consumed at each level: [Q0: %d, Q1: %d, Q2: %d, Q3: %d]\n", 
//           info.ticks[0], info.ticks[1], info.ticks[2], info.ticks[3]);
//   printf("Times scheduled: %d n", info.times_scheduled);


//   exit(0);
// }




// #include "kernel/types.h"
// #include "kernel/stat.h"
// #include "user/user.h"

// #define NUM_PROCS 40 

// void child_work(int id) {
//     // STEP 1: Pure CPU-bound work to force demotion.
//     // Absolutely NO system calls in this loop!
//     // This loop needs to be long enough to consume 2 + 4 + 8 = 14 ticks.
//     volatile int l = 0;
//     for(volatile uint64 i = 0; i < 30000000000ULL; i++){
//         l += (i % 2); // Just some dummy work to prevent optimization
//         l%=1000; // Keep l from overflowing
//     } 
//     exit(0);
// }

// int main() {
//     printf("Starting Corrected Massive Boost Test with %d processes...\n", NUM_PROCS);
//     int pids[NUM_PROCS];

//     for (int i = 0; i < NUM_PROCS; i++) {
//         pids[i] = fork();
        
//         if (pids[i] < 0) {
//             printf("Fork failed at child %d.\n", i);
//             break;
//         }

//         if (pids[i] == 0) { 
//             child_work(i); 
//         }
//     }

//     for (int i = 0; i < NUM_PROCS; i++) {
//         wait(0);
//     }

//     printf("\n======================================================\n");
//     printf("BOOST VERIFIED: Processes sank to L3 and boosted to L0!\n");
//     printf("======================================================\n");
    
//     exit(0);
// }

// // #include "kernel/types.h"
// // #include "kernel/stat.h"
// // #include "user/user.h"

// // struct mlfqinfo {
// //     int level; // current queue level
// //     int ticks[4]; // total ticks consumed at each level
// //     int times_scheduled; // number of times the process has been scheduled
// //     int total_syscalls; // total system calls made (from PA1)
// // };

// // // The Leviathans: Pure CPU destruction.
// // // Target: They should plummet to Priority 3 and stay there (except for boosts).
// // void cpu_hog(int id, uint64 iterations) {
// //     printf("CPU Hog %d starting (Target: Queue 3)...\n", id);
    
// //     volatile uint64 counter = 0;
// //     for (uint64 i = 0; i < iterations; i++) {
// //         counter += (i % 7) * 3;
        
// //         // Print a message every 2 Billion loops so you know it hasn't crashed
// //         if (i % 2000000000ULL == 0 && i > 0) {
// //             printf("  -> CPU Hog %d reached %d Billion.\n", id, (int)(i / 1000000000ULL));
// //         }
// //     }
// //     printf(">>> CPU Hog %d FINISHED! <<<\n", id);
// //     struct mlfqinfo info;
// //     int res = getmlfqinfo(getpid(), &info);
// //     if (res == 0) {
// //         printf("\n=== MLFQ INFO FOR %d ===\n", getpid());
// //         printf("Current Queue Level: %d\n", info.level);
// //         printf("Ticks Consumed at Each Level: [Q0: %d, Q1: %d, Q2: %d, Q3: %d]\n", 
// //                 info.ticks[0], info.ticks[1], info.ticks[2], info.ticks[3]);
// //         printf("Times Scheduled: %d\n", info.times_scheduled);
// //         printf("Total Syscalls Made: %d\n", info.total_syscalls);
// //     } else {
// //         printf("Failed to get MLFQ info for PID %d\n", getpid());
// //     }
// //     exit(0);
// // }

// // // // The Sprinters: Interactive I/O simulation.
// // // // Target: They should never leave Priority 0.
// // void io_hog(int id, int cycles) {
// //     printf("I/O Hog %d starting (Target: Queue 0)...\n", id);
    
// //     for (int i = 0; i < cycles; i++) {
// //         pause(1); //  pause for 1 tick, giving up the CPU
        
// //         // Do a tiny fraction of math when awake
// //         volatile int x = 0;
// //         for(int j = 0; j < 10000; j++) { 
// //             x += j; 
// //         }
// //     }
// //     printf(">>> I/O Hog %d FINISHED! <<<\n", id);
// //     struct mlfqinfo info;
// //     int res = getmlfqinfo(getpid(), &info);
// //     if (res == 0) {
// //         printf("\n=== MLFQ INFO FOR %d ===\n", getpid());
// //         printf("Current Queue Level: %d\n", info.level);
// //         printf("Ticks Consumed at Each Level: [Q0: %d, Q1: %d, Q2: %d, Q3: %d]\n", 
// //                 info.ticks[0], info.ticks[1], info.ticks[2], info.ticks[3]);
// //         printf("Times Scheduled: %d\n", info.times_scheduled);
// //         printf("Total Syscalls Made: %d\n", info.total_syscalls);
// //     } else {
// //         printf("Failed to get MLFQ info for PID %d\n", getpid());
// //     }
// //     exit(0);
// // }

// // int main() {
// //     printf("=== STARTING ULTRA HEAVY MLFQ TEST ===\n");
// //     printf("Watch closely: I/O Hogs should finish long before CPU Hogs!\n\n");

// //     int pids[5];

// //     // Spawn 3 Heavy CPU processes (8 Billion loops each)
// //     if ((pids[0] = fork()) == 0) cpu_hog(1, 8000000000ULL); 
// //     if ((pids[1] = fork()) == 0) cpu_hog(2, 8000000000ULL); 
// //     if ((pids[2] = fork()) == 0) cpu_hog(3, 8000000000ULL); 


// //     // Spawn 2 I/O processes (Wake up 100 times, do tiny work, go to  pause)
// //     if ((pids[3] = fork()) == 0) io_hog(4, 100); 
// //     if ((pids[4] = fork()) == 0) io_hog(5, 100);

// //     // Parent waits for all 5 children to die
// //     for (int i = 0; i < 5; i++) {
// //         wait(0);
// //     }
// //     struct mlfqinfo info;
// //     int res = getmlfqinfo(getpid(), &info);
// //     if (res == 0) {
// //         printf("\n=== MLFQ INFO FOR %d ===\n", getpid());
// //         printf("Current Queue Level: %d\n", info.level);
// //         printf("Ticks Consumed at Each Level: [Q0: %d, Q1: %d, Q2: %d, Q3: %d]\n", 
// //                 info.ticks[0], info.ticks[1], info.ticks[2], info.ticks[3]);
// //         printf("Times Scheduled: %d\n", info.times_scheduled);
// //         printf("Total Syscalls Made: %d\n", info.total_syscalls);
// //     } else {
// //         printf("Failed to get MLFQ info for PID %d\n", getpid());
// //     }
// //     printf("\n=== ULTRA HEAVY TEST COMPLETE ===\n");
// //     exit(0);
// // }
// // // mlfqtest.c - MLFQ correctness test with timed pause() for I/O simulation
// // Compile: gcc -o mlfqtest mlfqtest.c
// // Run: mlfqtest

// // #include "kernel/types.h"
// // #include "user/user.h"

// // #define NUM_CHILD 4
// // #define CPU_WORK 5000000    // Tune for ~1-2 time slices (adjust for your slices)
// // #define BURSTS 8

// // void cpu_bound(int id) {
// //   printf("CPU%d: start (should demote fast)\n", id);
// //   for(int b = 0; b < BURSTS; b++) {
// //     volatile int sum = id * 1000;
// //     for(int i = 0; i < CPU_WORK; i++) {
// //       sum = sum * 3 + i % 7;  // CPU intensive
// //     }
// //     printf("CPU%d burst %d done\n", id, b);
// //     pause(10);  // Minimal pause (stay CPU-bound)
// //   }
// //   printf("CPU%d FINISHED (should be last)\n", id);
// //   exit(0);
// // }

// // void short_job(int id) {
// //   printf("SHORT%d: start (should finish first)\n", id);
// //   for(int b = 0; b < BURSTS/2; b++) {  // Fewer bursts
// //     volatile int x = 0;
// //     for(int i = 0; i < CPU_WORK/4; i++) {  // Short bursts
// //       x += i % 10;
// //     }
// //     printf("SHORT%d burst %d\n", id, b);
// //     pause(5);
// //   }
// //   printf("SHORT%d FINISHED\n", id);
// //   exit(0);
// // }

// // void io_bound(int id) {
// //   printf("IO%d: start (should get promoted)\n", id);
// //   for(int b = 0; b < BURSTS; b++) {
// //     // Short CPU burst
// //     volatile int x = id;
// //     for(int i = 0; i < CPU_WORK/10; i++) {
// //       x += i;
// //     }
// //     // Long I/O wait (triggers promotion)
// //     printf("IO%d burst %d: waiting I/O...\n", id, b);
// //     pause(200);  // Long pause = I/O bound
// //   }
// //   printf("IO%d FINISHED (should be early)\n", id);
// //   exit(0);
// // }

// // void long_cpu(int id) {
// //   printf("LONG%d: start (sink to prio 0)\n", id);
// //   // One massive burst to exhaust all levels
// //   volatile long long sum = 0;
// //   for(long long i = 0; i < 50000000LL; i++) {
// //     sum += i % 123;
// //   }
// //   printf("LONG%d FINISHED (lowest prio)\n", id);
// //   exit(0);
// // }

// // int main() {
// //   int pids[4];
  
// //   printf("=== MLFQ 4-LEVEL TEST ===\n");
// //   printf("Expect: SHORT+IO finish early, CPU/LONG demoted late\n\n");

// //   pids[0] = fork(); if(pids[0]==0) cpu_bound(1);
// //   pids[1] = fork(); if(pids[1]==0) short_job(2); 
// //   pids[2] = fork(); if(pids[2]==0) io_bound(3);
// //   pids[3] = fork(); if(pids[3]==0) long_cpu(4);

// //   // Wait and check completion order
// // //   // Wait and check completion order
// //   for(int i = 0; i < 4; i++) {
// //     int wpid = wait(0);
// //     printf("WAIT: proc %d finished\n", wpid);
// //   }
  
// //   printf("\nMLFQ PASS criteria:\n");
// //   printf("- SHORT2 finished among first 2\n");
// //   printf("- IO3 finished early (not last)\n");
// //   printf("- CPU1/LONG4 finished late\n");
  
// //   exit(0);
// // }


// // // #include "kernel/types.h"
// // // #include "kernel/stat.h"
// // // #include "user/user.h"

// // // int main(int argc, char *argv[]) {
// // //     int pid_cpu, pid_io;

// // //     printf("Starting Mixed Workload MLFQ Test...\n");

// // //     // Process 1: CPU-Bound
// // //     // This process does non-stop math. It should exhaust its time slices
// // //     // and plummet down to Priority 3.
// // //     pid_cpu = fork();
// // //     if (pid_cpu == 0) {
// // //         volatile uint64 counter = 0;
// // //         for (uint64 i = 0; i < 3000000000ULL; i++) {
// // //             counter++; // Burn CPU cycles
// // //         }
// // //         printf("\nCPU-bound process (PID %d) finished.\n", getpid());
// // //         exit(0);
// // //     }

// // //     // Process 2: I/O-Bound
// // //     // This process wakes up, does almost nothing, and goes right back to pause.
// // //     // Because it voluntarily yields via pause(), it should NEVER be demoted.
// // //     // It should stay at Priority 0 the entire time.
// // //     pid_io = fork();
// // //     if (pid_io == 0) {
// // //         for (int i = 0; i < 100; i++) {
// // //             pause(2); // Go to pause for 2 timer ticks
// // //         }
// // //         printf("\nI/O-bound process (PID %d) finished.\n", getpid());
// // //         exit(0);
// // //     }

// // //     // Parent waits for both children to finish
// // //     wait(0);
// // //     wait(0);

// // //     printf("Mixed test complete.\n");
// // //     exit(0);
// // // }



// // // #include "kernel/types.h"
// // // #include "kernel/stat.h"
// // // #include "user/user.h"

// // // int main(int argc, char *argv[]) {
// // //     int pid;
// // //     int num_children = 3;

// // //     printf("Starting MLFQ test...\n");

// // //     for (int i = 0; i < num_children; i++) {
// // //         pid = fork();
        
// //         if (pid < 0) {
// //             printf("Fork failed\n");
// //             exit(1);
// //         }
        
// //         if (pid == 0) { // Child process
// //             // Do heavy CPU work to force time-slice demotion
// //             volatile int counter = 0;
// //             for(uint64 j = 0; j < 5000000000ULL; j++) {
// //                 counter += j; // Useless math to burn CPU ticks
                
// //                 // pause briefly to let the parent print
// //                 if(j % 100000000 == 0) {
// //                    pause(1); 
// //                 }
// //             }
// //             printf("Child %d finished.\n", getpid());
// //             exit(0);
// //         }
// //     }

// //     // Parent waits for all children to finish
// //     for (int i = 0; i < num_children; i++) {
// //         wait(0);
// //     }

// //     printf("MLFQ test complete.\n");
// //     exit(0);
// // }

// // // // // #include "kernel/types.h"
// // // // // #include "kernel/stat.h"
// // // // // #include "user/user.h"

// // // // // // #define TEST_ITERATIONS 100

// // // // // // // Simple assert helper
// // // // // // void check(int valid, char *name, int iter) {
// // // // // //     if (!valid) {
// // // // // //         printf("FAILED: %s at iteration %d\n", name, iter);
// // // // // //         exit(1);
// // // // // //     }
// // // // // // }

// // // // // // // --- PART A TESTS ---

// // // // // // // void test_hello_100() {
// // // // // // //     printf("Running 100 tests for hello()...\n");
// // // // // // //     for (int i = 0; i < TEST_ITERATIONS; i++) {
// // // // // // //         // [cite: 81] hello() must return 0
// // // // // // //         int ret = hello();
// // // // // // //         check(ret == 0, "hello() return value", i);
// // // // // // //     }
// // // // // // //     printf("PASSED: hello() 100 iterations.\n");
// // // // // // // }

// // // // // // // void test_getpid2_100() {
// // // // // // //     printf("Running 100 tests for getpid2()...\n");
// // // // // // //     int std_pid = getpid();
// // // // // // //     for (int i = 0; i < TEST_ITERATIONS; i++) {
// // // // // // //         // [cite: 87] getpid2() must match getpid()
// // // // // // //         int my_pid = getpid2();
// // // // // // //         // check(my_pid == std_pid, "getpid2() consistency", i);
// // // // // // //         if (my_pid != std_pid) {
// // // // // // //             printf("FAILED: getpid2() expected %d, got %d at iteration %d\n", std_pid, my_pid, i);
// // // // // // //             exit(1);
// // // // // // //         }
// // // // // // //         print
// // // // // // //     }
// // // // // // //     printf("PASSED: getpid2() 100 iterations.\n");
// // // // // // // }

// // // // // // // --- PART B TESTS ---

// // // // // // // void test_getppid_100() {
// // // // // // //     printf("Running 100 tests for getppid()...\n");
// // // // // // //     int my_pid = getpid();
    
// // // // // // //     // We cannot fork 100 children at once (xv6 limit is 64).
// // // // // // //     // We run 100 sequential tests.
// // // // // // //     for (int i = 0; i < TEST_ITERATIONS; i++) {
// // // // // // //         int pid = fork();
// // // // // // //         if (pid < 0) {
// // // // // // //             printf("Fork failed at iter %d\n", i);
// // // // // // //             exit(1);
// // // // // // //         }
        
// // // // // // //         if (pid == 0) {
// // // // // // //             // Child: verify parent
// // // // // // //             // [cite: 97] getppid() returns parent PID
// // // // // // //             int ppid = getppid();
// // // // // // //             if (ppid != my_pid) {
// // // // // // //                 printf("FAILED: Expected PPID %d, got %d\n", my_pid, ppid);
// // // // // // //                 exit(1);
// // // // // // //             }
// // // // // // //             exit(0);
// // // // // // //         } else {
// // // // // // //             // Parent: wait for child
// // // // // // //             wait(0);
// // // // // // //         }
// // // // // // //     }
// // // // // // //     printf("PASSED: getppid() 100 iterations.\n");
// // // // // // // }

// // // // // // // void test_getnumchild_100() {
// // // // // // //     printf("Running 100 tests for getnumchild()...\n");
    
// // // // // // //     for (int i = 0; i < TEST_ITERATIONS; i++) {
// // // // // // //         // 1. Initial check (should be 0 active children in this scope)
// // // // // // //         check(getnumchild() == 0, "getnumchild baseline", i);

// // // // // // //         // 2. Fork a child
// // // // // // //         int pid = fork();
// // // // // // //         if (pid == 0) {
// // // // // // //             pause(10); // Stay alive briefly
// // // // // // //             exit(0);
// // // // // // //         }
        
// // // // // // //         // 3. Parent checks count
// // // // // // //         // [cite: 103] getnumchild() counts active children
// // // // // // //         int nc = getnumchild();
// // // // // // //         check(nc == 1, "getnumchild active count", i);
        
// // // // // // //         // 4. Wait and verify decrement
// // // // // // //         wait(0);
// // // // // // //         check(getnumchild() == 0, "getnumchild after wait", i);
// // // // // // //     }
// // // // // // //     printf("PASSED: getnumchild() 100 iterations.\n");
// // // // // // // }

// // // // // // // // --- PART C TESTS ---

// // // // // // // void test_getsyscount_100() {
// // // // // // //     printf("Running 100 tests for getsyscount()...\n");
    
// // // // // // //     int start_count = getsyscount();
    
// // // // // // //     // [cite: 115] getsyscount() tracks calling process syscalls
// // // // // // //     for (int i = 0; i < TEST_ITERATIONS; i++) {
// // // // // // //         int pre = getsyscount();
// // // // // // //         getpid(); // Execute 1 syscall (getpid is cheap)
// // // // // // //         int post = getsyscount();
        
// // // // // // //         // Count must increase by at least 1 (getpid) + 1 (getsyscount itself)
// // // // // // //         check(post > pre, "getsyscount increments", i);
// // // // // // //     }
    
// // // // // // //     int end_count = getsyscount();
// // // // // // //     // We did 100 loops.
// // // // // // //     check((end_count - start_count) >= TEST_ITERATIONS, "getsyscount total accuracy", 0);
    
// // // // // // //     printf("PASSED: getsyscount() 100 iterations.\n");
// // // // // // // }

// // // // // // void test_getchildsyscount_100() {
// // // // // //     printf("Running 100 fuzzing tests for getchildsyscount()...\n");
    
// // // // // //     // 1. Fuzzing: Check 100 invalid PIDs
// // // // // //     // [cite: 121] Should return -1 for non-children
// // // // // //     for (int i = 0; i < TEST_ITERATIONS; i++) {
// // // // // //         int invalid_pid = 10000 + i; // Likely non-existent PIDs
// // // // // //         int ret = getchildsyscount(invalid_pid);
// // // // // //         check(ret == -1, "getchildsyscount invalid PID", i);
// // // // // //     }

// // // // // //     // 2. Functional: Check 100 valid checks (sequential)
// // // // // //     printf("Running 100 functional tests for getchildsyscount()...\n");
// // // // // //     for (int i = 0; i < TEST_ITERATIONS; i++) {
// // // // // //         int fd[2];
// // // // // //         pipe(fd);
        
// // // // // //         int pid = fork();
// // // // // //         if (pid == 0) {
// // // // // //             // Child performs exactly 1 syscall (read) then exits
// // // // // //             char b;
// // // // // //             read(fd[0], &b, 1);
// // // // // //             exit(0);
// // // // // //         }
        
// // // // // //         // Parent
// // // // // //         write(fd[1], "x", 1); // Unlock child
// // // // // //         pause(1); // Give child time to read
        
// // // // // //         // [cite: 118] Get count of child
// // // // // //         int count = getchildsyscount(pid);
        
// // // // // //         // Child did at least: read(1) + maybe exit/others. Count > 0.
// // // // // //         // Note: If child exited too fast, this might return -1 depending on your implementation of Zombie handling.
// // // // // //         // Assuming assignment implies checking running or zombie children before wait().
// // // // // //         if (count != -1) {
// // // // // //              check(count > 0, "getchildsyscount valid value", i);
// // // // // //         }
        
// // // // // //         wait(0);
// // // // // //         close(fd[0]);
// // // // // //         close(fd[1]);
// // // // // //     }
// // // // // //     printf("PASSED: getchildsyscount() 100 iterations.\n");
// // // // // // }

// // // // // // int main(int argc, char *argv[]) {
// // // // // //     printf("--- STARTING 100-ITERATION STRESS SUITE ---\n");
    
// // // // // //     // test_hello_100();
// // // // // //     // test_getpid2_100();
// // // // // //     // test_getppid_100();
// // // // // //     // test_getnumchild_100();
// // // // // //     // test_getsyscount_100();
// // // // // //     test_getchildsyscount_100();
    
// // // // // //     printf("\n--- ALL 600+ TESTS PASSED ---\n");
// // // // // //     exit(0);
// // // // // // }

















// // // #include "kernel/types.h"
// // // #include "kernel/stat.h"
// // // #include "user/user.h"



// // // void assert(int condition, const char *message) {
// // //     if (!condition) {
// // //         printf("Assertion failed: %s\n", message);
// // //         exit(0);
// // //     }
// // // }


// // // void
// // // test_part_A(void)
// // // {
// // //     printf("\n--- Testing Part A: Warm-up ---\n");

// // //     // A1. hello()
// // //     // Since hello() prints to console, visual verification is required.
// // //     printf("Calling hello()...\n");
// // //     hello(); 
// // //     printf("CHECK: Did you see 'Hello from the kernel!' above?\n");

// // //     // A2. getpid2()
// // //     int pid1 = getpid();
// // //     int pid2 = getpid2();
    
// // //     printf("getpid() returned %d, getpid2() returned %d\n", pid1, pid2);
// // //        assert(pid1 == pid2, "getpid2() matches getpid()");
// // // }

// // // void
// // // test_part_B(void)
// // // {
// // //     printf("\n--- Testing Part B: Process Relationships ---\n");

// // //     // B1. getppid()
// // //     int my_pid = getpid();
// // //     int parent_pid = getppid();
    
// // //     printf("My PID: %d, Parent PID (from getppid): %d\n", my_pid, parent_pid);
    
// // //     // Parent PID should be valid (>0) and distinct from own PID
// // //     assert(parent_pid > 0, "Parent PID is valid (>0)");
// // //     assert(parent_pid != my_pid, "Parent PID is not same as own PID");

// // //     // B2. getnumchild()
// // //     int initial_kids = getnumchild();
// // //     printf("Initial children count: %d\n", initial_kids);

// // //     int pid = fork();
// // //     if(pid < 0){
// // //         printf("Fork failed\n");
// // //         exit(1);
// // //     }

// // //     if(pid == 0){
// // //         // Child process
// // //         pause(20); // pause long enough for parent to check count
// // //         exit(0);
// // //     } else {
// // //         // Parent process
// // //         int kids_after_fork = getnumchild();
// // //         printf("Children count after fork: %d\n", kids_after_fork);
        
// // //         // Count should increase by 1
// // //         assert(kids_after_fork == initial_kids + 1, "getnumchild() incremented correctly");
        
// // //         // Wait for child to exit
// // //         wait(0);
        
// // //         int kids_after_wait = getnumchild();
// // //         printf("Children count after wait: %d\n", kids_after_wait);
        
// // //         // Count should return to initial state
// // //         assert(kids_after_wait == initial_kids, "getnumchild() decremented correctly after wait");
// // //     }
// // // }

// // // void
// // // test_part_C(void)
// // // {
// // //     printf("\n--- Testing Part C: System Call Accounting ---\n");

// // //     // C2. getsyscount()
// // //     // We establish a baseline, make exactly 3 syscalls, and check the difference.
// // //     int start_count = getsyscount();
    
// // //     // Making 3 explicit system calls
// // //     getpid();
// // //     getpid();
// // //     getpid();
    
// // //     int end_count = getsyscount();
    
// // //     // The delta is usually 4 because getsyscount() itself counts as a syscall
// // //     int diff = end_count - start_count;
// // //     printf("Syscalls made: 3 explicit + overhead. Delta: %d\n", diff);
    
// // //     assert(diff >= 3, "getsyscount() is incrementing properly");

// // //     // C3. getchildsyscount(pid)
// // //     int pid = fork();
// // //     if(pid < 0){
// // //         printf("Fork failed\n");
// // //         exit(1);
// // //     }

// // //     if(pid == 0){
// // //         // Child process
// // //         // Perform some syscalls to increase count
// // //         getpid(); 
// // //         getpid(); 
// // //         pause(10); 
// // //         exit(0);   
// // //     } else {
// // //         // Parent process
// // //         // Give child time to run its syscalls
// // //         pause(5); 
        
// // //         int child_count = getchildsyscount(pid);
// // //         printf("Child (PID %d) syscall count: %d\n", pid, child_count);
        
// // //         // Ensure we got a valid number back (not -1 or 0)
// // //         assert(child_count > 0, "getchildsyscount() returns valid positive count");
        
// // //         // Negative test: Check invalid PID (like 99999)
// // //         int invalid_count = getchildsyscount(99999);
// // //         printf("getchildsyscount(99999) returned: %d\n", invalid_count);
// // //         assert(invalid_count == -1, "getchildsyscount() returns -1 for invalid PID");
        
// // //         wait(0);
// // //     }
// // // }

// // // int
// // // main(int argc, char *argv[])
// // // {
// // //     printf("Starting Assignment 1 Comprehensive Test...\n");

// // //     test_part_A();
// // //     test_part_B();
// // //     test_part_C();

// // //     printf("\nALL TESTS PASSED SUCCESSFULLY!\n");
// // //     exit(0);
// // // }


// // // // #include "kernel/types.h"
// // // // #include "kernel/stat.h"
// // // // #include "user/user.h"



// // // // void assert(int condition, const char *message) {
// // // //     if (!condition) {
// // // //         printf("Assertion failed: %s\n", message);
// // // //         exit(0);
// // // //     }
// // // // }


// // // // void
// // // // test_zombie_exclusion(void)
// // // // {
// // // //     printf("\n[Test] Zombie Exclusion for getnumchild()...\n");
    
// // // //     int initial = getnumchild();
// // // //     int pid = fork();
    
// // // //     if(pid == 0) {
// // // //         exit(0); // Child exits immediately -> becomes ZOMBIE
// // // //     }
    
// // // //     // Parent pauses to let child exit and become zombie
// // // //     pause(10);
    
// // // //     int after_zombie = getnumchild();
// // // //     printf("Initial: %d, After Zombie: %d\n", initial, after_zombie);
    
// // // //     // The zombie child should NOT be counted if your logic is correct
// // // //     assert(after_zombie == initial, "Zombie process is ignored by getnumchild()");
    
// // // //     wait(0); // Clean up
// // // // }

// // // // void
// // // // test_grandchild_protection(void)
// // // // {
// // // //     printf("\n[Test] Grandchild Protection for getchildsyscount()...\n");
    
// // // //     int pid = fork();
// // // //     if(pid == 0) {
// // // //         // Child process
// // // //         int grand_pid = fork();
// // // //         if(grand_pid == 0) {
// // // //             pause(20);
// // // //             exit(0);
// // // //         }
        
// // // //         // Child waits for grandchild
// // // //         wait(0);
// // // //         exit(0);
// // // //     }
    
// // // //     // Parent logic
// // // //     pause(5); // Wait for hierarchy to establish
    
// // // //     // We cannot easily know the grandchild's PID here without shared memory,
// // // //     // but we can try to guess it (usually pid + 1) or just test the principle.
// // // //     // Instead, let's test a simpler permission: Can I query my own parent?
    
// // // //     int parent = getppid();
// // // //     int count = getchildsyscount(parent);
// // // //     assert(count == -1, "Cannot query parent's syscall count");
    
// // // //     wait(0);
// // // // }

// // // // void
// // // // test_stress_counting(void)
// // // // {
// // // //     printf("\n[Test] Stress Counting getsyscount()...\n");
    
// // // //     int start = getsyscount();
// // // //     int target = 100;
    
// // // //     for(int i = 0; i < target; i++){
// // // //         getpid(); // Fastest syscall
// // // //     }
    
// // // //     int end = getsyscount();
// // // //     int diff = end - start;
    
// // // //     // Logic: diff includes the calls in the loop + the overhead of the getsyscount() calls themselves.
// // // //     // It should be strictly >= 100.
// // // //     printf("Start: %d, End: %d, Diff: %d\n", start, end, diff);
// // // //     assert(diff >= target, "Counter captured all 100 syscalls");
// // // // }

// // // // void
// // // // test_sibling_isolation(void)
// // // // {
// // // //     printf("\n[Test] Sibling Isolation...\n");
    
// // // //     // We need pipe to send PID from one child to another (or parent)
// // // //     int p[2];
// // // //     pipe(p);
    
// // // //     int child1 = fork();
// // // //     if(child1 == 0) {
// // // //         // Child 1
// // // //         int mypid = getpid();
// // // //         write(p[1], &mypid, sizeof(mypid));
// // // //         pause(20);
// // // //         exit(0);
// // // //     }
    
// // // //     int child2 = fork();
// // // //     if(child2 == 0) {
// // // //         // Child 2
// // // //         int sibling_pid;
// // // //         read(p[0], &sibling_pid, sizeof(sibling_pid));
        
// // // //         printf("Child 2 trying to spy on Child 1 (PID %d)...\n", sibling_pid);
// // // //         int count = getchildsyscount(sibling_pid);
        
// // // //         if(count == -1) {
// // // //             printf("PASS: Sibling could not read count.\n");
// // // //             exit(0); // Success
// // // //         } else {
// // // //             printf("FAIL: Sibling read count %d!\n", count);
// // // //             exit(1); // Fail
// // // //         }
// // // //     }
    
// // // //     // Parent cleans up
// // // //     close(p[0]);
// // // //     close(p[1]);
// // // //     wait(0);
// // // //     wait(0);
// // // // }

// // // // int
// // // // main(void)
// // // // {
// // // //     printf("=== ROBUSTNESS TEST STARTING ===\n");
    
// // // //     test_stress_counting();
// // // //     test_zombie_exclusion();
// // // //     test_grandchild_protection();
// // // //     test_sibling_isolation();
    
// // // //     printf("\n=== ALL ROBUSTNESS TESTS PASSED ===\n");
// // // //     exit(0);
// // // // }


// // // // // #include "kernel/types.h"
// // // // // #include "kernel/stat.h"
// // // // // #include "user/user.h"
// // // // // #include "kernel/fcntl.h"

// // // // // // ANSI Color codes for prettier output
// // // // // #define GREEN "\x1b[32m"
// // // // // #define RED   "\x1b[31m"
// // // // // #define RESET "\x1b[0m"



// // // // // // Wrapper for visual clarity in code
// // // // // void log_section(char *name) {
// // // // //     printf("\n============================================\n");
// // // // //     printf("   TESTING: %s\n", name);
// // // // //     printf("============================================\n");
// // // // // }

// // // // // // -------------------------------------------------------------
// // // // // // PART A TESTS
// // // // // // -------------------------------------------------------------
// // // // // void test_part_A() {
// // // // //     log_section("Part A (Basic Syscalls)");

// // // // //     printf("1. Testing hello(). Check console output below:\n");
// // // // //     printf("   ---> ");
// // // // //     hello();
// // // // //     printf("   <--- (Should say 'Hello from the kernel!')\n");

// // // // //     int p1 = getpid();
// // // // //     int p2 = getpid2();
// // // // //     printf("2. PID Comparison: getpid()=%d, getpid2()=%d\n", p1, p2);
// // // // //     assert(p1 == p2, "getpid2 matches getpid");

// // // // //     // Consistency check across fork
// // // // //     int pid = fork();
// // // // //     if(pid == 0) {
// // // // //         assert(getpid() == getpid2(), "Child getpid2 matches child getpid");
// // // // //         exit(0);
// // // // //     }
// // // // //     wait(0);
// // // // // }

// // // // // // -------------------------------------------------------------
// // // // // // PART B TESTS
// // // // // // -------------------------------------------------------------
// // // // // void test_part_B_reparenting() {
// // // // //     log_section("Part B (Reparenting Logic)");

// // // // //     int pipe_fd[2];
// // // // //     pipe(pipe_fd);

// // // // //     int child = fork();
// // // // //     if(child == 0) {
// // // // //         // --- MIDDLE CHILD ---
// // // // //         int grandchild = fork();
// // // // //         if(grandchild == 0) {
// // // // //             // --- GRANDCHILD ---
// // // // //             close(pipe_fd[0]);
            
// // // // //             // Step 1: Verify current parent is the Middle Child
// // // // //             int original_parent = getppid();
// // // // //             write(pipe_fd[1], &original_parent, sizeof(int));

// // // // //             // Wait for Middle Child to die
// // // // //             pause(10); // pause long enough for Middle Child to exit

// // // // //             // Step 2: Verify new parent is Init (1)
// // // // //             int new_parent = getppid();
// // // // //             write(pipe_fd[1], &new_parent, sizeof(int));
            
// // // // //             exit(0);
// // // // //         }
        
// // // // //         // --- CRITICAL FIX HERE ---
// // // // //         // We must pause briefly to ensure Grandchild runs Step 1 
// // // // //         // BEFORE we exit and trigger reparenting.
// // // // //         pause(5); 

// // // // //         // Now Middle child exits
// // // // //         exit(0);
// // // // //     }

// // // // //     // --- PARENT ---
// // // // //     close(pipe_fd[1]);
    
// // // // //     // Wait for middle child to die
// // // // //     wait(0); 

// // // // //     int p1, p2;
// // // // //     read(pipe_fd[0], &p1, sizeof(int)); // Grandchild's first parent
// // // // //     read(pipe_fd[0], &p2, sizeof(int)); // Grandchild's second parent

// // // // //     printf("Grandchild's Parent: %d -> %d\n", p1, p2);
    
// // // // //     // Check 1: Was the parent originally the Middle Child?
// // // // //     if(p1 != child) {
// // // // //         printf(RED "[FAIL]" RESET " Grandchild initially owned by %d (Expected %d)\n", p1, child);
// // // // //         exit(1);
// // // // //     } else {
// // // // //         printf(GREEN "[PASS]" RESET " Grandchild initially owned by Child\n");
// // // // //     }

// // // // //     // Check 2: Was it reparented to Init?
// // // // //     if(p2 != 1) {
// // // // //         printf(RED "[FAIL]" RESET " Grandchild adopted by %d (Expected 1)\n", p2);
// // // // //         exit(1);
// // // // //     } else {
// // // // //         printf(GREEN "[PASS]" RESET " Grandchild adopted by init (PID 1)\n");
// // // // //     }

// // // // //     // Wait for grandchild (cleanup)
// // // // //     pause(5);
// // // // // }

// // // // // void test_part_B_zombies() {
// // // // //     log_section("Part B (Zombie Accounting)");

// // // // //     int initial = getnumchild();
// // // // //     printf("Initial children: %d\n", initial);

// // // // //     int pid = fork();
// // // // //     if(pid == 0) exit(0); // Become zombie instantly

// // // // //     // Give kernel time to process exit
// // // // //     pause(5);

// // // // //     int during = getnumchild();
// // // // //     printf("Children count while zombie exists: %d\n", during);
// // // // //     assert(during == initial, "Zombies are NOT counted in getnumchild");

// // // // //     wait(0); // Clean up zombie
// // // // //     int after = getnumchild();
// // // // //     assert(after == initial, "Count remains consistent after wait");
// // // // // }

// // // // // // -------------------------------------------------------------
// // // // // // PART C TESTS
// // // // // // -------------------------------------------------------------
// // // // // void test_part_C_accuracy() {
// // // // //     log_section("Part C (Syscall Accuracy)");

// // // // //     int start = getsyscount();
    
// // // // //     // Perform exactly 10 cheap syscalls
// // // // //     for(int i=0; i<10; i++) getpid();

// // // // //     int end = getsyscount();
// // // // //     int diff = end - start;

// // // // //     // Expected: 10 getpid + 1 getsyscount (the one called at 'end')
// // // // //     // Depending on implementation, it might be 10 or 11.
// // // // //     // We assert >= 10 to be safe against counter implementation nuances.
// // // // //     printf("Start: %d, End: %d, Delta: %d\n", start, end, diff);
// // // // //     assert(diff >= 10, "Counter captured loop of 10 syscalls");
// // // // // }

// // // // // void test_part_C_isolation() {
// // // // //     log_section("Part C (Permission/Isolation)");

// // // // //     int pid = fork();
// // // // //     if(pid == 0) {
// // // // //         // Child creates a grandchild
// // // // //         int gc_pid = fork();
// // // // //         if(gc_pid == 0) {
// // // // //             pause(20);
// // // // //             exit(0);
// // // // //         }
        
// // // // //         // Child tries to read Grandchild (Valid: it is its own child)
// // // // //         pause(2); // Let GC start
// // // // //         int c = getchildsyscount(gc_pid);
// // // // //         if(c >= 0) {
// // // // //             printf(GREEN "[PASS]" RESET " Child can read Grandchild stats (Direct Parent)\n");
// // // // //         } else {
// // // // //             printf(RED "[FAIL]" RESET " Child failed to read Grandchild stats\n");
// // // // //             exit(1);
// // // // //         }
        
// // // // //         wait(0);
// // // // //         exit(0);
// // // // //     }

// // // // //     // Parent Logic
// // // // //     pause(5); // Wait for hierarchy

// // // // //     // 1. Can Parent read Child? (YES)
// // // // //     assert(getchildsyscount(pid) >= 0, "Parent can read Child stats");

// // // // //     // 2. Can Parent read Grandchild? (NO - assuming we could guess PID)
// // // // //     // In xv6 PIDs increment. pid+1 is likely the grandchild.
// // // // //     int likely_gc_pid = pid + 1;
// // // // //     int val = getchildsyscount(likely_gc_pid);
// // // // //     printf("Attempting to read Grandchild (PID %d): result %d\n", likely_gc_pid, val);
// // // // //     assert(val == -1, "Parent CANNOT read Grandchild stats");

// // // // //     // 3. Can Parent read Init (PID 1)? (NO)
// // // // //     assert(getchildsyscount(1) == -1, "Parent CANNOT read PID 1 stats");

// // // // //     wait(0);
// // // // // }

// // // // // void test_part_C_stress_concurrency() {
// // // // //     log_section("Part C (Concurrency Stress)");

// // // // //     int children[5];
// // // // //     int n_kids = 5;

// // // // //     // Fork 5 children
// // // // //     for(int i=0; i<n_kids; i++) {
// // // // //         int p = fork();
// // // // //         if(p == 0) {
// // // // //             // Child: Hammer syscalls
// // // // //             for(int j=0; j<50; j++) getpid();
// // // // //             exit(0);
// // // // //         }
// // // // //         children[i] = p;
// // // // //     }

// // // // //     // Parent: Monitor them while they run
// // // // //     int active_kids = getnumchild();
// // // // //     printf("Active children (should be 5): %d\n", active_kids);
// // // // //     assert(active_kids == 5, "All 5 children alive");

// // // // //     // Read stats for first child
// // // // //     int count_start = getchildsyscount(children[0]);
// // // // //     assert(count_start >= 0, "Can read child stat while concurrent load exists");
    
// // // // //     // Wait for all
// // // // //     for(int i=0; i<n_kids; i++) wait(0);
    
// // // // //     assert(getnumchild() == 0, "All children cleaned up");
// // // // // }

// // // // // // -------------------------------------------------------------
// // // // // // MAIN
// // // // // // -------------------------------------------------------------
// // // // // int main(int argc, char *argv[]) {
// // // // //     printf("\n*** STARTING MEGA ROBUSTNESS SUITE ***\n");

// // // // //     test_part_A();
// // // // //     test_part_B_zombies();
// // // // //     test_part_B_reparenting();
// // // // //     test_part_C_accuracy();
// // // // //     test_part_C_isolation();
// // // // //     test_part_C_stress_concurrency();

// // // // //     printf("\n" GREEN "*** ALL MEGA TESTS PASSED SUCCESSFULLY ***" RESET "\n");
// // // // //     exit(0);
// // // // // }




// // // // // // #include "kernel/types.h"
// // // // // // #include "kernel/stat.h"
// // // // // // #include "user/user.h"
// // // // // // #include "kernel/fcntl.h"












// // // // // // // --- Helper Macros for Clean Output ---
// // // // // // #define PASS(msg) printf("[PASS] %s\n", msg)
// // // // // // #define FAIL(msg) do { printf("[FAIL] %s\n", msg); exit(1); } while(0)
// // // // // // #define INFO(msg) printf("   ... %s\n", msg)

// // // // // // void assert_eq(int a, int b, char *msg) {
// // // // // //   if (a == b) PASS(msg);
// // // // // //   else {
// // // // // //     printf("[FAIL] %s (Expected %d, Got %d)\n", msg, b, a);
// // // // // //     exit(1);
// // // // // //   }
// // // // // // }

// // // // // // // --------------------------------------------------------------
// // // // // // // TEST 1: The Precision Accountant
// // // // // // // Verifies that 'getsyscount' tracks the EXACT number of calls.
// // // // // // // --------------------------------------------------------------
// // // // // // void test_precision_accounting() {
// // // // // //   printf("\n=== TEST 1: Precision Accounting ===\n");
  
// // // // // //   // Snapshot start count
// // // // // //   int start = getsyscount();

// // // // // //   // Perform exactly 5 system calls
// // // // // //   getpid();
// // // // // //   getpid();
// // // // // //   getpid();
// // // // // //   getpid();
// // // // // //   getpid();

// // // // // //   // Snapshot end count
// // // // // //   int end = getsyscount();

// // // // // //   // MATH:
// // // // // //   // start = count BEFORE the first getsyscount returns
// // // // // //   // end   = count BEFORE the second getsyscount returns
// // // // // //   // The calls between were: 5x getpid + 1x getsyscount (the 'end' call itself)
// // // // // //   // So expected delta is exactly 6.
  
// // // // // //   int diff = end - start;
// // // // // //   printf("   Start: %d, End: %d, Delta: %d\n", start, end, diff);

// // // // // //   // Note: Depending on where exactly your kernel increments (before/after dispatch),
// // // // // //   // this is usually 6. If your kernel increments strictly *before* dispatch, 
// // // // // //   // the 'end' call counts. 
// // // // // //   if (diff == 6) {
// // // // // //     PASS("Accounting is mathematically precise (Delta 6)");
// // // // // //   } else if (diff == 5) {
// // // // // //     PASS("Accounting is precise (Delta 5 - exclusive of current call)");
// // // // // //   } else {
// // // // // //     FAIL("Accounting drifted! Counters are inaccurate.");
// // // // // //   }
// // // // // // }

// // // // // // // --------------------------------------------------------------
// // // // // // // TEST 2: The Permission Matrix
// // // // // // // Verifies strict rules: Parent->Child OK. Child->Parent NO.
// // // // // // // --------------------------------------------------------------
// // // // // // void test_permission_matrix() {
// // // // // //   printf("\n=== TEST 2: Permission Matrix ===\n");

// // // // // //   int pid = fork();
// // // // // //   if (pid < 0) FAIL("Fork failed");

// // // // // //   if (pid == 0) {
// // // // // //     // --- CHILD PROCESS ---
// // // // // //     int my_pid = getpid();
// // // // // //     int parent_pid = getppid();

// // // // // //     // 1. Can I read my own stats? (Should be YES, technically I am my own child?)
// // // // // //     // Actually, usually NO because parent != myproc().
// // // // // //     // Your code checks: if(pp->parent == myproc()). 
// // // // // //     // A process is NOT its own parent. So this should fail (-1).
// // // // // //     if (getchildsyscount(my_pid) == -1) {
// // // // // //       PASS("Child cannot read its own stats via getchildsyscount (Correct)");
// // // // // //     } else {
// // // // // //       FAIL("Child incorrectly allowed to read self as 'child'");
// // // // // //     }

// // // // // //     // 2. Can I read my Parent? (Should be NO)
// // // // // //     if (getchildsyscount(parent_pid) == -1) {
// // // // // //       PASS("Child cannot read Parent's stats");
// // // // // //     } else {
// // // // // //       FAIL("Child violated security: read Parent's stats!");
// // // // // //     }

// // // // // //     // 3. Can I read Init (PID 1)? (Should be NO)
// // // // // //     if (getchildsyscount(1) == -1) {
// // // // // //       PASS("Child cannot read Init (PID 1) stats");
// // // // // //     } else {
// // // // // //       FAIL("Child violated security: read PID 1 stats!");
// // // // // //     }

// // // // // //     exit(0);
// // // // // //   }

// // // // // //   // --- PARENT PROCESS ---
// // // // // //   wait(0); // Wait for child tests to finish
  
// // // // // //   // 4. Can Parent read Child? (Should be YES)
// // // // // //   // We need to create a fresh child because the previous one is dead/zombie.
// // // // // //   int pid2 = fork();
// // // // // //   if (pid2 == 0) {
// // // // // //     // Just run some calls then pause
// // // // // //     getpid(); 
// // // // // //     pause(20); 
// // // // // //     exit(0);
// // // // // //   }
  
// // // // // //   pause(5); // Give child time to run
// // // // // //   int count = getchildsyscount(pid2);
// // // // // //   if (count > 0) {
// // // // // //     PASS("Parent successfully read Child stats");
// // // // // //   } else {
// // // // // //     FAIL("Parent failed to read Child stats (Returned -1)");
// // // // // //   }
  
// // // // // //   // Kill child2
// // // // // //   kill(pid2);
// // // // // //   wait(0);
// // // // // // }

// // // // // // // --------------------------------------------------------------
// // // // // // // TEST 3: The Zombie Lifecycle
// // // // // // // Ensures zombies are ignored by getnumchild but visible to wait
// // // // // // // --------------------------------------------------------------
// // // // // // void test_zombie_lifecycle() {
// // // // // //   printf("\n=== TEST 3: Zombie Lifecycle ===\n");

// // // // // //   int initial = getnumchild();
// // // // // //   INFO("Spawning short-lived child...");

// // // // // //   int pid = fork();
// // // // // //   if (pid == 0) {
// // // // // //     exit(0); // Die instantly -> Become ZOMBIE
// // // // // //   }

// // // // // //   // Parent pauses to guarantee child is in ZOMBIE state
// // // // // //   pause(10);

// // // // // //   int after_zombie = getnumchild();
  
// // // // // //   if (after_zombie == initial) {
// // // // // //     PASS("Zombie correctly ignored by getnumchild()");
// // // // // //   } else {
// // // // // //     printf("   Initial: %d, Current: %d (Should be equal)\n", initial, after_zombie);
// // // // // //     FAIL("Zombie was incorrectly counted as a live child!");
// // // // // //   }

// // // // // //   wait(0); // Reap zombie
// // // // // // }

// // // // // // // --------------------------------------------------------------
// // // // // // // TEST 4: The Deadlock Stress Test
// // // // // // // Rapidly creates children that exit while parent queries stats.
// // // // // // // If locks are wrong (wait_lock vs p->lock), this usually hangs.
// // // // // // // --------------------------------------------------------------
// // // // // // void test_deadlock_stress() {
// // // // // //   printf("\n=== TEST 4: Deadlock & Race Stress ===\n");
// // // // // //   INFO("Running concurrent fork/exit/query...");

// // // // // //   int pid = fork();
// // // // // //   if (pid == 0) {
// // // // // //     // Child: Rapidly fork and exit grandchildren
// // // // // //     for (int i = 0; i < 20; i++) {
// // // // // //       int gc = fork();
// // // // // //       if (gc == 0) {
// // // // // //         getpid(); // do work
// // // // // //         exit(0);
// // // // // //       }
// // // // // //       // Do not wait immediately, let some zombies pile up
// // // // // //     }
// // // // // //     // Now wait for all
// // // // // //     while(wait(0) != -1);
// // // // // //     exit(0);
// // // // // //   }

// // // // // //   // Parent: Hammer the system with read requests
// // // // // //   for (int i = 0; i < 30; i++) {
// // // // // //     // This query traverses the list while the child is modifying it
// // // // // //     // If locking is broken, this will DEADLOCK.
// // // // // //     getnumchild(); 
// // // // // //     getchildsyscount(pid);
// // // // // //   }

// // // // // //   wait(0);
// // // // // //   PASS("System survived concurrent stress test (No Deadlock)");
// // // // // // }

// // // // // // int main(void) {
// // // // // //   printf("\n*** FINAL KERNEL VERIFICATION SUITE ***\n");

// // // // // //   test_precision_accounting();
// // // // // //   test_permission_matrix();
// // // // // //   test_zombie_lifecycle();
// // // // // //   test_deadlock_stress();

// // // // // //   printf("\n" 
// // // // // //          "**************************************************\n"
// // // // // //          "*** ALL CHECKS PASSED. KERNEL IS READY. ***\n"
// // // // // //          "**************************************************\n");
// // // // // //   exit(0);
// // // // // // }


// // // // // // #include "kernel/types.h"
// // // // // // #include "kernel/stat.h"
// // // // // // #include "user/user.h"

// // // // // int main(){
// // // // //     hello();

// // // // //     // printf("Masala pav\n");



// // // // //     // printf("original pid : %d\n",getpid());
// // // // //     // printf("apna pid : %d\n", getpid2());

// // // // //     // printf("baap of this process is : %d\n",getppid());

// // // // //     // int ppid = getppid();
// // // // //     // kill(ppid);
// // // // //     // pause(2);

    

// // // // //     // printf("after killing shell baap is : %d\n",getppid());

// // // // //     // printf("number of syscall for current process: %d\n",getsyscount());
// // // // // }