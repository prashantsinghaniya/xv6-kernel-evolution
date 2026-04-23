// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "proc.h"
#include "fs.h"
#include "buf.h"

#define BLKS_PER_PAGE    (PGSIZE / BSIZE)          // 4096 / 1024 = 4
#define SWAP_START_BLOCK (FSSIZE / 2)              // = 100000; MUST match vm.c and virtio_disk.c

void freerange(void *pa_start, void *pa_end);

extern char end[];   // first address after kernel (kernel.ld)

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run     *freelist;
} kmem;

void
kinit(void)
{
  initlock(&kmem.lock,   "kmem");
  initlock(&ftable_lock, "ftable");
  initlock(&swap_lock,   "swap");
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p = (char*)PGROUNDUP((uint64)pa_start);
  for (; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory at pa.
void
kfree(void *pa)
{
  struct run *r;

  if (((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  memset(pa, 1, PGSIZE);   // fill with junk to catch dangling refs

  r = (struct run*)pa;
  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns 0 if memory is exhausted (including swap).
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if (r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if (!r) {
    // Physical memory exhausted — try to evict a page to swap
    r = (struct run*)evict_page();
    if (!r)
      return 0;   // no swap space left either
  }

  if (r)
    memset((char*)r, 5, PGSIZE);   // fill with junk
  return (void*)r;
}

/* ------------------------------------------------------------------
 * Frame table and swap bookkeeping
 * ------------------------------------------------------------------ */
struct frame_info frames_table[MAX_FRAMES];
int              swap_used[SWAP_SIZE];
int              clock_hand = 0;
struct spinlock  ftable_lock;
struct spinlock  swap_lock;

// Return the index of a free swap slot, or -1 if swap is full.
int
get_free_frame_inswap(void)
{
  acquire(&swap_lock);
  for (int i = 0; i < SWAP_SIZE; i++) {
    if (swap_used[i] == 0) {
      swap_used[i] = 1;
      release(&swap_lock);
      return i;
    }
  }
  release(&swap_lock);
  return -1;
}

/* ------------------------------------------------------------------
 * evict_page
 *
 * Select a victim page using a priority-aware clock algorithm:
 *  - prefer to evict pages from low-priority processes
 *  - give a "second chance" to recently-referenced pages (PTE_A)
 *
 * Writes the victim's contents to disk via virtio_disk_rw, updates
 * the PTE to mark it swapped-out (PTE_S), and returns the now-free
 * physical frame so kalloc() can reuse it.
 * ------------------------------------------------------------------ */
void *
evict_page(void)
{
  acquire(&ftable_lock);

  int start_hand = clock_hand;
  struct frame_info *victim     = 0;
  int                victim_idx = -1;
  pte_t             *victim_pte = 0;
  int                full_cycle = 0;

  while (1) {
    struct frame_info *f = &frames_table[clock_hand];

    if (f->allocated) {
      pte_t *pte = walk(f->owner->pagetable, f->va, 0);
      if (pte && (*pte & PTE_V)) {
        if (*pte & PTE_A) {
          // Recently accessed — give second chance, clear bit
          *pte &= ~PTE_A;
        } else {
          // Candidate: prefer lower-priority (higher numeric) processes
          if (victim == 0 ||
              f->owner->priority > victim->owner->priority) {
            victim     = f;
            victim_idx = clock_hand;
            victim_pte = pte;
          }
        }
      }
    }

    clock_hand = (clock_hand + 1) % MAX_FRAMES;

    // Stop early if we have a victim from the lowest-priority queue
    if (victim && victim->owner->priority == (NQUEUE - 1))
      break;

    if (clock_hand == start_hand) {
      full_cycle++;
      if (victim || full_cycle >= 2)
        break;
    }
  }

  // Advance clock past the victim for next eviction
  clock_hand = (victim_idx >= 0) ? (victim_idx + 1) % MAX_FRAMES : 0;

  if (victim == 0) {
    release(&ftable_lock);
    return 0;
  }

  // Allocate a swap slot
  int swap_index = get_free_frame_inswap();
  if (swap_index == -1) {
    release(&ftable_lock);
    return 0;
  }

  uint64 pa = PTE2PA(*victim_pte);

  // Mark PTE as swapped-out BEFORE releasing ftable_lock
  int flags = PTE_FLAGS(*victim_pte);
  flags &= ~PTE_V;
  flags |=  PTE_S;
  *victim_pte = ((uint64)swap_index << 10) | flags;
  sfence_vma();

  victim->owner->pages_evicted++;
  victim->owner->pages_swapped_out++;
  victim->owner->resident_pages--;

  victim->allocated = 0;
  victim->va        = 0;
  victim->owner     = 0;
  victim->ref_bit   = 0;

  release(&ftable_lock);

  // Write the evicted page to disk (4 blocks of 1024 bytes each)
  // blockno encodes the swap slot; virtio_disk_rw applies RAID mapping.
  uint start_block = SWAP_START_BLOCK + (swap_index * BLKS_PER_PAGE);
  struct buf *b    = (struct buf*)myproc()->swap_io_buf;

  for (int i = 0; i < BLKS_PER_PAGE; i++) {
    memset(b, 0, sizeof(struct buf));
    b->blockno = start_block + i;
    b->dev     = ROOTDEV;
    memmove(b->data, (char*)pa + (i * BSIZE), BSIZE);
    virtio_disk_rw(b, 1);   // write block to disk (RAID mapping applied inside)
  }

  return (void*)pa;   // caller (kalloc) will reuse this frame
}

void
free_swap_slot(int idx)
{
  acquire(&swap_lock);
  if (idx >= 0 && idx < SWAP_SIZE)
    swap_used[idx] = 0;
  release(&swap_lock);
}

void
remove_from_frame_table(pagetable_t pagetable, uint64 va)
{
  acquire(&ftable_lock);
  for (int i = 0; i < MAX_FRAMES; i++) {
    if (frames_table[i].allocated &&
        frames_table[i].va == va  &&
        frames_table[i].owner != 0 &&
        frames_table[i].owner->pagetable == pagetable) {
      frames_table[i].allocated = 0;
      frames_table[i].owner->resident_pages--;
      break;
    }
  }
  release(&ftable_lock);
}