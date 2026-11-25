#ifndef NUWORLD_PORT_LANGUAGE_H
#define NUWORLD_PORT_LANGUAGE_H

/* All of these run on the game thread. Menu rows are original IDs 0..5, then PT-BR (6). */
int nw_language_game_count(void);
int nw_language_game_row(int row);
int nw_language_game_selected(void);
char *nw_language_game_name(int language);
void nw_language_game_select(int language);
void nw_language_game_poll(void);
/* Called by NewLanguage after the data image is loaded. Never writes ID 6 into Game. */
int nw_language_game_apply(int retail_language);
/* Render shared names without modifying their original UTF-8/ASCII consumers. */
char *nw_language_game_render_text(char *text);

#endif
