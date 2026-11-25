#ifndef NUWORLD_PORT_FONT3D_H
#define NUWORLD_PORT_FONT3D_H

#include "platform/prompts.h"

#define NW_FONT3D_GREY 5
#define NW_FONT3D_SAVING 6

/* Panel camera: small autosave disk, with a soft pulse and one glint while saving. */
void nw_font3d_autosave(float x, float y, float size, bool saving);

/* Any thread: the glyphs Text3D draws from its next call; retail's until the first. */
void nw_font3d_prompts(const NwPrompts *p);
/* NuMtlRelease: the icons' materials and textures went with the level's. */
void nw_font3d_release(void);

#endif
