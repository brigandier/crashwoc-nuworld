#ifndef NUWORLD_PLATFORM_SUBTITLES_H
#define NUWORLD_PLATFORM_SUBTITLES_H

#include <stdbool.h>
#include <stdint.h>

/* Same order as Game.language and the original voice tables. */
enum {
    NW_SUBTITLE_EN, NW_SUBTITLE_FR, NW_SUBTITLE_DE, NW_SUBTITLE_IT, NW_SUBTITLE_ES, NW_SUBTITLE_NL,
    NW_SUBTITLE_LANGUAGES,
};

/* Immutable UTF-8, identified by the original voice filename. */
const char *nw_subtitles_at(const char *voice, int language, uint32_t ms);
/* Preference from the main thread, read by the game's renderer. */
void nw_subtitles_enable(bool enabled, bool title);
bool nw_subtitles_enabled(bool title);

#define NW_SUBTITLE_LINES 3
#define NW_SUBTITLE_LINE_SIZE 256
typedef struct NwSubtitleLayout {
    char line[NW_SUBTITLE_LINES][NW_SUBTITLE_LINE_SIZE];
    int count, speaker_length; /* the first line, including the colon */
    float scale;
} NwSubtitleLayout;

/* Uppercase Text3D glyphs, word wrapping and scaling. */
bool nw_subtitles_layout(const char *utf8, NwSubtitleLayout *out);

#endif
