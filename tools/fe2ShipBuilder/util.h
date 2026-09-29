/*
 * util.h - files, folders, dialogs, starting the game, and putting ships
 * into a GLFrontier.exe.
 *
 * Ship pack appended to GLFrontier.exe (read by src/custom_ships.c):
 *   "FE2P" u32 count, then per file: u16 name length, name, u32 length, data
 *   then a 12 byte footer: u32 size of everything above, "FE2MPACK"
 * All big endian. Windows doesn't mind data after the end of an exe.
 */
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace studio
{

std::string exe_dir(); /* with a trailing slash */
bool file_exists(const std::string &path);
bool read_file(const std::string &path, std::vector<uint8_t> &out);
bool write_file(const std::string &path, const std::vector<uint8_t> &data);
std::vector<std::string> list_dir(const std::string &dir); /* file names */
bool make_dirs(const std::string &dir);
std::string join(const std::string &dir, const std::string &name);
std::string base_name(const std::string &path);
std::string normal_path(const std::string &path); /* no ./ or ../, native slashes */

/* native dialogs (empty string when cancelled); filter like "OBJ files|*.obj" */
std::string open_dialog(const char *title, const char *filter);
std::string save_dialog(const char *title, const char *filter, const char *default_name, const char *ext);

bool launch(const std::string &exe, std::string &err);
void open_folder(const std::string &dir);

/* files: (name, data). Replaces any pack already in the exe. */
bool write_game_with_ships(const std::string &game_exe, const std::string &out_exe,
						   const std::vector<std::pair<std::string, std::vector<uint8_t>>> &files, std::string &err);
/* the ship files currently packed into an exe */
bool read_ship_pack(const std::string &game_exe, std::vector<std::pair<std::string, std::vector<uint8_t>>> &files);

} // namespace studio
