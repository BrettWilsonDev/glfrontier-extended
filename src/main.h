/*
 * main.h - shared definitions for the host side of the emulator.
 * Originally based on Hatari's main.h (GPL v2 or later).
 *
 * Also included by the emulator core (host.h / fe2_modded.s.c).
 */
#ifndef MAIN_H
#define MAIN_H

#include <assert.h>
#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef int BOOL; /* kept for the older code; use bool in new code */
#ifndef FALSE
#define FALSE 0
#define TRUE  1
#endif

#define PROG_NAME           "Frontier: Elite 2" /* window title */
#define MAX_FILENAME_LENGTH 256

/* 68000 operand sizes */
#define SIZE_WORD 2
#define SIZE_LONG 4

/* 68000 registers, for GetReg() / SetReg() */
enum
{
	REG_D0,
	REG_D1,
	REG_D2,
	REG_D3,
	REG_D4,
	REG_D5,
	REG_D6,
	REG_D7,
	REG_A0,
	REG_A1,
	REG_A2,
	REG_A3,
	REG_A4,
	REG_A5,
	REG_A6,
	REG_A7, /* also the stack pointer */
};

/* The emulated screen: 320x200, one byte per pixel */
#define SCREEN_HEIGHT_HBL 200
#define SCREENBYTES_LINE  320

/* ---- main.c ------------------------------------------------------------- */
extern BOOL bQuitProgram;
extern BOOL bEmulationActive;
extern char szBootDiscImage[MAX_FILENAME_LENGTH];
extern char szWorkingDir[MAX_FILENAME_LENGTH];
extern char szCurrentDir[MAX_FILENAME_LENGTH];

extern bool toggle_touch_controls; /* on-screen touch controls */
extern bool toggle_m68k_menu;      /* emulator menu (Ctrl-F) */
extern bool toggle_fps_draw;
extern bool toggle_debug_draw; /* draw touch hit regions */
extern int dump_m68k_toggle;   /* request a RAM dump */
extern int emulation_speed;    /* ms per emulated VBL, 20 = original speed */

void Main_PauseEmulation(void);
void Main_UnPauseEmulation(void);
void Main_EventHandler(void);

/* Log to stdout (and logcat on Android); also kept in clslog for the
 * in-game console. */
void log_printf(const char *fmt, ...);
extern char *clslog;

#endif /* MAIN_H */
