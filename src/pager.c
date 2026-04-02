#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include "pager.h"

Pager *pager_open(const char *filename)
{
    int fd = open(filename, O_RDWR | O_CREAT, 0600);
    if (fd < 0)
    {
        perror("Unable to open file");
        exit(EXIT_FAILURE);
    }

    off_t file_length = lseek(fd, 0, SEEK_END);
    Pager *pager = malloc(sizeof(Pager));
    pager->fd = fd;
    pager->file_length = (uint32_t)file_length;
    pager->num_pages = pager->file_length / PAGE_SIZE;

    for (int i = 0; i < MAX_PAGES; i++)
    {
        pager->pages[i] = NULL;
    }

    return pager;
}

void *get_page(Pager *pager, uint32_t page_num)
{
    if (page_num > MAX_PAGES)
    {
        printf("Page number out of bounds: %d\n", page_num);
        exit(EXIT_FAILURE);
    }

    if (pager->pages[page_num] == NULL)
    {
        void *page = malloc(PAGE_SIZE);
        memset(page, 0, PAGE_SIZE);

        uint32_t num_pages = pager->file_length / PAGE_SIZE;
        if(pager->file_length % PAGE_SIZE != 0){
            num_pages++;
        }

        if (page_num < num_pages)
        {
            lseek(pager->fd, page_num * PAGE_SIZE, SEEK_SET);
            read(pager->fd, page, PAGE_SIZE);
        }

        pager->pages[page_num] = page;
    }

    if(page_num >= pager->num_pages){
        pager->num_pages = page_num + 1;
    }

    return pager->pages[page_num];
}

void pager_flush(Pager *pager, uint32_t page_num, uint32_t size)
{
    if (pager->pages[page_num] == NULL)
        return;

    lseek(pager->fd, page_num * PAGE_SIZE, SEEK_SET);
    write(pager->fd, pager->pages[page_num], size);
}

void pager_close(Pager *pager)
{
    for (int i = 0; i < MAX_PAGES; i++)
    {
        if (pager->pages[i])
        {
            free(pager->pages[i]);
            pager->pages[i] = NULL;
        }
    }

    close(pager->fd);
    free(pager);
}
