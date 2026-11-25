#include "platform/subtitles.h"

#include <SDL3/SDL.h>

#include "platform/language.h"

typedef struct NwSubtitleCue {
    uint32_t start, end; /* milliseconds in the voice sample, not display frames */
    const char *text;
} NwSubtitleCue;

typedef struct NwSubtitleTrack {
    const char *voice;
    int language;
    uint32_t duration;
    const NwSubtitleCue *cues;
    size_t count;
} NwSubtitleTrack;

#include "subtitles.inc"

enum { NW_SUBTITLES_ON = 1, NW_SUBTITLES_TITLE = 2 };
static SDL_AtomicInt nw_subtitles_flags;

const char *nw_subtitles_at(const char *voice, int language, uint32_t ms)
{
    if (!voice) {
        return NULL;
    }
    for (size_t i = 0; i < SDL_arraysize(nw_subtitle_tracks); i++) {
        const NwSubtitleTrack *track = &nw_subtitle_tracks[i];
        if (track->language != language || SDL_strcasecmp(track->voice, voice)) {
            continue;
        }
        if (ms >= track->duration) {
            return NULL;
        }
        for (size_t j = 0; j < track->count; j++) {
            const NwSubtitleCue *cue = &track->cues[j];
            if (ms >= cue->start && ms < cue->end) {
                return cue->text;
            }
        }
        break;
    }
    return NULL;
}

void nw_subtitles_enable(bool enabled, bool title)
{
    SDL_SetAtomicInt(&nw_subtitles_flags, (enabled ? NW_SUBTITLES_ON : 0) | (title ? NW_SUBTITLES_TITLE : 0));
}

bool nw_subtitles_enabled(bool title)
{
    int required = title ? NW_SUBTITLES_ON | NW_SUBTITLES_TITLE : NW_SUBTITLES_ON;
    return (SDL_GetAtomicInt(&nw_subtitles_flags) & required) == required;
}

static bool nw_subtitles_upper(const char *text, char *out, size_t size)
{
    size_t n = 0;
    while (*text) {
        Uint32 c = SDL_StepUTF8(&text, NULL);
        if (c == 0xdf) { /* Uppercase sharp S is SS in the original font. */
            if (n + 2 >= size) {
                return false;
            }
            out[n++] = 'S';
            out[n++] = 'S';
            continue;
        }
        if ((c >= 'a' && c <= 'z') || (c >= 0xe0 && c <= 0xfe && c != 0xf7)) {
            c -= 0x20;
        }
        if (c < 0x20 || c == '#' || c == 0xfffd || n + 1 >= size) {
            return false;
        }
        char utf8[5], glyph[2];
        *SDL_UCS4ToUTF8(c, utf8) = '\0';
        if (!nw_language_encode(glyph, sizeof(glyph), utf8)) {
            return false;
        }
        out[n++] = glyph[0];
    }
    out[n] = '\0';
    return n != 0;
}

static float nw_subtitles_width(const char *text, size_t n)
{
    float width = 0;
    for (size_t i = 0; i < n; i++) {
        width += text[i] == '.' || text[i] == ':' ? 0.5f : 1.0f;
    }
    return width;
}

static bool nw_subtitles_wrap(const char *text, float cells, NwSubtitleLayout *out)
{
    out->count = 1;
    SDL_zeroa(out->line);
    float width = 0, widest = 0;
    size_t used = 0;
    while (*text) {
        if (*text == ' ') {
            text++;
            continue;
        }
        const char *end = SDL_strchr(text, ' ');
        /* French leaves a space before these signs, but they belong to the preceding word. */
        if (end && end[1] && SDL_strchr("!?;:", end[1])) {
            const char *after = end + 2;
            while (*after && SDL_strchr("!?;:", *after)) {
                after++;
            }
            if (!*after || *after == ' ') {
                end = after;
            }
        }
        size_t n = end ? (size_t)(end - text) : SDL_strlen(text);
        float word = nw_subtitles_width(text, n);
        if (used && width + 1 + word > cells) {
            widest = SDL_max(widest, width);
            if (out->count == NW_SUBTITLE_LINES) {
                return false;
            }
            out->count++;
            used = 0;
            width = 0;
        }
        if (used + (used != 0) + n >= NW_SUBTITLE_LINE_SIZE) {
            return false;
        }
        char *line = out->line[out->count - 1];
        if (used) {
            line[used++] = ' ';
            width++;
        }
        SDL_memcpy(line + used, text, n);
        used += n;
        line[used] = '\0';
        width += word;
        text += n;
    }
    widest = SDL_max(widest, width);
    if (widest <= 0) {
        return false;
    }
    out->scale = SDL_min(0.34f, 1.76f / (widest * 0.1f));
    return true;
}

bool nw_subtitles_layout(const char *utf8, NwSubtitleLayout *out)
{
    char text[512];
    SDL_zerop(out);
    if (!utf8 || !nw_subtitles_upper(utf8, text, sizeof(text))) {
        return false;
    }
    for (int cells = 52; cells <= 128; cells += 4) {
        if (nw_subtitles_wrap(text, (float)cells, out)) {
            const char *dialogue = SDL_strchr(utf8, ':');
            dialogue = dialogue ? dialogue + 1 : utf8;
            bool upper = false, lower = false;
            while (*dialogue) {
                Uint32 c = SDL_StepUTF8(&dialogue, NULL);
                upper |= (c >= 'A' && c <= 'Z') || (c >= 0xc0 && c <= 0xde && c != 0xd7);
                lower |= (c >= 'a' && c <= 'z') || (c >= 0xdf && c <= 0xff && c != 0xf7);
            }
            if (!upper || lower) {
                out->scale = SDL_min(out->scale, 0.30f);
            }
            return true;
        }
    }
    SDL_zerop(out);
    return false;
}
