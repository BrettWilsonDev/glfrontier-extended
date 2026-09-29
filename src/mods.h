/*
 * mods.h - the mods made to the game code (fe2/fe2.s), what each one leaves
 * in saved games, and taking that out for saves the unmodded game can load.
 */
#ifndef MODS_H
#define MODS_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct
{
	const char *name;
	const char *about;
	/* What the mod stores in saved games, NULL if nothing */
	const char *in_saves;
	/* Takes the mod's state out of RAM with mods_strip_*() (undone after
	 * the save); NULL if there is nothing to take out */
	void (*strip)(void);
} GameMod;

/* Save games without mod state, loadable by the unmodded game */
extern bool mods_vanilla_saves;

int mods_count(void);
const GameMod *mods_get(int i);

/* For strip functions: change RAM for the duration of the save */
void mods_strip_byte(unsigned int addr, int value);
void mods_strip_word(unsigned int addr, int value);
void mods_strip_long(unsigned int addr, unsigned int value);

/* host calls, around L73038_MakeSaveData */
void Call_ModsSaveBegin(void);
void Call_ModsSaveEnd(void);

#ifdef __cplusplus
}
#endif

#endif /* MODS_H */
