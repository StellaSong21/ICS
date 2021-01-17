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
#define CHUNKSIZE  (1<<8)  /* Extend heap by this amount (bytes) */
/* Get max in x and y */
#define MAX(x, y) ((x) > (y)? (x) : (y))
/* Pack a size and allocated bit into a word */
#define PACK(size, alloc)  ((size) | (alloc))
/* Read and write a word at address p */
#define GET(p)           (*(unsigned int *)(p))
#define PUT(p, val)      (*(unsigned int *)(p) = (val))
#define PUTTRUNC(p, val)
/* Read the size and allocated fields from address p */
#define GET_SIZE(p) (GET(p) & ~0x07)
#define GET_ALLOC(p) (GET(p) & 0x01)
/* Given block ptr bp, compute address of its header and footer */
#define HDPR(p) (((char *)(p)) - WSIZE)
#define FTPR(p) (((char *)(p)) + GET_SIZE(HDPR(p)) - DSIZE)

/* Get the 64 bit pointer of the address stored in the address p */
#define GET_PTR(p)              (void *)(GET(p)|0x800000000)
#define PTR2INT(p)              (unsigned int)(long)p

/* Determine whether the previous block is allocated and free from header */
#define PREV_ALLOC(header_ptr)  (GET(header_ptr) & (0x2))
#define PREV_FREE(header_ptr)   (GET(header_ptr) & (0x4))

#define SET_PREV_ALLOC(bp)      (GET(HDRP(bp)) |= 0x2)
#define RESET_PREV_ALLOC(bp)    (GET(HDRP(bp)) &= ~0x2)
#define SET_PREV_FREE(bp)       (GET(HDRP(bp)) |= 0x4)
#define RESET_PREV_FREE(bp)     (GET(HDRP(bp)) &= ~0x4)

/*Given pointer p at the second word of the data structure, compute addresses
of its RIGHT,PARENT and SIBLING pointer, left is itself */
#define RIGHT(p)                (p + WSIZE)
#define PARENT(p)               (p + 2 * WSIZE)
#define SIBLING(p)              (p + 3 * WSIZE)

/*Given block pointer bp, get the POINTER of its directions*/
#define GET_PREV(bp)            (PREV_FREE(HDRP(bp)) ? (bp - DSIZE): (bp - GET_SIZE(bp - DSIZE)) )
#define GET_NEXT(bp)            (bp + GET_SIZE(bp - WSIZE))


static void *extend_heap(size_t size);

static void place(void *bp, size_t asize);

static void *find_fit(size_t asize);

static void *coalesce(void *bp);

static inline int is_prev_free(void *bp);

static void insert_node(void *bp);

static void delete_node(void *bp);

static char *heap_listp;
static char *heap_start_addr;
static void *root;//root of the BST
static void *list_16;//head of the 16-byte list
static void *list_8;//head of the 8-byte list

int mm_init(void) {
    /* create the initial empty heap */
    if ((heap_listp = mem_sbrk(6 * WSIZE)) == (void *) -1)
        return -1;
    heap_start_addr = heap_listp;
    PUT(heap_listp + (2 * WSIZE), 0);              /* Alignment padding */
    PUT(heap_listp + (3 * WSIZE), PACK(DSIZE, 1)); /* Prologue header */
    PUT(heap_listp + (4 * WSIZE), PACK(DSIZE, 1)); /* Prologue footer */
    PUT(heap_listp + (5 * WSIZE), PACK(0, 3));     /* Epilogue header */
    heap_listp += (4 * WSIZE);
    /*init the global variables*/
    root = heap_start_addr;
    list_16 = heap_start_addr;
    list_8 = heap_start_addr;
    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    if (extend_heap(CHUNKSIZE) == 0)
        return -1;
    return 0;
}

void *malloc(size_t size) {
    char *bp;

    if (heap_listp == 0) {
        mm_init();
    }

    /* Ignore spurious requests */
    if (size <= 0)
        return 0;

    /* Adjust block size to include overhead and alignment requirements. */
    size_t asize = ALIGN(size + WSIZE); /* adjusted block size */

    /* Search the free list for a fit */
    if ((bp = find_fit(asize)) == heap_start_addr) {
        /* No fit found. Get more memory and place the block */
        size_t extandsize = MAX(asize, CHUNKSIZE);
        extend_heap(extandsize);
//        bp = find_fit(asize);//diff
    }
    place(bp, asize);
    return bp;
}

void free(void *ptr) {
    if (bp == 0) return;
    int size = GET_SIZE(HDPR(ptr));
    int sign = is_prev_free(bp);
    PUT(HDPR(ptr), PACK(size, sign));
    PUT(FTRP(ptr), PACK(size, sign));
    insert(coalesce(ptr));
}

void *realloc(void *ptr, size_t size) {
    void *newptr, *temp;
    /* If old ptr is NULL, then this is just malloc. */
    if (ptr == 0)
        return mm_malloc(size);

    /* If size == 0 then this is just free, and we return NULL. */
    if (size == 0) {
        mm_free(ptr);
        return 0;
    }

    /* Adjust block size to include overhead and alignment requirements. */
    size_t newsize = ALIGN(size + WSIZE);
    size_t oldsize = GET_SIZE(HDPR(ptr));
    if (oldsize == newsize)
        return ptr;
    if (newsize < oldsize) {
        temp = GET_NEXT(ptr);//=GET_NEXT
        size_t next_alloc = GET_ALLOC(HDPR(temp));
        size_t compound_size = old_size + GET_SIZE(HDPR(temp));
        int new_free_size = !next_alloc ? (compound_size - newsize) : (oldsize - newsize);
        if (new_free_size >= DSIZE) {
            if (!next_alloc)
                delete_node(next);
            PUT(HDPR(ptr), PACK(newsize, 1 | is_prev_free(ptr)));
            void *new_next = GET_NEXT(ptr);
            PUT(HDRP(new_next), PACK(new_free_size, 2));
            PUT(FTRP(new_next), PACK(new_free_size, 2));
            insert_node(coalesce(new_next));
            return ptr;
        }
    } else {
        temp = NEXT_BLKP(oldptr);
        if ((!GET_ALLOC(HDPR(temp))) && (oldsize + GET_SIZE(HDPR(temp)) >= newsize)) {
            // next block is free and space is enough
            delete_node(temp);
            PUT(header_ptr, PACK(oldsize + GET_SIZE(HDPR(temp)), 1 | is_prev_free(ptr)));
            SET_PREV_ALLOC(next);
            return ptr;
        }
    }

    newptr = malloc(size);
    // If realloc() fails the original block is left untouched
    if (!newptr) {
        return 0;
    }
    // Copy the old data.
    if (size < oldsize) oldsize = size;
    memcpy(newptr, ptr, oldsize);
    mm_free(oldptr);// Free the old block.
    return newptr;
}

void *calloc(size_t nmemb, size_t size) {
    size_t bytes = nmemb * size;
    void *newptr = malloc(bytes);
    memset(newptr, 0, bytes);
    return newptr;
}

static void *extend_heap(size_t asize) {
    char *bp;
    bp = mem_sbrk(asize);

    /* Initialize free block header/footer and the epilogue header */
    int sign = 0 | is_prev_free(bp);
    PUT(HDRP(bp), PACK(size, sign));     /* Free block header */
    PUT(FTRP(bp), PACK(size, sign));     /* Free block footer */
    PUT(HDRP(GET_NEXT(bp)), PACK(0, 1)); /* New epilogue header */

    /* Coalesce if the previous block was free */
    if (!is_end_alloc)
        bp = coalesce(bp);
    insert_node(bp);
    return bp;
}

static void *coalesce(void *bp) {
    void *header_ptr = HDRP(bp);
    void *next = GET_NEXT(bp);
    void *next_header_ptr = HDRP(next);

    size_t size = GET_SIZE(header_ptr);
    size_t prev_alloc = PREV_ALLOC(header_ptr);
    size_t next_alloc = GET_ALLOC(next_header_ptr);

    if (prev_alloc && next_alloc) {             /* Case 1 */
        return bp;
    } else if (prev_alloc) {                    /* Case 2 */
        size += GET_SIZE(next_header_ptr);
        delete_node(GET_NEXT(bp));
        int sign = 0 | is_prev_free(bp);
        PUT(header_ptr, PACK(size, sign));
        PUT(FTRP(bp), PACK(size, sign));
        return bp;
    } else if (next_alloc) {                    /* Case 3 */
        void *prev = GET_PREV(bp);
        int sign = 0 | is_prev_free(prev);
        delete_node(prev);
        size += GET_SIZE(HDRP(prev));
        PUT(HDRP(prev), PACK(size, sign));
        PUT(FTRP(prev), PACK(size, sign));
        return prev;
    } else {                                    /* Case 4 */
        void *prev = GET_PREV(bp);
        void *prev_header_ptr = HDRP(prev);
        size += GET_SIZE(prev_header_ptr) + GET_SIZE(next_header_ptr);
        delete_node(prev);
        delete_node(next);
        int sign = 0 | is_prev_free(bp);
        PUT(prev_header_ptr, PACK(size, sign));
        PUT(FTRP(prev), PACK(size, sign));
        return prev;
    }
}

static void place(void *bp, size_t a_size) {
    unsigned int block_size = GET_SIZE(HDRP(bp));
    size_t remaining_size = block_size - a_size;
    int pre_free = 1 | is_prev_free(bp);
    delete_node(bp);
    if ((remaining_size) >= DSIZE) {
        PUT(HDRP(bp), PACK(a_size, pre_free));
        bp = GET_NEXT(bp);
        PUT(HDRP(bp), PACK(remaining_size, 2));
        PUT(FTRP(bp), PACK(remaining_size, 2));
        insert_node(coalesce(bp));
    } else
        PUT(HDRP(bp), PACK(block_size, pre_free));
}

static void *find_fit(size_t asize) {
    if (asize == 8 && list_8 != heap_start_addr) return list_8;
    if (asize <= 16 && list_16 != heap_start_addr) return list_16;
    /* the most fit block */
    void *fit = heap_start_addr;
    /* The currently searching node pointer */
    void *searching = root;
    /* Search from the root of the tree to the bottom */
    while (searching != heap_start_addr)
        /*Record the currently */
        if (asize <= GET_SIZE(HDRP(searching))) {
            fit = searching;
            searching = GET_PTR(searching);
        } else
            searching = GET_PTR(RIGHT(searching));
    return fit;
}

static void insert_node(void *bp) {
    RESET_PREV_ALLOC(GET_NEXT(bp));
    void *header_ptr = HDRP(bp);
    size_t block_size = GET_SIZE(header_ptr);
    if (block_size == 8) {
        SET_PREV_FREE(GET_NEXT(bp));
        PUT(bp, PTR2INT(list_8));
        list_8 = bp;
        return;
    }
    if (block_size == 16) {
        //if the block size = 16; insert it to the head of the 16-byte list
        PUT(bp, 0);
        PUT(RIGHT(bp), PTR2INT(list_16));
        PUT(list_16, PTR2INT(bp));
        list_16 = bp;
        return;
    }
    void *parent = heap_start_addr;
    void *current_node = root;
    int direction = -1;
    /* loop to locate the position */
    while (1) {
        if (current_node == heap_start_addr) {
            PUT(bp, 0);
            PUT(RIGHT(bp), 0);
            PUT(PARENT(bp), PTR2INT(parent));
            PUT(SIBLING(bp), 0);
            break;
        }
        void *curr_header_ptr = HDRP(current_node);
        /* Case 1: size of the block exactly matches the node. */
        if (GET_SIZE(header_ptr) == GET_SIZE(curr_header_ptr)) {
            PUT(bp, GET(current_node));
            PUT(RIGHT(bp), GET(RIGHT(current_node)));
            PUT(PARENT(GET_PTR(current_node)), PTR2INT(bp));
            PUT(PARENT(GET_PTR(RIGHT(current_node))), PTR2INT(bp));
            PUT(PARENT(bp), PTR2INT(parent));
            PUT(SIBLING(bp), PTR2INT(current_node));
            PUT(current_node, PTR2INT(bp));
            break;
        }
            /* Case 2: size of the block is less than that of the node. */
        else if (GET_SIZE(header_ptr) < GET_SIZE(curr_header_ptr)) {
            parent = current_node;
            direction = 0;
            current_node = GET_PTR(current_node);
        }
            /* Case 3 size of the block is greater than that of the node. */
        else {
            parent = current_node;
            direction = 1;
            current_node = GET_PTR(RIGHT(current_node));
        }
    }
    if (direction == -1) root = bp;
    else if (direction == 0) PUT(parent, PTR2INT(bp));
    else
        PUT(RIGHT(parent), PTR2INT(bp));
}

static void delete_node(void *bp) {
    SET_PREV_ALLOC(GET_NEXT(bp));
    int block_size = GET_SIZE(HDRP(bp));
    if (block_size == 8) {
        RESET_PREV_FREE(GET_NEXT(bp));
        void *current_block = list_8;
        if (current_block == bp) {
            list_8 = GET_PTR(bp);
            return;
        }
        while (current_block != heap_start_addr) {
            if (GET_PTR(current_block) == bp) break;
            current_block = GET_PTR(current_block);
        }
        PUT(current_block, PTR2INT(GET_PTR(bp)));
        return;
    }
    if (block_size == 16) {
        void *left_block = GET_PTR(bp);
        void *right_block = GET_PTR(RIGHT(bp));
        if (bp == list_16) list_16 = right_block;
        PUT(RIGHT(left_block), PTR2INT(right_block));
        PUT(right_block, PTR2INT(left_block));
        return;
    }
    /* If the removing node has siblings, the handling is simple */
    if (GET(bp) != 0 && GET_PTR(SIBLING(GET_PTR(bp))) == bp) {
        PUT(SIBLING(GET_PTR(bp)), PTR2INT(GET_PTR(SIBLING(bp))));
        PUT(GET_PTR(SIBLING(bp)), PTR2INT(GET_PTR(bp)));
        return;
    }
        /* If the removing node is the sole node with its size in the
         * binary search tree, the handling is more complex.  * */
    else if (GET(SIBLING(bp)) == 0) {
        if (GET(RIGHT(bp)) != 0) {
            void *successor = GET_PTR(RIGHT(bp));
            while (GET(successor) != 0)
                successor = GET_PTR(successor);
            void *origin_l = GET_PTR(bp);
            void *origin_r = GET_PTR(RIGHT(bp));
            void *successor_r = GET_PTR(RIGHT(successor));
            void *successor_p = GET_PTR(PARENT(successor));
            if (bp != root) {
                void *bpP = GET_PTR(PARENT(bp));
                if (GET_PTR(bpP) == bp)
                    PUT(bpP, PTR2INT(successor));
                else
                    PUT(RIGHT(bpP), PTR2INT(successor));
                PUT(PARENT(successor), PTR2INT(bpP));
            } else {
                root = successor;
                PUT(PARENT(successor), 0);
            }
            PUT(successor, PTR2INT(origin_l));
            PUT(PARENT(origin_l), PTR2INT(successor));
            if (successor != origin_r) {
                PUT(RIGHT(successor), PTR2INT(origin_r));
                PUT(PARENT(origin_r), PTR2INT(successor));
                PUT(successor_p, PTR2INT(successor_r));
                PUT(PARENT(successor_r), PTR2INT(successor_p));
            }
            return;
        }
        if (bp == root) root = GET_PTR(bp);
        if (GET_PTR(GET_PTR(PARENT(bp))) == bp)
            PUT(GET_PTR(PARENT(bp)), PTR2INT(GET_PTR(bp)));
        else
            PUT(RIGHT(GET_PTR(PARENT(bp))), PTR2INT(GET_PTR(bp)));
        PUT(PARENT(GET_PTR(bp)), PTR2INT(GET_PTR(PARENT(bp))));
        return;
    }
    /* Case that the block is first one in the node. */
    void *sibling = GET_PTR(SIBLING(bp));
    if (bp == root) {/* the node is the root */
        root = sibling;
        PUT(PARENT(sibling), 0);
    } else {/* the node is not the root */
        if (GET_PTR(GET_PTR(PARENT(bp))) == bp)
            PUT(GET_PTR(PARENT(bp)), PTR2INT(sibling));
        else
            PUT(RIGHT(GET_PTR(PARENT(bp))), PTR2INT(sibling));
        PUT(PARENT(sibling), PTR2INT(GET_PTR(PARENT(bp))));
    }
    PUT(sibling, GET(bp));
    PUT(RIGHT(sibling), GET(RIGHT(bp)));
    PUT(PARENT(GET_PTR(bp)), PTR2INT(sibling));
    PUT(PARENT(GET_PTR(RIGHT(bp))), PTR2INT(sibling));
}

void mm_checkheap(int ver) {}

