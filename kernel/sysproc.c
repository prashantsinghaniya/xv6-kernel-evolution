#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "vm.h"

extern struct proc proc[NPROC];
extern int disk_policy; // Defined in virtio_disk.c

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  kexit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return kfork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return kwait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int t;
  int n;

  argint(0, &n);
  argint(1, &t);
  addr = myproc()->sz;

  if(t == SBRK_EAGER || n < 0) {
    if(growproc(n) < 0) {
      return -1;
    }
  } else {
    // Lazily allocate memory for this process: increase its memory
    // size but don't allocate memory. If the processes uses the
    // memory, vmfault() will allocate it.
    if(addr + n < addr)
      return -1;
    if(addr + n > TRAPFRAME)
      return -1;
    myproc()->sz += n;
  }
  return addr;
}


uint64
sys_pause(void)
{
  int n;
  uint ticks0;

  argint(0, &n);
  if(n < 0)
    n = 0;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kkill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}


// _begin
uint64
sys_hello(void){
  printf("Hello from the kernel!\n");
  return 0;
}

uint64
sys_getpid2(void){
  return myproc()->pid;
}

uint64
sys_getppid(void){
  return kgetppid();
}

uint64
sys_getnumchild(void){
  return kgetnumchild();

} 

// sys_getsyscount returns the number of system calls made by the current process, which is stored in the syscount field of the proc structure.
uint64
sys_getsyscount(void){
  struct proc *p = myproc(); 
  return p->syscount;

}


uint64
sys_getchildsyscount(void){
  return kgetchildsyscount();
}

uint64
sys_getlevel(void){
  struct proc *p = myproc();
  int priority ;
  acquire(&p->lock);
  priority = p->priority;
  release(&p->lock);
  return priority;
}

uint64
sys_getmlfqinfo(void){
  return kgetmlfqinfo();
}

uint64
sys_getvmstats(void){
  return kgetvmstats();
}

uint64 sys_setdisksched(void) {
  int p;
  argint(0, &p);
  if(p < 0 || p > 1) return -1;
  disk_policy = p; // 0 for FCFS, 1 for SSTF
  return 0;
}

extern int raid_level; // Defined in virtio_disk.c

uint64 sys_setraid(void) {
  int level;
  argint(0, &level);
  if(level != 0 && level != 1 && level != 5) return -1;
  raid_level = level;
  return 0;
}
// _end