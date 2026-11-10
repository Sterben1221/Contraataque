/* main.c - CONTRAATAQUE (homebrew PS2) */
#include "gfx.h"
#include "input.h"
#include "game.h"

#ifndef PC_PREVIEW
#include <kernel.h>
#include <sifrpc.h>
#include <loadfile.h>
#include <iopcontrol.h>

/* contador de ciclos del EE (294.912 MHz) para medir el tiempo de cada frame */
static inline unsigned int ee_ticks(void)
{
    unsigned int c;
    __asm__ __volatile__("mfc0 %0, $9" : "=r"(c));
    return c;
}

int main(int argc, char *argv[])
{
    pad_t pad;
    unsigned int last, now;
    (void)argc; (void)argv;

    /* reinicia el IOP para arrancar limpio (desde ISO o desde .elf) */
    SifInitRpc(0);
    while (!SifIopReset("", 0)) {}
    while (!SifIopSync()) {}
    SifInitRpc(0);
    SifLoadFileInit();

    input_init();
    gfx_init();
    game_init();

    last = ee_ticks();
    for (;;) {
        float dt;
        now = ee_ticks();
        dt = (float)(now - last) / 294912000.0f;
        last = now;
        if (dt <= 0.0f || dt > 0.1f) dt = 1.0f / 60.0f;
        input_update(&pad);
        game_frame(&pad, dt);
    }
    return 0;
}
#endif
