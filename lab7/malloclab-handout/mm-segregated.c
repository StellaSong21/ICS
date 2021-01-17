/**
 * mm-segregated.c
 * This version is implemented by segregated free list,
 *   find free block by first-fit.
 *
 * Running on my computer:
 *    94  =  55 + 39
 * (total = util+thru)
 *
 * Heap is organized like this:
 *  ---------------------------------------------------
 * |       Pointer to free_list0, size: [0, 8]         | (8 bytes)
 * |       Pointer to free_list1, size: [9,16]         | (8 bytes)
 * |       Pointer to free_list2, size: [17,32]        | (8 bytes)
 * |                  …… …… …… ……                      |
 * | Pointer to list(SEG_MAX-1), size: > 2^(SEG_MAX+1) | (8 bytes)
 * |                    Heap_list                      |
 * |                Free block header                  |
 * |                Free block footer                  |
 * |                    Content                        |
 * |                 Epilogue header                   |
 *  ---------------------------------------------------
 *
 *
 * Allocated block struct is like this:
 *   31     ...             3 | 2  1  0
 *  --------------------------------------
 * | xx ... xx(size, 29 bits) | 0 a/f a/f | (header)
 * |            content ...               |
 *  --------------------------------------
 * For allocated blocks, only have header, without footer.
 * (header = size | is_prev_alloc | is_current_alloc)
 * 
 *
 * Free block struct is like this:
 *   31     ...             3 | 2  1  0
 *  --------------------------------------
 * | xx ... xx(size, 29 bits) | 0 a/f a/f | (header, 4bytes)
 * |   Pointer to next block in the list  | (4 bytes)
 * |   Pointer to prev block in the list  | (4 bytes)
 * |             free words ...           | (free)
 * | xx ... xx(size, 29 bits) | 0 a/f a/f | (footer, 4 bytes)
 *  --------------------------------------
 * For free blocks, have both header and footer.
 * In the appropriate list, they have address of the prev and next block.
 * Two pointers are both relative starting address of the corresponding list.
 * In each list, free blocks are organized by doubly listed list.
 * 
 * 
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "mm.h"
#include "memlib.h"

/*
 * If you want debugging output, use the following macro.
 * When you hand in, remove the #define DEBUG line.
 */
//#define DEBUG
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
#endif
/* def DRIVER */

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8
/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(p) (((size_t)(p) + (ALIGNMENT-1)) & ~0x7)
/* Basic constants and macros */
#define WSIZE       4       /* Word and header/footer size (bytes) */
#define DSIZE       8       /* Double word size (bytes) */
#define MABLOCK     16      /* Least allocated block size(bytes) */
#define CHUNKSIZE  (1<<8)   /* Extend heap by this amount (bytes) */
/* Get max in x and y */
#define MAX(x, y) ((x) > (y)? (x) : (y))
/* Pack a size and allocated bit into a word */
#define PACK(size, alloc)  ((size) | (alloc))
/* Read and write a word at address p */
#define GET(p)           (*(unsigned int *)(p))
#define PUT(p, val)      (*(unsigned int *)(p) = (val))
/* Num of free lists */
#define SEG_MAX 12
/* Read the size and allocated fields from address p */
#define GET_SIZE(p) (GET(p) & ~0x07)
#define GET_ALLOC(p) (GET(p) & 0x01)
/* Set allocated fields at address p */
#define SET_ALLOC(p) (PUT(p, GET(p) | 0x01))
/* Read, set and reset prev_allocated fields from address p */
#define GET_PREV_ALLOC(p) ((GET(p) & 0x02) >> 1)
#define SET_PREV_ALLOC(p)  (PUT(p, GET(p) | 0x02))
#define RESET_PREV_ALLOC(p) (PUT(p, GET(p) & ~0x02))
/* Given block ptr bp, get address of its header and footer */
#define HDRP(p) (((char *)(p)) - WSIZE)
#define FTRP(p) (((char *)(p)) + GET_SIZE(HDRP(p)) - DSIZE)
/* Given block ptr bp, get address of the word store pointer to prev and next blocks */
#define GET_PREV_PTR(p) ((char *)(p) + WSIZE)
#define GET_NEXT_PTR(p) ((char *)(p))
/* Given block ptr bp, get address of prev and next blocks (in the list) */
#define GET_PREV_VAL(p) GET(GET_PREV_PTR(p))
#define GET_NEXT_VAL(p) GET(GET_NEXT_PTR(p))
/* Write a word at address prev and next in block p */
#define SET_PREV_VAL(p, val) PUT(GET_PREV_PTR(p), val)
#define SET_NEXT_VAL(p, val) PUT(GET_NEXT_PTR(p), val)
/* Given block ptr bp, get address of next and previous blocks in the block address */
#define PREV_BLKP(p) (((char *)(p)) - GET_SIZE(HDRP(p) - WSIZE))
#define NEXT_BLKP(p) (FTRP(p) + DSIZE)

/* 0 = best-fit; 1 = next-fit; 2 = address-first-fit search(same as 0 when implicit-free-list); else use first-fit search */
#define FIT 3
/* 0, insert by address; 1, insert in head; else insert in the end */
#define INSERT 1

/* Global variable */
static char *heap_start_addr;
static char *heap_listp;
static char *free_listp;
#if FIT == 1
static char *rover;
#endif

/* Function prototypes for internal helper routines */
static inline unsigned int get_list_index(size_t size);

static void *next_free_blk(void *ptr);

static void *prev_free_blk(void *ptr);

static void insert_freelist(void *bp);

static void remove_freelist(void *bp);

static void *find_fit(size_t asize);

static void place(void *bp, size_t asize);

static void *coalesce(void *ptr);

static void *extend_heap(size_t words);

static void *find_first_fit(size_t size);

static void *find_next_fit(size_t size);

static void *find_best_fit(size_t size);

static unsigned int check_by_list(int verbose);

static unsigned int check_by_address(int verbose);

/*****************************************************************
 * Initialize: initialize the heap as the struct in the head      *
 * comment as as well as global variables. In which, each pointer *
 * to segregated free list has length of 8 bytes, with init value *
 * 8 * i (class index).                                           *
 * For global variables:                                          *
 *  1.heap_start_addr: the start address of the heap, which is    *
 * 0x800000000 in this lab.                                       *
 *  2.free_listp: pointer to the array of the heads of the free   *
 * lists equals to heap_start_addr in the design.                 *
 *  3.heap_listp: pointer to the blocks in the heap, points to the*
 * free block footer in this design.                              *
 * After init, the heap and global variables like:                *
 *      heap_start_addr              heap_listp                   *
 *      |                                  |                      *
 *      |   seg array  (SEG_MAX * 8)   | 4 | 4 | 4 | 4 |          *
 *      |                                                         *
 *      free_listp                                                *
 *****************************************************************/
int mm_init(void) {
    if ((heap_listp = mem_sbrk(4 * WSIZE + SEG_MAX * DSIZE)) == (void *) -1) {
        return -1;
    }

    heap_start_addr = heap_listp;
    free_listp = heap_listp;

    for (unsigned int i = 0; i < SEG_MAX; i++) {
        PUT(heap_listp + i * DSIZE, i * DSIZE);
        PUT(heap_listp + WSIZE + i * DSIZE, i * DSIZE);
    }

    heap_listp += SEG_MAX * DSIZE;
    PUT(heap_listp, 0);
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1));
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1));
    PUT(heap_listp + (3 * WSIZE), PACK(0, 3));
    heap_listp += DSIZE;

#if FIT == 1
    rover = free_listp;
#endif

#ifdef DEBUG
    mm_checkheap(__LINE__);
#endif

    return 0;
}


/******************************************************************
 * Malloc:                                                         *
 *  1. If heap_listp = NULL, init the heap by calling mm_init.     *
 *  2. If size <= 0, ignore it and return NULL.                    *
 *  3. Get the a_size to be malloc and find a approriate block in  *
 *     the list. If find, malloc a_size in the block; else, extend *
 *     the heap by calling mem_sbrk in the memlib, malloc          *
 *     MAX(a_size, (1 << 8)) bytes, then after corresponding mark, *
 *     insert it in the free list, and place a_size in it.         *
 *******************************************************************/
void *malloc(size_t size) {
    size_t asize, extendsize;      /* Adjusted block size */

    char *bp;

    if (heap_listp == 0) {
        mm_init();
    }

    /* Ignore spurious requests */
    if (size <= 0)
        return NULL;

    /* Adjust block size to include overhead and alignment reqs. */
    asize = MAX(MABLOCK, ALIGN(size + WSIZE));

    /* Search the free list for a fit */
    if ((bp = find_fit(asize)) == NULL) {/* No fit found. Get more memory and place the block */
        /* Amount to extend heap if no fit */
        extendsize = ALIGN(MAX(asize, CHUNKSIZE));
        if ((bp = mem_sbrk(extendsize)) == (void *) -1) {
            return NULL;
        }
        PUT(bp + extendsize - WSIZE, 1);

        unsigned int alloc = GET_PREV_ALLOC(bp - WSIZE) << 1;

        PUT(HDRP(bp), PACK(extendsize, alloc));
        PUT(FTRP(bp), PACK(extendsize, alloc));

        bp = coalesce(bp);
    }
    place(bp, asize);
#ifdef DEBUG
    mm_checkheap(__LINE__);
#endif
    return bp;
}

/***************************************************************
 * Free:                                                       *
 *  1.If ptr = NULL, do nothing.                               *
 *  2.If heap_listp = NULL, init the heap by calling mm_init   *
 *  3.Set the current block free, and reset the prev_alloc_bit *
 *    in the header of the next blocks.                        *
 *  4.Coalesce with adjacent free blocks.                      *
 ***************************************************************/
void free(void *ptr) {
    if (ptr == 0)
        return;

    if (heap_listp == 0) {
        mm_init();
    }

    /* set alloc bit to 0 */
    PUT(HDRP(ptr), GET(HDRP(ptr)) & ~0x01);
    PUT(FTRP(ptr), GET(HDRP(ptr)) & ~0x01);

    /* reset the prev_alloc_bit in the header of the next blocks. */
    RESET_PREV_ALLOC(HDRP(NEXT_BLKP(ptr)));

#if FIT == 1
    rover = coalesce(ptr);
#else
    coalesce(ptr);
#endif

#ifdef DEBUG
    mm_checkheap(__LINE__);
#endif
}

/*************************************************************
 * Realloc:                                                  *
 *  Change the size of the block by mallocing a new block,   *
 *  copying its data, and freeing the old block. The address *
 *  of the new block may be same with the old.               *
 *************************************************************/
void *realloc(void *oldptr, size_t size) {
    size_t oldsize, newsize;
    void *newptr, *temp;

    /* If size == 0 then this is just free, and we return NULL. */
    if (size == 0) {
        mm_free(oldptr);
        return 0;
    }
    /* If oldptr is NULL, then this is just malloc. */
    if (oldptr == NULL) {
        return mm_malloc(size);
    }

    oldsize = GET_SIZE(HDRP(oldptr));
    newsize = MAX(ALIGN(size + WSIZE), MABLOCK);

    /* 1. oldsize = newsize */
    if (oldsize == newsize) {
        return oldptr;
    }
    /* 2. newsize < oldsize, do not need to malloc new block */
    if (newsize < oldsize) {
        /* oldsize - newsize < 16(MIN_BLOCK_SIZE), then return oldptr */
        if (oldsize - newsize < MABLOCK)
            return oldptr;
        /* need to split and coalesce */
        PUT(HDRP(oldptr), PACK(newsize, GET_PREV_ALLOC(HDRP(oldptr)) << 1 | 1));

        temp = NEXT_BLKP(oldptr);
        PUT(HDRP(temp), PACK(oldsize - newsize, 2));
        PUT(FTRP(temp), PACK(oldsize - newsize, 2));
#if FIT == 1
        rover = coalesce(temp);
#else
        coalesce(temp);
#endif

#ifdef DEBUG
        mm_checkheap(__LINE__);
#endif
        return oldptr;
    }

    /* 3. newsize > oldsize */
    temp = NEXT_BLKP(oldptr);
    /* next block is free and space is enough */
    if ((!GET_ALLOC(HDRP(temp))) && (oldsize + GET_SIZE(HDRP(temp)) >= newsize)) {
        place(temp, newsize - oldsize);
        PUT(HDRP(oldptr), PACK(oldsize + GET_SIZE(HDRP(temp)), GET_PREV_ALLOC(HDRP(oldptr)) << 1 | 1));
#if FIT == 1
        rover = oldptr;
#endif
#ifdef DEBUG
        mm_checkheap(__LINE__);
#endif
        return oldptr;
    }

    /* malloc new block */
    newptr = mm_malloc(size);

    /* If realloc() fails the original block is left untouched */
    if (!newptr) {
        return 0;
    }

    /* Copy the old data. */
    oldsize = GET_SIZE(HDRP(oldptr));
    if (size < oldsize) oldsize = size;
    memcpy(newptr, oldptr, oldsize);

    /* Free the old block. */
    mm_free(oldptr);
#ifdef DEBUG
    mm_checkheap(__LINE__);
#endif
    return newptr;
}


/******************************************
 * Calloc:                                *
 *  Allocate the block and set it to zero.*
 ******************************************/
void *calloc(size_t nmemb, size_t size) {
    size_t bytes = nmemb * size;
    void *newptr = mm_malloc(bytes);
    memset(newptr, 0, bytes);
#ifdef DEBUG
    mm_checkheap(__LINE__);
#endif
    return newptr;
}

/* Given a size return the size class. */
static inline unsigned int get_list_index(size_t size) {
    unsigned int index = 0;
    size = size >> 3;
    while (size >> index) index++;
    return index > SEG_MAX ? SEG_MAX - 1 : index - 1;
}

/* Given ptr, return pointer to the next free block in the same list */
static inline void *next_free_blk(void *ptr) {
    unsigned int val = GET_NEXT_VAL(ptr);
    return heap_start_addr + val;
}

/* Given ptr, return pointer to the prev free block in the same list */
static inline void *prev_free_blk(void *ptr) {
    unsigned int val = GET_PREV_VAL(ptr);
    return heap_start_addr + val;
}

/* remove request block from free list */
static inline void remove_freelist(void *ptr) {
    unsigned int prev_val = GET_PREV_VAL(ptr);
    unsigned int next_val = GET_NEXT_VAL(ptr);

    char *prev = heap_start_addr + prev_val;
    char *next = heap_start_addr + next_val;

    SET_NEXT_VAL(prev, next_val);
    SET_PREV_VAL(next, prev_val);
}

/* Insert the request block to the free list. */
static inline void insert_freelist(void *ptr) {
    unsigned int index = get_list_index(GET_SIZE(HDRP(ptr)));
    char *start = free_listp + DSIZE * index;
    unsigned int next_val = GET_NEXT_VAL(start);
    unsigned int cur_val = ((char *) ptr) - heap_start_addr;

    /* set block self */
    SET_PREV_VAL(ptr, DSIZE * index);
    SET_NEXT_VAL(ptr, next_val);
    /* set next block pred point */
    SET_PREV_VAL(heap_start_addr + next_val, cur_val);
    /* set prev block next point */
    SET_NEXT_VAL(start, cur_val);
}

/* Segregated fits. */
static void *find_fit(size_t size) {
#if FIT == 0
    return find_best_fit(size);
#elif FIT == 1
    return find_next_fit(size);
#else
    return find_first_fit(size);
#endif
}

/***************************************************************
 * Segregated fits.                                            *
 * We determine the size of class of the request and do        *
 * a first-fit search of the appropriate free list for a block *
 * that fits. If we cannot find a block that fits, then we     *
 * search the free list for the next larger size class.        *
 * If none of the free lists yields a block that fits, then    *
 * returns null.                                               *
 ***************************************************************/
static void *find_first_fit(size_t size) {
    /* Get the size class */
    unsigned int index = get_list_index(size);
    char *ptr;
    char *free_start_addr;

    for (; index < SEG_MAX; index++) {
        /* the start address of the size class list */
        free_start_addr = free_listp + DSIZE * index;
        ptr = heap_start_addr + GET_NEXT_VAL(free_start_addr);
        while (ptr != free_start_addr) {
            /* first fit */
            if (size <= GET_SIZE(HDRP(ptr))) {
                return ptr;
            }
            /* next free block in the list */
            ptr = heap_start_addr + GET_NEXT_VAL(ptr);
        }
    }
    return NULL;
}

/***************************************************************
 * Segregated fits.                                            *
 * We determine the size of class of the request and do        *
 * a next-fit search of the appropriate free list for a block  *
 * that fits. If we cannot find a block that fits, then we     *
 * search the free list for the next larger size class.        *
 * If none of the free lists yields a block that fits, then    *
 * returns null.                                               *
 ***************************************************************/
static void *find_next_fit(size_t size) {
#if FIT == 1
    char *ptr = rover;
    char *temp;

    do {
        temp = HDRP(ptr);
        /* next fit */
        if (GET_ALLOC(temp) == 0 && size <= GET_SIZE(temp)) {
            rover = ptr;
            return ptr;
        }

        ptr = NEXT_BLKP(ptr);

        /* the end of the heap */
        if (GET(HDRP(ptr)) == 1)
            ptr = heap_listp;

    } while (ptr != rover);
#endif
    return NULL;
}

/****************************************************************
 * Segregated fits.                                             *
 * We determine the size of class of the request and do         *
 * a best-fit search of the appropriate free list for a block   *
 * that best fits. If we cannot find a block that fits, then we *
 * search the free list for the next larger size class.         *
 * If none of the free lists yields a block that fits, then     *
 * returns null.                                                *
 ****************************************************************/
static void *find_best_fit(size_t size) {
    char *ptr = heap_start_addr + GET_NEXT_VAL(free_listp);
    char *best_ptr = NULL;
    size_t size_gap = UINT_MAX;
    char *temp;

    while (ptr != heap_start_addr) {
        temp = HDRP(ptr);
        /* find free && have enough space */
        if (size <= GET_SIZE(temp)) {
            /* first or the size of gap smaller than the last one */
            if (GET_SIZE(temp) - size < size_gap) {
                size_gap = GET_SIZE(temp) - size;
                best_ptr = ptr;
                /* best-fit: equal */
                if (size_gap == 0) {
                    return best_ptr;
                }
            }
        }

        ptr = heap_start_addr + GET_NEXT_VAL(ptr);
    }

    return best_ptr;
}

/******************************************************************
 * Place:                                                         *
 *  Place the requested block at the beginning of the free block, *
 *  splitting only if the size of remainder would equal or exceed *
 *  the minimum block size.                                       *
 ******************************************************************/
static void place(void *ptr, size_t size) {
    size_t blocksize = GET_SIZE(HDRP(ptr));

    /* the remainder cannot be allocated */
    if ((blocksize - size) < MABLOCK) {
        remove_freelist(ptr);

        PUT(HDRP(ptr), PACK(blocksize, 3));

        SET_PREV_ALLOC(HDRP(NEXT_BLKP(ptr)));
    } else {
        /* split */
        remove_freelist(ptr); /* remove ptr from the list first */

        /* set both alloc_bit and prev_alloc_bit 1 */
        /* the prev block must be allocated, otherwise will be coalesce */
        PUT(HDRP(ptr), PACK(size, 3));
        PUT(FTRP(ptr), PACK(size, 3));

        /* get the remainder */
        ptr = NEXT_BLKP(ptr);

        /* set alloc_bit free and prev_alloc_bit alloc */
        PUT(HDRP(ptr), PACK(blocksize - size, 2));
        PUT(FTRP(ptr), PACK(blocksize - size, 2));

        insert_freelist(ptr);/* insert remainder into the list first */
    }
}

/******************************************************
 * Coalesce:                                          *
 *  Merge adjacent free blocks by remove the adjacent *
 *  free blocks from list -> merge -> insert new free *
 *  block.                                            *
 *  Return a free block point after coalesce finish.  *
 *  The next-fit need return the point.               *
 ******************************************************/
static void *coalesce(void *ptr) {
    void *prev;
    void *next = NEXT_BLKP(ptr);
    size_t size = GET_SIZE(HDRP(ptr));

    unsigned int prev_alloc = GET_PREV_ALLOC(HDRP(ptr));/* if the prev block alloc or nor */
    unsigned int next_alloc = GET_ALLOC(HDRP(next));

    /* case 1 pre:alloc, next:alloc */
    if (prev_alloc && next_alloc) {
        insert_freelist(ptr);
#ifdef DEBUG
        mm_checkheap(__LINE__);
#endif
        return ptr;
    }
    /* case 2 pre:alloc, next:free */
    if (prev_alloc) {
        remove_freelist(next);
        size += GET_SIZE(HDRP(next));
        PUT(HDRP(ptr), PACK(size, 2));
        PUT(FTRP(ptr), PACK(size, 2));
        insert_freelist(ptr);
#ifdef DEBUG
        mm_checkheap(__LINE__);
#endif
        return ptr;
    }
    /* case 3 pre:free, next:alloc */
    if (next_alloc) {
        prev = PREV_BLKP(ptr);
        remove_freelist(prev);
        size += GET_SIZE(HDRP(prev));
        PUT(HDRP(prev), PACK(size, 2));
        PUT(FTRP(prev), PACK(size, 2));
        insert_freelist(prev);
#ifdef DEBUG
        mm_checkheap(__LINE__);
#endif
        return prev;
    }
    /* case 4 pre:free, next:free */
    prev = PREV_BLKP(ptr);
    remove_freelist(next);
    remove_freelist(prev);
    size += GET_SIZE(HDRP(prev)) + GET_SIZE(HDRP(next));
    PUT(HDRP(prev), PACK(size, 2));
    PUT(FTRP(prev), PACK(size, 2));
    insert_freelist(prev);
#ifdef DEBUG
    mm_checkheap(__LINE__);
#endif
    return prev;


}

/**********************************************
 * Return whether the pointer is in the heap. *
 * May be useful for debugging.               *
 **********************************************/
static int in_heap(const void *p) {
    return p <= mem_heap_hi() && p >= mem_heap_lo();
}

/******************************************
 * Return whether the pointer is aligned. *
 * May be useful for debugging.           *
 ******************************************/
static int aligned(const void *p) {
    return (size_t)ALIGN(p) == (size_t) p;
}

/* cheak if there something wrong about the allocator */
void mm_checkheap(int verbose) {
    unsigned int free_heap = 0;/* number of free blocks got traversing the heap by address */
    unsigned int free_list = 0;/* number of free blocks got traversing the free lists */

    dbg_printf("%d ====================================\n", verbose);

    /* check by address in the heap */
    free_heap = check_by_address(verbose);

    /* check by list */
    free_list = check_by_list(verbose);

    dbg_printf("Free list count: %d, Heap free list count: %d\n", free_list, free_heap);

    /* check free block num in heap equal with in free list */
    if (free_heap != free_list) {
        printf("Error: free block num error \n");
        exit(1);
    }
    dbg_printf("%d ====================================\n", verbose);
}

/**********************************************
 * cheak by traversing the free lists.        *
 * Return number of free blocks in the lists. *
 **********************************************/
static unsigned int check_by_list(int verbose) {
    char *ptr;
    char *free_start_addr;
    unsigned int free_num = 0;

    for (unsigned int i = 0; i < SEG_MAX; i++) {
        free_start_addr = free_listp + DSIZE * i; /* the start address of the current list */
        ptr = heap_start_addr + GET_NEXT_VAL(free_start_addr); /* the first block in the list */
        while (ptr != free_start_addr) {
            if ((void *) ptr > mem_heap_hi() || (void *) ptr < mem_heap_lo()) { /* cheak if the ptr in the heap */
                dbg_printf("Error: block address %p is not in the heap\n", ptr);
                exit(-1);
            }
            free_num++;
            ptr = heap_start_addr + GET_NEXT_VAL(ptr);/* get the next block in the list */
        }
    }
    return free_num;
}

/**********************************************
 * cheak by traversing the heap by address.   *
 * Return number of free blocks in the heap.  *
 **********************************************/
static unsigned int check_by_address(int verbose) {
    unsigned int free_num = 0; /* number of free blocks */
    unsigned int last_alloc = 0; /* is the prev block alloc or not */
    char *temp;
    unsigned int temp_size;
    char *ptr = heap_listp; /* the first block */

    while (GET_SIZE(HDRP(ptr))) {
        temp = HDRP(ptr);
        temp_size = GET_SIZE(temp);
        if (GET_ALLOC(temp)) {/* if the block alloc */
            dbg_printf("At %p: header = %p | size = %08x | alloc_bit = %u \n", ptr,
                       temp, temp_size, GET(temp) & 0x3);
            last_alloc = 1;
        } else { /* if the block free */
            dbg_printf("At %p: header = %p | size = %08x | alloc_bit = %u | prev_free_blk = %p | next_free_blk = %p \n",
                       ptr, temp, temp_size, GET(temp) & 0x3,
                       prev_free_blk(ptr), next_free_blk(ptr));
            free_num++;
            if (!last_alloc) { /* if last_alloc == 0, means coalesce error */
                dbg_printf("Error: %x bytes at %p not coalesce\n", temp_size, ptr);
                exit(-1);
            }
            last_alloc = 0;
            if (GET(HDRP(ptr)) != GET(FTRP(ptr))) { /* check if the header = footer */
                dbg_printf("Error: head != foot at %p\n", ptr);
                exit(-1);
            }
        }
        ptr = NEXT_BLKP(ptr); /* get the next block in the heap */
        if (GET_PREV_ALLOC(HDRP(ptr)) != last_alloc) { /* check if the prev_alloc_bit correct */
            dbg_printf("Error: pre alloc bit at %p is %u\n", ptr, GET(HDRP(ptr)) & 0x3);
        }
    }

    /* check epilogue */
    temp = HDRP(ptr);
    dbg_printf("At epilogue %p: header = %p | size = %08x | alloc_bit = %u \n", ptr, temp, GET_SIZE(temp),
               GET(temp) & 0x3);
    dbg_printf("free_listp: %p\n", free_listp);
    return free_num;
}
