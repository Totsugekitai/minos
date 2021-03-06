#include <stdio.h>
#include <stdint.h>
#include <string.h>

struct malloc_header {
    uint64_t block_size;
    struct malloc_header *next;
};

#define HEAP_SIZE 0x400000
static uint64_t heap[HEAP_SIZE];

// This global variable is base pointer of malloc.
// This has only chunk header, don't have data.
static struct malloc_header base =
{
    0,
    0
};

static struct malloc_header *next_search_position = 0;

void *minmalloc(uint64_t size)
{
    printf("start minmalloc\n");

    uint64_t number_of_blocks =
        ((size + sizeof(struct malloc_header) - 1) / sizeof(struct malloc_header)) + 1;

    printf("number_of_blocks: %lx\n", number_of_blocks);

    struct malloc_header *p, *prev_p = next_search_position;

    if (prev_p == 0) {
        printf("first malloc!\n");

        next_search_position = prev_p = &base;
        base.block_size = 0;
        base.next = (struct malloc_header *)heap;
        base.next->block_size =
            (HEAP_SIZE / sizeof(struct malloc_header));
        base.next->next = &base;

        printf("init base\n");
        printf("base: %p\n", &base);
        printf("base.block_size: %lx\n", base.block_size);
        printf("base.next: %p\n", base.next);
        printf("\n");

        p = prev_p->next;

        if (p->block_size >= number_of_blocks) {    // big enough
            if (p->block_size == number_of_blocks) {    // exactly
                prev_p->next = p->next;
            } else {    // allocate tail end
                next_search_position = p;
                p->block_size -= number_of_blocks;
                p += p->block_size;
                p->block_size = number_of_blocks;
            }
            printf("prev_p: %p\n", prev_p);
            printf("prev_p->block_size: %lx\n", prev_p->block_size);
            printf("prev_p->next: %p\n", prev_p->next);
            printf("p: %p\n", p);
            printf("p->block_size: %lx\n", p->block_size);
            printf("p->next: %p\n", p->next);
            printf("\n");
            return ((void *)(p + 1));
        }
    }
    // normal malloc
    else {
        printf("malloc\n");
        for (p = prev_p->next;; prev_p = p, p = p->next) {
            if (p->block_size >= number_of_blocks) {
                if (p->block_size == number_of_blocks) {
                    prev_p->next = p->next;
                } else {
                    next_search_position = prev_p;
                    p->block_size -= number_of_blocks;
                    p += p->block_size;
                    p->block_size = number_of_blocks;
                }
                printf("prev_p: %p\n", prev_p);
                printf("prev_p->block_size: %lx\n", prev_p->block_size);
                printf("prev_p->next: %p\n", prev_p->next);
                printf("p: %p\n", prev_p);
                printf("p->block_size: %lx\n", p->block_size);
                printf("p->next: %p\n", p->next);
                printf("\n");
                return ((void *)(p + 1));
            }
            if (p == next_search_position) {
                return 0;
            }
        }
    }
    return 0;
}

void minfree(void *ptr)
{
    struct malloc_header *p, *q;

    p = (struct malloc_header *)ptr - 1;
    for (q = next_search_position; !(p > q && p < q->next); q = q->next) {
        if (q >= q->next && (p > q || p < q->next)) {
            break;
        }
    }

    if (p + p->block_size == q->next) {
        p->block_size += q->next->block_size;
        p->next = q->next->next;
    } else {
        p->next = q->next;
    }

    if (q + q->block_size == p) {
        q->block_size += p->block_size;
        q->next = p->next;
    } else {
        q->next = p;
    }
    next_search_position = p;
}

void print_heap(struct malloc_header *base, struct malloc_header *malloc[], int nummalloc)
{
    struct malloc_header *p;

    printf("1 block = %lx\n", sizeof(struct malloc_header));

    printf("Empty heap\n");
    for (p = base;; p = p->next)
    {
        printf("address: %p, block: %05lx, next address: %p\n", p, p->block_size, p->next);
        if (p->next == base)
        {
            printf("\n");
            break;
        }
    }

    printf("Allocated heap\n");
    for (int i = 0; i < nummalloc; i++)
    {
        p = malloc[i] - 1;
        printf("address: %p, block: %05lx, next address: %p\n", p, p->block_size, p->next);
    }
    printf("\n");
}

char itoc(int i)
{
    return '0' + i;
}

int main(void)
{
    char *malloc[6];
    malloc[0] = minmalloc(0x10);
    malloc[1] = minmalloc(0x20);
    malloc[2] = minmalloc(0x30);
    malloc[3] = minmalloc(0x40);
    malloc[4] = minmalloc(0x50);
    malloc[5] = minmalloc(0x60);
    for (int i = 0; i < 6; i++) {
        malloc[i][0] = 'm';
        malloc[i][1] = 'a';
        malloc[i][2] = 'l';
        malloc[i][3] = 'l';
        malloc[i][4] = 'o';
        malloc[i][5] = 'c';
        malloc[i][6] = itoc(i);
        malloc[i][7] = '\0';
    }

    print_heap(&base, (struct malloc_header **)malloc, 6);

    for (int i = 0; i < 6; i++) {
        minfree(malloc[i]);
    }
    print_heap(&base, (struct malloc_header **)malloc, 6);

    return 0;
}
