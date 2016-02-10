 /*
 * mm.c implements malloc using linked lists of different sized blocks and
 * buffers for realloc.
 *
 * Each block has a header and a footer and after each reallocation there is a 
 * buffer for a following realloc assuming they increse by the same size.
 *
 * Header entries consist of the block size (all 32 bits), reallocation tag
 * (second-last bit), and allocation bit (last bit).
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include "mm.h"
#include "memlib.h"

team_t team = {
	/* Team name */
	"student",
	/* First member's full name */
	"Jiwon Shin",
	/* First member's NYU NetID*/
	"js6450@nyu.edu",
	/* Second member's full name (leave blank if none) */
	"",
	/* Second member's email address (leave blank if none) */
	""
};

#define HEADER(currblock) ((char *)(currblock) - WSIZE)
#define FOOTER(currblock) ((char *)(currblock) + GETSIZE(HEADER(currblock)) - DSIZE)
#define NEXT(currblock) ((char *)(currblock) + GETSIZE((char *)(currblock) - WSIZE))
#define PREV(currblock) ((char *)(currblock) - GETSIZE((char *)(currblock) - DSIZE))
#define PACK(size, alloc) ((size) | (alloc))
#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val) | RTAG(p))
#define CLEARTAG(p, val) (*(unsigned int *)(p) = (val))
#define SETPTR(p, currblock) (*(unsigned int *)(p) = (unsigned int)(currblock))
#define SETRTAG(p)   (*(unsigned int *)(p) = GET(p) | 0x2)
#define UNSETRTAG(p) (*(unsigned int *)(p) = GET(p) & ~0x2)
#define GETSIZE(p)  (GET(p) & ~0x7)
#define FULL(p) (GET(p) & 0x1)
#define RTAG(p)   (GET(p) & 0x2)
#define PREVPTR(currblock) ((char *)(currblock))
#define NEXTPTR(currblock) ((char *)(currblock) + WSIZE)
#define PREVFREE(currblock) (*(char **)(currblock))
#define NEXTFREE(currblock) (*(char **)(NEXTPTR(currblock)))
#define ALIGN(p) (((size_t)(p) + 7) & ~(0x7))
#define MMCHECK 0
#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1<<7)
#define LISTS 16

void *free_lists[LISTS];
char *prologue_block;
int line_count;

static void insertfree(void *currblock, size_t size) {
  int listcounter = 0;
  void *search_currblock = currblock;
  void *insert_currblock = NULL;

  for(;(listcounter < LISTS - 1) && (size > 1); listcounter++) {
    size >>= 1;
  }

  search_currblock = free_lists[listcounter];
  while ((search_currblock != NULL) && (size > GETSIZE(HEADER(search_currblock)))) {
    insert_currblock = search_currblock;
    search_currblock = PREVFREE(search_currblock);
  }
  
  if (search_currblock != NULL) {
    if (insert_currblock != NULL) {
      SETPTR(PREVPTR(currblock), search_currblock); 
      SETPTR(NEXTPTR(search_currblock), currblock);
      SETPTR(NEXTPTR(currblock), insert_currblock);
      SETPTR(PREVPTR(insert_currblock), currblock);
    } else {
      SETPTR(PREVPTR(currblock), search_currblock); 
      SETPTR(NEXTPTR(search_currblock), currblock);
      SETPTR(NEXTPTR(currblock), NULL);
      free_lists[listcounter] = currblock;
    }
  } else {
    if (insert_currblock != NULL) {
      SETPTR(PREVPTR(currblock), NULL);
      SETPTR(NEXTPTR(currblock), insert_currblock);
      SETPTR(PREVPTR(insert_currblock), currblock);
    } else {
      SETPTR(PREVPTR(currblock), NULL);
      SETPTR(NEXTPTR(currblock), NULL);
      
      free_lists[listcounter] = currblock;
    }
  }

  return;
}

static void deletefree(void *currblock) {
  int listcounter = 0;
  size_t size = GETSIZE(HEADER(currblock));
  
  for (;(listcounter < LISTS - 1) && (size > 1); listcounter++) {
    size >>= 1;
  }
  
  if (PREVFREE(currblock) != NULL) {
    if (NEXTFREE(currblock) != NULL) {
      SETPTR(NEXTPTR(PREVFREE(currblock)), NEXTFREE(currblock));
      SETPTR(PREVPTR(NEXTFREE(currblock)), PREVFREE(currblock));
    } else {
      SETPTR(NEXTPTR(PREVFREE(currblock)), NULL);
      free_lists[listcounter] = PREVFREE(currblock);
    }
  } else {
    if (NEXTFREE(currblock) != NULL) {
      SETPTR(PREVPTR(NEXTFREE(currblock)), NULL);
    } else {
      free_lists[listcounter] = NULL;
    }
  }
  
  return;
}

static void *coalesce(void *currblock)
{
  size_t prev_alloc = FULL(HEADER(PREV(currblock)));
  size_t next_alloc = FULL(HEADER(NEXT(currblock)));
  size_t size = GETSIZE(HEADER(currblock));
  
  if (prev_alloc && next_alloc) {
    return currblock;
  }
  
  if (RTAG(HEADER(PREV(currblock))))
    prev_alloc = 1;
  
  deletefree(currblock);
  
  if (prev_alloc && !next_alloc) {
    deletefree(NEXT(currblock));
    size += GETSIZE(HEADER(NEXT(currblock)));
    PUT(HEADER(currblock), PACK(size, 0));
    PUT(FOOTER(currblock), PACK(size, 0));
  } else if (!prev_alloc && next_alloc) {
    deletefree(PREV(currblock));
    size += GETSIZE(HEADER(PREV(currblock)));
    PUT(FOOTER(currblock), PACK(size, 0));
    PUT(HEADER(PREV(currblock)), PACK(size, 0));
    currblock = PREV(currblock);
  } else {
    deletefree(PREV(currblock));
    deletefree(NEXT(currblock));
    size += GETSIZE(HEADER(PREV(currblock))) + GETSIZE(HEADER(NEXT(currblock)));
    PUT(HEADER(PREV(currblock)), PACK(size, 0));
    PUT(FOOTER(NEXT(currblock)), PACK(size, 0));
    currblock = PREV(currblock);
  }
  
  insertfree(currblock, size);
  
  return coalesce(currblock);
}

static void placeblock(void *currblock, size_t asize)
{
  size_t currblock_size = GETSIZE(HEADER(currblock));
  size_t remainder = currblock_size - asize;
  
  deletefree(currblock);
  
  if (remainder >= 16) {
    PUT(HEADER(currblock), PACK(asize, 1));
    PUT(FOOTER(currblock), PACK(asize, 1));
    CLEARTAG(HEADER(NEXT(currblock)), PACK(remainder, 0));
    CLEARTAG(FOOTER(NEXT(currblock)), PACK(remainder, 0));  
    insertfree(NEXT(currblock), remainder);
  } else {
    PUT(HEADER(currblock), PACK(currblock_size, 1));
    PUT(FOOTER(currblock), PACK(currblock_size, 1));
  }
  return;
}

static void *extend_heap(size_t size)
{
  void *currblock;          
  size_t words = size / WSIZE;
  size_t asize;
  
  asize = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;

  if ((long)(currblock = mem_sbrk(asize)) == -1)
    return NULL;
  
  CLEARTAG(HEADER(currblock), PACK(asize, 0)); 
  CLEARTAG(FOOTER(currblock), PACK(asize, 0));
  CLEARTAG(HEADER(NEXT(currblock)), PACK(0, 1));
  
  insertfree(currblock, asize);

  return coalesce(currblock);
}

static void mm_check(char caller, void *currblock, int size);

int mm_init(void)
{
  int listcounter; 
  char *heap_start;
  
  for (listcounter = 0; listcounter < LISTS; listcounter++) {
    free_lists[listcounter] = NULL;
  }

  heap_start = mem_sbrk(4 * WSIZE);
  
  CLEARTAG(heap_start, 0);
  CLEARTAG(heap_start + (1 * WSIZE), PACK(DSIZE, 1));
  CLEARTAG(heap_start + (2 * WSIZE), PACK(DSIZE, 1));
  CLEARTAG(heap_start + (3 * WSIZE), PACK(0, 1)); 
  prologue_block = heap_start + DSIZE;
  
  extend_heap(CHUNKSIZE);  
  
  return 0;
}

void *mm_malloc(size_t size)
{
  size_t asize; 
  void *currblock = NULL;  
  int listcounter = 0; 
  
  size_t checksize = size; // Copy of request size
  
  if (size == 0)
    return NULL;
  
  if (size <= DSIZE) {
    asize = 2 * DSIZE;
  } else {
    asize = DSIZE * ((size + DSIZE + DSIZE - 1) / DSIZE);
  }
  
  size = asize;
  for (;listcounter < LISTS; listcounter++) {
    if ((listcounter == LISTS - 1) || ((asize <= 1) && (free_lists[listcounter] != NULL))) {
      currblock = free_lists[listcounter];
      while ((currblock != NULL) && ((size > GETSIZE(HEADER(currblock))) || (RTAG(HEADER(currblock))))) {
        currblock = PREVFREE(currblock);
      }
      if (currblock != NULL)
        break;
    }   
    asize >>= 1;
  }
  
  if (currblock == NULL) {
    currblock = extend_heap(size);
  }
  
  placeblock(currblock, size);
  
  line_count++;
  if(MMCHECK)
    mm_check('a', currblock, checksize);
  
  return currblock;
}

void mm_free(void *currblock)
{
  size_t size = GETSIZE(HEADER(currblock)); // Size of block 
  
  UNSETRTAG(HEADER(NEXT(currblock)));
  
  PUT(HEADER(currblock), PACK(size, 0));
  PUT(FOOTER(currblock), PACK(size, 0));
  
  insertfree(currblock, size);
  
  coalesce(currblock);
  
  line_count++;
  if (MMCHECK) {
    mm_check('f', currblock, size);
  }

  return;
}

void *mm_realloc(void *currblock, size_t size)
{
  void *new_currblock = currblock; 
  size_t new_size = size; 
  int remainder;  
  int extendsize; 
  int block_buffer; 

  if (size == 0)
    return NULL;
  
  if (new_size <= DSIZE) {
    new_size = 2 * DSIZE;
  } else {
    new_size = DSIZE * ((new_size + (DSIZE) + (DSIZE - 1)) / DSIZE);
  }
  
  new_size += 16;
  
  block_buffer = GETSIZE(HEADER(currblock)) - new_size;
  
  if (block_buffer < 0) {
    if (!FULL(HEADER(NEXT(currblock))) || !GETSIZE(HEADER(NEXT(currblock)))) {
      remainder = GETSIZE(HEADER(currblock)) + GETSIZE(HEADER(NEXT(currblock))) - new_size;
      if (remainder < 0) {
        extendsize = ((-remainder)>CHUNKSIZE?(-remainder):CHUNKSIZE);
        if (extend_heap(extendsize) == NULL)
          return NULL;
        remainder += extendsize;
      }
      
      deletefree(NEXT(currblock));
      
      CLEARTAG(HEADER(currblock), PACK(new_size + remainder, 1)); // Block header 
      CLEARTAG(FOOTER(currblock), PACK(new_size + remainder, 1)); // Block footer 
    } else {
      new_currblock = mm_malloc(new_size - DSIZE);
      line_count--;
      memmove(new_currblock, currblock,((size)<new_size?(size):new_size));
      mm_free(currblock);
      line_count--;
    }
    block_buffer = GETSIZE(HEADER(new_currblock)) - new_size;
  }  

  if (block_buffer < 2 * 16)
    SETRTAG(HEADER(NEXT(new_currblock)));
  
  line_count++;
   if (MMCHECK) {
    mm_check('r', currblock, size);
     }
  
  return new_currblock;
}

void mm_check(char caller, void* caller_currblock, int caller_size)
{
  int size;
  int alloc;
  char *currblock = prologue_block + DSIZE;
  int block_count = 1;
  int count_size;
  int count_list;
  int loc; 
  int caller_loc = (char *)caller_currblock - currblock;
  int listcounter;
  char *scan_currblock;
  
    printf("\n[%d] %c %d %d: Checking heap...\n",
      line_count, caller, caller_size, caller_loc);
  
  while (1) {
    loc = currblock - prologue_block - DSIZE;
    
    size = GETSIZE(HEADER(currblock));
    if (size == 0)
      break;
    
    alloc = FULL(HEADER(currblock));

    printf("%d: Block at location %d has size %d and allocation %d\n",
    block_count, loc, size, alloc);
    if (RTAG(HEADER(currblock))) {
	printf("%d: Block at location %d is tagged\n",
        block_count, loc);
    }
    
    if (size != GETSIZE(FOOTER(currblock))) {
        printf("%d: Header size of %d does not match footer size of %d\n",
        block_count, size, GETSIZE(FOOTER(currblock)));
    }
    if (alloc != FULL(FOOTER(currblock))) {
      printf("%d: Header allocation of %d does not match footer allocation "
        "of %d\n", block_count, alloc, FULL(FOOTER(currblock)));
    }
    
    if (!alloc) {
      listcounter = 0;
      count_size = size;
      while ((listcounter < LISTS - 1) && (count_size > 1)) {
        count_size >>= 1;
        listcounter++;

      }
      
      scan_currblock = free_lists[listcounter];
      while ((scan_currblock != NULL) && (scan_currblock != currblock)) {
        scan_currblock = PREVFREE(scan_currblock);
      }
      if (scan_currblock == NULL) {
        printf("%d: Free block of size %d is not in list index %d\n",
          block_count, size, listcounter);
      }
    }
    
    currblock = NEXT(currblock);
    block_count++;
  }
  
    printf("[%d] %c %d %d: Checking lists...\n",
      line_count, caller, caller_size, caller_loc);
  
  for (listcounter = 0; listcounter < LISTS; listcounter++) {
    currblock = free_lists[listcounter];
    block_count = 1;
    
    while (currblock != NULL) {
      loc = currblock - prologue_block - DSIZE;
      size = GETSIZE(HEADER(currblock));
      
      printf("%d %d: Free block at location %d has size %d\n",
      listcounter, block_count, loc, size);
      if (RTAG(HEADER(currblock))) {
	printf("%d %d: Block at location %d is tagged\n",
        listcounter, block_count, loc);
      }

      count_list = 0;
      count_size = size;
      
      while ((count_list < LISTS - 1) && (count_size > 1)) {
        count_size >>= 1;
        count_list++;
      }
      if (listcounter != count_list) {
        printf("%d: Free block of size %d is in list %d instead of %d\n",
        loc, size, listcounter, count_list);
      }
      
      if (FULL(HEADER(currblock)) != 0) {
        printf("%d: Free block has an invalid header allocation of %d\n",
        loc, FULL(FOOTER(currblock)));
      }
      if (FULL(FOOTER(currblock)) != 0) {
        printf("%d: Free block has an invalid footer allocation of %d\n",
        loc, FULL(FOOTER(currblock)));
      }
      
      currblock = PREVFREE(currblock);
      block_count++;
    }
  }
      printf("[%d] %c %d %d: Finished check\n\n",
      line_count, caller, caller_size, caller_loc);
  
    printf("Continue\n");
      
  return;
}