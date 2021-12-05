#include "bztree.h"
#include "include/pmwcas.h"

namespace pmwcas {

struct BzPMDKMetadata *BzTree::get_metadata() {
  return D_RW(D_RW(POBJ_ROOT(pop, struct BzPMDKRootObj))->metadata);
}

template<typename T> void BzTree::desc_add_toid(Descriptor *desc, T *loc, T a, T b) {
  static_assert(sizeof(T) == 16);
  desc->AddEntry((uint64_t*)loc, *(uint64_t*)&a, *(uint64_t*)&b);
  desc->AddEntry(((uint64_t*)loc)+1, *(((uint64_t*)&a)+1), *(((uint64_t*)&b)+1));
}

TOID(struct Node) BzTree::find_leaf(const std::string key, bool perform_smo) {
  auto *md = get_metadata();
  auto [leaf, parent, idx] = find_leaf_parent(key, md);
  return leaf;
}

std::tuple<TOID(struct Node), std::optional<TOID(struct Node)>, uint16_t>
  BzTree::find_leaf_parent(const std::string key, bool perform_smo) {
  while (1) {
    auto v = find_leaf_parent_smo(key, perform_smo, get_metadata());
    if (v == std::nullopt) continue;
    return *v;
  }
}

bool BzTree::swap_node(std::optional<TOID(struct Node)> parent, uint64_t *node_off_ptr, TOID(struct Node) old_node, TOID(struct Node) new_node) {
  struct NodeHeaderStatusWord sw;
  if (parent.has_value()) {
    sw = D_RW(*parent)->header.status_word;
    if (sw.frozen) return false;
  }

  uint64_t old_offset = toid_get_offset(old_node);
  uint64_t new_offset = toid_get_offset(new_node);

  // retry if the node didn't become frozen, because that's recoverable
  // and returning false out of here is very expensive, it unfreezes the new node and re-traverses
  while (1) {
    auto *desc = desc_pool->AllocateDescriptor();
    assert(desc);
    if (parent.has_value()) desc->AddEntry((uint64_t*)&D_RW(*parent)->header.status_word, *(uint64_t*)&sw, *(uint64_t*)&sw);
    desc->AddEntry(node_off_ptr, old_offset, new_offset);
    if (desc->MwCAS()) return true;

    // failed, check if it is because it became frozen or if the current value changed
    if (*node_off_ptr != old_offset) return false;
    if (parent.has_value()) {
      sw = D_RW(*parent)->header.status_word;
      if (sw.frozen) return false;
    }

    // did not fail because of a permanent issue! retry
  }
}

void BzTree::toid_set_offset(TOID(struct Node) target, uint64_t off) {
  target.oid.off = off;
}
uint64_t BzTree::toid_get_offset(TOID(struct Node) target) {
  return target.oid.off;
}

std::optional<std::tuple<TOID(struct Node), std::optional<TOID(struct Node)>, uint16_t>>
    BzTree::find_leaf_parent_smo(const std::string key, bool perform_smo, struct BzPMDKMetadata *md) {
  // hack: we want the original toid of the metadata to be able to add it
  TOID(struct BzPMDKMetadata) md_oid;
  TOID_ASSIGN(md_oid, pmemobj_oid(md));

  // special case: does the root need SMO? if so, do them
  // todo(optimization): this is checked on nearly every operation, optimize this maybe? only check if root changes?
  if (perform_smo) {
    const struct NodeHeaderStatusWord *root_sw = &D_RO(md->root_node)->header.status_word;
    // root, of course, cannot be merged with a sibling (it has no siblings)
    bool root_compact = root_sw->delete_size > BZTREE_MAX_DELETED_SPACE;
    bool root_split = sizeof(struct Node) - sizeof(struct NodeHeader) - root_sw->block_size -
                        (root_sw->record_count*(sizeof(struct NodeMetadata))) < BZTREE_MIN_FREE_SPACE;

    // root split needs to be a special case because we modify height, so the root cannot be swapped with swap_node
    // todo(optimization): move root_compact out of here, it's needlessly complex (no new md needed, and with it, no
    // double pmwcas) and only attached to this for ease of implementation
    TOID(struct BzPMDKMetadata) md_new_oid;
    struct BzPMDKMetadata *md_new;
    if (root_compact || root_split) {
      // first, freeze the root node
      {
        struct NodeHeaderStatusWord sw_old = *root_sw, sw=*root_sw;
        sw.frozen = 1;

        auto *desc = desc_pool->AllocateDescriptor();
        assert(desc);
        desc->AddEntry((uint64_t*)root_sw, *(uint64_t*)&sw_old, *(uint64_t*)&sw);
        if (!desc->MwCAS()) return std::nullopt;
      }

      // create a new pmdk metadata object for the new root node
      POBJ_ZNEW(pop, &md_new_oid, struct BzPMDKMetadata);
      md_new = D_RW(md_new_oid);
      md_new->height = md->height;
      md_new->global_epoch = md->global_epoch;
    }

    // keep these for cleanup
    std::optional<std::pair<TOID(struct Node), TOID(struct Node)>> new_children = std::nullopt;

    // if both are needed, perform compact first, since it's possible splitting isn't needed after compaction
    // (whereas splitting will implicitly compact them, so the resulting ones might just get merged back next step)
    if (root_compact) md_new->root_node = node_compact(md->root_node);
    else if (root_split) {
        std::tie(md_new->root_node, new_children) = node_split(std::nullopt, md->root_node);
        md_new->height++;
    }

    // swap it into the pmem root data structure
    if (root_compact || root_split) {
      struct BzPMDKRootObj *root = D_RW(POBJ_ROOT(pop, struct BzPMDKRootObj));

      auto *desc = desc_pool->AllocateDescriptor();
      assert(desc);
      desc_add_toid(desc, &root->metadata, md_oid, md_new_oid);
      if (desc->MwCAS()) {
        // destroy old metadata and root
        assert(garbage.Push(md, BzTree::DestroyNode, nullptr).ok());
        assert(garbage.Push(D_RW(md->root_node), BzTree::DestroyNode, nullptr).ok());
      } else {
        // destroy new metadata and root and children, if any
        POBJ_FREE(md_new);
        POBJ_FREE(D_RW(md_new->root_node));
        if (new_children.has_value()) {
          POBJ_FREE(D_RW(new_children->first));
          POBJ_FREE(D_RW(new_children->second));
        }
      }
      // whether or not it worked, return nullopt to re-traverse
      return std::nullopt;
    }
  }

  // if there's only the root node we're done
  if (md->height == 1) return std::make_tuple(md->root_node, std::nullopt, 0);

  TOID(struct Node) child;
  uint64_t *child_off_ptr;
  TOID(struct Node) parent = md->root_node;
  // todo(cleanup): this is not a good way to abstract between updating the root_node offset and updating a regular node offset
  // we should keep the ".oid.off" hack isolated in toid_set_offset and toid_get_offset
  uint64_t *parent_off_ptr = &md->root_node.oid.off;
  std::optional<TOID(struct Node)> grandparent = std::nullopt;

  // perform height-1 dereferences to get to leaf
  uint64_t h=0;
  while (true) { // for (uint64_t h=0; h<md->height-1; h++) {
    // perform one dereference
    // the inner nodes do not have a full TOID!
    // the spec dictates that they are 8 bytes, which means we don't have enough space for pool id
    // the tradeoff is that this means inner nodes can hold more keys, but, bztrees cannot span pools
    // for us, we need to reconstitute the TOID from the parent's pool id and the offset in the node body
    struct NodeHeader *parent_header = &D_RW(parent)->header;
    const struct NodeHeaderStatusWord parent_sw = parent_header->status_word;

    // todo(optimization): binary search sorted keys, inner nodes are guaranteed to
    // be sorted because they are immutable
    uint16_t i;
    {
      const struct NodeMetadata *nmd = reinterpret_cast<const struct NodeMetadata*>(parent_header + 1);
      for (i=0; i<parent_sw.record_count; i++) {
        assert(nmd[i].total_len == nmd[i].key_len + 8); // optimization to use 8 for value len
        if (strcmp(&D_RO(parent)->body[nmd[i].offset], key.c_str()) >= 0) break;
      }
      child_off_ptr = (uint64_t*)&D_RW(parent)->body[nmd[i].offset + nmd[i].key_len];

      // dereference child (first set is to set pool id for first iteration)
      child = parent;
      toid_set_offset(child, *child_off_ptr);
    }

    // do SMOs on child if needed
    if (perform_smo) {
      struct NodeHeaderStatusWord *child_sw = &D_RW(child)->header.status_word;
      bool do_compact = child_sw->delete_size > BZTREE_MAX_DELETED_SPACE;
      size_t free_space = sizeof(struct Node) - sizeof(struct NodeHeader) - child_sw->block_size -
                            (child_sw->record_count*(sizeof(struct NodeMetadata)));
      bool do_split = free_space < BZTREE_MIN_FREE_SPACE;
      bool do_merge = free_space > BZTREE_MAX_FREE_SPACE;

      // compact takes priority because it may remove/add need to do splits or merges, and is implicitly done for them
      if (do_compact) {
        // opportunistically ensure parent is not frozen
        if (parent_sw.frozen) return std::nullopt;

        // first, freeze the node
        {
          struct NodeHeaderStatusWord sw_old = *child_sw, sw=*child_sw;
          sw.frozen = 1;

          auto *desc = desc_pool->AllocateDescriptor();
          assert(desc);
          desc->AddEntry((uint64_t*)child_sw, *(uint64_t*)&sw_old, *(uint64_t*)&sw);
          if (!desc->MwCAS()) return std::nullopt;
        }

        // perform the compaction
        TOID(struct Node) new_child = node_compact(child);

        // swap the new node in
        if (swap_node(parent, child_off_ptr, child, new_child)) {
          // success, delete the old node
          assert(garbage.Push((void*)D_RW(child), BzTree::DestroyNode, nullptr).ok());
        } else {
          // failure should happen only when the parent node freezes
          // we can just unfreeze the child node directly safely because only this thread could have frozen it
          // the adjacent fields of the struct are not going to be modified by any other thread while it is frozen
          child_sw->frozen = 0;

          POBJ_FREE(D_RW(new_child));
        }

        // whether or not it worked, return nullopt to re-traverse
        return std::nullopt;
      }

      if (do_split || do_merge) {
        // opportunistically ensure parent and grandparent are unfrozen
        if (parent_sw.frozen) return std::nullopt;
        if (grandparent.has_value() && D_RW(*grandparent)->header.status_word.frozen) return std::nullopt;

        // freeze the node and the parent (deviation from paper)
        struct NodeHeaderStatusWord sw_old = *child_sw, sw=*child_sw;
        sw.frozen = 1;

        struct NodeHeaderStatusWord parent_sw_new = parent_sw;
        parent_sw_new.frozen = 1;

        auto *desc = desc_pool->AllocateDescriptor();
        assert(desc);
        desc->AddEntry((uint64_t*)child_sw, *(uint64_t*)&sw_old, *(uint64_t*)&sw);
        desc->AddEntry((uint64_t*)&parent_header->status_word, *(uint64_t*)&parent_sw, *(uint64_t*)&parent_sw_new);
        if (!desc->MwCAS()) return std::nullopt;
      }

      if (do_split) {
        // perform the split
        auto [new_parent, new_children] = node_split(parent, child);

        // swap the new parent in
        if (swap_node(grandparent, parent_off_ptr, parent, new_parent)) {
          // success, delete the old nodes
          assert(garbage.Push((void*)D_RW(child), BzTree::DestroyNode, nullptr).ok());
          assert(garbage.Push((void*)D_RW(parent), BzTree::DestroyNode, nullptr).ok());
        } else {
          // failure should happen only when the grandparent node freezes
          // we can just unfreeze the nodes directly safely because only this thread could have frozen them, see above
          parent_header->status_word.frozen = 0;
          child_sw->frozen = 0;

          POBJ_FREE(D_RW(new_children.first));
          POBJ_FREE(D_RW(new_children.second));
          POBJ_FREE(D_RW(new_parent));
        }

        // whether or not it worked, return nullopt to re-traverse
        return std::nullopt;
      }

      if (do_merge) {
        // todo(req): identify which way to merge, left or right
        /*
        // perform the merge
        TOID(struct Node) new_parent = node_merge(parent, child);

        // swap the new parent in
        if (swap_node(grandparent, parent_off_ptr, parent, new_parent)) {
          // success, delete the old nodes
          if (new_children.has_value()) {
            assert(garbage.Push((void*)D_RW(new_children->first), BzTree::DestroyNode, nullptr).ok());
            assert(garbage.Push((void*)D_RW(new_children->second), BzTree::DestroyNode, nullptr).ok());
          }
          assert(garbage.Push((void*)D_RW(parent), BzTree::DestroyNode, nullptr).ok());
        } else {
          // failure should happen only when the grandparent node freezes
          // we can just unfreeze the nodes directly safely because only this thread could have frozen them, see above
          parent_header->status_word.frozen = 0;

          POBJ_FREE(D_RW(new_parent));
        }
        */
        // whether or not it worked, return nullopt to re-traverse
        return std::nullopt;
      }
    }

    // if we're at the end, then return
    if (++h >= md->height-1) return std::make_tuple(child, parent, i);

    // prepare next iteration
    grandparent = parent;
    parent_off_ptr = child_off_ptr;
    parent = child;
  }
}

}  // namespace pmwcas
