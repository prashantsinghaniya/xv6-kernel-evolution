#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

struct cpu cpus[NCPU];

struct proc proc[NPROC];

struct queue mlfq[NQUEUE];
struct spinlock mlfq_lock;

struct proc *initproc;

int nextpid = 1;
struct spinlock pid_lock;


extern void forkret(void);
static void freeproc(struct proc *p);

extern uint total_disk_reads;
extern uint total_disk_writes;
extern uint total_disk_latency;
extern uint completed_requests;

void enqueue(int, struct proc *);
void dequeue(int);
int isEmpty(int);
extern char trampoline[]; // trampoline.S

// helps ensure that wakeups of wait()ing
// parents are not lost. helps obey the
// memory model when using p->parent.
// must be acquired before any p->lock.
struct spinlock wait_lock;

// Allocate a page for each process's kernel stack.
// Map it high in memory, followed by an invalid
// guard page.
void
proc_mapstacks(pagetable_t kpgtbl)
{
  struct proc *p;
  
  for(p = proc; p < &proc[NPROC]; p++) {
    char *pa = kalloc();
    if(pa == 0)
      panic("kalloc");
    uint64 va = KSTACK((int) (p - proc));
    kvmmap(kpgtbl, va, (uint64)pa, PGSIZE, PTE_R | PTE_W);
  }
}

// initialize the proc table.
void
procinit(void)
{
  struct proc *p;
  
  initlock(&pid_lock, "nextpid");
  initlock(&wait_lock, "wait_lock");
  initlock(&mlfq_lock, "mlfq_lock");
  for(p = proc; p < &proc[NPROC]; p++) {
      initlock(&p->lock, "proc");
      p->state = UNUSED;
      p->kstack = KSTACK((int) (p - proc));
  }
}

// Must be called with interrupts disabled,
// to prevent race with process being moved
// to a different CPU.
int
cpuid()
{
  int id = r_tp();
  return id;
}

// Return this CPU's cpu struct.
// Interrupts must be disabled.
struct cpu*
mycpu(void)
{
  int id = cpuid();
  struct cpu *c = &cpus[id];
  return c;
}

// Return the current struct proc *, or zero if none.
struct proc*
myproc(void)
{
  push_off();
  struct cpu *c = mycpu();
  struct proc *p = c->proc;
  pop_off();
  return p;
}

int
allocpid()
{
  int pid;
  
  acquire(&pid_lock);
  pid = nextpid;
  nextpid = nextpid + 1;
  release(&pid_lock);

  return pid;
}

// Look in the process table for an UNUSED proc.
// If found, initialize state required to run in the kernel,
// and return with p->lock held.
// If there are no free procs, or a memory allocation fails, return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if(p->state == UNUSED) {
      goto found;
    } else {
      release(&p->lock);
    }
  }
  return 0;

found:
  p->pid = allocpid();
  p->state = USED;
  p->syscount = 0;
  p->priority = 0;
  p->ticks_used_clevel = 0;
  for(int i = 0; i < NQUEUE; i++) {
    p->total_ticks_per_level[i] = 0;
  }
  p->no_of_times_scheduled = 0;
  p->s_syscount = 0;

  p->page_faults = 0;
  p->pages_evicted = 0;
  p->pages_swapped_in = 0;
  p->pages_swapped_out = 0;
  p->resident_pages = 0;


  // Allocate a trapframe page.
  if((p->trapframe = (struct trapframe *)kalloc()) == 0){
    freeproc(p);
    release(&p->lock);
    return 0;
  }
  if((p->swap_io_buf = kalloc()) == 0){
    // kfree((void*)p->trapframe);
    freeproc(p);
    release(&p->lock);
    return 0;
  }
  // An empty user page table.
  p->pagetable = proc_pagetable(p);
  if(p->pagetable == 0){
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  // Set up new context to start executing at forkret,
  // which returns to user space.
  memset(&p->context, 0, sizeof(p->context));
  p->context.ra = (uint64)forkret;
  p->context.sp = p->kstack + PGSIZE;


  return p;
}

// free a proc structure and the data hanging from it,
// including user pages.
// p->lock must be held.
static void
freeproc(struct proc *p)
{
  if(p->trapframe)
    kfree((void*)p->trapframe);
  p->trapframe = 0;
  if(p->pagetable)
    proc_freepagetable(p->pagetable, p->sz);
  p->pagetable = 0;
  p->sz = 0;
  p->pid = 0;
  p->parent = 0;
  p->name[0] = 0;
  p->chan = 0;
  p->killed = 0;
  p->xstate = 0;
  p->syscount = 0;
  p->state = UNUSED;
  if(p->swap_io_buf) kfree(p->swap_io_buf);
  p->swap_io_buf = 0;
}

// Create a user page table for a given process, with no user memory,
// but with trampoline and trapframe pages.
pagetable_t
proc_pagetable(struct proc *p)
{
  pagetable_t pagetable;

  // An empty page table.
  pagetable = uvmcreate();
  if(pagetable == 0)
    return 0;

  // map the trampoline code (for system call return)
  // at the highest user virtual address.
  // only the supervisor uses it, on the way
  // to/from user space, so not PTE_U.
  if(mappages(pagetable, TRAMPOLINE, PGSIZE,
              (uint64)trampoline, PTE_R | PTE_X) < 0){
    uvmfree(pagetable, 0);
    return 0;
  }

  // map the trapframe page just below the trampoline page, for
  // trampoline.S.
  if(mappages(pagetable, TRAPFRAME, PGSIZE,
              (uint64)(p->trapframe), PTE_R | PTE_W) < 0){
    uvmunmap(pagetable, TRAMPOLINE, 1, 0);
    uvmfree(pagetable, 0);
    return 0;
  }

  return pagetable;
}

// Free a process's page table, and free the
// physical memory it refers to.
void
proc_freepagetable(pagetable_t pagetable, uint64 sz)
{
  uvmunmap(pagetable, TRAMPOLINE, 1, 0);
  uvmunmap(pagetable, TRAPFRAME, 1, 0);
  uvmfree(pagetable, sz);
}

// Set up first user process.
void
userinit(void)
{
  struct proc *p;

  p = allocproc();
  initproc = p;
  
  p->cwd = namei("/");

  p->state = RUNNABLE;
  enqueue(0, p);

  release(&p->lock);
}

// Grow or shrink user memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint64 sz;
  struct proc *p = myproc();

  sz = p->sz;
  if(n > 0){
    if(sz + n > TRAPFRAME) {
      return -1;
    }
    if((sz = uvmalloc(p->pagetable, sz, sz + n, PTE_W)) == 0) {
      return -1;
    }
  } else if(n < 0){
    sz = uvmdealloc(p->pagetable, sz, sz + n);
  }
  p->sz = sz;
  return 0;
}

// Create a new process, copying the parent.
// Sets up child kernel stack to return as if from fork() system call.
int
kfork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *p = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  release(&np->lock);
  // Copy user memory from parent to child.
  if(uvmcopy(p->pagetable, np->pagetable, p->sz, np) < 0){
    acquire(&np->lock);
    freeproc(np);
    release(&np->lock);
    return -1;
  }
  np->sz = p->sz;

  // copy saved user registers.
  *(np->trapframe) = *(p->trapframe);

  // Cause fork to return 0 in the child.
  np->trapframe->a0 = 0;

  // increment reference counts on open file descriptors.
  for(i = 0; i < NOFILE; i++)
    if(p->ofile[i])
      np->ofile[i] = filedup(p->ofile[i]);
  np->cwd = idup(p->cwd);

  safestrcpy(np->name, p->name, sizeof(p->name));

  pid = np->pid;

  // release(&np->lock);

  acquire(&wait_lock);
  np->parent = p;
  release(&wait_lock);

  acquire(&np->lock);
  np->state = RUNNABLE;
  enqueue(np->priority, np);
  release(&np->lock);

  acquire(&np->lock);
  np->syscount = 0;
  release(&np->lock);

  return pid;
}

// Pass p's abandoned children to init.
// Caller must hold wait_lock.
void
reparent(struct proc *p)
{
  struct proc *pp;

  for(pp = proc; pp < &proc[NPROC]; pp++){
    if(pp->parent == p){
      pp->parent = initproc;
      wakeup(initproc);
    }
  }
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait().
void
kexit(int status)
{
  struct proc *p = myproc();

  if(p == initproc)
    panic("init exiting");

  // Close all open files.
  for(int fd = 0; fd < NOFILE; fd++){
    if(p->ofile[fd]){
      struct file *f = p->ofile[fd];
      fileclose(f);
      p->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(p->cwd);
  end_op();
  p->cwd = 0;

  acquire(&wait_lock);

  // Give any children to init.
  reparent(p);

  // Parent might be sleeping in wait().
  wakeup(p->parent);
  
  acquire(&p->lock);

  p->xstate = status;
  p->state = ZOMBIE;

  release(&wait_lock);

  // Jump into the scheduler, never to return.
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
kwait(uint64 addr)
{
  struct proc *pp;
  int havekids, pid;
  struct proc *p = myproc();

  acquire(&wait_lock);

  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(pp = proc; pp < &proc[NPROC]; pp++){
      if(pp->parent == p){
        // make sure the child isn't still in exit() or swtch().
        acquire(&pp->lock);

        havekids = 1;
        if(pp->state == ZOMBIE){
          // Found one.
          pid = pp->pid;
          if(addr != 0 && copyout(p->pagetable, addr, (char *)&pp->xstate,
                                  sizeof(pp->xstate)) < 0) {
            release(&pp->lock);
            release(&wait_lock);
            return -1;
          }
          freeproc(pp);
          release(&pp->lock);
          release(&wait_lock);
          return pid;
        }
        release(&pp->lock);
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || killed(p)){
      release(&wait_lock);
      return -1;
    }
    
    // Wait for a child to exit.
    sleep(p, &wait_lock);  //DOC: wait-sleep
  }
}

// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run.
//  - swtch to start running that process.
//  - eventually that process transfers control
//    via swtch back to the scheduler.
// void
// scheduler(void)
// {
//   struct proc *p;
//   struct cpu *c = mycpu();

//   c->proc = 0;
//   for(;;){
//     // The most recent process to run may have had interrupts
//     // turned off; enable them to avoid a deadlock if all
//     // processes are waiting. Then turn them back off
//     // to avoid a possible race between an interrupt
//     // and wfi.
//     intr_on();
//     intr_off();

//     int found = 0;
//     for(p = proc; p < &proc[NPROC]; p++) {
//       acquire(&p->lock);
//       if(p->state == RUNNABLE) {
//         // Switch to chosen process.  It is the process's job
//         // to release its lock and then reacquire it
//         // before jumping back to us.
//         p->state = RUNNING;
//         c->proc = p;
//         swtch(&c->context, &p->context);

//         // Process is done running for now.
//         // It should have changed its p->state before coming back.
//         c->proc = 0;
//         found = 1;
//       }
//       release(&p->lock);
//     }
//     if(found == 0) {
//       // nothing to run; stop running on this core until an interrupt.
//       asm volatile("wfi");
//     }
//   }
// }

// Switch to scheduler.  Must hold only p->lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->noff, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&p->lock))
    panic("sched p->lock");
  if(mycpu()->noff != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched RUNNING");
  if(intr_get())
    panic("sched interruptible");

  intena = mycpu()->intena;
  swtch(&p->context, &mycpu()->context);
  mycpu()->intena = intena;
}

void 
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();

  c->proc = 0;
  for(;;){
    intr_on();
    intr_off();

    int found = 0;
    for(int level =0; level < NQUEUE; level++){
      acquire(&mlfq_lock);
      if(!isEmpty(level)){
        struct queue *q = &mlfq[level];
        p = q->proc[q->head];
        q->proc[q->head] = 0;
        q->head = (q->head + 1) % NPROC;
        release(&mlfq_lock);

        acquire(&p->lock);
        if(p->state == RUNNABLE){
          // Switch to chosen process.  It is the process's job
          // to release its lock and then reacquire it
          // before jumping back to us.
          p->state = RUNNING;
          c->proc = p;
          p->no_of_times_scheduled++;
          p->ticks_used_clevel = 0;
          p->s_syscount = p->syscount;
          swtch(&c->context, &p->context);

          // Process is done running for now.
          // It should have changed its p->state before coming back.
          c->proc = 0;
          found = 1;
        }
        release(&p->lock);
        if(found == 1){
          break;
        }
      }
      else
      release(&mlfq_lock);
    }
    if(found == 0){
      asm volatile("wfi");
    }

  }
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  struct proc *p = myproc();
  acquire(&p->lock);
  p->state = RUNNABLE;
  enqueue(p->priority, p);
  sched();
  release(&p->lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch to forkret.
void
forkret(void)
{
  extern char userret[];
  static int first = 1;
  struct proc *p = myproc();

  // Still holding p->lock from scheduler.
  release(&p->lock);

  if (first) {
    // File system initialization must be run in the context of a
    // regular process (e.g., because it calls sleep), and thus cannot
    // be run from main().
    fsinit(ROOTDEV);

    first = 0;
    // ensure other cores see first=0.
    __sync_synchronize();

    // We can invoke kexec() now that file system is initialized.
    // Put the return value (argc) of kexec into a0.
    p->trapframe->a0 = kexec("/init", (char *[]){ "/init", 0 });
    if (p->trapframe->a0 == -1) {
      panic("exec");
    }
  }

  // return to user space, mimicing usertrap()'s return.
  prepare_return();
  uint64 satp = MAKE_SATP(p->pagetable);
  uint64 trampoline_userret = TRAMPOLINE + (userret - trampoline);
  ((void (*)(uint64))trampoline_userret)(satp);
}

// Sleep on channel chan, releasing condition lock lk.
// Re-acquires lk when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  
  // Must acquire p->lock in order to
  // change p->state and then call sched.
  // Once we hold p->lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup locks p->lock),
  // so it's okay to release lk.

  acquire(&p->lock);  //DOC: sleeplock1
  release(lk);

  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  release(&p->lock);
  acquire(lk);
}

// Wake up all processes sleeping on channel chan.
// Caller should hold the condition lock.
void
wakeup(void *chan)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++) {
    if(p != myproc()){
      acquire(&p->lock);
      if(p->state == SLEEPING && p->chan == chan) {
        p->state = RUNNABLE;
        enqueue(p->priority, p);
      }
      release(&p->lock);
    }
  }
}

void priority_boost(void){
  // struct proc *p;
  // for(p = proc; p < &proc[NPROC]; p++){
  //   acquire(&p->lock);
  //   if(p->state != UNUSED){
  //     p->priority = 0;
  //   }
  //   release(&p->lock);
  // }
  acquire(&mlfq_lock);
  for(int level = 1; level < NQUEUE; level++){
    struct queue *q = &mlfq[level];
    while(!isEmpty(level)){
      struct proc *p = q->proc[q->head];
      q->proc[q->head] = 0;
      q->head = (q->head + 1) % NPROC;
      // acquire(&p->lock);
      if(p->state == RUNNABLE){
        p->priority = 0;
        mlfq[0].proc[mlfq[0].tail] = p;
        mlfq[0].tail = (mlfq[0].tail + 1) % NPROC;
      }
      // release(&p->lock);
    }
  }
   release(&mlfq_lock);
}

// Kill the process with the given pid.
// The victim won't exit until it tries to return
// to user space (see usertrap() in trap.c).
int
kkill(int pid)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++){
    acquire(&p->lock);
    if(p->pid == pid){
      p->killed = 1;
      if(p->state == SLEEPING){
        // Wake process from sleep().
        p->state = RUNNABLE;
        enqueue(p->priority, p);
      }
      release(&p->lock);
      return 0;
    }
    release(&p->lock);
  }
  return -1;
}

void
setkilled(struct proc *p)
{
  acquire(&p->lock);
  p->killed = 1;
  release(&p->lock);
}

int
killed(struct proc *p)
{
  int k;
  
  acquire(&p->lock);
  k = p->killed;
  release(&p->lock);
  return k;
}

// Copy to either a user address, or kernel address,
// depending on usr_dst.
// Returns 0 on success, -1 on error.
int
either_copyout(int user_dst, uint64 dst, void *src, uint64 len)
{
  struct proc *p = myproc();
  if(user_dst){
    return copyout(p->pagetable, dst, src, len);
  } else {
    memmove((char *)dst, src, len);
    return 0;
  }
}

// Copy from either a user address, or kernel address,
// depending on usr_src.
// Returns 0 on success, -1 on error.
int
either_copyin(void *dst, int user_src, uint64 src, uint64 len)
{
  struct proc *p = myproc();
  if(user_src){
    return copyin(p->pagetable, dst, src, len);
  } else {
    memmove(dst, (char*)src, len);
    return 0;
  }
}

// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [USED]      "used",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  struct proc *p;
  char *state;

  printf("\n");
  for(p = proc; p < &proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    // printf("%d %s %s", p->pid, state, p->name);
    // NEW: Print priority and schedule count
    printf("PID: %d | State: %s | Name: %s | Prio: %d | Scheds: %d\n", 
            p->pid, state, p->name, p->priority, p->no_of_times_scheduled);
    printf("\n");
  }
}

uint64
kgetppid(void){
  struct proc *pp = myproc()->parent;
  int ppid = -1;
  acquire(&wait_lock);
  //acquire(&pp->lock);
  if(killed(pp) ){
    ppid = -1;
  }else{
    ppid = pp->pid;
  }
  //release(&pp->lock);
  release(&wait_lock);
  if(ppid == 0){
    return -1; // If the process has no parent, return 0
  }
  return ppid;
}

uint64
kgetnumchild(void){
  struct proc *p = myproc();
  struct proc *pp;
  int count = 0;
  // We need to acquire the wait_lock to ensure that we don't miss any wakeups of wait()ing parents, and to obey the memory model when using p->parent.
  acquire(&wait_lock);
  for(pp = proc; pp < &proc[NPROC]; pp++){
    if(pp->parent == p){
      acquire(&pp->lock);
      if(pp -> killed != 1 && pp -> state != ZOMBIE){ // Only count children that are not killed or zombies
        count++;
      }
      release(&pp->lock);
    }
  }
  release(&wait_lock);
  return count;
}

uint64
kgetchildsyscount(void){
  struct proc *p = myproc();
  int pid;
  int syscount = -1;
  argint(0, &pid);
  acquire(&wait_lock);
  for(struct proc *pp = proc; pp < &proc[NPROC]; pp++){
    if(pp->parent == p ){
      acquire(&pp->lock);
      if(pp->pid == pid){
        syscount = pp->syscount;
        release(&pp->lock);
        break;
      }
      release(&pp->lock);
    }
  }
  release(&wait_lock);
  return syscount;
}

void enqueue(int q, struct proc *p){
  acquire(&mlfq_lock);
  mlfq[q].proc[mlfq[q].tail] = p;
  mlfq[q].tail = (mlfq[q].tail + 1) % NPROC;
  release(&mlfq_lock);
}

void dequeue(int q){
  acquire(&mlfq_lock);
  if(isEmpty(q)){
    release(&mlfq_lock);
    return;
  }
  mlfq[q].proc[mlfq[q].head] = 0;
  mlfq[q].head = (mlfq[q].head + 1) % NPROC;
  release(&mlfq_lock);
}

int isEmpty(int q){
  int empty = 0;
  if(mlfq[q].head == mlfq[q].tail){
    empty = 1;
  }
  return empty;
}

uint64
kgetmlfqinfo(void){
  int pid;
  argint(0, &pid);
  uint64 info_addr;
  argaddr(1, &info_addr);
  struct mlfqinfo info ;
  struct proc * p;
  int found = 0;
  for( p = proc; p < &proc[NPROC]; p++){
    acquire(&p->lock);
    if(pid == p->pid){
      info.level = p->priority;
      for(int i = 0; i < NQUEUE; i++){
        info.ticks[i] = p->total_ticks_per_level[i];
      }
      info.times_scheduled = p->no_of_times_scheduled;
      info.total_syscalls = p->syscount;
      found = 1;
    }
    release(&p->lock);
  }
  if(!found){
    return -1;
  }
  if(copyout(myproc()->pagetable, info_addr, (char *)&info, sizeof(info)) < 0){
    return -1;
  }
  return 0;
}

uint64
kgetvmstats(void){
  int pid;
  uint64 stats_addr;
  struct proc *p;
  struct vmstats local_stats;
  int found = 0;

  // Call them directly without the if() check, since they return void
  argint(0, &pid);
  argaddr(1, &stats_addr);

  for(p = proc; p < &proc[NPROC]; p++){
    acquire(&p->lock);
    if(pid == p->pid && p->state != UNUSED){
      local_stats.page_faults = p->page_faults;
      local_stats.pages_evicted = p->pages_evicted;
      local_stats.pages_swapped_in = p->pages_swapped_in;
      local_stats.pages_swapped_out = p->pages_swapped_out;
      local_stats.resident_pages = p->resident_pages;
      found = 1;
      release(&p->lock);
      break;
    }
    release(&p->lock);
  }
  
  if(!found){
    return -1;
  }
  local_stats.total_disk_reads = total_disk_reads;
  local_stats.total_disk_writes = total_disk_writes;
  if(completed_requests > 0){
    local_stats.avg_disk_latency = total_disk_latency / completed_requests;
  } else {
    local_stats.avg_disk_latency = 0;
  }
  if(copyout(myproc()->pagetable, stats_addr, (char *)&local_stats, sizeof(local_stats)) < 0){
    return -1;
  }

  return 0;
}