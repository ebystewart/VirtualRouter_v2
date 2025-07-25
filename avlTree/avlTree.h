#ifndef _AVL_TREE_H
#define _AVL_TREE_H

#include <stdint.h>
#include <stddef.h>

/*
 * The definition has been stolen from the Linux kernel.
 */
#ifdef __GNUC__
#  define bstree_container_of(node, type, member) ({			\
	const struct bstree_node *__mptr = (node);			\
	(type *)( (char *)__mptr - offsetof(type,member) );})
#  define rbtree_container_of(node, type, member) ({			\
	const struct rbtree_node *__mptr = (node);			\
	(type *)( (char *)__mptr - offsetof(type,member) );})
#  define avltree_container_of(node, type, member) ({			\
	const struct avltree_node *__mptr = (node);			\
	(type *)( (char *)__mptr - offsetof(type,member) );})
#  define splaytree_container_of(node, type, member) ({			\
	const struct splaytree_node *__mptr = (node);			\
	(type *)( (char *)__mptr - offsetof(type,member) );})
#else
#  define bstree_container_of(node, type, member)			\
	((type *)((char *)(node) - offsetof(type, member)))
#  define rbtree_container_of(node, type, member)			\
	((type *)((char *)(node) - offsetof(type, member)))
#  define avltree_container_of(node, type, member)			\
	((type *)((char *)(node) - offsetof(type, member)))
#  define splaytree_container_of(node, type, member)			\
	((type *)((char *)(node) - offsetof(type, member)))
#endif	/* __GNUC__ */

/*
 * Threaded binary search tree
 */
#ifdef UINTPTR_MAX

struct bstree_node {
	uintptr_t left, right;
} __attribute__((aligned(2)));

#else

struct bstree_node {
	struct bstree_node *left, *right;
	unsigned left_is_thread:1;
	unsigned right_is_thread:1;
};

#endif	/* UINTPTR_MAX */

typedef int (*bstree_cmp_fn_t)(const struct bstree_node *, const struct bstree_node *);

struct bstree {
	struct bstree_node *root;
	bstree_cmp_fn_t cmp_fn;
	struct bstree_node *first, *last;
	uint64_t reserved[4];
};

struct bstree_node *bstree_first(const struct bstree *tree);
struct bstree_node *bstree_last(const struct bstree *tree);
struct bstree_node *bstree_next(const struct bstree_node *node);
struct bstree_node *bstree_prev(const struct bstree_node *node);

struct bstree_node *bstree_lookup(const struct bstree_node *key, const struct bstree *tree);
struct bstree_node *bstree_insert(struct bstree_node *node, struct bstree *tree);
void bstree_remove(struct bstree_node *node, struct bstree *tree);
void bstree_replace(struct bstree_node *old, struct bstree_node *node, struct bstree *tree);
int bstree_init(struct bstree *tree, bstree_cmp_fn_t cmp, unsigned long flags);

/*
 * Red-black tree
 */
enum rb_color {
	RB_BLACK,
	RB_RED,
};

#ifdef UINTPTR_MAX

struct rbtree_node {
	struct rbtree_node *left, *right;
	uintptr_t parent;
} __attribute__((aligned(2)));

#else

struct rbtree_node {
	struct rbtree_node *left, *right;
	struct rbtree_node *parent;
	enum rb_color color;
};

#endif	/* UINTPTR_MAX */

typedef int (*rbtree_cmp_fn_t)(const struct rbtree_node *, const struct rbtree_node *);

struct rbtree {
	struct rbtree_node *root;
	rbtree_cmp_fn_t cmp_fn;
	struct rbtree_node *first, *last;
	uint64_t reserved[4];
};

struct rbtree_node *rbtree_first(const struct rbtree *tree);
struct rbtree_node *rbtree_last(const struct rbtree *tree);
struct rbtree_node *rbtree_next(const struct rbtree_node *node);
struct rbtree_node *rbtree_prev(const struct rbtree_node *node);

struct rbtree_node *rbtree_lookup(const struct rbtree_node *key, const struct rbtree *tree);
struct rbtree_node *rbtree_insert(struct rbtree_node *node, struct rbtree *tree);
void rbtree_remove(struct rbtree_node *node, struct rbtree *tree);
void rbtree_replace(struct rbtree_node *old, struct rbtree_node *node, struct rbtree *tree);
int rbtree_init(struct rbtree *tree, rbtree_cmp_fn_t cmp, unsigned long flags);

/*
 * AVL tree
 */
#if defined UINTPTR_MAX && UINTPTR_MAX == UINT64_MAX

struct avltree_node {
	struct avltree_node *left, *right;
	uintptr_t parent;		/* balance factor [0:4] */
} __attribute__((aligned(8)));

#else

struct avltree_node {
	struct avltree_node *left, *right;
	struct avltree_node *parent;
	signed balance:3;		/* balance factor [-2:+2] */
};

#endif
typedef struct avltree_node avltree_node_t;

typedef int (*avltree_cmp_fn_t)(const struct avltree_node *, const struct avltree_node *);

struct avltree {
	struct avltree_node *root;
	avltree_cmp_fn_t cmp_fn;
	int height;
	struct avltree_node *first, *last;
	uint64_t reserved[4];
};
typedef struct avltree avltree_t;

struct avltree_node *avltree_first(const struct avltree *tree);
struct avltree_node *avltree_last(const struct avltree *tree);
struct avltree_node *avltree_next(const struct avltree_node *node);
struct avltree_node *avltree_prev(const struct avltree_node *node);

struct avltree_node *avltree_lookup(const struct avltree_node *key, const struct avltree *tree);
struct avltree_node *avltree_insert(struct avltree_node *node, struct avltree *tree);
void avltree_remove(struct avltree_node *node, struct avltree *tree);
void avltree_replace(struct avltree_node *old, struct avltree_node *node, struct avltree *tree);
int avltree_init(struct avltree *tree, avltree_cmp_fn_t cmp);

static inline int avltree_is_empty(struct avltree *tree) {
	int rc = !avltree_first(tree) ? 1 : 0;
	return rc;
}

/* Iterator over the AVL tree (delete safe loop)*/
#define ITERATE_AVL_TREE_BEGIN(avltree_ptr, avltree_node_ptr)	\
{																\
	struct avltree_node *next_avltree_node_ptr = NULL;			\
	for(avltree_node_ptr = avltree_first(avltree_ptr); 			\
		avltree_node_ptr;										\
		avltree_node_ptr = next_avltree_node_ptr){				\
		next_avltree_node_ptr = avltree_next(avltree_node_ptr);

#define ITERATE_AVL_TREE_END	}}

/*
 * Splay tree
 */
#ifdef UINTPTR_MAX

struct splaytree_node {
	uintptr_t left, right;
} __attribute__((aligned(2)));

#else

struct splaytree_node {
	struct splaytree_node *left, *right;
	unsigned left_is_thread:1;
	unsigned right_is_thread:1;
};

#endif

typedef int (*splaytree_cmp_fn_t)(const struct splaytree_node *, const struct splaytree_node *);

struct splaytree {
	struct splaytree_node *root;
	struct splaytree_node *first, *last;
	splaytree_cmp_fn_t cmp_fn;
	uint64_t reserved[4];
};

struct splaytree_node *splaytree_first(const struct splaytree *tree);
struct splaytree_node *splaytree_last(const struct splaytree *tree);
struct splaytree_node *splaytree_next(const struct splaytree_node *node);
struct splaytree_node *splaytree_prev(const struct splaytree_node *node);

struct splaytree_node *splaytree_lookup(const struct splaytree_node *key, struct splaytree *tree);
struct splaytree_node *splaytree_insert( struct splaytree_node *node, struct splaytree *tree);
void splaytree_remove(struct splaytree_node *node, struct splaytree *tree);
void splaytree_replace(struct splaytree_node *old, struct splaytree_node *node, struct splaytree *tree);
int splaytree_init(struct splaytree *tree, splaytree_cmp_fn_t cmp, unsigned long flags);

#endif