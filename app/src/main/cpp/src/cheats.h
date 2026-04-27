#ifndef CHEATS_H
#define CHEATS_H

#include <stdint.h>
#include <stdbool.h>

// External variables (kept for compatibility with your existing code)
// extern int dump_m68k_toggle;
extern int cash_value[4];
extern int cheat_player_values[];
extern int cheat_ship_values[];
extern char *ship_ids[];
extern int ship_id_index;

// Core cheat functions (pure logic, no GUI)
void init_cheats(void);

void update_player_cheat_values_m68kram(void);
void inject_player_cheat_values_m68kram(void);
void inject_cash_value_m68kram(void);

int  find_and_store_ship_ids(void);
void update_ship_cheat_values_m68kram(unsigned int ship_index);
void inject_ship_cheat_values_m68kram(void);

#endif