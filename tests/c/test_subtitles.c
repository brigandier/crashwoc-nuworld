#include <stdio.h>
#include <string.h>

#include <SDL3/SDL.h>

#include "audio/spu2.h"
#include "platform/subtitles.h"

static int failures;
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "%d: %s\n", __LINE__, #c); failures++; } } while (0)

static void playback(void)
{
    nw_subtitles_enable(false, true);
    CHECK(!nw_subtitles_enabled(false) && !nw_subtitles_enabled(true));
    nw_subtitles_enable(true, false);
    CHECK(nw_subtitles_enabled(false) && !nw_subtitles_enabled(true));
    nw_subtitles_enable(true, true);
    CHECK(nw_subtitles_enabled(false) && nw_subtitles_enabled(true));

    NwSubtitleLayout layout;
    CHECK(nw_subtitles_layout("x o s t q u / Straße", &layout));
    CHECK(!strcmp(layout.line[0], "X O S T Q U / STRASSE"));
    CHECK(!nw_subtitles_layout("", &layout));
    CHECK(!nw_subtitles_layout("   ", &layout));
    CHECK(!nw_subtitles_layout("#o", &layout));
    CHECK(!nw_subtitles_layout("🙂", &layout));
    CHECK(nw_subtitles_layout("Lo-Lo : C'est donc ça, le héros que je dois balayer ?", &layout));
    CHECK(layout.count == 2 && !strcmp(layout.line[1], "BALAYER ?"));
    CHECK(nw_subtitles_layout("Cortex: Do you hear me?", &layout));
    float normal = layout.scale;
    CHECK(nw_subtitles_layout("Cortex: DO YOU HEAR ME?", &layout));
    CHECK(layout.scale > normal);

    nw_spu2_init();
    uint64_t cycle = nw_spu2_cycle();
    nw_spu2_set_param(NW_SD_VP_PITCH | (4 << 1), 0x1000);
    nw_spu2_run_to(cycle + 768 * 100);
    CHECK(nw_spu2_voice_samples(4) == 100);
    nw_spu2_set_param(NW_SD_VP_PITCH | (4 << 1), 0); /* the existing audio pause */
    nw_spu2_run_to(cycle + 768 * 200);
    CHECK(nw_spu2_voice_samples(4) == 100);
    nw_spu2_set_param(NW_SD_VP_PITCH | (4 << 1), 0x1000);
    nw_spu2_set_switch(NW_SD_S_KON, 1u << 4);
    nw_spu2_run_to(cycle + 768 * 210);
    CHECK(nw_spu2_voice_samples(4) == 10);
    CHECK(!nw_spu2_voice_samples(48));
}

int main(void)
{
    playback();
    return failures ? 1 : 0;
}
