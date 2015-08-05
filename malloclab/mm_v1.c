/*
 * mm.c
 *
 * Si Lao sil@andrew.cmu.edu
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
#define ALIGN(p) (((size_t)(p) + (ALIGNMENT-1)) & ~0x7)

#define WSIZE 8
#define SET(p, size, bit) ((*(unsigned int*)(p)) = ((size) | (bit)))
#define GETSIZEBIT(p) (*(unsigned int*)(p))
#define GETSIZE(p) (GETSIZEBIT(p) & (~0x1))

static char* heap;
static char* flist;
static void* find_fit(size_t size);
static void place(void* sp, size_t size);
static void extendFree(void* sp, size_t size);
static void doCoalescing(void* ptr, size_t size);

/*
 * Initialize: return -1 on error, 0 on success.
 */
int mm_init(void) {
    if ((heap = (char *)mem_sbrk(16 * WSIZE)) == (char*)-1)
        return -1;

    SET(heap, 16 * WSIZE, 0);
    SET(heap + 16 * WSIZE - 8, 16 * WSIZE, 0);
    flist = heap;

    return 0;
}

/*
 * malloc
 */
void *malloc (size_t size) {
    //printf("malloc size%d\n", (int)size);
    if (size == 0)
        return NULL;
    size_t newSize;
    void* sp;

    //adjust size
    newSize = ALIGN(size) + 2 * WSIZE;
    //printf("new size is %d\n", (int)newSize);
    if ((sp = find_fit(newSize)) != (void*)-1) {
        place(sp, newSize);
        //printf("address is %d\n", (int)(sp - heap));
        return (sp + 8);
    }

    //need to allocate extra space for heap
    void* newStartAddress;
    size_t newHeapSpace = newSize > (16 * WSIZE) ? newSize : (16 * WSIZE);
    if ((newStartAddress = (void*)mem_sbrk(newHeapSpace)) == (void*)-1)
        return (void*)-1;
    //printf("new address is %d\n", (int)(newStartAddress - heap));
    extendFree(newStartAddress, newHeapSpace);
    sp = find_fit(newSize);
    place(sp, newSize);
    //printf("address is %d\n", (int)(sp - heap));
    return (sp + 8);

}

/*
 * extendFree - set extended free blocks
 */

void extendFree(void* sp, size_t size) {
    SET(sp, size, 0);
    SET(sp + size - 8, size, 0);
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
    size_t size = GETSIZE(ptr - 8);
    SET(ptr - 8, size, 0);
    SET(ptr - 8 + size - 8, size, 0);
    doCoalescing(ptr - 8, size);
}

/*
 * do coalescing
 */

void doCoalescing(void* ptr, size_t size) {
    void* former;
    void* latter;
    size_t formerSize = 0, latterSize = 0;
    int formerFree = 1;
    int latterFree = 1;
    if (ptr - 8 >= (void*)heap + 8) {
        formerSize = GETSIZEBIT(ptr - 8);
        formerFree = formerSize & 0x1;
    }
    //printf("formersize is %d\n", (int)formerSize);

    if (ptr + size <= mem_heap_hi() - 7) {
        latterSize = GETSIZEBIT(ptr + size);
        latterFree = latterSize & 0x1;
    }
    //printf("lattersize is %d\n", (int)latterSize);

    if (!formerFree && latterFree) {
        former = ptr - formerSize;
        SET(former, formerSize + size, 0);
        //printf("former val is %d\n", GETSIZE(former));
        SET(ptr + size - 8, formerSize + size, 0);
        //printf("in1\n");
    }
    if (formerFree && !latterFree) {
        latter = ptr + size;
        SET(ptr, latterSize + size, 0);
        SET(latter + latterSize - 8, latterSize + size, 0);
        //printf("in2\n");
    }
    if (!formerFree && !latterFree) {
        former = ptr - formerSize;
        latter = ptr + size;
        SET(former, formerSize + size + latterSize, 0);
        SET(latter + latterSize - 8, formerSize + size + latterSize, 0);
        //printf("in3\n");
    }
}

/*
 * realloc - you may want to look at mm-naive.c
 */
void *realloc(void *oldptr, size_t size) {
    //printf("realloc size%d\n", (int)size);
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
    size_t count = size < (oldSize  - 2 * WSIZE)? size : (oldSize - 2 * WSIZE);
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
    //printf("calloc size%d\n", (int)size);
    size_t bytes = nmemb * size;
    void* newPtr;

    newPtr = malloc(bytes);
    if (!newPtr)
        return NULL;
    bytes = GETSIZE(newPtr - 8);
    size_t i = 0;
    for (i = 0; i < bytes; i ++)
        *(char*)(newPtr + i) = 0;

    return newPtr;
}

/*
 * find_fit -  find the fit free blocks
 * use best-fit here
 */
void* find_Bestfit(size_t size) {
    size_t sizeAndBit;
    char* walker = flist;
    char* tail = (char*)mem_heap_hi() - 7;
    while (walker <= tail) {
        sizeAndBit = GETSIZEBIT(walker);
        if (!(sizeAndBit & 0x1)) {
            if (sizeAndBit >= size) {
                return (void*)walker;
            }
            walker += sizeAndBit;
        } else {
            walker += (sizeAndBit - 1);
        }
    }
    return (void*)-1;
}
void* find_fit(size_t size) {
    void* result = 0;
    int flag = 0;
    size_t temp;
    size_t sizeAndBit;
    char* walker = flist;
    char* tail = (char*)mem_heap_hi() - 7;
    while (walker <= tail) {
        sizeAndBit = GETSIZEBIT(walker);
        if (!(sizeAndBit & 0x1)) {
            if (sizeAndBit >= size) {
                if (!flag) {
                    flag = 1;
                    temp = sizeAndBit;
                    result = walker;
                } else {
                    result = temp < sizeAndBit ? result :walker;
                    temp = temp < sizeAndBit ? temp : sizeAndBit;
                }
            }
            walker += sizeAndBit;
        } else {
            walker += (sizeAndBit - 1);
        }
    }
    return (flag? result : (void*)-1);
}

/*
 * place - modify the new allocated block
 */
void place(void* sp, size_t size) {
    //printf("in place\n");
    size_t oldSize = GETSIZE(sp);
    //printf("%d\n", (int)oldSize);
    if (oldSize - size >= 2 * 8) {
        //printf("1\n");
        SET(sp, size, 1);
        //printf("2\n");
        SET(sp + size - 8, size , 1);
        //printf("3\n");
        SET(sp + size, oldSize - size, 0);
        //printf("4\n");
        SET(sp + oldSize - 8, oldSize - size, 0);
        //printf("5\n");
    } else {
        //printf("6\n");
        SET(sp, oldSize, 1);
        //printf("7\n");
        SET(sp + oldSize - 8, oldSize, 1);
        //printf("8\n");
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
