#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stddef.h>

typedef struct meta_block *meta_ptr;

struct meta_block {
    int free;
    size_t size;
    meta_ptr next;
    meta_ptr prev;
    void *ptr;
    char data[1];
};

#define BLOCK_SIZE offsetof(struct meta_block, data)
void *base = NULL;

meta_ptr get_block_addr(void *p) {
    return (meta_ptr)((char*)p - BLOCK_SIZE);
}

// --- NEW MERGING LOGIC ---
meta_ptr merge_blocks(meta_ptr block) {
    if (block->next && block->next->free) {
        block->size += BLOCK_SIZE + block->next->size;
        block->next = block->next->next;
        if (block->next) {
            block->next->prev = block;
        }
    }
    return block;
}

meta_ptr find_suitable_block(meta_ptr *last, size_t size) {
    meta_ptr b = (meta_ptr)base;
    while (b && !(b->free && b->size >= size)) {
        *last = b;
        b = b->next;
    }
    return b;
}

void split_space(meta_ptr block, size_t size) {
    meta_ptr new_block;
    new_block = (meta_ptr)(block->data + size);
    new_block->size = block->size - size - BLOCK_SIZE;
    new_block->next = block->next;
    new_block->prev = block;
    new_block->free = 1;
    new_block->ptr = new_block->data;
    
    if (new_block->next) {
        new_block->next->prev = new_block;
    }
    block->next = new_block;
    block->size = size;
}

meta_ptr extend_heap(meta_ptr last, size_t size) {
    meta_ptr b = sbrk(0);
    if (sbrk(BLOCK_SIZE + size) == (void*)-1) return NULL;
    
    b->size = size;
    b->next = NULL;
    b->prev = last; // CRITICAL CHANGE: Connects the back-link
    b->ptr = b->data;
    b->free = 0;
    if (last) last->next = b;
    return b;
}

// --- PUBLIC API ---

void *my_malloc(size_t size) {
    meta_ptr b, last;
    if (base) {
        last = base;
        b = find_suitable_block(&last, size);
        if (b) {
            if ((b->size - size) >= (BLOCK_SIZE + 4)) split_space(b, size);
            b->free = 0;
        } else {
            b = extend_heap(last, size);
        }
    } else {
        b = extend_heap(NULL, size);
        base = b;
    }
    return (b ? b->data : NULL);
}

void my_free(void *ptr) {
    if (!ptr) return;
    meta_ptr b = get_block_addr(ptr);
    b->free = 1;

    // Merge forward
    if (b->next && b->next->free) {
        b = merge_blocks(b);
    }
    // Merge backward
    if (b->prev && b->prev->free) {
        b = merge_blocks(b->prev);
    }
}

void *my_calloc(size_t num, size_t size) {
    size_t total = num * size;
    void *ptr = my_malloc(total);
    if (ptr) memset(ptr, 0, total);
    return ptr;
}

void *my_realloc(void *ptr, size_t size) {
    if (!ptr) return my_malloc(size);
    meta_ptr b = get_block_addr(ptr);
    if (b->size >= size) return ptr;

    void *new_ptr = my_malloc(size);
    if (new_ptr) {
        memcpy(new_ptr, ptr, b->size);
        my_free(ptr);
    }
    return new_ptr;
}


void fill_matrix(int **matrix, int rows, int cols) {
    int val = 1;
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            matrix[i][j] = val++;
        }
    }
}

void print_matrix(int **matrix, int rows, int cols) {
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            printf("%d\t", matrix[i][j]);
        }
        printf("\n");
    }
}

int** create_matrix(int rows, int cols) {
    // Allocate array of row pointers
    int **matrix = (int**)my_calloc(rows, sizeof(int*));
    if (!matrix) return NULL;

    // Allocate each individual row
    for (int i = 0; i < rows; i++) {
        matrix[i] = (int*)my_calloc(cols, sizeof(int));
        if (!matrix[i]) return NULL;
    }
    return matrix;
}

void free_matrix(int **matrix, int rows) {
    for (int i = 0; i < rows; i++) {
        my_free(matrix[i]); // Free each row
    }
    my_free(matrix); // Free the pointer array
}

int main() {
    int r = 3, c = 3;
    printf("Creating %dx%d matrix using my_calloc...\n", r, c);
    
    int **matrix = create_matrix(r, c);
    
    printf("Initial Matrix (should be all zeros):\n");
    print_matrix(matrix, r, c);
    
    printf("\nFilling and printing matrix:\n");
    fill_matrix(matrix, r, c);
    print_matrix(matrix, r, c);
    
    free_matrix(matrix, r);
    return 0;
}