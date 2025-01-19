# DynamicMemoryAllocator

Custom implementation of C stdlib memory management functions (malloc, realloc, free):
- Memory blocks aligned to 16 byte boundaries (each block has a header and footer)
- Freed blocks are immediately coalesced (no deferred policy) into 'Free List'
- Free lists maintained Last in - First Out discipline
- Free lists segregated by size ranges, first-fit policy to fit a freed block in specific free list
- Block splitting without splinters
- Prologue and Epilogue blocks at the 2 ends of heap for convenience of managing dynamic memory allocation
