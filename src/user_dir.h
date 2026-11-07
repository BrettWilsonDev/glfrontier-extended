/*
 * user_dir.h - where the desktop builds keep the player's files.
 *
 *   Windows   Documents\My Games\GLFrontier\           saves, cache and glfrontier.cfg
 *   Linux     $XDG_DATA_HOME/glfrontier/   (~/.local/share/glfrontier/)   saves and cache
 *             $XDG_CONFIG_HOME/glfrontier/ (~/.config/glfrontier/)        glfrontier.cfg
 *   macOS     ~/Library/Application Support/GLFrontier/  everything
 *
 * Android and the web build keep their own places (internal storage, IndexedDB)
 * and do not use this. Paths are UTF-8 and end in a slash; "" (the working
 * directory) if the folder could not be found or made.
 */
#ifndef USER_DIR_H
#define USER_DIR_H

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Finds and makes the folders, and once copies over saves, settings and the
   galaxy cache left in the working directory by older versions */
void user_dirs_init(void);

const char *user_data_dir(void);   /* saves (savs/) and the galaxy cache */
const char *user_config_dir(void); /* glfrontier.cfg */

/* fopen / remove that take UTF-8 paths on Windows too */
FILE *user_fopen(const char *path, const char *mode);
int user_remove(const char *path);

#ifdef __cplusplus
}
#endif

#endif
