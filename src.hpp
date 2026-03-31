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
    }
    delete[] free_lists;
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

    // Find minimum aligned address by trying addresses from 0
    for (int addr = 0; addr < ram_size; addr += size) {
      if (allocate_block(layer, addr)) {
        return addr;
      }
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

    if (allocate_block(layer, addr)) {
      return addr;
    }
    return -1;
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

    add_free_block(layer, addr);

    // Try to merge with buddy
    merge_buddies(layer, addr);
  }

private:
  int ram_size;
  int min_block_size;
  int num_layers;
  FreeNode** free_lists;

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

  void add_free_block(int layer, int addr) {
    FreeNode* node = new FreeNode(addr);
    // Insert in sorted order for efficiency
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
      if (curr->addr > addr) break;  // Optimization: list is sorted
      curr = curr->next;
    }
    return false;
  }

  int get_buddy_addr(int layer, int addr) {
    int block_size = get_block_size(layer);
    return addr ^ block_size;
  }

  // Try to allocate a block at the given layer and address
  // Returns true if successful
  bool allocate_block(int layer, int addr) {
    int block_size = get_block_size(layer);

    // Check alignment
    if (addr % block_size != 0) return false;
    if (addr >= ram_size) return false;

    // If block is already free at this layer, use it
    if (has_free_block(layer, addr)) {
      remove_free_block(layer, addr);
      return true;
    }

    // Try to split a parent block
    if (layer >= num_layers - 1) return false;

    int parent_layer = layer + 1;
    int parent_block_size = get_block_size(parent_layer);
    int parent_addr = (addr / parent_block_size) * parent_block_size;

    // Recursively try to allocate the parent
    if (!allocate_block(parent_layer, parent_addr)) {
      return false;
    }

    // Parent was allocated, split it
    add_free_block(layer, parent_addr);
    add_free_block(layer, parent_addr + block_size);

    // Now allocate from the newly split blocks
    if (has_free_block(layer, addr)) {
      remove_free_block(layer, addr);
      return true;
    }

    return false;
  }

  void merge_buddies(int layer, int addr) {
    if (layer >= num_layers - 1) return;

    int buddy_addr = get_buddy_addr(layer, addr);

    // Check if buddy is free
    if (has_free_block(layer, buddy_addr)) {
      // Merge with buddy
      remove_free_block(layer, addr);
      remove_free_block(layer, buddy_addr);

      int parent_addr = (addr < buddy_addr) ? addr : buddy_addr;
      int parent_layer = layer + 1;

      add_free_block(parent_layer, parent_addr);

      // Recursively try to merge parent
      merge_buddies(parent_layer, parent_addr);
    }
  }
};

} // namespace sjtu
