#include "cheats.h"
#include "host.h"   // for rdbyte, wrbyte, MEM_SIZE, log_printf, etc.

// int dump_m68k_toggle = 0;
int cash_value[4] = {0, 0, 0, 0};
int cheat_player_values[200] = {0};
int cheat_ship_values[100] = {0};
char *ship_ids[256] = {NULL};
int ship_id_index = 0;

static u32 address_player = 1072;
static u32 address_ship = 0;
static u32 ship_ids_address[256];

static int is_uppercase(u8 val)
{
    return (val >= 65 && val <= 90); // A-Z
}

static int is_digit(u8 val)
{
    return (val >= 48 && val <= 57); // 0-9
}

// ====================== Player Cheats ======================

void update_player_cheat_values_m68kram(void)
{
    for (int i = 0; i < 200; i++)
    {
        cheat_player_values[i] = (u32)((u8)rdbyte(address_player + i));
    }
}

void inject_player_cheat_values_m68kram(void)
{
    for (int i = 0; i < 200; i++)
    {
        wrbyte(address_player + i, (u32)cheat_player_values[i]);
    }
    update_player_cheat_values_m68kram();   // refresh after write (your original behavior)
}

void inject_cash_value_m68kram(void)
{
    wrbyte(address_player + 12, cash_value[0]);
    wrbyte(address_player + 13, cash_value[1]);
    wrbyte(address_player + 14, cash_value[2]);
    wrbyte(address_player + 15, cash_value[3]);
}

// ====================== Ship ID Scanner (your exact logic) ======================

int find_and_store_ship_ids(void)
{
    u32 match_addresses[256];
    int match_count = 0;

    for (u32 pos = 0; pos < MEM_SIZE; pos += 1)
    {
        u8 byte_val = (u8)rdbyte(pos);

        if (is_uppercase(byte_val))
        {
            u32 current_pos = pos;

            if (current_pos < MEM_SIZE && is_uppercase((u8)rdbyte(current_pos))) current_pos++; else continue;
            if (current_pos < MEM_SIZE && is_uppercase((u8)rdbyte(current_pos))) current_pos++; else continue;

            if (current_pos >= MEM_SIZE || (u8)rdbyte(current_pos) != 45) continue;
            current_pos++;

            if (current_pos < MEM_SIZE && is_digit((u8)rdbyte(current_pos))) current_pos++; else continue;
            if (current_pos < MEM_SIZE && is_digit((u8)rdbyte(current_pos))) current_pos++; else continue;
            if (current_pos < MEM_SIZE && is_digit((u8)rdbyte(current_pos))) current_pos++; else continue;

            if (current_pos >= MEM_SIZE || (u8)rdbyte(current_pos) != 0) continue;
            current_pos++;

            if (current_pos >= MEM_SIZE || (u8)rdbyte(current_pos) != 114) continue;

            match_addresses[match_count] = pos;
            match_count++;

            pos = current_pos;
        }
    }

    for (u32 i = 0; i < match_count; i++)
    {
        char ship_id[10];
        sprintf(ship_id, "%c%c%c%c%c%c",
                (char)rdbyte(match_addresses[i]),
                (char)rdbyte(match_addresses[i] + 1),
                (char)rdbyte(match_addresses[i] + 2),
                (char)rdbyte(match_addresses[i] + 3),
                (char)rdbyte(match_addresses[i] + 4),
                (char)rdbyte(match_addresses[i] + 5));

        if (ship_ids[i]) free(ship_ids[i]);
        ship_ids[i] = strdup(ship_id);
    }

    memcpy(ship_ids_address, match_addresses, sizeof(u32) * match_count);

    for (int i = match_count; i < 256; i++)
    {
        if (ship_ids[i])
        {
            free(ship_ids[i]);
            ship_ids[i] = NULL;
        }
    }

    return match_count;
}

// ====================== Ship Cheats ======================

void update_ship_cheat_values_m68kram(unsigned int ship_index)
{
    if (ship_index >= 256) return;

    address_ship = ship_ids_address[ship_index];
    address_ship -= 100;

    for (int i = 0; i < 100; i++)
    {
        cheat_ship_values[i] = (u32)((u8)rdbyte(address_ship + i));
    }
}

void inject_ship_cheat_values_m68kram(void)
{
    if (address_ship == 0) return;

    for (int i = 0; i < 100; i++)
    {
        wrbyte(address_ship + i, (u32)cheat_ship_values[i]);
    }
    for (int i = 0; i < 100; i++)
    {
        cheat_ship_values[i] = (u32)((u8)rdbyte(address_ship + i));
    }
}

void init_cheats(void)
{
    for (int i = 0; i < 256; i++)
        ship_ids[i] = NULL;
}