# Dynamic-Memory-Allocator
This program implements three types of heap allocators, including **bump allocator**, **implicit free list allocator** and **explicit free list allocator**.

## Files
* **bump.c, implicit.c, explicit.c**: files that implement bump allocator, implicit allocator and explicit allocator respectively
* **test_harness.c**: a testing program that allows heap allocators to run on different test cases and catch potential errors
* **allocator.h**: a file containing function prototypes for functionality in each of the allocators
* **segment.h, segment.c**: supporting files that provides the heap segment size and heap segment start.
* **perfromance.txt**: evaluation of utilization and throughput of implicit  allocators and explicit allocators on various scripts. 
* **Scripts/**:
  An allocator script is a file that contains a sequence of requests in a compact text-based format. The three request types are a (allocate) r(reallocate) and f (free). Each request has an id-number that can be referred to in a subsequent realloc or free.
  
  ```
  a id-number size
  r id-number size
  f id-number
  ```
  A script file containing:
  ```
  a 0 24
  a 1 100
  f 0
  r 1 300
  f 1
  ```
  is converted into these calls to your allocator:
  ```
  void *ptr0 = mymalloc(24);
  void *ptr1 = mymalloc(100);
  myfree(ptr0);
  ptr1 = myrealloc(ptr1, 300);
  myfree(ptr1);
  ```
  Scripts folder contains three types of test scripts:
  * example scripts have workloads targeted to test a particular allocator feature. These scripts are very small (< 10 requests), easily traced, and especially useful for early development and debugging. These are much too small to be useful for performance measurements.
  * pattern scripts were mechanically constructed from various pattern templates (e.g. 500 mallocs followed by 500 frees or 500 malloc-free pairs). The scripts make enough requests (about 1,000) that they become useful for measuring performance, albeit on a fairly artificial workload. 
  * trace scripts contain real-world workloads capturing by tracing executing programs. These scripts are large (> 5,000 requests), exhibit diverse behaviors, and useful for comprehensive correctness testing. They also give broad measurement of performance you might expect to get "out in the wild".



## Usage
compile program
```
make
```

test utility of heap allocators on a script
```
./test_bump scripts/pattern-mixed.script
./test_implicit scripts/pattern-mixed.script
./test_explicit scirpts/pattern-mixed.script
```
test througput of heap allocators on a script
```
valgrind --tool=callgrind --toggle-collect=mymalloc --toggle-collect=myrealloc --toggle-collect=myfree ./test_bump scripts/pattern-mixed.script
callgrind_annotate --auto=yes callgrind.out.pid
```

## Design of heap allocators
### Bump allocator
A bump allocator is is a heap allocator design that simply allocates the next available memory address upon an allocate request and does nothing on a free request.
* **mymallo**:
  Bump allocator allocates memory by placing block at the end of heap.
* **myfree**:
  This allocator does nothing when freeing a block!
* **myrealloc**
  This function satisfies requests for resizing previously-allocated memory blocks by allocating a new block of the requested size and moving the existing contents to that region.  
* **performance and efficiency**:
  * **utilization**: each memory blck is used at most once, no memory is ever reused. So the utilization is extremely low.
  * **throughput**: malloc and free execute only a handful of instructions, so it is ultra fast.

### Implicit free list allocator
* **mymalloc**: 
  mymalloc adopts first fit, it traverses block by block from the beginning and chooses the first one that fits. And it throws remaining space into the allocated memory as extra padding. 
* **myfree**:
  myfree updates the header of the freed block memory.
* **myremalloc**:
   myrealloc first checks if it can reallocate in place without resizing. If approved, then check if the remaining space is sufficient to create a new free block. If in-place reallocation is not supported, it allocates a new block, copies existing content to that region and frees the original block.
* **performace and efficiency**:
  * **utilization**: This implementation keeps track of free blocks by creating headers, so it can recycle freed blocks, t achieved average utility over 70%.
  * **throughput**: This allocator supports in-place reallocation, reducing time to search for free blocksincreases throughput. Optimizing level of gcc compiler from Og to O2 can further increase throughput by 30%.

### Explicit free list allocator
* **mymalloc**: 
  This explicit allocator traverses free blocks following the linked list and chooses the first free block that fits. If remaining space is large enough for another allocation, it replaces original block with a new resized block. If not, it removes that block from the linked list.
* **myfree**: 
  When a block is freed, if its right neighbor is also free, the right free block absorbs freed block and updates itself in linked list. If not, allocator inserts the freed block into linked list.
* **myrealloc**: 
  Allocator would first absorb right free blocks as much as possible until there is no adjacent free blocks on the right and check if the original block can accommodate the reallocation. If in-place reallocation is allowed, check if it is necessary to create a new free block  when the remainng space is large enough.If not, it allocates a new block and copies existing content to that region. 
* **performace and efficiency**
  * **utilization**: This allocator supports in-place reallocation by coalescing all the free blocks on the right when reallocating a block, combining free blocks separated by headers. It increases average utilization to 90%.  
  * **throughput**: It uses linked list to keep track of all the free blocks, reducing time to search for a free block by jumping between free blocks. This implementation works best with a gcc optimization level of O3, which increases throughout by 127%.
