/* Game thread owns the session and its snapshots; a worker only writes an immutable snapshot. */
#ifndef NUWORLD_PORT_AUTOSAVE_H
#define NUWORLD_PORT_AUTOSAVE_H

#include "save/savestore.h"

struct game_s;

typedef enum NwAutosaveStatus {
    NW_AUTOSAVE_IDLE,
    NW_AUTOSAVE_WRITING,
    NW_AUTOSAVE_COMPLETE,
    NW_AUTOSAVE_FAILED,
    NW_AUTOSAVE_RECOVERED,
} NwAutosaveStatus;

void nw_autosave_init(const char *dir);
void nw_autosave_new(bool enabled);
void nw_autosave_loaded(const NwSaveMeta *meta, const struct game_s *game, const NwSaveNames *names, bool recovered);
void nw_autosave_checkpoint(bool rewards);
void nw_autosave_poll(void);
/* Also callable by the host once the game thread is parked. */
void nw_autosave_finish(void);
void nw_autosave_leave(void);
const NwSaveMeta *nw_autosave_meta(void);
bool nw_autosave_enabled(void);
bool nw_autosave_active(void);
bool nw_autosave_rollback(void);
bool nw_autosave_loading(void);
/* false: enabling needs rollback confirmation; no write has taken place. */
bool nw_autosave_set(bool enabled);
void nw_autosave_accept_rollback(bool enabled);
NwAutosaveStatus nw_autosave_status(void);
/* The notification's lifetime; the renderer handles its pulse without hiding the text. */
bool nw_autosave_notice_visible(uint64_t now);
void nw_autosave_notice_presented(void);
void nw_autosave_recovered(void);

#endif
