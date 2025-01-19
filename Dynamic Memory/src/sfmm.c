#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "debug.h"
#include "sfmm.h"
#include <errno.h>

#define MAX_BLK_SIZE 0xFFFFFFF0
static size_t maxPayload = 0; //max aggregate payload
static size_t currPayload = 0; //current payload in use
static size_t memUsed = 0; //memory allocated
static size_t heapSize = 0; //heap size
static int listEmpty = 0; //0 if heap not yet touched, else 1
//blocks to help contain memory currently used from heap
static sf_block* prologue = NULL;
static sf_block* epilogue = NULL;

static size_t pad(size_t size) {
    size_t padded = size;
    if(padded % 16 != 0) padded += 16 - (size % 16); //make it multiple of 16 bytes, 0-15 will be 16, 17-31 will be 32
    padded += 16; //header & footer
    return padded;
}

static int getIdx(size_t size) {
    size_t lower = 32, upper = 32, temp;
    int idx = 0;
    while(upper < size && idx < NUM_FREE_LISTS - 1) {
        idx++;
        temp = upper;
        upper += lower;
        lower = temp;
    }

    return idx;
}

static int isInvalidPointer(void* ptr) {
    sf_block* block = (sf_block*) (ptr - (size_t)16);
    sf_header header = block->header;
    size_t blockSize = (header & MAX_BLK_SIZE);

    //block size below min size or not multiple of 16 or not 16 byte aligned
    if(blockSize < 32 || blockSize % 16 != 0 || ((uintptr_t) ptr) % 16 != 0) {
        return 1;
    }

    //block not contained b/w prologue & epilogue (the boundary blocks)
    if(block < prologue || block > epilogue) {
        return 1;
    }

    //freeing un-allocated block
    if((header & 0x8) == 0) {
        return 1;
    }

    sf_footer prevFooter = block->prev_footer;
    size_t prevBLKSize = prevFooter & MAX_BLK_SIZE;
    sf_block* prev = (sf_block*) ((void*) block - prevBLKSize);
    //Block says previous block is free but in actuality it is not
    if((header & 0x4) == 0 && (prev->header & 0x8) != 0) {
        return 1;
    }

    //valid pointer
    return 0;
}

static void initialize_free_list() {
    for(int i=0; i<NUM_FREE_LISTS; i++) {
        sf_free_list_heads[i].body.links.next = &sf_free_list_heads[i];
        sf_free_list_heads[i].body.links.prev = &sf_free_list_heads[i];
    }
}

static void remove_block(sf_block* block) {
    sf_block* prev = block->body.links.prev;
    sf_block* next = block->body.links.next;

    prev->body.links.next = next;
    next->body.links.prev = prev;
}

static void insert_free_list(sf_block* block) {
    size_t blockSize = block->header & MAX_BLK_SIZE;
    int idx = getIdx(blockSize);
    if(idx > NUM_FREE_LISTS - 2) idx = NUM_FREE_LISTS - 1;
    sf_block* sentinel = &sf_free_list_heads[idx];

    //If empty free list
    if(sentinel == sentinel->body.links.next && sentinel == sentinel->body.links.prev) {
        block->body.links.prev = sentinel;
        block->body.links.next = sentinel->body.links.next;

        sentinel->body.links.next = block;
        sentinel->body.links.prev = block;
    } else { //at least one node
        sf_block* next = sentinel->body.links.next;
        block->body.links.next = next;
        block->body.links.prev = sentinel;
        sentinel->body.links.next = block;
        next->body.links.prev = block;
    }
}

static void* search_free_list(int idx, size_t size) {
    sf_block* allocated = NULL;
    for(int i = idx; i<NUM_FREE_LISTS - 1; i++) {
        if(&sf_free_list_heads[i] == sf_free_list_heads[i].body.links.next && &sf_free_list_heads[i] == sf_free_list_heads[i].body.links.prev) { 
            //free list is empty
            continue;
        } else {
            sf_block* sentinal = &sf_free_list_heads[i];
            sf_block* current = sentinal->body.links.next;
            while(current != sentinal) { //While list has not been fully looked
                sf_header header = current->header;
                header = header & MAX_BLK_SIZE;
                if(header >= size) { //block found
                    allocated = current;
                    // remove_block(allocated);
                    break;
                } else {
                    current = current->body.links.next; //keep traversing this list
                }
            }
        }
    }

    return (void*) allocated;
}

static void* coalesce(sf_block* a, sf_block* b) {
    sf_block* block = a;
    size_t blockSize = (a->header & MAX_BLK_SIZE) + (b->header & MAX_BLK_SIZE);

    block->prev_footer = a->prev_footer; //seems redundant
    block->header = blockSize | (a->header & 0x4); //new block header with new size of a + b, and copy over pAlloc bit

    sf_block* next = (sf_block*) ((void*) block + blockSize);
    next->prev_footer = block->header;
    b->prev_footer = 0x0; //clear b prev_footer
    b->header = 0x0; //clear b header

    return block;
}

static void* split(sf_block* block, size_t sizeA, size_t payload) {
    sf_block* nextBlock = (sf_block*) ((void*) block + (size_t) (block->header & MAX_BLK_SIZE));
    size_t blockSize = block->header & MAX_BLK_SIZE;
    size_t sizeB = blockSize - sizeA;
    if(sizeB >= 32 && sizeB % 16 == 0) {
        //a is first split block, b is second split block, block = a + b*
        //header of a, footer of a which is prev footer in b
        sf_header headerA = (payload << 32) | sizeA | (1 << 3) | (block->header & 0x4);
        sf_footer footerA = (payload << 32) | sizeA | (1 << 3) | (block->header & 0x4);
        sf_block* a = block;
        a->header = headerA;

        //header of b, footer of b which is prev footer in next block
        sf_header headerB = sizeB | (1 << 2);
        sf_footer footerB = sizeB | (1 << 2);
        sf_block* b = (sf_block*) ((void*) block + (size_t)(sizeA));
        b->header = headerB;
        b->prev_footer = footerA;
        nextBlock->prev_footer = footerB;

        nextBlock->header = (nextBlock->header >> 3) << 3; //Clear out previous allocation bit

        if(nextBlock != epilogue) {
            sf_block* nextNextBlock = (sf_block*) ((void*) nextBlock + (nextBlock->header & MAX_BLK_SIZE));
            nextNextBlock->prev_footer = nextBlock->header;

            if((nextBlock->header & 0x8) == 0) { //if next block is free, coalesce both
                remove_block(nextBlock);
                b = (sf_block*) coalesce(b, nextBlock);
            }
        }

        insert_free_list(b);
        return a;
    }

    block->header |= (payload << 32);
    block->header |= 0x8;
    nextBlock->prev_footer |= (payload << 32);
    nextBlock->prev_footer |= 0x8;
    nextBlock->header |= 0x4;
    return block; //Split not possible
}

//Set up prologue, wilderness, and epilogue blocks, put wilderness in free list
static void heap_setup() {
    prologue = (sf_block*) sf_mem_grow();
    heapSize += PAGE_SZ;
    prologue->header = 0x28; //size 32 and allocated
    sf_block* block = (sf_block*) (sf_mem_start() + 32);
    block->prev_footer = 0x28;
    block->header = 0xfd4;
    epilogue = (sf_block*) (sf_mem_end() - 16);
    epilogue->prev_footer = 0xfd4;
    epilogue->header = 0x8;

    sf_block* sentinel = &sf_free_list_heads[NUM_FREE_LISTS - 1];
    sentinel->body.links.next = block;
    sentinel->body.links.prev = block;
    block->body.links.next = sentinel;
    block->body.links.prev = sentinel;
}

static void heap_extend() {
    sf_block* block = (sf_block*) sf_mem_grow();
    if(block == NULL) {
        //Allocation failed
        sf_errno = ENOMEM;
        return;
    }

    //Update heap size, format the block
    heapSize += PAGE_SZ;

    //prevBlock footer & old epilogue header
    sf_footer prevFooter = epilogue->prev_footer;
    sf_header epiHeader = epilogue->header;

    block = epilogue; //New block actually starts from old epilogue
    block->prev_footer = prevFooter;
    block->header = PAGE_SZ | epiHeader;

    //create new epilogue
    epilogue = (sf_block*) (sf_mem_end() - 16);
    epilogue->prev_footer = block->header;
    epilogue->header = epiHeader;

    //check if previous last block was free or not, if free merge
    if((block->header & 0x4) == 0) { //free
        size_t blkSize = prevFooter & MAX_BLK_SIZE;
        sf_block* prev = (sf_block*) ((void*) block - blkSize);
        remove_block(prev);
        block = (sf_block*) coalesce(prev, block);
    }

    sf_block* sentinel = &sf_free_list_heads[NUM_FREE_LISTS - 1];
    sf_block* next = sentinel->body.links.next;
    block->body.links.next = next;
    block->body.links.prev = sentinel;
    sentinel->body.links.next = block;
    next->body.links.prev = block;
}

void *sf_malloc(size_t size) {
    if(size == 0) { //Empty request
        return NULL;
    }

    if(listEmpty == 0) {
        initialize_free_list();
        heap_setup();
        listEmpty = 1;
    }

    size_t sizeP = pad(size);
    sf_block* allocated = (sf_block*) search_free_list(getIdx(sizeP), sizeP);

    //Check wilderness region
    if(allocated == NULL) {
        //Check if heap is empty, then extend heap. Afterwards continue extending until allocation block successfully done so
        sf_block* sentinel = &sf_free_list_heads[NUM_FREE_LISTS - 1];
        if(sentinel == sentinel->body.links.next && sentinel == sentinel->body.links.prev) {
            heap_extend();
            if(sf_errno == ENOMEM) {
                return NULL;
            }
        }

        allocated = (sf_block*) sf_free_list_heads[NUM_FREE_LISTS - 1].body.links.next;
        size_t allocatedBLKSize = allocated->header & MAX_BLK_SIZE;
        while((void*) allocated + allocatedBLKSize != epilogue && allocatedBLKSize < sizeP) {
            allocated = allocated->body.links.next;
            allocatedBLKSize = allocated->header & MAX_BLK_SIZE;
        }

        while(allocatedBLKSize < sizeP) { //continuously extend heap until large allocation request met
            heap_extend();
            if(sf_errno == ENOMEM) {
                return NULL;
            }

            allocatedBLKSize += PAGE_SZ;
        }
    }

    //Allocation not successful
    if(allocated == NULL) {
        sf_errno = ENOMEM;
        return NULL;
    } else {
        //if possible to split, split it + insert_free_list remainder
        remove_block(allocated);
        allocated = (sf_block*) split(allocated, sizeP, size);
        //for statistics
        sf_header header = allocated->header;
        size_t payload = header >> 32; //get payload size
        currPayload += payload;
        size_t blockSize = header & MAX_BLK_SIZE;
        memUsed += blockSize;
        if(currPayload > maxPayload) {
            maxPayload = currPayload;
        }
    }

    return allocated->body.payload;
}

void sf_free(void *pp) {
    if(pp == NULL || isInvalidPointer(pp)) {
        abort();
    }

    sf_block* block = (sf_block*) (pp - 16);
    sf_header header = block->header;
    size_t blockSize = header & MAX_BLK_SIZE;

    memUsed -= blockSize; //allocated memory decreases
    currPayload -= header >> 32; //less payload in circulation

    //clear allocation bit in current block both in header & footer, pal of next block
    size_t prevAlloc = (header & 0x4) >> 2;
    block->header = (header & MAX_BLK_SIZE) | (prevAlloc << 2);
    sf_block* next = (sf_block*) ((void*) block + blockSize);
    next->header &= 0xFFFFFFFFFFFFFFF8;
    next->prev_footer = block->header;

    if(next != epilogue) {
        //make footer of next block same as header
        sf_block* nextNext = (sf_block*) ((void*) next + (next->header & MAX_BLK_SIZE));
        nextNext->prev_footer = next->header;
    }

    //If previous block in heap is free, coalesce with previous block
    if(prevAlloc == 0) {
        size_t prevBlockSize = block->prev_footer & MAX_BLK_SIZE;
        sf_block* prev = (sf_block*) ((void*) block - prevBlockSize);
        if(prev > prologue) {
            remove_block(prev);
            block = (sf_block*) coalesce(prev, block);
        }
    }

    //If next block is not epilogue & it is free block
    if(next < epilogue && (next->header & 0x8) == 0) {
        remove_block(next);
        block = (sf_block*) coalesce(block, next);
    }

    insert_free_list(block);
}

void *sf_realloc(void *pp, size_t rsize) {
    if(isInvalidPointer(pp)) {
        sf_errno = EINVAL;
        abort();
    }

    if(rsize == 0) {
        sf_free(pp);
        return NULL;
    }

    //New Requested block size min
    size_t newSize = pad(rsize);

    //Get current block
    sf_block* oldBlock = (sf_block*) (pp - 16); //Get to root address from payload
    size_t oldSize = oldBlock->header & MAX_BLK_SIZE;
    size_t oldPayloadSize = oldBlock->header >> 32;

    //if same size, just return the pointer back, else get new pointer
    if(newSize == oldSize) return pp;
    currPayload -= oldPayloadSize;
    memUsed -= oldSize;

    //new block is larger
    if(oldSize < newSize) {
        void* payload = sf_malloc(rsize);
        if(payload == NULL) {
            return NULL;
        }

        payload = memcpy(payload, pp, rsize);
        sf_free(pp);
        return payload;
    }

    //new block is smaller
    sf_block* newBlock = (sf_block*) split(oldBlock, newSize, rsize);

    currPayload += newBlock->header >> 32;
    memUsed += newBlock->header & MAX_BLK_SIZE;
    return newBlock->body.payload;
}

double sf_fragmentation() {
    if(memUsed == 0) return 0.0;
    return (double) currPayload / (double) memUsed;
}

double sf_utilization() {
    if(heapSize == 0) return 0.0;
    return (double) maxPayload / (double) heapSize;
}