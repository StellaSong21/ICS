/*
 * mm-segregated-p8.c
 * Allocator implement by segregated free list.
 * Each allocated block struct like this
 *  31      ...           3| 2  1  0
 *  --------------------------------
 * | 00 ... size (29 bits) | 0 0 a/f| header 
 * |       content ...              |
 * |       content ...              |
 * | 00 ... size (29 bits) | 0 0 a/f| footer
 *  --------------------------------
 * The header encodes the block size as well as 
 * whether the block is allocated or free.
 *
 *
 * Each free block struct like this
 *  31      ...           3| 2  1  0
 *  --------------------------------
 * | 00 ... size (29 bits) | 0 0 a/f| header 
 * |   succ (successor addr low)    | 
 * |   succ (successor addr high)   | 
 * |   pred (predecessor addr low)  | 
 * |   pred (predecessor addr high) | 
 * | 00 ... size (29 bits) | 0 0 a/f| footer
 *  --------------------------------
 * All free blocks organized by doubly linked list.
 * 
 * The allocator maintains an array of free lists,
 * with one free list per size class, ordered by increasing size.
 * If given a allocate size n then the class size i is satisfied
 * (2^(i+3)) <= n < (2^(i+4)). 
 * If i >= SEG_MAX then i = SEG_MAX - 1.
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
#define WSIZE 4
#define DSIZE 8

#define CHUNKSIZE  (1<<8)

#define SEG_MAX 10
#define MIN_BLOCK_SIZE 24
#define POINT_SIZE 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define PACK(size, alloc) ((size) | (alloc))
#define GET(p) (*(unsigned int*)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

#define GET_SIZE(p) (GET(p) & ~0x07)
#define GET_ALLOC(p) (GET(p) & 0x01)

#define HDPR(p) (((char *)(p)) - WSIZE)
#define FTPR(p) (((char *)(p)) + GET_SIZE(HDPR(p)) - DSIZE)
#define NEXT_BLPR(p) (FTPR(p) + DSIZE)
#define PRE_BLPR(p) (((char *)(p)) - GET_SIZE(HDPR(p) - WSIZE))


#define GET_LONG(p) (*(unsigned long *)(p))
#define GET_POINT(p) ((char *)GET_LONG(p))
#define SET_POINT(p, val) (*(unsigned long *)(p) = ((unsigned long)(val)))
#define GET_PRED_POINT(p) GET_POINT(p + DSIZE)
#define GET_SUCC_POINT(p) GET_POINT(p)
#define SET_PRED_VAL(p, val) SET_POINT(p + DSIZE, val)
#define SET_SUCC_VAL(p, val) SET_POINT(p, val)

/* Global variables */
static char *heap_listp;
static char *next_find_ptr;
static char *heap_start_addr;
static char *free_listp;

/* Function prototypes for internal helper routines */
static unsigned int get_seg_index(size_t size);
static void *next_free_blk(void *ptr);
static void *pred_free_blk(void *ptr);
static void freelist_remove(void *ptr);
static void freelist_add(void *ptr);
static void *find_first_fit(size_t size);
static void *find_next_fit(size_t size);
static void *find_best_fit(size_t size);
static void place(void *ptr, size_t size);
static void *coalesce(void *ptr);
static unsigned int check_seg_list(int verbose);
static unsigned int check_whole_heap(int verbose);
/*
 * mm_init - Called when a new trace starts.
 * heap_start_addr              heap_listp
 * |                                 |
 * |   seg array  (30 * 4)   | 4 | 4 | 4 | 4 |
 * | 
 * free_listp
 */
int mm_init(void)
{
    // allocate init block -> | arrary size: 16 * SEG_MAX | 16bytes |  
    if ((heap_listp = mem_sbrk(4 * WSIZE + SEG_MAX * DSIZE * 2))
        == (void *)-1)
        return -1;

    char *temp;
    heap_start_addr = heap_listp;
    free_listp = heap_listp;

    for (unsigned int i = 0; i < SEG_MAX; i++)
    {
        temp = heap_listp + DSIZE * 2 * i;
        SET_PRED_VAL(temp, temp);
        SET_SUCC_VAL(temp, temp);
    }

    heap_listp += (SEG_MAX * DSIZE * 2);
    PUT(heap_listp, 0);
    PUT(heap_listp + WSIZE, PACK(8, 1));
    PUT(heap_listp + 2 * WSIZE, PACK(8, 1));
    PUT(heap_listp + 3 * WSIZE, PACK(0, 1));
    heap_listp = heap_listp + DSIZE;
    next_find_ptr = heap_listp;

    dbg_printf("init heap_listp:%p\n", heap_listp);
    dbg_printf("init heap_start_addr:%p\n", heap_start_addr);

    return 0;
}

/*
 * malloc - Allocate a block by incrementing the brk pointer.
 *      Always allocate a block whose size is a multiple of the alignment.
 */
void *malloc(size_t size)
{
    size_t newsize, sbrk_size;
    char *ptr;

    if (size <= 0)
        return NULL;
    newsize = ALIGN(size + DSIZE);
    newsize = MIN_BLOCK_SIZE > newsize ? MIN_BLOCK_SIZE : newsize;

    ptr = find_first_fit(newsize);
    // ptr = find_best_fit(newsize);   
    // ptr = find_next_fit(newsize);

    if (!ptr)
    {
        // call mem_sbrk
        sbrk_size = ALIGN(MAX(newsize, CHUNKSIZE));
        if ((ptr = mem_sbrk(sbrk_size)) == (void *)-1)
            return NULL;
        PUT(ptr + sbrk_size - WSIZE, 1);
        PUT(HDPR(ptr), PACK(sbrk_size, 0));
        PUT(FTPR(ptr), PACK(sbrk_size, 0));
        freelist_add(ptr);
        place(ptr, newsize);

        dbg_printf("malloc: %x, %x, %x, %p\n", (unsigned int)size, (unsigned int)newsize, (unsigned int)sbrk_size, ptr);
    }
    else
    {
        place(ptr, newsize);
        dbg_printf("malloc: %x, %x, %p\n", (unsigned int)size, (unsigned int)newsize, ptr);
    }

    return ptr;
}

/*
 * free - free allocted block 
 *      free(NULL) has no effect
 */
void free(void *ptr)
{
    size_t size;

    if (!ptr)
        return;

    dbg_printf("free: %p, %p \n", HDPR(ptr), ptr);

    size = GET_SIZE(HDPR(ptr));
    PUT(HDPR(ptr), PACK(size, 0));
    PUT(FTPR(ptr), PACK(size, 0));

    // freelist_add(ptr);

    next_find_ptr = coalesce(ptr);
}

/*
 * realloc - Change the size of the block by mallocing a new block,
 *      copying its data, and freeing the old block.  
 */
void *realloc(void *oldptr, size_t size)
{
    dbg_printf("realloc: %p, %x\n", oldptr, (unsigned int)size);
    size_t oldsize, newsize;
    void *newptr, *temp;

    // If size == 0 then this is just free, and we return NULL.
    if(size == 0)
    {
        free(oldptr);
        return 0;
    }

    // If oldptr is NULL, then this is just malloc.
    if(oldptr == NULL)
    {
        return malloc(size);
    }

    oldsize = GET_SIZE(HDPR(oldptr));
    newsize = ALIGN(size + DSIZE);
    newsize = MIN_BLOCK_SIZE > newsize ? MIN_BLOCK_SIZE : newsize;

    // oldsize == newsize, return oldptr
    if (oldsize == newsize)
    {
        return oldptr;
    }
        // newsize < oldsize, do not need to alloc new block
    else if (newsize < oldsize)
    {
        if (oldsize - newsize < MIN_BLOCK_SIZE)
            return oldptr;
        // oldsize - newsize > min block -> need to split and coalesce
        PUT(HDPR(oldptr), PACK(newsize, 1));
        PUT(FTPR(oldptr), PACK(newsize, 1));

        temp = NEXT_BLPR(oldptr);
        PUT(HDPR(temp), PACK(oldsize - newsize, 0));
        PUT(FTPR(temp), PACK(oldsize - newsize, 0));

        // freelist_add(temp);
        next_find_ptr = coalesce(temp);

        return oldptr;
    }
        // newsize > oldsize
    else
    {
        temp = NEXT_BLPR(oldptr);
        if ((!GET_ALLOC(HDPR(temp))) && (oldsize + GET_SIZE(HDPR(temp)) >= newsize))
        {
            // next block is free and space is enough
            place(temp, newsize - oldsize);
            PUT(HDPR(oldptr), PACK(oldsize + GET_SIZE(HDPR(temp)), 1));
            PUT(FTPR(oldptr), PACK(GET_SIZE(HDPR(oldptr)), 1));
            next_find_ptr = oldptr;
            return oldptr;
        }
    }

    newptr = malloc(size);

    // If realloc() fails the original block is left untouched
    if(!newptr)
    {
        return 0;
    }

    // Copy the old data. 
    oldsize = GET_SIZE(HDPR(oldptr));
    if(size < oldsize) oldsize = size;
    memcpy(newptr, oldptr, oldsize);

    // Free the old block. 
    free(oldptr);

    return newptr;
}

/*
 * calloc - Allocate the block and set it to zero.
 */
void *calloc(size_t nmemb, size_t size)
{
    size_t bytes = nmemb * size;
    void *newptr;

    newptr = malloc(bytes);
    memset(newptr, 0, bytes);

    return newptr;
}

/*
 * mm_checkheap - Show detail of cureent heap and do some check.
 *      block point| size | alloc | user point
 */
void mm_checkheap(int verbose)
{
    unsigned int freecount_heap = 0;
    unsigned int freecount_list = 0;

    dbg_printf("%d ====================================\n", verbose);

    // check whole heap
    freecount_heap = check_whole_heap(verbose);

    // check free list
    freecount_list = check_seg_list(verbose);

    dbg_printf("free list count: %d, heap free list count: %d\n", freecount_list, freecount_heap);

    // check free block num in heap equal with in free list  
    if (freecount_heap != freecount_list)
    {
        printf("error ! free block num errr \n");
        exit(1);
    }

    dbg_printf("%d ====================================\n", verbose);

}

/* Given a size return the size class. */
static inline unsigned int get_seg_index(size_t size)
{
    unsigned int index = 0;
    size = size >> 4; // because the min block size is 24 (11000)
    while (size >> index) index++;
    return index > SEG_MAX ? SEG_MAX - 1 : index - 1;
}

/* remove request block from free list */
static inline void freelist_remove(void *ptr)
{
    char *pred = GET_PRED_POINT(ptr);
    char *succ = GET_SUCC_POINT(ptr);

    SET_SUCC_VAL(pred, succ);
    SET_PRED_VAL(succ, pred);
}

static inline void freelist_add(void *ptr)
{
    unsigned int index = get_seg_index(GET_SIZE(HDPR(ptr)));
    char *start = free_listp + DSIZE * 2 * index;

    char *succ_p = GET_SUCC_POINT(start);

    // set block self
    SET_PRED_VAL(ptr, start);
    SET_SUCC_VAL(ptr, succ_p);

    // set succ block pred point
    SET_PRED_VAL(succ_p, ptr);
    // set pred block succ point
    SET_SUCC_VAL(start, ptr);
}

/* 
 * Segregated fits.
 * We determine the size of class of the request and do 
 * a first-fit search of the appropriate free list for
 * a block that fits. If we cannot find a block that fits,
 * then we search the free list for the next larger size class.  
 * If none of the free lists yields a block that fits, then returns null.
 */
static void *find_first_fit(size_t size)
{
    unsigned int index = 0;
    char *ptr;
    char *free_list_start;

    // compute the index of seg array by given size
    index = get_seg_index(size);

    for (; index < SEG_MAX; index++)
    {
        free_list_start = index * DSIZE * 2 + free_listp;
        ptr = GET_SUCC_POINT(free_list_start);
        while (ptr != free_list_start)
        {
            if (size <= GET_SIZE(HDPR(ptr)))
            {
                return ptr;
            }
            ptr = GET_SUCC_POINT(ptr);
        }
    }

    return NULL;
}

/*
 * place - Place the requested block at the beginning of the free block, 
 *      splitting only if the size of remainder would equal or exceed 
 *      the minimun block size.
 */
static void place(void *ptr, size_t size)
{
    size_t blocksize = GET_SIZE(HDPR(ptr));

    if ((blocksize - size) < MIN_BLOCK_SIZE)
    {
        freelist_remove(ptr);

        PUT(HDPR(ptr), PACK(blocksize, 1));
        PUT(FTPR(ptr), PACK(blocksize, 1));
    }
    else
    {
        // split
        freelist_remove(ptr);

        PUT(HDPR(ptr), PACK(size, 1));
        PUT(FTPR(ptr), PACK(size, 1));

        ptr = NEXT_BLPR(ptr);
        PUT(HDPR(ptr), PACK(blocksize - size, 0));
        PUT(FTPR(ptr), PACK(blocksize - size, 0));
        freelist_add(ptr);
    }
}

/*
 * coalesce - merge adjacent free blocks
 *      Return a free block point after coalesce finish. 
 *      The next find fit need return the point. 
 */
static void *coalesce(void *ptr)
{
    void *pre = PRE_BLPR(ptr);
    void *next = NEXT_BLPR(ptr);
    size_t size;

    // case 1 pre:alloc, next:alloc
    if (GET_ALLOC(HDPR(pre)) && GET_ALLOC(HDPR(next)))
    {
        freelist_add(ptr);
        return ptr;
    }
        // case 2 pre:alloc, next:free
    else if (GET_ALLOC(HDPR(pre)) && (!GET_ALLOC(HDPR(next))))
    {
        freelist_remove(next);

        size = GET_SIZE(HDPR(ptr)) + GET_SIZE(HDPR(next));
        PUT(HDPR(ptr), PACK(size, 0));
        PUT(FTPR(ptr), PACK(size, 0));

        freelist_add(ptr);
        return ptr;
    }
        // case 3 pre:free, next:alloc
    else if ((!GET_ALLOC(HDPR(pre))) && GET_ALLOC(HDPR(next)))
    {
        freelist_remove(pre);
        size = GET_SIZE(HDPR(ptr)) + GET_SIZE(HDPR(pre));
        PUT(HDPR(pre), PACK(size, 0));
        PUT(FTPR(pre), PACK(size, 0));
        freelist_add(pre);
        return pre;
    }
        // case 4 pre:free, next:free
    else
    {
        freelist_remove(pre);
        freelist_remove(next);

        size = GET_SIZE(HDPR(ptr)) + GET_SIZE(HDPR(pre)) + GET_SIZE(HDPR(next));
        PUT(HDPR(pre), PACK(size, 0));
        PUT(FTPR(pre), PACK(size, 0));

        freelist_add(pre);
        return pre;
    }
}

/* Check free block by seg array. Return the count of free blocks */
static unsigned int check_seg_list(int verbose)
{
    unsigned int index;
    char *ptr;
    char *free_list_start;
    unsigned int count = 0;
    unsigned count_temp;

    for (index = 0; index < SEG_MAX; index++)
    {
        count_temp = 0;
        free_list_start = index * DSIZE * 2 + free_listp;
        ptr = GET_SUCC_POINT(free_list_start);
        if (verbose > 2)
        {
            dbg_printf("%p:%p-", free_list_start, ptr);
        }
        while (ptr != free_list_start)
        {
            count++;
            count_temp++;
            ptr = GET_SUCC_POINT(ptr);
        }
        if (verbose > 2)
        {
            dbg_printf("%u:%u\n", index, count_temp);
        }
        printf("%03d ", count_temp);
    }
    printf("\n");

    return count;
}

static unsigned int check_whole_heap(int verbose)
{
    if (verbose < 0)
        return 0;
    unsigned int size_temp;
    char *temp;
    unsigned int heap_block_count = 0;
    unsigned int freecount_heap = 0;
    char *ptr = heap_listp;
    int last_free = 0;

    while (GET(HDPR(ptr)) != 0x01)
    {
        heap_block_count++;
        temp = HDPR(ptr);
        size_temp = GET_SIZE(temp);

        if (GET_ALLOC(temp))
        {
            dbg_printf("%p | %08x | %u | %p \n",
                       temp, size_temp, GET_ALLOC(temp), ptr);
            last_free = 0;
        }
        else
        {
            dbg_printf("%p | %08x | %u | %p | %p | %p \n", temp,
                       size_temp, GET_ALLOC(temp), ptr,
                       GET_PRED_POINT(ptr), GET_SUCC_POINT(ptr));
            freecount_heap++;
            if (last_free)
            {
                printf("%x error not coalesc!!! %p\n", size_temp, ptr);
                exit(1);
            }
            last_free = 1;
        }

        if (GET(HDPR(ptr)) != GET(FTPR(ptr)))
        {
            printf("error!! head != foot %p\n", ptr);
            exit(1);
        }

        // check minimun size
        if(size_temp < MIN_BLOCK_SIZE && heap_block_count > 1)
        {
            printf("error! found the error size\n");
            exit(0);
        }

        ptr = NEXT_BLPR(ptr);
    }

    // epilogue
    temp = HDPR(ptr);
    dbg_printf("%p | %08x | %u | %p \n", temp, GET_SIZE(temp), GET_ALLOC(temp), ptr);

    dbg_printf("free listp: %p\n", free_listp);

    return freecount_heap;
}

