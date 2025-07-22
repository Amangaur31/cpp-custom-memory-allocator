# cpp-custom-memory-allocator
An efficient custom memory allocator in C++ with block splitting and coalescing.

# Custom Memory Allocator in C++

This project is a custom memory allocator written in C++ that demonstrates efficient, low-level memory management. It serves as a replacement for the standard `malloc`/`free` or `new`/`delete` by managing a pre-allocated pool of memory.

## Core Features

This allocator was built to be efficient and reduce memory waste using two key techniques:

1.  **Block Splitting:** When a memory request is made, the allocator finds a free block. If the block is larger than necessary, it is split into two: one part is allocated to the user, and the smaller, leftover part is returned to the free list. This minimizes internal fragmentation.

2.  **Block Coalescing:** When a block of memory is deallocated, the allocator checks its immediate physical neighbors. If an adjacent block is also free, they are merged (coalesced) into a single, larger free block. This fights external fragmentation and ensures large contiguous blocks remain available.

## Data Structure Used

The core of this allocator is a **doubly linked list** that keeps track of all the free memory blocks. The nodes of this list are cleverly stored *inside* the free blocks themselves (in the `BlockHeader`), meaning no extra memory is wasted on managing the list.

## How to Build and Run

This project is self-contained in `main.cpp`. You can compile and run it using a C++17 compliant compiler (like g++ or Clang).

```bash
# Compile the program
g++ -std=c++17 -o allocator main.cpp

# Run the executable to see the test cases
./allocator
