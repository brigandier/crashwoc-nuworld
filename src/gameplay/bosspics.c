#include "gameplay/bosspics.h"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <SDL3/SDL.h>

#include "gameplay/gameplay.h"
#include "gfx/packet.h"
#include "gfx/portrait.h"
#include "gfx/rndr.h"
#include "gfx/tex.h"
#include "nu3d/nuanim.h"
#include "nu3d/nugobj.h"
#include "nu3d/nugscn.h"
#include "nu3d/nuhgobj.h"
#include "nu3d/numtl.h"
#include "nu3d/nutex.h"
#include "nu3d/nutexanm.h"
#include "nu_port.h"
#include "nucore/nucoretypes.h"
#include "numath/numtx.h"
#include "platform/log.h"
#include "port/heap.h"

/* Declared by no header. */
extern s32 Level;
extern struct nugscn_s *world_scene[32];
void *NuDatOpen(char *name, char **mem, s32 *used);
void NuDatSet(void *dat);
void NuDatClose(void *dat);
void NuHGobjEval(struct NUHGOBJ_s *hobj, int njanims, struct NUJOINTANIM_s *janim, struct numtx_s *mtx_array);
void NuMtxMulVU0(struct numtx_s *dst, struct numtx_s *a, struct numtx_s *b);

#define NW_BOSS_PICS_HUB 0x25
#define NW_BOSS_PICS_ROOMS 5
#define NW_BOSS_PICS_SIZE 256
#define NW_BOSS_PICS_SS 3
#define NW_BOSS_PICS_BYTES 0x140000 /* weather.ghg, the largest, is 0x114416 bytes */
#define NW_BOSS_PICS_GRID 128
#define NW_BOSS_PICS_CUT 0.5f
#define NW_BOSS_PICS_FLAT 0.002f
#define NW_BOSS_PICS_GROW 1.25 /* of retail's picture */

typedef struct NwBossPic {
    const char *model;
    bool water; /* each texture animation's last frame, as in water_b */
    float yaw, pitch, fit;
    float rim[3];
} NwBossPic;

static const NwBossPic nw_boss_pics[NW_BOSS_PICS_ROOMS] = {
    { "chars\\d\\crunch\\crunch.ghg", false, 30.0f, 5.0f, 0.96f, { 1.0f, 0.75f, 0.45f } },
    { "chars\\d\\crunch\\crunch.ghg", true, 30.0f, 5.0f, 0.96f, { 0.45f, 0.9f, 1.0f } },
    { "chars\\d\\fire\\fire.ghg", false, 30.0f, 5.0f, 0.96f, { 1.0f, 0.5f, 0.15f } },
    { "chars\\d\\weather\\weather.ghg", false, 30.0f, 5.0f, 0.96f, { 0.8f, 0.55f, 1.0f } },
    { "chars\\b\\cortex\\cortex.ghg", false, 25.0f, -14.0f, 0.96f, { 0.6f, 0.75f, 1.0f } },
};

static const char *const nw_boss_pics_pads[NW_BOSS_PICS_ROOMS] = {
    "pcylinder53", "pcylinder54", "pcylinder55", "pcylinder56", "pcylinder57",
};

typedef struct NwPicMesh {
    NwPortraitMesh m;
    const struct numtl_s **mtl;
    uint32_t vcap, tcap, scap, mcap;
    bool failed;
} NwPicMesh;

static struct {
    uint8_t *pixels[NW_BOSS_PICS_ROOMS];
    bool tried;
    char *buf;
    struct numtl_s *mtl[NW_BOSS_PICS_ROOMS], *retail_mtl[NW_BOSS_PICS_ROOMS];
    struct nugeom_s *geom[NW_BOSS_PICS_ROOMS][2]; /* face, block */
    void *retail[NW_BOSS_PICS_ROOMS][2];
    NwGpuMesh *mesh[NW_BOSS_PICS_ROOMS][2];
} nw_bp;

int nw_boss_pics_room(float x, float z)
{
    if (x == 0.0f && z == 0.0f) {
        return -1;
    }
    float deg = atan2f(z, x) * (180.0f / SDL_PI_F);
    int k = (int)lroundf((30.0f - deg) / 60.0f);
    k = ((k % 6) + 6) % 6;
    return k < NW_BOSS_PICS_ROOMS ? k : -1;
}

static bool nw_pic_grow(void **p, uint32_t *cap, uint32_t need, size_t each)
{
    if (need <= *cap) {
        return true;
    }
    uint32_t n = *cap ? *cap : 256;
    while (n < need) {
        n *= 2;
    }
    void *grown = SDL_realloc(*p, (size_t)n * each);
    if (!grown) {
        return false;
    }
    *p = grown;
    *cap = n;
    return true;
}

static void nw_pic_free(NwPicMesh *m)
{
    for (uint32_t i = 0; i < m->m.nsurf; i++) {
        SDL_free(m->m.surf[i].rgba);
    }
    SDL_free(m->m.v);
    SDL_free(m->m.tri);
    SDL_free(m->m.surf);
    SDL_free(m->mtl);
    memset(m, 0, sizeof(*m));
}

static int nw_pic_tid(const struct NUHGOBJ_s *o, const struct numtl_s *mtl, bool water)
{
    for (int i = 0; water && i < o->numtexanims; i++) {
        const struct nutexanim_s *a = &o->texanims[i];
        if (a->mtl == mtl && a->numtids > 0) {
            return a->tids[a->numtids - 1];
        }
    }
    return mtl->tid;
}

static int nw_pic_surface(NwPicMesh *m, const struct NUHGOBJ_s *o, const struct numtl_s *mtl, bool water)
{
    if (mtl->attrib.alpha >= 2) {
        return -1;
    }
    for (uint32_t i = 0; i < m->m.nsurf; i++) {
        if (m->mtl[i] == mtl) {
            return (int)i;
        }
    }
    if (!nw_pic_grow((void **)&m->m.surf, &m->scap, m->m.nsurf + 1, sizeof(*m->m.surf)) ||
        !nw_pic_grow((void **)&m->mtl, &m->mcap, m->m.nsurf + 1, sizeof(*m->mtl))) {
        m->failed = true;
        return -1;
    }
    m->mtl[m->m.nsurf] = mtl;
    NwPortraitSurface *s = &m->m.surf[m->m.nsurf];
    memset(s, 0, sizeof(*s));
    const NwGpuTex *g = mtl->tid ? nw_rndr_texture(nw_pic_tid(o, mtl, water)) : NULL;
    /* 0x80 = 1.0 on a texel */
    float scale = g ? 2.0f : 1.0f;
    s->tint[0] = SDL_min(mtl->diffuse.r * scale, 1.0f);
    s->tint[1] = SDL_min(mtl->diffuse.g * scale, 1.0f);
    s->tint[2] = SDL_min(mtl->diffuse.b * scale, 1.0f);
    s->cutout = mtl->attrib.alpha != 0;
    if (g && g->tex.nlevels) {
        s->w = (int)g->tex.levels[0].width;
        s->h = (int)g->tex.levels[0].height;
        s->rgba = SDL_malloc((size_t)s->w * (size_t)s->h * 4);
        if (!s->rgba) {
            m->failed = true;
            return -1;
        }
        for (int y = 0; y < s->h; y++) {
            nw_tex_rgba(&g->tex, (uint32_t)y, s->rgba + (size_t)y * (size_t)s->w * 4);
        }
    }
    return (int)m->m.nsurf++;
}

static void nw_pic_xform(float out[3], const float v[3], const struct numtx_s *t)
{
    out[0] = v[0] * t->_00 + v[1] * t->_10 + v[2] * t->_20 + t->_30;
    out[1] = v[0] * t->_01 + v[1] * t->_11 + v[2] * t->_21 + t->_31;
    out[2] = v[0] * t->_02 + v[1] * t->_12 + v[2] * t->_22 + t->_32;
}

static bool nw_pic_under(const struct NUHGOBJ_s *o, int j, int head)
{
    for (int n = 0; j >= 0 && n <= o->num_joints; j = o->joints[j].parent_ix, n++) {
        if (j == head) {
            return true;
        }
    }
    return false;
}

static int nw_pic_head(const struct NUHGOBJ_s *o)
{
    for (int j = 0; j < o->num_joints; j++) {
        const char *n = o->joints[j].name;
        const char *bar = n ? SDL_strrchr(n, '|') : NULL;
        if (n && SDL_strcasecmp(bar ? bar + 1 : n, "head") == 0) {
            return j;
        }
    }
    return -1;
}

static bool nw_pic_tri(NwPicMesh *m, uint32_t a, uint32_t b, uint32_t c, int surf)
{
    if (!nw_pic_grow((void **)&m->m.tri, &m->tcap, m->m.ntri + 1, sizeof(*m->m.tri))) {
        return false;
    }
    m->m.tri[m->m.ntri][0] = a;
    m->m.tri[m->m.ntri][1] = b;
    m->m.tri[m->m.ntri][2] = c;
    m->m.tri[m->m.ntri][3] = (uint32_t)surf;
    m->m.ntri++;
    return true;
}

static void nw_pic_rigid(NwPicMesh *m, const struct NUHGOBJ_s *o, struct nugobj_s *gobj, const struct numtx_s *t,
                         bool water)
{
    for (; gobj; gobj = gobj->next_gobj) {
        for (struct nugeom_s *geom = gobj->geom; geom; geom = geom->next) {
            const NwGpuMesh *mesh = geom->dmastream;
            int surf = mesh ? nw_pic_surface(m, o, geom->mtl, water) : -1;
            if (surf < 0 ||
                !nw_pic_grow((void **)&m->m.v, &m->vcap, m->m.nv + mesh->mesh.nverts, sizeof(*m->m.v))) {
                continue;
            }
            uint32_t base = m->m.nv;
            for (uint32_t i = 0; i < mesh->mesh.nverts; i++) {
                const NwVifVertex *s = &mesh->mesh.verts[i];
                NwPortraitVert *v = &m->m.v[m->m.nv++];
                nw_pic_xform(v->p, s->pos, t);
                v->uv[0] = s->tex[0] / 4096.0f;
                v->uv[1] = s->tex[1] / 4096.0f;
            }
            for (uint32_t k = 0; k < mesh->mesh.ntris; k++) {
                const uint32_t *x = &mesh->mesh.tris[k * 3];
                if (!nw_pic_tri(m, base + x[0], base + x[1], base + x[2], surf)) {
                    m->failed = true;
                    return;
                }
            }
        }
    }
}

static int nw_pic_heaviest(const NwVifSkinVertex *v)
{
    int best = 0;
    for (int k = 1; k < 8; k++) {
        if (v->weight[k] > v->weight[best]) {
            best = k;
        }
    }
    return v->joint[best];
}

static void nw_pic_skin(NwPicMesh *m, const struct NUHGOBJ_s *o, struct nugobj_s *gobj, const struct numtx_s *pal,
                        int head, bool water)
{
    for (; gobj; gobj = gobj->next_gobj) {
        for (struct nugeom_s *geom = gobj->geom; geom; geom = geom->next) {
            const NwGpuSkin *skin = (const NwGpuSkin *)geom->skindma;
            int surf = skin ? nw_pic_surface(m, o, geom->mtl, water) : -1;
            if (surf < 0 ||
                !nw_pic_grow((void **)&m->m.v, &m->vcap, m->m.nv + skin->skin.nverts, sizeof(*m->m.v))) {
                continue;
            }
            const NwVifSkin *s = &skin->skin;
            uint32_t base = m->m.nv;
            for (uint32_t i = 0; i < s->nverts; i++) {
                const NwVifSkinVertex *sv = &s->verts[i];
                NwPortraitVert *v = &m->m.v[m->m.nv++];
                memset(v, 0, sizeof(*v));
                for (int k = 0; k < 8; k++) {
                    float w = sv->weight[k] / 32768.0f, q[3];
                    if (w != 0.0f) {
                        nw_pic_xform(q, sv->pos, &pal[sv->joint[k]]);
                        v->p[0] += w * q[0];
                        v->p[1] += w * q[1];
                        v->p[2] += w * q[2];
                    }
                }
                v->uv[0] = sv->uv[0];
                v->uv[1] = sv->uv[1];
            }
            for (uint32_t k = 0; k < s->ntris; k++) {
                const uint32_t *x = &s->tris[k * 3];
                bool in = true;
                for (int c = 0; c < 3; c++) {
                    in = in && nw_pic_under(o, nw_pic_heaviest(&s->verts[x[c]]), head);
                }
                if (in && !nw_pic_tri(m, base + x[0], base + x[1], base + x[2], surf)) {
                    m->failed = true;
                    return;
                }
            }
        }
    }
}

static void nw_pic_collect(NwPicMesh *m, struct NUHGOBJ_s *o, bool water)
{
    static struct numtx_s tmtx[256], pal[256];
    int head = nw_pic_head(o);
    if (head < 0 || o->num_layers < 1) {
        m->failed = true;
        return;
    }
    NuHGobjEval(o, 0, NULL, tmtx);
    for (int j = 0; j < o->num_joints; j++) {
        NuMtxMulVU0(&pal[j], &o->INV_WT[j], &tmtx[j]);
    }
    struct NULAYERDATA_s *l = &o->layers[0];
    for (int set = 0; set < 2; set++) {
        struct nugobj_s **gobjs = set ? l->blend_gobjs : l->gobjs;
        for (int j = 0; gobjs && j < o->num_joints; j++) {
            if (gobjs[j] && nw_pic_under(o, j, head)) {
                nw_pic_rigid(m, o, gobjs[j], &tmtx[j], water);
            }
        }
    }
    if (l->skin_gobj) {
        nw_pic_skin(m, o, l->skin_gobj, pal, head, water);
    }
    if (l->blend_skin_gobj) {
        nw_pic_skin(m, o, l->blend_skin_gobj, pal, head, water);
    }
}

static uint8_t *nw_pic_render(struct NUHGOBJ_s *o, int room)
{
    const NwBossPic *pic = &nw_boss_pics[room];
    NwPortraitLook look = { pic->yaw, pic->pitch, pic->fit, { pic->rim[0], pic->rim[1], pic->rim[2] },
                            { 118, 118, 126 }, 3 };
    NwPicMesh m;
    memset(&m, 0, sizeof(m));
    nw_pic_collect(&m, o, pic->water);
    uint8_t *out = m.failed ? NULL : nw_portrait_render(&m.m, &look, NW_BOSS_PICS_SIZE, NW_BOSS_PICS_SS);
    nw_pic_free(&m);
    return out;
}

static void nw_boss_pics_make(void)
{
    nw_bp.tried = true;
    if (!nw_bp.buf) {
        if (!(nw_bp.buf = SDL_malloc(NW_BOSS_PICS_BYTES))) {
            return;
        }
        nw_load_buffer(nw_bp.buf, NW_BOSS_PICS_BYTES);
    }
    void *dat = NuDatOpen("chars.dat", NULL, NULL);
    NuDatSet(dat);
    for (int i = 0; i < NW_BOSS_PICS_ROOMS; i++) {
        if (nw_bp.pixels[i]) {
            continue;
        }
        union variptr_u buf = { .charptr = nw_bp.buf };
        struct NUHGOBJ_s *o = NuGHGRead(&buf, (char *)nw_boss_pics[i].model);
        if (!o) {
            nw_log_warn(NW_LOG_GAME, "boss pictures: %s did not load", nw_boss_pics[i].model);
            continue;
        }
        for (int k = i; k < NW_BOSS_PICS_ROOMS; k++) {
            if (!nw_bp.pixels[k] && SDL_strcmp(nw_boss_pics[k].model, nw_boss_pics[i].model) == 0) {
                nw_bp.pixels[k] = nw_pic_render(o, k);
            }
        }
        NuHGobjDestroy(o);
    }
    NuDatSet(NULL);
    if (dat) {
        NuDatClose(dat);
    }
}

static struct nugobj_s *nw_boss_pics_pad(struct nugscn_s *scene, const char *name, int *room)
{
    for (int i = 0; i < scene->numspecial; i++) {
        struct nuspecial_s *s = &scene->specials[i];
        if (s->name && SDL_strcasecmp(s->name, name) == 0 && s->instance && s->instance->objid >= 0 &&
            s->instance->objid < scene->numgobj) {
            *room = nw_boss_pics_room(s->mtx._30, s->mtx._32);
            return scene->gobjs[s->instance->objid];
        }
    }
    return NULL;
}

static bool nw_boss_pics_uses(struct nugobj_s *gobj, const struct numtl_s *mtl)
{
    for (; gobj; gobj = gobj->next_gobj) {
        for (struct nugeom_s *g = gobj->geom; g; g = g->next) {
            if (g->mtl == mtl) {
                return true;
            }
        }
    }
    return false;
}

static struct numtl_s *nw_boss_pics_shared(struct nugobj_s *const pads[NW_BOSS_PICS_ROOMS])
{
    for (struct nugobj_s *gobj = pads[0]; gobj; gobj = gobj->next_gobj) {
        for (struct nugeom_s *g = gobj->geom; g; g = g->next) {
            bool all = g->mtl && g->mtl->tid;
            for (int i = 1; all && i < NW_BOSS_PICS_ROOMS; i++) {
                all = nw_boss_pics_uses(pads[i], g->mtl);
            }
            if (all) {
                return g->mtl;
            }
        }
    }
    return NULL;
}

static struct numtl_s *nw_boss_pics_material(const struct numtl_s *from, const uint8_t *pixels)
{
    struct nutex_s tex = { 3, NW_BOSS_PICS_SIZE, NW_BOSS_PICS_SIZE, 1, (void *)pixels, NULL };
    struct numtl_s *mtl = NuMtlCreate(1);
    mtl->attrib = from->attrib;
    mtl->attrib.alpha = 1;
    mtl->attrib.atst = 6;
    mtl->attrib.aref = 0x3F;
    mtl->attrib.afail = 0;
    mtl->ambient = from->ambient;
    mtl->diffuse = from->diffuse;
    mtl->power = from->power;
    mtl->alpha = from->alpha;
    mtl->sort_pri = from->sort_pri;
    mtl->K = from->K;
    mtl->L = from->L;
    mtl->context = from->context;
    mtl->fxid = from->fxid;
    mtl->tid = NuTexCreate(&tex);
    NuMtlUpdate(mtl);
    return mtl;
}

static double nw_boss_pics_det(double m[3][3])
{
    return m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]) - m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0]) +
           m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
}

static bool nw_boss_pics_solve(double a[3][3], const double b[3], double x[3])
{
    double det = nw_boss_pics_det(a);
    if (fabs(det) < 1e-12) {
        return false;
    }
    for (int c = 0; c < 3; c++) {
        double m[3][3];
        memcpy(m, a, sizeof(m));
        for (int r = 0; r < 3; r++) {
            m[r][c] = b[r];
        }
        x[c] = nw_boss_pics_det(m) / det;
    }
    return true;
}

/* Retail's plaque is cut to Crunch: a block under a face whose texcoords are a top-down projection. */
typedef struct NwPadFrame {
    double cx[3], cz[3];
    float top, foot;
} NwPadFrame;

static bool nw_boss_pics_frame(const NwVifMesh *face, const NwVifMesh *block, NwPadFrame *f)
{
    if (face->nverts < 3 || face->uvrow || !block->nverts) {
        return false;
    }
    f->top = -1e30f;
    for (uint32_t i = 0; i < face->nverts; i++) {
        f->top = SDL_max(f->top, face->verts[i].pos[1]);
    }
    /* the face's rim is off the projection */
    double ata[3][3] = { { 0 } }, ax[3] = { 0 }, az[3] = { 0 };
    for (uint32_t t = 0; t < face->ntris; t++) {
        const uint32_t *x = &face->tris[t * 3];
        bool on_top = true;
        for (int k = 0; k < 3; k++) {
            on_top = on_top && face->verts[x[k]].pos[1] > f->top - NW_BOSS_PICS_FLAT;
        }
        for (int k = 0; on_top && k < 3; k++) {
            const NwVifVertex *v = &face->verts[x[k]];
            double r[3] = { 1.0, v->tex[0] / 4096.0, v->tex[1] / 4096.0 };
            for (int a = 0; a < 3; a++) {
                for (int b = 0; b < 3; b++) {
                    ata[a][b] += r[a] * r[b];
                }
                ax[a] += r[a] * v->pos[0];
                az[a] += r[a] * v->pos[2];
            }
        }
    }
    f->foot = 1e30f;
    for (uint32_t i = 0; i < block->nverts; i++) {
        f->foot = SDL_min(f->foot, block->verts[i].pos[1]);
    }
    return nw_boss_pics_solve(ata, ax, f->cx) && nw_boss_pics_solve(ata, az, f->cz);
}

static NwVifVertex *nw_boss_pics_vert(NwVifVertex *o, const uint8_t col[4], const NwPadFrame *f, double u, double v,
                                      float y, struct nugobj_s *gobj)
{
    memset(o, 0, sizeof(*o));
    u = 0.5 + (u - 0.5) * NW_BOSS_PICS_GROW;
    v = 0.5 + (v - 0.5) * NW_BOSS_PICS_GROW;
    o->pos[0] = (float)(f->cx[0] + f->cx[1] * u + f->cx[2] * v);
    o->pos[1] = y;
    o->pos[2] = (float)(f->cz[0] + f->cz[1] * u + f->cz[2] * v);
    o->tex[2] = 4096;
    memcpy(o->col, col, 4);
    gobj->min.x = SDL_min(gobj->min.x, o->pos[0]);
    gobj->max.x = SDL_max(gobj->max.x, o->pos[0]);
    gobj->min.z = SDL_min(gobj->min.z, o->pos[2]);
    gobj->max.z = SDL_max(gobj->max.z, o->pos[2]);
    return o;
}

static NwGpuMesh *nw_boss_pics_mesh(uint32_t nverts, uint32_t ntris)
{
    NwGpuMesh *mesh = SDL_calloc(1, sizeof(*mesh));
    if (!mesh) {
        return NULL;
    }
    mesh->mesh.verts = SDL_malloc(nverts * sizeof(NwVifVertex));
    mesh->mesh.tris = SDL_malloc(ntris * 3 * sizeof(uint32_t));
    if (!mesh->mesh.verts || !mesh->mesh.tris) {
        SDL_free(mesh->mesh.verts);
        SDL_free(mesh->mesh.tris);
        SDL_free(mesh);
        return NULL;
    }
    mesh->mesh.vcap = nverts;
    mesh->mesh.tcap = ntris * 3;
    mesh->mesh.nbatches = 1;
    return mesh;
}

static NwGpuMesh *nw_boss_pics_face(const NwVifMesh *face, const NwPadFrame *f, struct nugobj_s *gobj)
{
    NwGpuMesh *mesh = nw_boss_pics_mesh(4, 2);
    if (!mesh) {
        return NULL;
    }
    const NwVifVertex *like = &face->verts[0];
    for (uint32_t i = 1; i < face->nverts; i++) {
        if (face->verts[i].pos[1] > like->pos[1]) {
            like = &face->verts[i];
        }
    }
    for (int k = 0; k < 4; k++) {
        NwVifVertex *o = nw_boss_pics_vert(&mesh->mesh.verts[k], like->col, f, k & 1, k >> 1, f->top, gobj);
        o->tex[0] = (int16_t)((k & 1) * 4096);
        o->tex[1] = (int16_t)((k >> 1) * 4096);
    }
    static const uint32_t tris[6] = { 0, 1, 2, 1, 3, 2 };
    memcpy(mesh->mesh.tris, tris, sizeof(tris));
    mesh->mesh.nverts = 4;
    mesh->mesh.ntris = 2;
    return mesh;
}

static float nw_boss_pics_alpha(const uint8_t *pixels, int i, int j)
{
    if (i <= 0 || j <= 0 || i >= NW_BOSS_PICS_GRID || j >= NW_BOSS_PICS_GRID) {
        return 0.0f;
    }
    int x = i * NW_BOSS_PICS_SIZE / NW_BOSS_PICS_GRID, y = j * NW_BOSS_PICS_SIZE / NW_BOSS_PICS_GRID;
    return pixels[(y * NW_BOSS_PICS_SIZE + x) * 4] / 255.0f;
}

/* millimetres: the game's float goes through eefpu */
typedef struct NwBlockPt {
    int32_t x, z;
    bool upper;
    const uint8_t *col;
} NwBlockPt;

/* The block is unlit: a wall takes the colour of its nearest vertex. */
static const uint8_t *nw_boss_pics_baked(const NwBlockPt *pts, uint32_t n, int32_t x, int32_t z, bool upper)
{
    int64_t best = INT64_MAX;
    const uint8_t *col = pts[0].col;
    for (uint32_t i = 0; i < n; i++) {
        int64_t dx = pts[i].x - x, dz = pts[i].z - z, d = dx * dx + dz * dz;
        if (pts[i].upper == upper && d < best) {
            best = d;
            col = pts[i].col;
        }
    }
    return col;
}

static NwGpuMesh *nw_boss_pics_walls(const NwVifMesh *block, const NwPadFrame *f, const uint8_t *pixels,
                                     struct nugobj_s *gobj)
{
    enum { G = NW_BOSS_PICS_GRID };
    /* marching squares: corners 1 top left, 2 top right, 4 bottom right, 8 bottom left */
    static const int8_t cases[16][4] = {
        { -1, -1, -1, -1 }, { 3, 0, -1, -1 }, { 0, 1, -1, -1 }, { 3, 1, -1, -1 }, { 1, 2, -1, -1 }, { 3, 0, 1, 2 },
        { 0, 2, -1, -1 },   { 3, 2, -1, -1 }, { 2, 3, -1, -1 }, { 0, 2, -1, -1 }, { 0, 1, 2, 3 },   { 1, 2, -1, -1 },
        { 1, 3, -1, -1 },   { 0, 1, -1, -1 }, { 3, 0, -1, -1 }, { -1, -1, -1, -1 },
    };
    uint32_t nseg = 0;
    for (int j = 0; j < G; j++) {
        for (int i = 0; i < G; i++) {
            int c = (nw_boss_pics_alpha(pixels, i, j) >= NW_BOSS_PICS_CUT) |
                    (nw_boss_pics_alpha(pixels, i + 1, j) >= NW_BOSS_PICS_CUT) << 1 |
                    (nw_boss_pics_alpha(pixels, i + 1, j + 1) >= NW_BOSS_PICS_CUT) << 2 |
                    (nw_boss_pics_alpha(pixels, i, j + 1) >= NW_BOSS_PICS_CUT) << 3;
            nseg += (cases[c][0] >= 0) + (cases[c][2] >= 0);
        }
    }
    NwGpuMesh *mesh = nseg ? nw_boss_pics_mesh(nseg * 4, nseg * 2) : NULL;
    if (!mesh) {
        return NULL;
    }
    NwBlockPt *pts = SDL_malloc(block->nverts * sizeof(*pts));
    if (!pts) {
        nw_vif_free(&mesh->mesh);
        SDL_free(mesh);
        return NULL;
    }
    float mid = (f->top + f->foot) * 0.5f;
    for (uint32_t i = 0; i < block->nverts; i++) {
        const NwVifVertex *b = &block->verts[i];
        pts[i] = (NwBlockPt){ (int32_t)(b->pos[0] * 1000.0f), (int32_t)(b->pos[2] * 1000.0f), b->pos[1] > mid,
                              b->col };
    }
    NwVifVertex *v = mesh->mesh.verts;
    uint32_t *t = mesh->mesh.tris, n = 0;
    for (int j = 0; j < G; j++) {
        for (int i = 0; i < G; i++) {
            float a[4] = { nw_boss_pics_alpha(pixels, i, j), nw_boss_pics_alpha(pixels, i + 1, j),
                           nw_boss_pics_alpha(pixels, i + 1, j + 1), nw_boss_pics_alpha(pixels, i, j + 1) };
            int c = 0;
            for (int k = 0; k < 4; k++) {
                c |= (a[k] >= NW_BOSS_PICS_CUT) << k;
            }
            static const int at[4][2] = { { 0, 0 }, { 1, 0 }, { 1, 1 }, { 0, 1 } };
            for (int s = 0; s < 4 && cases[c][s] >= 0; s += 2) {
                double uv[2][2];
                for (int e = 0; e < 2; e++) {
                    int edge = cases[c][s + e], p = edge, q = (edge + 1) & 3;
                    double w = (NW_BOSS_PICS_CUT - a[p]) / (a[q] - a[p]);
                    uv[e][0] = (i + at[p][0] + (at[q][0] - at[p][0]) * w) / G;
                    uv[e][1] = (j + at[p][1] + (at[q][1] - at[p][1]) * w) / G;
                }
                for (int k = 0; k < 4; k++) {
                    nw_boss_pics_vert(&v[n + k], block->verts[0].col, f, uv[k & 1][0], uv[k & 1][1],
                                      k >> 1 ? f->top : f->foot, gobj);
                }
                int32_t mx = (int32_t)(v[n].pos[0] * 1000.0f) / 2 + (int32_t)(v[n + 1].pos[0] * 1000.0f) / 2;
                int32_t mz = (int32_t)(v[n].pos[2] * 1000.0f) / 2 + (int32_t)(v[n + 1].pos[2] * 1000.0f) / 2;
                for (int k = 0; k < 4; k++) {
                    memcpy(v[n + k].col, nw_boss_pics_baked(pts, block->nverts, mx, mz, k >> 1), 4);
                }
                uint32_t q[6] = { n, n + 1, n + 2, n + 1, n + 3, n + 2 };
                memcpy(t, q, sizeof(q));
                t += 6;
                n += 4;
            }
        }
    }
    SDL_free(pts);
    mesh->mesh.nverts = n;
    mesh->mesh.ntris = n / 2;
    return mesh;
}

static struct nugeom_s *nw_boss_pics_block(struct nugobj_s *gobj, const struct nugeom_s *face)
{
    const NwVifMesh *fm = &((const NwGpuMesh *)face->dmastream)->mesh;
    float foot = 1e30f;
    for (uint32_t i = 0; i < fm->nverts; i++) {
        foot = SDL_min(foot, fm->verts[i].pos[1]);
    }
    for (struct nugeom_s *g = gobj->geom; g; g = g->next) {
        const NwGpuMesh *m = g->dmastream;
        if (g == face || !m || g->tristream || !m->mesh.nverts) {
            continue;
        }
        float top = -1e30f;
        for (uint32_t i = 0; i < m->mesh.nverts; i++) {
            top = SDL_max(top, m->mesh.verts[i].pos[1]);
        }
        if (fabsf(top - foot) < 0.01f) {
            return g;
        }
    }
    return NULL;
}

static void nw_boss_pics_dress(struct nugobj_s *gobj, const struct numtl_s *shared, int room)
{
    struct nugeom_s *face = NULL;
    for (struct nugeom_s *g = gobj->geom; g && !face; g = g->next) {
        face = g->mtl == shared && g->dmastream && !g->tristream ? g : NULL;
    }
    struct nugeom_s *block = face ? nw_boss_pics_block(gobj, face) : NULL;
    NwPadFrame f;
    if (!block || !nw_boss_pics_frame(&((NwGpuMesh *)face->dmastream)->mesh, &((NwGpuMesh *)block->dmastream)->mesh,
                                      &f)) {
        return;
    }
    NwGpuMesh *top = nw_boss_pics_face(&((NwGpuMesh *)face->dmastream)->mesh, &f, gobj);
    NwGpuMesh *walls = nw_boss_pics_walls(&((NwGpuMesh *)block->dmastream)->mesh, &f, nw_bp.pixels[room], gobj);
    if (!top || !walls) {
        for (int k = 0; k < 2; k++) {
            NwGpuMesh *m = k ? walls : top;
            if (m) {
                nw_vif_free(&m->mesh);
                SDL_free(m);
            }
        }
        return;
    }
    if (!nw_bp.mtl[room]) {
        nw_bp.mtl[room] = nw_boss_pics_material(shared, nw_bp.pixels[room]);
    }
    nw_gpu_queue(NW_GPU_UPLOAD_MESH, top);
    nw_gpu_queue(NW_GPU_UPLOAD_MESH, walls);
    nw_bp.geom[room][0] = face;
    nw_bp.geom[room][1] = block;
    nw_bp.retail[room][0] = face->dmastream;
    nw_bp.retail[room][1] = block->dmastream;
    nw_bp.retail_mtl[room] = face->mtl;
    nw_bp.mesh[room][0] = top;
    nw_bp.mesh[room][1] = walls;
}

static void nw_boss_pics_show(bool on)
{
    for (int i = 0; i < NW_BOSS_PICS_ROOMS; i++) {
        if (nw_bp.mesh[i][0]) {
            nw_bp.geom[i][0]->dmastream = on ? nw_bp.mesh[i][0] : nw_bp.retail[i][0];
            nw_bp.geom[i][1]->dmastream = on ? nw_bp.mesh[i][1] : nw_bp.retail[i][1];
            nw_bp.geom[i][0]->mtl = on ? nw_bp.mtl[i] : nw_bp.retail_mtl[i];
        }
    }
}

void nw_boss_pics_level(void)
{
    struct nugscn_s *scene = world_scene[0];
    if (!nw_profile.boss_pics || Level != NW_BOSS_PICS_HUB || !scene) {
        return;
    }
    struct nugobj_s *pads[NW_BOSS_PICS_ROOMS];
    int rooms[NW_BOSS_PICS_ROOMS];
    for (int i = 0; i < NW_BOSS_PICS_ROOMS; i++) {
        if (!(pads[i] = nw_boss_pics_pad(scene, nw_boss_pics_pads[i], &rooms[i])) || rooms[i] < 0) {
            return;
        }
    }
    struct numtl_s *shared = nw_boss_pics_shared(pads);
    if (!shared) {
        return;
    }
    NW_DEVIATION(boss_pics);
    if (!nw_bp.tried) {
        nw_boss_pics_make();
    }
    for (int i = 0; i < NW_BOSS_PICS_ROOMS; i++) {
        if (nw_bp.pixels[rooms[i]] && !nw_bp.mesh[rooms[i]][0]) {
            nw_boss_pics_dress(pads[i], shared, rooms[i]);
        }
    }
    nw_boss_pics_show(true);
}

void nw_boss_pics_apply(void)
{
    if (Level != NW_BOSS_PICS_HUB) {
        return;
    }
    if (nw_profile.boss_pics) {
        nw_boss_pics_level();
    } else {
        nw_boss_pics_show(false);
    }
}

void nw_boss_pics_release(void)
{
    for (int i = 0; i < NW_BOSS_PICS_ROOMS; i++) {
        for (int k = 0; k < 2; k++) {
            if (nw_bp.mesh[i][k]) {
                nw_gpu_queue(NW_GPU_FREE_MESH, nw_bp.mesh[i][k]);
            }
        }
    }
    memset(nw_bp.mtl, 0, sizeof(nw_bp.mtl));
    memset(nw_bp.mesh, 0, sizeof(nw_bp.mesh));
    memset(nw_bp.geom, 0, sizeof(nw_bp.geom));
}
