#define MINIMUM_BRK_SIZE 0
extern void    set_heap_poiner(unsigned long local_heap_begin, unsigned int size);
extern void    extend_heap_size(unsigned int size);
extern size_t  dl_malloc(size_t bytes);
extern void    dl_free(size_t offset);

