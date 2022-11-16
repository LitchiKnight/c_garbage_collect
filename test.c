#include <string.h>
#include "gc.h"

#define MIN_STRING_LEN 32

struct garbage_s{
  unsigned int size;
  char* name;
  int * array;
};

// global
int* g_garbage_int_p = NULL;
struct garbage_s g_garbage_s;

void recursion_gen_garbage(int times);

int main(void)
{
  struct garbage_s *l_garbage_s_p;

  GC_init();
  g_garbage_int_p = (int*)GC_malloc(sizeof(int));
  printf("g_garbage_int_p address is: 0x%x\n", g_garbage_int_p);
  *g_garbage_int_p = 1024;

  // global garbage init
  printf("g_garbage_s address is: 0x%x\n", &g_garbage_s);
  g_garbage_s.size  = 10;
  g_garbage_s.name  = (char*)GC_malloc(MIN_STRING_LEN*sizeof(char));
  printf("g_garbage_s.name address is: 0x%x\n", g_garbage_s.name);
  g_garbage_s.array = (int*)GC_malloc((g_garbage_s.size)*sizeof(int));
  printf("g_garbage_s.array address is: 0x%x\n", g_garbage_s.array);

  strcpy(g_garbage_s.name, "global_garbage");
  for(int i=0; i<(g_garbage_s.size);i++)
    (g_garbage_s.array)[i] = 1014+i;

  // local garbage init
  l_garbage_s_p   = (struct grabage_s*)GC_malloc(sizeof(struct garbage_s));
  printf("l_garbage_s_p address is: 0x%x\n", l_garbage_s_p);
  l_garbage_s_p->size  = 20;
  l_garbage_s_p->name  = (char*)GC_malloc(MIN_STRING_LEN*sizeof(char));
  printf("l_garbage_s_p->name address is: 0x%x\n", l_garbage_s_p->name);
  l_garbage_s_p->array = (int*)GC_malloc((l_garbage_s_p->size)*sizeof(int));
  printf("l_garbage_s_p->array address is: 0x%x\n", l_garbage_s_p->array);
  
  strcpy(l_garbage_s_p->name, "main_garbage");
  for(int i=0; i<(l_garbage_s_p->size); i++)
    (l_garbage_s_p->array)[i] = 1024+i;
  
  recursion_gen_garbage(5);
  GC_collect();
  GC_print_usedp();
  GC_print_freep();
}

void recursion_gen_garbage(int times)
{
  struct garbage_s* f_garbage_s_p;

  if(times == 0)
    return;
  
  printf("\n========== recursion %d ==========\n", times);
  // function inside garbage malloc
  f_garbage_s_p = (struct garbage_s*)GC_malloc(sizeof(struct garbage_s));
  printf("f_garbage_s_p address is: 0x%x\n", f_garbage_s_p);
  f_garbage_s_p->size = 30;
  f_garbage_s_p->name = (char*)GC_malloc(MIN_STRING_LEN*sizeof(char));
  printf("f_garbage_s_p->name address is: 0x%x\n", f_garbage_s_p->name);
  f_garbage_s_p->array = (int*)GC_malloc((f_garbage_s_p->size)*sizeof(int));
  printf("f_garbage_s_p->array address is: 0x%x\n", f_garbage_s_p->array);

  // function inside garbage init
  strcpy(f_garbage_s_p->name, "function_garbage");
  for(int i=0; i<(f_garbage_s_p->size); i++)
    (f_garbage_s_p->array)[i] = i;

  recursion_gen_garbage(times-1);
}
