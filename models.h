#ifndef MODELS_H
#define MODELS_H
#include "gfx.h"
#include "defs.h"

typedef struct {
    float bob_x, bob_y;
    float kick;    /* 0..1 retroceso del disparo */
    float reload;  /* 0..1 animacion de recarga */
    float swing;   /* 0..1 cuchillazo */
    float raise;   /* 0..1 sacar arma (1 = lista) */
    float flash;   /* 0..1 fogonazo */
} vm_anim_t;

/* soldado en tercera persona. death 0..1 = cayendo */
void model_soldier(int team, vec3 pos, float yaw, float pitch, float walk, float death,
                   int weapon, float flash, int lod);
/* brazos + arma en primera persona */
void model_viewmodel(int team, int weapon, const vm_anim_t *a);
/* arma suelta (para menu de compra / vista previa) */
void model_weapon(const xform *m, int weapon, float slide, float flash);

#endif
