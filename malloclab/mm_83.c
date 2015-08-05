/*
 * mm.c
 *
 * comment that gives a high level description of your solution.
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

#define TSIZE 4
#define WSIZE 8
#define SET(p, size, bit) (*(int*)(p) = ((size) | (bit)))
#define SETP(p, addr) (*(int*)(p)) = ((int)(addr))
#define GETSIZEBIT(p) (*(int*)(p))
#define GETSIZE(p) (GETSIZEBIT(p) & (~0x1))
#define GETINDEX(p) (*(int*)(p))
//#define GETPTR(p) ((void *)(*((unsigned long*)(p))))
//#define GETPTR(p) (*(void**)(p))
#define LIST 11

static void* heap;
static char* flist;
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
size_t seglist[LIST];

/*
 * Initialize: return -1 on error, 0 on success.
 */
int mm_init(void) {
    if ((heap = mem_sbrk(32 * WSIZE)) == (void*)-1)
        return -1;
    int i = 0;
    for (; i < LIST; i ++)
        seglist[i] = -1;
    SET(heap, 32 * WSIZE, 0);
    SET(heap + 32 * WSIZE - 4, 32 * WSIZE, 0);
    SETP(heap + 4, -1);
    seglist[5] = heap - (void*)mem_heap_lo();
    SETP(heap + 32 * WSIZE - 8, -7);
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
    newSize = alignSize + 2 * TSIZE + 2 * TSIZE;
    listIndex = find_list(newSize);
    if ((sp = find_fit(newSize, listIndex)) != -1) {
        place(mem_heap_lo() + sp, newSize);
        return (mem_heap_lo() + sp + 8);
    }

    //need to allocate extra space for heap
    void* newStartAddress;
    size_t newHeapSpace = newSize > (16 * WSIZE) ? newSize : (16 * WSIZE);
    if ((newStartAddress = (void*)mem_sbrk(newHeapSpace)) == (void*)-1)
        return (void*)-1;

    extendFree(newStartAddress, newHeapSpace);
    sp = find_fit(newSize, listIndex);
    place(mem_heap_lo() + sp, newSize);
    return (mem_heap_lo() + sp + 8);

}

/*
 * extendFree - set extended free blocks
 */

void extendFree(void* sp, int size) {
    SET(sp, size, 0);
    SET(sp + size - 4, size, 0);

    doCoalescing(sp, size);
}

/*
 * free
 */
void free (void *ptr) {
    if(!ptr) return;

    if (heap == 0)
        mm_init();
    //free the block pointed by ptr
    size_t bit = (GETSIZEBIT(ptr - 8)) & 0x1;
    if (bit == 0)
        return;
    size_t size = GETSIZE(ptr - 8);

    SET(ptr - 8, size, 0);
    SET(ptr - 8 + size - 4 , size, 0);

    doCoalescing(ptr - 8, size);
}


/*
 * insertfreeblock - insert new free block into new seglist
 *
 */
void insertOneFreeBlock(void* ptr, int size) {
    int index = find_list(size);
    int oldNextIndex = seglist[index];

    int tempSize = 0;
    SETP(ptr + 4, oldNextIndex);
    SETP(ptr + size - 8, -index - 2);
    seglist[index] = (int)(ptr - mem_heap_lo());
    if (oldNextIndex != -1) {
        tempSize = GETSIZE(mem_heap_lo() + oldNextIndex);
        SETP(mem_heap_lo() + oldNextIndex + tempSize - 8, (int)(ptr - mem_heap_lo()));
    }
}
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
    seglist[index] = (int)(newHead - mem_heap_lo());
    SETP(newHead + size - 8, -index - 2);
    if (nextIndex != -1) {
        tempSize = GETSIZE(mem_heap_lo() + nextIndex);
        SETP(mem_heap_lo() + nextIndex + tempSize - 8, (int)(newHead - mem_heap_lo()));
    }
}
void insertThreeFreeBlock(void* fptr, void* nptr, int size, int oldSize1, int oldSize2) {
    int index = find_list(size);
    int oldPreIndex = GETINDEX(fptr + oldSize1 - 8);
    int oldNextIndex = GETINDEX(fptr + 4);

    int tempSize = 0;
    if (oldPreIndex >= 0) {
        SETP(mem_heap_lo() + oldPreIndex + 4, oldNextIndex);
    } else {
        seglist[-(oldPreIndex + 2)] = oldNextIndex;
    }
    if (oldNextIndex != -1) {
        tempSize = GETSIZE(mem_heap_lo() + oldNextIndex);
        SETP(mem_heap_lo() + oldNextIndex + tempSize - 8, oldPreIndex);
    }

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
    seglist[index] = (int)(fptr - mem_heap_lo());
    SETP(fptr + size - 8, -index - 2);
    if (nextIndex != -1) {
        tempSize = GETSIZE(mem_heap_lo() + nextIndex);
        SETP(mem_heap_lo() + nextIndex + tempSize - 8, (int)(fptr - mem_heap_lo()));
    }
}


/*
 * do coalescing
 */

void doCoalescing(void* ptr, int size) {
    int fFree = 1, nFree = 1;
    int fSize = 0, nSize = 0;

    if ((ptr - 4) >= (heap + 4)) {
       fSize = GETSIZEBIT(ptr - 4);
       fFree = fSize & 0x1;
    }

    if ((ptr + size) < (mem_heap_hi() + 1 - 64)) {
        nSize = GETSIZEBIT(ptr + size);
        nFree = nSize & 0x1;
    }

    if (fFree && nFree) { //before and next are 1
        insertOneFreeBlock(ptr, size);
    } else if (!fFree && nFree) { //before is 0, next is 1
        insertTwoFreeBlock(ptr - fSize, ptr - fSize, fSize + size, fSize);
        SET(ptr - fSize, fSize + size, 0);
        SET(ptr + size - 4, fSize + size, 0);
    } else if (fFree && !nFree) { //before is 1, next is 0
        insertTwoFreeBlock(ptr,  ptr + size, size + nSize, nSize);
        SET(ptr, size + nSize, 0);
        SET(ptr + size + nSize - 4, size + nSize, 0);
    } else if (!fFree && !nFree){
        insertThreeFreeBlock(ptr - fSize, ptr + size, fSize + size + nSize, fSize, nSize);
        SET(ptr - fSize, size + fSize + nSize, 0);
        SET(ptr + size + nSize - 4, size + fSize + nSize, 0);
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
    size_t count = size < (oldSize  - 2 * TSIZE - 2 * 4)? size : (oldSize - 2 * TSIZE - 2 * 4);
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
    int base = 1;
    int count = 3;
    do {
        base = 1 << count;
        count ++;
    } while (count <= 13 && size > base);
 
    return count - 4;
}


/*
 * find_fit -  find the fit free blocks
 * use best-fit here
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

void reArrangeFreeBlock(void* fptr, void* cptr, int size, int oldSize) {
    //reset the allocated block of the free list
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
    //assert(oldNextIndex == -1 || in_heap(mem_heap_lo() + oldNextIndex) == 1);
    SETP(cptr + 4,oldNextIndex);
    SETP(cptr + GETSIZE(cptr) - 8, -newIndex - 2);
    seglist[newIndex] = (int)(cptr - mem_heap_lo());
    if (oldNextIndex >= 0) {
        tempSize = GETSIZE(mem_heap_lo() + oldNextIndex);
        SETP(mem_heap_lo() + oldNextIndex + tempSize - 8, seglist[newIndex]);
    }
}

/*
 * place - modify the new allocated block
 */
void place(void* sp, int size) {
    int oldSize = GETSIZE(sp);
    if (oldSize - size >= (2 * TSIZE + 2 * TSIZE)) {
        SET(sp, size, 1);
        SET(sp + size - 4, size, 1);
        SET(sp + size, oldSize - size, 0);
        SET(sp + oldSize - 4, oldSize - size, 0);
        reArrangeFreeBlock(sp, sp + size, oldSize - size, oldSize);
    } else {
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
 */
void mm_checkheap(int lineno) {
    char* ptr = heap;
    for (; ptr + GETSIZE(ptr); ptr = ptr + 1) {
        if (!in_heap(ptr))
            printf("Ptr not in heap! %d\n", lineno);
        if (!aligned(ptr))
            printf("Ptr not aligned! %d\n", lineno);
    }
}
