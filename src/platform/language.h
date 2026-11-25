/* Native language preference. Retail saves and voice tables keep their six IDs. */
#ifndef NUWORLD_PLATFORM_LANGUAGE_H
#define NUWORLD_PLATFORM_LANGUAGE_H

#include <stddef.h>

typedef enum NwLanguage {
    NW_LANGUAGE_DISC,
    NW_LANGUAGE_EN,
    NW_LANGUAGE_FR,
    NW_LANGUAGE_DE,
    NW_LANGUAGE_IT,
    NW_LANGUAGE_ES,
    NW_LANGUAGE_NL,
    NW_LANGUAGE_PT_BR,
    NW_LANGUAGES,
} NwLanguage;

extern const char *const nw_language_keys[NW_LANGUAGES];
/* Atomic, callable by the main and game threads. DISC follows the save. */
void nw_language_request(int language);
int nw_language_preference(void);
/* Immutable UTF-8 for the native UI; unknown strings fall back unchanged. */
const char *nw_language_ui_text(const char *text);
/* Game thread only: cached strings in Text3D's single-byte glyph encoding. */
char *nw_language_game_text(const char *text);
/* Returns 0 on unsupported characters or insufficient space; never truncates. */
size_t nw_language_encode(char *out, size_t size, const char *utf8);

#endif
