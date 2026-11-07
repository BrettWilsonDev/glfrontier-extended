/*
 * user_dir.c - see user_dir.h.
 */
#if !defined(__EMSCRIPTEN__) && !defined(ANDROID)

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>
#else
#include <dirent.h>
#include <errno.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#include "main.h"
#include "user_dir.h"

#define PATH_LEN 1024

static char data_dir[PATH_LEN];
static char config_dir[PATH_LEN];
static bool ready;

/* -- UTF-8 file calls ------------------------------------------------------ */

#ifdef _WIN32
/* UTF-8 to UTF-16; false if it does not fit */
static bool widen(const char *s, wchar_t *out, int size)
{
	return MultiByteToWideChar(CP_UTF8, 0, s, -1, out, size) > 0;
}

FILE *user_fopen(const char *path, const char *mode)
{
	wchar_t wpath[PATH_LEN], wmode[16];
	if (!widen(path, wpath, PATH_LEN) || !widen(mode, wmode, 16))
		return NULL;
	return _wfopen(wpath, wmode);
}

int user_remove(const char *path)
{
	wchar_t wpath[PATH_LEN];
	if (!widen(path, wpath, PATH_LEN))
		return -1;
	return _wremove(wpath);
}

static bool is_dir(const char *path)
{
	wchar_t wpath[PATH_LEN];
	if (!widen(path, wpath, PATH_LEN))
		return false;
	DWORD attr = GetFileAttributesW(wpath);
	return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY);
}

static void make_one_dir(const char *path)
{
	wchar_t wpath[PATH_LEN];
	if (widen(path, wpath, PATH_LEN))
		CreateDirectoryW(wpath, NULL); /* fails harmlessly if it is there */
}
#else
FILE *user_fopen(const char *path, const char *mode)
{
	return fopen(path, mode);
}

int user_remove(const char *path)
{
	return remove(path);
}

static bool is_dir(const char *path)
{
	struct stat st;
	return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static void make_one_dir(const char *path)
{
	mkdir(path, 0755); /* fails harmlessly if it is there */
}
#endif

static bool is_sep(char c)
{
	return c == '/' || c == '\\';
}

/* mkdir -p; true if the folder is there afterwards */
static bool make_dirs(const char *path)
{
	char part[PATH_LEN];
	size_t len = strlen(path);
	if (!len || len >= PATH_LEN)
		return false;
	for (size_t i = 1; i <= len; i++)
	{
		if (i == len || is_sep(path[i]))
		{
			memcpy(part, path, i);
			part[i] = 0;
			if (!is_dir(part))
				make_one_dir(part);
		}
	}
	return is_dir(path);
}

/* -- finding the folders --------------------------------------------------- */

#if defined(_WIN32)
/* Documents\My Games\GLFrontier\ (follows Documents if it was moved, e.g. to OneDrive) */
static void find_dirs(void)
{
	wchar_t docs[MAX_PATH];
	char utf8[PATH_LEN - 32];
	if (SHGetFolderPathW(NULL, CSIDL_PERSONAL | CSIDL_FLAG_CREATE, NULL, SHGFP_TYPE_CURRENT, docs) != S_OK ||
		!WideCharToMultiByte(CP_UTF8, 0, docs, -1, utf8, sizeof(utf8), NULL, NULL))
		return;
	snprintf(data_dir, sizeof(data_dir), "%s\\My Games\\GLFrontier\\", utf8);
	snprintf(config_dir, sizeof(config_dir), "%s", data_dir);
}
#else
static const char *home_dir(void)
{
	const char *home = getenv("HOME");
	if (home && *home)
		return home;
	struct passwd *pw = getpwuid(getuid());
	return pw ? pw->pw_dir : NULL;
}

#if defined(__APPLE__)
/* ~/Library/Application Support/GLFrontier/ */
static void find_dirs(void)
{
	const char *home = home_dir();
	if (!home)
		return;
	snprintf(data_dir, sizeof(data_dir), "%s/Library/Application Support/GLFrontier/", home);
	snprintf(config_dir, sizeof(config_dir), "%s", data_dir);
}
#else
/* $env/glfrontier/, or ~/fallback/glfrontier/ when $env is unset or not
   absolute (the XDG base directory spec) */
static void xdg_dir(const char *env, const char *fallback, char *out, size_t size)
{
	const char *base = getenv(env);
	if (base && base[0] == '/')
	{
		snprintf(out, size, "%s/glfrontier/", base);
		return;
	}
	const char *home = home_dir();
	if (home)
		snprintf(out, size, "%s/%s/glfrontier/", home, fallback);
}

static void find_dirs(void)
{
	xdg_dir("XDG_DATA_HOME", ".local/share", data_dir, sizeof(data_dir));
	xdg_dir("XDG_CONFIG_HOME", ".config", config_dir, sizeof(config_dir));
}
#endif
#endif

/* -- older versions kept everything in the working directory --------------- */

/* Copies from into to, unless to is there already */
static void copy_file(const char *from, const char *to)
{
	FILE *out = user_fopen(to, "rb");
	if (out)
	{
		fclose(out);
		return;
	}
	FILE *in = fopen(from, "rb");
	if (!in)
		return;
	out = user_fopen(to, "wb");
	if (out)
	{
		char buf[16384];
		size_t n;
		bool ok = true;
		while (ok && (n = fread(buf, 1, sizeof(buf), in)) > 0)
			ok = fwrite(buf, 1, n, out) == n;
		ok = (fclose(out) == 0) && ok;
		if (ok)
			log_printf("Copied %s to %s\n", from, to);
		else
			user_remove(to);
	}
	fclose(in);
}

/* The saves in ./savs, copied (not moved) so an older build still finds them */
static void copy_old_saves(const char *to_dir)
{
	char to[PATH_LEN * 2];
#ifdef _WIN32
	WIN32_FIND_DATAA fd;
	HANDLE h = FindFirstFileA("savs\\*", &fd);
	if (h == INVALID_HANDLE_VALUE)
		return;
	do
	{
		if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			continue;
		char from[MAX_PATH + 8];
		snprintf(from, sizeof(from), "savs\\%s", fd.cFileName);
		snprintf(to, sizeof(to), "%s%s", to_dir, fd.cFileName);
		copy_file(from, to);
	} while (FindNextFileA(h, &fd));
	FindClose(h);
#else
	DIR *d = opendir("savs");
	if (!d)
		return;
	struct dirent *e;
	while ((e = readdir(d)))
	{
		char from[PATH_LEN];
		snprintf(from, sizeof(from), "savs/%s", e->d_name);
		struct stat st;
		if (stat(from, &st) != 0 || !S_ISREG(st.st_mode))
			continue;
		snprintf(to, sizeof(to), "%s%s", to_dir, e->d_name);
		copy_file(from, to);
	}
	closedir(d);
#endif
}

/* Only on the first run (no savs folder yet), so a save deleted later is not
   brought back from the old folder */
static void copy_old_files(void)
{
	char path[PATH_LEN + 64];
	snprintf(path, sizeof(path), "%ssavs/", data_dir);
	if (is_dir(path))
		return;
	snprintf(path, sizeof(path), "%sglfrontier.cfg", config_dir);
	copy_file("glfrontier.cfg", path);
	snprintf(path, sizeof(path), "%sFE2Galaxy.cache", data_dir);
	copy_file("FE2Galaxy.cache", path);
	snprintf(path, sizeof(path), "%ssavs/", data_dir);
	if (make_dirs(path))
		copy_old_saves(path);
}

/* -- the public calls ------------------------------------------------------ */

void user_dirs_init(void)
{
	if (ready)
		return;
	ready = true;
	find_dirs();
	if (data_dir[0] && !make_dirs(data_dir))
	{
		log_printf("Could not make %s, saving to the working directory\n", data_dir);
		data_dir[0] = 0;
	}
	if (config_dir[0] && !make_dirs(config_dir))
	{
		log_printf("Could not make %s, saving settings to the working directory\n", config_dir);
		config_dir[0] = 0;
	}
	log_printf("Saves in %s, settings in %s\n", data_dir[0] ? data_dir : "./", config_dir[0] ? config_dir : "./");
	if (data_dir[0] && config_dir[0])
		copy_old_files();
}

const char *user_data_dir(void)
{
	user_dirs_init();
	return data_dir;
}

const char *user_config_dir(void)
{
	user_dirs_init();
	return config_dir;
}

#else
typedef int user_dir_unused; /* Android and web keep their own folders */
#endif
