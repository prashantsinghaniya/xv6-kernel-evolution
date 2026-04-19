typedef unsigned int   uint;
typedef unsigned short ushort;
typedef unsigned char  uchar;

typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int  uint32;
typedef unsigned long uint64;

typedef uint64 pde_t;

struct frame_info {
  int allocated; // 1 if allocated, 0 if free
  uint64 va; // virtual address mapped to this frame, if allocated
  struct proc *owner; // process that owns this frame, if allocated
  int ref_bit ; // reference bit for clock algorithm 1 if recently accessed, 0 otherwise
};

