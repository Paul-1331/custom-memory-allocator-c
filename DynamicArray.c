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



typedef struct {
    int *data;     // Pointer to the allocated block
    int size;     // Current number of elements
    int capacity; // Current maximum capacity
} DynamicArray;

// Initialize the array
void init_array(DynamicArray *arr, int initial_cap) {
    arr->data = (int*)my_malloc(initial_cap * sizeof(int));
    if (!arr->data) {
        printf("Initial allocation failed!\n");
        return;
    }
    arr->size = 0;
    arr->capacity = initial_cap;
    printf("Initial array created with capacity: %d\n", initial_cap);
}

// Requirement: Double capacity if full
void insert_element(DynamicArray *arr, int val) {
    if (arr->size == arr->capacity) {
        int old_cap = arr->capacity;
        arr->capacity *= 2; 
        
        // my_realloc handles: 1. Finding new space 2. Copying data 3. Freeing old space
        arr->data = (int*)my_realloc(arr->data, arr->capacity * sizeof(int));
        
        printf("Capacity reached! Resized from %d to %d\n", old_cap, arr->capacity);
    }
    arr->data[arr->size++] = val;
    printf("Inserted %d (Current Size: %d)\n", val, arr->size);
}

// Requirement: Shrink if size < 25% of capacity
void remove_element(DynamicArray *arr) {
    if (arr->size == 0) {
        printf("Array is empty!\n");
        return;
    }
    
    arr->size--;
    printf("Removed last element. (Current Size: %d)\n", arr->size);

    // Only shrink if we have a reasonable capacity to avoid constant reallocs
    if (arr->size > 0 && arr->size < arr->capacity / 4 && arr->capacity > 4) {
        int old_cap = arr->capacity;
        arr->capacity /= 2;
        arr->data = (int*)my_realloc(arr->data, arr->capacity * sizeof(int));
        printf("Size < 25%%. Shrunk capacity from %d to %d\n", old_cap, arr->capacity);
    }
}

void print_array(DynamicArray *arr) {
    if (arr->size == 0) {
        printf("Array: [Empty]\n");
        return;
    }
    printf("Array: ");
    for (int i = 0; i < arr->size; i++) {
        printf("%d ", arr->data[i]);
    }
    printf("| Size: %d, Capacity: %d\n", arr->size, arr->capacity);
}

void free_array(DynamicArray *arr) {
    my_free(arr->data);
    arr->data = NULL;
    arr->size = 0;
    arr->capacity = 0;
    printf("Dynamic Array memory freed.\n");
}

int main() {
    DynamicArray arr;
    int n;
    printf("Enter the initial size of the Dynamic Array\n");
    scanf("%d",&n);
    init_array(&arr, n);
    char op;
    do{
        scanf(" %c",&op);
        if(op=='i'){
            int d;
            scanf("%d",&d);
            insert_element(&arr,d);
        }
        else if(op=='p'){
            print_array(&arr);
        }
        else if(op=='r'){
            remove_element(&arr);
        }
    }while(op!='e');

    free_array(&arr);
    return 0;
}