#include "platform/language.h"

#include <SDL3/SDL.h>

#include "platform/log.h"

const char *const nw_language_keys[NW_LANGUAGES] = { "disc", "en", "fr", "de", "it", "es", "nl", "pt-BR" };
static SDL_AtomicInt nw_language;

typedef struct NwTranslation {
    const char *source, *utf8;
    bool text3d;
    char game[256];
} NwTranslation;

static NwTranslation nw_pt_br[] = {
#define NW_TEXT(source, translation) { source, translation, false, { 0 } },
#define NW_GAME_TEXT(source, translation) { source, translation, true, { 0 } },
#include "pt_br.inc"
#undef NW_GAME_TEXT
#undef NW_TEXT
};

void nw_language_request(int language)
{
    SDL_SetAtomicInt(&nw_language, language >= 0 && language < NW_LANGUAGES ? language : NW_LANGUAGE_DISC);
}

int nw_language_preference(void)
{
    return SDL_GetAtomicInt(&nw_language);
}

static NwTranslation *nw_language_find(const char *text)
{
    if (text && nw_language_preference() == NW_LANGUAGE_PT_BR) {
        for (size_t i = 0; i < SDL_arraysize(nw_pt_br); i++) {
            if (SDL_strcmp(text, nw_pt_br[i].source) == 0) {
                return &nw_pt_br[i];
            }
        }
    }
    return NULL;
}

const char *nw_language_ui_text(const char *text)
{
    if (text && nw_language_preference() == NW_LANGUAGE_PT_BR) {
        for (size_t i = 0; i < SDL_arraysize(nw_pt_br); i++) {
            if (text == nw_pt_br[i].game) {
                return nw_pt_br[i].utf8;
            }
        }
    }
    NwTranslation *t = nw_language_find(text);
    return t ? t->utf8 : text;
}

size_t nw_language_encode(char *out, size_t size, const char *utf8)
{
    /* RemapAccentedCharacter: cedilla, diaeresis, tilde, circumflex, acute, grave. */
    static const struct { unsigned char utf8, glyph; } accents[] = {
        { 0x87, 0x80 }, { 0xa7, 0x87 }, /* C cedilla */
        { 0x83, 0xc7 }, { 0xa3, 0xc6 }, /* A tilde */
        { 0x95, 0xe5 }, { 0xb5, 0xe4 }, /* O tilde */
        { 0x81, 0xb5 }, { 0xa1, 0xa0 }, { 0x89, 0x90 }, { 0xa9, 0x82 },
        { 0x8d, 0xd6 }, { 0xad, 0xa1 }, { 0x93, 0xe0 }, { 0xb3, 0xa2 },
        { 0x9a, 0xe9 }, { 0xba, 0xa3 }, /* acute vowels */
        { 0x82, 0xb6 }, { 0xa2, 0x83 }, { 0x8a, 0xd2 }, { 0xaa, 0x88 },
        { 0x94, 0xe2 }, { 0xb4, 0x93 }, /* circumflex A, E, O */
        { 0x80, 0xb7 }, { 0xa0, 0x85 }, /* A grave */
        { 0x9c, 0x9a }, { 0xbc, 0x81 }, /* U diaeresis */
        { 0x84, 0x8e }, { 0xa4, 0x84 }, { 0x8b, 0xd3 }, { 0xab, 0x89 },
        { 0x8f, 0xd8 }, { 0xaf, 0x8b }, { 0x96, 0x99 }, { 0xb6, 0x94 }, /* diaeresis */
        { 0x91, 0xa5 }, { 0xb1, 0xa4 }, /* N tilde */
        { 0x8e, 0xd7 }, { 0xae, 0x8c }, { 0x9b, 0xea }, { 0xbb, 0x96 }, /* circumflex I, U */
        { 0x88, 0xd4 }, { 0xa8, 0x8a }, { 0x8c, 0xde }, { 0xac, 0x8d },
        { 0x92, 0xe3 }, { 0xb2, 0x95 }, { 0x99, 0xeb }, { 0xb9, 0x97 }, /* grave E, I, O, U */
    };
    size_t n = 0;
    if (!size) {
        return 0;
    }
    out[0] = '\0';
    for (const unsigned char *s = (const unsigned char *)utf8; *s; s++) {
        unsigned char c = *s;
        if (c == 0xc2 && s[1] == 0xba) {
            s++;
            c = 0xf8; /* the original ordinal model */
        } else if (c == 0xc2 && (s[1] == 0xa1 || s[1] == 0xbf)) {
            c = *++s == 0xa1 ? 0xad : 0xa8; /* inverted punctuation */
        } else if (c >= 0x80) {
            if (c != 0xc3 || !s[1]) {
                out[0] = '\0';
                return 0;
            }
            c = 0;
            s++;
            for (size_t i = 0; i < SDL_arraysize(accents); i++) {
                if (*s == accents[i].utf8) {
                    c = accents[i].glyph;
                    break;
                }
            }
            if (!c) {
                out[0] = '\0';
                return 0;
            }
        }
        if (n + 1 >= size) {
            out[0] = '\0';
            return 0;
        }
        out[n++] = (char)c;
    }
    out[n] = '\0';
    return n;
}

char *nw_language_game_text(const char *text)
{
    NwTranslation *t = nw_language_find(text);
    if (!t) {
        return (char *)text;
    }
    if (!t->game[0] && (!t->text3d || !nw_language_encode(t->game, sizeof(t->game), t->utf8))) {
        nw_log_error(NW_LOG_PORT, "pt-BR: invalid Text3D translation for \"%s\": \"%s\"", t->source, t->utf8);
        SDL_strlcpy(t->game, "TEXT ERROR", sizeof(t->game));
    }
    return t->game;
}
