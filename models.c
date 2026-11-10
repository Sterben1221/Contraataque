/* models.c - modelos low-poly hechos con cajas (estilo PS2 / CS 1.6) */
#include "models.h"

/* ---------------- paletas de los equipos ---------------- */
/* Equipo 2: GIGN (camuflaje verde, pasamontanas azul, mascara de gas) */
static const uint32_t GIGN_CAMO[4] = { RGB(66, 88, 48), RGB(44, 62, 34), RGB(88, 108, 62), RGB(32, 44, 26) };
static const uint32_t GIGN_VEST[3] = { RGB(54, 72, 42), RGB(44, 58, 34), RGB(62, 80, 48) };
/* Equipo 1: GUERRILLA (camuflaje desierto, pasamontanas negro) */
static const uint32_t G_JACKET[4] = { RGB(178, 152, 106), RGB(126, 98, 62), RGB(154, 128, 86), RGB(100, 82, 52) };
static const uint32_t G_PANTS[3] = { RGB(118, 96, 64), RGB(138, 112, 76), RGB(92, 76, 50) };

#define SKIN RGB(206, 152, 116)
#define SKIN_D RGB(176, 124, 94)
#define GLOVE RGB(34, 34, 36)

static void limb(const xform *b, vec3 a, vec3 c, float w, float h, const uint32_t *pal, int n, int sub, uint32_t seed)
{
    float len;
    xform s = xf_segment(b, a, c, &len);
    gfx_box(&s, v3(-w * 0.5f, -h * 0.5f, 0), v3(w * 0.5f, h * 0.5f, len), pal, n, sub, seed);
}

static xform mulx(xform a, xform b) { return xf_mul(&a, &b); }

/* ------------------------------------------------------------------ */
/* Armas                                                               */
/* ------------------------------------------------------------------ */
static float muzzle_z(int w)
{
    switch (w) {
    case W_USP: return 0.29f;
    case W_BERETTA: return 0.13f;
    case W_DEAGLE: return 0.165f;
    default: return 0.118f;
    }
}

static void muzzle_flash(const xform *m, float z, float f, int small)
{
    float s = (small ? 0.016f : 0.034f) * (0.6f + 0.4f * f);
    xform r = mulx(*m, xf_translate(0, 0.016f, z + s * 0.6f));
    xform r2 = mulx(r, xf_rot_z(0.785f));
    gfx_box1(&r, v3(-s, -0.006f, -0.01f), v3(s, 0.006f, s * 1.4f), RGB(255, 210, 90));
    gfx_box1(&r, v3(-0.006f, -s, -0.01f), v3(0.006f, s, s * 1.4f), RGB(255, 170, 50));
    gfx_box1(&r2, v3(-s * 0.7f, -0.005f, 0), v3(s * 0.7f, 0.005f, s), RGB(255, 240, 160));
}

static void knife(const xform *m)
{
    static const uint32_t BLADE[3] = { RGB(170, 172, 176), RGB(150, 152, 156), RGB(186, 188, 190) };
    int i;
    /* mango con anillos */
    gfx_box1(m, v3(-0.012f, -0.016f, -0.12f), v3(0.012f, 0.016f, -0.004f), RGB(30, 32, 30));
    for (i = 0; i < 4; i++) {
        float z = -0.108f + i * 0.026f;
        gfx_box1(m, v3(-0.014f, -0.018f, z), v3(0.014f, 0.018f, z + 0.01f), RGB(44, 46, 44));
    }
    gfx_box1(m, v3(-0.014f, -0.018f, -0.132f), v3(0.014f, 0.018f, -0.12f), RGB(70, 70, 72)); /* pomo */
    gfx_box1(m, v3(-0.01f, -0.04f, -0.004f), v3(0.01f, 0.04f, 0.006f), RGB(60, 60, 64));      /* guarda */
    /* hoja */
    gfx_box(m, v3(-0.0035f, -0.02f, 0.006f), v3(0.0035f, 0.02f, 0.14f), BLADE, 3, 2, 77);
    gfx_box1(m, v3(-0.003f, -0.016f, 0.14f), v3(0.003f, 0.012f, 0.165f), RGB(176, 178, 182));
    gfx_box1(m, v3(-0.0025f, -0.012f, 0.165f), v3(0.0025f, 0.002f, 0.185f), RGB(190, 192, 196));
    gfx_box1(m, v3(-0.002f, -0.01f, 0.185f), v3(0.002f, -0.004f, 0.198f), RGB(200, 200, 204));
    /* sierra en el lomo */
    for (i = 0; i < 6; i++) {
        float z = 0.02f + i * 0.016f;
        gfx_box1(m, v3(-0.003f, 0.02f, z), v3(0.003f, 0.027f, z + 0.008f), RGB(120, 122, 126));
    }
    /* canal / agujero */
    gfx_box1(m, v3(-0.0038f, -0.002f, 0.04f), v3(0.0038f, 0.006f, 0.11f), RGB(96, 98, 102));
}

void model_weapon(const xform *m0, int w, float slide, float flash)
{
    xform m = *m0, g;
    uint32_t cs, cf, cg;
    float sz = 0.115f, sy = 0.030f;
    int wood = 0;

    if (w == W_KNIFE) { knife(&m); return; }
    if (w == W_DEAGLE) { xform s = xf_scale(1.15f); m = mulx(m, s); sz = 0.15f; sy = 0.036f; }

    switch (w) {
    case W_GLOCK:   cs = RGB(80, 82, 86);    cf = RGB(36, 36, 38); cg = RGB(40, 40, 42); break;
    case W_USP:     cs = RGB(46, 46, 48);    cf = RGB(34, 34, 36); cg = RGB(38, 38, 40); break;
    case W_BERETTA: cs = RGB(54, 54, 56);    cf = RGB(46, 46, 48); cg = RGB(112, 66, 40); wood = 1; break;
    case W_M1911:   cs = RGB(60, 68, 90);    cf = RGB(56, 64, 84); cg = RGB(130, 76, 42); wood = 1; break;
    default:        cs = RGB(186, 188, 192); cf = RGB(170, 172, 176); cg = RGB(30, 30, 32); break;
    }

    /* corredera */
    if (w == W_BERETTA) {
        gfx_box1(&m, v3(-0.0145f, 0.0f, -0.075f + slide), v3(0.0145f, sy, 0.03f + slide), cs);
        gfx_box1(&m, v3(-0.0145f, 0.0f, 0.03f + slide), v3(0.0145f, 0.012f, 0.115f + slide), cs);
        gfx_box1(&m, v3(-0.008f, 0.012f, 0.02f), v3(0.008f, 0.026f, 0.13f), RGB(150, 150, 154)); /* canon */
    } else {
        gfx_box1(&m, v3(-0.0145f, 0.0f, -0.075f + slide), v3(0.0145f, sy, sz + slide), cs);
    }
    /* estrias traseras y ventana de expulsion */
    gfx_box1(&m, v3(-0.0150f, 0.004f, -0.068f + slide), v3(0.0150f, sy - 0.004f, -0.05f + slide),
             RGB(CR(cs) * 3 / 4, CG(cs) * 3 / 4, CB(cs) * 3 / 4));
    gfx_box1(&m, v3(0.0146f, 0.012f, 0.0f), v3(0.0152f, sy - 0.004f, 0.04f), RGB(20, 20, 22));
    /* miras */
    gfx_box1(&m, v3(-0.01f, sy, -0.07f + slide), v3(0.01f, sy + 0.007f, -0.06f + slide), RGB(28, 28, 28));
    gfx_box1(&m, v3(-0.003f, sy, sz - 0.014f + slide), v3(0.003f, sy + 0.006f, sz - 0.006f + slide), RGB(28, 28, 28));
    /* armazon, guardamonte y gatillo */
    gfx_box1(&m, v3(-0.013f, -0.016f, -0.05f), v3(0.013f, 0.0f, sz - 0.015f), cf);
    gfx_box1(&m, v3(-0.006f, -0.044f, -0.004f), v3(0.006f, -0.037f, 0.052f), cf);
    gfx_box1(&m, v3(-0.006f, -0.044f, 0.045f), v3(0.006f, -0.016f, 0.052f), cf);
    gfx_box1(&m, v3(-0.003f, -0.034f, 0.01f), v3(0.003f, -0.014f, 0.016f), RGB(24, 24, 24));
    /* empunadura inclinada */
    g = mulx(m, xf_translate(0, -0.004f, -0.034f));
    g = mulx(g, xf_rot_x(0.24f));
    gfx_box1(&g, v3(-0.016f, -0.128f, -0.024f), v3(0.016f, 0.0f, 0.022f), wood ? cf : cg);
    if (wood) {
        gfx_box1(&g, v3(-0.019f, -0.112f, -0.018f), v3(0.019f, -0.012f, 0.016f), cg);
    } else {
        gfx_box1(&g, v3(-0.0175f, -0.105f, -0.02f), v3(0.0175f, -0.03f, 0.018f), RGB(CR(cg) + 10, CG(cg) + 10, CB(cg) + 10));
    }
    gfx_box1(&g, v3(-0.015f, -0.136f, -0.026f), v3(0.015f, -0.124f, 0.024f), RGB(24, 24, 26)); /* base cargador */

    /* extras */
    if (w == W_USP) { /* silenciador */
        static const uint32_t SUP[3] = { RGB(50, 50, 54), RGB(44, 44, 48), RGB(58, 58, 62) };
        gfx_box1(&m, v3(-0.007f, 0.008f, sz + slide), v3(0.007f, 0.022f, sz + 0.02f), RGB(90, 90, 92));
        gfx_box(&m, v3(-0.018f, -0.003f, sz + 0.016f), v3(0.018f, 0.033f, 0.29f), SUP, 3, 3, 5);
    }
    if (w == W_M1911) {
        gfx_box1(&m, v3(-0.006f, 0.016f, -0.088f), v3(0.006f, 0.04f, -0.075f), RGB(40, 40, 44)); /* martillo */
        gfx_box1(&m, v3(-0.0155f, 0.008f, -0.02f), v3(-0.0145f, 0.02f, 0.0f), RGB(40, 40, 44));
    }
    if (w == W_DEAGLE) {
        gfx_box1(&m, v3(-0.006f, sy, -0.04f), v3(0.006f, sy + 0.005f, sz - 0.02f), RGB(150, 152, 156)); /* riel */
        gfx_box1(&m, v3(-0.015f, 0.0f, sz), v3(0.015f, sy, sz + 0.016f), RGB(160, 162, 166));            /* freno */
        gfx_box1(&m, v3(-0.0155f, 0.008f, sz + 0.004f), v3(0.0155f, 0.024f, sz + 0.012f), RGB(20, 20, 20));
        gfx_box1(&m, v3(-0.006f, 0.016f, -0.088f), v3(0.006f, 0.038f, -0.075f), RGB(60, 60, 64));
    }
    if (flash > 0.01f) muzzle_flash(&m, muzzle_z(w), flash, w == W_USP);
}

/* ------------------------------------------------------------------ */
/* Primera persona                                                     */
/* ------------------------------------------------------------------ */
static void forearm(int team, vec3 elbow, vec3 wrist, int right)
{
    const uint32_t *pal = team == TEAM_GIGN ? GIGN_CAMO : G_JACKET;
    xform id = xf_identity();
    float len;
    xform s = xf_segment(&id, elbow, wrist, &len);
    float w = 0.078f;
    gfx_box(&s, v3(-w * 0.5f, -w * 0.5f, 0), v3(w * 0.5f, w * 0.5f, len - 0.03f), pal, 4, 4, right ? 11 : 23);
    /* puno del guante */
    gfx_box1(&s, v3(-w * 0.42f, -w * 0.42f, len - 0.045f), v3(w * 0.42f, w * 0.42f, len + 0.005f), GLOVE);
    if (team == TEAM_GIGN) {
        /* parche redondo (azul y dorado) */
        float sx = right ? w * 0.5f : -w * 0.5f - 0.004f;
        gfx_box1(&s, v3(sx, -0.022f, 0.05f), v3(sx + 0.004f, 0.022f, 0.096f), RGB(200, 160, 70));
        gfx_box1(&s, v3(right ? sx + 0.002f : sx - 0.001f, -0.015f, 0.058f),
                 v3(right ? sx + 0.005f : sx + 0.002f, 0.015f, 0.088f), RGB(40, 80, 170));
        /* banda oscura (como en la referencia) */
        gfx_box1(&s, v3(-w * 0.52f, -w * 0.52f, 0.12f), v3(w * 0.52f, w * 0.52f, 0.17f), RGB(46, 50, 52));
    }
}

/* mano derecha agarrando la empunadura (coordenadas de la empunadura) */
static void right_hand_grip(const xform *g)
{
    int i;
    gfx_box1(g, v3(-0.022f, -0.112f, -0.042f), v3(0.032f, -0.012f, -0.012f), GLOVE);  /* dorso */
    gfx_box1(g, v3(0.016f, -0.112f, -0.03f), v3(0.032f, -0.02f, 0.03f), GLOVE);       /* palma lado */
    for (i = 0; i < 3; i++) {                                                         /* dedos (sin guante) */
        float y0 = -0.052f - i * 0.022f;
        gfx_box1(g, v3(-0.02f, y0 - 0.019f, 0.022f), v3(0.03f, y0, 0.036f), i == 1 ? SKIN_D : SKIN);
    }
    gfx_box1(g, v3(-0.03f, -0.03f, -0.02f), v3(-0.016f, -0.014f, 0.045f), SKIN);      /* pulgar */
}

static void left_hand_support(const xform *g)
{
    int i;
    gfx_box1(g, v3(-0.05f, -0.108f, -0.02f), v3(-0.017f, -0.03f, 0.035f), GLOVE);
    for (i = 0; i < 3; i++) {
        float y0 = -0.05f - i * 0.02f;
        gfx_box1(g, v3(-0.034f, y0 - 0.018f, 0.036f), v3(0.022f, y0, 0.048f), i == 0 ? SKIN : SKIN_D);
    }
    gfx_box1(g, v3(-0.046f, -0.03f, 0.0f), v3(-0.03f, -0.016f, 0.06f), SKIN);
}

void model_viewmodel(int team, int weapon, const vm_anim_t *a)
{
    xform m, g;
    float drop = (1.0f - a->raise) * 0.32f;
    gfx_set_viewmodel(1);

    if (weapon == W_KNIFE) {
        float s = a->swing;
        float arc = s > 0 ? sinf((1.0f - s) * 3.14159f) : 0.0f;
        float yaw = (s > 0) ? (0.75f - 1.6f * (1.0f - s)) : -0.25f;
        vec3 wr;
        m = xf_translate(0.15f + a->bob_x - arc * 0.08f, -0.17f + a->bob_y - drop + arc * 0.03f, 0.34f + arc * 0.08f);
        m = mulx(m, xf_rot_y(yaw));
        m = mulx(m, xf_rot_x(-0.25f));
        m = mulx(m, xf_rot_z(s > 0 ? -0.6f * arc : 0.0f));
        model_weapon(&m, W_KNIFE, 0, 0);
        /* mano en el mango */
        gfx_box1(&m, v3(-0.026f, -0.034f, -0.112f), v3(0.026f, 0.02f, -0.02f), GLOVE);
        gfx_box1(&m, v3(-0.024f, -0.038f, -0.11f), v3(0.022f, -0.022f, -0.03f), SKIN);
        gfx_box1(&m, v3(-0.03f, 0.0f, -0.08f), v3(-0.016f, 0.024f, -0.02f), SKIN);
        wr = xf_apply(&m, v3(0.0f, -0.006f, -0.11f));
        forearm(team, v3(0.28f, -0.36f, 0.16f), wr, 1);
        gfx_set_viewmodel(0);
        return;
    }

    {
        float k = a->kick, r = a->reload;
        float rd = sinf(r * 3.14159f);
        vec3 rw, lw;
        m = xf_translate(0.125f + a->bob_x, -0.15f + a->bob_y - drop - rd * 0.09f, 0.40f - k * 0.035f);
        m = mulx(m, xf_rot_y(-0.05f));
        m = mulx(m, xf_rot_x(-k * 0.32f + rd * 0.55f));
        m = mulx(m, xf_rot_z(rd * 0.5f));
        model_weapon(&m, weapon, -k * 0.03f, a->flash);

        g = mulx(m, weapon == W_DEAGLE ? xf_scale(1.15f) : xf_identity());
        g = mulx(g, xf_translate(0, -0.004f, -0.034f));
        g = mulx(g, xf_rot_x(0.24f));
        right_hand_grip(&g);
        rw = xf_apply(&g, v3(0.006f, -0.08f, -0.03f));
        forearm(team, v3(0.27f, -0.35f, 0.17f), rw, 1);

        if (r < 0.05f || r > 0.95f) {
            left_hand_support(&g);
            lw = xf_apply(&g, v3(-0.036f, -0.075f, -0.004f));
        } else {
            /* mano izquierda va a buscar el cargador */
            xform lh = xf_translate(0.06f, -0.26f + rd * 0.05f, 0.38f);
            lh = mulx(lh, xf_rot_z(0.5f));
            gfx_box1(&lh, v3(-0.022f, -0.03f, -0.02f), v3(0.018f, 0.025f, 0.02f), GLOVE);
            gfx_box1(&lh, v3(-0.008f, 0.02f, -0.012f), v3(0.008f, 0.07f, 0.01f), RGB(30, 30, 32)); /* cargador */
            lw = xf_apply(&lh, v3(-0.01f, -0.025f, -0.015f));
        }
        forearm(team, v3(-0.09f, -0.37f, 0.2f), lw, 0);
    }
    gfx_set_viewmodel(0);
}

/* ------------------------------------------------------------------ */
/* Tercera persona                                                     */
/* ------------------------------------------------------------------ */
static void head(const xform *b, int team)
{
    if (team == TEAM_GIGN) {
        gfx_box1(b, v3(-0.12f, 1.52f, -0.12f), v3(0.12f, 1.8f, 0.12f), RGB(30, 36, 62));      /* pasamontanas */
        gfx_box1(b, v3(-0.105f, 1.53f, 0.1f), v3(0.105f, 1.735f, 0.16f), RGB(42, 44, 46));    /* mascara */
        gfx_box1(b, v3(-0.088f, 1.65f, 0.155f), v3(-0.022f, 1.71f, 0.17f), RGB(230, 128, 44)); /* lentes */
        gfx_box1(b, v3(0.022f, 1.65f, 0.155f), v3(0.088f, 1.71f, 0.17f), RGB(230, 128, 44));
        gfx_box1(b, v3(-0.03f, 1.55f, 0.155f), v3(0.03f, 1.61f, 0.19f), RGB(34, 34, 36));      /* respirador */
        gfx_box1(b, v3(0.06f, 1.55f, 0.13f), v3(0.135f, 1.63f, 0.2f), RGB(52, 54, 56));        /* filtro */
    } else {
        gfx_box1(b, v3(-0.12f, 1.52f, -0.12f), v3(0.12f, 1.8f, 0.12f), RGB(28, 28, 30));
        gfx_box1(b, v3(-0.095f, 1.645f, 0.118f), v3(0.095f, 1.71f, 0.125f), SKIN);
        gfx_box1(b, v3(-0.07f, 1.665f, 0.124f), v3(-0.03f, 1.69f, 0.128f), RGB(40, 30, 24));
        gfx_box1(b, v3(0.03f, 1.665f, 0.124f), v3(0.07f, 1.69f, 0.128f), RGB(40, 30, 24));
    }
}

void model_soldier(int team, vec3 pos, float yaw, float pitch, float walk, float death,
                   int weapon, float flash, int lod)
{
    xform b, leg, t;
    int sub = lod ? 1 : 2;
    const uint32_t *top = team == TEAM_GIGN ? GIGN_CAMO : G_JACKET;
    const uint32_t *bot = team == TEAM_GIGN ? GIGN_CAMO : G_PANTS;
    int ntop = team == TEAM_GIGN ? 4 : 4, nbot = team == TEAM_GIGN ? 4 : 3;
    uint32_t boot = team == TEAM_GIGN ? RGB(26, 26, 26) : RGB(74, 58, 42);
    float sw = sinf(walk) * 0.5f;
    float aimy = 1.32f + pitch * 0.25f;
    vec3 rh, lh;
    int i;

    b = xf_translate(pos.x, pos.y, pos.z);
    b = mulx(b, xf_rot_y(yaw));
    if (death > 0.0f) {
        float a = (death > 1.0f ? 1.0f : death) * 1.5708f;
        b = mulx(b, xf_translate(0, sinf(a) * 0.14f, 0));
        b = mulx(b, xf_rot_x(-a));
        sw = 0;
    }

    /* piernas */
    for (i = 0; i < 2; i++) {
        float x0 = i ? 0.035f : -0.215f, x1 = i ? 0.215f : -0.035f;
        float a = i ? sw : -sw;
        leg = mulx(b, xf_translate(0, 0.88f, 0));
        leg = mulx(leg, xf_rot_x(a));
        leg = mulx(leg, xf_translate(0, -0.88f, 0));
        gfx_box(&leg, v3(x0, 0.12f, -0.09f), v3(x1, 0.88f, 0.09f), bot, nbot, sub, 3 + i);
        gfx_box1(&leg, v3(x0 - 0.01f, 0.0f, -0.1f), v3(x1 + 0.01f, 0.14f, 0.17f), boot);
        if (team == TEAM_GIGN) gfx_box1(&leg, v3(x0 + 0.02f, 0.44f, 0.08f), v3(x1 - 0.02f, 0.58f, 0.115f), RGB(40, 42, 44));
        else gfx_box1(&leg, v3(i ? x1 : x0 - 0.02f, 0.45f, -0.06f), v3(i ? x1 + 0.02f : x0, 0.62f, 0.06f), G_PANTS[2]);
    }
    /* torso */
    gfx_box(&b, v3(-0.25f, 0.86f, -0.13f), v3(0.25f, 1.5f, 0.13f), top, ntop, sub, 9);
    if (team == TEAM_GIGN) {
        gfx_box(&b, v3(-0.235f, 0.92f, 0.12f), v3(0.235f, 1.46f, 0.175f), GIGN_VEST, 3, sub, 31);
        gfx_box(&b, v3(-0.235f, 0.92f, -0.175f), v3(0.235f, 1.46f, -0.12f), GIGN_VEST, 3, 1, 32);
        for (i = 0; i < 3; i++) /* bolsillos del chaleco */
            gfx_box1(&b, v3(-0.2f + i * 0.14f, 0.96f, 0.17f), v3(-0.09f + i * 0.14f, 1.1f, 0.2f), RGB(46, 60, 36));
        gfx_box1(&b, v3(-0.04f, 1.32f, 0.174f), v3(0.13f, 1.37f, 0.178f), RGB(200, 200, 200)); /* "GIGN" */
        gfx_box1(&b, v3(-0.26f, 0.84f, -0.14f), v3(0.26f, 0.9f, 0.14f), RGB(36, 36, 34));      /* cinturon */
    } else {
        gfx_box1(&b, v3(-0.2f, 1.04f, 0.125f), v3(-0.04f, 1.16f, 0.16f), G_JACKET[1]);
        gfx_box1(&b, v3(0.04f, 1.04f, 0.125f), v3(0.2f, 1.16f, 0.16f), G_JACKET[1]);
        gfx_box1(&b, v3(0.06f, 1.26f, 0.13f), v3(0.15f, 1.36f, 0.14f), RGB(60, 44, 28)); /* parche del puno */
        gfx_box1(&b, v3(-0.006f, 0.9f, 0.13f), v3(0.006f, 1.48f, 0.136f), RGB(90, 74, 50)); /* cierre */
    }
    head(&b, team);

    /* brazos apuntando */
    if (weapon == W_KNIFE) {
        rh = v3(0.22f, 1.05f, 0.3f);
        lh = v3(-0.3f, 0.95f, 0.06f);
    } else {
        rh = v3(0.04f, aimy, 0.46f);
        lh = v3(-0.03f, aimy - 0.02f, 0.44f);
    }
    limb(&b, v3(0.3f, 1.44f, 0.0f), v3(0.27f, 1.18f, 0.16f), 0.12f, 0.12f, top, ntop, 1, 41);
    limb(&b, v3(0.27f, 1.18f, 0.16f), rh, 0.11f, 0.11f, top, ntop, 1, 42);
    limb(&b, v3(-0.3f, 1.44f, 0.0f), v3(-0.25f, 1.18f, 0.16f), 0.12f, 0.12f, top, ntop, 1, 43);
    limb(&b, v3(-0.25f, 1.18f, 0.16f), lh, 0.11f, 0.11f, top, ntop, 1, 44);
    if (team == TEAM_GIGN)
        gfx_box1(&b, v3(0.3f, 1.3f, -0.04f), v3(0.315f, 1.38f, 0.04f), RGB(40, 80, 170)); /* parche hombro */
    gfx_box1(&b, vsub(rh, v3(0.045f, 0.045f, 0.045f)), vadd(rh, v3(0.045f, 0.045f, 0.045f)), GLOVE);
    gfx_box1(&b, vsub(lh, v3(0.045f, 0.045f, 0.045f)), vadd(lh, v3(0.045f, 0.045f, 0.045f)), GLOVE);

    /* arma (a escala de mundo, x2 respecto al modelo de primera persona) */
    t = mulx(b, xf_translate(rh.x, rh.y + 0.06f, rh.z));
    t = mulx(t, xf_rot_x(-pitch * 0.6f));
    t = mulx(t, xf_scale(1.3f));
    if (lod) {
        if (weapon == W_KNIFE) gfx_box1(&t, v3(-0.01f, -0.02f, 0.0f), v3(0.01f, 0.02f, 0.2f), RGB(160, 160, 164));
        else gfx_box1(&t, v3(-0.02f, -0.02f, -0.06f), v3(0.02f, 0.04f, 0.14f), RGB(40, 40, 42));
    } else {
        model_weapon(&t, weapon, 0, flash);
    }
}
