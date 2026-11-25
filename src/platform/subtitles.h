#ifndef NUWORLD_PLATFORM_SUBTITLES_H
#define NUWORLD_PLATFORM_SUBTITLES_H

#include <stdbool.h>
#include <stdint.h>

/* Immutable UTF-8, identified by the original voice filename. */
const char *nw_subtitles_at(const char *voice, int language, uint32_t ms);
/* Preference from the main thread, read by the game's renderer. */
void nw_subtitles_enable(bool enabled, bool title);
bool nw_subtitles_enabled(bool title);

#define NW_SUBTITLE_LINES 3
#define NW_SUBTITLE_LINE_SIZE 256
typedef struct NwSubtitleLayout {
    char line[NW_SUBTITLE_LINES][NW_SUBTITLE_LINE_SIZE];
    int count;
    float scale;
} NwSubtitleLayout;

/* Uppercase Text3D glyphs, word wrapping and the autosave notice's maximum size. */
bool nw_subtitles_layout(const char *utf8, NwSubtitleLayout *out);

#endif
