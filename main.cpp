#include <iostream>
#include <cstddef> // for size_t
#include <vector>
#include <numeric> // for std::accumulate
#include <iomanip> // for std::setw

// =================================================================================
// BlockHeader: Metadata for each memory block
//
// This struct is the core of our memory management. It's placed at the beginning
// of every memory block (both allocated and free). The clever part is that the
// linked list pointers for the free list are stored within the free blocks
// themselves, so we don't waste extra space.
// =================================================================================
struct BlockHeader {
    size_t size;      // The size of this block (including the header).
    bool is_free;     // True if the block is free, false if allocated.
    BlockHeader* next;  // Pointer to the next block in the *free list*.
    BlockHeader* prev;  // Pointer to the previous block in the *free list*.
};

// =================================================================================
// Allocator Class
//
// This class encapsulates all the logic for memory management. It requests a large
// chunk of memory from the OS upon creation and then manages it internally.
// =================================================================================
class Allocator {
public:
    // Constructor: Initializes the memory pool.
    Allocator(size_t pool_size) : m_pool_size(pool_size) {
        if (pool_size < sizeof(BlockHeader)) {
            m_memory_pool = nullptr;
            m_free_list_head = nullptr;
            std::cerr << "Pool size is too small." << std::endl;
            return;
        }

        // Allocate the memory pool from the OS.
        m_memory_pool = new char[pool_size];

        // The entire pool starts as a single, large free block.
        m_free_list_head = static_cast<BlockHeader*>(m_memory_pool);
        m_free_list_head->size = pool_size;
        m_free_list_head->is_free = true;
        m_free_list_head->next = nullptr;
        m_free_list_head->prev = nullptr;
    }

    // Destructor: Releases the memory pool back to the OS.
    ~Allocator() {
        delete[] static_cast<char*>(m_memory_pool);
    }

    // allocate: The custom 'malloc' implementation.
    void* allocate(size_t size);

    // deallocate: The custom 'free' implementation.
    void deallocate(void* ptr);

    // print_free_list: A utility to visualize the state of our free list.
    void print_free_list() const;

private:
    void* m_memory_pool;
    size_t m_pool_size;
    BlockHeader* m_free_list_head;

    // removeFromFreeList: Helper to remove a block from the doubly linked free list.
    void removeFromFreeList(BlockHeader* block);

    // addToFreeList: Helper to add a block to the front of the free list.
    void addToFreeList(BlockHeader* block);
};

// --- Allocator Method Implementations ---

void Allocator::removeFromFreeList(BlockHeader* block) {
    if (block->prev) {
        block->prev->next = block->next;
    } else {
        // This block was the head of the list.
        m_free_list_head = block->next;
    }
    if (block->next) {
        block->next->prev = block->prev;
    }
}

void Allocator::addToFreeList(BlockHeader* block) {
    block->is_free = true;
    block->next = m_free_list_head;
    block->prev = nullptr;
    if (m_free_list_head) {
        m_free_list_head->prev = block;
    }
    m_free_list_head = block;
}

void* Allocator::allocate(size_t size) {
    if (size == 0) {
        return nullptr;
    }

    // Calculate the total size needed, including the header.
    const size_t total_size_needed = size + sizeof(BlockHeader);
    BlockHeader* current = m_free_list_head;

    // --- First-Fit Search ---
    // Traverse the free list to find a suitable block.
    while (current) {
        if (current->size >= total_size_needed) {
            // Found a suitable block.
            
            // --- Block Splitting ---
            // If the block is large enough to be split, do so.
            // The remaining part must be large enough to hold at least a header.
            if (current->size > total_size_needed + sizeof(BlockHeader)) {
                
                // Create the new free block from the remainder.
                BlockHeader* new_free_block = (BlockHeader*)((char*)current + total_size_needed);
                new_free_block->size = current->size - total_size_needed;
                new_free_block->is_free = true; // It's a free block.

                // Update the original block to be the allocated size.
                current->size = total_size_needed;
                
                // Replace the old large block with the new smaller free block in the list.
                new_free_block->next = current->next;
                new_free_block->prev = current->prev;
                if (current->prev) {
                    current->prev->next = new_free_block;
                } else {
                    m_free_list_head = new_free_block;
                }
                if (current->next) {
                    current->next->prev = new_free_block;
                }

            } else {
                // The block is a perfect fit or too small to split. Use the whole thing.
                removeFromFreeList(current);
            }

            current->is_free = false;
            // Return a pointer to the memory region *after* the header.
            return (void*)((char*)current + sizeof(BlockHeader));
        }
        current = current->next;
    }

    // No suitable block found.
    std::cerr << "Out of memory!" << std::endl;
    return nullptr;
}

void Allocator::deallocate(void* ptr) {
    if (ptr == nullptr) {
        return;
    }

    // Get the header from the user's pointer.
    BlockHeader* block_to_free = (BlockHeader*)((char*)ptr - sizeof(BlockHeader));
    
    // --- Coalescing (Merging) Logic ---
    
    // 1. Coalesce with the block physically to the right.
    BlockHeader* next_physical_block = (BlockHeader*)((char*)block_to_free + block_to_free->size);
    
    // Check if the next block is within the pool bounds and is free.
    if ((char*)next_physical_block < (char*)m_memory_pool + m_pool_size && next_physical_block->is_free) {
        block_to_free->size += next_physical_block->size; // Merge sizes.
        removeFromFreeList(next_physical_block); // Remove the merged block from the free list.
    }

    // 2. Coalesce with the block physically to the left.
    // This is trickier. We iterate through the free list to find a block
    // that ends exactly where our block_to_free begins.
    BlockHeader* current_free = m_free_list_head;
    while(current_free) {
        if ((char*)current_free + current_free->size == (char*)block_to_free) {
            current_free->size += block_to_free->size; // Merge sizes.
            // The block to free is now part of the left block, so we just return.
            // The left block is already in the free list, so no further action is needed.
            return;
        }
        current_free = current_free->next;
    }

    // If no coalescing happened with the left block, add the current block to the free list.
    addToFreeList(block_to_free);
}

void Allocator::print_free_list() const {
    std::cout << "--- Free List Status ---" << std::endl;
    BlockHeader* current = m_free_list_head;
    if (!current) {
        std::cout << "[EMPTY]" << std::endl;
        return;
    }

    int i = 0;
    while (current) {
        std::cout << "Block " << std::setw(2) << i++
                  << ": Address = " << current
                  << ", Size = " << std::setw(5) << current->size << " bytes" << std::endl;
        current = current->next;
    }
    std::cout << "------------------------" << std::endl << std::endl;
}


// =================================================================================
// main: Test driver for the Allocator
// =================================================================================
int main() {
    const size_t POOL_SIZE = 1024; // 1 KB pool
    Allocator allocator(POOL_SIZE);

    std::cout << "Initial state:" << std::endl;
    allocator.print_free_list();

    // --- Test 1: Simple Allocation & Block Splitting ---
    std::cout << "--- Test 1: Allocating 100, 200, and 50 bytes ---" << std::endl;
    void* p1 = allocator.allocate(100);
    void* p2 = allocator.allocate(200);
    void* p3 = allocator.allocate(50);

    std::cout << "State after allocations:" << std::endl;
    allocator.print_free_list();

    // --- Test 2: Deallocation & Coalescing ---
    std::cout << "--- Test 2: Freeing the middle block (p2) ---" << std::endl;
    allocator.deallocate(p2);
    p2 = nullptr;
    std::cout << "State after freeing p2:" << std::endl;
    allocator.print_free_list(); // Should have two free blocks now.

    std::cout << "--- Freeing the first block (p1) ---" << std::endl;
    allocator.deallocate(p1);
    p1 = nullptr;
    std::cout << "State after freeing p1 (should coalesce with p2's old space):" << std::endl;
    allocator.print_free_list(); // The two free blocks should merge.

    std::cout << "--- Freeing the last block (p3) ---" << std::endl;
    allocator.deallocate(p3);
    p3 = nullptr;
    std::cout << "State after freeing p3 (should coalesce into one large block):" << std::endl;
    allocator.print_free_list(); // Should be back to a single free block of 1024 bytes.

    // --- Test 3: Stress Test ---
    std::cout << "\n--- Test 3: Stress Test ---" << std::endl;
    std::vector<void*> pointers;
    for (int i = 0; i < 5; ++i) {
        pointers.push_back(allocator.allocate(60));
    }
    allocator.print_free_list();

    allocator.deallocate(pointers[1]);
    allocator.deallocate(pointers[3]);
    std::cout << "State after freeing pointers at index 1 and 3:" << std::endl;
    allocator.print_free_list();

    allocator.deallocate(pointers[2]);
    std::cout << "State after freeing pointer at index 2 (should coalesce 1, 2, and 3):" << std::endl;
    allocator.print_free_list();

    // Clean up remaining allocations
    allocator.deallocate(pointers[0]);
    allocator.deallocate(pointers[4]);
    std::cout << "Final state after all cleanup:" << std::endl;
    allocator.print_free_list();

    return 0;
}
