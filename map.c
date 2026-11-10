/* map.c - mapa "de_polvo": grilla de paredes y cajas */
#include "map.h"
#include <string.h>

/* # pared, c caja, G aparicion GUERRILLA, N aparicion GIGN */
static const char *MAP_SRC[MAP_H] = {
    "########################",
    "#GGG..#.........#......#",
    "#GGG..#...cc....#..c...#",
    "#.....#.........#......#",
    "#..c..####..#####......#",
    "#......................#",
    "#....c..........c......#",
    "###....###....###....###",
    "#......#........#......#",
    "#..cc..#...##...#..cc..#",
    "#..........##..........#",
    "#..........##..........#",
    "#......#........#......#",
    "#......#...cc...#......#",
    "###....###....###....###",
    "#......c...............#",
    "#......................#",
    "#.....####..#####..c...#",
    "#.....#.........#......#",
    "#..c..#....c....#...NNN#",
    "#.....#.........#...NNN#",
    "#.....#.........#...NNN#",
    "#......................#",
    "########################",
};

static unsigned char tiles[MAP_W * MAP_H];
static int spawns[2][16], nspawn[2];
static int freecells[MAP_W * MAP_H], nfree;

typedef struct { vec3 v[4]; vec3 n; uint32_t c; vec3 mid; } face_t;
#define MAX_FACES 900
static face_t faces[MAX_FACES];
static int nfaces;
static int crates[64], ncrates;

static uint32_t h32(uint32_t x)
{
    x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16;
    return x;
}

static uint32_t vary(uint32_t c, uint32_t seed, int amt)
{
    int d = (int)(h32(seed) % (unsigned)(amt * 2 + 1)) - amt;
    int r = CR(c) + d, g = CG(c) + d, b = CB(c) + d;
    if (r < 0) r = 0;
    if (g < 0) g = 0;
    if (b < 0) b = 0;
    if (r > 255) r = 255;
    if (g > 255) g = 255;
    if (b > 255) b = 255;
    return RGB(r, g, b);
}

static void add_face(vec3 a, vec3 b, vec3 c, vec3 d, vec3 n, uint32_t col)
{
    face_t *f;
    if (nfaces >= MAX_FACES) return;
    f = &faces[nfaces++];
    f->v[0] = a; f->v[1] = b; f->v[2] = c; f->v[3] = d;
    f->n = n; f->c = col;
    f->mid = vscale(vadd(vadd(a, b), vadd(c, d)), 0.25f);
}

/* lado de pared con zocalo oscuro, revoque y franja superior */
static void add_wall_side(vec3 p0, vec3 p1, vec3 n, uint32_t seed)
{
    static const float bands[4] = { 0.0f, 0.45f, 2.75f, WALL_H };
    uint32_t cols[3];
    int i;
    cols[0] = vary(RGB(120, 96, 66), seed, 6);
    cols[1] = vary(RGB(206, 176, 128), seed * 3u + 1u, 10);
    cols[2] = vary(RGB(178, 150, 106), seed * 5u + 2u, 6);
    for (i = 0; i < 3; i++) {
        vec3 a = v3(p0.x, bands[i], p0.z), b = v3(p1.x, bands[i], p1.z);
        vec3 c = v3(p1.x, bands[i + 1], p1.z), d = v3(p0.x, bands[i + 1], p0.z);
        add_face(a, b, c, d, n, cols[i]);
    }
}

int map_tile(int cx, int cz)
{
    if (cx < 0 || cz < 0 || cx >= MAP_W || cz >= MAP_H) return T_WALL;
    return tiles[cz * MAP_W + cx];
}

int map_walkable(int cx, int cz) { return map_tile(cx, cz) == T_FLOOR; }

void map_init(void)
{
    int x, z;
    nspawn[0] = nspawn[1] = 0;
    nfree = 0;
    nfaces = 0;
    ncrates = 0;
    for (z = 0; z < MAP_H; z++)
        for (x = 0; x < MAP_W; x++) {
            char ch = MAP_SRC[z][x];
            int t = T_FLOOR;
            if (ch == '#') t = T_WALL;
            else if (ch == 'c') t = T_CRATE;
            tiles[z * MAP_W + x] = (unsigned char)t;
            if (ch == 'G' && nspawn[0] < 16) spawns[0][nspawn[0]++] = z * MAP_W + x;
            if (ch == 'N' && nspawn[1] < 16) spawns[1][nspawn[1]++] = z * MAP_W + x;
            if (t == T_FLOOR) freecells[nfree++] = z * MAP_W + x;
            if (t == T_CRATE && ncrates < 64) crates[ncrates++] = z * MAP_W + x;
        }

    /* piso: damero de arena */
    for (z = 0; z < MAP_H; z++)
        for (x = 0; x < MAP_W; x++) {
            float x0 = x * CELL, z0 = z * CELL, x1 = x0 + CELL, z1 = z0 + CELL;
            uint32_t base;
            if (map_tile(x, z) == T_WALL) continue;
            base = ((x + z) & 1) ? RGB(196, 168, 120) : RGB(184, 156, 110);
            add_face(v3(x0, 0, z0), v3(x0, 0, z1), v3(x1, 0, z1), v3(x1, 0, z0), v3(0, 1, 0),
                     vary(base, (uint32_t)(z * MAP_W + x) * 7u, 7));
        }

    /* paredes: solo los lados visibles */
    for (z = 0; z < MAP_H; z++)
        for (x = 0; x < MAP_W; x++) {
            float x0 = x * CELL, z0 = z * CELL, x1 = x0 + CELL, z1 = z0 + CELL;
            uint32_t s = (uint32_t)(z * MAP_W + x) * 13u;
            if (map_tile(x, z) != T_WALL) continue;
            if (map_tile(x + 1, z) != T_WALL && x + 1 < MAP_W) add_wall_side(v3(x1, 0, z0), v3(x1, 0, z1), v3(1, 0, 0), s + 1);
            if (map_tile(x - 1, z) != T_WALL && x - 1 >= 0) add_wall_side(v3(x0, 0, z1), v3(x0, 0, z0), v3(-1, 0, 0), s + 2);
            if (map_tile(x, z + 1) != T_WALL && z + 1 < MAP_H) add_wall_side(v3(x1, 0, z1), v3(x0, 0, z1), v3(0, 0, 1), s + 3);
            if (map_tile(x, z - 1) != T_WALL && z - 1 >= 0) add_wall_side(v3(x0, 0, z0), v3(x1, 0, z0), v3(0, 0, -1), s + 4);
            add_face(v3(x0, WALL_H, z0), v3(x0, WALL_H, z1), v3(x1, WALL_H, z1), v3(x1, WALL_H, z0), v3(0, 1, 0),
                     vary(RGB(160, 134, 96), s + 5, 6));
        }
}

static void cell_box(int cx, int cz, float *x0, float *z0, float *x1, float *z1)
{
    float in = (map_tile(cx, cz) == T_CRATE) ? CRATE_IN : 0.0f;
    *x0 = cx * CELL + in; *z0 = cz * CELL + in;
    *x1 = (cx + 1) * CELL - in; *z1 = (cz + 1) * CELL - in;
}

void map_collide(vec3 *pos, float r)
{
    int it, dx, dz;
    for (it = 0; it < 2; it++) {
        int cx = (int)floorf(pos->x / CELL), cz = (int)floorf(pos->z / CELL);
        for (dz = -1; dz <= 1; dz++)
            for (dx = -1; dx <= 1; dx++) {
                int t = map_tile(cx + dx, cz + dz);
                float x0, z0, x1, z1, px, pz, ddx, ddz, d2;
                if (t == T_FLOOR) continue;
                if (t == T_CRATE && pos->y >= CRATE_H - 0.05f) continue; /* parado arriba */
                cell_box(cx + dx, cz + dz, &x0, &z0, &x1, &z1);
                px = pos->x < x0 ? x0 : (pos->x > x1 ? x1 : pos->x);
                pz = pos->z < z0 ? z0 : (pos->z > z1 ? z1 : pos->z);
                ddx = pos->x - px; ddz = pos->z - pz;
                d2 = ddx * ddx + ddz * ddz;
                if (d2 < r * r) {
                    if (d2 > 1e-8f) {
                        float d = sqrtf(d2), push = r - d;
                        pos->x += ddx / d * push;
                        pos->z += ddz / d * push;
                    } else {
                        /* centro adentro: salir por el lado mas cercano */
                        float l = pos->x - x0, rr = x1 - pos->x, u = pos->z - z0, w = z1 - pos->z;
                        float m = l;
                        if (rr < m) m = rr;
                        if (u < m) m = u;
                        if (w < m) m = w;
                        if (m == l) pos->x = x0 - r;
                        else if (m == rr) pos->x = x1 + r;
                        else if (m == u) pos->z = z0 - r;
                        else pos->z = z1 + r;
                    }
                }
            }
    }
}

float map_ground(vec3 pos, float r)
{
    int cx = (int)floorf(pos.x / CELL), cz = (int)floorf(pos.z / CELL), dx, dz;
    float g = 0.0f;
    for (dz = -1; dz <= 1; dz++)
        for (dx = -1; dx <= 1; dx++) {
            float x0, z0, x1, z1;
            if (map_tile(cx + dx, cz + dz) != T_CRATE) continue;
            cell_box(cx + dx, cz + dz, &x0, &z0, &x1, &z1);
            if (pos.x + r * 0.6f > x0 && pos.x - r * 0.6f < x1 && pos.z + r * 0.6f > z0 && pos.z - r * 0.6f < z1 &&
                pos.y >= CRATE_H - 0.3f)
                g = CRATE_H;
        }
    return g;
}

float map_ray(vec3 from, vec3 dir, float maxd)
{
    float t;
    const float step = 0.08f;
    for (t = 0.0f; t < maxd; t += step) {
        vec3 p = vadd(from, vscale(dir, t));
        int cx, cz, tl;
        if (p.y <= 0.0f) return t;
        cx = (int)floorf(p.x / CELL);
        cz = (int)floorf(p.z / CELL);
        tl = map_tile(cx, cz);
        if (tl == T_WALL && p.y < WALL_H) return t;
        if (tl == T_CRATE && p.y < CRATE_H) {
            float x0, z0, x1, z1;
            cell_box(cx, cz, &x0, &z0, &x1, &z1);
            if (p.x > x0 && p.x < x1 && p.z > z0 && p.z < z1) return t;
        }
    }
    return maxd;
}

static const uint32_t WOOD[4] = { RGB(150, 104, 58), RGB(128, 86, 46), RGB(164, 118, 68), RGB(112, 76, 40) };

void map_draw(const camera_t *cam)
{
    int i;
    xform id = xf_identity();
    for (i = 0; i < nfaces; i++) {
        face_t *f = &faces[i];
        vec3 d = vsub(cam->pos, f->mid);
        if (vdot(d, f->n) <= 0) continue;
        if (fabsf(d.x) > 64.0f || fabsf(d.z) > 64.0f) continue;
        gfx_poly(f->v, 4, f->n, f->c);
    }
    for (i = 0; i < ncrates; i++) {
        int cx = crates[i] % MAP_W, cz = crates[i] / MAP_W;
        float x0, z0, x1, z1;
        cell_box(cx, cz, &x0, &z0, &x1, &z1);
        gfx_box(&id, v3(x0, 0, z0), v3(x1, CRATE_H, z1), WOOD, 4, 2, (uint32_t)crates[i]);
        /* tablas de refuerzo */
        gfx_box1(&id, v3(x0 - 0.02f, CRATE_H - 0.12f, z0 - 0.02f), v3(x1 + 0.02f, CRATE_H, z1 + 0.02f), RGB(96, 64, 34));
        gfx_box1(&id, v3(x0 - 0.02f, 0.0f, z0 - 0.02f), v3(x1 + 0.02f, 0.12f, z1 + 0.02f), RGB(96, 64, 34));
    }
}

int map_spawn_count(int team) { return nspawn[team]; }
vec3 map_spawn(int team, int idx)
{
    int c = spawns[team][idx % (nspawn[team] ? nspawn[team] : 1)];
    return v3((c % MAP_W + 0.5f) * CELL, 0.0f, (c / MAP_W + 0.5f) * CELL);
}
int map_free_count(void) { return nfree; }
int map_free_cell(int i) { return freecells[i % nfree]; }

void map_bfs(int goal, uint16_t *dist)
{
    static int queue[MAP_W * MAP_H];
    int qh = 0, qt = 0, i;
    for (i = 0; i < MAP_W * MAP_H; i++) dist[i] = 0xffff;
    if (goal < 0 || goal >= MAP_W * MAP_H || !map_walkable(goal % MAP_W, goal / MAP_W)) return;
    dist[goal] = 0;
    queue[qt++] = goal;
    while (qh < qt) {
        int c = queue[qh++], cx = c % MAP_W, cz = c / MAP_W, k;
        static const int DX[4] = { 1, -1, 0, 0 }, DZ[4] = { 0, 0, 1, -1 };
        for (k = 0; k < 4; k++) {
            int nx = cx + DX[k], nz = cz + DZ[k], n;
            if (!map_walkable(nx, nz)) continue;
            n = nz * MAP_W + nx;
            if (dist[n] != 0xffff) continue;
            dist[n] = (uint16_t)(dist[c] + 1);
            queue[qt++] = n;
        }
    }
}

void map_draw_radar(float x, float y, float s)
{
    int cx, cz;
    gfx_rect(x - 2, y - 2, x + MAP_W * s + 2, y + MAP_H * s + 2, RGB(0, 0, 0), 0x50);
    for (cz = 0; cz < MAP_H; cz++) {
        cx = 0;
        while (cx < MAP_W) {
            int t = map_tile(cx, cz), st = cx;
            if (t == T_FLOOR) { cx++; continue; }
            while (cx < MAP_W && map_tile(cx, cz) == t) cx++;
            gfx_rect(x + st * s, y + cz * s, x + cx * s, y + (cz + 1) * s,
                     t == T_WALL ? RGB(150, 150, 140) : RGB(130, 90, 50), 0x70);
        }
    }
}
