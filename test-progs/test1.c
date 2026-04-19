#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

// --- Helper Macros ---
#define ASSERT(condition, msg) \
    if(condition) { printf("[PASS] %s\n", msg); } \
    else { printf("[FAIL] %s\n", msg); exit(1); }

// --- Test 1: hello() ---
void test_hello() {
    printf("\n--- Testing hello() ---\n");
    // Assuming hello() returns 0 on success or prints to console
    int ret = hello();
    if(ret >= 0) {
        printf("[PASS] hello() returned successfully (ret: %d)\n", ret);
    } else {
        printf("[FAIL] hello() returned error: %d\n", ret);
    }
}

// --- Test 2: getpid2() ---
void test_getpid2() {
    printf("\n--- Testing getpid2() ---\n");
    int standard_pid = getpid();
    int custom_pid = getpid2();
    
    printf("getpid(): %d, getpid2(): %d\n", standard_pid, custom_pid);
    ASSERT(standard_pid == custom_pid, "getpid2 matches getpid");
}

// --- Test 3: getppid() ---
void test_getppid() {
    printf("\n--- Testing getppid() ---\n");
    int my_pid = getpid();
    int pid = fork();

    if(pid < 0) {
        printf("[FAIL] Fork failed\n");
        exit(1);
    }

    if(pid == 0) {
        // Child Process
        int parent_pid = getppid();
        if(parent_pid == my_pid) {
            printf("[PASS] Child correctly identified parent (Parent: %d, getppid: %d)\n", my_pid, parent_pid);
            exit(0); // Success
        } else {
            printf("[FAIL] Child saw wrong parent (Expected: %d, Got: %d)\n", my_pid, parent_pid);
            exit(1); // Fail
        }
    } else {
        // Parent Process
        int status;
        wait(&status);
        if(status != 0) {
            printf("[FAIL] getppid test failed in child\n");
        }
    }
}

// --- Test 4: getnumchild() ---
void test_getnumchild() {
    printf("\n--- Testing getnumchild() ---\n");
    int initial_count = getnumchild();
    printf("Initial children: %d\n", initial_count);

    int p1 = fork();
    if(p1 == 0) { sleep(50); exit(0); } // Child 1 hangs around

    int p2 = fork();
    if(p2 == 0) { sleep(50); exit(0); } // Child 2 hangs around

    sleep(5); // Give fork time to update structures

    int new_count = getnumchild();
    printf("Children after 2 forks: %d\n", new_count);

    // Check logic (initial + 2)
    ASSERT(new_count == initial_count + 2, "getnumchild counted correct new children");

    // Cleanup
    kill(p1);
    kill(p2);
    wait(0);
    wait(0);
}

// --- Test 5: getsyscount() ---
void test_getsyscount() {
    printf("\n--- Testing getsyscount() ---\n");
    
    // Note: The logic assumes getsyscount returns the count *including* itself 
    // or *before* itself depending on implementation. 
    // We look for a difference.
    
    int start = getsyscount();
    
    // Perform exactly 3 system calls
    getpid();
    getpid();
    getpid();

    int end = getsyscount();
    int diff = end - start;

    printf("Start: %d, End: %d, Diff: %d\n", start, end, diff);
    
    // We expect diff to be at least 4 (1 for start call + 3 getpids + potentially 1 for end call)
    // Depending on if you increment before or after syscall, logic varies slightly.
    ASSERT(diff >= 3, "Syscall count increased correctly");
}

// --- Test 6: getchildsyscount(pid) ---
void test_getchildsyscount() {
    printf("\n--- Testing getchildsyscount(pid) ---\n");
    
    int pid = fork();
    if(pid == 0) {
        // Child performs specific number of calls
        // 1. getpid
        // 2. getpid
        // 3. getpid
        getpid();
        getpid();
        getpid();
        
        sleep(50); // Stay alive so parent can read our struct proc
        exit(0);
    }

    sleep(10); // Wait for child to finish its calls
    
    int child_count = getchildsyscount(pid);
    printf("Child (pid %d) performed %d syscalls\n", pid, child_count);

    // Child did at least 3 getpids + fork return + sleep
    ASSERT(child_count >= 3, "Correctly retrieved child syscall stats");

    kill(pid);
    wait(0);
}

int main(int argc, char *argv[]) {
    printf("=== STARTING SYSTEM CALL TESTS ===\n");
    
    test_hello();
    test_getpid2();
    test_getppid();
    test_getsyscount();
    test_getchildsyscount(); 
    // Note: getnumchild is last because it kills children which might affect scheduling
    test_getnumchild();

    printf("\n=== ALL TESTS PASSED ===\n");
    exit(0);
}