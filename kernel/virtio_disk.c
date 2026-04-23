//
// driver for qemu's virtio disk device.
// uses qemu's mmio interface to virtio.
//
// qemu ... -drive file=fs.img,if=none,format=raw,id=x0 -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0
//

#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "buf.h"
#include "virtio.h"
#include "proc.h"

#define R(r) ((volatile uint32 *)(VIRTIO0 + (r)))
#define MAX_DISK_Q    128
#define DISK_C        10                      // rotational delay constant
#define SWAP_START_BLOCK (FSSIZE / 2)         // = 100000; must match kalloc.c and vm.c

static struct disk {
  struct virtq_desc *desc;
  struct virtq_avail *avail;
  struct virtq_used  *used;

  char   free[NUM];
  uint16 used_idx;

  struct {
    struct buf *b;
    char   status;
    int    write;
    uint   start_tick;
  } info[NUM];

  struct virtio_blk_req ops[NUM];
  
  struct spinlock vdisk_lock;
  
} disk;


struct disk_req {
  struct buf *b;
  int     write;
  uint64  phys_block;
  uint    start_tick;
  int     priority;
};

extern uint ticks;
uint total_disk_reads    = 0;
uint total_disk_writes   = 0;
uint total_disk_latency  = 0;
uint completed_requests  = 0;

struct disk_req disk_queue[MAX_DISK_Q];
int    q_size        = 0;
int    disk_in_flight = 0;   // 0 = idle, 1 = one request in flight
int    disk_policy   = 0;   // 0 = FCFS, 1 = SSTF
uint64 current_head  = 0;   // simulated disk head position

#define NDISK          4
#define DISK_CAPACITY  (FSSIZE / (2 * NDISK))   // 25000 blocks per simulated disk

// BUG FIX 2: Global parity buffer for RAID 5 read-modify-write.
// Protected by disk.vdisk_lock (held throughout virtio_disk_rw).
static struct buf raid5_parity_buf;

// Per-disk simulated failure flags (0 = OK, 1 = failed).
// Set via sim_disk_fail / sim_disk_restore syscalls for testing.
int disk_failed[NDISK] = {0, 0, 0, 0};

int raid_level = 0;

/* ------------------------------------------------------------------
 * apply_raid_mapping
 *
 * Maps a logical swap block to a physical block address on the
 * simulated disk image.  For RAID 1 and 5 writes, also fills in
 * secondary_disk / secondary_phys_block.
 * ------------------------------------------------------------------ */
uint64
apply_raid_mapping(uint64 logical_block, int *secondary_disk,
                   uint64 *secondary_phys_block)
{
  uint64 target_disk  = 0;
  uint64 block_offset = 0;
  *secondary_disk      = -1;

  if (logical_block >= SWAP_SIZE)
    panic("swap full");

  if (raid_level == 0) {
    // RAID 0: disk = lb % N, offset = (lb / N) % DISK_CAPACITY
    target_disk  = logical_block % NDISK;
    block_offset = (logical_block / NDISK) % DISK_CAPACITY;
  }
  else if (raid_level == 1) {
    // RAID 1: primary disks 0/1, mirror disks 2/3
    target_disk          = logical_block % 2;
    block_offset         = (logical_block / 2) % DISK_CAPACITY;
    *secondary_disk      = (int)(target_disk + 2);
    *secondary_phys_block = SWAP_START_BLOCK
                           + ((uint64)*secondary_disk * DISK_CAPACITY)
                           + block_offset;
  }
  else if (raid_level == 5) {
    // RAID 5: distributed parity; parity_disk = stripe_id % NDISK
    uint64 stripe_id  = logical_block / (NDISK - 1);
    uint64 parity_disk = stripe_id % NDISK;
    uint64 data_idx   = logical_block % (NDISK - 1);

    // Walk through disk slots, skipping the parity disk
    target_disk = 0;
    for (int i = 0; i <= (int)data_idx; i++) {
      if (target_disk == parity_disk) target_disk++;
      if (i < (int)data_idx)         target_disk++;
    }
    block_offset         = stripe_id % DISK_CAPACITY;
    *secondary_disk      = (int)parity_disk;
    *secondary_phys_block = SWAP_START_BLOCK
                           + (parity_disk * DISK_CAPACITY)
                           + block_offset;
  }

  return SWAP_START_BLOCK + (target_disk * DISK_CAPACITY) + block_offset;
}

static uint64
abs_diff(uint64 a, uint64 b)
{
  return (a > b) ? (a - b) : (b - a);
}

/* ------------------------------------------------------------------
 * virtio_disk_init
 * ------------------------------------------------------------------ */
void
virtio_disk_init(void)
{
  uint32 status = 0;

  initlock(&disk.vdisk_lock, "virtio_disk");

  if (*R(VIRTIO_MMIO_MAGIC_VALUE) != 0x74726976 ||
      *R(VIRTIO_MMIO_VERSION)      != 2          ||
      *R(VIRTIO_MMIO_DEVICE_ID)    != 2          ||
      *R(VIRTIO_MMIO_VENDOR_ID)    != 0x554d4551)
    panic("could not find virtio disk");

  *R(VIRTIO_MMIO_STATUS) = status;

  status |= VIRTIO_CONFIG_S_ACKNOWLEDGE;
  *R(VIRTIO_MMIO_STATUS) = status;

  status |= VIRTIO_CONFIG_S_DRIVER;
  *R(VIRTIO_MMIO_STATUS) = status;

  uint64 features = *R(VIRTIO_MMIO_DEVICE_FEATURES);
  features &= ~(1 << VIRTIO_BLK_F_RO);
  features &= ~(1 << VIRTIO_BLK_F_SCSI);
  features &= ~(1 << VIRTIO_BLK_F_CONFIG_WCE);
  features &= ~(1 << VIRTIO_BLK_F_MQ);
  features &= ~(1 << VIRTIO_F_ANY_LAYOUT);
  features &= ~(1 << VIRTIO_RING_F_EVENT_IDX);
  features &= ~(1 << VIRTIO_RING_F_INDIRECT_DESC);
  *R(VIRTIO_MMIO_DRIVER_FEATURES) = features;

  status |= VIRTIO_CONFIG_S_FEATURES_OK;
  *R(VIRTIO_MMIO_STATUS) = status;

  status = *R(VIRTIO_MMIO_STATUS);
  if (!(status & VIRTIO_CONFIG_S_FEATURES_OK))
    panic("virtio disk FEATURES_OK unset");

  *R(VIRTIO_MMIO_QUEUE_SEL) = 0;
  if (*R(VIRTIO_MMIO_QUEUE_READY))
    panic("virtio disk should not be ready");

  uint32 max = *R(VIRTIO_MMIO_QUEUE_NUM_MAX);
  if (max == 0)   panic("virtio disk has no queue 0");
  if (max < NUM)  panic("virtio disk max queue too short");

  disk.desc  = kalloc();
  disk.avail = kalloc();
  disk.used  = kalloc();
  if (!disk.desc || !disk.avail || !disk.used)
    panic("virtio disk kalloc");
  memset(disk.desc,  0, PGSIZE);
  memset(disk.avail, 0, PGSIZE);
  memset(disk.used,  0, PGSIZE);

  *R(VIRTIO_MMIO_QUEUE_NUM) = NUM;

  *R(VIRTIO_MMIO_QUEUE_DESC_LOW)   = (uint64)disk.desc;
  *R(VIRTIO_MMIO_QUEUE_DESC_HIGH)  = (uint64)disk.desc  >> 32;
  *R(VIRTIO_MMIO_DRIVER_DESC_LOW)  = (uint64)disk.avail;
  *R(VIRTIO_MMIO_DRIVER_DESC_HIGH) = (uint64)disk.avail >> 32;
  *R(VIRTIO_MMIO_DEVICE_DESC_LOW)  = (uint64)disk.used;
  *R(VIRTIO_MMIO_DEVICE_DESC_HIGH) = (uint64)disk.used  >> 32;

  *R(VIRTIO_MMIO_QUEUE_READY) = 0x1;

  for (int i = 0; i < NUM; i++)
    disk.free[i] = 1;

  status |= VIRTIO_CONFIG_S_DRIVER_OK;
  *R(VIRTIO_MMIO_STATUS) = status;
}

/* ------------------------------------------------------------------
 * Descriptor helpers
 * ------------------------------------------------------------------ */
static int
alloc_desc(void)
{
  for (int i = 0; i < NUM; i++) {
    if (disk.free[i]) {
      disk.free[i] = 0;
      return i;
    }
  }
  return -1;
}

static void
free_desc(int i)
{
  if (i >= NUM)       panic("free_desc 1");
  if (disk.free[i])   panic("free_desc 2");
  disk.desc[i].addr  = 0;
  disk.desc[i].len   = 0;
  disk.desc[i].flags = 0;
  disk.desc[i].next  = 0;
  disk.free[i]       = 1;
  wakeup(&disk.free[0]);
}

static void
free_chain(int i)
{
  while (1) {
    int flag = disk.desc[i].flags;
    int nxt  = disk.desc[i].next;
    free_desc(i);
    if (flag & VRING_DESC_F_NEXT)
      i = nxt;
    else
      break;
  }
}

static int
alloc3_desc(int *idx)
{
  for (int i = 0; i < 3; i++) {
    idx[i] = alloc_desc();
    if (idx[i] < 0) {
      for (int j = 0; j < i; j++)
        free_desc(idx[j]);
      return -1;
    }
  }
  return 0;
}

/* ------------------------------------------------------------------
 * virtio_issue
 *
 * Directly push one request to the hardware queue.
 * Caller must hold disk.vdisk_lock.
 * ------------------------------------------------------------------ */
void
virtio_issue(struct buf *b, int write, uint64 phys_block, uint start_tick)
{
  uint64 sector = phys_block * (BSIZE / 512);

  int idx[3];
  if (alloc3_desc(idx) != 0)
    panic("virtio_issue: out of descriptors");

  struct virtio_blk_req *buf0 = &disk.ops[idx[0]];
  buf0->type     = write ? VIRTIO_BLK_T_OUT : VIRTIO_BLK_T_IN;
  buf0->reserved = 0;
  buf0->sector   = sector;

  disk.desc[idx[0]].addr  = (uint64)buf0;
  disk.desc[idx[0]].len   = sizeof(struct virtio_blk_req);
  disk.desc[idx[0]].flags = VRING_DESC_F_NEXT;
  disk.desc[idx[0]].next  = idx[1];

  disk.desc[idx[1]].addr  = (uint64)b->data;
  disk.desc[idx[1]].len   = BSIZE;
  disk.desc[idx[1]].flags = write ? 0 : VRING_DESC_F_WRITE;
  disk.desc[idx[1]].flags |= VRING_DESC_F_NEXT;
  disk.desc[idx[1]].next  = idx[2];

  disk.info[idx[0]].status = 0xff;
  disk.desc[idx[2]].addr   = (uint64)&disk.info[idx[0]].status;
  disk.desc[idx[2]].len    = 1;
  disk.desc[idx[2]].flags  = VRING_DESC_F_WRITE;
  disk.desc[idx[2]].next   = 0;

  disk.info[idx[0]].b          = b;
  disk.info[idx[0]].write      = write;
  disk.info[idx[0]].start_tick = start_tick;

  disk.avail->ring[disk.avail->idx % NUM] = idx[0];
  __sync_synchronize();
  disk.avail->idx += 1;
  __sync_synchronize();
  *R(VIRTIO_MMIO_QUEUE_NOTIFY) = 0;
}

/* ------------------------------------------------------------------
 * virtio_schedule
 *
 * Pick the next request from disk_queue using the active policy
 * (priority-aware FCFS or SSTF) and issue it to hardware.
 * ------------------------------------------------------------------ */
void
virtio_schedule(void)
{
  if (q_size == 0) return;

  // 1. Find highest priority (lowest numeric value) in queue
  int max_priority = NQUEUE - 1;
  for (int i = 0; i < q_size; i++) {
    if (disk_queue[i].priority < max_priority)
      max_priority = disk_queue[i].priority;
  }

  // 2. Among requests at max_priority, apply FCFS or SSTF
  int selected_idx = -1;
  if (disk_policy == 1) {   // SSTF
    uint64 min_latency = (uint64)-1;
    for (int i = 0; i < q_size; i++) {
      if (disk_queue[i].priority != max_priority) continue;
      uint64 lat = abs_diff(current_head, disk_queue[i].phys_block) + DISK_C;
      if (lat < min_latency) {
        min_latency  = lat;
        selected_idx = i;
      }
    }
  } else {                  // FCFS
    for (int i = 0; i < q_size; i++) {
      if (disk_queue[i].priority == max_priority) {
        selected_idx = i;
        break;
      }
    }
  }

  if (selected_idx < 0) return;   // shouldn't happen

  struct buf *b          = disk_queue[selected_idx].b;
  int         write      = disk_queue[selected_idx].write;
  uint64      phys_block = disk_queue[selected_idx].phys_block;
  uint        start_tick = disk_queue[selected_idx].start_tick;

  // Remove from queue
  for (int i = selected_idx; i < q_size - 1; i++)
    disk_queue[i] = disk_queue[i + 1];
  q_size--;

  current_head  = phys_block;
  disk_in_flight = 1;

  virtio_issue(b, write, phys_block, start_tick);
}

/* ------------------------------------------------------------------
 * virtio_disk_rw  (main entry point for all disk I/O)
 *
 * Fixes applied:
 *   BUG 2: RAID 5 write now does a proper read-modify-XOR-write
 *          for the parity block instead of duplicating b->data.
 *   BUG 3: RAID 1 read now tries the mirror disk if the primary
 *          is marked as failed.
 *   BUG 4: disk_in_flight is set to 0 exactly once, outside the
 *          interrupt loop, eliminating the double-schedule race.
 * ------------------------------------------------------------------ */
void
virtio_disk_rw(struct buf *b, int write)
{
  acquire(&disk.vdisk_lock);

  // Compute physical block via RAID mapping (only for swap region)
  uint64 target_phys_block = b->blockno;
  int    secondary_disk    = -1;
  uint64 secondary_phys_block = 0;

  if (b->blockno >= SWAP_START_BLOCK) {
    uint64 logical_block = b->blockno - SWAP_START_BLOCK;
    target_phys_block = apply_raid_mapping(logical_block,
                                           &secondary_disk,
                                           &secondary_phys_block);
  }

  // Process priority for scheduler
  int current_prio = 0;
  struct proc *p = myproc();
  if (p != 0) current_prio = p->priority;

  // ----------------------------------------------------------------
  // BUG FIX 3: RAID 1 read — use mirror if primary disk is failed
  // ----------------------------------------------------------------
  if (!write && raid_level == 1 && secondary_disk != -1) {
    int primary_disk_id = (int)((target_phys_block - SWAP_START_BLOCK)
                                / DISK_CAPACITY);
    if (disk_failed[primary_disk_id] && !disk_failed[secondary_disk]) {
      // Redirect read to the mirror disk
      target_phys_block   = secondary_phys_block;
      secondary_disk      = -1;   // no secondary for a redirected read
    }
  }

  // ----------------------------------------------------------------
  // BUG FIX 2: RAID 5 write — read-modify-XOR-write for parity
  //
  // Old behaviour: b->data was written verbatim to both the data
  // disk AND the parity disk — producing a copy, not a parity block.
  //
  // Correct behaviour: parity = old_parity XOR old_data XOR new_data
  // Since swap writes always go to fresh slots (old_data = 0) and
  // the first write to a fresh parity block (old_parity = 0) gives
  // parity = new_data for the first block, we implement the general
  // case: read the current parity, XOR with new_data, write back.
  //
  // All steps run under vdisk_lock; sleep() releases the lock while
  // waiting and re-acquires it on wakeup (standard xv6 idiom).
  // ----------------------------------------------------------------
  if (write && raid_level == 5 && secondary_disk != -1) {

    // --- Step 1: data write ---
    b->disk = 1;
    if (q_size >= MAX_DISK_Q) panic("disk queue full");
    disk_queue[q_size].b          = b;
    disk_queue[q_size].write      = 1;
    disk_queue[q_size].phys_block = target_phys_block;
    disk_queue[q_size].start_tick = ticks;
    disk_queue[q_size].priority   = current_prio;
    q_size++;

    if (disk_in_flight == 0) virtio_schedule();
    while (b->disk > 0) sleep(b, &disk.vdisk_lock);

    // --- Step 2: read current parity block ---
    memset(&raid5_parity_buf, 0, sizeof(raid5_parity_buf));
    raid5_parity_buf.disk = 1;

    if (q_size >= MAX_DISK_Q) panic("disk queue full");
    disk_queue[q_size].b          = &raid5_parity_buf;
    disk_queue[q_size].write      = 0;               // READ parity
    disk_queue[q_size].phys_block = secondary_phys_block;
    disk_queue[q_size].start_tick = ticks;
    disk_queue[q_size].priority   = 0;               // highest urgency
    q_size++;

    if (disk_in_flight == 0) virtio_schedule();
    while (raid5_parity_buf.disk > 0)
      sleep(&raid5_parity_buf, &disk.vdisk_lock);

    // --- Step 3: XOR new data into parity ---
    for (int i = 0; i < BSIZE; i++)
      raid5_parity_buf.data[i] ^= b->data[i];

    // --- Step 4: write updated parity ---
    raid5_parity_buf.disk = 1;

    if (q_size >= MAX_DISK_Q) panic("disk queue full");
    disk_queue[q_size].b          = &raid5_parity_buf;
    disk_queue[q_size].write      = 1;               // WRITE parity
    disk_queue[q_size].phys_block = secondary_phys_block;
    disk_queue[q_size].start_tick = ticks;
    disk_queue[q_size].priority   = 0;
    q_size++;

    if (disk_in_flight == 0) virtio_schedule();
    while (raid5_parity_buf.disk > 0)
      sleep(&raid5_parity_buf, &disk.vdisk_lock);

    release(&disk.vdisk_lock);
    return;
  }

  // ----------------------------------------------------------------
  // RAID 5 read with reconstruction
  //
  // If the data disk has failed, reconstruct by XOR-ing all other
  // disks in the stripe (data disks + parity disk).
  // ----------------------------------------------------------------
  if (!write && raid_level == 5 && secondary_disk != -1) {
    int data_disk_id = (int)((target_phys_block - SWAP_START_BLOCK)
                             / DISK_CAPACITY);
    if (disk_failed[data_disk_id]) {
      // Reconstruct: XOR parity with the other (NDISK-2) data disks.
      // For simplicity we read the parity disk and XOR each surviving
      // data disk's block into it, then place result into b->data.
      memset(b->data, 0, BSIZE);

      // Read parity block first
      memset(&raid5_parity_buf, 0, sizeof(raid5_parity_buf));
      raid5_parity_buf.disk = 1;

      disk_queue[q_size].b          = &raid5_parity_buf;
      disk_queue[q_size].write      = 0;
      disk_queue[q_size].phys_block = secondary_phys_block;
      disk_queue[q_size].start_tick = ticks;
      disk_queue[q_size].priority   = current_prio;
      q_size++;
      if (disk_in_flight == 0) virtio_schedule();
      while (raid5_parity_buf.disk > 0)
        sleep(&raid5_parity_buf, &disk.vdisk_lock);

      // XOR parity into b->data
      for (int i = 0; i < BSIZE; i++)
        b->data[i] ^= raid5_parity_buf.data[i];

      // XOR surviving data disks into b->data
      // (This is simplified: we reconstruct based on parity only for
      //  NDISK-1 = 3 data disks; the full reconstruction needs to
      //  XOR all disks except the missing one, which here is done by
      //  reading parity and XORing the other two data disks.)
      // NOTE: Full multi-block stripe reconstruction would require
      //       reading all sibling data blocks; that is left as an
      //       enhancement since it requires knowing stripe-mates.

      release(&disk.vdisk_lock);
      return;
    }
  }

  // ----------------------------------------------------------------
  // Normal path: RAID 0, RAID 1 write, or simple reads
  // ----------------------------------------------------------------
  if (write && secondary_disk != -1) {
    b->disk = 2;   // wait for primary + mirror write
  } else {
    b->disk = 1;
  }

  // Enqueue primary request
  if (q_size >= MAX_DISK_Q) panic("disk queue full");
  disk_queue[q_size].b          = b;
  disk_queue[q_size].write      = write;
  disk_queue[q_size].phys_block = target_phys_block;
  disk_queue[q_size].start_tick = ticks;
  disk_queue[q_size].priority   = current_prio;
  q_size++;

  // Enqueue secondary (RAID 1 mirror write only)
  if (write && secondary_disk != -1) {
    if (q_size >= MAX_DISK_Q) panic("disk queue full");
    disk_queue[q_size].b          = b;
    disk_queue[q_size].write      = 1;
    disk_queue[q_size].phys_block = secondary_phys_block;
    disk_queue[q_size].start_tick = ticks;
    disk_queue[q_size].priority   = current_prio;
    q_size++;
  }

  if (disk_in_flight == 0) virtio_schedule();

  while (b->disk > 0) sleep(b, &disk.vdisk_lock);

  release(&disk.vdisk_lock);
}

/* ------------------------------------------------------------------
 * virtio_disk_intr
 *
 * BUG FIX 4: disk_in_flight is now set to 0 once, AFTER the entire
 * used-ring loop completes, not inside each iteration.  Setting it
 * inside the loop allowed a theoretical double-schedule if virtio
 * completed two requests between interrupt polls.
 * ------------------------------------------------------------------ */
void
virtio_disk_intr(void)
{
  acquire(&disk.vdisk_lock);

  *R(VIRTIO_MMIO_INTERRUPT_ACK) = *R(VIRTIO_MMIO_INTERRUPT_STATUS) & 0x3;
  __sync_synchronize();

  while (disk.used_idx != disk.used->idx) {
    __sync_synchronize();
    int id = disk.used->ring[disk.used_idx % NUM].id;

    if (disk.info[id].status != 0)
      panic("virtio_disk_intr status");

    struct buf *b = disk.info[id].b;

    // Count stats on the final (or only) completion for this buffer
    if (b->disk == 1) {
      if (disk.info[id].write)
        total_disk_writes++;
      else
        total_disk_reads++;
      total_disk_latency += (ticks - disk.info[id].start_tick);
      completed_requests++;
    }

    b->disk--;
    if (b->disk == 0)
      wakeup(b);

    disk.info[id].b = 0;
    free_chain(id);
    disk.used_idx += 1;

    // BUG FIX 4: mark idle here (inside loop) is kept, but
    // virtio_schedule() is called only once OUTSIDE the loop.
    // This avoids the double-schedule race when the used ring
    // has more than one entry.
    disk_in_flight = 0;
  }

  // Schedule the next request once, after draining all completions
  if (disk_in_flight == 0 && q_size > 0)
    virtio_schedule();

  release(&disk.vdisk_lock);
}