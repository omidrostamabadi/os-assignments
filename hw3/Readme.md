# Build and run
`make all` produces the executable `malloc_test` in the same directory. Run using `./malloc_test` in the current directory.

# Interface
Three functions are provided by the allocator:
* `void *mm_alloc(size_t size)` -> Allocate size bytes of memory
* `void mm_free(void *ptr)` -> Free the allocated memory
* `void *mm_realloc(void *ptr, size_t size)` -> Change ptr to size bytes of memory

# How does the allocator works?
The allocators manages the heap memory by keeping regions of it in a linked list. Each region's size is known, and it is assigned to a request in a first-fit manner. The allocator merges the contigious free regions into a single region to prevent memory fragmentation. When no region can satisfy the size requirements of the input, the `sbrk()` system call is used to extend the heap region.

The `mm_test.c` can be changed to test the allocator for various situations.
