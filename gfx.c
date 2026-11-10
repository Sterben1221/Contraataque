/* gfx.c - transformacion, recorte y dibujo. */
#include "gfx.h"
#include <string.h>
#include <stdio.h>

int g_scr_w = 640, g_scr_h = 448;

/* ------------------------------------------------------------------ */
/* Backend                                                             */
/* ------------------------------------------------------------------ */
#ifdef PC_PREVIEW
/* Rasterizador simple para probar en PC (no se usa en la PS2) */
static uint32_t fb[640 * 512];
static uint16_t zb[640 * 512];

static void be_init(void) { g_scr_w = 640; g_scr_h = 448; }

static void be_clear(uint32_t c)
{
    int i, n = g_scr_w * g_scr_h;
    for (i = 0; i < n; i++) { fb[i] = c; zb[i] = 0; }
}

static float edgef(float ax, float ay, float bx, float by, float px, float py)
{
    return (bx - ax) * (py - ay) - (by - ay) * (px - ax);
}

static void be_tri(float x1, float y1, int z1, float x2, float y2, int z2,
                   float x3, float y3, int z3, uint32_t c)
{
    int minx = (int)floorf(fminf(x1, fminf(x2, x3)));
    int maxx = (int)ceilf(fmaxf(x1, fmaxf(x2, x3)));
    int miny = (int)floorf(fminf(y1, fminf(y2, y3)));
    int maxy = (int)ceilf(fmaxf(y1, fmaxf(y2, y3)));
    float area = edgef(x1, y1, x2, y2, x3, y3);
    int x, y;
    if (fabsf(area) < 1e-6f) return;
    if (minx < 0) minx = 0;
    if (miny < 0) miny = 0;
    if (maxx > g_scr_w - 1) maxx = g_scr_w - 1;
    if (maxy > g_scr_h - 1) maxy = g_scr_h - 1;
    for (y = miny; y <= maxy; y++)
        for (x = minx; x <= maxx; x++) {
            float px = x + 0.5f, py = y + 0.5f;
            float w1 = edgef(x2, y2, x3, y3, px, py) / area;
            float w2 = edgef(x3, y3, x1, y1, px, py) / area;
            float w3 = 1.0f - w1 - w2;
            int z;
            if (w1 < 0 || w2 < 0 || w3 < 0) continue;
            z = (int)(w1 * z1 + w2 * z2 + w3 * z3);
            if (z >= zb[y * g_scr_w + x]) {
                zb[y * g_scr_w + x] = (uint16_t)z;
                fb[y * g_scr_w + x] = c;
            }
        }
}

static void be_rect(float x0, float y0, float x1, float y1, uint32_t c, int a)
{
    int x, y;
    int ix0 = (int)x0, iy0 = (int)y0, ix1 = (int)x1, iy1 = (int)y1;
    if (ix0 < 0) ix0 = 0;
    if (iy0 < 0) iy0 = 0;
    if (ix1 > g_scr_w) ix1 = g_scr_w;
    if (iy1 > g_scr_h) iy1 = g_scr_h;
    for (y = iy0; y < iy1; y++)
        for (x = ix0; x < ix1; x++) {
            uint32_t d = fb[y * g_scr_w + x];
            int r = (CR(c) * a + CR(d) * (128 - a)) / 128;
            int g = (CG(c) * a + CG(d) * (128 - a)) / 128;
            int b = (CB(c) * a + CB(d) * (128 - a)) / 128;
            fb[y * g_scr_w + x] = RGB(r, g, b);
        }
}

static void be_flip(void) {}

void gfx_save_ppm(const char *path)
{
    FILE *f = fopen(path, "wb");
    int i;
    if (!f) return;
    fprintf(f, "P6\n%d %d\n255\n", g_scr_w, g_scr_h);
    for (i = 0; i < g_scr_w * g_scr_h; i++) {
        unsigned char p[3] = { (unsigned char)CR(fb[i]), (unsigned char)CG(fb[i]), (unsigned char)CB(fb[i]) };
        fwrite(p, 1, 3, f);
    }
    fclose(f);
}

#else
/* ----------------------------- PS2 / gsKit ------------------------ */
#include <kernel.h>
#include <gsKit.h>
#include <dmaKit.h>

static GSGLOBAL *gs;

static inline u64 gscol(uint32_t c, int a)
{
    return GS_SETREG_RGBAQ(CR(c), CG(c), CB(c), a, 0x00);
}

static void be_init(void)
{
    gs = gsKit_init_global();
    gs->PSM = GS_PSM_CT32;
    gs->PSMZ = GS_PSMZ_16S;
    gs->ZBuffering = GS_SETTING_ON;
    gs->DoubleBuffering = GS_SETTING_ON;
    gs->PrimAlphaEnable = GS_SETTING_ON;

    dmaKit_init(D_CTRL_RELE_OFF, D_CTRL_MFD_OFF, D_CTRL_STS_UNSPEC,
                D_CTRL_STD_OFF, D_CTRL_RCYC_8, 1 << DMA_CHANNEL_GIF);
    dmaKit_chan_init(DMA_CHANNEL_GIF);

    gsKit_init_screen(gs);
    gsKit_mode_switch(gs, GS_ONESHOT);

    gsKit_set_primalpha(gs, GS_SETREG_ALPHA(0, 1, 0, 1, 0), 0);
    gs->Test->ZTST = 2; /* GEQUAL: z mas grande = mas cerca */
    gsKit_set_test(gs, GS_ZTEST_ON);

    g_scr_w = gs->Width;
    g_scr_h = gs->Height;
}

static void be_clear(uint32_t c)
{
    gsKit_clear(gs, gscol(c, 0x80));
    gs->Test->ZTST = 2;
    gsKit_set_test(gs, GS_ZTEST_ON);
}

static void be_tri(float x1, float y1, int z1, float x2, float y2, int z2,
                   float x3, float y3, int z3, uint32_t c)
{
    gsKit_prim_triangle_3d(gs, x1, y1, z1, x2, y2, z2, x3, y3, z3, gscol(c, 0x80));
}

static void be_rect(float x0, float y0, float x1, float y1, uint32_t c, int a)
{
    gsKit_prim_sprite(gs, x0, y0, x1, y1, 65000, gscol(c, a));
}

static void be_flip(void)
{
    gsKit_queue_exec(gs);
    gsKit_sync_flip(gs);
}
#endif

/* ------------------------------------------------------------------ */
/* Matematica                                                          */
/* ------------------------------------------------------------------ */
xform xf_identity(void)
{
    xform m;
    m.x = v3(1, 0, 0); m.y = v3(0, 1, 0); m.z = v3(0, 0, 1); m.o = v3(0, 0, 0);
    return m;
}
xform xf_translate(float x, float y, float z) { xform m = xf_identity(); m.o = v3(x, y, z); return m; }
xform xf_rot_x(float a)
{
    xform m = xf_identity(); float c = cosf(a), s = sinf(a);
    m.y = v3(0, c, s); m.z = v3(0, -s, c);
    return m;
}
xform xf_rot_y(float a)
{
    xform m = xf_identity(); float c = cosf(a), s = sinf(a);
    m.x = v3(c, 0, -s); m.z = v3(s, 0, c);
    return m;
}
xform xf_rot_z(float a)
{
    xform m = xf_identity(); float c = cosf(a), s = sinf(a);
    m.x = v3(c, s, 0); m.y = v3(-s, c, 0);
    return m;
}
xform xf_scale(float s)
{
    xform m = xf_identity();
    m.x = v3(s, 0, 0); m.y = v3(0, s, 0); m.z = v3(0, 0, s);
    return m;
}
vec3 xf_dir(const xform *m, vec3 d)
{
    return v3(m->x.x * d.x + m->y.x * d.y + m->z.x * d.z,
              m->x.y * d.x + m->y.y * d.y + m->z.y * d.z,
              m->x.z * d.x + m->y.z * d.y + m->z.z * d.z);
}
vec3 xf_apply(const xform *m, vec3 p) { return vadd(m->o, xf_dir(m, p)); }
xform xf_mul(const xform *a, const xform *b)
{
    xform r;
    r.x = xf_dir(a, b->x); r.y = xf_dir(a, b->y); r.z = xf_dir(a, b->z);
    r.o = xf_apply(a, b->o);
    return r;
}
xform xf_segment(const xform *parent, vec3 a, vec3 b, float *len_out)
{
    xform l;
    vec3 d = vsub(b, a);
    vec3 up = v3(0, 1, 0);
    float len = vlen(d);
    l.z = vnorm(d);
    if (fabsf(l.z.y) > 0.95f) up = v3(1, 0, 0);
    l.x = vnorm(vcross(up, l.z));
    l.y = vcross(l.z, l.x);
    l.o = a;
    if (len_out) *len_out = len;
    return xf_mul(parent, &l);
}

/* ------------------------------------------------------------------ */
/* Camara, recorte y proyeccion                                        */
/* ------------------------------------------------------------------ */
static vec3 cam_pos, cam_r, cam_u, cam_f;
static int vm_mode;
static float fx, fy, cx0, cy0, tx, ty;
static vec3 light_world, light_cam, light_vm;
static uint32_t fog_col = RGB(190, 200, 210);
static float fog_s = 25.0f, fog_e = 58.0f;

#define FAR_Z 62.0f
#define NEAR_W 0.08f
#define NEAR_VM 0.015f

void gfx_init(void)
{
    be_init();
    cx0 = g_scr_w * 0.5f;
    cy0 = g_scr_h * 0.5f;
    fx = cx0;                                   /* FOV horizontal 90 grados */
    fy = fx * ((float)g_scr_h / (g_scr_w * 0.75f)); /* corrige pixel no cuadrado */
    tx = cx0 / fx * 1.01f;
    ty = cy0 / fy * 1.01f;
    light_world = vnorm(v3(0.45f, 0.8f, 0.3f));
    light_vm = vnorm(v3(-0.35f, 0.85f, -0.4f));
}

void gfx_set_fog(uint32_t c, float s, float e) { fog_col = c; fog_s = s; fog_e = e; }

void gfx_set_camera(const camera_t *c)
{
    float sy = sinf(c->yaw), cy = cosf(c->yaw), sp = sinf(c->pitch), cp = cosf(c->pitch);
    cam_pos = c->pos;
    cam_r = v3(cy, 0, -sy);
    cam_f = v3(sy * cp, sp, cy * cp);
    cam_u = v3(-sy * sp, cp, -cy * sp);
    light_cam = v3(vdot(light_world, cam_r), vdot(light_world, cam_u), vdot(light_world, cam_f));
}

void gfx_set_viewmodel(int on) { vm_mode = on; }

static inline vec3 to_cam(vec3 p)
{
    vec3 d;
    if (vm_mode) return p;
    d = vsub(p, cam_pos);
    return v3(vdot(d, cam_r), vdot(d, cam_u), vdot(d, cam_f));
}
static inline vec3 dir_to_cam(vec3 n)
{
    if (vm_mode) return n;
    return v3(vdot(n, cam_r), vdot(n, cam_u), vdot(n, cam_f));
}

int gfx_project(vec3 p, float *sx, float *sy)
{
    vec3 c = to_cam(p);
    if (c.z < 0.1f) return 0;
    *sx = cx0 + c.x / c.z * fx;
    *sy = cy0 - c.y / c.z * fy;
    return 1;
}

static float plane_d(int pl, vec3 v)
{
    switch (pl) {
    case 0: return v.z - (vm_mode ? NEAR_VM : NEAR_W);
    case 1: return v.x + v.z * tx;
    case 2: return -v.x + v.z * tx;
    case 3: return v.y + v.z * ty;
    default: return -v.y + v.z * ty;
    }
}

static int clip_poly(vec3 *in, int n, vec3 *out, int pl)
{
    int i, m = 0;
    for (i = 0; i < n; i++) {
        vec3 a = in[i], b = in[(i + 1) % n];
        float da = plane_d(pl, a), db = plane_d(pl, b);
        if (da >= 0) out[m++] = a;
        if ((da >= 0) != (db >= 0)) {
            float t = da / (da - db);
            out[m++] = vlerp(a, b, t);
        }
    }
    return m;
}

static inline int depth_of(float z)
{
    int d;
    if (vm_mode) {
        d = 45000 + (int)((1.0f - z * 0.5f) * 15000.0f);
        if (d < 45000) d = 45000;
        if (d > 60000) d = 60000;
    } else {
        d = 1000 + (int)((1.0f - z / FAR_Z) * 39000.0f);
        if (d < 1000) d = 1000;
        if (d > 40000) d = 40000;
    }
    return d;
}

static uint32_t shade(uint32_t c, vec3 ncam, float zavg)
{
    vec3 L = vm_mode ? light_vm : light_cam;
    float k = vdot(ncam, L);
    float s, f;
    int r, g, b;
    if (k < 0) k = 0;
    s = 0.58f + 0.48f * k;
    r = (int)(CR(c) * s); g = (int)(CG(c) * s); b = (int)(CB(c) * s);
    if (!vm_mode && zavg > fog_s) {
        f = (zavg - fog_s) / (fog_e - fog_s);
        if (f > 1) f = 1;
        r = (int)(r + (CR(fog_col) - r) * f);
        g = (int)(g + (CG(fog_col) - g) * f);
        b = (int)(b + (CB(fog_col) - b) * f);
    }
    if (r > 255) r = 255;
    if (g > 255) g = 255;
    if (b > 255) b = 255;
    return RGB(r, g, b);
}

/* poligono ya en coordenadas de camara */
static void poly_cam(const vec3 *vc, int n, vec3 ncam, uint32_t color)
{
    vec3 a[12], b[12];
    int i, pl, m = n;
    float zs = 0, sx[12], sy[12];
    int sz[12];
    uint32_t c;

    /* cara de espaldas */
    if (vdot(ncam, vc[0]) >= 0) return;

    /* rechazo rapido */
    for (pl = 0; pl < 5; pl++) {
        int out = 0;
        for (i = 0; i < n; i++) if (plane_d(pl, vc[i]) < 0) out++;
        if (out == n) return;
    }
    if (!vm_mode) {
        int far = 0;
        for (i = 0; i < n; i++) if (vc[i].z > FAR_Z) far++;
        if (far == n) return;
    }

    for (i = 0; i < n; i++) { a[i] = vc[i]; zs += vc[i].z; }
    for (pl = 0; pl < 5 && m >= 3; pl++) {
        int k;
        m = clip_poly(a, m, b, pl);
        for (k = 0; k < m; k++) a[k] = b[k];
    }
    if (m < 3) return;

    c = shade(color, ncam, zs / n);
    for (i = 0; i < m; i++) {
        float iz = 1.0f / a[i].z;
        sx[i] = cx0 + a[i].x * iz * fx;
        sy[i] = cy0 - a[i].y * iz * fy;
        sz[i] = depth_of(a[i].z);
    }
    for (i = 1; i + 1 < m; i++)
        be_tri(sx[0], sy[0], sz[0], sx[i], sy[i], sz[i], sx[i + 1], sy[i + 1], sz[i + 1], c);
}

void gfx_poly(const vec3 *v, int n, vec3 normal, uint32_t color)
{
    vec3 c[4];
    int i;
    for (i = 0; i < n && i < 4; i++) c[i] = to_cam(v[i]);
    poly_cam(c, n, dir_to_cam(normal), color);
}

static inline uint32_t hash32(uint32_t x)
{
    x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16;
    return x;
}

/* indices de las esquinas de cada cara (0..7 = bits x,y,z) y su normal local */
static const int FACE_IDX[6][4] = {
    { 1, 3, 7, 5 }, /* +x */
    { 0, 4, 6, 2 }, /* -x */
    { 2, 6, 7, 3 }, /* +y */
    { 0, 1, 5, 4 }, /* -y */
    { 4, 5, 7, 6 }, /* +z */
    { 0, 2, 3, 1 }, /* -z */
};
static const float FACE_N[6][3] = {
    { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 }
};

void gfx_box(const xform *m, vec3 lo, vec3 hi, const uint32_t *pal, int npal, int sub, uint32_t seed)
{
    vec3 cc[8];
    int i, f;
    if (sub < 1) sub = 1;
    if (sub > 6) sub = 6;
    for (i = 0; i < 8; i++) {
        vec3 p = v3((i & 1) ? hi.x : lo.x, (i & 2) ? hi.y : lo.y, (i & 4) ? hi.z : lo.z);
        cc[i] = to_cam(xf_apply(m, p));
    }
    for (f = 0; f < 6; f++) {
        vec3 nl = v3(FACE_N[f][0], FACE_N[f][1], FACE_N[f][2]);
        vec3 nc = dir_to_cam(vnorm(xf_dir(m, nl)));
        vec3 p0 = cc[FACE_IDX[f][0]], p1 = cc[FACE_IDX[f][1]];
        vec3 p2 = cc[FACE_IDX[f][2]], p3 = cc[FACE_IDX[f][3]];
        if (vdot(nc, p0) >= 0) continue;
        if (sub == 1 || npal == 1) {
            vec3 q[4];
            q[0] = p0; q[1] = p1; q[2] = p2; q[3] = p3;
            poly_cam(q, 4, nc, pal[(npal > 1) ? hash32(seed + f) % npal : 0]);
        } else {
            int u, w;
            for (u = 0; u < sub; u++)
                for (w = 0; w < sub; w++) {
                    float u0 = (float)u / sub, u1 = (float)(u + 1) / sub;
                    float w0 = (float)w / sub, w1 = (float)(w + 1) / sub;
                    vec3 a0 = vlerp(p0, p1, u0), a1 = vlerp(p0, p1, u1);
                    vec3 b0 = vlerp(p3, p2, u0), b1 = vlerp(p3, p2, u1);
                    vec3 q[4];
                    q[0] = vlerp(a0, b0, w0);
                    q[1] = vlerp(a1, b1, w0);
                    q[2] = vlerp(a1, b1, w1);
                    q[3] = vlerp(a0, b0, w1);
                    poly_cam(q, 4, nc, pal[hash32(seed * 131u + f * 37u + u * 7u + w) % npal]);
                }
        }
    }
}

void gfx_box1(const xform *m, vec3 lo, vec3 hi, uint32_t color)
{
    gfx_box(m, lo, hi, &color, 1, 1, 0);
}

/* ------------------------------------------------------------------ */
void gfx_begin(uint32_t clear_color) { be_clear(clear_color); }
void gfx_end(void) { be_flip(); }

void gfx_rect(float x0, float y0, float x1, float y1, uint32_t color, int alpha)
{
    be_rect(x0, y0, x1, y1, color, alpha);
}

/* ------------------------------------------------------------------ */
/* Fuente 5x7                                                          */
/* ------------------------------------------------------------------ */
typedef struct { char c; const char *r; } glyph_t;
static const glyph_t FONT[] = {
    { '0', ".###.#...##..###.#.###..##...#.###." },
    { '1', "..#...##....#....#....#....#...###." },
    { '2', ".###.#...#....#...#...#...#...#####" },
    { '3', "#####...#...#.....#.....##...#.###." },
    { '4', "...#...##..#.#.#..#.#####...#....#." },
    { '5', "######....####.....#....##...#.###." },
    { '6', "..##..#...#....####.#...##...#.###." },
    { '7', "#####....#...#...#...#....#....#..." },
    { '8', ".###.#...##...#.###.#...##...#.###." },
    { '9', ".###.#...##...#.####....#...#..##.." },
    { 'A', ".###.#...##...#######...##...##...#" },
    { 'B', "####.#...##...#####.#...##...#####." },
    { 'C', ".###.#...##....#....#....#...#.###." },
    { 'D', "###..#..#.#...##...##...##..#.###.." },
    { 'E', "######....#....####.#....#....#####" },
    { 'F', "######....#....####.#....#....#...." },
    { 'G', ".###.#...##....#.####...##...#.####" },
    { 'H', "#...##...##...#######...##...##...#" },
    { 'I', ".###...#....#....#....#....#...###." },
    { 'J', "..###...#....#....#....#.#..#..##.." },
    { 'K', "#...##..#.#.#..##...#.#..#..#.#...#" },
    { 'L', "#....#....#....#....#....#....#####" },
    { 'M', "#...###.###.#.##.#.##...##...##...#" },
    { 'N', "#...##...###..##.#.##..###...##...#" },
    { 'O', ".###.#...##...##...##...##...#.###." },
    { 'P', "####.#...##...#####.#....#....#...." },
    { 'Q', ".###.#...##...##...##.#.##..#..##.#" },
    { 'R', "####.#...##...#####.#.#..#..#.#...#" },
    { 'S', ".#####....#.....###.....#....#####." },
    { 'T', "#####..#....#....#....#....#....#.." },
    { 'U', "#...##...##...##...##...##...#.###." },
    { 'V', "#...##...##...##...##...#.#.#...#.." },
    { 'W', "#...##...##...##.#.##.#.##.#.#.#.#." },
    { 'X', "#...##...#.#.#...#...#.#.#...##...#" },
    { 'Y', "#...##...#.#.#...#....#....#....#.." },
    { 'Z', "#####....#...#...#...#...#....#####" },
    { '!', "..#....#....#....#....#.........#.." },
    { '$', "..#...#####.#...###...#.#####...#.." },
    { '%', "##..###.#....#...#...#....#.###..##" },
    { '(', "...#...#...#....#....#.....#.....#." },
    { ')', ".#.....#.....#....#....#...#...#..." },
    { '+', ".......#....#..#####..#....#......." },
    { ',', ".....................##....#...#..." },
    { '-', "...............#####..............." },
    { '.', "..........................##...##.." },
    { '/', "....#...#....#...#...#....#...#...." },
    { ':', "......##...##........##...##......." },
    { '?', ".###.#...#....#...#...#.........#.." },
    { '|', "..#....#....#....#....#....#....#.." },
    { '<', "...#...#...#...#.....#.....#.....#." },
    { '>', ".#.....#.....#.....#...#...#...#..." },
    { '=', "..........#####.....#####.........." },
    { '_', "..............................#####" },
    { '*', ".......#..#.#.#.###.#.#.#..#......." },
    { '[', ".###..#....#....#....#....#....###." },
    { ']', ".###....#....#....#....#....#..###." },
    { '\'', "..#....#...#......................." },
};

static const char *glyph_rows(char ch)
{
    unsigned i;
    if (ch >= 'a' && ch <= 'z') ch = (char)(ch - 32);
    for (i = 0; i < sizeof(FONT) / sizeof(FONT[0]); i++)
        if (FONT[i].c == ch) return FONT[i].r;
    return 0;
}

int gfx_text_w(int scale, const char *s)
{
    int n = (int)strlen(s);
    return n * 6 * scale - scale;
}

void gfx_text(float x, float y, int scale, uint32_t color, const char *s)
{
    float sh = (float)scale;
    for (; *s; s++, x += 6 * scale) {
        const char *r = glyph_rows(*s);
        int row, col;
        if (!r) continue;
        for (row = 0; row < 7; row++) {
            col = 0;
            while (col < 5) {
                int start;
                if (r[row * 5 + col] != '#') { col++; continue; }
                start = col;
                while (col < 5 && r[row * 5 + col] == '#') col++;
                /* sombra + letra */
                be_rect(x + start * sh + 1, y + row * sh + 1, x + col * sh + 1, y + (row + 1) * sh + 1, 0, 0x60);
                be_rect(x + start * sh, y + row * sh, x + col * sh, y + (row + 1) * sh, color, 0x80);
            }
        }
    }
}

void gfx_text_center(float y, int scale, uint32_t color, const char *s)
{
    gfx_text((g_scr_w - gfx_text_w(scale, s)) * 0.5f, y, scale, color, s);
}
