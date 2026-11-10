#ifndef INPUT_H
#define INPUT_H
#include <stdint.h>

/* botones (mismos bits que libpad) */
#define BTN_SELECT   0x0001
#define BTN_L3       0x0002
#define BTN_R3       0x0004
#define BTN_START    0x0008
#define BTN_UP       0x0010
#define BTN_RIGHT    0x0020
#define BTN_DOWN     0x0040
#define BTN_LEFT     0x0080
#define BTN_L2       0x0100
#define BTN_R2       0x0200
#define BTN_L1       0x0400
#define BTN_R1       0x0800
#define BTN_TRIANGLE 0x1000
#define BTN_CIRCLE   0x2000
#define BTN_CROSS    0x4000
#define BTN_SQUARE   0x8000

typedef struct {
    uint16_t held, pressed;
    float lx, ly, rx, ry; /* -1..1 */
    int analog;
} pad_t;

void input_init(void);
void input_update(pad_t *p);

#endif
