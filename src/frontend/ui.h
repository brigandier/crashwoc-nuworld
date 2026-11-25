/* Main thread only. */
#ifndef NUWORLD_FRONTEND_UI_H
#define NUWORLD_FRONTEND_UI_H

#include <stdbool.h>

#include <SDL3/SDL.h>

#include "platform/platform.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum NwUiFont {
    NW_UI_REGULAR,
    NW_UI_BOLD,
} NwUiFont;

bool nw_ui_init(NwPlatform *p, bool compact);
void nw_ui_shutdown(void);

void nw_ui_event(const SDL_Event *e);

void nw_ui_begin(void);
void nw_ui_end(void);

void nw_ui_push_font(NwUiFont font, float scale);
void nw_ui_pop_font(void);
/* Main-thread scratch storage: visible translation with the source as ImGui ID. */
const char *nw_ui_label(const char *label);

#ifdef __cplusplus
}
#endif

#endif
