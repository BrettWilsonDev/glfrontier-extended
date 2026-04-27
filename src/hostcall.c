#include <time.h>
#include <ctype.h>
#include <SDL.h>
#include <SDL_endian.h>

#include <stdio.h>

#include "main.h"
#include "../host.h"
#include "hostcall.h"
#include "renderer.h"
#include "audio.h"
#include "input.h"
#include "keymap.h"
#include "shortcut.h"

#include <stdint.h>

/*
  GEMDOS error codes, See 'The Atari Compendium' D.3
  Call_Fread, Fwrite, etc should return these.
*/
#define GEMDOS_EOK 0	   // OK
#define GEMDOS_ERROR -1	   // Generic error
#define GEMDOS_EDRVNR -2   // Drive not ready
#define GEMDOS_EUNCMD -3   // Unknown command
#define GEMDOS_E_CRC -4	   // CRC error
#define GEMDOS_EBADRQ -5   // Bad request
#define GEMDOS_E_SEEK -6   // Seek error
#define GEMDOS_EMEDIA -7   // Unknown media
#define GEMDOS_ESECNF -8   // Sector not found
#define GEMDOS_EPAPER -9   // Out of paper
#define GEMDOS_EWRITF -10  // Write fault
#define GEMDOS_EREADF -11  // Read fault
#define GEMDOS_EWRPRO -12  // Device is write protected
#define GEMDOS_E_CHNG -14  // Media change detected
#define GEMDOS_EUNDEV -15  // Unknown device
#define GEMDOS_EINVFN -32  // Invalid function
#define GEMDOS_EFILNF -33  // File not found
#define GEMDOS_EPTHNF -34  // Path not found
#define GEMDOS_ENHNDL -35  // No more handles
#define GEMDOS_EACCDN -36  // Access denied
#define GEMDOS_EIHNDL -37  // Invalid handle
#define GEMDOS_ENSMEM -39  // Insufficient memory
#define GEMDOS_EIMBA -40   // Invalid memory block address
#define GEMDOS_EDRIVE -46  // Invalid drive specification
#define GEMDOS_ENSAME -48  // Cross device rename
#define GEMDOS_ENMFIL -49  // No more files
#define GEMDOS_ELOCKED -58 // Record is already locked
#define GEMDOS_ENSLOCK -59 // Invalid lock removal request
#define GEMDOS_ERANGE -64  // Range error
#define GEMDOS_EINTRN -65  // Internal error
#define GEMDOS_EPLFMT -66  // Invalid program load format
#define GEMDOS_EGSBF -67   // Memory block growth failure
#define GEMDOS_ELOOP -80   // Too many symbolic links
#define GEMDOS_EMOUNT -200 // Mount point crossed (indicator)

void Call_Memset()
{
	// int adr, count;
	int32_t adr, count;
	// unsigned long Params;
	uint32_t Params;

	Params = GetReg(REG_A7);
	Params -= SIZE_WORD;

	count = STMemory_ReadLong(Params + SIZE_WORD);
	adr = STMemory_ReadLong(Params + SIZE_WORD + SIZE_LONG);

	if (use_renderer == R_OLD)
	{
		memset(STRam + adr, 0, count);
	}
	else
	{
		memset(STRam + adr, 255, count);
	}
	fe2_bgcol = 0;
}

void Call_MemsetBlue()
{
	int adr, count;
	unsigned long Params;

	Params = GetReg(REG_A7);
	Params -= SIZE_WORD;

	count = STMemory_ReadLong(Params + SIZE_WORD);
	adr = STMemory_ReadLong(Params + SIZE_WORD + SIZE_LONG);
	if (use_renderer == R_OLD)
	{
		memset(STRam + adr, 0xe, count);
	}
	else
	{
		memset(STRam + adr, 255, count);
	}
	fe2_bgcol = 0xe;
}

void Call_Memcpy()
{
	int dest, src, count;
	unsigned long Params;

	Params = GetReg(REG_A7);
	Params -= SIZE_WORD;

	dest = STMemory_ReadLong(Params + SIZE_WORD);
	src = STMemory_ReadLong(Params + SIZE_WORD + SIZE_LONG);
	count = STMemory_ReadLong(Params + SIZE_WORD + 2 * SIZE_LONG);

	memcpy(STRam + dest, STRam + src, count);
}

static const char mouse_bmp[256] = {
	0, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	0, 15, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	0, 15, 15, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	0, 15, 15, 15, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	0, 15, 15, 15, 15, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	0, 15, 15, 15, 15, 15, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	0, 15, 15, 15, 15, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	0, 15, 15, 15, 15, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	0, 15, 0, 15, 15, 15, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	0, 0, -1, 0, 15, 15, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, 0, 15, 15, 15, 0, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, 0, 15, 15, 0, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, 0, 15, 15, 15, 0, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, 0, 15, 0, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};

Uint8 under_mouse[256];

void Call_BlitCursor()
{
#if 0
	int x, y, adr, org_x, org_y;
	Uint8 *pixbase, *pix;
	const char *bmp;
	Uint8 *save;
	unsigned long Params;
	
	Params = GetReg (REG_A7);
	Params -= SIZE_WORD;
	
	org_x = STMemory_ReadLong (Params+SIZE_WORD);
	org_y = STMemory_ReadLong (Params+SIZE_WORD+SIZE_LONG);
	adr = STMemory_ReadLong (Params+SIZE_WORD+2*SIZE_LONG);

	pixbase = STRam + adr + (org_y*SCREENBYTES_LINE) + org_x;
	bmp = mouse_bmp;
	save = under_mouse;

	for (y=0; y<16; y++) {
		if (y + org_y > SCREEN_HEIGHT_HBL) break;
		pix = pixbase;
		pixbase += SCREENBYTES_LINE;
		for (x=0; x<16; x++, pix++, bmp++, save++) {
			if (x+org_x >= SCREENBYTES_LINE) continue;
			*save = *pix;
			if (*bmp != -1) *pix = *bmp;
		}
	}
#endif /* 0 */
	/* in screen.h */
	mouse_shown = 1;
}

void Call_RestoreUnderCursor()
{
#if 0
	int x, y, adr, org_x, org_y;
	Uint8 *pixbase, *pix;
	char *bmp;
	unsigned long Params;
	
	Params = GetReg (REG_A7);
	Params -= SIZE_WORD;
	
	org_x = STMemory_ReadLong (Params+SIZE_WORD);
	org_y = STMemory_ReadLong (Params+SIZE_WORD+SIZE_LONG);
	adr = STMemory_ReadLong (Params+SIZE_WORD+2*SIZE_LONG);

	pixbase = STRam + adr + (org_y*SCREENBYTES_LINE) + org_x;
	bmp = under_mouse;

	for (y=0; y<16; y++) {
		if (y + org_y > SCREEN_HEIGHT_HBL) break;
		pix = pixbase;
		pixbase += SCREENBYTES_LINE;
		for (x=0; x<16; x++, pix++, bmp++) {
			if (x+org_x >= SCREENBYTES_LINE) continue;
			if (*bmp != -1) *pix = *bmp;
		}
	}
#endif /* 0 */
}

void Call_PutPix()
{
	int col, org_x, scr;
	char *pix;
	unsigned long Params;

	Params = GetReg(REG_A7);
	Params -= SIZE_WORD;

	col = STMemory_ReadWord(Params + SIZE_WORD) >> 2;
	org_x = (unsigned short)GetReg(REG_D4);
	scr = GetReg(REG_A3);

	/* hack to fix screen line. frontier's logic still thinks
	 * there are 160 bytes per line */
	/* which screen buffer it is based on */
	if (scr & 0x100000)
	{
		scr -= 0x100000;
		scr *= 2;
		scr += 0x100000;
	}
	else
	{
		scr -= 0xf0000;
		scr *= 2;
		scr += 0xf0000;
	}
	pix = (char *)STRam + scr + org_x;
	*pix = col;
	return;
}

void Call_FillLine()
{
	int org_x, len, scr, col;
	char *pix;
	unsigned long Params;

	Params = GetReg(REG_A7);
	Params -= SIZE_WORD;

	col = STMemory_ReadWord(Params + SIZE_WORD) >> 2;
	org_x = (unsigned short)GetReg(REG_D4);
	len = (~GetReg(REG_D5)) & 0xffff;
	scr = GetReg(REG_A3);

	/* hack to fix screen line. frontier's logic still thinks
	 * there are 160 bytes per line */
	/* which screen buffer it is based on */
	if (scr & 0x100000)
	{
		scr -= 0x100000;
		scr *= 2;
		scr += 0x100000;
	}
	else
	{
		scr -= 0xf0000;
		scr *= 2;
		scr += 0xf0000;
	}
	pix = (char *)STRam + scr;
	org_x = SCREENBYTES_LINE;
	while (org_x--)
	{
		*pix = col;
		pix++;
	}
}

/*
 * This is used by the scanner code to draw object stalks
 * which are below the plane of the scanner.
 * The mask d7 indicates which pixels in the plane to set,
 * and they are set if their current colour is zero.
 *
 * This implementation isn't the way the function is really
 * supposed to be implemented (colour and draw mask was
 * combined in d6 but the colour mask is wrong now for
 * non-planar screen).
 */
void Call_BackHLine()
{
	int i, scr, col, bitfield;
	char *pix;
	unsigned long Params;

	Params = GetReg(REG_A7);
	Params -= SIZE_WORD;

	col = STMemory_ReadWord(Params + SIZE_WORD) >> 2;
	scr = GetReg(REG_A3);
	bitfield = (unsigned short)GetReg(REG_D7);

	/* hack to fix screen line. frontier's logic still thinks
	 * there are 160 bytes per line */
	/* which screen buffer it is based on */
	if (scr & 0x100000)
	{
		scr -= 0x100000;
		scr *= 2;
		scr += 0x100000;
	}
	else
	{
		scr -= 0xf0000;
		scr *= 2;
		scr += 0xf0000;
	}
	pix = STRam + scr;
	for (i = 15; i >= 0; i--)
	{
		if ((bitfield & (1 << i)) && (*pix == 0))
			*pix = col;
		pix++;
	}
}

void Call_OldHLine()
{
	int org_x, len, scr, col;
	char *pix;
	unsigned long Params;

	Params = GetReg(REG_A7);
	Params -= SIZE_WORD;

	col = STMemory_ReadWord(Params + SIZE_WORD) >> 2;
	// printf ("col=%d, d4=%d, (idx) d5=%d, (scr_line) a3=%p\n", col, Regs[REG_D4]&0xffff, Regs[REG_D5]&0xffff, (void*)Regs[REG_A3]);
	org_x = (unsigned short)GetReg(REG_D4);
	len = (unsigned short)GetReg(REG_D5);
	scr = GetReg(REG_A3);

	/* hack to fix screen line. frontier's logic still thinks
	 * there are 160 bytes per line */
	/* which screen buffer it is based on */
	if (scr & 0x100000)
	{
		scr -= 0x100000;
		scr *= 2;
		scr += 0x100000;
	}
	else
	{
		scr -= 0xf0000;
		scr *= 2;
		scr += 0xf0000;
	}
	len = len / 2;
	/* horizontal line */
	pix = STRam + scr + org_x;
	while (len--)
	{
		*pix = col;
		pix++;
	}
}

void Call_HLine()
{
	int org_x, len, scr, col;
	char *pix;

	col = (GetReg(REG_D1) & 0xffff) >> 2;
	org_x = GetReg(REG_D4) & 0xffff;
	len = GetReg(REG_D5) & 0xffff;
	scr = GetReg(REG_A3);

	/* horizontal line */
	pix = STRam + scr + org_x;
	while (len--)
	{
		*pix = col;
		pix++;
	}
}

/*
 * Blits frontier format 4-plane thingy
 */
void Call_BlitBmp()
{
	int width, height, org_x, org_y, bmp, scr;
	char *bmp_pix, *scr_pix, *ybase;
	int xpoo, i, ypoo, plane_incr;

	short word0, word1, word2, word3;
	unsigned long Params;

	Params = GetReg(REG_A7);
	Params -= SIZE_WORD;

	width = STMemory_ReadWord(Params + SIZE_WORD);
	height = STMemory_ReadWord(Params + SIZE_WORD * 2);
	org_x = STMemory_ReadWord(Params + SIZE_WORD * 3);
	org_y = STMemory_ReadWord(Params + SIZE_WORD * 4);
	bmp = STMemory_ReadLong(Params + SIZE_WORD * 5);
	scr = STMemory_ReadLong(Params + SIZE_WORD * 5 + SIZE_LONG);

	/* width is in words (width/16) */
	// printf ("Blit %dx%d to %d,%d, bmp 0x%x, scr 0x%x.\n", width, height, org_x, org_y, bmp, scr);
	bmp_pix = STRam + bmp + 4;
	ybase = STRam + scr + (org_y * SCREENBYTES_LINE) + org_x;

	/* These checks were in the original blit routine */
	if (org_x < 0)
		return;
	if (org_y < 0)
		return;
	if (height > 200)
		return;
	if (width > 320)
		return;

	plane_incr = 2 * height * width;

	ypoo = height;
	while (ypoo--)
	{
		scr_pix = (char *)ybase;
		ybase += SCREENBYTES_LINE;
		for (xpoo = width; xpoo; xpoo--)
		{
			word0 = SDL_SwapBE16(*((short *)bmp_pix));
			bmp_pix += plane_incr;
			word1 = SDL_SwapBE16(*((short *)bmp_pix));
			bmp_pix += plane_incr;
			word2 = SDL_SwapBE16(*((short *)bmp_pix));
			bmp_pix += plane_incr;
			word3 = SDL_SwapBE16(*((short *)bmp_pix));

			for (i = 0; i < 16; i++)
			{
				*scr_pix = (word0 >> (15 - i)) & 0x1;
				*scr_pix |= ((word1 >> (15 - i)) & 0x1) << 1;
				*scr_pix |= ((word2 >> (15 - i)) & 0x1) << 2;
				*scr_pix |= ((word3 >> (15 - i)) & 0x1) << 3;
				scr_pix++;
			}
			bmp_pix -= 3 * plane_incr;
			bmp_pix += 2;
		}
	}
#if 0
	glDisable (GL_DEPTH_TEST);
	glMatrixMode (GL_PROJECTION);
	glPushMatrix ();
	glLoadIdentity ();
	glOrtho (0, 320, 0, 200, -1, 1);

	glMatrixMode (GL_MODELVIEW);
	glPushMatrix ();
	glLoadIdentity ();

	printf ("%d,%d %dx%d\n", org_x, org_y, width*16, height);
	glTranslated (org_x, 0, 0);

	glBegin (GL_TRIANGLE_STRIP);
		glColor3f (0.0f, 0.0f, 1.0f);
		glVertex2i (0, 0);
		glVertex2i (width*16, 0);
		glVertex2i (0, height);
		glVertex2i (width*16, height);
	glEnd ();

	glMatrixMode (GL_PROJECTION);
	glPopMatrix ();
	glMatrixMode (GL_MODELVIEW);
	glPopMatrix ();
	glEnable (GL_DEPTH_TEST);
#endif
}

#define SCR_W 320

void Call_DrawStrShadowed()
{
	unsigned char *str;

	str = (unsigned char *)(GetReg(REG_A0) + STRam);

	SetReg(REG_D1, DrawStr(
					   GetReg(REG_D1), GetReg(REG_D2),
					   GetReg(REG_D0), str, TRUE));
}

void Call_DrawStr()
{
	unsigned char *str;

	str = (unsigned char *)(GetReg(REG_A0) + STRam);

	SetReg(REG_D1, DrawStr(
					   GetReg(REG_D1), GetReg(REG_D2),
					   GetReg(REG_D0), str, FALSE));
}

void Call_SetMainPalette()
{
	Uint32 pal_ptr;
	int i;
	unsigned long Params;

	Params = GetReg(REG_A7);
	Params -= SIZE_WORD;

	pal_ptr = STMemory_ReadLong(Params + SIZE_WORD);

	for (i = 0; i < 16; i++)
	{
		MainPalette[i] = STMemory_ReadWord(pal_ptr);
		// printf ("%hx ", MainPalette[i]);
		pal_ptr += 2;
	}
	// printf ("\n");
}

void Call_SetCtrlPalette()
{
	Uint32 pal_ptr;
	int i;
	unsigned long Params;

	Params = GetReg(REG_A7);
	Params -= SIZE_WORD;

	pal_ptr = STMemory_ReadLong(Params + SIZE_WORD);

	for (i = 0; i < 16; i++)
	{
		CtrlPalette[i] = STMemory_ReadWord(pal_ptr);
		pal_ptr += 2;
	}
}

int len_working_ext_pal;
unsigned short working_ext_pal[240];

void Call_InformScreens()
{
	unsigned long Params;

	Params = GetReg(REG_A7);
	Params -= SIZE_WORD;

	physcreen2 = STMemory_ReadLong(Params + SIZE_WORD);
	logscreen2 = STMemory_ReadLong(Params + SIZE_WORD + SIZE_LONG);
	physcreen = STMemory_ReadLong(Params + SIZE_WORD + 2 * SIZE_LONG);
	logscreen = STMemory_ReadLong(Params + SIZE_WORD + 3 * SIZE_LONG);
}

/* also copies the extended palette into main palette */
void Call_SetScreenBase()
{
	int i;
	unsigned long Params;

	Params = GetReg(REG_A7);
	Params -= SIZE_WORD;

	VideoBase = STMemory_ReadLong(Params + SIZE_WORD);
	// VideoRaster = STRam + VideoBase;
	VideoRaster = (unsigned char *)STRam + VideoBase;

	for (i = 0; i < len_working_ext_pal; i++)
	{
		MainPalette[16 + i] = working_ext_pal[i];
	}
	len_main_palette = 16 + len_working_ext_pal;
}

void Call_MakeExtPalette()
{
	int col_list, len, col_idx, col_val, i;
	unsigned long Params;

	Params = GetReg(REG_A7);
	Params -= SIZE_WORD;

	col_list = STMemory_ReadLong(Params + SIZE_WORD);

	len = STMemory_ReadWord(col_list) >> 2;
	len_working_ext_pal = len;
	col_list += 2;
	// printf ("%d colours.\n", len+2);
	for (i = 0; i < len; i++)
	{
		col_val = STMemory_ReadWord(col_list);
		working_ext_pal[i] = col_val;
		col_list += 2;
		col_idx = STMemory_ReadWord(col_list);
		/* offset dynamic colours into extended palette
		 * range (colours 16+) */
		col_idx += 16 << 2;
		STMemory_WriteWord(col_list, col_idx);
		col_list += 2;
	}
}
void Call_DumpRegs()
{
	int i;
	log_printf("D: ");
	for (i = 0; i < 8; i++)
	{
		log_printf("$%x ", GetReg(i));
	}
	log_printf("\n");
	log_printf("A: ");
	for (i = 0; i < 8; i++)
	{
		log_printf("$%x ", GetReg(i + 8));
	}
	log_printf("\n");
}

void Call_DumpDebug()
{
	int i, j;
#ifdef M68K_DEBUG
	printf("Debug info. PC @ 68k line %d.\n", line_no);
#endif

	Call_DumpRegs();

	log_printf("Stack:");
	j = GetReg(15);
	for (i = 0; i < 8; i++)
	{
		j += 4;
		log_printf(" $%x", STMemory_ReadLong(j));
	}
	putchar('\n');
}

/* mouse pos in d3,d4 */
void Call_NotifyMousePos()
{
#if 0
	int x, y;
	
	x = GetReg (REG_D3) & 0xffff;
	y = GetReg (REG_D4) & 0xffff;

	x *= ScreenDraw.MouseScale;
	y *= ScreenDraw.MouseScale;
	
	SDL_EventState (SDL_MOUSEMOTION, SDL_DISABLE);
	SDL_WarpMouse (x, y);
	SDL_EventState (SDL_MOUSEMOTION, SDL_ENABLE);
	SDL_ShowCursor (SDL_ENABLE);
#endif /* 0 */
}

static void Call_Idle()
{
	SDL_Delay(0);
}

void Call_HostUpdate()
{
	/* Clear any key presses which are due to be de-bounced (held for one ST frame) */
	Keymap_DebounceAllKeys();
	/* Check 'Function' keys, so if press F12 we update screen correctly to Window! */
	ShortCut_CheckKeys();
	/* And handle any messages, check for quit message */
	Main_EventHandler(); /* Process messages, set 'bQuitProgram' if user tries to quit */
						 /* Pass NULL interrupt function to quit cleanly */
						 // if (bQuitProgram) Int_AddAbsoluteInterrupt(4, 0L);
}

/* d0.b = exception number, a0 = handler. */
static void SetExceptionHandler()
{
	/* only 32 handlers */
	exception_handlers[GetReg(0) & 31] = GetReg(8);
}

#ifdef M68K_DEBUG
int DumpMess(int pos, int line)
{
	if (GetXFlag())
		putchar('X');
	if (GetZFlag())
		putchar('Z');
	if (GetNFlag())
		putchar('N');
	if (GetVFlag())
		putchar('V');
	if (GetCFlag())
		putchar('C');
	return 0;
	log_printf(" $%x $%x $%x $%x $%x $%x $%x $%x*$%x $%x $%x $%x $%x $%x $%x $%x:%d\n",
			   GetReg(0),
			   GetReg(1),
			   GetReg(2),
			   GetReg(3),
			   GetReg(4),
			   GetReg(5),
			   GetReg(6),
			   GetReg(7),
			   GetReg(8),
			   GetReg(9),
			   GetReg(10),
			   GetReg(11),
			   GetReg(12),
			   GetReg(13),
			   GetReg(14),
			   GetReg(15), line_no);
}
#endif
static int _X, _Z, _N, _V, _C;
static int PrevRegs[16];

int changed()
{
	int i;
	if (GetXFlag() != _X)
		return 1;
	if (GetZFlag() != _Z)
		return 1;
	if (GetNFlag() != _N)
		return 1;
	if (GetVFlag() != _V)
		return 1;
	if (GetCFlag() != _C)
		return 1;
	for (i = 0; i < 16; i++)
	{
		if (PrevRegs[i] != GetReg(i))
			return 1;
	}
	return 0;
}

#ifdef M68K_DEBUG
void DumpRegsChanged()
{
	int i;

	// if (!changed ()) return;

	_X = GetXFlag();
	_Z = GetZFlag();
	_V = GetVFlag();
	_N = GetNFlag();
	_C = GetCFlag();

	if (_X)
		putchar('X');
	if (_Z)
		putchar('Z');
	if (_N)
		putchar('N');
	if (_V)
		putchar('V');
	if (_C)
		putchar('C');

	for (i = 0; i < 16; i++)
	{
		if (PrevRegs[i] != GetReg(i))
		{
			log_printf(" %c%d:%x->%x", (i < 8 ? 'd' : 'a'), (i < 8 ? i : i - 8), PrevRegs[i], GetReg(i));
			PrevRegs[i] = GetReg(i);
		}
	}
	log_printf(" @%d\n", line_no);
}
#endif

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#else
#include <physfs.h>
#endif

#include <stdio.h>
#include <string.h>

// Add a sync function to persist data to IndexedDB
#ifdef __EMSCRIPTEN__
static void sync_filesystem(void)
{
	EM_ASM(
		FS.syncfs(false, function(err) {
            if (err) {
                console.log('FS sync error:', err);
            } else {
                console.log('Save data synced to persistent storage');
            } }););
}
#endif

static void Call_Fdelete()
{
	int p, i;
	char filename[64];

	p = GetReg(REG_D1);
	for (i = 0;; i++)
	{
		filename[i] = STMemory_ReadByte(p++);
		if (!filename[i])
			break;
	}

#ifdef __EMSCRIPTEN__
	// Sync after delete operation
	sync_filesystem();
	SetReg(REG_D0, remove(filename));
#elif defined(ANDROID)
	char full_path[512];
	const char *rel = filename;
	if (*rel) rel++;
	if (*rel == '/' || *rel == '\\') rel++;
	snprintf(full_path, sizeof(full_path), "%s/%s",
			 SDL_AndroidGetInternalStoragePath(), rel);
	log_printf("Fdelete path: '%s'", full_path);
	SetReg(REG_D0, remove(full_path));
#else
	SetReg(REG_D0, remove(filename));
#endif
}

static void Call_Fwrite()
{
	int p, i;
	int pBuf = GetReg(REG_A4);
	int len = GetReg(REG_D7);
	char filename[64];
	FILE *f;

	p = GetReg(REG_D1);
	for (i = 0;; i++)
	{
		filename[i] = STMemory_ReadByte(p++);
		if (!filename[i])
			break;
	}

#ifdef __EMSCRIPTEN__
	// Ensure the saves directory exists and use it
	char full_path[128];
	if (strncmp(filename, "/saves/", 7) != 0)
	{
		snprintf(full_path, sizeof(full_path), "/saves/%s", filename);
	}
	else
	{
		strncpy(full_path, filename, sizeof(full_path) - 1);
		full_path[sizeof(full_path) - 1] = '\0';
	}

	if (!(f = fopen(full_path, "wb")))
#elif defined(ANDROID)
	char full_path[512];
	const char *rel = filename;
	if (*rel) rel++;
	if (*rel == '/' || *rel == '\\') rel++;
	snprintf(full_path, sizeof(full_path), "%s/%s",
			 SDL_AndroidGetInternalStoragePath(), rel);
	log_printf("Fwrite path: '%s'", full_path);
	if (!(f = fopen(full_path, "wb")))
#else
	if (!(f = fopen(filename, "wb")))
#endif
	{
		SetReg(REG_D0, 0);
	}
	else
	{
		SetReg(REG_D0, fwrite(STRam + pBuf, 1, len, f));
		fclose(f);

#ifdef __EMSCRIPTEN__
		// Sync after write operation - this is crucial for persistence!
		sync_filesystem();
#endif
	}
}

static void Call_Fread()
{
	int p, i;
	int pBuf = GetReg(REG_A4);
	int len = GetReg(REG_D7);
	char filename[64];
	FILE *f;

	p = GetReg(REG_D1);
	for (i = 0;; i++)
	{
		filename[i] = STMemory_ReadByte(p++);
		if (!filename[i])
			break;
	}

#ifdef __EMSCRIPTEN__
	// Use the saves directory
	char full_path[128];
	if (strncmp(filename, "/saves/", 7) != 0)
	{
		snprintf(full_path, sizeof(full_path), "/saves/%s", filename);
	}
	else
	{
		strncpy(full_path, filename, sizeof(full_path) - 1);
		full_path[sizeof(full_path) - 1] = '\0';
	}

	if (!(f = fopen(full_path, "rb")))
#elif defined(ANDROID)
	char full_path[512];
	const char *rel = filename;
	if (*rel) rel++;
	if (*rel == '/' || *rel == '\\') rel++;
	snprintf(full_path, sizeof(full_path), "%s/%s",
			 SDL_AndroidGetInternalStoragePath(), rel);
	log_printf("Fread path: '%s'", full_path);
	if (!(f = fopen(full_path, "rb")))
#else
	if (!(f = fopen(filename, "rb")))
#endif
	{
		SetReg(REG_D0, 0);
	}
	else
	{
		SetReg(REG_D0, fread(STRam + pBuf, 1, len, f));
		fclose(f);
	}
}

// ======================================== Directory operations ========================================

static char cur_dir[1024];
static char **poo_files = NULL;
static int poo_index = 0;

#ifdef __EMSCRIPTEN__
static int poo_count = 0;
#else
static int physfs_inited = 0;
#endif

#ifdef ANDROID
static char android_mount_path[512];
#endif

static void Call_Fopendir()
{
	int p, i;
	char name[64];

	p = GetReg(REG_A2);
	for (i = 0;; i++)
	{
		name[i] = STMemory_ReadByte(p++);
		if (!name[i])
			break;
	}

#ifdef __EMSCRIPTEN__
	char *dirName = name;
	dirName++;

	// Ensure we're working with the saves directory
	char full_dir_path[128];
	if (strncmp(dirName, "saves", 5) != 0)
	{
		snprintf(full_dir_path, sizeof(full_dir_path), "/saves/%s", dirName);
	}
	else
	{
		snprintf(full_dir_path, sizeof(full_dir_path), "/%s", dirName);
	}

	// Use Emscripten FS
	int result = EM_ASM_INT({
    try {
        var dirName = UTF8ToString($0);
        
        try {
            FS.mkdir(dirName);
        } catch(e) {}

        var files = FS.readdir(dirName);
        var fileCount = 0;

        for (var i = 0; i < files.length; i++) {
            if (files[i] !== '.' && files[i] !== '..') {
                fileCount++;
            }
        }

        Module._poo_file_count = fileCount;
        Module._poo_file_list = files;

        return 0;
    } catch(e) {
        return -1;
    }
}, full_dir_path);

if (result == 0)
{
	strcpy(cur_dir, full_dir_path);
	poo_index = 0;
	poo_count = EM_ASM_INT({ return Module._poo_file_count || 0; });
	SetReg(REG_D0, 0);
}
else
{
	SetReg(REG_D0, -1);
}

#else
	// PhysFS implementation
	if (!physfs_inited)
	{
		if (!PHYSFS_init(NULL))
		{
			SetReg(REG_D0, -1);
			return;
		}

#ifdef ANDROID
		// char *pref_path = SDL_GetPrefPath("glfrontier", "glfrontier");
		// log_printf("pref path %s\n", pref_path);
		// if (!pref_path || !PHYSFS_setWriteDir(pref_path))
		// {
		// 	SDL_free(pref_path);
		// 	SetReg(REG_D0, -1);
		// 	log_printf("failed to load %s\n", pref_path);
		// 	return;
		// }
		// SDL_free(pref_path);

		const char *internal_path = SDL_AndroidGetInternalStoragePath();
		log_printf("android internal path: '%s'", internal_path ? internal_path : "NULL");
		if (!internal_path || !PHYSFS_setWriteDir(internal_path))
		{
			log_printf("failed to load %s\n", internal_path);
			SetReg(REG_D0, -1);
			return;
		}
#else
		if (!PHYSFS_setWriteDir("."))
		{
			SetReg(REG_D0, -1);
			return;
		}
#endif

		physfs_inited = 1;
	}

	char *dirName = name + 1; // skips drive letter
	if (*dirName == '/') dirName++; // skips leading slash if present
	if (PHYSFS_mkdir(dirName) == 0)
	{
		log_printf("failed to create savs directory...");
	}

	strncpy(cur_dir, name, sizeof(cur_dir) - 1);
	cur_dir[sizeof(cur_dir) - 1] = '\0';

	if (poo_files)
	{
		PHYSFS_freeList(poo_files);
		poo_files = NULL;
	}

#ifdef ANDROID
	snprintf(android_mount_path, sizeof(android_mount_path), "%s/%s",
			 SDL_AndroidGetInternalStoragePath(), dirName);
	log_printf("dirName: '%s'", dirName);
	log_printf("android mount path: '%s'", android_mount_path);
	if (!PHYSFS_mount(android_mount_path, "/", 1))
#else
	if (!PHYSFS_mount(cur_dir, "/", 1))
#endif
	{
		log_printf("PHYSFS_mount failed: %s", PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()));
		SetReg(REG_D0, -1);
		return;
	}

	poo_files = PHYSFS_enumerateFiles("/");
	log_printf("enumerateFiles result: %s", poo_files ? "ok" : PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()));
	poo_index = 0;

	if (poo_files)
	{
		while (poo_files[poo_index] &&
			   (strcmp(poo_files[poo_index], ".") == 0 ||
				strcmp(poo_files[poo_index], "..") == 0))
		{
			poo_index++;
		}

		SetReg(REG_D0, 0);
	}
	else
	{
		SetReg(REG_D0, -1);
	}
#endif
}

static void Call_Fclosedir()
{
#ifdef __EMSCRIPTEN__
	EM_ASM({
		if (Module._poo_file_list)
		{
			delete Module._poo_file_list;
			delete Module._poo_file_count;
		}
	});
#else
	if (poo_files)
	{
		PHYSFS_freeList(poo_files);
		poo_files = NULL;
	}

#ifdef ANDROID
	PHYSFS_unmount(android_mount_path);
#else
	PHYSFS_unmount(cur_dir);
#endif
#endif
}

#define MAX_FILENAME_LEN 14

static void Call_Freaddir()
{
	int p, i, attribs = 0, len = 0;
	char name[MAX_FILENAME_LEN];

#ifdef __EMSCRIPTEN__
	if (poo_index >= poo_count)
	{
		SetReg(REG_D0, -1);
		return;
	}

	// Get filename from JS
	EM_ASM({
		var files = Module._poo_file_list;
		var realIndex = 0;
		var currentIndex = 0;
		
		for (var i = 0; i < files.length; i++) {
			if (files[i] !== '.' && files[i] !== '..') {
				if (currentIndex === $0) {
					var filename = files[i];
					var maxLen = $1;
					
					for (var j = 0; j < maxLen - 1 && j < filename.length; j++) {
						setValue($2 + j, filename.charCodeAt(j), 'i8');
					}
					setValue($2 + Math.min(maxLen - 1, filename.length), 0, 'i8');
					
					try {
						var fullPath = UTF8ToString($3) + '/' + filename;
						var stat = FS.stat(fullPath);
						setValue($4, stat.size, 'i32');
						setValue($5, FS.isDir(stat.mode) ? 0x10 : 0, 'i32');
					} catch(e) {
						setValue($4, 0, 'i32');
						setValue($5, 0, 'i32');
					}
					
					return;
				}
				currentIndex++;
			}
		} }, poo_index, MAX_FILENAME_LEN, name, cur_dir, &len, &attribs);

#else
	char full_virtual_path[1024];
	char full_physical_path[1024];
	PHYSFS_Stat _stat;
	PHYSFS_Stat fs_stat;

	if (!poo_files || !poo_files[poo_index])
	{
		SetReg(REG_D0, -1);
		return;
	}

	strncpy(name, poo_files[poo_index], MAX_FILENAME_LEN - 1);
	name[MAX_FILENAME_LEN - 1] = '\0';

	snprintf(full_virtual_path, sizeof(full_virtual_path), "/%s", poo_files[poo_index]);

	if (PHYSFS_stat(full_virtual_path, &_stat))
	{
		len = (int)_stat.filesize;
		attribs = (_stat.filetype == PHYSFS_FILETYPE_DIRECTORY) ? 0x10 : 0;
	}
	else
	{
		snprintf(full_physical_path, sizeof(full_physical_path), "%s/%s", cur_dir, poo_files[poo_index]);

		if (PHYSFS_stat(full_physical_path, &fs_stat))
		{
			len = (int)fs_stat.filesize;
			attribs = (fs_stat.filetype == PHYSFS_FILETYPE_DIRECTORY) ? 0x10 : 0;
		}
		else
		{
			len = 0;
			attribs = 0;
		}
	}
#endif

	p = GetReg(REG_A0);
	for (i = 0; i < MAX_FILENAME_LEN; i++)
	{
		STMemory_WriteByte(p++, name[i]);
	}

	SetReg(REG_D2, attribs);
	SetReg(REG_D1, len);
	SetReg(REG_D0, 0);

	poo_index++;
}
// ==========================================================================================================

static void not_available() {}

HOSTCALL hcalls[] = {
	&SetExceptionHandler,
	&Call_Memset,			  /* 0x1 */
	&Call_MemsetBlue,		  /* 0x2 */
	&Call_BlitCursor,		  /* 0x3 */
	&Call_RestoreUnderCursor, /* 0x4 */
	&Call_BlitBmp,			  /* 0x5 */
	&Call_OldHLine,			  /* 0x6 */
	&Call_HostUpdate,		  /* 0x7 */
	&Call_Memcpy,			  /* 0x8 */
	&Call_PutPix,			  /* 0x9 */
	&Call_BackHLine,		  /* 0xa */
	&Call_FillLine,			  /* 0xb */
	&Call_SetMainPalette,	  /* 0xc */
	&Call_SetCtrlPalette,	  /* 0xd */
	&Call_SetScreenBase,	  /* 0xe */
	NULL,					  /* 0xf */
	&Call_DumpRegs,			  /* 0x10 */
	&Call_MakeExtPalette,	  /* 0x11 */
	&Call_PlaySFX,			  /* 0x12 */
	&Call_GetMouseInput,	  /* 0x13 */
	&Call_GetKeyboardEvent,	  /* 0x14 */
	NULL,					  /* 0x15 */
	&Call_HLine,			  /* 0x16 */
	NULL,					  /* 0x17 */
	&Call_NotifyMousePos,	  /* 0x18 */
	&Call_InformScreens,	  /* 0x19 */
	NULL,
	&Call_DrawStrShadowed, /* 0x1b */
	&Call_DrawStr,		   /* 0x1c */
	&Call_PlayMusic,	   /* 0x1d */
	&Call_StopMusic,	   /* 0x1e */
	&Call_Idle,			   /* 0x1f */
#ifdef M68K_DEBUG
	&Call_DumpDebug, /* 0x20 */
#else
	NULL,
#endif
	&Call_IsMusicPlaying,
	&Call_Fread, /* 0x22 */
	&Call_Fwrite,
	&Call_Fdelete,
	&Call_Fopendir, /* 0x25 */
	&Call_Freaddir,
	&Call_Fclosedir,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL, /* 0x30 */
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL, /* 0x40 */
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL, /* 0x50 */
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
#ifdef WITH_GL
	Nu_PutTriangle, /* 0x60 */
	Nu_PutQuad,
	Nu_PutLine,
	Nu_PutPoint,
	Nu_PutTwinklyCircle,
	Nu_PutColoredPoint,
	Nu_PutBezierLine,
	Nu_ComplexStart,
	Nu_ComplexSNext,
	Nu_ComplexSBegin,
	Nu_ComplexEnd,
	Nu_3DViewInit,
	Nu_InsertZNode,
	Nu_ComplexStartInner,
	Nu_ComplexBezier,
	Nu_DrawScreen,
	Nu_PutTeardrop,
	Nu_PutCircle,
	Nu_PutOval,
	Nu_IsGLRenderer,
	Nu_GLClearArea,
	Nu_QueueDrawStr,
	Nu_PutCylinder,
	Nu_PutBlob,
	Nu_PutPlanet,
	Nu_Put2DLine
#else
	not_available,
	not_available,
	not_available,
	not_available,
	not_available,
	not_available,
	not_available,
	not_available,
	not_available,
	not_available,
	not_available,
	not_available,
	not_available,
	not_available,
	not_available,
	Nu_DrawScreen,
	not_available,
	not_available,
	not_available,
	Nu_IsGLRenderer,
	not_available,
	not_available,
	not_available,
	not_available,
	not_available,
	not_available,
#endif
};