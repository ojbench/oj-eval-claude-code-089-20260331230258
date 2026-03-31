// Copyright (c) 2024 ACM Class, SJTU

namespace sjtu {

// Simple linked list node for free blocks (no STL allowed)
struct FreeNode {
  int addr;
  FreeNode* next;

  FreeNode(int a) : addr(a), next(nullptr) {}
};

class BuddyAllocator {
public:
  /**
   * @brief Construct a new Buddy Allocator object with the given RAM size and
   * minimum block size.
   *
   * @param ram_size Size of the RAM. The address space is 0 ~ ram_size - 1.
   * @param min_block_size Minimum size of a block. The block size is 2^k where
   * k >= min_block_size.
   */
  BuddyAllocator(int ram_size, int min_block_size) {
    this->ram_size = ram_size;
    this->min_block_size = min_block_size;

    // Calculate number of layers
    num_layers = 0;
    int size = min_block_size;
    while (size <= ram_size) {
      num_layers++;
      size *= 2;
    }

    // Allocate free lists for each layer
    free_lists = new FreeNode*[num_layers];
    for (int i = 0; i < num_layers; i++) {
      free_lists[i] = nullptr;
    }

    // Allocate allocated tracking arrays
    allocated = new bool*[num_layers];
    for (int i = 0; i < num_layers; i++) {
      int num_blocks = ram_size / get_block_size(i);
      allocated[i] = new bool[num_blocks];
      for (int j = 0; j < num_blocks; j++) {
        allocated[i][j] = false;
      }
    }

    // Initialize top layer with all available blocks
    int top_layer = num_layers - 1;
    int block_size = get_block_size(top_layer);
    int num_blocks = ram_size / block_size;

    for (int i = 0; i < num_blocks; i++) {
      int addr = i * block_size;
      add_free_block(top_layer, addr);
    }
  }

  ~BuddyAllocator() {
    // Clean up all free lists
    for (int i = 0; i < num_layers; i++) {
      FreeNode* curr = free_lists[i];
      while (curr != nullptr) {
        FreeNode* next = curr->next;
        delete curr;
        curr = next;
      }
      delete[] allocated[i];
    }
    delete[] free_lists;
    delete[] allocated;
  }

  /**
   * @brief Allocate a block with the given size at the minimum available
   * address.
   *
   * @param size The size of the block.
   * @return int The address of the block. Return -1 if the block cannot be
   * allocated.
   */
  int malloc(int size) {
    int layer = get_layer(size);
    if (layer < 0) return -1;

    int min_addr = -1;

    // Find minimum available address aligned to size
    // First check existing free blocks
    FreeNode* curr = free_lists[layer];
    while (curr != nullptr) {
      if (curr->addr % size == 0) {
        if (min_addr == -1 || curr->addr < min_addr) {
          min_addr = curr->addr;
        }
      }
      curr = curr->next;
    }

    // Also check if we can create blocks at lower addresses by splitting
    for (int addr = 0; addr < ram_size && (min_addr == -1 || addr < min_addr); addr += size) {
      if (ensure_block_exists(layer, addr)) {
        if (has_free_block(layer, addr)) {
          min_addr = addr;
          break;  // Found minimum, no need to check further
        }
      }
    }

    if (min_addr != -1) {
      remove_free_block(layer, min_addr);
      mark_allocated(layer, min_addr, true);
      return min_addr;
    }

    return -1;
  }

  /**
   * @brief Allocate a block with the given size at the given address.
   *
   * @param addr The address of the block.
   * @param size The size of the block.
   * @return int The address of the block. Return -1 if the block cannot be
   * allocated.
   */
  int malloc_at(int addr, int size) {
    int layer = get_layer(size);
    if (layer < 0) return -1;

    // Ensure the block exists and is available
    if (!ensure_block_exists(layer, addr)) {
      return -1;
    }

    if (!has_free_block(layer, addr)) {
      return -1;
    }

    remove_free_block(layer, addr);
    mark_allocated(layer, addr, true);
    return addr;
  }

  /**
   * @brief Deallocate a block with the given size at the given address.
   *
   * @param addr The address of the block. It is ensured that the block is
   * allocated before.
   * @param size The size of the block.
   */
  void free_at(int addr, int size) {
    int layer = get_layer(size);
    if (layer < 0) return;

    mark_allocated(layer, addr, false);
    add_free_block(layer, addr);

    // Try to merge with buddy
    merge_if_possible(layer, addr);
  }

private:
  int ram_size;
  int min_block_size;
  int num_layers;
  FreeNode** free_lists;
  bool** allocated;  // Track which blocks are allocated at each layer

  int get_layer(int size) {
    int layer = 0;
    int block_size = min_block_size;
    while (block_size < size) {
      block_size *= 2;
      layer++;
    }
    return (layer < num_layers) ? layer : -1;
  }

  int get_block_size(int layer) {
    return min_block_size << layer;
  }

  int addr_to_index(int layer, int addr) {
    return addr / get_block_size(layer);
  }

  void mark_allocated(int layer, int addr, bool alloc) {
    int index = addr_to_index(layer, addr);
    allocated[layer][index] = alloc;
  }

  bool is_allocated(int layer, int addr) {
    int index = addr_to_index(layer, addr);
    return allocated[layer][index];
  }

  void add_free_block(int layer, int addr) {
    FreeNode* node = new FreeNode(addr);
    // Insert in sorted order
    if (free_lists[layer] == nullptr || free_lists[layer]->addr > addr) {
      node->next = free_lists[layer];
      free_lists[layer] = node;
    } else {
      FreeNode* curr = free_lists[layer];
      while (curr->next != nullptr && curr->next->addr < addr) {
        curr = curr->next;
      }
      node->next = curr->next;
      curr->next = node;
    }
  }

  bool remove_free_block(int layer, int addr) {
    if (free_lists[layer] == nullptr) return false;

    if (free_lists[layer]->addr == addr) {
      FreeNode* node = free_lists[layer];
      free_lists[layer] = node->next;
      delete node;
      return true;
    }

    FreeNode* curr = free_lists[layer];
    while (curr->next != nullptr) {
      if (curr->next->addr == addr) {
        FreeNode* node = curr->next;
        curr->next = node->next;
        delete node;
        return true;
      }
      curr = curr->next;
    }
    return false;
  }

  bool has_free_block(int layer, int addr) {
    FreeNode* curr = free_lists[layer];
    while (curr != nullptr) {
      if (curr->addr == addr) return true;
      curr = curr->next;
    }
    return false;
  }

  int get_buddy_addr(int layer, int addr) {
    int block_size = get_block_size(layer);
    return addr ^ block_size;
  }

  // Check if a range is available (no blocks allocated in this range at any layer)
  bool is_range_available(int start_addr, int size) {
    // Check all layers to see if any allocated block overlaps with this range
    for (int l = 0; l < num_layers; l++) {
      int block_size = get_block_size(l);
      // Check blocks that could overlap
      int start_block = start_addr / block_size;
      int end_block = (start_addr + size - 1) / block_size;
      for (int b = start_block; b <= end_block; b++) {
        int block_addr = b * block_size;
        if (is_allocated(l, block_addr)) {
          // Check if this allocated block overlaps with our range
          if (block_addr < start_addr + size && block_addr + block_size > start_addr) {
            return false;
          }
        }
      }
    }
    return true;
  }

  // Ensure a block exists at the given layer and address
  // by splitting parent blocks if necessary
  bool ensure_block_exists(int layer, int addr) {
    // Check if we can even have a block at this address
    int block_size = get_block_size(layer);
    if (addr % block_size != 0) return false;
    if (addr >= ram_size) return false;

    // If block is already allocated, we can't use it
    if (is_allocated(layer, addr)) return false;

    // Check if the range overlaps with any allocated blocks
    if (!is_range_available(addr, block_size)) return false;

    // If block is already free, it exists
    if (has_free_block(layer, addr)) return true;

    // Need to split parent
    if (layer >= num_layers - 1) return false;

    int parent_layer = layer + 1;
    int parent_block_size = get_block_size(parent_layer);
    int parent_addr = (addr / parent_block_size) * parent_block_size;

    // Recursively ensure parent exists
    if (!ensure_block_exists(parent_layer, parent_addr)) {
      return false;
    }

    // Parent exists and is free, split it
    if (!has_free_block(parent_layer, parent_addr)) {
      return false;
    }

    remove_free_block(parent_layer, parent_addr);
    add_free_block(layer, parent_addr);
    add_free_block(layer, parent_addr + block_size);

    return has_free_block(layer, addr);
  }

  void merge_if_possible(int layer, int addr) {
    if (layer >= num_layers - 1) return;

    int buddy_addr = get_buddy_addr(layer, addr);

    // Check if buddy is free and not allocated
    if (has_free_block(layer, buddy_addr) && !is_allocated(layer, buddy_addr)) {
      // Merge
      remove_free_block(layer, addr);
      remove_free_block(layer, buddy_addr);

      int parent_addr = (addr < buddy_addr) ? addr : buddy_addr;
      int parent_layer = layer + 1;

      add_free_block(parent_layer, parent_addr);

      // Recursively try to merge parent
      merge_if_possible(parent_layer, parent_addr);
    }
  }
};

} // namespace sjtu
