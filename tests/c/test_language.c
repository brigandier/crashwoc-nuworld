#include <stdio.h>
#include <string.h>

#include <SDL3/SDL.h>

#include "game/gametypes.h"
#include "platform/language.h"
#include "port/language.h"
#include "save/savecodec.h"

extern struct game_s Game;
extern s32 LANGUAGEOPTION;
extern char *tNEWGAME[6], *tOPTIONS[6], *LevelName[44][6];
extern struct powerdata_s PData[6];
#define NW_LANGUAGE_DECLARE
#include "port/language_tables.inc"
#undef NW_LANGUAGE_DECLARE
static struct { char **slot; char *english; } game_texts[] = {
#include "port/language_tables.inc"
};
void NewLanguage(s32 lang);
s32 RemapAccentedCharacter(char *c);

static int failures;
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "%d: %s\n", __LINE__, #c); failures++; } } while (0)

static const struct { const char *source, *text; bool text3d; } catalog[] = {
#define NW_TEXT(source, translation) { source, translation, false },
#define NW_GAME_TEXT(source, translation) { source, translation, true },
#include "platform/pt_br.inc"
#undef NW_GAME_TEXT
#undef NW_TEXT
};

/* Labels also contain literal percentages. Only consume complete printf tokens. */
static size_t format_token(const char *s)
{
    if (*s != '%') return 0;
    size_t n = 1;
    if (s[n] == '%') return 2;
    while (s[n] && strchr("+-#0.123456789*hljztL", s[n])) n++;
    return s[n] && strchr("diuoxXfFeEgGaAcspn", s[n]) ? n + 1 : 0;
}

static bool same_formats(const char *a, const char *b)
{
    for (;;) {
        size_t na = 0, nb = 0;
        while (*a && !(na = format_token(a))) a++;
        while (*b && !(nb = format_token(b))) b++;
        if (na != nb || (na && strncmp(a, b, na))) return false;
        if (!na) return true;
        a += na;
        b += nb;
    }
}

static void check_catalog(void)
{
    CHECK(!same_formats("%s %i", "%i %s"));
    CHECK(!same_formats("%04d %%", "%d %%"));
    CHECK(!same_formats("%s", "%s %s"));
    CHECK(same_formats("Speed % of PS2", "Velocidade % do PS2"));
    for (size_t i = 0; i < SDL_arraysize(catalog); i++) {
        const char *source = catalog[i].source, *text = catalog[i].text;
        if (!same_formats(source, text)) {
            fprintf(stderr, "Format mismatch: %s -> %s\n", source, text);
            failures++;
        }
        const char *id = strstr(source, "###");
        CHECK(!id || (strstr(text, "###") && !strcmp(id, strstr(text, "###"))));
        for (size_t j = 0; j < i; j++) CHECK(strcmp(source, catalog[j].source));
        if (catalog[i].text3d) {
            char encoded[256];
            if (!nw_language_encode(encoded, sizeof(encoded), text)) {
                fprintf(stderr, "Text3D encoding failed: %s\n", source);
                failures++;
            }
            /* Lowercase letters in Text3D are button/model controls, not prose. */
            int controls[128] = { 0 };
            const char *strings[] = { source, text };
            for (int side = 0; side < 2; side++) {
                for (const char *p = strings[side]; *p;) {
                    size_t n = format_token(p);
                    if (n) { p += n; continue; }
                    if (*p >= 'a' && *p <= 'z') controls[(unsigned char)*p] += side ? -1 : 1;
                    p++;
                }
            }
            for (int c = 'a'; c <= 'z'; c++) CHECK(!controls[c]);
        }
    }
}

int main(void)
{
    check_catalog();
    char glyphs[32];
    CHECK(nw_language_encode(glyphs, sizeof(glyphs), "ÁÀÂÃÇÉÊÍÓÔÕÚÜ") == 13);
    for (int i = 0; i < 13; i++) {
        char c = glyphs[i];
        CHECK(RemapAccentedCharacter(&c) >= 0);
    }
    CHECK(nw_language_encode(glyphs, sizeof(glyphs), "1º") == 2);
    CHECK(glyphs[0] == '1' && glyphs[1] == (char)0xf8);
    CHECK(nw_language_encode(glyphs, 3, "OPÇÕES") == 0 && !glyphs[0]);
    CHECK(nw_language_encode(glyphs, sizeof(glyphs), "🙂") == 0 && !glyphs[0]);
    CHECK(nw_language_encode(glyphs, sizeof(glyphs), "APERTE x / q") == 12);

    LANGUAGEOPTION = 1;
    nw_language_request(NW_LANGUAGE_DISC);
    NewLanguage(0);
    char *english = tNEWGAME[0], *french = tNEWGAME[1], *level = LevelName[7][0];
    for (size_t i = 0; i < SDL_arraysize(game_texts); i++) game_texts[i].english = *game_texts[i].slot;
    CHECK(nw_language_game_count() == 7);
    nw_language_game_select(6);
    CHECK(Game.language == 0 && Game.language < NW_SAVE_LANGUAGES);
    CHECK(nw_language_game_selected() == 6);
    CHECK(strcmp(tNEWGAME[0], "NOVO JOGO") == 0 && tNEWGAME[1] == french);
    CHECK(strcmp(nw_language_ui_text("Settings"), "Configurações") == 0);
    CHECK(LevelName[7][0] == level && nw_language_game_render_text(level) != level);
    CHECK(strcmp(tWARPROOM[0], "WARP ROOM") == 0 && nw_language_game_render_text(tWARPROOM[0]) != tWARPROOM[0]);
    CHECK(nw_language_game_render_text(HubName[0][0]) != HubName[0][0]);
    for (size_t i = 0; i < SDL_arraysize(game_texts); i++) {
        const char *text = *game_texts[i].slot;
        if (*text && text == game_texts[i].english) {
            fprintf(stderr, "Missing game translation: %s\n", text);
            failures++;
        }
    }
    CHECK(strstr(PData[0].text[1], "u") != NULL); /* bound shoulder button stays a glyph */
    NewLanguage(4); /* loading a Spanish save cannot turn off the explicit PT-BR preference */
    CHECK(Game.language == 0 && nw_language_game_selected() == 6);

    nw_language_game_select(0);
    CHECK(tNEWGAME[0] == english && LevelName[7][0] == level);
    CHECK(strcmp(nw_language_ui_text("Settings"), "Settings") == 0);
    for (int language = 0; language < 6; language++) {
        nw_language_game_select(language);
        CHECK(Game.language == language && nw_language_game_selected() == language);
        CHECK(tNEWGAME[0] == english && tNEWGAME[1] == french);
    }
    nw_language_request(NW_LANGUAGE_DISC);
    NewLanguage(4);
    CHECK(Game.language == 4);

    LANGUAGEOPTION = 0;
    CHECK(nw_language_game_count() == 2 && nw_language_game_row(1) == 6);
    nw_language_game_select(6);
    CHECK(Game.language == 0 && nw_language_game_selected() == 6);
    nw_language_game_select(1); /* an English-only disc does not expose missing voices */
    CHECK(nw_language_game_selected() == 6);
    nw_language_game_select(0);
    CHECK(Game.language == 0 && tNEWGAME[0] == english);
    nw_language_request(999);
    CHECK(nw_language_preference() == NW_LANGUAGE_DISC);
    NewLanguage(255);
    CHECK(Game.language == 0);
    return failures ? 1 : 0;
}
