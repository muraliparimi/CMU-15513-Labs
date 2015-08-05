/*
 * mm.c
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
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
#define DEBUG
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
#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define SIZE_PTR(p)  ((size_t*)(((char*)(p)) - SIZE_T_SIZE))

/*some useful macros:zijunz*/
 #define WSIZE 4
 #define DSIZE 8
 #define CHUNKSIZE  (1<<12)//why?

 #define MAX(x,y)  ((x)>(y) ? (x) : (y))

 #define PACK(size,alloc)  ((size) | (alloc))

 #define GET(p)  (*(unsigned int *)(p))
 #define PUT(p,val)  (*(unsigned int *)(p)=(val))

 #define GET_SIZE(p) (GET(p) & ~0x7)
 #define GET_ALLOC(p) (GET(p) & 0x1)

 #define HDRP(bp)  ((char *)(bp) - WSIZE)
 #define FTRP(bp)  ((char*)(bp)+GET_SIZE(HDRP(bp))-DSIZE)

 #define NEXT_BLKP(bp) ((char *)(bp) +GET_SIZE((char *)(bp)-WSIZE))
 #define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE((char *)(bp)-DSIZE))

//get the offset ptr
 #define PREV_OFFSET(bp) ((char *)(bp))
 #define NEXT_OFFSET(bp) ((char *)(bp)+WSIZE)
 //get the ptr of prev/next block
 #define PREV_PTR(bp) ((void *)(mem_heap_lo()+GET(PREV_OFFSET(bp))))
 #define NEXT_PTR(bp) ((void *)(mem_heap_lo()+GET(NEXT_OFFSET(bp))))


 //globals 
 static char* heap_listp=0;/*ptr to the prelogue*/
 static void* free_listp=NULL;/*ptr offset to the last free block*/

 //helper functions
 static void *extend_heap(size_t words);
 static void *coalesce(void *bp);
 static void *find_fit(size_t size);
 static void place(void *bp, size_t aligned_size);
 static void addFreeBlock(void *bp);
 static void deleteFreeBlock(void *bp);
 //static void printHeap();
/*
 * Initialize: return -1 on error, 0 on success.
 */
int mm_init(void) {
	if((heap_listp =mem_sbrk(4*WSIZE))==(void *)-1){
		return -1;
	}
	PUT(heap_listp,0);//first unused word
	PUT(heap_listp+WSIZE,PACK(DSIZE,1));//prologue header
	PUT(heap_listp+2*WSIZE,PACK(DSIZE,1));//prologue footer
	PUT(heap_listp+3*WSIZE,PACK(0,1));//epilogue header
	heap_listp+=(2*WSIZE);
	free_listp=NULL;

	if(extend_heap(CHUNKSIZE/WSIZE)==NULL){//fail to extend
		return -1;
	}
	return 0;
}

static void *extend_heap(size_t words){
	char *bp;
	size_t size;

	size=(words%2) ? (words+1)*WSIZE : words*WSIZE;
	if((long)(bp=mem_sbrk(size))==-1){
		return NULL;
	}
	PUT(HDRP(bp),PACK(size,0));
	PUT(FTRP(bp),PACK(size,0));
	PUT(HDRP(NEXT_BLKP(bp)),PACK(0,1));
	return coalesce(bp);
}

static void deleteFreeBlock(void *bp){
	//initialize
	char *prev=NULL;
	char *next=NULL;
	//delete block
	if(GET(PREV_OFFSET(bp))==0 && GET(NEXT_OFFSET(bp))==0){//only element
		free_listp=NULL;//empty
		PUT(PREV_OFFSET(bp),0);
		PUT(NEXT_OFFSET(bp),0);
	}
	else if(GET(PREV_OFFSET(bp))!=0 && GET(NEXT_OFFSET(bp))==0){//last one
		prev=(char *)((unsigned long)GET(PREV_OFFSET(bp))+
					(unsigned long)mem_heap_lo());
		PUT(NEXT_OFFSET(prev),0);
		PUT(PREV_OFFSET(bp),0);
		PUT(NEXT_OFFSET(bp),0);
		free_listp=prev;
	}
	else if(GET(PREV_OFFSET(bp))==0 && GET(NEXT_OFFSET(bp))!=0){//first one
		next=(char *)((unsigned long)GET(NEXT_OFFSET(bp))+
					(unsigned long)mem_heap_lo());
		PUT(PREV_OFFSET(next),0);
		PUT(PREV_OFFSET(bp),0);
		PUT(NEXT_OFFSET(bp),0);
	}
	else{//in the middle
		prev=(char *)((unsigned long)GET(PREV_OFFSET(bp))+
					(unsigned long)mem_heap_lo());
		next=(char *)((unsigned long)GET(NEXT_OFFSET(bp))+
					(unsigned long)mem_heap_lo());
		PUT(PREV_OFFSET(next),GET(PREV_OFFSET(bp)));
		PUT(NEXT_OFFSET(prev),GET(NEXT_OFFSET(bp)));
		PUT(PREV_OFFSET(bp),0);
		PUT(NEXT_OFFSET(bp),0);
	}
}

static void addFreeBlock(void *bp){
	if(free_listp==NULL){//empty
		free_listp=bp;
		PUT(PREV_OFFSET(bp),0);
		PUT(NEXT_OFFSET(bp),0);
	}
	else{
		PUT(NEXT_OFFSET(free_listp),(unsigned int)(bp-mem_heap_lo()));
		PUT(PREV_OFFSET(bp),(unsigned int)(free_listp-mem_heap_lo()));
		PUT(NEXT_OFFSET(bp),0);
		free_listp=bp;
	}
}

static void *coalesce(void *bp){
	size_t prev_alloc=GET_ALLOC(FTRP(PREV_BLKP(bp)));
	size_t next_alloc=GET_ALLOC(HDRP(NEXT_BLKP(bp)));
	size_t size=GET_SIZE(HDRP(bp));

	if(prev_alloc && next_alloc){//both allocated
		addFreeBlock(bp);
		return bp;
	}
	else if (prev_alloc && ! next_alloc){
		deleteFreeBlock(NEXT_BLKP(bp));
		addFreeBlock(bp);

		size+=GET_SIZE(HDRP(NEXT_BLKP(bp)));
		PUT(HDRP(bp),PACK(size,0));//new header
		PUT(FTRP(bp),PACK(size,0));//new footer
	}
	else if(!prev_alloc && next_alloc){
		//deleteFreeBlock(PREV_BLKP(bp));
		//addFreeBlock(PREV_BLKP(bp));

		size+=GET_SIZE(FTRP(PREV_BLKP(bp)));
		PUT(FTRP(bp),PACK(size,0));
		PUT(HDRP(PREV_BLKP(bp)),PACK(size,0));
		bp=PREV_BLKP(bp);
	}
	else{
		//deleteFreeBlock(PREV_BLKP(bp));
		deleteFreeBlock(NEXT_BLKP(bp));
		//addFreeBlock(PREV_BLKP(bp));

		size+=GET_SIZE(HDRP(NEXT_BLKP(bp)))+
			GET_SIZE(FTRP(PREV_BLKP(bp)));
		PUT(HDRP(PREV_BLKP(bp)),PACK(size,0));
		PUT(FTRP(NEXT_BLKP(bp)),PACK(size,0));
		bp=PREV_BLKP(bp);
	}
	//mm_checkheap(203);
	return bp;
}

/*
 * malloc
 */
void *malloc (size_t size) {
	size_t aligned_size;
	size_t extend_size;
	char *bp;
	//printf("size to malloc:%d\n",(int )(size));
	if(size==0){
		return NULL;
	}

	if(size<=DSIZE){
		aligned_size=2*DSIZE;
	}else{
		aligned_size=DSIZE*((size+2*DSIZE-1)/DSIZE);
	}

	if((bp=find_fit(aligned_size))!=NULL){
		place(bp,aligned_size);
		return bp;
	}

	extend_size=MAX(aligned_size,CHUNKSIZE);
	if((bp=extend_heap(extend_size/WSIZE))==NULL){
		return NULL;
	}
	place(bp,aligned_size);
	//mm_checkheap(233);
	return bp;
}

/*
 * find fit for given size,return pointer of the block
 * first-fit
 */
 static void *find_fit(size_t aligned_size){
 	void *bp=free_listp;
 	//printf("current free_listp: %p\n",free_listp);
 	//printHeap();
 	if(free_listp==NULL){//no free block
 		return NULL;
 	}
 	else if(GET(PREV_OFFSET(bp))==0){
 		if(GET_SIZE(HDRP(bp))>=aligned_size){
 			return bp; 
 		}
 	}
 	else {
 		//printf("searching for block...\n");
 		//printf("%p\n", PREV_PTR(free_listp));
 		for(bp=free_listp;GET(PREV_OFFSET(bp))!=0;bp=PREV_PTR(bp)){
 			if(GET_SIZE(HDRP(bp))>=aligned_size){//must be free
 				//printf("available space :%d\n\n",GET_SIZE(HDRP(bp)));
 				return bp;
 			} 
 		}
 		if(GET_SIZE(HDRP(bp))>=aligned_size){//must be free
 				//printf("available space :%d\n\n",GET_SIZE(HDRP(bp)));
 				return bp;
 		} 
 	}
 	
 	//printf("miss\n\n");
 	return NULL;
 }

 static void place(void *bp, size_t aligned_size){
 	size_t new_size=GET_SIZE(HDRP(bp));
 	if((new_size-aligned_size)>=2*DSIZE){//there will be a free block split
 		PUT(HDRP(bp),PACK(aligned_size,1));
 		PUT(FTRP(bp),PACK(aligned_size,1));
 		PUT(HDRP(NEXT_BLKP(bp)),PACK(new_size-aligned_size,0));
 		PUT(FTRP(NEXT_BLKP(bp)),PACK(new_size-aligned_size,0));
 		deleteFreeBlock(bp);
 		addFreeBlock(NEXT_BLKP(bp));
 	}
 	else{
 		PUT(HDRP(bp),PACK(new_size,1));
 		PUT(FTRP(bp),PACK(new_size,1));
 		deleteFreeBlock(bp);
 	}
 	//mm_checkheap(293); 
 }

/*
 * free
 */
void free (void *ptr) {
    	if(!ptr) return;
    	size_t size=GET_SIZE(HDRP(ptr));

    	PUT(HDRP(ptr),PACK(size,0));
    	PUT(FTRP(ptr),PACK(size,0));
    	coalesce(ptr);
    	//mm_checkheap(279); 
}

/*
 * realloc - you may want to look at mm-naive.c
 */
void *realloc(void *oldptr, size_t size) {
    	size_t oldsize;
  	void *newptr;

  	/* If size == 0 then this is just free, and we return NULL. */
  	if(size == 0) {
    		free(oldptr);
    		return 0;
  	}

  	/* If oldptr is NULL, then this is just malloc. */
  	if(oldptr == NULL) {
    		return malloc(size);
  	}

  	newptr = malloc(size);

  	/* If realloc() fails the original block is left untouched  */
  	if(!newptr) {
    		return 0;
  	}

  	/* Copy the old data. */
  	oldsize = *SIZE_PTR(oldptr);
  	if(size < oldsize) oldsize = size;
  	memcpy(newptr, oldptr, oldsize);

  	/* Free the old block. */
  	free(oldptr);

  	return newptr;
}

/*
 * calloc - you may want to look at mm-naive.c
 * This function is not tested by mdriver, but it is
 * needed to run the traces.
 */
void *calloc (size_t nmemb, size_t size) {
  	size_t bytes = nmemb * size;
  	void *newptr;

  	newptr = malloc(bytes);
  	memset(newptr, 0, bytes);

  	return newptr;
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


// static void printHeap(){
// 	char *bp=heap_listp;
// 	for(bp=heap_listp;GET_SIZE(HDRP(bp))>0;bp=NEXT_BLKP(bp)){
// 		size_t size= GET_SIZE(HDRP(bp));
// 	 	int is_alloc= GET_ALLOC(HDRP(bp));
// 	 	printf("block ptr:%p; block size:%d; is_allocated: %d\n",bp,(int)size,is_alloc);
// 	}
// }
/*
 * mm_checkheap
 */
void mm_checkheap(int lineno) {
	char *bp=heap_listp;
	char *lb=mem_heap_lo();
	char *ub=mem_heap_hi();
	/* check prologue */
	if (lb!=bp-DSIZE){
		printf("illegal prologue  position %d\n",lineno);
		exit(0);
	}

	/*check blocks*/
	for(bp=heap_listp;GET_SIZE(HDRP(bp))>0;bp=NEXT_BLKP(bp)){
		size_t size= GET_SIZE(HDRP(bp));
	 	int is_alloc= GET_ALLOC(HDRP(bp));
	 	if(!in_heap(bp)){
	 		printf("not int heap\n");
	 		exit(0);
	 	}
 		if(!aligned(bp)){
 			printf("block not aligned %d\n",lineno);
			exit(0);
 		}
 		if (GET(HDRP(bp))!=PACK(size,is_alloc)){
 			printf("invalid header %d\n",lineno);
			exit(0);
 		}
 		if (GET(FTRP(bp))!=PACK(size,is_alloc)) {
 			printf("invalid footer %d\n",lineno);
			exit(0);
 		}
 		if (!is_alloc){
 			if(!GET_ALLOC(FTRP(PREV_BLKP(bp))) ||
 				!GET_ALLOC(HDRP(NEXT_BLKP(bp))) ){
 				printf("invalid coalesce %d\n",lineno);
				exit(0);
 			}
 		}
 	}

 	if(bp!=ub+1){//incorrect epilogue
 		printf("illegal epilogue %d\n",lineno);
 		printf("%p\n",bp);
 		printf("%p\n",mem_heap_hi());
 	}

	return;
}
