#ifndef GAME_H
#define GAME_H
#include "input.h"

void game_init(void);
void game_frame(const pad_t *pad, float dt);

#ifdef PC_PREVIEW
void game_debug_scene(int scene);
#endif

#endif
