/* File: implicit heap allocator
 * -----------------------------------------
 * An implicit heap allocator keeps track block size and stautus by creating headers.
 * It allocates memory by iteraing all the blocks from the begining and chooses the
 * first free block that fits. And it creates a new free block if the remaing space can 
 * accomodate the minimum-sized block (8 bytes).
 * This allocator frees pointer by updating its status in header.
 * It support in-place reallocation if the block where old pointer lives is large enough 
 * for reallocation. If not, it allocates a new block of the requested size, move the 
 * existing content to that region and free the old pointer.
 */

#include "allocator.h"
#include "debug_break.h"
#include <stdio.h>
#include <string.h>

// status of a block
const size_t FREE = 7; //0b111;
const size_t USED = 0; //0b000
const size_t HEADER_LENGTH = 8;

static void *segment_start;
static void *segment_end;

/* Function: roundup()
 * -----------------
 * This function rounds up the given number to the given multiple, which 
 * must be a power of 2, and returns the result.
 * When an allocation request asks for 0 bytes, allocate a minimum sized block (8 bytes)
 */
size_t roundup(size_t sz, size_t mult) {
    // when the requested size is 0, allocates 8 bytes 
    if (sz <= ALIGNMENT) {
        return ALIGNMENT;
    }
    
    return (sz + mult - 1) & ~(mult - 1);
}

/* Function: myinit()
 * ------------------
 * The function returns true if initialization was 
 * successful, or false otherwise. The myinit function can be 
 * called to reset the heap to an empty state. When running 
 * against a set of of test scripts, our test harness calls 
 * myinit before starting each new script.
*/
bool myinit(void *heap_start, size_t heap_size) {

    /* initialization fails if heap starting address is a NULL value,
     *  or heap size is too small,
     */ 
    if (heap_start == NULL || heap_size <= HEADER_LENGTH) {
        return false;
    }
    segment_start = heap_start;

    // create the first header
    *(size_t *)segment_start = (heap_size - HEADER_LENGTH) | FREE;

    // the largets memory location of heap segment
    segment_end = (char *)heap_start + heap_size;
    return true;
}


/* Function: mymalloc()
 * --------------------
 * This function allocates requested_size bytes of memory and returns 
 * a pointer  to the allocated memory.
 * It is implemented with first fit: search the list from the begining 
 * each time and choose the first free block that fits.
 * 
 */
void *mymalloc(size_t requested_size) {
    
    size_t needed = roundup(requested_size, ALIGNMENT); 
    void *ptr = segment_start;
    
    while ((char *)ptr < (char *)segment_end) {
        size_t curr_size = *(size_t *)ptr & ~FREE;
        size_t curr_status = *(size_t *)ptr & FREE;
        
        if (curr_size >= needed && curr_status == FREE) {

            /* throw the remaning space into the allocated block as extra padding
             * if reamining space is not large enough for a new block
             */
            if (curr_size - needed <= HEADER_LENGTH) {
                // mark the header USED
                *(size_t *)ptr = curr_size & ~FREE;
            } else {
               // mark the header USED
                *(size_t *)ptr = needed & ~FREE;

                //create a new free block
                void *next_header = (char *)ptr + HEADER_LENGTH + needed;
                *(size_t *)next_header = (curr_size - needed - HEADER_LENGTH) | FREE;
            }
            return (char *)ptr + HEADER_LENGTH;
        }
        
        ptr = (char *)ptr + curr_size + HEADER_LENGTH;
    }

    printf("Not enough heap space for this allocation!\n");
    return NULL;
}


/* Function: myfree
 * ----------------
 * This function deallocates the memory allocation pointed to by ptr, 
 * If ptr is a NULL pointer, no operation is performed
 */
void myfree(void *ptr) {
    if (ptr != NULL) {
        void *header = (char *)ptr - HEADER_LENGTH;
        *(size_t *)header = *(size_t *)header | FREE;
    }   
}


/* Function: myrealloc
 * -------------------
 * This function tries to change the size of the allocation pointed to by old_ptr to new_size,
 * and returns ptr pointed to the reallocated memory.
 * This function first check is there is enough room at the enlarge the memory allocation 
 * pointed to by old_ptr, creates a new allocation if not.
 * If old_ptr is NULL, myrealloc() is identical to a call to malloc for new_size bytes.
 * If new_size is zero and old_ptr is not NULL, a new minimum sized object is allocatd 
 * and the original one is freed.
 */
void *myrealloc(void *old_ptr, size_t new_size) {
    if (old_ptr == NULL) return mymalloc(new_size);
    size_t old_size = *(size_t *)((char *)old_ptr - HEADER_LENGTH);
    size_t needed = roundup(new_size, ALIGNMENT);
    
    if (old_size >= needed) {
        // creates a new free block is remianing space is large enough for another allocation
        if (old_size - needed > HEADER_LENGTH) {
            void *new_header = (char *)old_ptr + needed;
            *(size_t *)new_header = (old_size - needed - HEADER_LENGTH) | FREE;
        } 
        return old_ptr; 
    }
    
    void *new_ptr = mymalloc(new_size);
    if (new_ptr == NULL) {
        return NULL;
    }
    memcpy((char *)new_ptr, (char *)old_ptr, old_size);
    myfree(old_ptr);
    return new_ptr;
}
 /* Function: validate_heap
  * ----------------------
  * This function checks for potential error/inconsistencies in the heap data
  * data structure and returns false if there were issues, or true otherwise.
  * This implementation checks
  *   - if the header indicates the status of that block (FREE or USED)
  *   - if block size stored in header is correct
  */
bool validate_heap() {
    void *ptr = segment_start;
    while ((char *)ptr < (char *)segment_end) {
        size_t curr_size = *(size_t *)ptr & ~FREE;
        size_t curr_status = *(size_t *)ptr & FREE;

        // test if the header contains information about status of this block
        if (curr_status != FREE && curr_status != USED) {
            breakpoint();
            printf("Opps! The status of headers are not reliable!\n");
            return false;
        }
        
        ptr = (char *)ptr + HEADER_LENGTH + curr_size;
    }

    // check if the block size stored in header is corrct by
    // traversing block by block to the end of the heap
    if ((char *)ptr != (char *)segment_end) {
        printf("Opps! It seems like the headers are messed up!\n");
        breakpoint();
        return false;
    }
    return true;
}
