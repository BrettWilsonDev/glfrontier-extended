/*
 * This is used by the C output mode of the 68k assembler.
 * Loads 68k executable into m68k memory and applies all the relocations.
 * Executable is in an Atari .prg format.
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

#include "m68000.h"

// #define likely(x)       __builtin_expect((x),1)
// #define unlikely(x)     __builtin_expect((x),0)

#define LOAD_BASE 0x0
#define MEM_SIZE (0x110000)

union Reg
{
	u16 word[2];
	u32 _u32;
	u16 _u16;
	u8 _u8;
	s32 _s32;
	s16 _s16;
	s8 _s8;
};

/* m68000 state ----------------------------------------------------- */
extern s8 m68kram[MEM_SIZE];
extern union Reg Regs[16];

/* Status flags.
 * Using nZ (not zero) instead of Z saves operations in common cases.
 */
extern s32 N, nZ, V, C, X;
extern s32 bN, bnZ, bV, bC, bX;
extern s32 rdest; // Return address from interrupt (0 if none active)
extern s32 exceptions_pending;
extern s32 exceptions_pending_nums[32];
extern u32 exception_handlers[32];

void SetReg(int reg, int val);
int GetReg(int reg);

/* Macros for getting/setting flags */
#define GetZFlag() (!nZ)
#define GetNFlag() (N)
#define GetCFlag() (C)
#define GetVFlag() (V)
#define GetXFlag() (X)
#define SetZFlag(val) nZ = !(val)

void FlagException(int num);
void load_binfile(const char *bin_filename);

/* Optional bounds checking when debugging */
#ifdef M68K_DEBUG
#define BOUNDS_CHECK
#if 0
 static inline void BOUNDS_CHECK(u32 pos, int num)
 {
	 if ((pos + num) > MEM_SIZE) {
		 printf("Error. 68K memory access out of bounds (address $%x, line %d).\n", pos, line_no);
		 abort();
	 }
 }
#endif
#endif /* M68K_DEBUG */

/* ------------------ Memory Access Helpers (Endian-Safe) ------------------ */

static inline u32 do_get_mem_long(u32 *a)
{
#if defined(__i386__) || defined(__x86_64__)
	u32 val = *a;
	val = ((val & 0xFF000000) >> 24) | ((val & 0x00FF0000) >> 8) | ((val & 0x0000FF00) << 8) | ((val & 0x000000FF) << 24);
	return val;
#elif defined(LITTLE_ENDIAN)
	u8 *b = (u8 *)a;
	return ((u32)b[0] << 24) | ((u32)b[1] << 16) | ((u32)b[2] << 8) | b[3];
#else
	return *a;
#endif
}

static inline u16 do_get_mem_word(u16 *a)
{
#if defined(__i386__) || defined(__x86_64__)
	u16 val = *a;
	val = (val >> 8) | (val << 8);
	return val;
#elif defined(LITTLE_ENDIAN)
	u8 *b = (u8 *)a;
	return ((u16)b[0] << 8) | b[1];
#else
	return *a;
#endif
}

static inline u8 do_get_mem_byte(u8 *a)
{
	return *a;
}

static inline void do_put_mem_long(u32 *a, u32 v)
{
#if defined(__i386__) || defined(__x86_64__)
	v = (v >> 24) | ((v & 0x00FF0000) >> 8) | ((v & 0x0000FF00) << 8) | (v << 24);
	*a = v;
#elif defined(LITTLE_ENDIAN)
	u8 *b = (u8 *)a;
	b[0] = v >> 24;
	b[1] = v >> 16;
	b[2] = v >> 8;
	b[3] = v;
#else
	*a = v;
#endif
}

static inline void do_put_mem_word(u16 *a, u16 v)
{
#if defined(__i386__) || defined(__x86_64__)
	*a = (v >> 8) | (v << 8);
#elif defined(LITTLE_ENDIAN)
	u8 *b = (u8 *)a;
	b[0] = v >> 8;
	b[1] = v;
#else
	*a = v;
#endif
}

static inline void do_put_mem_byte(u8 *a, u8 v)
{
	*a = v;
}

/* ------------------ Read/Write Macros to 68K Memory ------------------ */

static inline s32 rdlong(u32 pos)
{
#ifdef M68K_DEBUG
	BOUNDS_CHECK(pos, 4);
#endif
	return do_get_mem_long((u32 *)(m68kram + pos));
}

static inline s16 rdword(u32 pos)
{
#ifdef M68K_DEBUG
	BOUNDS_CHECK(pos, 2);
#endif
	return do_get_mem_word((u16 *)(m68kram + pos));
}

static inline s8 rdbyte(u32 pos)
{
#ifdef M68K_DEBUG
	BOUNDS_CHECK(pos, 1);
#endif
	return do_get_mem_byte((u8 *)(m68kram + pos));
}

static inline void wrbyte(u32 pos, int val)
{
#ifdef M68K_DEBUG
	BOUNDS_CHECK(pos, 1);
#endif
	do_put_mem_byte((u8 *)(m68kram + pos), (u8)val);
}

static inline void wrword(u32 pos, int val)
{
#ifdef M68K_DEBUG
	BOUNDS_CHECK(pos, 2);
#endif
	do_put_mem_word((u16 *)(m68kram + pos), (u16)val);
}

static inline void wrlong(u32 pos, int val)
{
#ifdef M68K_DEBUG
	BOUNDS_CHECK(pos, 4);
#endif
	do_put_mem_long((u32 *)(m68kram + pos), (u32)val);
}

#ifdef M68K_DEBUG
void m68k_print_line_no();
#endif /* M68K_DEBUG */
