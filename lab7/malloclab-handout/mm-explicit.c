/*
 * mm-explicit.c
 * explicit free list + first-fit | next-fit | best-fit
 *
 * Running on my computer:(total = util+thru)
 * explicit first-fit               74=37+37
 * explicit next-fit                44=36+7
 * explicit best-fit                57=45+12
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#include "mm.h"
#include "memlib.h"
//todo
/* If you want debugging output, use the following macro.  When you hand
 * in, remove the #define DEBUG line. */
// #define DEBUG
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
/* Basic constants and macros */
#define WSIZE       4       /* Word and header/footer size (bytes) */
#define DSIZE       8       /* Double word size (bytes) */
#define MABLOCK     16      /* Least allocated block size(bytes) */
#define MFBLOCK     32      /* Least free block size(bytes) */
#define CHUNKSIZE  (1<<12)  /* Extend heap by this amount (bytes) */
/* Get max in x and y */
#define MAX(x, y) ((x) > (y)? (x) : (y))
/* Pack a size and allocated bit into a word */
#define PACK(size, alloc)  ((size) | (alloc))
/* Read and write a word at address p */
#define GET(p)           (*(unsigned int *)(p))
#define PUT(p, val)      (*(unsigned int *)(p) = (val))
/* Read the size and allocated fields from address p */
#define GET_SIZE(p) (GET(p) & ~0x07)
#define GET_ALLOC(p) (GET(p) & 0x01)
/* Given block ptr bp, compute address of its header and footer */
#define HDPR(p) (((char *)(p)) - WSIZE)
#define FTPR(p) (((char *)(p)) + GET_SIZE(HDPR(p)) - DSIZE)
/* Given block ptr bp, compute address of prev and next */
#define GET_PREV_PTR(p) ((char *)(p) + WSIZE)
#define GET_NEXT_PTR(p) ((char *)(p))
/* Given block ptr bp, compute address of prev and next blocks in the list */
#define GET_PREV_VAL(p) GET(GET_PREV_PTR(p))
#define GET_NEXT_VAL(p) GET(GET_NEXT_PTR(p))
/* Write a word at address prev and next in block p */
#define SET_PREV_VAL(p, val) PUT(GET_PREV_PTR(p), val)
#define SET_NEXT_VAL(p, val) PUT(GET_NEXT_PTR(p), val)
/* Given block ptr bp, compute address of next and previous blocks in the block address */
#define PREV_BLKP(p) (((char *)(p)) - GET_SIZE(HDPR(p) - WSIZE))
#define NEXT_BLKP(p) (FTPR(p) + DSIZE)

/* 0 = best-fit; 1 = next-fit; 2 = address-first-fit search(same as 0 when implicit-free-list); else use first-fit search */
#define FIT 3
/* 0, insert by address; 1, insert in head; else insert in the end */
#define INSERT 1

/* Global variables */
static char *heap_listp;  /* Pointer to first free block */
static char *heap_start_addr; /*  */
static char *free_listp;      /* Pointer to free list */
#if FIT == 1
static char *rover;           /* Next fit rover */
#endif

/* Function prototypes for internal helper routines */
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

/*
 * Initialize: return -1 on error, 0 on success.
 */
int mm_init(void) {
    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *) -1)
        return -1;

    heap_start_addr = heap_listp;
    free_listp = heap_listp;

    PUT(heap_listp, 0);
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1));
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1));
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1));
    heap_listp += DSIZE;

#if FIT == 1
    rover = heap_listp;
#endif

    return 0;
}

/*
 * malloc
 */
void *malloc(size_t size) {
    size_t asize;      /* Adjusted block size */
    char *bp;

    if (heap_listp == 0) {
        mm_init();
    }
    /* Ignore spurious requests */
    if (size <= 0) {
        return NULL;
    }
    /* Adjust block size to include overhead and alignment reqs. */
    asize = ALIGN(size + DSIZE);
    /* Search the free list for a fit */
    if ((bp = find_fit(asize)) == NULL) {
        /* No fit found. Get more memory and place the block */
        size_t extendsize; /* Amount to extend heap if no fit */
//        extendsize = MAX(asize, CHUNKSIZE);
        extendsize = asize;
        if ((bp = mem_sbrk(extendsize)) == (void *) -1)
            return NULL;
        PUT(bp + extendsize - WSIZE, 1);
        PUT(HDPR(bp), PACK(extendsize, 0));
        PUT(FTPR(bp), PACK(extendsize, 0));

        insert_freelist(bp);
    }

    place(bp, asize);

    return bp;
}

/*
 * free
 */
void free(void *ptr) {
    if (ptr == 0)
        return;

    size_t size = GET_SIZE(HDPR(ptr));

    if (heap_listp == 0) {
        mm_init();
    }

    PUT(HDPR(ptr), PACK(size, 0));
    PUT(FTPR(ptr), PACK(size, 0));

#if FIT == 1
    rover = coalesce(ptr);
#else
    coalesce(ptr);
#endif
}

/*
 * realloc - you may want to look at mm-naive.c
 */
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

    oldsize = GET_SIZE(HDPR(oldptr));
    newsize = ALIGN(size + DSIZE);

    if (oldsize == newsize) {
        return oldptr;
    }
        // newsize < oldsize, do not need to alloc new block
    else if (newsize < oldsize) {
        if (oldsize - newsize < MABLOCK)
            return oldptr;
        // oldsize - newsize > 16 -> need to split and coalesce
        PUT(HDPR(oldptr), PACK(newsize, 1));
        PUT(FTPR(oldptr), PACK(newsize, 1));

        temp = NEXT_BLKP(oldptr);
        PUT(HDPR(temp), PACK(oldsize - newsize, 0));
        PUT(FTPR(temp), PACK(oldsize - newsize, 0));

#if FIT == 1
        rover = coalesce(temp);
#else
        coalesce(temp);
#endif
        return oldptr;
    } else {// newsize > oldsize
        temp = NEXT_BLKP(oldptr);
        if ((!GET_ALLOC(HDPR(temp))) && (oldsize + GET_SIZE(HDPR(temp)) >= newsize)) {
            // next block is free and space is enough
            place(temp, newsize - oldsize);
            PUT(HDPR(oldptr), PACK(oldsize + GET_SIZE(HDPR(temp)), 1));
            PUT(FTPR(oldptr), PACK(GET_SIZE(HDPR(oldptr)), 1));
#if FIT == 1
            rover = oldptr;
#endif
            return oldptr;
        }
    }

    newptr = malloc(size);

    // If realloc() fails the original block is left untouched
    if (!newptr) {
        return 0;
    }

    // Copy the old data.
    oldsize = GET_SIZE(HDPR(oldptr));
    if (size < oldsize) oldsize = size;
    memcpy(newptr, oldptr, oldsize);

    // Free the old block.
    mm_free(oldptr);

    return newptr;
}

/*
 * calloc - you may want to look at mm-naive.c
 * This function is not tested by mdriver, but it is
 * needed to run the traces.
 */
void *calloc(size_t nmemb, size_t size) {
    size_t bytes = nmemb * size;
    void *newptr;

    newptr = mm_malloc(bytes);
    memset(newptr, 0, bytes);

    return newptr;
}

/*
 * coalesce - merge adjacent free blocks
 *      Return a free block point after coalesce finish.
 *      The next find fit need return the point.
 */
static void *coalesce(void *ptr) {
    void *prev = PREV_BLKP(ptr);
    void *next = NEXT_BLKP(ptr);
    size_t size = GET_SIZE(HDPR(ptr));

    // case 1 pre:alloc, next:alloc
    if (GET_ALLOC(HDPR(prev)) && GET_ALLOC(HDPR(next))) {
        insert_freelist(ptr);

#ifdef DEBUG
        mm_checkheap(__LINE__);
#endif

        return ptr;
    }
        // case 2 pre:alloc, next:free
    else if (GET_ALLOC(HDPR(prev)) && (!GET_ALLOC(HDPR(next)))) {
        remove_freelist(next);
        size += GET_SIZE(HDPR(next));
        PUT(HDPR(ptr), PACK(size, 0));
        PUT(FTPR(ptr), PACK(size, 0));
        insert_freelist(ptr);
#ifdef DEBUG
        mm_checkheap(__LINE__);
#endif
        return ptr;
    }
        // case 3 pre:free, next:alloc
    else if ((!GET_ALLOC(HDPR(prev))) && GET_ALLOC(HDPR(next))) {
        size += GET_SIZE(HDPR(prev));
        PUT(HDPR(prev), PACK(size, 0));
        PUT(FTPR(prev), PACK(size, 0));

#ifdef DEBUG
        mm_checkheap(__LINE__);
#endif
        return prev;
    }
        // case 4 pre:free, next:free
    else {
        remove_freelist(next);

        size += GET_SIZE(HDPR(prev)) + GET_SIZE(HDPR(next));
        PUT(HDPR(prev), PACK(size, 0));
        PUT(FTPR(prev), PACK(size, 0));
#ifdef DEBUG
        mm_checkheap(__LINE__);
#endif
        return prev;
    }

}

static inline void *next_free_blk(void *ptr) {
    unsigned int val = GET_NEXT_VAL(ptr);
    return heap_start_addr + val;
}

static inline void *prev_free_blk(void *ptr) {
    unsigned int val = GET_PREV_VAL(ptr);
    return heap_start_addr + val;
}

/* remove request block from free list */
static inline void remove_freelist(void *ptr) {
    unsigned int prev_val = GET_PREV_VAL(ptr);
    unsigned int next_val = GET_NEXT_VAL(ptr);

    char *prev = free_listp + prev_val;
    char *next = free_listp + next_val;

    SET_NEXT_VAL(prev, next_val);

    if (next_val) {
        SET_PREV_VAL(next, prev_val);
    }
}

//LIFO
static inline void insert_freelist(void *ptr) {
    unsigned int next_val = GET_NEXT_VAL(free_listp);

    // set block self
    SET_PREV_VAL(ptr, 0);
    SET_NEXT_VAL(ptr, next_val);

    // set next block prev point, relative address
    if (next_val) {
        SET_PREV_VAL(heap_start_addr + next_val,
                     ((char *) ptr) - heap_start_addr);
    }
    // set prev block next point
    SET_NEXT_VAL(free_listp, ((char *) ptr) - heap_start_addr);
}

static void *find_fit(size_t size) {
#if FIT == 0
    return find_best_fit(size);
#elif FIT == 1
    return find_next_fit(size);
#else
    return find_first_fit(size);
#endif
}

static void *find_first_fit(size_t size) {
    char *ptr = heap_start_addr + GET_NEXT_VAL(free_listp);

    while (ptr != heap_start_addr) {
        if (size <= GET_SIZE(HDPR(ptr))) {
            return ptr;
        }
        ptr = heap_start_addr + GET_NEXT_VAL(ptr);
    }

    return NULL;
}

static void *find_next_fit(size_t size) {
#if FIT == 1
    char *ptr = rover;
    char *temp;

    do {
        temp = HDPR(ptr);
        if (GET_ALLOC(temp) == 0 && size <= GET_SIZE(temp)) {
            rover = ptr;
            return ptr;
        }

        ptr = NEXT_BLKP(ptr);

        if (GET(HDPR(ptr)) == 1)
            ptr = heap_listp;

    } while (ptr != rover);
#endif
    return NULL;
}

static void *find_best_fit(size_t size) {
    char *ptr = heap_start_addr + GET_NEXT_VAL(free_listp);
    char *best_ptr = NULL;
    size_t size_gap = UINT_MAX;
    char *temp;

    while (ptr != heap_start_addr) {
        temp = HDPR(ptr);
        // find free && have enough space
        if (size <= GET_SIZE(temp)) {
            // first or the size of gap smaller than the last one
            if (GET_SIZE(temp) - size < size_gap) {
                size_gap = GET_SIZE(temp) - size;
                best_ptr = ptr;
                if (size_gap == 0)
                    return best_ptr;
            }
        }

        ptr = heap_start_addr + GET_NEXT_VAL(ptr);
    }

    return best_ptr;
}


/*
 * place - Place the requested block at the beginning of the free block,
 *      splitting only if the size of remainder would equal or exceed
 *      the minimun block size.
 */
static void place(void *ptr, size_t size) {
    size_t blocksize = GET_SIZE(HDPR(ptr));

    if ((blocksize - size) < MABLOCK) {
        remove_freelist(ptr);

        PUT(HDPR(ptr), PACK(blocksize, 1));
        PUT(FTPR(ptr), PACK(blocksize, 1));
    } else {
        // split
        remove_freelist(ptr);

        PUT(HDPR(ptr), PACK(size, 1));
        PUT(FTPR(ptr), PACK(size, 1));

        ptr = NEXT_BLKP(ptr);
        PUT(HDPR(ptr), PACK(blocksize - size, 0));
        PUT(FTPR(ptr), PACK(blocksize - size, 0));
        insert_freelist(ptr);
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
    return (size_t)ALIGN(p) == (size_t) p;
}

void mm_checkheap(int verbose);