/*
 * gl_scene.c - depth-sorted record/replay of emulator draw calls.
 * See gl_scene.h for the overview.
 */
#include <string.h>

#include "gl_scene.h"
#include "game_state.h"
#include "host.h"
#include "main.h"
#include "m68000.h"
#include "renderer.h"

#define SCENE_DATA_SIZE (1 << 20) /* bytes of records per frame */
#define SCENE_MAX_NODES 4096

/* The game's own depth tree nodes: z, two child pointers, then the opcodes.
 * A group node (L3a4b4) has only the group opcode, followed by the root of
 * its nested tree. See scene_insert_game_node. */
#define GAME_NODE_FIRST_OP   12
#define GAME_NODE_GROUP_ROOT 14
#define GAME_OP_GROUP        0xff96

typedef struct
{
	uint16_t op;
	uint16_t size; /* payload bytes, rounded up to 8 */
} RecHeader;

typedef struct
{
	uint32_t z;
	int less, more;     /* child node indices, -1 = none */
	int group;          /* root of this node's nested tree, -1 = none */
	uint32_t game_node; /* the game's matching tree node, 0 = none */
	uint32_t start, end;
} ZNode;

static unsigned char data[SCENE_DATA_SIZE];
static uint32_t data_pos;

static ZNode nodes[SCENE_MAX_NODES];
static int n_nodes;
static int root_node = -1;
/* The group the last node went into, to avoid searching for it every time */
static uint32_t group_root_addr;
static int group_owner = -1;
static int cur_node = -1; /* -1: no node, records are dropped */
static bool node_locked;

typedef void (*PrimDrawFn)(const void *);
static const PrimDrawFn draw_table[PRIM_COUNT] = {NULL,
#define X(name, fn) fn,
												  GL_PRIMITIVES(X)
#undef X
};

/* =========================================================================
 * Helpers shared by primitive files
 * ========================================================================= */
Vec3i m68k_vertex(uint32_t addr)
{
	Vec3i v;
	v.x = STMemory_ReadLong(addr);
	v.y = STMemory_ReadLong(addr + 4);
	v.z = -STMemory_ReadLong(addr + 8);
	return v;
}

Rgb8 rgb444_to_rgb8(int rgb)
{
	Rgb8 c;
	c.r = (uint8_t)((rgb & 0xf00) >> 4);
	c.g = (uint8_t)(rgb & 0xf0);
	c.b = (uint8_t)((rgb & 0xf) << 4);
	c.a = 255;
	return c;
}

bool scene_active(void)
{
	return use_renderer != R_OLD;
}

/* =========================================================================
 * Recording
 * ========================================================================= */
void scene_reset(void)
{
	data_pos = 0;
	n_nodes = 0;
	root_node = -1;
	group_root_addr = 0;
	group_owner = -1;
	cur_node = -1;
	node_locked = false;
}

static void close_current(void)
{
	if (cur_node >= 0)
		nodes[cur_node].end = data_pos;
}

static int new_node(uint32_t z)
{
	if (n_nodes >= SCENE_MAX_NODES)
		return -1;
	int i = n_nodes++;
	nodes[i].z = z;
	nodes[i].less = nodes[i].more = nodes[i].group = -1;
	nodes[i].game_node = 0;
	nodes[i].start = nodes[i].end = data_pos;
	return i;
}

/* Starts a node in the tree at *root. */
static bool insert_node(uint32_t z, int *root, uint32_t game_node)
{
	if (!scene_active() || node_locked)
		return false;

	close_current();
	int n = new_node(z);
	cur_node = n;
	if (n < 0) /* out of nodes */
		return false;
	nodes[n].game_node = game_node;
	if (*root < 0)
	{
		*root = n;
		return true;
	}

	/* Iterative BST insert: larger z to `more`, ties to `less` */
	int i = *root;
	for (;;)
	{
		int *next = (z > nodes[i].z) ? &nodes[i].more : &nodes[i].less;
		if (*next < 0)
		{
			*next = n;
			return true;
		}
		i = *next;
	}
}

bool scene_insert_node(uint32_t z)
{
	return insert_node(z, &root_node, 0);
}

/* The tree the game is inserting into right now. Normally that is the whole
 * scene, but a model can open a group (L3a4b4): a node with a nested tree of
 * its own, which the game's L385d0_3dview_thing2 then points to until the
 * model closes it. The group is sorted into the scene as one item and its
 * nodes only against each other, which is how the flat ground of a city keeps
 * its roads on top: their depths alone would put them behind the ground. */
static int *current_tree(void)
{
	uint32_t root = (uint32_t)STMemory_ReadLong(FE2_3dview_thing2);
	if (root == (uint32_t)STMemory_ReadLong(FE2_primitives_base))
		return &root_node;
	if (root != group_root_addr)
	{
		group_root_addr = root;
		group_owner = -1;
		for (int i = n_nodes - 1; i >= 0 && group_owner < 0; i--)
			if (nodes[i].game_node + GAME_NODE_GROUP_ROOT == root)
				group_owner = i;
	}
	/* Not found if the game opened it while a complex polygon held the node
	 * lock, so GL never got the group's node. */
	return group_owner >= 0 ? &nodes[group_owner].group : &root_node;
}

bool scene_insert_game_node(uint32_t z, uint32_t game_node)
{
	return insert_node(z, current_tree(), game_node);
}

void scene_lock_node(bool lock)
{
	node_locked = lock;
}

void *scene_record(enum PrimOp op, size_t size)
{
	if (!scene_active() || cur_node < 0)
		return NULL;

	size_t padded = (size + 7) & ~(size_t)7;
	if (data_pos + sizeof(RecHeader) + padded > SCENE_DATA_SIZE || padded > 0xffff)
	{
		static bool warned;
		if (!warned)
			log_printf("gl_scene: frame record buffer full, dropping geometry\n");
		warned = true;
		return NULL;
	}

	RecHeader *h = (RecHeader *)(data + data_pos);
	h->op = (uint16_t)op;
	h->size = (uint16_t)padded;
	data_pos += sizeof(RecHeader);
	void *payload = data + data_pos;
	memset(payload, 0, padded);
	data_pos += (uint32_t)padded;
	nodes[cur_node].end = data_pos;
	return payload;
}

/* =========================================================================
 * Replay
 * ========================================================================= */
static void draw_node(const ZNode *n)
{
	uint32_t pos = n->start;
	while (pos + sizeof(RecHeader) <= n->end)
	{
		const RecHeader *h = (const RecHeader *)(data + pos);
		pos += sizeof(RecHeader);
		if (h->op > PRIM_NONE && h->op < PRIM_COUNT)
			draw_table[h->op](data + pos);
		pos += h->size;
	}
}

static bool is_group_node(const ZNode *n)
{
	if (n->game_node == 0 || n->game_node + GAME_NODE_FIRST_OP + 2 > MEM_SIZE)
		return false;
	return (uint16_t)STMemory_ReadWord(n->game_node + GAME_NODE_FIRST_OP) == GAME_OP_GROUP;
}

/* In-order walk, `more` (farther) side first: painter's algorithm. A group
 * draws its nested tree and nothing else: anything the game adds between
 * opening a group and its first node is never reached by L3866e_Draw3DView. */
static void draw_tree(int i)
{
	/* every node is on the stack at most once, nested walks included */
	static int stack[SCENE_MAX_NODES];
	static int sp;
	int base = sp;
	while (i >= 0 || sp > base)
	{
		while (i >= 0)
		{
			stack[sp++] = i;
			i = nodes[i].more;
		}
		i = stack[--sp];
		if (is_group_node(&nodes[i]))
			draw_tree(nodes[i].group);
		else
			draw_node(&nodes[i]);
		i = nodes[i].less;
	}
}

void scene_draw(void)
{
	close_current();
	draw_tree(root_node);
}
