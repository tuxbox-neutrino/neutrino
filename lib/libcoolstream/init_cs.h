#ifndef __INIT_CS_H
#define __INIT_CS_H
void init_cs_api();
void shutdown_cs_api();
void * cs_malloc_uncached(size_t size);
void cs_free_uncached(void *ptr);
void * cs_phys_addr(void *ptr);
#endif
