/*
 * galaxy_labels.h - the game routines and data the galaxy atlas uses, by
 * their RAM addresses from the labels header tools/build_fe2.py writes
 * (the unnamed ones are exported through its EXTRA_LABELS). Both builds of
 * the game have them all.
 */
#ifndef GALAXY_LABELS_H
#define GALAXY_LABELS_H

#ifndef FE2_USE_MODDED
#define FE2_USE_MODDED 1
#endif
#if FE2_USE_MODDED
#include "fe2_labels.h"
#else
#include "fe2_orig_labels.h"
#endif

/* setup and machine */
#define G_a6_base FE2_a6_base
#define G_a5_jumptab FE2_a5_jumptab
#define G_SetupA6Jumptab FE2_SetupA6Jumptab
#define G_UseMainGameData FE2_UseMainGameData
#define G_logscreen FE2_logscreen
#define G_physcreen FE2_physcreen
#define G_logscreen2 FE2_logscreen2
#define G_physcreen2 FE2_physcreen2
#define G_rts FE2_L6c_rts /* an rts to send unwanted calls to */

/* strings and palettes */
#define G_GetFmtStr FE2_GetFmtStr
#define G_SetDefaultPalette FE2_SetDefaultPalette

/* the galaxy map (module 28(a6)) */
#define G_StarSystemsViewDrawSystems FE2_StarSystemsViewDrawSystems /* a sector's stars */
#define G_core_systems_sector FE2_core_systems_sector				 /* the hand placed sectors */
#define G_core_specials FE2_L84616									 /* their special objects */
#define G_GetSystem FE2_L83b46										 /* fn 32 */
#define G_StarPosition FE2_L83d3c
#define G_MapString FE2_L84700 /* fn 40 */
#define G_Syllables FE2_L84830
#define G_SelectSystem FE2_L849a2
#define G_Distance FE2_L84ae6
#define G_Density FE2_L8ad0e
#define G_galaxy_bmp FE2_galaxy_bmp
#define G_ScreenTrade FE2_l84b34
#define G_ScreenSystem FE2_l84c04
#define G_ScreenBody FE2_l84cde
#define G_ScreenBodies FE2_l84f02

/* the system info module (60(a6)) fn 32 */
#define G_SystemInfo FE2_L4beee

#endif /* GALAXY_LABELS_H */
