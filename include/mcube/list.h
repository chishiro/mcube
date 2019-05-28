/**
 * @file include/mcube/list.h
 *
 * @author Hiroyuki Chishiro
 */
#ifndef __MCUBE_MCUBE_LIST_H__
#define __MCUBE_MCUBE_LIST_H__
/*
 * Type-generic doubly-linked lists
 *
 * Copyright (C) 2010 Ahmed S. Darwish <darwish.07@gmail.com>
 *
 * This API is very close to the Linux-2.6 one, which I've used back in
 * the days and found it to be very flexible. The code itself was
 * "cleanroomed" from Bovet & Cesati's `Understanding the Linux Kernel'
 * book to avoid copyrights mismatch.
 *
 * A good feature of this API is letting us allocate and deallocate both
 * the structure _and_ its linked-list pointers in one-shot. This avoids
 * unnecessary fragmentation in kmalloc() buffers, lessens the chance of
 * memory leaks, and improves locality of reference.
 *
 * Such lists typically look like this:
 *
 *
 *     .--------------------------------------------------------.
 *     |                                                        |
 *     |            struct A        struct B        struct C    |
 *     |           ..........      ..........      ..........   |
 *     |           .        .      .        .      .        .   |
 *     v           .        .      .        .      .        .   |
 *    ---          .  ---   .      .  ---   .      .  ---   .   |
 *    |@| --------->  |@| --------->  |@| --------->  |@| ------.
 *    | |          .  | |   .      .  | |   .      .  | |   .
 *    |*|  <--------- |*|  <--------- |*|  <--------- |*|  <----.
 *    ---          .  ---   .      .  ---   .      .  ---   .   |
 *    `H'          .  `n'   .      .  `n'   .      .  `n'   .   |
 *     |           ..........      ..........      ..........   |
 *     |                                                        |
 *     .--------------------------------------------------------.
 *
 *
 * where 'H' and 'n' are list_node structures, and 'H' is the list's
 * head. '@' is a node's next pointer, while '*' is the same node's
 * prev pointer. All of the next and prev pointers point to _other_
 * list_node objects, not to the super-objects A, B, or C.
 *
 * Check the test-cases for usage examples.
 */

#ifndef __ASSEMBLY__

/*
 * Doubly-linked list node
 */
struct list_node {
  struct list_node *next;
  struct list_node *prev;
};

/*
 * Static init, for inside-structure nodes
 */
#define LIST_INIT(n)                            \
  {                                             \
    .next = &(n),                               \
      .prev = &(n),                             \
  }

/*
 * Global declaration with a static init
 */
#define LIST_NODE(n)                            \
  struct list_node n = LIST_INIT(n)

/*
 * Dynamic init, for run-time
 */
static inline void list_init(struct list_node *node)
{
  node->next = node;
  node->prev = node;
}

/*
 * Is this node connected with any neighbours?
 */
static inline bool list_empty(const struct list_node *node)
{
  if (node->next == node) {
    assert(node->prev == node);
    return true;
  }

  assert(node->prev != node);
  return false;
}

/*
 * Insert @new right after @node
 */
static inline void list_add(struct list_node *node, struct list_node *new)
{
  new->next = node->next;
  new->next->prev = new;

  node->next = new;
  new->prev = node;
}

/*
 * Insert @new right before @node
 */
static inline void list_add_tail(struct list_node *node, struct list_node *new)
{
  new->prev = node->prev;
  new->prev->next = new;

  node->prev = new;
  new->next = node;
}

/*
 * Return the address of the data structure of type @type
 * that includes given @node. @node_name is the node's
 * name inside that structure declaration.
 *
 * The "useless" pointer assignment is for type-checking.
 * `Make it hard to misuse' -- a golden APIs advice.
 */
#define list_entry(node, type, node_name)       \
  ({                                            \
    size_t offset;                              \
    __unused struct list_node *m;               \
                                                \
    m = (node);                                 \
                                                \
    offset = offsetof(type, node_name);         \
    (type *)((uint8_t *)(node) - offset);       \
  })

/*
 * Scan the list, beginning from @node, using the iterator
 * @struc. @struc is of type pointer to the structure
 * containing @node. @name is the node's name inside that
 * structure (the one containing @node) declaration.
 *
 * NOTE! Don't delete the the iterator's list node inside
 * loop: we use it in the initialization of next iteration.
 */
#define list_for_each(node, struc, name)                            \
  for (struc = list_entry((node)->next, typeof(*struc), name);      \
       &(struc->name) != (node);                                    \
       struc = list_entry(struc->name.next, typeof(*struc), name))

/*
 * Same as list_for_each(), but with making it safe to
 * delete the iterator's list node inside the loop. This
 * is useful for popping-up list elements as you go.
 *
 * You'll need to give us a spare iterator for this.
 */
#define list_for_each_safe(node, struc, spare_struc, name)              \
  for (struc = list_entry((node)->next, typeof(*struc), name),          \
         spare_struc = list_entry(struc->name.next, typeof(*struc), name); \
       &(struc->name) != (node);                                        \
       struc = spare_struc,                                             \
         spare_struc = list_entry(struc->name.next, typeof(*struc), name))

/*
 * Pop @node out of its connected neighbours.
 */
static inline void list_del(struct list_node *node)
{
  struct list_node *prevn;
  struct list_node *nextn;

  prevn = node->prev;
  nextn = node->next;

  assert(prevn);
  assert(nextn);
  assert(prevn->next == node);
  assert(nextn->prev == node);

  prevn->next = node->next;
  nextn->prev = node->prev;

  node->next = node;
  node->prev = node;
}



#endif /* !__ASSEMBLY__ */

#endif /* __MCUBE_MCUBE_LIST_H__ */

