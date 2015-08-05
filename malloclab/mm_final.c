/*
 * mm.c
 *
 * General overview: segregate free list + LIFO strategy
 * structure of free block: |header|fptr|xxxx.......xxxx|nptr|footer|
 *                          header is (size | 0) , footer is (size | 0)
 *                          fptr stores last free block's address offset, nptr stores next free block's offset
 *                          header, footer, fptr and nptr are both 4bytes
 *
 * structure of allocated block:
 *                          |header|-1|xxx......xxxxx|-1|footer|
 *                          header and footer are (size | 1)
 *
 * the segregate list contains 14 groups, the sizes of each as follows:
 *                          0-32, 33-64, 65-128, 129-256, 257-512, 513-768
 *                          769-1024, 1025-1536, 1537-2048, 2049-2560
 *                          2561-3072, 3073-3584, 3585-4096, 4097--
 *
 * when a free block has to store the head of each group of segregate list in its fptr,
 * it stores (-index - 2), index is the index of head in the list array.
 *
 * if a free block is the last one in the free list, it stores -1 in its nptr, because
 * 0 means the start of the heap which we can not regard as NULL.
 *
 * coalesce strategy: coalesce immediately after free or allocation of a block
 * free list management policy: LIFO --- insert the new block into the head position
 *
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mm.h"
#include "memlib.h"

/* If you want debugging output, use the following macro.  When you hand
 * in, remove the #define DEBUG line. */
#ifdef DEBUG
# define dbg_printf(...) printf(__VA_ARGS__)
#else
# define dbg_printf(...)
#endif


/* do not change the following! */
#ifdef DRIVER
/* create aliases for driver tests */
#define malloc mm_malloc
#define free mm_free
#define realloc mm_realloc
#define calloc mm_calloc
#endif /* def DRIVER */

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(p) (((size_t)(p) + (ALIGNMENT-1)) & ~(0x7))

#define WSIZE 4
#define DSIZE 8
#define SET(p, size, bit) (*(int*)(p) = ((size) | (bit))) //set size and bit
#define SETP(p, addr) (*(int*)(p)) = ((int)(addr)) //set fptr and nptr
#define GETSIZEBIT(p) (*(int*)(p)) //get size and bit
#define GETSIZE(p) (GETSIZEBIT(p) & (~0x1)) //get size
#define GETINDEX(p) (*(int*)(p)) //get fptr and nptr
#define LIST 18

static void* heap;
static int find_fit(int size, int index);
static int find_list(int size);
static void place(void* sp, int size);
static void extendFree(void* sp, int size);
static void doCoalescing(void* ptr, int size);
static void insertOneFreeBlock(void* ptr, int size);
static void insertTwoFreeBlock(void* fptr, void* cptr, int size, int oldSize);
static void insertThreeFreeBlock(void* fptr, void* nptr, int size, int oldSize1, int oldSize2);
static int in_heap(const void *p);
static void reArrangeFreeBlock(void* fptr, void* cptr, int size, int oldSize);
size_t seglist[LIST]; //segregate list

/*
 * Initialize: return -1 on error, 0 on success.
 */
int mm_init(void) {
    if ((heap = mem_sbrk(32 * DSIZE)) == (void*)-1)
        return -1;
    int i = 0;
    for (; i < LIST; i ++)
        seglist[i] = -1;
    SET(heap, 32 * DSIZE, 0);
    SET(heap + 32 * DSIZE - 4, 32 * DSIZE, 0);
    SETP(heap + 4, -1);
    seglist[3] = heap - (void*)mem_heap_lo();
    SETP(heap + 32 * DSIZE - 8, -5);
    return 0;
}

/*
 * malloc
 */
void *malloc (size_t size) {
    if (size == 0)
        return NULL;
    size_t newSize, alignSize;
    int listIndex = 0;
    int sp;

    //adjust size
    alignSize = ALIGN(size);
    newSize = alignSize + 2 * WSIZE + 2 * WSIZE;
    listIndex = find_list(newSize);
    if ((sp = find_fit(newSize, listIndex)) != -1) {
        place(mem_heap_lo() + sp, newSize);
        //mm_checkheap(__LINE__);
        return (mem_heap_lo() + sp + 8);
    }

    //need to allocate extra space for heap
    void* newStartAddress;
    size_t newHeapSpace = newSize > (32 * DSIZE) ? newSize : (32 * DSIZE);
    if ((newStartAddress = (void*)mem_sbrk(newHeapSpace)) == (void*)-1)
        return (void*)-1;

    extendFree(newStartAddress, newHeapSpace);
    sp = find_fit(newSize, listIndex);
    place(mem_heap_lo() + sp, newSize);
    //mm_checkheap(__LINE__);
    return (mem_heap_lo() + sp + 8);

}

/*
 * extendFree - set extended free blocks
 */

void extendFree(void* sp, int size) {
    SET(sp, size, 0);
    SET(sp + size - 4, size, 0);

    doCoalescing(sp, size); //need to do coalescing
}

/*
 * free
 */
void free (void *ptr) {
    if(!ptr) return;

    if (heap == 0)
        mm_init();
    //free the block pointed by ptrs
    size_t sizeAndBit = GETSIZEBIT(ptr - 8);
    size_t bit = sizeAndBit & 0x1;
    if (bit == 0) //can not free a free block
        return;
    size_t size = sizeAndBit - 1;

    SET(ptr - 8, size, 0);
    SET(ptr - 8 + size - 4 , size, 0);

    doCoalescing(ptr - 8, size);
    //mm_checkheap(__LINE__);
}


/*
 * insertOnefreeblock - insert new free block into new seglist
 * the block does not need to coalesce the last and/or next block
 */
void insertOneFreeBlock(void* ptr, int size) {
    int index = find_list(size);
    int oldNextIndex = seglist[index];

    int tempSize = 0;
    SETP(ptr + 4, oldNextIndex);
    SETP(ptr + size - 8, -index - 2);
    int addressOffset = (int)(ptr - mem_heap_lo());
    seglist[index] = addressOffset;
    if (oldNextIndex != -1) {
        tempSize = GETSIZE(mem_heap_lo() + oldNextIndex);
        SETP(mem_heap_lo() + oldNextIndex + tempSize - 8, addressOffset);
    }
}
/*
 * insertTwofreeblock - insert new free block into new seglist
 * the block needs to coalesce the last or next block
 * newHead is the new head of the list after coalescing
 * ptr is the pointer of the block which needs to resets its fptr and nptr
 *     and to be removed from the according segregate list
 */
void insertTwoFreeBlock(void* newHead, void* ptr, int size, int oldSize) {
    int index = find_list(size);
    int oldPreIndex = GETINDEX(ptr + oldSize - 8);
    int oldNextIndex = GETINDEX(ptr + 4);

    int tempSize = 0;
    //assert(oldPreIndex != -1);
    if (oldPreIndex >= 0) {
        SETP(mem_heap_lo() + oldPreIndex + 4, oldNextIndex);
    } else {
        seglist[-(oldPreIndex + 2)] = oldNextIndex;
    }
    if (oldNextIndex != -1) {
        tempSize = GETSIZE(mem_heap_lo() + oldNextIndex);
        SETP(mem_heap_lo() + oldNextIndex + tempSize - 8, oldPreIndex);
    }

    //add the new free block to seglist

    int nextIndex = seglist[index];
    SETP(newHead + 4, nextIndex);
    int addressOffset = (int)(newHead - mem_heap_lo());
    seglist[index] = addressOffset;
    SETP(newHead + size - 8, -index - 2);
    if (nextIndex != -1) {
        tempSize = GETSIZE(mem_heap_lo() + nextIndex);
        SETP(mem_heap_lo() + nextIndex + tempSize - 8, addressOffset);
    }
}
/*
 * insertThreefreeblock - insert new free block into new seglist
 * the block needs to coalesce the last and next block
 * fptr is the new head of the list after coalescing and the pointer of last block, oldSize1 is its size
 * nptr is the pointer of last block, oldSize2 is its size
 * size is new size of the coalesced block
 */
void insertThreeFreeBlock(void* fptr, void* nptr, int size, int oldSize1, int oldSize2) {
    int index = find_list(size);
    int oldPreIndex = GETINDEX(fptr + oldSize1 - 8);
    int oldNextIndex = GETINDEX(fptr + 4);

    int tempSize = 0;
    //remove fptr from segregate free list
    if (oldPreIndex >= 0) {
        SETP(mem_heap_lo() + oldPreIndex + 4, oldNextIndex);
    } else {
        seglist[-(oldPreIndex + 2)] = oldNextIndex;
    }
    if (oldNextIndex != -1) {
        tempSize = GETSIZE(mem_heap_lo() + oldNextIndex);
        SETP(mem_heap_lo() + oldNextIndex + tempSize - 8, oldPreIndex);
    }

    //remove nptr from segregate free list
    oldPreIndex = GETINDEX(nptr + oldSize2 - 8);
    oldNextIndex = GETINDEX(nptr + 4);
    if (oldPreIndex >= 0) {
        SETP(mem_heap_lo() + oldPreIndex + 4, oldNextIndex);
    } else {
        seglist[-(oldPreIndex + 2)] = oldNextIndex;
    }
    if (oldNextIndex != -1) {
        tempSize = GETSIZE(mem_heap_lo() + oldNextIndex);
        SETP(mem_heap_lo() + oldNextIndex + tempSize - 8, oldPreIndex);
    }

    //add the new free block to seglist

    int nextIndex = seglist[index];

    SETP(fptr + 4, nextIndex);
    int addressOffset = (int)(fptr - mem_heap_lo());
    seglist[index] = addressOffset;
    SETP(fptr + size - 8, -index - 2);
    if (nextIndex != -1) {
        tempSize = GETSIZE(mem_heap_lo() + nextIndex);
        SETP(mem_heap_lo() + nextIndex + tempSize - 8, addressOffset);
    }
}


/*
 * do coalescing
 */

void doCoalescing(void* ptr, int size) {
    int fFree = 1, nFree = 1;
    int fSize = 0, nSize = 0;

    //check boundary and free/allocated block
    if ((ptr - 4) >= (heap + 4)) {
       fSize = GETSIZEBIT(ptr - 4);
       fFree = fSize & 0x1;
    }

    //check boundary and free/allocated block
    if ((ptr + size) < (mem_heap_hi() + 1)) {
        nSize = GETSIZEBIT(ptr + size);
        nFree = nSize & 0x1;
    }
    int newSize = 0;

    if (fFree && nFree) { //last and next are 1
        insertOneFreeBlock(ptr, size);
    } else if (!fFree && nFree) { //last is 0, next is 1
        newSize = fSize + size;
        insertTwoFreeBlock(ptr - fSize, ptr - fSize, newSize, fSize);
        SET(ptr - fSize, newSize, 0);
        SET(ptr + size - 4, newSize, 0);
    } else if (fFree && !nFree) { //last is 1, next is 0
        newSize = size + nSize;
        insertTwoFreeBlock(ptr,  ptr + size, newSize, nSize);
        SET(ptr, newSize, 0);
        SET(ptr + newSize - 4, newSize, 0);
    } else if (!fFree && !nFree){
        newSize = fSize + size + nSize;
        insertThreeFreeBlock(ptr - fSize, ptr + size, newSize, fSize, nSize);
        SET(ptr - fSize, newSize, 0);
        SET(ptr + size + nSize - 4, newSize, 0);
    }

}

/*
 * realloc - you may want to look at mm-naive.c
 */
void *realloc(void *oldptr, size_t size) {
    size_t oldSize;
    void* newPtr;

    //if size == 0 then this is just free
    if (size == 0) {
        free(oldptr);
        return NULL;
    }

    if (oldptr == NULL)
        return malloc(size);

    newPtr = malloc(size);
    if (!newPtr)
        return NULL;

    //copy the old data
    oldSize = GETSIZE(oldptr - 8);
    size_t count = size < (oldSize  - 2 * WSIZE - 2 * 4)? size : (oldSize - 2 * WSIZE - 2 * 4);
    size_t i = 0;
    for (i = 0; i < count; i ++) {
        *(char*)(newPtr + i) = *(char*)(oldptr + i);
    }

    //Free the old block
    free(oldptr);

    return newPtr;
}

/*
 * calloc - you may want to look at mm-naive.c
 * This function is not tested by mdriver, but it is
 * needed to run the traces.
 */
void *calloc (size_t nmemb, size_t size) {
    size_t bytes = nmemb * size;
    void* newPtr;

    newPtr = malloc(bytes);
    if (!newPtr)
        return NULL;
    size_t i = 0;
    for (i = 0; i < bytes; i ++)
        *(char*)(newPtr + i) = 0;

    return newPtr;
}
/*
 * find_list - find the appropriate size seglist
 * return the index
 */

int find_list(int size) {
    if (size <= 32)
        return 0;
    else if (size <= 64)
        return 1;
    else if (size <= 128)
        return 2;
    else if (size <= 256)
        return 3;
    else if (size <= 512)
        return 4;
    else if (size <= 768)
        return 5;
    else if (size <= 1024)
        return 6;
    else if (size <= 1536)
        return 7;
    else if (size <= 2048)
        return 8;
    else if (size <= 2560)
        return 9;
    else if (size <= 3072)
        return 10;
    else if (size <= 3584)
        return 11;
    else if (size <= 4096)
        return 12;
    else if (size <= 7680)
        return 13;
    else if (size <= 15360)
        return 14;
    else if (size <= 32720)
        return 15;
    else if (size <= 62696)
        return 16;
    else
        return 17;

}


/*
 * find_fit -  find the fit free blocks
 * use first-fit here
 */
int find_fit(int size, int index) {
    int addr = seglist[index];
    int i = index;
    int tempSize = 0;
    for (i = index; i < LIST; i ++) {
        addr = seglist[i];
        while (addr >= 0) {
            tempSize = GETSIZE(mem_heap_lo() + addr);
            if (tempSize >= size) {
                return addr;
            }
            else {
                addr = GETINDEX(mem_heap_lo() + addr + 4);
            }
        }

    }
    return -1;
}

/*
 * rearrangefreeblock - rearrange the left free block to new segregate free list
 *                      after allocating a fraction of it
 * fptr is start address of the allocated fraction
 * cptr is current start address of the left free block
 * oldSize is size of the old free block before allocation
 */
void reArrangeFreeBlock(void* fptr, void* cptr, int size, int oldSize) {

    int preIndex = GETINDEX(fptr + oldSize- 8);
    int nextIndex = GETINDEX(fptr + 4);

    int tempSize = 0;
    if (preIndex >= 0)
        SETP(mem_heap_lo() + preIndex + 4, nextIndex);
    else
        seglist[-(preIndex + 2)] = nextIndex;
    if (nextIndex >= 0) {
        tempSize = GETSIZE(mem_heap_lo() + nextIndex);
        SETP(mem_heap_lo() + nextIndex + tempSize - 8, preIndex);
    }

    //need to rearrange the left block
    int newIndex = find_list(size);
    int oldNextIndex = seglist[newIndex];
    SETP(cptr + 4,oldNextIndex);
    SETP(cptr + GETSIZE(cptr) - 8, -newIndex - 2);
    seglist[newIndex] = (int)(cptr - mem_heap_lo());
    if (oldNextIndex >= 0) {
        tempSize = GETSIZE(mem_heap_lo() + oldNextIndex);
        SETP(mem_heap_lo() + oldNextIndex + tempSize - 8, seglist[newIndex]);
    }
}

/*
 * place - modify the bits of a newly allocated block
 */
void place(void* sp, int size) {
    int oldSize = GETSIZE(sp);
    if (oldSize - size >= (2 * WSIZE + 2 * WSIZE)) {
        SET(sp, size, 1);
        SET(sp + size - 4, size, 1);
        SET(sp + size, oldSize - size, 0);
        SET(sp + oldSize - 4, oldSize - size, 0);
        reArrangeFreeBlock(sp, sp + size, oldSize - size, oldSize);
    } else { //the whole block is allocated
        SET(sp, oldSize, 1);
        SET(sp + oldSize - 4, oldSize, 1);
        int preIndex = GETINDEX(sp + oldSize - 8);
        int nextIndex = GETINDEX(sp + 4);
        if (preIndex >= 0)
            SETP(mem_heap_lo() + preIndex + 4, nextIndex);
        else
            seglist[-(preIndex + 2)] = nextIndex;
        if (nextIndex >= 0) {
            int tempSize = GETSIZE(mem_heap_lo() + nextIndex);
            SETP(mem_heap_lo() + nextIndex + tempSize - 8, preIndex);
        }
    }
}


/*
 * Return whether the pointer is in the heap.
 * May be useful for debugging.
 */
static int in_heap(const void *p) {
    return p <= mem_heap_hi() && p >= mem_heap_lo();
}

/*
 * Return whether the pointer is aligned.
 * May be useful for debugging.
 */
static int aligned(const void *p) {
    return (size_t)ALIGN(p) == (size_t)p;
}

/*
 * mm_checkheap
 * it will check:
 *      1. check each block for header, footer, address alignment, size match, whether or not in heap.
 *      2. get next block in free list, check whether last block and next block match
 *      3. check free list, if there exists cycle, or if one of the blocks does not belong to the list
 *      4. check coalescing
 */
void mm_checkheap(int lineno) {
    //first check heap
    void* ptr = heap;
    void* tail = mem_heap_hi() + 1;
    int headerSize = 0, footerSize = 0;
    int fptr = -1, nptr = -1;
    int allocateBit = 0;
    for (; ptr < tail; ) {
        if (!in_heap(ptr)) {
            printf("Address %p  not in heap! Program exists at line %d\n",
                    ptr, lineno);
            exit(1);
        }
        if (!aligned(ptr)) {
            printf("Address %p is not aligned 8! Program exists at line %d\n",
                    ptr, lineno);
            exit(1);
        }
        headerSize = GETSIZE(ptr);
        footerSize = GETSIZE(ptr);
        if (headerSize != footerSize) {
            printf("Header and Footer does not match at address %p!\n", ptr);
            printf("Header is %d, footer is %d\n", headerSize, footerSize);
            printf("Program exits at line %d\n", lineno);
            exit(1);

        }
        allocateBit = GETSIZEBIT(ptr) & 0x1;

        nptr = GETINDEX(ptr + 4);
        fptr = GETINDEX(ptr + headerSize - 8);
        if (!allocateBit && (fptr == -1 || nptr <= -2)) {
            printf("Last and next block address error! at line %d\n", lineno);
            printf("The block starts at %p is free\n", ptr);
            exit(1);
        }
        ptr = ptr + headerSize;
    }

    //then check segregate free list;
    int i = 0;
    int head = 0, lastWalker = 0, nextWalker = 0;
    int lastIndex = 0;
    void* heapStart = mem_heap_lo();
    int lastBlockBit = 0;
    for (; i < 18; i ++) {
        head = i;
        if ((int)seglist[i] < -1) {
            printf("Segregrate list %d head error at line %d\n", i, lineno);
            exit(1);
        }
        lastWalker = - i - 2;
        nextWalker = seglist[i];


        while (nextWalker != -1) {
            //check if free block in heap
            if (!in_heap(heapStart + nextWalker)) {
                printf("Free list %d block offset %d not in the heap\n",
                        i, nextWalker);
                printf("Error at line %d\n", lineno);
                exit(1);
            }
            //check if allocated block in free list
            allocateBit = GETSIZEBIT(heapStart + nextWalker) & 0x1;
            if (allocateBit) {
                printf("Allocated block in free list %d, offset %d\n",
                        i, nextWalker);
                printf("Error at line %d\n", lineno);
                exit(1);
            }
            headerSize = GETSIZE(heapStart + nextWalker);
            lastIndex = GETINDEX(heapStart + nextWalker + headerSize - 8);

            //check if the block belongs to this list
            if (head != find_list(headerSize)) {
                printf("Free block offset at %d should not in free list %d\n",
                        nextWalker, i);
                printf("Error at line %d\n", lineno);
                exit(1);
            }

            //check if last index equals to last walker
            if (lastIndex != lastWalker) {
                printf("Last index error in free list %d, offset %d\n",
                        i, nextWalker);
                printf("Error at line %d\n", lineno);
                exit(1);
            }

            //check coalescing - next block
            if (heapStart + nextWalker + headerSize < tail) {
                if (!(GETSIZEBIT(heapStart + nextWalker + headerSize) & 0x1)) {
                    printf("Coalescing error in free list %d, offset %d\n",
                            i, nextWalker);
                    printf("Next block coalescing Error at line %d\n", lineno);
                    exit(1);
                }
            }
            //check coalescing - last block
            if (nextWalker > 4) {
                lastBlockBit = GETSIZEBIT(heapStart + nextWalker - 4) & 0x1;
                if (!lastBlockBit) {
                    printf("Coalescing error in free list %d, offset %d\n",
                            i, nextWalker - GETSIZE(heapStart + nextWalker - 4));
                    printf("Last block coalescing Error at line %d\n", lineno);
                    exit(1);
                }
            }
            lastWalker = nextWalker;
            nextWalker = GETINDEX(heapStart + nextWalker + 4);
            //check if nextwalker equals to the head of each free list(if there is cycle)
            if (nextWalker < -1) {
                printf("Next index error in free list %d, offet %d\n",
                        i, lastWalker);
                printf("Error at line %d\n", lineno);
                exit(1);
            }
        }

    }
}
