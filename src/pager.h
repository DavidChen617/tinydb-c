#ifndef PAGER_H
#define PAGER_H

#include <stdint.h>

#define PAGE_SIZE 4096
#define MAX_PAGES 100

typedef struct
{
    int fd;
    uint32_t file_length;
    uint32_t num_pages;
    void *pages[MAX_PAGES];
} Pager;

Pager *pager_open(const char *filename);
void *get_page(Pager *pager, uint32_t pager_num);
void pager_flush(Pager *pager, uint32_t page_num, uint32_t size);
void pager_close(Pager *pager);

#endif
