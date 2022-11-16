#include <stdio.h>
#include <stddef.h>
#include <assert.h>
#include <unistd.h>
#include "gc.h"

typedef struct header {
  unsigned int  size;
  struct header *next;
}header_t;

static header_t base;           //zero size block to get us started
static header_t *freep = &base; //points to first free block of memory
static header_t *usedp;          //points to first used block of memory

/*
 * Scan the free list and look for a place to put the block. Basically, we're
 * looking for any the the to-be-freed block might have been partitioned from.
 */
static void add_to_free_list(header_t *bp)
{
  header_t *p;

  for(p=freep; !(bp > p && bp < p->next); p = p->next)
    if(p >= p->next && (bp > p || bp < p->next))
      break;
  
  // insert free block
  if(bp + bp->size == p->next){
    bp->size += p->next->size;
    bp->next = p->next->next;
  }else
    bp->next = p->next;

  if(p+p->size == bp){
    p->size += bp->size;
    p->next = bp->next;
  }else
    p->next = bp;

  freep = p;
}

#define MIN_ALLOC_SIZE 4096 //we allocate blocks in page sized chunks

/*
 * request more memory from the kernel
 */
static header_t *morecore(size_t num_units)
{
  void *vp;
  header_t *up;

  if(num_units > MIN_ALLOC_SIZE)
    num_units = MIN_ALLOC_SIZE / sizeof(header_t);

  if((vp = sbrk(num_units*sizeof(header_t))) == (void*)-1)
    return NULL;

  up = (header_t*)vp;
  up->size = num_units;
  add_to_free_list(up);
  return freep;
}

/*
 * find a chunk from the free list and put it in the used list
 */
void *GC_malloc(size_t alloc_size)
{
  size_t num_units;
  header_t *p, *prevp;

  num_units = (alloc_size+sizeof(header_t)-1)/sizeof(header_t)+1;
  prevp = freep;

  for(p = prevp->next; ; prevp = p, p = p->next){
    if(p->size >= num_units){ //big enough
      if(p->size == num_units) //exact size
        prevp->next = p->next;
      else{
        p->size -= num_units;
        p += p->size;
        p->size = num_units;
      }

      freep = prevp;

      //add p to the used list
      if(usedp == NULL)
        usedp = p->next = p;
      else{
        p->next = usedp->next;
	usedp->next = p;
      }

      return (void *)(p+1);
    }
    if(p == freep){ //not enough memory
      p = morecore(num_units);
      if(p == NULL) //request for more memory failed
        return NULL;
    }
  }
}

#define UNTAG(p) (((unsigned long)(p)) & 0xfffffffffffffffc)
/*
 * Scan a region of memory and mark any items in the used list appropriately
 * Both arguments should be word algined.
 */
static void scan_region(unsigned long *sp, unsigned long *end)
{
  header_t *bp;

  for(; sp < end; sp++){
    unsigned long v = *sp;
    bp = usedp;
    do{
      if(v >= bp+1 && v < bp+1+bp->size){ 
        bp->next = ((unsigned long) bp->next) | 1;
	break;
      }
    }while((bp = UNTAG(bp->next)) != usedp);
  }
}

/*
 * Sacn the marked blocks for references to other unmarked blocks.
 */
static void scan_heap(void)
{
  unsigned long *vp;
  header_t *bp, *up;

  for(bp = UNTAG(usedp->next); bp != usedp; bp = UNTAG(bp->next)){
    if(!((unsigned long)bp->next & 1))
      continue;
    for(vp = (unsigned long *)(bp+1); vp < (bp+bp->size+1); vp++){
      unsigned long v = *vp;
      up = UNTAG(bp->next);
      do{
        if(up != bp && v >= up+1 && v < up+1+up->size){
	  up->next = ((unsigned long) up->next) | 1;
	  break;
	}
      }while((up = UNTAG(up->next)) != bp);
    }
  }
}

static unsigned long stack_bottom;
/*
 * Find the absolute bottom of the stack and set stuff up.
 */
void GC_init(void)
{
  static int initted;
  FILE *statfp;

  if(initted)
    return;

  initted = 1;

  statfp = fopen("/proc/self/stat", "r");
  assert(statfp != NULL);
  fscanf(statfp,
	 "%*d %*s %*c %*d %*d %*d %*d %*d %*u "
	 "%*lu %*lu %*lu %*lu %*lu %*lu %*ld %*ld "
	 "%*ld %*ld %*ld %*ld %*llu %*lu %*ld "
	 "%*lu %*lu %*lu %lu", &stack_bottom);
  fclose(statfp);

  usedp = NULL;
  base.next = freep = &base;
  base.size = 0;
}

/*
 * Mark blocks of memory in use and free the ones not in use.
 */
void GC_collect(void)
{
  header_t *p, *prevp, *tp;
  unsigned long stack_top;
  extern char __data_start, _end; //provided by the linker.

  if(usedp == NULL)
    return;

  //scan the BSS and initialized data segments
  scan_region(&__data_start, &_end);

  //scan the stack
  asm volatile("mov %0, sp" : "=r" (stack_top));
  scan_region(stack_top, stack_bottom);

  //mark from the heap
  scan_heap();

  //collect memory garbage
  for(prevp = usedp, p = UNTAG(usedp->next); ; prevp = p, p = UNTAG(p->next)){
    next_chunk:
      if(!((unsigned long)p->next & 1)){
        //the chunk hasn't been marked. Thus, it must be free
	tp = p;
	p = UNTAG(p->next);
	add_to_free_list(tp);

	if(usedp == tp){
	  usedp = NULL;
	  break;
	}

	prevp->next = (unsigned long)p | ((unsigned long)prevp->next & 1);
	goto next_chunk;
      }
      p->next = ((unsigned long)p->next) & ~1;
      if(p == usedp)
        break;
  }
}

/*
 * Print usedp
 */
void GC_print_usedp(void)
{
  header_t *cp;

  if(usedp == NULL){
    printf("\nusedp now is empty!\n");
    return; 
  }
  
  printf("\n========== usedp detail ==========\n");
  cp = usedp;
  goto loop;
  while(cp != usedp){
    loop:
      printf("address: 0x%x, size: %d\n", cp, cp->size);
      cp = cp->next;
  }
}

/*
 * Print freep
 */
void GC_print_freep(void)
{
  header_t *cp;

  if(freep->next == &base){
    printf("\nfreep now is empty!\n");
    return;
  }

  printf("\n========== freep detail ==========\n");
  cp = freep->next;
  while(cp != &base){
    printf("address: 0x%x, size: %d\n", cp, cp->size);
    cp = cp->next;
  }
}
