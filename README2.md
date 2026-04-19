# README: CS3523 Programming Assignment 01 - Extending xv6

This submission contains the modified xv6-riscv source code and user-space test programs for Programming Assignment 01. The objective was to extend the xv6 kernel by implementing custom system calls related to process management and kernel-level statistics.
## Implemented Features

The following system calls were added to the kernel and exposed to user space:

### A. Basic System Calls
* **`hello()`** (Feature A1): A basic sanity check system call that prints a greeting to the console to verify the syscall mechanism.
* **`getpid2()`** (Feature A2): Returns the process identifier (PID) of the calling process. Implemented independently of the existing getpid. It accesses myproc()->pid and returns the value.

### B. Process Hierarchy Information
* **`getppid()`** (Feature B1): Returns the Process ID (PID) of the calling process's parent. Note that the design choice made by me in this function call is that the processes whose parent is init will return -1 since all running processes will have a parent and in xv6, when a process is killed the process doesnt exit execution immediately but rather exits when a syscall is invoked. In this implementation when the childs parent has been set to killed -> 1 the function returns -1.

* **`getnumchild()`** (Feature B2): Returns the number of currently alive child processes.

### C. System Call Statistics
* **`getsyscount()`** (Feature C1/C2): Returns the total number of system calls invoked by the *current* process since its creation including itself.
* **`getchildsyscount(int pid)`** (Feature C3): Returns the system call count of a specific child process identified by `pid`.

---
## Files Modified
```
cs3523-xv6-26\
|__kernel\
    |__defs.h
    |__proc.c
    |__proc.h
    |__syscall.c
    |__syscall.h
    |__sysproc.c
    |
|__user\
    |__testfile.c
    |__user.h
    |__usys.pl
    |
|__README2.md
|__Makefile
```
## Implementations
The implementations are well coded and explained in their definations mostly in proc.c.
## Testing (`testfie.c`)

The `testfile.c` user program serves as the primary verification tool. It is implemented in /user/ to make the verification process simpler (taking idea from the usertests.c ). It exercises all implemented features in a single execution flow:

Note that testing edge cases in an uncontrolled setup is very difficult.Such as checking if my implementations are working when the child is exiting or switching etc.Thus, in an attempt to simulate such scenarios I ran this code multiple time with different delays -> yielding expected results, hence assuring the correctness of the implementations.

1.  **Initial Stats:** Verifies `getsyscount()` tracks calls immediately upon process creation.
2.  **Fork/Exec Flow:** * Forks a child (Child 0).
    * Child 0 checks `getpid()` vs `getppid()`, then performs an `exec` (running the `hello` program).
    * Parent monitors the transition and verifies `getchildsyscount()` for the child.
3.  **Child Counting:**
    * Forks multiple children (Child 1, Child 2) sequentially.
    * Uses `getnumchild()` to verify the count increments as children are created and decrements as they are waited upon.
4.  **Orphan/Adoption Logic:**
    * The test attempts to kill its own parent (the shell) to simulate orphan adoption.
    * Verifies `getppid()` behavior when the process is adopted by `init`.
5.  **Sanity Check:** Compares the standard `getpid()` against the custom `getpid2()`.

### Usage
from cs3523-xv6-26 run command "make qemu"
```bash
$ test_file
```

---
### Observation 
user programs cannot create or refer to a file whose name exceeds 14 characters.

## AI usage
Google gemini pro was used to generate this  README file. Most of the implementations are inspired from previosly defined system calls like  kkill() and kwait() implementions provided in proc.c.

---