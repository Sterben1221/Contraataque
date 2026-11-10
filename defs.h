#ifndef DEFS_H
#define DEFS_H

enum { TEAM_GUERRILLA = 0, TEAM_GIGN = 1 };

enum { W_KNIFE = 0, W_GLOCK, W_USP, W_BERETTA, W_M1911, W_DEAGLE, W_COUNT };

typedef struct {
    const char *name;
    int price;
    int damage;
    int mag;
    int reserve;
    float rate;    /* segundos entre disparos */
    float spread;  /* dispersion base (radianes) */
    float reload;  /* segundos de recarga */
    float kick;    /* retroceso */
    float range;
} weapon_t;

extern const weapon_t WEAPONS[W_COUNT];

#endif
