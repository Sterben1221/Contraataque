/* gfx.h - mini motor 3D por software (transformacion + recorte) que dibuja
 * triangulos planos con el GS de la PS2 usando gsKit. */
#ifndef GFX_H
#define GFX_H

#include <stdint.h>
#include <math.h>

typedef struct { float x, y, z; } vec3;

/* Transformacion: mundo = o + x*l.x + y*l.y + z*l.z */
typedef struct { vec3 x, y, z, o; } xform;

typedef struct { vec3 pos; float yaw, pitch; } camera_t;

#define RGB(r, g, b) ((uint32_t)(((r) << 16) | ((g) << 8) | (b)))
#define CR(c) ((int)(((c) >> 16) & 255))
#define CG(c) ((int)(((c) >> 8) & 255))
#define CB(c) ((int)((c) & 255))

extern int g_scr_w, g_scr_h;

static inline vec3 v3(float x, float y, float z) { vec3 r; r.x = x; r.y = y; r.z = z; return r; }
static inline vec3 vadd(vec3 a, vec3 b) { return v3(a.x + b.x, a.y + b.y, a.z + b.z); }
static inline vec3 vsub(vec3 a, vec3 b) { return v3(a.x - b.x, a.y - b.y, a.z - b.z); }
static inline vec3 vscale(vec3 a, float s) { return v3(a.x * s, a.y * s, a.z * s); }
static inline float vdot(vec3 a, vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static inline vec3 vcross(vec3 a, vec3 b) { return v3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x); }
static inline float vlen(vec3 a) { return sqrtf(vdot(a, a)); }
static inline vec3 vnorm(vec3 a) { float l = vlen(a); return l > 1e-6f ? vscale(a, 1.0f / l) : v3(0, 1, 0); }
static inline vec3 vlerp(vec3 a, vec3 b, float t) { return v3(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t); }

xform xf_identity(void);
xform xf_translate(float x, float y, float z);
xform xf_rot_x(float a);
xform xf_rot_y(float a);
xform xf_rot_z(float a);
xform xf_scale(float s);
xform xf_mul(const xform *a, const xform *b); /* a primero aplica b */
vec3 xf_apply(const xform *m, vec3 p);
vec3 xf_dir(const xform *m, vec3 d);
/* caja orientada entre dos puntos (ancho w, alto h) */
xform xf_segment(const xform *parent, vec3 a, vec3 b, float *len_out);

void gfx_init(void);
void gfx_begin(uint32_t clear_color);
void gfx_end(void);

/* modo de vista: 0 = mundo (usa camara), 1 = arma/brazos (coordenadas de camara) */
void gfx_set_camera(const camera_t *c);
void gfx_set_viewmodel(int on);
void gfx_set_fog(uint32_t color, float start, float end);

/* poligono plano de 3 o 4 vertices */
void gfx_poly(const vec3 *v, int n, vec3 normal, uint32_t color);
/* caja con "camuflaje pixelado": cada cara se divide en sub x sub con colores de la paleta */
void gfx_box(const xform *m, vec3 lo, vec3 hi, const uint32_t *pal, int npal, int sub, uint32_t seed);
void gfx_box1(const xform *m, vec3 lo, vec3 hi, uint32_t color);

/* 2D */
void gfx_rect(float x0, float y0, float x1, float y1, uint32_t color, int alpha);
void gfx_text(float x, float y, int scale, uint32_t color, const char *s);
int gfx_text_w(int scale, const char *s);
void gfx_text_center(float y, int scale, uint32_t color, const char *s);

/* proyecta un punto del mundo a pantalla (para marcadores). devuelve 0 si esta detras */
int gfx_project(vec3 p, float *sx, float *sy);

#ifdef PC_PREVIEW
void gfx_save_ppm(const char *path);
#endif

#endif
