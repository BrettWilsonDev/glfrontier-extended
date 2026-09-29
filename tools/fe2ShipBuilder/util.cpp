/*
 * util.cpp - see util.h.
 */
#include "util.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>
#else
#include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace studio
{

std::string exe_dir()
{
#ifdef _WIN32
	char path[MAX_PATH];
	DWORD n = GetModuleFileNameA(nullptr, path, MAX_PATH);
	std::string p(path, n);
	return p.substr(0, p.find_last_of("\\/") + 1);
#else
	char path[1024];
	ssize_t n = readlink("/proc/self/exe", path, sizeof(path) - 1);
	if (n <= 0)
		return "./";
	std::string p(path, (size_t)n);
	return p.substr(0, p.find_last_of('/') + 1);
#endif
}

bool file_exists(const std::string &path)
{
	std::error_code ec;
	return fs::exists(fs::u8path(path), ec);
}

bool read_file(const std::string &path, std::vector<uint8_t> &out)
{
	std::ifstream f(fs::u8path(path), std::ios::binary);
	if (!f)
		return false;
	out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
	return true;
}

bool write_file(const std::string &path, const std::vector<uint8_t> &data)
{
	std::ofstream f(fs::u8path(path), std::ios::binary);
	if (!f)
		return false;
	f.write((const char *)data.data(), (std::streamsize)data.size());
	return (bool)f;
}

std::vector<std::string> list_dir(const std::string &dir)
{
	std::vector<std::string> out;
	std::error_code ec;
	for (auto &e : fs::directory_iterator(fs::u8path(dir), ec))
		if (e.is_regular_file())
			out.push_back(e.path().filename().u8string());
	std::sort(out.begin(), out.end());
	return out;
}

bool make_dirs(const std::string &dir)
{
	std::error_code ec;
	fs::create_directories(fs::u8path(dir), ec);
	return !ec;
}

std::string join(const std::string &dir, const std::string &name)
{
	if (dir.empty())
		return name;
	char last = dir.back();
	return (last == '/' || last == '\\') ? dir + name : dir + "/" + name;
}

std::string base_name(const std::string &path) { return path.substr(path.find_last_of("/\\") + 1); }

std::string normal_path(const std::string &path)
{
	return fs::u8path(path).lexically_normal().make_preferred().u8string();
}

#ifdef _WIN32
static std::string filter_string(const char *filter)
{
	/* "Name|*.x|Name2|*.y" -> double-NUL separated */
	std::string f(filter);
	std::replace(f.begin(), f.end(), '|', '\0');
	f.push_back('\0');
	f.push_back('\0');
	return f;
}

std::string open_dialog(const char *title, const char *filter)
{
	char file[MAX_PATH] = "";
	std::string flt = filter_string(filter);
	OPENFILENAMEA ofn = {};
	ofn.lStructSize = sizeof(ofn);
	ofn.lpstrFilter = flt.c_str();
	ofn.lpstrFile = file;
	ofn.nMaxFile = MAX_PATH;
	ofn.lpstrTitle = title;
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
	return GetOpenFileNameA(&ofn) ? std::string(file) : std::string();
}

std::string save_dialog(const char *title, const char *filter, const char *default_name, const char *ext)
{
	char file[MAX_PATH] = "";
	snprintf(file, sizeof(file), "%s", default_name ? default_name : "");
	std::string flt = filter_string(filter);
	OPENFILENAMEA ofn = {};
	ofn.lStructSize = sizeof(ofn);
	ofn.lpstrFilter = flt.c_str();
	ofn.lpstrFile = file;
	ofn.nMaxFile = MAX_PATH;
	ofn.lpstrTitle = title;
	ofn.lpstrDefExt = ext;
	ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
	return GetSaveFileNameA(&ofn) ? std::string(file) : std::string();
}

bool launch(const std::string &exe, std::string &err)
{
	std::string dir = exe.substr(0, exe.find_last_of("\\/") + 1);
	std::string cmd = "\"" + exe + "\"";
	STARTUPINFOA si = {};
	si.cb = sizeof(si);
	PROCESS_INFORMATION pi = {};
	if (!CreateProcessA(exe.c_str(), &cmd[0], nullptr, nullptr, FALSE, 0, nullptr, dir.empty() ? nullptr : dir.c_str(),
						&si, &pi))
	{
		err = "couldn't start " + exe;
		return false;
	}
	CloseHandle(pi.hThread);
	CloseHandle(pi.hProcess);
	return true;
}

void open_folder(const std::string &dir) { ShellExecuteA(nullptr, "open", dir.c_str(), nullptr, nullptr, SW_SHOWNORMAL); }
#else
std::string open_dialog(const char *, const char *) { return ""; }
std::string save_dialog(const char *, const char *, const char *, const char *) { return ""; }
bool launch(const std::string &exe, std::string &err)
{
	std::string cmd = "\"" + exe + "\" &";
	if (system(cmd.c_str()) != 0)
	{
		err = "couldn't start " + exe;
		return false;
	}
	return true;
}
void open_folder(const std::string &dir)
{
	std::string cmd = "xdg-open \"" + dir + "\" &";
	(void)!system(cmd.c_str());
}
#endif

/* -- ship pack --------------------------------------------------------------------- */

static const char MAGIC[8] = {'F', 'E', '2', 'M', 'P', 'A', 'C', 'K'};

static void put32(std::vector<uint8_t> &b, uint32_t v)
{
	b.push_back((uint8_t)(v >> 24));
	b.push_back((uint8_t)(v >> 16));
	b.push_back((uint8_t)(v >> 8));
	b.push_back((uint8_t)v);
}
static uint32_t get32(const uint8_t *p) { return ((uint32_t)p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3]; }

/* size of the exe without any pack on the end */
static size_t exe_size_without_pack(const std::vector<uint8_t> &exe)
{
	if (exe.size() < 12 || memcmp(exe.data() + exe.size() - 8, MAGIC, 8) != 0)
		return exe.size();
	uint32_t size = get32(exe.data() + exe.size() - 12);
	if (size + 12 > exe.size())
		return exe.size();
	return exe.size() - 12 - size;
}

bool write_game_with_ships(const std::string &game_exe, const std::string &out_exe,
						   const std::vector<std::pair<std::string, std::vector<uint8_t>>> &files, std::string &err)
{
	std::vector<uint8_t> exe;
	if (!read_file(game_exe, exe))
	{
		err = "can't read " + game_exe;
		return false;
	}
	exe.resize(exe_size_without_pack(exe));
	std::vector<uint8_t> pack = {'F', 'E', '2', 'P'};
	put32(pack, (uint32_t)files.size());
	for (auto &f : files)
	{
		pack.push_back((uint8_t)(f.first.size() >> 8));
		pack.push_back((uint8_t)f.first.size());
		pack.insert(pack.end(), f.first.begin(), f.first.end());
		put32(pack, (uint32_t)f.second.size());
		pack.insert(pack.end(), f.second.begin(), f.second.end());
	}
	if (!files.empty())
	{
		exe.insert(exe.end(), pack.begin(), pack.end());
		put32(exe, (uint32_t)pack.size());
		exe.insert(exe.end(), MAGIC, MAGIC + 8);
	}
	/* write next to it, then swap in: the old exe may be in use */
	std::string tmp = out_exe + ".new";
	if (!write_file(tmp, exe))
	{
		err = "can't write " + tmp;
		return false;
	}
	std::error_code ec;
	fs::rename(fs::u8path(tmp), fs::u8path(out_exe), ec);
	if (ec)
	{
		fs::remove(fs::u8path(tmp), ec);
		err = "can't replace " + out_exe + " (is the game running?)";
		return false;
	}
	return true;
}

bool read_ship_pack(const std::string &game_exe, std::vector<std::pair<std::string, std::vector<uint8_t>>> &files)
{
	files.clear();
	std::vector<uint8_t> exe;
	if (!read_file(game_exe, exe))
		return false;
	size_t start = exe_size_without_pack(exe);
	if (start == exe.size())
		return true;
	const uint8_t *p = exe.data() + start, *end = exe.data() + exe.size() - 12;
	if (end - p < 8 || memcmp(p, "FE2P", 4) != 0)
		return false;
	uint32_t count = get32(p + 4);
	p += 8;
	for (uint32_t i = 0; i < count && p + 2 <= end; i++)
	{
		int nl = (p[0] << 8) | p[1];
		p += 2;
		if (p + nl + 4 > end)
			return false;
		std::string name((const char *)p, nl);
		p += nl;
		uint32_t len = get32(p);
		p += 4;
		if (p + len > end)
			return false;
		files.push_back({name, std::vector<uint8_t>(p, p + len)});
		p += len;
	}
	return true;
}

} // namespace studio
