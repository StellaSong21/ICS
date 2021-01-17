/*
 * mm-segregated.c
 *todo
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>

#include "mm.h"
#include "memlib.h"
//todo
/* If you want debugging output, use the following macro.  When you hand
 * in, remove the #define DEBUG line. */
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
#define HDRP(p) (((char *)(p)) - WSIZE)
#define FTRP(p) (((char *)(p)) + GET_SIZE(HDRP(p)) - DSIZE)
/* Given block ptr bp, compute address of next blocks in the block address */
#define NEXT_BLKP(p) (FTRP(p) + DSIZE)//todo footer?
#define PREV_BLKP(p) ((char *)(p) - GET_SIZE(((char *)(p) - DSIZE)))
/****************** list ***********************/
/* Given block ptr bp, compute address of prev and next */
#define GET_NEXT_BLKL(p) ((char *)(p))
#define GET_PREV_BLKL(p) ((char *)(p) + WSIZE)
/* Given block ptr bp, compute address of prev and next blocks in the list */
#define GET_PREV_BLKV(p) GET(GET_PREV_BLKL(p))
#define GET_NEXT_BLKV(p) GET(GET_NEXT_BLKL(p))
/* Write a word at address prev and next in block p */
#define SET_PREV_BLKV(p, val) PUT(GET_PREV_BLKL(p), val)
#define SET_NEXT_BLKV(p, val) PUT(GET_NEXT_BLKL(p), val)
/***************** end list *******************/

/***** heap *****/
/* Given BST block ptr bp, compute address of its left child or right child */
#define LCHLD_BLKPREF(bp)        (((void *) *)((char *)(bp) + DSIZE))
#define RCHLD_BLKPREF(bp)        (((void *) *)((char *)(bp) + DSIZE * 2))
/* Crap-like triple pointer > < */
#define PARENT_CHLDSLOTPREF(bp)    ((block_ptr **)((char *)(bp) + DSIZE * 3))

#define LCHLD_BLKP(bp) (*LCHLD_BLKPREF(bp))
#define RCHLD_BLKP(bp) (*RCHLD_BLKPREF(bp))
#define PARENT_CHLDSLOTP(bp) (*PARENT_CHLDSLOTPREF(bp))
/*** end heap ***/

/* Given block ptr bp, get prev_alloc */
#define GET_PREV_ALLOC(bp)    (GET(HDRP(bp)) & 0x02)
/* Given block ptr bp, set the next block's prev_alloc */
#define SET_ALLOC(p) (GET(HDRP(NEXT_BLKP(p))) |= 0x02)
#define SET_UNALLOC(p) (GET(HDRP(NEXT_BLKP(p))) &= ~0x02)


/* Global variables */
static char *heap_start_addr;
static char *heap_listp;  /* Pointer to first block */
static char *tree_root;   /* Pointer to the root of the tree */
static char *list_root;   /* Array of the roots of the lists */
static const unsigned int LIST_NUM = 2; /* Number of lists to record small blocks */


#define ON_TREE(size) ((size) > DSIZE * LIST_NUM)

/* Function prototypes for internal helper routines */
static void *coalesce(void *bp);

static void *extend_heap(void *words);

static void place(void *bp, size_t asize);

static void *find_fit(size_t asize);

static void insert_free_block(void *bp, size_t blocksize);

static void printblock(void *bp);

static void checkblock(void *bp);

//todo list：分离适配链表--16bytes（header+prev+next+footer+8bytes）；
/*
 * Initialize: return -1 on error, 0 on success.
 */
/* todo
    --必要的初始化，栈空间的初始化
    --正常返回0；出错返回-1
    --必须初始化全局的指针
    --不能调用mem_init
    --每次执行程序之前，都会调用这个函数重设整个heap
 */
int mm_init(void) {
    if ((heap_listp = mem_sbrk(LIST_NUM * WSIZE + 4 * WSIZE)) == (void *) -1)
        return -1;
    heap_start_addr = heap_listp;
    memset(list_root, 0, WSIZE * LIST_NUM);
    heap_listp = ALIGN(list_root + LIST_NUM);
    tree_root = 0;
    PUT(heap_listp, 0);
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1));
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1));
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1));
    heap_listp += DSIZE;

    SET_ALLOC(heap_listp);

    return 0;
}

/*
 * malloc
 */
//todo 寻找空闲块的策略
/* todo
    --返回一个指向至少size bytes的已分配的空间
    --该空间不会被其他的使用
    --8-byte对齐
 */
void *malloc(size_t size) {
    size_t asize;      /* Adjusted block size */
    size_t extendsize;
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
        extendsize = MAX(asize, CHUNKSIZE);
        extendsize = asize;
        if ((bp = extend_heap(extendsize / WSIZE)) == (void *) -1)
            return NULL;
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
    SET_UNALLOC(ptr);

    reset_block(ptr);

    void *freeblock = coalesce(ptr);
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
        if (oldsize - newsize < DSIZE)
            return oldptr;
        // oldsize - newsize > 16 -> need to split and coalesce
        //todo 优化处理
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

    newptr = mm_malloc(size);

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
    void *newptr = mm_malloc(bytes);
    memset(newptr, 0, bytes);

    return newptr;
}

/*
 * coalesce - Boundary tag coalescing. Return ptr to coalesced block
 */
static void *coalesce(block_ptr bp) {
    /*
     * TODO Here is the bug: Do update the bins while doing this.
     * Tried to fix. Not sure what will happen.
     */
    void *next = NEXT_BLKP(ptr);

    /* Use GET_PREV_ALLOC to judge if prev block is allocated */
    size_t prev_alloc = GET_PREV_ALLOC(bp);
    size_t next_alloc = GET_ALLOC(HDRP(next));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc)            /* Case 1 */
    {
        insert(bp);
        return bp;
    } else if (prev_alloc && !next_alloc)      /* Case 2 */
    {
        remove(next);
        size += GET_SIZE(HDRP(next));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    } else if (!prev_alloc && next_alloc)       /* Case 3 */
    {
        prev = PREV_BLKP(bp);
        remove(prev);
        size += GET_SIZE(HDRP(prev));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(prev), PACK(size, 0));
        bp = prev;
    } else                                      /* Case 4 */
    {
        prev = PREV_BLKP(bp);
        remove_freed_block(next);
        remove_freed_block(prev);
        size += GET_SIZE(HDRP(prev)) + GET_SIZE(FTRP(next));
        PUT(HDRP(prev), PACK(size, 0));
        PUT(FTRP(next), PACK(size, 0));
        bp = prev;
    }
    reset_block(bp);
    //todo insert
    insert(bp, size);
    return bp;
}

/*
 * extend_heap - Extend heap with free block and return its block pointer
 */
static block_ptr extend_heap(size_t words) {
    char *bp;
    size_t size;

    /* Allocate an even number of words to maintain alignment */
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    if ((long) (bp = mem_sbrk(size)) == -1)
        return NULL;

    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(size, 0));            /* Free block header */
    PUT(FTRP(bp), PACK(size, 0));            /* Free block footer */
    //todo reset
    reset_block(bp);
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));    /* New epilogue header */

    /* Coalesce if the previous block was free */
    return coalesce(bp);
}

/*
 * place - Place block of asize bytes at start of free block bp
 *		 and split if remainder would be at least minimum block size
 */
static void place(block_ptr bp, size_t asize) {
    size_t csize = GET_SIZE(HDRP(bp));
    size_t size_gap = csize - asize;

    if (size_gap < 2 * DSIZE) {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
        SET_ALLOC(bp);
    } else {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        SET_ALLOC(bp);
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(size_gap, 0));
        PUT(FTRP(bp), PACK(size_gap, 0));
        //todo 不需要？
        SET_UNALLOC(bp);
        //todo reset
        reset_block(bp);
        insert(bp, size_gap);
    }
}


/*
 * insert_free_block - insert a block into BST or segregated free list
 * BLOCKSIZE should be duplicate of double word
 */
static void insert(block_ptr bp, size_t blocksize) {
    if (!ON_TREE(blocksize)) {
        insert_list(bp, blocksize);
    } else {
        insert_tree(bp, blocksize);
    }
}

static void insert_list(block_ptr bp, size_t blocksize) {
/* Insert into segregated free list */
    size_t which = blocksize / DSIZE;
    //todo + which???
    unsigned int next_val = GET_NEXT_BLKL(list_root + which);
    SET_PREV_BLKV(bp, 0);
    SET_NEXT_BLKV(bp, next_val;)
    if (next_val) {
        SET_PREV_BLKV(heap_start_addr + next_val,
                      ((char *) bp) - heap_start_addr);
    }
    SET_NEXT_BLKV(list_root + which, ((char *) ptr) - (list_root + which));
}

static void insert_tree(block_ptr bp, size_t blocksize) {
/* Figure out where to put the new node in BST */
    (void *) *current = &tree_root;
    void *parent = 0;
    while (*current) {
        size_t curr_size = GET_SIZE(HDRP(parent = *current));
        if (blocksize < curr_size)
            current = LCHLD_BLKPREF(parent);
        else if (blocksize > curr_size)
            current = RCHLD_BLKPREF(parent);
        else {
            block_ptr next = word_to_ptr(NEXT_SAMESZ_BLKP(bp) = NEXT_SAMESZ_BLKP(parent));
            if (next)
                /* Connect it before the existing block as a new header */
                PREV_SAMESZ_BLKP(next) = ptr_to_word(bp);
            NEXT_SAMESZ_BLKP(parent) = ptr_to_word(bp);
            PREV_SAMESZ_BLKP(bp) = ptr_to_word(parent);
            return;
        }
    }

    /* Connect this node as a child */
    *current = bp;
    PARENT_CHLDSLOTP(bp) = current;
}

/*
 * find_fit - Find a fit for a block with asize bytes
 * asize should be duplicate of double word
 */
static block_ptr find_fit(size_t asize) {
    block_ptr curr, *blocks;
    size_t dcount = asize / DSIZE;

    if (!IS_OVER_BST_SIZE(asize)) {
        if (bins[dcount - 1]) {
            curr = bins[dcount - 1];
            bins[dcount - 1] = word_to_ptr(NEXT_SAMESZ_BLKP(curr));
            remove_freed_block(curr);
            return curr;
        }
    }

    if ((blocks = bestfit_search(&larger_bin_root, asize, 0)) == NULL)
        return NULL;

    curr = *blocks;


    remove_freed_block(curr);
    return curr;
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