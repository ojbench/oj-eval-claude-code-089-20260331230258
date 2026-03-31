// Copyright (c) 2024 ACM Class, SJTU

namespace sjtu {

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

    // Allocate free bit arrays for each layer
    free_blocks = new bool*[num_layers];
    num_blocks_per_layer = new int[num_layers];

    for (int i = 0; i < num_layers; i++) {
      int block_size = get_block_size(i);
      num_blocks_per_layer[i] = ram_size / block_size;
      free_blocks[i] = new bool[num_blocks_per_layer[i]];
      for (int j = 0; j < num_blocks_per_layer[i]; j++) {
        free_blocks[i][j] = false;
      }
    }

    // Initialize top layer with all available blocks
    int top_layer = num_layers - 1;
    for (int i = 0; i < num_blocks_per_layer[top_layer]; i++) {
      free_blocks[top_layer][i] = true;
    }
  }

  ~BuddyAllocator() {
    for (int i = 0; i < num_layers; i++) {
      delete[] free_blocks[i];
    }
    delete[] free_blocks;
    delete[] num_blocks_per_layer;
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

    // Find minimum aligned address
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

    int index = addr_to_index(layer, addr);
    free_blocks[layer][index] = true;

    // Try to merge with buddy
    merge_buddies(layer, addr);
  }

private:
  int ram_size;
  int min_block_size;
  int num_layers;
  bool** free_blocks;
  int* num_blocks_per_layer;

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

  int index_to_addr(int layer, int index) {
    return index * get_block_size(layer);
  }

  bool is_free(int layer, int addr) {
    int block_size = get_block_size(layer);
    if (addr % block_size != 0) return false;
    if (addr >= ram_size) return false;
    int index = addr_to_index(layer, addr);
    if (index >= num_blocks_per_layer[layer]) return false;
    return free_blocks[layer][index];
  }

  int get_buddy_index(int layer, int index) {
    return index ^ 1;
  }

  // Try to allocate a block at the given layer and address
  // Returns true if successful
  bool allocate_block(int layer, int addr) {
    int block_size = get_block_size(layer);

    // Check alignment
    if (addr % block_size != 0) return false;
    if (addr >= ram_size) return false;

    int index = addr_to_index(layer, addr);
    if (index >= num_blocks_per_layer[layer]) return false;

    // If block is already free at this layer, use it
    if (free_blocks[layer][index]) {
      free_blocks[layer][index] = false;
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
    int parent_index = addr_to_index(parent_layer, parent_addr);
    int child_index1 = parent_index * 2;
    int child_index2 = child_index1 + 1;

    free_blocks[layer][child_index1] = true;
    free_blocks[layer][child_index2] = true;

    // Now allocate the requested block
    if (free_blocks[layer][index]) {
      free_blocks[layer][index] = false;
      return true;
    }

    return false;
  }

  void merge_buddies(int layer, int addr) {
    if (layer >= num_layers - 1) return;

    int index = addr_to_index(layer, addr);
    int buddy_index = get_buddy_index(layer, index);

    if (buddy_index >= num_blocks_per_layer[layer]) return;

    // Check if buddy is free
    if (free_blocks[layer][buddy_index]) {
      // Merge with buddy
      free_blocks[layer][index] = false;
      free_blocks[layer][buddy_index] = false;

      int parent_index = index / 2;
      int parent_layer = layer + 1;
      int parent_addr = index_to_addr(parent_layer, parent_index);

      free_blocks[parent_layer][parent_index] = true;

      // Recursively try to merge parent
      merge_buddies(parent_layer, parent_addr);
    }
  }
};

} // namespace sjtu
