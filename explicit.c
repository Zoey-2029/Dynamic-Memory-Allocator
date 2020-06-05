/*File: explicit heap allocator
 * -----------------------
 * An explicit free list allocator stores block size and status in headers, 
 * and it keeps tracks of free blocks by creating a linked list, which is sorted
 * with address.
 * This allocator allocates memory by iterating the free blocks through the linked
 * list, chooses the first free block that fits and updtaes the header and linked list.
 * A freed block would coalesce with the neighbor to the right if it is free, and updates
 * the block header and linked list.
 * Realloc would first absorb adjacent free blocks as much as possible. And check if the 
 * resized block is large enough for the reallocation. If not, allocate a new block and 
 * moves the existing content to that region.
 */

#include "allocator.h"
#include "debug_break.h"
#include <string.h>
#include <stdio.h>

const size_t MIN_BLOCK_SIZE = 16;
const size_t HEADER_LENGTH = 8;

//status of a block
const size_t FREE = 7; // 0b111, this block is free;
const size_t USED = 0; // 0b000, this block is occupied
const size_t NON_EXISTENT = 1; // indicate one block doesn't have a header


static void *segment_start; 
static void *segment_end; 
static void *list_head; //address of the first block in freelist
static void *list_end; //address of the last block in freelist
static size_t free_blocks_num; //number of blocks in the freelist

/* This function returns the pointer pointing to the prev pointer, which store
 * the address of the previous free block
 */
void **get_curr_prev(void *curr) {
    return (void **)((char *)curr + HEADER_LENGTH);
}

/* This function returns the pointer pointing to the next pointer, which stores
 * the address of next free block
 */ 
void **get_curr_next(void *curr) {
    return (void **)((char *)curr + HEADER_LENGTH + sizeof(void **));
}

// This function returns the address of the previous free block in freelist
void *get_prev_free_block(void *curr) {
    return *get_curr_prev(curr);
}


// This function returns the address of the next free block in freelist 
void *get_next_free_block(void *curr) {
    return *get_curr_next(curr);
}


/* Function: roundup()
 * -------------------
 * This function rounds up the given number to the given multiple, which 
 * must be a power of 2, and returns the result.
 * When the requested size if less than 16, allocate a minimum 
 * sized block(16 bytes)
 */
size_t roundup(size_t sz, size_t mult) {
    if (sz <= MIN_BLOCK_SIZE) {
        return MIN_BLOCK_SIZE;
    }
    return (sz + mult - 1) & ~(mult - 1);
}


// This function inserts a free block into the free list
void insert_block_into_freelist(void *curr) {
    free_blocks_num++;
    void **prev = get_curr_prev(curr);
    void **next = get_curr_next(curr);

    // there is no free blocks in the freelist
    if (list_head == NULL) {
        *prev = NULL;
        *next = NULL;
        list_head = curr;
        list_end = curr;
        return;
    }

    // currrent block -> list_head -> list_end
    if ((char *)curr < (char *)list_head) {
        *prev = NULL;
        *next = list_head;

        *get_curr_prev(list_head) = curr;
        list_head = curr;
        return;
    }

    //list_head -> list_end -> currrent block
    if ((char *)list_end < (char *)curr) {
        *prev = list_end;
        *next = NULL;
        *get_curr_next(list_end) = curr;
        list_end = curr;
        return;
    }

    //list_head -> currrent block -> list_end
    void *list_next = list_head;
    while ((char *)list_next < (char *)curr) {
        list_next = get_next_free_block(list_next);
    }

    void *list_prev = get_prev_free_block(list_next);
    *get_curr_next(list_prev) = curr;
    *get_curr_prev(list_next) = curr;
    *prev = list_prev;
    *next = list_next;
}

// This function removes a block from the freelist
void remove_block_from_freelist(void *curr) {
    free_blocks_num--;
    void *prev = get_prev_free_block(curr);
    void *next = get_next_free_block(curr);

    if (prev != NULL) {
        *get_curr_next(prev) = next;
    }
    if (next != NULL) {
        *get_curr_prev(next) = prev;
    }

    if ((char *)curr == (char *)list_head) {
        list_head = next;
    }
    
    if ((char *)curr == (char *)list_end) {
        list_end = prev;
    }
}


/* Function: myinit
 * ----------------
 * This function returns true if the heap address is valid and the heap
 * can at least store one minimum block. And it crates the first free block 
 * on the heap.
 */
bool myinit(void *heap_start, size_t heap_size) {

    /* initialization fails if heap starting address is a NULL value,
     * or heap size if too small
     */ 
    if (heap_start == NULL || heap_size < HEADER_LENGTH + MIN_BLOCK_SIZE) {
        return false;
    }

    segment_start = heap_start;
    segment_end = (char *)heap_start + heap_size;
    list_head = heap_start;
    list_end = heap_start;

    // creates the first free block on the heap
    *(size_t *)list_head = (heap_size - HEADER_LENGTH) | FREE;
    *get_curr_prev(list_head) = NULL;
    *get_curr_next(list_head) = NULL;
    
    free_blocks_num = 1;
    return true;
}


/* Function: mymalloc()
 * --------------------
 * This function allocates requested_size bytes of memory and returns 
 * a pointer to the allocated memory
 * It traverses free blocks following the linked list and chooses the first
 * block that fits. If the remianing space is large enough for another allocation, 
 * this function creates a new free block and insert it into the linked list
 */
void *mymalloc(size_t requested_size) {
    size_t needed = roundup(requested_size, ALIGNMENT);
    void *curr = list_head;
    while ((char *)curr <= (char *)list_end) {
        size_t curr_size = *(size_t *)curr & ~FREE;
        
        if (curr_size >= needed) {
            /* remove current block from freelist if remaining space is not enough 
             * is not enough for another allocation
             */
            if (curr_size - needed < HEADER_LENGTH + MIN_BLOCK_SIZE) {
                *(size_t *)curr = curr_size & ~FREE;
                remove_block_from_freelist(curr);
            } else {
                /* create a new free block and insert it into freelist if 
                 * remaning space is large enough for another allocation
                 */
  
                *(size_t *)curr = needed & ~FREE;

                // create a new free block
                void *new_header = (char *)curr + needed + HEADER_LENGTH;
                *(size_t *)new_header = (curr_size - needed - HEADER_LENGTH) | FREE;
                
                void *prev = get_prev_free_block(curr);
                void *next = get_next_free_block(curr);

                if (prev != NULL) {
                    *get_curr_next(prev) = new_header;
                }
                if (next != NULL) {
                    *get_curr_prev(next) = new_header;
                }
                *get_curr_prev(new_header) = prev;
                *get_curr_next(new_header) = next;

                if ((char *)curr == (char *)list_head) {
                    list_head = new_header;
                }
                if ((char *)curr == (char *)list_end) {
                    list_end = new_header;
                }
            }
            
            return (char *)curr + HEADER_LENGTH;
        }

        curr = get_next_free_block(curr);
    }
    printf("Not enough heap space for this allocation!");
    return NULL;
}

/* Function: myfree()
 * ------------------
 * This function deallocates the memory allocation pointed to by ptr.
 * A freed block would coalesce with its neighbor block to the right if 
 * it is also free. If not, just insert this freed block into freelist.
 * If the ptr is a NULL, no operation is performed
 */
void myfree(void *ptr) {
    if (ptr == NULL) return;
    void *curr = (char *)ptr - HEADER_LENGTH;
    size_t curr_size = *(size_t *)curr & ~FREE; 
      
    void *right = (char *)curr + HEADER_LENGTH + curr_size; 
    size_t right_status = ((char *)right == (char *)segment_end) ? NON_EXISTENT : *(size_t *)right & FREE;

    // if the block right to current block is free, coalesce this pair of blocks
     if (right_status == FREE) {
         size_t right_size = *(size_t *)right;
         *(size_t *)right = right_size & ~FREE;
         *(size_t *)curr = (curr_size + HEADER_LENGTH + right_size) | FREE;
        
         void *right_prev = get_prev_free_block(right);
         void *right_next = get_next_free_block(right);

         if (right_prev != NULL) {
             *get_curr_next(right_prev) = curr;
         }
         if (right_next != NULL) {
             *get_curr_prev(right_next) = curr;
         }
         
         *get_curr_prev(curr) = right_prev;
         *get_curr_next(curr) = right_next;

         if ((char *)right == (char *)list_head ){
             list_head = curr;
         }
         if ((char *)right == (char *)list_end) {
             list_end = curr;
         }
         
     } else { // if the right neighbor is not freed, insert current block to freelist
        *(size_t *)curr = curr_size | FREE;
        insert_block_into_freelist(curr);   
    }
}

/* Function: myrealloc()
 * ---------------------
 * This function tries to change the size of the allocation pointed by old_ptr
 * to new_size and returns the ptr pointed to the reallocates memory.
 * myrealloc() first absorbs adjacent free blocks as much as possible.
 * After resizing the original block, if it can be used for reallocation, check
 * if it is neccessay to create a new free block if remaning space is large enough for 
 * a minimum block. If resized block is not qualified for reallocation, allocate a new block,
 * move the existing content to that region and free the old block.
 */
void *myrealloc(void *old_ptr, size_t new_size) {
    if (old_ptr == NULL) return mymalloc(new_size);
    
    size_t needed = roundup(new_size, ALIGNMENT);
    void *curr = (char *)old_ptr - HEADER_LENGTH;
    size_t old_size = *(size_t *)curr & ~FREE;
    size_t curr_size = old_size;
    
    void *right = (char *)curr + HEADER_LENGTH + curr_size;
    size_t right_status = ((char *)right == (char *)segment_end) ? NON_EXISTENT : *(size_t *)right & FREE;

    // current block absorbs adjacent free blocks as much as possible
    while (right_status == FREE) {
        size_t right_size = *(size_t *)right & ~FREE;
        *(size_t *)right = right_size & ~FREE;
        remove_block_from_freelist(right);
        
        curr_size += HEADER_LENGTH + right_size;
        right = (char *)curr + HEADER_LENGTH + curr_size;
        right_status = ((char *)right == (char *)segment_end) ? NON_EXISTENT : *(size_t *)right & FREE;
    }

    // resized block can be used as reallocation
    if (curr_size >= needed) {
        // remianing space is not large enough for another allocation
        if (curr_size - needed < HEADER_LENGTH + MIN_BLOCK_SIZE) {
            *(size_t *)curr = curr_size & ~FREE;
        } else {
            // creates a new free block and inserts it into freelist if
            // remianing space is enough for another allocation
            *(size_t *)curr = needed & ~FREE;
            void *new_pointer = (char *)curr + HEADER_LENGTH + needed;
            *(size_t *)new_pointer = (curr_size - needed - HEADER_LENGTH) | FREE;
            insert_block_into_freelist(new_pointer);   
        }
        return old_ptr;
    } else {

        //if resized block is not qualified for reallocation, free current block and
        // returns a new allocation
        void *new_ptr = mymalloc(new_size);
        memcpy((void *)new_ptr, (void *)old_ptr, old_size);
        *(size_t *)curr = curr_size | FREE;
        myfree(old_ptr);
        return new_ptr;
    }
}


/* Function: traverse_heap()
 * -------------------------
 * This function iterates through all blocks and checks:
 *   - if each block is at least 16 bytes
 *   - if the headers stores block status (USER or FREE)
 *   - if the block size information stroed in header is valid
 *   - if freelist contains all the free blocks
 */
bool traverse_heap() {
    void *ptr = segment_start;
    size_t count = 0;
    while ((char *)ptr < (char *)segment_end) {
        size_t curr_size = *(size_t *)ptr & ~FREE;
        size_t curr_status = *(size_t *)ptr & FREE;

        // every block should be at least 16 bytes
        if (curr_size < MIN_BLOCK_SIZE) {
            printf("Each block should be at least 16 bytes!\n");
            breakpoint();
            return false;
        }

        // the status of blocks should be either FREE or USED
        if (curr_status != FREE && curr_status != USED) {
            printf("Opps! The status of headers are not reliable!\n");
            breakpoint();
            return false;
        }

        if (curr_status == FREE) {
            count++;
        }
        
        ptr = (char *)ptr + HEADER_LENGTH + curr_size;
    }

    // check if the block size information stored in block headers are reasonable
    if ((char *)ptr != (char *)segment_end) {
        printf("Opps! It seems like the headers are messed up!\n");
        breakpoint();
        return false;
    }

    // check if freelist contains all the free blocks
    if (count != free_blocks_num) {
        printf("Extra free blocks observed on heap!\n");
        breakpoint();
        return false;
    }
    
    return true;
}


//This function checks if a address is a valid heap address
bool within_heap_range(void *ptr) {
    if (ptr == NULL) {
        return false;
    }
    return (char *)segment_start <= (char *)ptr && (char *)ptr <= (char *)segment_end; 
}

/* Function: traverse_freelist()
 * -----------------------------
 * This function iterates forward and backwards through all free blocks follwing the 
 * linked list, and checks:
 *   - if all the blocks in freelist are free
 *   - if the freelist are sorted with heap address
 *   - if the address stored in free blocks are valid heap address or NULL 
 *      (NULL is only allowed when current block is the first or last block of the freelist).
 */
bool traverse_freelist(bool reverse) {
    if (free_blocks_num == 0) {
        return list_head == NULL && list_end == NULL;
    }

    void *ptr = (reverse) ? list_end : list_head;
    size_t count = 1;
    while (count <= free_blocks_num) {
        size_t curr_status = *(size_t *)ptr & FREE;

        // all blocks in freelist should be free
        if (curr_status != FREE) {
            breakpoint();
            printf("All blocks in the linked list should be free!\n");
            return false;
        }

        void *prev = get_prev_free_block(ptr);
        void *next = get_next_free_block(ptr);


        // first free block does not have a previous free block
        // last free block should not have a next free block
        // all other addresses should be valid heap address
        bool test_prev;
        bool test_next;
        
        if (!reverse) {
            test_prev = (count == 1) ? (prev == NULL) : within_heap_range(prev);
            test_next = (count == free_blocks_num) ? (next == NULL) : within_heap_range(next);
        } else {
            test_next = (count == 1) ? (next == NULL) : within_heap_range(next);
            test_prev = (count == free_blocks_num) ? (prev == NULL) : within_heap_range(prev);  
        }

        if (!test_prev || !test_next) {
            breakpoint();
            printf("Ths address stored in linked list is out of heap memory!\n");
            return false;
        }


        // check if the freelist is sorted with heap address
        if ((prev != NULL && (char *)prev >= (char *)ptr)
            || (next != NULL && (char *)ptr >= (char *)next)) {
            breakpoint();
            printf("The linked list should be sorted with address order!\n");
            return false;
        }
        
        ptr = (reverse) ? prev : next;
        count++;
    }

    return true;
}

/* Function : validate()
 * ---------------------
 * This function checks potential errors/inconsistencies in the heap data
 * structure and returns false if there were issues ot true otheriwse.
 * Thish function checks:
 *   - the consistency of heap data
 *   - the consistency of linked list
 */
bool validate_heap() {
    if (!traverse_heap() || !traverse_freelist(true) || !traverse_freelist(false)) {
        return false;
    }
    return true;
}
