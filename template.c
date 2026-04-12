#include<stdio.h>
#include<stddef.h>
#include<unistd.h>
#include<string.h>

typedef struct meta_block *meta_ptr;

struct meta_block{
    int free;
    size_t size;
    meta_ptr next;
    meta_ptr prev;
    void *ptr;
    char data[1];
};

#define BLOCK_SIZE offsetof(struct meta_block, data)
void *base = NULL;

meta_ptr get_block_addr(void *p){
    return (meta_ptr)((char*)p-BLOCK_SIZE);
}

meta_ptr merge_blocks(meta_ptr block){// reduces external fragmentation
    if(block->next&&block->next->free){// if next block is available and it is free
        block->size += BLOCK_SIZE + block->next->size;
        block->next = block->next->next; // connect to the block after the next block
        if(block->next){
            block->next->prev = block;
        }
    }
    return block;
}

meta_ptr find_suitable_block(meta_ptr*last, size_t size){
    meta_ptr b = (meta_ptr)base;
    while(b && !(b->free&&b->size>=size)){ // searching for a valid block, the valid block
        // should be free and its size should be >= size
        *last = b;
        b = b->next;
    }
    return b;
}

void split_space(meta_ptr block,size_t size){ // reduces internal fragmentation
    meta_ptr new_block;
    new_block = (meta_ptr)(block->data+size);
    new_block->size = block->size - size  - BLOCK_SIZE;
    new_block->next = block->next;
    new_block->prev = block;
    new_block->free = 1;
    new_block->ptr = new_block->data;

    if(new_block->next){
        new_block->next->prev = new_block;
    }

    block->size = size;
    block->next = new_block;
}

meta_ptr extend_heap(meta_ptr last, size_t size){
    meta_ptr b = sbrk(0);
    if(sbrk(BLOCK_SIZE+size)== (void*)-1) return NULL;

    b->size = size;
    b->next = NULL;
    b->prev = last;
    b->ptr = b->data;
    b->free = 0;
    if(last) last->next = b;
    return b;
}


//PUBLIC_API

void *my_malloc(size_t size){
    meta_ptr b,last;
    if(base){
        last = base;
        b = find_suitable_block(&last,size);
        if(b){
            if((b->size-size)>=(BLOCK_SIZE+4)) split_space(b,size);
            b->free = 0;
        }
        else{
            b = extend_heap(last,size);
        }
    }
    else{
        b = extend_heap(NULL,size);
        base = b;
    }
    return (b?b->data:NULL);
}

void my_free(void*ptr){
    if(!ptr) return;
    meta_ptr b = get_block_addr(ptr);
    b->free = 1;

    if(b->next&&b->next->free){
        b = merge_block(b);
    }
    if(b->prev&&b->prev->free){
        b = merge_blocks(b->prev);
    }
}

void *my_calloc(size_t num, size_t size){
    size_t total = num*size;
    void*ptr = my_malloc(total);
    if(ptr) memset(ptr,0,total);
    return ptr;
}

void*realloc(void*ptr,size_t size){
    if(!ptr) return my_malloc(size);
    meta_ptr b = get_block_addr(ptr);
    if(b->size>=size) return ptr;

    void *new_ptr = my_malloc(size);
    if(new_ptr){
        memcpy(new_ptr,ptr,b->size);
        my_free(ptr);
    }
    return new_ptr;
}


// Dynamic Array Implementation

typedef struct{
    int *data;
    int size;
    int capacity;
}DynamicArray;

void init_array(DynamicArray*arr,int initial){
    arr->data = (int*)my_malloc(sizeof(int)*initial);
    if(!arr->data){
        printf("error\n");
        return;
    }
    arr->size = 0;
    arr->capacity = initial;
}

void insert_element(DynamicArray*arr,int val){
    if(arr->size==arr->capacity){
        arr->capacity*=2;

        arr->data = (int*)my_realloc(arr->data,arr->capacity*sizeof(int));
    }
    arr->data[arr->size++] = val;
}