// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.


#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"
#include "proc.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&ftable_lock, "ftable");
  initlock(&swap_lock, "swap");
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.

void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(!r){
    r = (struct run*)evict_page();
    if(!r){
      return 0; // No free space in swap
    }
  }

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}



struct frame_info frames_table[MAX_FRAMES]; // array to hold information about each frame
char swap_space[SWAP_SIZE][PGSIZE]; // 2D array to represent swap space, each entry is a page
int swap_used[SWAP_SIZE]; // array for tracking used swap slots
int clock_hand = 0; // index for the clock algorithm
struct spinlock ftable_lock; // lock for synchronizing access to swap space
struct spinlock swap_lock; // lock for synchronizing access to swap space


int get_free_frame_inswap(){
  acquire(&swap_lock);
  for(int i = 0; i < SWAP_SIZE; i++){
    if(swap_used[i] == 0){
      swap_used[i] = 1;
      release(&swap_lock);
      return i;
    }
  }
  release(&swap_lock);
  // panic("get_free_frame_inswap: no free swap slots");
  return -1; 
}

void* evict_page(){
  acquire(&ftable_lock); // Lock the frame table
  
  int start_hand = clock_hand;
  struct frame_info *victim = 0;
  int victim_index = -1;
  pte_t *victim_pte = 0;
  int full_cycle = 0; 
  
  while(1){
    struct frame_info *f = &frames_table[clock_hand];
    if(f->allocated){
      pte_t *pte = walk(f->owner->pagetable, f->va, 0);
      if(pte && (*pte & PTE_V)){
        if(*pte & PTE_A){
          *pte &= ~PTE_A; // Clear the hardware reference bit for second chance
        } else {
          // higher value = lower priority queue
          if(victim == 0 || (f->owner->priority > victim->owner->priority) ){
            victim = f;
            victim_index = clock_hand;
            victim_pte = pte;
          }
        }
      }
    }
    clock_hand = (clock_hand + 1) % MAX_FRAMES;
    if(victim && victim->owner->priority == (NQUEUE - 1)){
      break; // Found victim in lowest priority queue, stop searching
    }
    
    if(clock_hand == start_hand){
      full_cycle++;
      if(victim || full_cycle >= 2){
        break; 
      }
    }
  }

  clock_hand = (victim_index + 1) % MAX_FRAMES; 

  if(victim == 0){
    clock_hand = 0; // Reset clock hand for next eviction attempt
    release(&ftable_lock); // Don't forget to release if returning early!
    return 0; 
  }

  // swap slot and physical address
  int swap_index = get_free_frame_inswap();
  if(swap_index == -1){         // <-- ADD THIS CHECK
    release(&ftable_lock);
    return 0; 
  }
  uint64 pa = PTE2PA(*victim_pte); // physical address of the victim page
  
  memmove(swap_space[swap_index], (char*)pa, PGSIZE);  //copy the victim page to swap space
  
  
  int flags = PTE_FLAGS(*victim_pte);
  flags &= ~PTE_V; 
  flags |= PTE_S; 
  *victim_pte = ((uint64)swap_index << 10) | flags; 
  sfence_vma();
 
  victim->owner->pages_evicted++;
  victim->owner->pages_swapped_out++;
  victim->owner->resident_pages--;

  victim->allocated = 0; 
  victim->va = 0;
  victim->owner = 0;
  victim->ref_bit = 0;
  

  release(&ftable_lock); // Safe to release now!
  return (void*)pa;
}

void free_swap_slot(int idx){
  acquire(&swap_lock);
  if(idx >=0 && idx < SWAP_SIZE){
    swap_used[idx] = 0;
  } 
  release(&swap_lock);
}

void remove_from_frame_table(pagetable_t pagetable, uint64 va) {
  acquire(&ftable_lock);
  for(int i = 0; i < MAX_FRAMES; i++) {
    if(frames_table[i].allocated && frames_table[i].va == va && frames_table[i].owner && frames_table[i].owner->pagetable == pagetable) {
      frames_table[i].allocated = 0;
      frames_table[i].owner->resident_pages--;
      break;
    }
  }
  release(&ftable_lock);
}