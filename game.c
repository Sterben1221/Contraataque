/* game.c - logica: jugador, bots, rondas, compra, HUD */
#include "game.h"
#include "gfx.h"
#include "map.h"
#include "models.h"
#include "defs.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

const weapon_t WEAPONS[W_COUNT] = {
    /* nombre          precio dano cargador reserva cadencia dispersion recarga retroceso alcance */
    { "CUCHILLO",          0,  55,  0,   0,  0.50f, 0.000f, 0.0f, 0.00f, 1.9f },
    { "GLOCK 18",        400,  25, 20, 120,  0.13f, 0.012f, 2.2f, 0.55f, 80.0f },
    { "USP .45",         500,  34, 12, 100,  0.16f, 0.008f, 2.6f, 0.70f, 80.0f },
    { "BERETTA 92",      550,  30, 15,  90,  0.14f, 0.010f, 2.3f, 0.65f, 80.0f },
    { "M1911",           600,  38,  7,  56,  0.18f, 0.010f, 2.2f, 0.85f, 80.0f },
    { "DESERT EAGLE",    650,  54,  7,  35,  0.28f, 0.016f, 2.2f, 1.00f, 80.0f },
};

#define MAX_ENTS 8
#define EYE_H 1.62f
#define RADIUS 0.32f
#define WIN_ROUNDS 8
#define ROUND_TIME 115.0f
#define FREEZE_TIME 4.0f
#define BUY_TIME 20.0f

static const char *TEAM_NAME[2] = { "GUERRILLA", "GIGN" };
static const uint32_t TEAM_COL[2] = { RGB(232, 192, 112), RGB(120, 172, 255) };
#define HUD_COL RGB(255, 178, 48)

typedef struct {
    int used, alive, team, bot;
    char name[12];
    vec3 pos;
    float vy;
    int ground;
    float yaw, pitch, punch;
    int hp, armor, helmet, money;
    int pistol, slot; /* slot 0 = cuchillo, 1 = pistola */
    int mag, reserve;
    float fire_cd, reload_t, raise_t, swing_t, kick_t, flash_t;
    float spread, walk, speed;
    float death_t, hurt_t;
    int kills, deaths, survived;
    /* bot */
    int target, goal;
    float react_t, think_t, strafe_t, strafe_dir, stuck_t, aim_y;
    vec3 stuck_ref, last_seen;
    int has_last;
    uint16_t dist[MAP_W * MAP_H];
} ent_t;

typedef struct { char a[12], v[12]; int ta, tv, w, hs; float t; } kill_t;
typedef struct { vec3 p; float t; uint32_t c; } impact_t;

enum { ST_TITLE, ST_PLAY, ST_MATCHEND };
enum { PH_FREEZE, PH_LIVE, PH_END };

static ent_t ents[MAX_ENTS];
static int nents;
static int state = ST_TITLE, phase;
static float phase_t, round_time, live_t, title_t;
static int score[2], round_no, title_sel, paused, pause_sel;
static char msg[48];
static float msg_t;
static uint32_t msg_col;
static kill_t feed[5];
static impact_t imps[32];
static int imp_next;
static int buy_open, buy_sel, spec;
static int show_scores;
static float hit_marker;
static uint32_t rng = 0x12345u;

/* ------------------------------------------------------------------ */
static float frand(void)
{
    rng = rng * 1664525u + 1013904223u;
    return (float)((rng >> 8) & 0xffff) / 65536.0f;
}
static float frand2(void) { return frand() * 2.0f - 1.0f; }
static float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
static float wrap_pi(float a)
{
    while (a > 3.14159265f) a -= 6.2831853f;
    while (a < -3.14159265f) a += 6.2831853f;
    return a;
}
static vec3 eye_of(const ent_t *e) { return v3(e->pos.x, e->pos.y + EYE_H, e->pos.z); }
static vec3 dir_of(float yaw, float pitch)
{
    return v3(sinf(yaw) * cosf(pitch), sinf(pitch), cosf(yaw) * cosf(pitch));
}

static void set_msg(const char *s, uint32_t c, float t)
{
    strncpy(msg, s, sizeof(msg) - 1);
    msg[sizeof(msg) - 1] = 0;
    msg_col = c;
    msg_t = t;
}

static void add_impact(vec3 p, uint32_t c)
{
    imps[imp_next].p = p;
    imps[imp_next].t = 0.45f;
    imps[imp_next].c = c;
    imp_next = (imp_next + 1) % 32;
}

static int default_pistol(int team) { return team == TEAM_GIGN ? W_USP : W_GLOCK; }

static void give_pistol(ent_t *e, int w)
{
    e->pistol = w;
    e->mag = WEAPONS[w].mag;
    e->reserve = WEAPONS[w].reserve;
    e->slot = 1;
    e->raise_t = 0.45f;
    e->reload_t = 0;
}

/* ------------------------------------------------------------------ */
/* Partida y rondas                                                    */
/* ------------------------------------------------------------------ */
static const char *BOT_NAMES[2][4] = {
    { "FLACO", "CHUCHO", "RAMIRO", "PABLO" },
    { "PIERRE", "JULIEN", "MARCEL", "LUC" },
};

static void new_match(int player_team)
{
    int i, k[2] = { 0, 0 };
    memset(ents, 0, sizeof(ents));
    nents = MAX_ENTS;
    for (i = 0; i < nents; i++) {
        ent_t *e = &ents[i];
        e->used = 1;
        e->team = (i == 0) ? player_team : ((i < 4) ? player_team : !player_team);
        e->bot = (i != 0);
        if (i == 0) strcpy(e->name, "TU");
        else strcpy(e->name, BOT_NAMES[e->team][k[e->team]++ % 4]);
        e->money = 800;
        e->survived = 0;
    }
    score[0] = score[1] = 0;
    round_no = 0;
    memset(feed, 0, sizeof(feed));
    state = ST_PLAY;
}

static void round_start(void)
{
    int i, k[2] = { 0, 0 };
    round_no++;
    phase = PH_FREEZE;
    phase_t = FREEZE_TIME;
    round_time = ROUND_TIME;
    live_t = 0;
    buy_open = 0;
    for (i = 0; i < 32; i++) imps[i].t = 0;
    for (i = 0; i < nents; i++) {
        ent_t *e = &ents[i];
        vec3 sp = map_spawn(e->team, k[e->team]++);
        e->pos = v3(sp.x + frand2() * 0.3f, 0, sp.z + frand2() * 0.3f);
        e->vy = 0;
        e->yaw = e->team == TEAM_GUERRILLA ? 2.4f : -0.8f;
        e->pitch = 0;
        e->punch = 0;
        e->hp = 100;
        if (!e->survived || round_no == 1) {
            e->armor = 0;
            e->helmet = 0;
            give_pistol(e, default_pistol(e->team));
        } else {
            e->mag = WEAPONS[e->pistol].mag;
            e->reserve = WEAPONS[e->pistol].reserve;
        }
        e->alive = 1;
        e->slot = 1;
        e->death_t = 0;
        e->hurt_t = 0;
        e->fire_cd = e->reload_t = e->swing_t = e->kick_t = e->flash_t = 0;
        e->raise_t = 0.45f;
        e->spread = 0;
        e->target = -1;
        e->goal = -1;
        e->think_t = frand() * 0.3f;
        e->has_last = 0;
        e->stuck_t = 1.0f;
        e->stuck_ref = e->pos;
        /* los bots compran */
        if (e->bot) {
            int want = W_GLOCK + (int)(frand() * 5.0f);
            if (want >= W_COUNT) want = W_DEAGLE;
            if (want != e->pistol && e->money >= WEAPONS[want].price + 650 && frand() < 0.7f) {
                e->money -= WEAPONS[want].price;
                give_pistol(e, want);
            }
            if (!e->armor && e->money >= 1000 && frand() < 0.8f) {
                e->money -= 1000;
                e->armor = 100;
                e->helmet = 1;
            }
        }
    }
    spec = 0;
    {
        char b[48];
        sprintf(b, "RONDA %d", round_no);
        set_msg(b, RGB(255, 255, 255), FREEZE_TIME);
    }
}

static void end_round(int winner)
{
    int i;
    char b[48];
    score[winner]++;
    for (i = 0; i < nents; i++) {
        ents[i].money += (ents[i].team == winner) ? 3250 : 1400;
        if (ents[i].money > 16000) ents[i].money = 16000;
        ents[i].survived = ents[i].alive;
    }
    sprintf(b, "%s GANA LA RONDA", TEAM_NAME[winner]);
    set_msg(b, TEAM_COL[winner], 5.0f);
    phase = PH_END;
    phase_t = 5.0f;
    buy_open = 0;
}

/* ------------------------------------------------------------------ */
/* Combate                                                             */
/* ------------------------------------------------------------------ */
static void push_feed(const ent_t *a, const ent_t *v, int w, int hs)
{
    int i;
    for (i = 4; i > 0; i--) feed[i] = feed[i - 1];
    strcpy(feed[0].a, a->name);
    strcpy(feed[0].v, v->name);
    feed[0].ta = a->team;
    feed[0].tv = v->team;
    feed[0].w = w;
    feed[0].hs = hs;
    feed[0].t = 6.0f;
}

static void damage(int vi, int ai, float dmg, int head, int w)
{
    ent_t *v = &ents[vi], *a = &ents[ai];
    int d;
    if (!v->alive) return;
    if (v->armor > 0 && (!head || v->helmet)) {
        v->armor -= (int)(dmg * 0.5f);
        if (v->armor < 0) v->armor = 0;
        dmg *= 0.55f;
    }
    d = (int)(dmg + 0.5f);
    if (d < 1) d = 1;
    v->hp -= d;
    v->hurt_t = 1.0f;
    v->punch -= 0.03f;
    if (ai == 0) hit_marker = 0.25f;
    if (v->bot && v->target < 0) { /* se da vuelta hacia el atacante */
        v->target = ai;
        v->react_t = 0.25f + frand() * 0.3f;
    }
    if (v->hp <= 0) {
        v->hp = 0;
        v->alive = 0;
        v->death_t = 0.001f;
        v->deaths++;
        a->kills++;
        a->money += (w == W_KNIFE) ? 1500 : 300;
        if (a->money > 16000) a->money = 16000;
        push_feed(a, v, w, head);
        if (vi == 0) spec = 0;
    }
}

/* rayo contra cilindro vertical; devuelve t o -1 */
static float ray_cyl(vec3 o, vec3 d, vec3 c, float r, float y0, float y1)
{
    float ox = o.x - c.x, oz = o.z - c.z;
    float A = d.x * d.x + d.z * d.z, B = 2.0f * (ox * d.x + oz * d.z), C = ox * ox + oz * oz - r * r;
    float disc, t, y;
    if (A < 1e-6f) return -1;
    disc = B * B - 4 * A * C;
    if (disc < 0) return -1;
    disc = sqrtf(disc);
    t = (-B - disc) / (2 * A);
    if (t < 0) t = (-B + disc) / (2 * A);
    if (t < 0) return -1;
    y = o.y + d.y * t - c.y;
    if (y < y0 || y > y1) return -1;
    return t;
}

static void fire_bullet(int si, vec3 o, vec3 d, int w)
{
    ent_t *s = &ents[si];
    float best = map_ray(o, d, WEAPONS[w].range);
    int hit = -1, head = 0, i;
    float hy = 0;
    for (i = 0; i < nents; i++) {
        ent_t *e = &ents[i];
        float tb, th;
        if (i == si || !e->alive || e->team == s->team) continue;
        tb = ray_cyl(o, d, e->pos, 0.3f, 0.0f, 1.5f);
        th = ray_cyl(o, d, e->pos, 0.16f, 1.5f, 1.84f);
        if (tb >= 0 && tb < best) { best = tb; hit = i; head = 0; }
        if (th >= 0 && th < best) { best = th; hit = i; head = 1; }
    }
    if (hit >= 0) {
        float dmg = (float)WEAPONS[w].damage * clampf(1.0f - best * 0.006f, 0.6f, 1.0f);
        vec3 hp = vadd(o, vscale(d, best));
        hy = hp.y - ents[hit].pos.y;
        if (head) dmg *= 4.0f;
        else if (hy < 0.85f) dmg *= 0.75f;
        add_impact(hp, RGB(170, 20, 20));
        damage(hit, si, dmg, head, w);
    } else if (best < WEAPONS[w].range) {
        add_impact(vadd(o, vscale(d, best - 0.05f)), RGB(120, 104, 80));
    }
}

static void knife_attack(int si)
{
    ent_t *s = &ents[si];
    int i;
    vec3 f = dir_of(s->yaw, 0);
    for (i = 0; i < nents; i++) {
        ent_t *e = &ents[i];
        vec3 d;
        float dist;
        if (i == si || !e->alive || e->team == s->team) continue;
        d = vsub(e->pos, s->pos);
        d.y = 0;
        dist = vlen(d);
        if (dist < WEAPONS[W_KNIFE].range && dist > 0.01f && vdot(vscale(d, 1.0f / dist), f) > 0.6f) {
            /* por la espalda = letal */
            vec3 ef = dir_of(e->yaw, 0);
            int back = vdot(ef, vscale(d, 1.0f / dist)) > 0.5f;
            add_impact(vadd(e->pos, v3(0, 1.2f, 0)), RGB(170, 20, 20));
            damage(i, si, back ? 195.0f : (float)WEAPONS[W_KNIFE].damage, 0, W_KNIFE);
            return;
        }
    }
}

static void start_reload(ent_t *e)
{
    const weapon_t *w = &WEAPONS[e->pistol];
    if (e->slot != 1 || e->reload_t > 0 || e->mag >= w->mag || e->reserve <= 0) return;
    e->reload_t = w->reload;
}

static void try_fire(int idx, float extra_spread)
{
    ent_t *e = &ents[idx];
    const weapon_t *w;
    float sp, yaw, pit;
    if (e->fire_cd > 0 || e->reload_t > 0 || e->raise_t > 0 || !e->alive) return;
    if (e->slot == 0) {
        e->fire_cd = WEAPONS[W_KNIFE].rate;
        e->swing_t = 1.0f;
        knife_attack(idx);
        return;
    }
    w = &WEAPONS[e->pistol];
    if (e->mag <= 0) { start_reload(e); return; }
    e->mag--;
    e->fire_cd = w->rate;
    e->kick_t = 1.0f;
    e->flash_t = 1.0f;
    sp = w->spread + e->spread + extra_spread;
    if (e->speed > 1.0f) sp += 0.02f;
    if (!e->ground) sp += 0.08f;
    e->spread = clampf(e->spread + 0.02f * w->kick, 0, 0.09f);
    yaw = e->yaw + frand2() * sp;
    pit = e->pitch + e->punch + frand2() * sp;
    fire_bullet(idx, eye_of(e), dir_of(yaw, pit), e->pistol);
    e->punch += 0.022f * w->kick;
}

/* ------------------------------------------------------------------ */
/* Movimiento                                                          */
/* ------------------------------------------------------------------ */
static void move_ent(ent_t *e, float strafe, float fwd, int jump, float dt)
{
    vec3 f = v3(sinf(e->yaw), 0, cosf(e->yaw)), r = v3(cosf(e->yaw), 0, -sinf(e->yaw));
    vec3 mv = vadd(vscale(r, strafe), vscale(f, fwd));
    float l = vlen(mv), spd, g;
    vec3 old = e->pos;
    spd = (e->slot == 0) ? 5.6f : (e->pistol == W_DEAGLE ? 4.9f : 5.2f);
    if (l > 1.0f) mv = vscale(mv, 1.0f / l);
    e->pos = vadd(e->pos, vscale(mv, spd * dt));
    map_collide(&e->pos, RADIUS);
    /* gravedad / salto */
    if (jump && e->ground) { e->vy = 5.6f; e->ground = 0; }
    e->vy -= 16.0f * dt;
    e->pos.y += e->vy * dt;
    g = map_ground(e->pos, RADIUS);
    if (e->pos.y <= g) { e->pos.y = g; e->vy = 0; e->ground = 1; }
    else e->ground = 0;
    {
        vec3 dd = vsub(e->pos, old);
        dd.y = 0;
        e->speed = vlen(dd) / (dt > 0 ? dt : 1);
    }
    e->walk += e->speed * dt * 2.4f;
}

static void separate_ents(void)
{
    int i, j;
    for (i = 0; i < nents; i++)
        for (j = i + 1; j < nents; j++) {
            ent_t *a = &ents[i], *b = &ents[j];
            float dx, dz, d2, d;
            if (!a->alive || !b->alive) continue;
            dx = b->pos.x - a->pos.x;
            dz = b->pos.z - a->pos.z;
            d2 = dx * dx + dz * dz;
            if (d2 < (RADIUS * 2) * (RADIUS * 2) && d2 > 1e-6f) {
                d = sqrtf(d2);
                dx /= d; dz /= d;
                d = (RADIUS * 2 - d) * 0.5f;
                a->pos.x -= dx * d; a->pos.z -= dz * d;
                b->pos.x += dx * d; b->pos.z += dz * d;
                map_collide(&a->pos, RADIUS);
                map_collide(&b->pos, RADIUS);
            }
        }
}

static void tick_weapon(ent_t *e, float dt)
{
    if (e->fire_cd > 0) e->fire_cd -= dt;
    if (e->raise_t > 0) e->raise_t -= dt;
    if (e->swing_t > 0) { e->swing_t -= dt * 2.6f; if (e->swing_t < 0) e->swing_t = 0; }
    if (e->kick_t > 0) { e->kick_t -= dt * 7.0f; if (e->kick_t < 0) e->kick_t = 0; }
    if (e->flash_t > 0) { e->flash_t -= dt * 18.0f; if (e->flash_t < 0) e->flash_t = 0; }
    if (e->hurt_t > 0) e->hurt_t -= dt * 2.0f;
    e->spread -= dt * 0.12f;
    if (e->spread < 0) e->spread = 0;
    e->punch *= expf(-dt * 9.0f);
    if (e->reload_t > 0) {
        e->reload_t -= dt;
        if (e->reload_t <= 0) {
            const weapon_t *w = &WEAPONS[e->pistol];
            int need = w->mag - e->mag;
            if (need > e->reserve) need = e->reserve;
            e->mag += need;
            e->reserve -= need;
            e->reload_t = 0;
        }
    }
}

/* ------------------------------------------------------------------ */
/* Bots                                                                */
/* ------------------------------------------------------------------ */
static int can_see(const ent_t *a, const ent_t *b)
{
    vec3 o = eye_of(a), t = v3(b->pos.x, b->pos.y + 1.45f, b->pos.z);
    vec3 d = vsub(t, o);
    float dist = vlen(d);
    if (dist > 50.0f) return 0;
    d = vscale(d, 1.0f / dist);
    if (map_ray(o, d, dist) >= dist - 0.2f) return 1;
    t.y = b->pos.y + 0.9f; /* prueba el pecho (detras de cajas) */
    d = vsub(t, o);
    dist = vlen(d);
    d = vscale(d, 1.0f / dist);
    return map_ray(o, d, dist) >= dist - 0.2f;
}

static void bot_pick_goal(ent_t *e)
{
    int c, tries = 0, cur = (int)(e->pos.z / CELL) * MAP_W + (int)(e->pos.x / CELL);
    if (e->has_last && frand() < 0.8f) {
        c = (int)(e->last_seen.z / CELL) * MAP_W + (int)(e->last_seen.x / CELL);
        e->has_last = 0;
    } else if (frand() < 0.45f) {
        vec3 s = map_spawn(!e->team, (int)(frand() * 9));
        c = (int)(s.z / CELL) * MAP_W + (int)(s.x / CELL);
    } else {
        do {
            c = map_free_cell((int)(frand() * map_free_count()));
            tries++;
        } while (tries < 10 && abs(c % MAP_W - cur % MAP_W) + abs(c / MAP_W - cur / MAP_W) < 6);
    }
    e->goal = c;
    map_bfs(c, e->dist);
}

static void turn_to(ent_t *e, float yaw, float pitch, float rate, float dt)
{
    float dy = wrap_pi(yaw - e->yaw), dp = pitch - e->pitch, m = rate * dt;
    e->yaw = wrap_pi(e->yaw + clampf(dy, -m, m));
    e->pitch += clampf(dp, -m, m);
}

static void bot_update(int idx, float dt)
{
    ent_t *e = &ents[idx];
    float strafe = 0, fwd = 0;
    int i;

    if (phase == PH_FREEZE) { move_ent(e, 0, 0, 0, dt); return; }

    /* buscar enemigo visible */
    e->think_t -= dt;
    if (e->think_t <= 0) {
        int best = -1;
        float bd = 1e9f;
        vec3 f = dir_of(e->yaw, 0);
        e->think_t = 0.2f;
        for (i = 0; i < nents; i++) {
            ent_t *o = &ents[i];
            vec3 d;
            float dist;
            if (!o->alive || o->team == e->team) continue;
            d = vsub(o->pos, e->pos);
            d.y = 0;
            dist = vlen(d);
            if (dist > 1.0f && vdot(vscale(d, 1.0f / dist), f) < -0.15f && i != e->target) continue; /* fuera de vision */
            if (dist < bd && can_see(e, o)) { bd = dist; best = i; }
        }
        if (best >= 0 && best != e->target) {
            e->target = best;
            e->react_t = 0.3f + frand() * 0.4f;
            e->aim_y = 1.15f + frand() * 0.55f;
            e->strafe_dir = frand() < 0.5f ? -1.0f : 1.0f;
        } else if (best < 0 && e->target >= 0) {
            if (ents[e->target].alive) { e->last_seen = ents[e->target].pos; e->has_last = 1; e->goal = -1; }
            e->target = -1;
        }
    }

    if (e->target >= 0 && !ents[e->target].alive) e->target = -1;

    if (e->reload_t <= 0 && e->mag <= 0) start_reload(e);

    if (e->target >= 0) {
        ent_t *t = &ents[e->target];
        vec3 o = eye_of(e), aim = v3(t->pos.x, t->pos.y + e->aim_y, t->pos.z);
        vec3 d = vsub(aim, o);
        float hd = sqrtf(d.x * d.x + d.z * d.z);
        float wy = atan2f(d.x, d.z), wp = atan2f(d.y, hd);
        float err;
        turn_to(e, wy, wp, 4.2f, dt);
        err = fabsf(wrap_pi(wy - e->yaw)) + fabsf(wp - e->pitch);
        e->react_t -= dt;
        e->strafe_t -= dt;
        if (e->strafe_t <= 0) { e->strafe_t = 0.5f + frand() * 0.8f; e->strafe_dir = -e->strafe_dir; }
        if (e->react_t <= 0 && err < 0.07f && e->reload_t <= 0 && e->fire_cd <= 0) {
            try_fire(idx, 0.03f); /* se queda quieto para disparar */
        } else {
            strafe = e->strafe_dir;
            if (hd > 14.0f) fwd = 0.6f;
            if (e->reload_t > 0) fwd = -0.5f;
        }
    } else {
        /* seguir el camino BFS hacia el objetivo */
        int cx = (int)(e->pos.x / CELL), cz = (int)(e->pos.z / CELL), cur = cz * MAP_W + cx;
        if (e->goal < 0 || e->dist[cur] == 0 || e->dist[cur] == 0xffff) bot_pick_goal(e);
        if (e->dist[cur] != 0xffff && e->dist[cur] > 0) {
            static const int DX[4] = { 1, -1, 0, 0 }, DZ[4] = { 0, 0, 1, -1 };
            int k, bn = -1;
            uint16_t bdv = e->dist[cur];
            for (k = 0; k < 4; k++) {
                int nx = cx + DX[k], nz = cz + DZ[k];
                if (!map_walkable(nx, nz)) continue;
                if (e->dist[nz * MAP_W + nx] < bdv) { bdv = e->dist[nz * MAP_W + nx]; bn = nz * MAP_W + nx; }
            }
            if (bn >= 0) {
                float tx = (bn % MAP_W + 0.5f) * CELL, tz = (bn / MAP_W + 0.5f) * CELL;
                /* mantenerse centrado en la celda actual al girar */
                float ccx = (cx + 0.5f) * CELL, ccz = (cz + 0.5f) * CELL;
                vec3 d = v3(tx - e->pos.x + (ccx - e->pos.x) * 0.5f, 0, tz - e->pos.z + (ccz - e->pos.z) * 0.5f);
                float wy = atan2f(d.x, d.z);
                turn_to(e, wy, 0, 5.0f, dt);
                fwd = fabsf(wrap_pi(wy - e->yaw)) < 0.8f ? 1.0f : 0.3f;
            }
        }
        /* atascado? */
        e->stuck_t -= dt;
        if (e->stuck_t <= 0) {
            vec3 dd = vsub(e->pos, e->stuck_ref);
            if (vlen(dd) < 0.5f) { e->goal = -1; e->yaw += frand2() * 1.5f; }
            e->stuck_ref = e->pos;
            e->stuck_t = 1.2f;
        }
    }
    move_ent(e, strafe, fwd, 0, dt);
}

/* ------------------------------------------------------------------ */
/* Jugador                                                             */
/* ------------------------------------------------------------------ */
static const int BUY_ITEMS[7] = { W_GLOCK, W_USP, W_BERETTA, W_M1911, W_DEAGLE, -1, -2 };

static void player_buy(ent_t *e, int item)
{
    if (item >= 0) {
        if (e->pistol == item) { set_msg("YA TENES ESA ARMA", RGB(255, 255, 255), 1.5f); return; }
        if (e->money < WEAPONS[item].price) { set_msg("NO TENES SUFICIENTE DINERO", RGB(255, 90, 90), 1.5f); return; }
        e->money -= WEAPONS[item].price;
        give_pistol(e, item);
    } else {
        int price = item == -1 ? 650 : 1000;
        if (e->armor >= 100 && (item == -1 || e->helmet)) { set_msg("YA TENES CHALECO", RGB(255, 255, 255), 1.5f); return; }
        if (e->money < price) { set_msg("NO TENES SUFICIENTE DINERO", RGB(255, 90, 90), 1.5f); return; }
        e->money -= price;
        e->armor = 100;
        if (item == -2) e->helmet = 1;
    }
}

static int can_buy(void) { return phase == PH_FREEZE || (phase == PH_LIVE && live_t < BUY_TIME); }

static void player_update(const pad_t *p, float dt)
{
    ent_t *e = &ents[0];
    float mx = p->lx, mz = -p->ly;
    int frozen = (phase == PH_FREEZE);

    if (!e->alive) {
        /* espectador: L1/R1 cambian de jugador */
        if (p->pressed & (BTN_R1 | BTN_L1)) {
            int i;
            for (i = 1; i <= nents; i++) {
                int c = (spec + i) % nents;
                if (ents[c].alive && ents[c].team == e->team) { spec = c; break; }
            }
        }
        return;
    }

    /* mirar con el stick derecho */
    e->yaw = wrap_pi(e->yaw + p->rx * fabsf(p->rx) * 3.0f * dt);
    e->pitch = clampf(e->pitch - p->ry * fabsf(p->ry) * 2.1f * dt, -1.35f, 1.35f);

    if (buy_open) {
        if (p->pressed & BTN_UP) buy_sel = (buy_sel + 6) % 7;
        if (p->pressed & BTN_DOWN) buy_sel = (buy_sel + 1) % 7;
        if (p->pressed & BTN_CROSS) player_buy(e, BUY_ITEMS[buy_sel]);
        if (p->pressed & (BTN_CIRCLE | BTN_TRIANGLE)) buy_open = 0;
        if (!can_buy()) buy_open = 0;
    } else {
        if (p->held & BTN_UP) mz = 1;
        if (p->held & BTN_DOWN) mz = -1;
        if (p->held & BTN_LEFT) mx = -1;
        if (p->held & BTN_RIGHT) mx = 1;
        if ((p->pressed & BTN_TRIANGLE) && can_buy()) { buy_open = 1; }
        if (p->pressed & BTN_L1) {
            e->slot = !e->slot;
            e->raise_t = 0.4f;
            e->reload_t = 0;
        }
        if (p->pressed & BTN_SQUARE) start_reload(e);
        if (!frozen) {
            if (e->slot == 0 ? (p->held & (BTN_R1 | BTN_R2)) : (p->pressed & (BTN_R1 | BTN_R2)))
                try_fire(0, 0);
        }
    }
    if (frozen) { mx = 0; mz = 0; }
    move_ent(e, mx, mz, !frozen && (p->pressed & BTN_CROSS) && !buy_open, dt);
}

/* ------------------------------------------------------------------ */
/* Dibujo                                                              */
/* ------------------------------------------------------------------ */
static void draw_world(const camera_t *cam, int skip)
{
    int i;
    gfx_set_camera(cam);
    map_draw(cam);
    for (i = 0; i < nents; i++) {
        ent_t *e = &ents[i];
        vec3 d;
        float dist;
        if (i == skip || !e->used) continue;
        d = vsub(e->pos, cam->pos);
        dist = vlen(d);
        if (dist > 60.0f) continue;
        model_soldier(e->team, e->pos, e->yaw, e->pitch, e->walk, e->alive ? 0.0f : e->death_t,
                      e->slot ? e->pistol : W_KNIFE, e->flash_t, dist > 14.0f);
    }
    for (i = 0; i < 32; i++) {
        impact_t *m = &imps[i];
        float s;
        xform id = xf_identity();
        if (m->t <= 0) continue;
        s = 0.03f + (0.45f - m->t) * 0.12f;
        gfx_box1(&id, v3(m->p.x - s, m->p.y - s + (0.45f - m->t) * 0.3f, m->p.z - s),
                 v3(m->p.x + s, m->p.y + s + (0.45f - m->t) * 0.3f, m->p.z + s), m->c);
    }
}

static void draw_viewmodel(const ent_t *e)
{
    vm_anim_t a;
    float sp = clampf(e->speed / 5.0f, 0, 1);
    memset(&a, 0, sizeof(a));
    a.bob_x = sinf(e->walk * 0.5f) * 0.012f * sp;
    a.bob_y = -fabsf(cosf(e->walk * 0.5f)) * 0.012f * sp;
    a.kick = e->kick_t;
    a.flash = e->flash_t;
    a.swing = e->swing_t;
    a.raise = 1.0f - clampf(e->raise_t / 0.45f, 0, 1);
    if (e->slot == 1 && e->reload_t > 0)
        a.reload = 1.0f - e->reload_t / WEAPONS[e->pistol].reload;
    model_viewmodel(e->team, e->slot ? e->pistol : W_KNIFE, &a);
}

static void hud_text_right(float x, float y, int s, uint32_t c, const char *t)
{
    gfx_text(x - gfx_text_w(s, t), y, s, c, t);
}

static void draw_feed(void)
{
    int i;
    float y = 12;
    for (i = 0; i < 5; i++) {
        kill_t *k = &feed[i];
        char w[24];
        float x;
        if (k->t <= 0) continue;
        sprintf(w, " [%s%s] ", k->w == W_KNIFE ? "CUCHILLO" : WEAPONS[k->w].name, k->hs ? " *" : "");
        x = g_scr_w - 10 - gfx_text_w(1, k->a) - gfx_text_w(1, w) - gfx_text_w(1, k->v) - 12;
        gfx_rect(x - 4, y - 3, g_scr_w - 8, y + 10, 0, 0x40);
        gfx_text(x, y, 1, TEAM_COL[k->ta], k->a);
        x += gfx_text_w(1, k->a) + 6;
        gfx_text(x, y, 1, RGB(230, 230, 230), w);
        x += gfx_text_w(1, w) + 6;
        gfx_text(x, y, 1, TEAM_COL[k->tv], k->v);
        y += 15;
    }
}

static void draw_scoreboard(void)
{
    int t, i;
    float x0 = g_scr_w * 0.5f - 230, y0 = 80;
    char b[64];
    gfx_rect(x0, y0, x0 + 460, y0 + 250, 0, 0x60);
    for (t = 0; t < 2; t++) {
        float y = y0 + 12 + t * 120;
        sprintf(b, "%s  %d", TEAM_NAME[t], score[t]);
        gfx_text(x0 + 14, y, 2, TEAM_COL[t], b);
        gfx_text(x0 + 300, y + 4, 1, RGB(200, 200, 200), "BAJAS  MUERTES");
        y += 24;
        for (i = 0; i < nents; i++) {
            ent_t *e = &ents[i];
            if (e->team != t) continue;
            sprintf(b, "%-10s %s", e->name, e->alive ? "" : "MUERTO");
            gfx_text(x0 + 20, y, 1, i == 0 ? RGB(255, 255, 255) : TEAM_COL[t], b);
            sprintf(b, "%5d  %7d", e->kills, e->deaths);
            gfx_text(x0 + 300, y, 1, RGB(230, 230, 230), b);
            y += 18;
        }
    }
}

static void draw_hud(const ent_t *view, int spectating)
{
    ent_t *me = &ents[0];
    char b[64];
    float cx = g_scr_w * 0.5f, cy = g_scr_h * 0.5f;
    int i;

    /* pantalla roja al recibir dano */
    if (view->hurt_t > 0 && view->alive)
        gfx_rect(0, 0, g_scr_w, g_scr_h, RGB(200, 0, 0), (int)(view->hurt_t * 40));

    /* mira estilo clasico */
    if (view->alive && !(view->slot == 1 && view->reload_t > 0)) {
        float gap = 5 + (view->spread + (view->speed > 1 ? 0.02f : 0)) * 260.0f;
        uint32_t c = RGB(60, 255, 60);
        gfx_rect(cx - gap - 9, cy - 1, cx - gap, cy + 1, c, 0x70);
        gfx_rect(cx + gap, cy - 1, cx + gap + 9, cy + 1, c, 0x70);
        gfx_rect(cx - 1, cy - gap - 8, cx + 1, cy - gap, c, 0x70);
        gfx_rect(cx - 1, cy + gap, cx + 1, cy + gap + 8, c, 0x70);
        if (hit_marker > 0 && !spectating) {
            gfx_rect(cx - 6, cy - 6, cx - 3, cy - 3, RGB(255, 255, 255), 0x80);
            gfx_rect(cx + 3, cy - 6, cx + 6, cy - 3, RGB(255, 255, 255), 0x80);
            gfx_rect(cx - 6, cy + 3, cx - 3, cy + 6, RGB(255, 255, 255), 0x80);
            gfx_rect(cx + 3, cy + 3, cx + 6, cy + 6, RGB(255, 255, 255), 0x80);
        }
    }

    /* radar */
    {
        float rs = 3.0f, rx = 10, ry = 10;
        map_draw_radar(rx, ry, rs);
        for (i = 0; i < nents; i++) {
            ent_t *e = &ents[i];
            float px, pz;
            if (!e->alive || e->team != me->team) continue;
            px = rx + e->pos.x / CELL * rs;
            pz = ry + e->pos.z / CELL * rs;
            gfx_rect(px - 2, pz - 2, px + 2, pz + 2, e == view ? RGB(255, 255, 255) : RGB(80, 220, 80), 0x80);
        }
    }

    /* tiempo y marcador */
    {
        int t = (int)(phase == PH_FREEZE ? ROUND_TIME : round_time);
        if (t < 0) t = 0;
        sprintf(b, "%d:%02d", t / 60, t % 60);
        gfx_text_center(10, 2, round_time < 15 ? RGB(255, 80, 60) : HUD_COL, b);
        sprintf(b, "%d", score[0]);
        hud_text_right(cx - 46, 13, 2, TEAM_COL[0], b);
        sprintf(b, "%d", score[1]);
        gfx_text(cx + 46, 13, 2, TEAM_COL[1], b);
    }

    draw_feed();

    if (spectating) {
        sprintf(b, "ESPECTANDO A %s  (L1/R1)", view->name);
        gfx_text_center(g_scr_h - 60, 2, RGB(230, 230, 230), b);
    } else {
        /* vida, chaleco, dinero, municion */
        float by = g_scr_h - 40;
        sprintf(b, "+ %d", me->hp);
        gfx_text(16, by, 3, me->hp <= 25 ? RGB(255, 60, 40) : HUD_COL, b);
        if (me->armor > 0) {
            sprintf(b, "%s %d", me->helmet ? "CHALECO+C" : "CHALECO", me->armor);
            gfx_text(150, by + 7, 2, HUD_COL, b);
        }
        sprintf(b, "$ %d", me->money);
        gfx_text(16, by - 30, 2, RGB(120, 230, 120), b);
        if (me->slot == 1) {
            sprintf(b, "%d | %d", me->mag, me->reserve);
            hud_text_right(g_scr_w - 16, by, 3, me->mag == 0 ? RGB(255, 60, 40) : HUD_COL, b);
            hud_text_right(g_scr_w - 16, by - 18, 1, RGB(220, 220, 220), WEAPONS[me->pistol].name);
        } else {
            hud_text_right(g_scr_w - 16, by + 6, 2, HUD_COL, "CUCHILLO");
        }
        if (can_buy() && !buy_open && phase != PH_END)
            gfx_text_center(g_scr_h - 86, 1, RGB(230, 230, 230), "TRIANGULO: COMPRAR");
    }

    if (msg_t > 0) gfx_text_center(g_scr_h * 0.3f, 2, msg_col, msg);

    /* menu de compra */
    if (buy_open) {
        float x0 = cx - 170, y0 = 90;
        gfx_rect(x0, y0, x0 + 340, y0 + 220, 0, 0x68);
        gfx_text(x0 + 14, y0 + 12, 2, HUD_COL, "COMPRAR");
        sprintf(b, "$ %d", me->money);
        hud_text_right(x0 + 326, y0 + 12, 2, RGB(120, 230, 120), b);
        for (i = 0; i < 7; i++) {
            int it = BUY_ITEMS[i], price;
            const char *n;
            float y = y0 + 46 + i * 22;
            uint32_t c;
            if (it >= 0) { n = WEAPONS[it].name; price = WEAPONS[it].price; }
            else if (it == -1) { n = "CHALECO"; price = 650; }
            else { n = "CHALECO + CASCO"; price = 1000; }
            c = me->money >= price ? RGB(235, 235, 235) : RGB(130, 130, 130);
            if (i == buy_sel) gfx_rect(x0 + 8, y - 4, x0 + 332, y + 14, HUD_COL, 0x40);
            sprintf(b, "%d. %s", i + 1, n);
            gfx_text(x0 + 16, y, 1, c, b);
            sprintf(b, "$%d", price);
            hud_text_right(x0 + 320, y, 1, c, b);
        }
        gfx_text(x0 + 14, y0 + 202, 1, RGB(180, 180, 180), "X COMPRAR   O SALIR");
    }

    if (show_scores) draw_scoreboard();
}

/* ------------------------------------------------------------------ */
/* Pantallas                                                           */
/* ------------------------------------------------------------------ */
static void title_frame(const pad_t *p, float dt)
{
    camera_t cam;
    float sx, sy;
    vec3 pg = v3(25.2f, 0, 31.0f), pn = v3(20.8f, 0, 31.0f);
    title_t += dt;
    if (p->pressed & (BTN_LEFT | BTN_RIGHT | BTN_UP | BTN_DOWN)) title_sel = !title_sel;
    if (p->pressed & (BTN_CROSS | BTN_START)) {
        new_match(title_sel ? TEAM_GIGN : TEAM_GUERRILLA);
        round_start();
        return;
    }
    cam.pos = v3(23.0f, 1.45f, 35.4f);
    cam.yaw = 3.14159f;
    cam.pitch = -0.08f;
    gfx_begin(RGB(150, 186, 222));
    gfx_set_camera(&cam);
    map_draw(&cam);
    model_soldier(TEAM_GUERRILLA, pg, sinf(title_t * 0.7f) * 0.6f, 0, 0, 0, W_GLOCK, 0, 0);
    model_soldier(TEAM_GIGN, pn, -sinf(title_t * 0.7f) * 0.6f, 0, 0, 0, W_USP, 0, 0);

    gfx_rect(0, 0, g_scr_w, 100, 0, 0x50);
    gfx_text_center(22, 5, HUD_COL, "CONTRAATAQUE");
    gfx_text_center(68, 1, RGB(230, 230, 230), "EDICION PS2  -  HOMENAJE A LOS FPS CLASICOS");

    if (gfx_project(v3(pg.x, 2.05f, pg.z), &sx, &sy)) {
        const char *t1 = "EQUIPO 1", *t2 = "GUERRILLA";
        uint32_t c = title_sel == 0 ? HUD_COL : RGB(180, 180, 180);
        if (title_sel == 0) gfx_rect(sx - 80, sy - 40, sx + 80, sy - 2, 0, 0x50);
        gfx_text(sx - gfx_text_w(2, t1) / 2, sy - 36, 2, c, t1);
        gfx_text(sx - gfx_text_w(2, t2) / 2, sy - 18, 2, TEAM_COL[0], t2);
    }
    if (gfx_project(v3(pn.x, 2.05f, pn.z), &sx, &sy)) {
        const char *t1 = "EQUIPO 2", *t2 = "GIGN";
        uint32_t c = title_sel == 1 ? HUD_COL : RGB(180, 180, 180);
        if (title_sel == 1) gfx_rect(sx - 80, sy - 40, sx + 80, sy - 2, 0, 0x50);
        gfx_text(sx - gfx_text_w(2, t1) / 2, sy - 36, 2, c, t1);
        gfx_text(sx - gfx_text_w(2, t2) / 2, sy - 18, 2, TEAM_COL[1], t2);
    }
    gfx_rect(0, g_scr_h - 52, g_scr_w, g_scr_h, 0, 0x50);
    gfx_text_center(g_scr_h - 44, 1, RGB(255, 255, 255), "IZQ/DER: ELEGIR EQUIPO     X: JUGAR");
    gfx_text_center(g_scr_h - 26, 1, RGB(190, 190, 190), "R1/R2 DISPARAR  L1 CUCHILLO  CUADRADO RECARGAR  X SALTAR  SELECT PUNTAJE");
    gfx_end();
}

static void play_update(const pad_t *p, float dt)
{
    int i, alive[2] = { 0, 0 };

    if (p->pressed & BTN_START) { paused = !paused; pause_sel = 0; buy_open = 0; }
    show_scores = (p->held & BTN_SELECT) != 0;
    if (paused) {
        if (p->pressed & (BTN_UP | BTN_DOWN)) pause_sel = !pause_sel;
        if (p->pressed & BTN_CROSS) {
            if (pause_sel == 0) paused = 0;
            else { paused = 0; state = ST_TITLE; }
        }
        return;
    }

    if (msg_t > 0) msg_t -= dt;
    if (hit_marker > 0) hit_marker -= dt;
    for (i = 0; i < 5; i++) if (feed[i].t > 0) feed[i].t -= dt;
    for (i = 0; i < 32; i++) if (imps[i].t > 0) imps[i].t -= dt;

    for (i = 0; i < nents; i++) {
        ent_t *e = &ents[i];
        if (!e->alive) {
            if (e->death_t > 0 && e->death_t < 1.0f) e->death_t += dt * 2.2f;
            continue;
        }
        tick_weapon(e, dt);
        if (e->bot) bot_update(i, dt);
        else player_update(p, dt);
    }
    if (!ents[0].alive) player_update(p, dt);
    separate_ents();

    switch (phase) {
    case PH_FREEZE:
        phase_t -= dt;
        if (phase_t <= 0) { phase = PH_LIVE; set_msg("VAMOS, VAMOS, VAMOS!", RGB(255, 255, 255), 1.5f); }
        break;
    case PH_LIVE:
        round_time -= dt;
        live_t += dt;
        for (i = 0; i < nents; i++) if (ents[i].alive) alive[ents[i].team]++;
        if (alive[0] == 0) end_round(TEAM_GIGN);
        else if (alive[1] == 0) end_round(TEAM_GUERRILLA);
        else if (round_time <= 0) end_round(TEAM_GIGN); /* se acabo el tiempo: defensores ganan */
        break;
    case PH_END:
        phase_t -= dt;
        if (phase_t <= 0) {
            if (score[0] >= WIN_ROUNDS || score[1] >= WIN_ROUNDS) state = ST_MATCHEND;
            else round_start();
        }
        break;
    }
}

static void play_draw(void)
{
    camera_t cam;
    ent_t *view = &ents[0];
    int spectating = 0;

    if (!ents[0].alive) {
        if (!ents[spec].alive || ents[spec].team != ents[0].team || spec == 0) {
            int i;
            spec = 0;
            for (i = 1; i < nents; i++) if (ents[i].alive && ents[i].team == ents[0].team) { spec = i; break; }
            if (spec == 0) for (i = 1; i < nents; i++) if (ents[i].alive) { spec = i; break; }
        }
        if (spec != 0) { view = &ents[spec]; spectating = 1; }
    }

    if (view->alive) {
        cam.pos = eye_of(view);
        cam.yaw = view->yaw;
        cam.pitch = clampf(view->pitch + view->punch, -1.5f, 1.5f);
    } else {
        /* camara de muerte: arriba del cuerpo */
        cam.pos = v3(view->pos.x, view->pos.y + 2.6f, view->pos.z - 1.5f);
        cam.yaw = 0;
        cam.pitch = -0.9f;
    }

    gfx_begin(RGB(150, 186, 222));
    draw_world(&cam, view->alive ? (int)(view - ents) : -1);
    if (view->alive) draw_viewmodel(view);
    draw_hud(view, spectating);

    if (paused) {
        float x0 = g_scr_w * 0.5f - 130, y0 = g_scr_h * 0.5f - 60;
        gfx_rect(x0, y0, x0 + 260, y0 + 120, 0, 0x68);
        gfx_text_center(y0 + 14, 2, HUD_COL, "PAUSA");
        gfx_text_center(y0 + 50, 2, pause_sel == 0 ? RGB(255, 255, 255) : RGB(140, 140, 140), "CONTINUAR");
        gfx_text_center(y0 + 78, 2, pause_sel == 1 ? RGB(255, 255, 255) : RGB(140, 140, 140), "SALIR AL MENU");
    }
    gfx_end();
}

static void matchend_frame(const pad_t *p)
{
    camera_t cam;
    int w = score[1] > score[0];
    char b[48];
    cam.pos = v3(23.0f, 6.0f, 23.0f);
    cam.yaw = title_t * 0.1f;
    cam.pitch = -0.35f;
    title_t += 0.016f;
    gfx_begin(RGB(150, 186, 222));
    gfx_set_camera(&cam);
    draw_world(&cam, -1);
    sprintf(b, "%s GANA LA PARTIDA", TEAM_NAME[w]);
    gfx_text_center(30, 3, TEAM_COL[w], b);
    show_scores = 1;
    draw_scoreboard();
    gfx_text_center(g_scr_h - 40, 2, RGB(255, 255, 255), "START: VOLVER AL MENU");
    gfx_end();
    if (p->pressed & (BTN_START | BTN_CROSS)) state = ST_TITLE;
}

/* ------------------------------------------------------------------ */
void game_init(void)
{
    map_init();
    gfx_set_fog(RGB(176, 192, 206), 22.0f, 58.0f);
    state = ST_TITLE;
}

void game_frame(const pad_t *p, float dt)
{
    if (dt > 0.066f) dt = 0.066f;
    switch (state) {
    case ST_TITLE: title_frame(p, dt); break;
    case ST_PLAY: play_update(p, dt); if (state == ST_PLAY) play_draw(); break;
    default: matchend_frame(p); break;
    }
}

#ifdef PC_PREVIEW
/* escenas fijas para generar capturas de prueba en la PC */
void game_debug_scene(int scene)
{
    pad_t z;
    memset(&z, 0, sizeof(z));
    if (scene == 0) { state = ST_TITLE; title_t = 0.6f; title_frame(&z, 0); return; }
    new_match(scene == 2 || scene == 4 ? TEAM_GUERRILLA : TEAM_GIGN);
    round_start();
    phase = PH_LIVE;
    live_t = 5;
    msg_t = 0;
    {
        ent_t *me = &ents[0];
        int i, k = 0;
        me->raise_t = 0;
        me->pos = v3(23.0f, 0, 33.0f);
        me->yaw = 3.14159f;
        me->pitch = -0.02f;
        for (i = 1; i < nents; i++) {
            ent_t *e = &ents[i];
            e->raise_t = 0;
            if (e->team != me->team) {
                e->pos = v3(19.0f + k * 3.0f, 0, 22.0f - k * 1.5f);
                e->yaw = 0.3f * k;
                e->walk = k * 1.3f;
                k++;
            } else {
                e->pos = v3(5.0f + i, 0, 44.0f);
            }
        }
        if (scene == 2) { me->slot = 0; me->swing_t = 0.55f; }
        if (scene == 3) { buy_open = 1; buy_sel = 4; me->money = 2650; }
        if (scene == 4) { give_pistol(me, W_DEAGLE); me->raise_t = 0; me->flash_t = 0.8f; me->kick_t = 0.6f; me->hp = 46; me->armor = 85; me->helmet = 1; }
        if (scene == 5) { give_pistol(me, W_M1911); me->raise_t = 0; me->reload_t = 1.1f; }
        if (scene == 6) { give_pistol(me, W_BERETTA); me->raise_t = 0; me->pos = v3(23.0f, 0, 26.0f); me->kills = 2;
            push_feed(me, &ents[5], W_BERETTA, 1); push_feed(&ents[6], &ents[2], W_DEAGLE, 0); }
    }
    play_draw();
}
#endif
