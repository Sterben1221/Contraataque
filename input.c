/* input.c - lectura del control (DualShock 2) con libpad */
#include "input.h"
#include <string.h>

#ifdef PC_PREVIEW
void input_init(void) {}
void input_update(pad_t *p) { memset(p, 0, sizeof(*p)); }
#else
#include <kernel.h>
#include <sifrpc.h>
#include <loadfile.h>
#include <libpad.h>

static char pad_buf[256] __attribute__((aligned(64)));
static int pad_mode_set = 0;
static uint16_t prev_btn = 0;

void input_init(void)
{
    SifLoadModule("rom0:SIO2MAN", 0, NULL);
    SifLoadModule("rom0:PADMAN", 0, NULL);
    padInit(0);
    padPortOpen(0, 0, pad_buf);
}

static float axis(unsigned char v)
{
    float f = ((int)v - 128) / 127.0f;
    if (f > 1.0f) f = 1.0f;
    if (f < -1.0f) f = -1.0f;
    if (f > -0.22f && f < 0.22f) return 0.0f; /* zona muerta */
    return f > 0 ? (f - 0.22f) / 0.78f : (f + 0.22f) / 0.78f;
}

void input_update(pad_t *p)
{
    struct padButtonStatus b;
    int st = padGetState(0, 0);
    uint16_t btn = 0;

    memset(p, 0, sizeof(*p));
    if (st != PAD_STATE_STABLE && st != PAD_STATE_FINDCTP1) {
        prev_btn = 0;
        return;
    }
    if (pad_mode_set < 60) {
        /* activar modo analogico y bloquearlo (reintenta unos frames) */
        if (st == PAD_STATE_STABLE && padSetMainMode(0, 0, PAD_MMODE_DUALSHOCK, PAD_MMODE_LOCK) == 1) {
            pad_mode_set = 60;
            return;
        }
        pad_mode_set++;
    }
    if (padRead(0, 0, &b) != 0) {
        btn = (uint16_t)(0xffff ^ b.btns);
        if ((b.mode >> 4) == 0x07) { /* analogico */
            p->analog = 1;
            p->lx = axis(b.ljoy_h);
            p->ly = axis(b.ljoy_v);
            p->rx = axis(b.rjoy_h);
            p->ry = axis(b.rjoy_v);
        }
    }
    p->held = btn;
    p->pressed = (uint16_t)(btn & ~prev_btn);
    prev_btn = btn;
}
#endif
