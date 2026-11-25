#include "platform/subtitles.h"
#include "port/subtitles.h"

#include <SDL3/SDL.h>

#include "audio/irx.h"
#include "audio/spu2.h"
#include "game/font3d.h"
#include "game/gametypes.h"
#include "game/main.retail.h"
#include "game/panel.retail.h"
#include "nu3d/nucamera.h"
#include "numath/numtx.h"
#include "platform/language.h"
#include "platform/log.h"
#include "port/font3d.h"

extern struct game_s Game;
extern s32 Level, cutmovie, new_level, new_mode;
extern s32 Paused, screendump, save_paused;
extern s32 loadthread_id;
extern struct nucamera_s *pNuCam;
void MaxVP(void);
void GameVP(void);
void LoadFont3D(void);
void NuCameraSet(struct nucamera_s *camera);
void NuRndrClear(int flags, int colour, float depth);
void Text3D(char *txt, f32 x, f32 y, f32 z, f32 sx, f32 sy, f32 sz, s32 align, s32 colour);

static float nw_subtitles_hud_top = -1.0f;

void nw_subtitles_hud_line(float y)
{
    nw_subtitles_hud_top = SDL_max(nw_subtitles_hud_top, y);
}

static const char *nw_subtitles_current(void)
{
    if (!nw_subtitles_enabled(cutmovie == 0) || loadthread_id != 0) {
        return NULL;
    }
    const NwIrx *audio = nw_nusound_irx();
    int language = nw_language_preference() == NW_LANGUAGE_PT_BR ? NW_LANGUAGE_PT_BR : Game.language + NW_LANGUAGE_EN;
    for (int i = 0; i < NW_IRX_CHANNELS; i++) {
        const STREAMDATA *voice = &audio->StreamBuffer[i];
        if (!voice->Status || voice->iStatus != 0 || voice->Channel >= NW_IRX_CHANNELS ||
            audio->SPU_KeyStatus[voice->Channel] != 1 || nw_spu2_voice_lead_in(voice->Channel)) {
            continue;
        }
        for (int j = 0; j < audio->fileinfo_cnt; j++) {
            const NwIrxFilename *file = &audio->ADPCM_FileNames[j];
            if (file->ID != voice->ID || SDL_strncasecmp(file->Filename, "SFX\\VCE_", 8)) {
                continue;
            }
            const char *name = SDL_strrchr(file->Filename, '\\');
            uint32_t rate = (uint16_t)file->Pitch;
            if (name && rate) {
                /* The SPU starts counting at key-on, including after a stream seek. */
                uint64_t samples = (uint64_t)voice->SampleOffset + nw_spu2_voice_samples(voice->Channel);
                uint32_t ms = (uint32_t)(samples * 1000 / rate);
                const char *text = nw_subtitles_at(name + 1, language, ms);
                if (text) {
                    return text;
                }
            }
            break;
        }
    }
    return NULL;
}

static void nw_subtitles_draw(void)
{
    if (cutmovie < 0 && (screendump ? save_paused : Paused)) {
        return;
    }
    const char *text = nw_subtitles_current();
    if (text && pNuCam) {
        static const char *last;
        static NwSubtitleLayout layout;
        static char first_line[NW_SUBTITLE_LINE_SIZE + 4];
        if (text != last) {
            last = text;
            if (!nw_subtitles_layout(text, &layout)) {
                nw_log_error(NW_LOG_PORT, "subtitles: cannot render Text3D line: %s", text);
                SDL_strlcpy(layout.line[0], "TEXT ERROR", sizeof(layout.line[0]));
                layout.count = 1;
                layout.scale = 0.34f;
            }
            if (layout.speaker_length) {
                /* Text3D ignores its colour controls when measuring and centring the line. */
                SDL_snprintf(first_line, sizeof(first_line), "#b%.*s#o%s",
                    layout.speaker_length, layout.line[0], layout.line[0] + layout.speaker_length);
            }
        }
        struct numtx_s camera = pNuCam->mtx;
        /* Finish the scene and clear only its depth before drawing the text. */
        NuRndrClear(2, 0, 1.0f);
        MaxVP();
        NuMtxSetIdentity(&pNuCam->mtx);
        NuCameraSet(pNuCam);
        /* Vehicle health and objectives occupy the bottom of gameplay's HUD. */
        float bottom = cutmovie < 0 && Level != 0x25 && Level != 0x26 && Level != 0x28 ? -0.50f : -0.88f;
        if (cutmovie < 0) {
            bottom = SDL_max(bottom, nw_subtitles_hud_top + 0.10f);
        }
        float y = bottom + (layout.count - 1) * 0.10f;
        bool accents = nw_font3d_subtitle_accents(true);
        for (int i = 0; i < layout.count; i++) {
            char *line = i == 0 && layout.speaker_length ? first_line : layout.line[i];
            Text3D(line, 0.0f, y, 1.0f, layout.scale, layout.scale, layout.scale, 0, 0);
            y -= 0.10f;
        }
        nw_font3d_subtitle_accents(accents);
        pNuCam->mtx = camera;
        NuCameraSet(pNuCam);
        GameVP();
    }
}

/* Movies and gameplay have separate drawing loops. */
void DrawFade(void)
{
    if (cutmovie >= 0) {
        nw_subtitles_draw();
    }
    DrawFade__retail();
}

void DrawPanel(void)
{
    nw_subtitles_hud_top = -1.0f;
    DrawPanel__retail();
    if (cutmovie < 0 && new_level == -1 && new_mode == -1) {
        nw_subtitles_draw();
    }
}

void LoadLevel(void)
{
    LoadLevel__retail();
    /* Space dialogs omit the HUD font. Keep it available for the subtitle toggle. */
    if (Level == 0x28) {
        LoadFont3D();
    }
}
