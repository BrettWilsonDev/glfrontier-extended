/*
 * gl_overlay.h - touch controls and debug overlays drawn on top of the game.
 */
#ifndef GL_OVERLAY_H
#define GL_OVERLAY_H

/* Queues the overlay into the gl_draw batch (window pixel coordinates). */
void gl_overlay_draw(void);

#endif /* GL_OVERLAY_H */
