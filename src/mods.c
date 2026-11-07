/*
 * mods.c - see mods.h.
 *
 * Saving with mods_vanilla_saves on: L73038_MakeSaveData calls
 * Call_ModsSaveBegin, every mod's strip function changes RAM through
 * mods_strip_*() (which remembers the old bytes), the game builds the save
 * from that, and Call_ModsSaveEnd puts every byte back.
 */
#include <math.h>

#include "mods.h"
#include "game_state.h"
#include "host.h" /* rdbyte, wrbyte, ... */
#include "main.h"
#include "custom_ships.h"

bool mods_vanilla_saves;

/* =========================================================================
 * Undo log
 * ========================================================================= */
#define UNDO_MAX 1024

static struct
{
	u32 addr;
	u8 old;
} undo[UNDO_MAX];
static int undo_count;
static bool undo_full;

static void strip(u32 addr, u32 value, int bytes)
{
	for (int i = 0; i < bytes; i++)
	{
		if (undo_count == UNDO_MAX)
		{
			undo_full = true;
			return;
		}
		undo[undo_count].addr = addr + i;
		undo[undo_count].old = (u8)rdbyte(addr + i);
		undo_count++;
		wrbyte(addr + i, (value >> (8 * (bytes - 1 - i))) & 0xff);
	}
}

void mods_strip_byte(unsigned int addr, int value)
{
	strip(addr, (u32)value, 1);
}
void mods_strip_word(unsigned int addr, int value)
{
	strip(addr, (u32)value, 2);
}
void mods_strip_long(unsigned int addr, unsigned int value)
{
	strip(addr, value, 4);
}

/* =========================================================================
 * Shared: station module strings added by mods
 * ========================================================================= */
/* The game's own station strings end at $99cd; mods add theirs after it
 * (see Lafmu_strings). Anything showing one of those ids would print
 * garbage, or crash when clicked, in the unmodded game. */
#define MOD_STRING_FIRST 0x99ce
#define MOD_STRING_LAST  0x99ff
#define BLANK_STRING     0x98aa /* just a new line */

#define A6_LIST          10602 /* 45 rows of 24 bytes, id in the low word of +0 */
#define A6_LIST_ROWS     11724
#define A6_MESSAGE       11690 /* message being shown, id in the low word */
#define A6_MESSAGES_USED 11792 /* bytes used in the queue below */
#define A6_MESSAGES      11794 /* queued messages, 28 bytes each */

static bool mod_string(u32 id_addr)
{
	u16 id = (u16)rdword(id_addr);
	return id >= MOD_STRING_FIRST && id <= MOD_STRING_LAST;
}

static void strip_mod_strings(void)
{
	/* list rows: end the list at the first one of ours */
	for (int row = 0; row < 45; row++)
	{
		u32 r = GAME_A6 + A6_LIST + 24 * row;
		if (rdlong(r) == 0)
			break;
		if (mod_string(r + 2))
		{
			mods_strip_long(r, 0);
			mods_strip_word(GAME_A6 + A6_LIST_ROWS, row);
			break;
		}
	}
	/* messages: show a blank one instead */
	if (mod_string(GAME_A6 + A6_MESSAGE + 2))
		mods_strip_word(GAME_A6 + A6_MESSAGE + 2, BLANK_STRING);
	int used = rdword(GAME_A6 + A6_MESSAGES_USED);
	for (int off = 0; off + 28 <= used && off < 0x70; off += 28)
		if (mod_string(GAME_A6 + A6_MESSAGES + off + 2))
			mods_strip_word(GAME_A6 + A6_MESSAGES + off + 2, BLANK_STRING);
}

/* =========================================================================
 * Auto Field-Maintenance Unit (Lafmu_ in fe2.s)
 * ========================================================================= */
#define AFMU_BIT       19
#define OBJ_EQUIPMENT  158
#define A6_CARGO_SPACE 738

#if FE2_USE_MODDED
static void afmu_strip(void)
{
	u32 ship = game_player_ship();
	if (!ship)
		return;
	u32 fitted = (u32)rdlong(ship + OBJ_EQUIPMENT);
	if (!(fitted & (1u << AFMU_BIT)))
		return;
	/* as if it was sold: the bit goes, its mass is cargo space again */
	mods_strip_long(ship + OBJ_EQUIPMENT, fitted & ~(1u << AFMU_BIT));
	int mass = rdword(FE2_Lafmu_equipment + 4);
	mods_strip_word(GAME_A6 + A6_CARGO_SPACE, rdword(GAME_A6 + A6_CARGO_SPACE) + mass);
}
#endif

/* =========================================================================
 * Registry
 * ========================================================================= */
/* None in the original game build (GLF_MODDED_FE2 off) */
#if FE2_USE_MODDED
static const GameMod mods[] = {
	{"Auto Field-Maintenance Unit",
	 "Equipment sold everywhere. For a tonne of hydrogen, from the comms panel: repairs the hull "
	 "and all damaged equipment and gives the ship a deluxe service (drive and equipment), so "
	 "nothing breaks down for lack of servicing far from a station.",
	 "Equipment flag bit 19 on your ship and its 3t mass. Left out, the unit is gone (as if "
	 "sold, without the money) when the save is loaded.",
	 afmu_strip},
	{"Free camera",
	 "Ctrl-V, FREE CAMERA in this menu, or the camera button in the touch dropdown pauses the "
	 "game in the flight view and lets you fly a camera around freely.",
	 NULL, NULL},
	{"Custom ship models",
	 "Models and new ship types made with FE2ShipBuilder (tools/fe2ShipBuilder/custom_ships): replace the "
	 "game's ships or add new ones to shipyards and traffic.",
	 "Ships of a new type (the player's too, and those for sale in the shipyards) become the original "
	 "ship they are based on.",
	 custom_ships_strip},
	{"Original game bug fixes",
	 "From the frontier-elite-vibes fork. Gravity doubling in low orbit; ships destroyed far out in orbit; "
	 "\"Relative to\" never switching to a station; altitude shown from 2000 km over stars and gas giants; "
	 "passengers past their month leaving at the next landing; police Vipers exploding on launch; proximity "
	 "mines going off near a ship; pirates more often in groups; time acceleration stepping down as a ship "
	 "closes in on you. Smoke and mine options are in the settings.",
	 "A police ship's launch time, in a field the unmodded game does not use.", NULL},
};
#define MOD_COUNT ((int)(sizeof(mods) / sizeof(mods[0])))
#else
static const GameMod *const mods = NULL;
#define MOD_COUNT 0
#endif

int mods_count(void)
{
	return MOD_COUNT;
}

const GameMod *mods_get(int i)
{
	return (i >= 0 && i < mods_count()) ? &mods[i] : NULL;
}

/* =========================================================================
 * Host calls
 * ========================================================================= */
void Call_ModsSaveBegin(void)
{
	undo_count = 0;
	undo_full = false;
	if (!mods_vanilla_saves)
		return;
	strip_mod_strings();
	for (int i = 0; i < mods_count(); i++)
		if (mods[i].strip)
			mods[i].strip();
	if (undo_full)
		log_printf("Mods: too much to strip from the save, it may not load unmodded\n");
}

void Call_ModsSaveEnd(void)
{
	/* newest first, so bytes changed twice get their original value */
	while (undo_count > 0)
	{
		undo_count--;
		wrbyte(undo[undo_count].addr, undo[undo_count].old);
	}
}

/* =========================================================================
 * Options of the game's bug fixes
 * ========================================================================= */
bool smoke_inherit = false;
int smoke_cap_ms = 500;       /* the speed smoke is left behind at, m/s */
#define SMOKE_UNITS_PER_MS 10.6 /* 5300 velocity units = 500 m/s */

bool mine_prox = true;
int mine_radius_m = 250;

/* Lsmoke_vel: A0 = the damaged ship, A1 = its new puff of smoke, made at rest.
 * Gives the puff the part of the ship's velocity over the limit, so a fast
 * ship's trail does not stretch for hundreds of km. */
void Call_SmokeVel(void)
{
	if (!smoke_inherit)
		return;
	u32 ship = (u32)GetReg(REG_A0), puff = (u32)GetReg(REG_A1);
	if (!ship || !puff)
		return;
	double v[3], len2 = 0;
	for (int i = 0; i < 3; i++)
	{
		v[i] = (double)(s32)rdlong(ship + 98 + 4 * i);
		len2 += v[i] * v[i];
	}
	double cap = smoke_cap_ms * SMOKE_UNITS_PER_MS;
	if (len2 <= cap * cap)
		return;
	double keep = 1.0 - cap / sqrt(len2);
	for (int i = 0; i < 3; i++)
	{
		wrlong(puff + 98 + 4 * i, (int)(v[i] * keep));
		wrword(puff + 192 + 2 * i, 0);
	}
}

/* Lmine_phys, Lmine_boom: D0 = the radius in units of 1024 mm, 0 = no fuse */
void Call_MineRadius(void)
{
	SetReg(REG_D0, mine_prox ? mine_radius_m * 1000 / 1024 : 0);
}
