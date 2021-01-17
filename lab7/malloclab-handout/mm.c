/*
 * mm.c
 *todo
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
//todo
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

//todo list：隐式链表--16bytes（header+footer+8bytes）；
// 显式链表--24bytes（header+prev+next+footer+8bytes=24bytes）；
// 分隔链表--24bytes（header+prev+next+footer+8bytes=24bytes）
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
    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *) -1)
        return -1;
    PUT(heap_listp, 0);                          /* Alignment padding */
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1)); /* Prologue header */
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1)); /* Prologue footer */
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1));     /* Epilogue header */
    heap_listp += (2 * WSIZE);

    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    if (extend_heap(CHUNKSIZE / WSIZE) == NULL)
        return -1;
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
    return NULL;
}

/*
 * free
 */
//todo 合并
/* todo
    --通过ptr指针释放已分配的块
    --只有是之前malloc、calloc、realloc返回的ptr并且没有被释放时才会执行
 */
void free(void *ptr) {
    if (!ptr) return;
}

/*
 * realloc - you may want to look at mm-naive.c
 */
/* todo
    --返回一个指向至少为size bytes的已分配空间的指针
    --ptr == NULL ==> malloc(size)
    --size == 0 ==> free(ptr),return NULL
    --ptr != NULL，那么只有是之前malloc、calloc、realloc返回的ptr并且没有被释放时才会执行
    --重新分配size bytes大小的块，并返回地址，前min{原size，size}的内容不变
    --原来的块要被释放
 */
void *realloc(void *oldptr, size_t size) {
    return NULL;
}

/*
 * calloc - you may want to look at mm-naive.c
 * This function is not tested by mdriver, but it is
 * needed to run the traces.
 */
/* todo
    --申请一个数组有nmemb个元素，每个元素大小为size bytes，返回指向该块内存的指针，初始化为0
    --这个不会用throughput和performance评分，实现正确就行
 */
void *calloc(size_t nmemb, size_t size) {
    return NULL;
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
/* todo
    --heap consistency checker 或 simply heap checker，浏览堆检查正确性（如header和footer是否正确）
    --必须默默的跑，直到在堆中检测到错误，打印信息，并exit（）结束
    --会打印很多在large trace中有用的信息
    --一个有质量的checker是重要的，许多malloc的bug太微小以至于不能用gdb来debug。
      最有用的就是用checker，当找到一个bug，可以分离出来，重复调用checker直到找到出错的指令
    --很重要，会被评分，问助教的话，第一件事就是看这个函数
    --传递行号作为参数，出错时打印行号
 */
void mm_checkheap(int lineno) {
}
