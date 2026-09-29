/*
 * gl_scene.c - depth-sorted record/replay of emulator draw calls.
 * See gl_scene.h for the overview.
 */
#include <string.h>

#include "gl_scene.h"
#include "main.h"
#include "m68000.h"
#include "renderer.h"

#define SCENE_DATA_SIZE (1 << 20) /* bytes of records per frame */
#define SCENE_MAX_NODES 4096

typedef struct
{
	uint16_t op;
	uint16_t size; /* payload bytes, rounded up to 8 */
} RecHeader;

typedef struct
{
	uint32_t z;
	int less, more; /* child node indices, -1 = none */
	uint32_t start, end;
} ZNode;

static unsigned char data[SCENE_DATA_SIZE];
static uint32_t data_pos;

static ZNode nodes[SCENE_MAX_NODES];
static int n_nodes;
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
	nodes[i].less = nodes[i].more = -1;
	nodes[i].start = nodes[i].end = data_pos;
	return i;
}

bool scene_insert_node(uint32_t z)
{
	if (!scene_active() || node_locked)
		return false;

	close_current();
	int n = new_node(z);
	cur_node = n;
	if (n <= 0) /* first node is the root, or out of nodes */
		return n == 0;

	/* Iterative BST insert: larger z to `more`, ties to `less` */
	int i = 0;
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

void scene_draw(void)
{
	close_current();
	if (n_nodes == 0)
		return;

	/* In-order walk, `more` (farther) side first: painter's algorithm */
	static int stack[SCENE_MAX_NODES];
	int sp = 0;
	int i = 0;
	while (i >= 0 || sp > 0)
	{
		while (i >= 0)
		{
			stack[sp++] = i;
			i = nodes[i].more;
		}
		i = stack[--sp];
		draw_node(&nodes[i]);
		i = nodes[i].less;
	}
}
