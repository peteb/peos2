#include "filesystem.h"
#include "support/pool.h"
#include "support/string.h"
#include "support/utils.h"
#include "screen.h"

struct vfs_name_node {
  p2::string<32> name;
  uint16_t vnode;
  uint16_t next_sibling = 0, head_child = 0, tail_child = 0;
};

struct vfs_file_info {
  // Data for a specific file
};

static p2::pool<vfs_name_node, 256> nodes{};
static p2::pool<vfs_file_info, 256> files{};
static uint16_t root_idx = 0;

static uint16_t vfs_create_child(uint16_t parent, const char *name, uint16_t vnode) {
  uint16_t new_node = nodes.push_back({name, vnode});

  if (!nodes[parent].tail_child) {
    // First child
    nodes[parent].head_child = new_node;
    nodes[parent].tail_child = new_node;
    return new_node;
  }
  else {
    uint16_t prev_tail = nodes[parent].tail_child;
    nodes[prev_tail].next_sibling = new_node;
    nodes[parent].tail_child = new_node;
  }

  return new_node;
}

static uint16_t vfs_lookup_node(uint16_t parent, const char *path) {
  // Empty string or / references the parent
  if (!*path || (path[0] == '/' && path[1] == '\0')) {
    return parent;
  }

  assert(path[0] == '/');

  // TODO: cleanup string handling using string_view or similar
  const char *seg_start = path + 1;
  const char *next_segment = strchr(seg_start, '/');
  if (!next_segment) {
    next_segment = strchr(path, '\0');
  }

  const p2::string<32> segment(seg_start, next_segment - seg_start);

  uint16_t idx = nodes[parent].head_child;
  while (idx) {
    vfs_name_node &node = nodes[idx];
    if (node.name == segment) {
      return vfs_lookup_node(idx, next_segment);
    }

    idx = node.next_sibling;
  }

  return 0;
}

void vfs_init() {
  root_idx = nodes.push_back({"", 0});
  assert(root_idx == 0);

  uint16_t dev_idx = vfs_create_child(root_idx, "dev", 5);
  vfs_create_child(root_idx, "etc", 1);
  vfs_create_child(root_idx, "test", 2);
  vfs_create_child(dev_idx, "term1", 3);
  vfs_create_child(dev_idx, "term2", 4);
}

static void vfs_print_siblings(uint16_t node, int level) {
  if (!node) {
    return;
  }

  while (node) {
    p2::string<128> prefix;
    for (int i = 0; i < level - 1; ++i) {
      prefix.append("   ");
    }

    if (level > 0) {
      if (!nodes[node].next_sibling) {
        prefix.append("\\--");
      }
      else {
        prefix.append("|--");
      }
    }


    print(prefix);
    puts(nodes[node].name);
    vfs_print_siblings(nodes[node].head_child, level + 1);

    node = nodes[node].next_sibling;
  }
}

void vfs_print() {
  puts("/");
  vfs_print_siblings(nodes[root_idx].head_child, 1);
}
