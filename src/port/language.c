#include "port/language.h"

#include <SDL3/SDL.h>

#include "game/gametypes.h"
#include "platform/language.h"

extern struct game_s Game;
extern s32 LANGUAGEOPTION;
extern char *LanguageName[6];
extern char *LevelName[44][6];
void NewLanguage(s32 lang);

/* Point only English's menu strings at the extra locale while it is selected.
 * The six original tables keep their dimensions and English is restored on exit. */
#define NW_LANGUAGE_DECLARE
#include "language_tables.inc"
#undef NW_LANGUAGE_DECLARE

static struct { char **slot; char *english; } nw_language_tables[] = {
#include "language_tables.inc"
};
static bool nw_language_captured;
static int nw_language_applied = -1;

char *nw_language_game_render_text(char *text)
{
    if (nw_language_preference() == NW_LANGUAGE_PT_BR) {
        for (int i = 0; i < 44; i++) {
            if (text == LevelName[i][0]) {
                return nw_language_game_text(text);
            }
        }
        for (int i = 0; i < 6; i++) {
            if (text == HubName[i][0]) {
                return nw_language_game_text(text);
            }
        }
        if (text == tWARPROOM[0]) {
            return nw_language_game_text(text);
        }
    }
    return text;
}

int nw_language_game_count(void)
{
    return LANGUAGEOPTION ? 7 : 2;
}

int nw_language_game_row(int row)
{
    return !LANGUAGEOPTION && row == 1 ? 6 : row;
}

int nw_language_game_selected(void)
{
    return nw_language_preference() == NW_LANGUAGE_PT_BR ? 6 : Game.language;
}

char *nw_language_game_name(int language)
{
    if (language == 6) {
        return "PORTUGU\322S (BR)";
    }
    return LanguageName[language >= 0 && language < 6 ? language : 0];
}

int nw_language_game_apply(int retail_language)
{
    int pref = nw_language_preference();
    if (!nw_language_captured) {
        for (size_t i = 0; i < SDL_arraysize(nw_language_tables); i++) {
            nw_language_tables[i].english = *nw_language_tables[i].slot;
        }
        nw_language_captured = true;
    }
    for (size_t i = 0; i < SDL_arraysize(nw_language_tables); i++) {
        char *en = nw_language_tables[i].english;
        *nw_language_tables[i].slot = pref == NW_LANGUAGE_PT_BR ? nw_language_game_text(en) : en;
    }
    nw_language_applied = pref;
    if (pref == NW_LANGUAGE_PT_BR) {
        return 0; /* English voices and all original table lookups. */
    }
    if (pref != NW_LANGUAGE_DISC) {
        retail_language = pref - NW_LANGUAGE_EN;
    }
    return LANGUAGEOPTION && retail_language >= 0 && retail_language < 6 ? retail_language : 0;
}

void nw_language_game_select(int language)
{
    if (language < 0 || language > 6 || (!LANGUAGEOPTION && language != 0 && language != 6)) {
        return;
    }
    nw_language_request(language + NW_LANGUAGE_EN);
    NewLanguage(language < 6 ? language : 0);
}

void nw_language_game_poll(void)
{
    if (nw_language_applied != nw_language_preference()) {
        NewLanguage(Game.language);
    }
}
