#include "gfx/portrait.h"

#include <math.h>
#include <string.h>

#include <SDL3/SDL.h>

#define NW_PORTRAIT_WELD 1e-4f
#define NW_PORTRAIT_CUTOUT 0.3f

typedef struct NwPortraitView {
    float eye[3], r[3], u[3], f[3];
    float scale, mx, my, ox, oy;
} NwPortraitView;

static void nw_portrait_sub(float o[3], const float a[3], const float b[3])
{
    o[0] = a[0] - b[0];
    o[1] = a[1] - b[1];
    o[2] = a[2] - b[2];
}

static void nw_portrait_cross(float o[3], const float a[3], const float b[3])
{
    o[0] = a[1] * b[2] - a[2] * b[1];
    o[1] = a[2] * b[0] - a[0] * b[2];
    o[2] = a[0] * b[1] - a[1] * b[0];
}

static float nw_portrait_dot(const float a[3], const float b[3])
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

static void nw_portrait_norm(float v[3])
{
    float l = sqrtf(nw_portrait_dot(v, v));
    if (l > 0.0f) {
        v[0] /= l;
        v[1] /= l;
        v[2] /= l;
    }
}

static int32_t nw_portrait_cell(float x)
{
    return (int32_t)lroundf(x / NW_PORTRAIT_WELD);
}

static uint32_t nw_portrait_hash(const float p[3])
{
    uint32_t h = 2166136261u;
    for (int i = 0; i < 3; i++) {
        h = (h ^ (uint32_t)nw_portrait_cell(p[i])) * 16777619u;
    }
    return h;
}

static bool nw_portrait_same(const float a[3], const float b[3])
{
    return nw_portrait_cell(a[0]) == nw_portrait_cell(b[0]) && nw_portrait_cell(a[1]) == nw_portrait_cell(b[1]) &&
           nw_portrait_cell(a[2]) == nw_portrait_cell(b[2]);
}

/* PS2 strips mix their winding: each face is turned to the eye before the welded normals add up. */
static float (*nw_portrait_normals(const NwPortraitMesh *m, const float eye[3]))[3]
{
    uint32_t cap = 1;
    while (cap < m->nv * 2) {
        cap <<= 1;
    }
    uint32_t *slot = SDL_malloc(cap * sizeof(*slot)), *weld = SDL_malloc(m->nv * sizeof(*weld));
    float (*acc)[3] = SDL_calloc(m->nv, sizeof(*acc)), (*n)[3] = SDL_malloc(m->nv * sizeof(*n));
    if (!slot || !weld || !acc || !n) {
        SDL_free(slot);
        SDL_free(weld);
        SDL_free(acc);
        SDL_free(n);
        return NULL;
    }
    memset(slot, 0xFF, cap * sizeof(*slot));
    for (uint32_t i = 0; i < m->nv; i++) {
        uint32_t h = nw_portrait_hash(m->v[i].p) & (cap - 1);
        while (slot[h] != UINT32_MAX && !nw_portrait_same(m->v[slot[h]].p, m->v[i].p)) {
            h = (h + 1) & (cap - 1);
        }
        if (slot[h] == UINT32_MAX) {
            slot[h] = i;
        }
        weld[i] = slot[h];
    }
    for (uint32_t t = 0; t < m->ntri; t++) {
        const float *a = m->v[m->tri[t][0]].p, *b = m->v[m->tri[t][1]].p, *c = m->v[m->tri[t][2]].p;
        float e1[3], e2[3], fn[3], to_eye[3];
        nw_portrait_sub(e1, b, a);
        nw_portrait_sub(e2, c, a);
        nw_portrait_cross(fn, e1, e2);
        nw_portrait_sub(to_eye, eye, a);
        float s = nw_portrait_dot(fn, to_eye) < 0.0f ? -1.0f : 1.0f;
        for (int k = 0; k < 3; k++) {
            float *d = acc[weld[m->tri[t][k]]];
            d[0] += s * fn[0];
            d[1] += s * fn[1];
            d[2] += s * fn[2];
        }
    }
    for (uint32_t i = 0; i < m->nv; i++) {
        memcpy(n[i], acc[weld[i]], sizeof(n[i]));
        nw_portrait_norm(n[i]);
    }
    SDL_free(slot);
    SDL_free(weld);
    SDL_free(acc);
    return n;
}

static void nw_portrait_sample(const NwPortraitSurface *s, float u, float v, float out[4])
{
    if (!s->rgba) {
        out[0] = out[1] = out[2] = out[3] = 1.0f;
        return;
    }
    float x = u * (float)s->w - 0.5f, y = v * (float)s->h - 0.5f;
    float fx = floorf(x), fy = floorf(y);
    float ax = x - fx, ay = y - fy;
    int x0 = (int)fx, y0 = (int)fy;
    out[0] = out[1] = out[2] = out[3] = 0.0f;
    for (int k = 0; k < 4; k++) {
        int xx = ((x0 + (k & 1)) % s->w + s->w) % s->w, yy = ((y0 + (k >> 1)) % s->h + s->h) % s->h;
        float w = (k & 1 ? ax : 1.0f - ax) * (k >> 1 ? ay : 1.0f - ay);
        const uint8_t *c = s->rgba + ((size_t)yy * (size_t)s->w + (size_t)xx) * 4;
        for (int i = 0; i < 4; i++) {
            out[i] += w * c[i] / 255.0f;
        }
    }
}

static void nw_portrait_view(NwPortraitView *view, const NwPortraitMesh *m, const NwPortraitLook *look, int size)
{
    float lo[3] = { 1e30f, 1e30f, 1e30f }, hi[3] = { -1e30f, -1e30f, -1e30f };
    for (uint32_t t = 0; t < m->ntri; t++) {
        for (int k = 0; k < 3; k++) {
            const float *p = m->v[m->tri[t][k]].p;
            for (int i = 0; i < 3; i++) {
                lo[i] = SDL_min(lo[i], p[i]);
                hi[i] = SDL_max(hi[i], p[i]);
            }
        }
    }
    float mid[3] = { (lo[0] + hi[0]) * 0.5f, (lo[1] + hi[1]) * 0.5f, (lo[2] + hi[2]) * 0.5f }, span[3];
    nw_portrait_sub(span, hi, lo);
    float radius = sqrtf(nw_portrait_dot(span, span)) * 0.5f;
    float yaw = look->yaw * (SDL_PI_F / 180.0f), pitch = look->pitch * (SDL_PI_F / 180.0f);
    /* left-handed */
    float to_eye[3] = { sinf(yaw) * cosf(pitch), sinf(pitch), -cosf(yaw) * cosf(pitch) }, up[3] = { 0.0f, 1.0f, 0.0f };
    for (int i = 0; i < 3; i++) {
        view->eye[i] = mid[i] + to_eye[i] * radius * 6.0f;
        view->f[i] = -to_eye[i];
    }
    nw_portrait_cross(view->r, up, view->f);
    nw_portrait_norm(view->r);
    nw_portrait_cross(view->u, view->f, view->r);
    float x0 = 1e30f, x1 = -1e30f, y0 = 1e30f, y1 = -1e30f;
    for (uint32_t t = 0; t < m->ntri; t++) {
        for (int k = 0; k < 3; k++) {
            float d[3];
            nw_portrait_sub(d, m->v[m->tri[t][k]].p, view->eye);
            float z = nw_portrait_dot(d, view->f);
            float x = nw_portrait_dot(d, view->r) / z, y = nw_portrait_dot(d, view->u) / z;
            x0 = SDL_min(x0, x);
            x1 = SDL_max(x1, x);
            y0 = SDL_min(y0, y);
            y1 = SDL_max(y1, y);
        }
    }
    view->scale = look->fit * (float)size / SDL_max(x1 - x0, y1 - y0);
    view->mx = (x0 + x1) * 0.5f;
    view->my = (y0 + y1) * 0.5f;
    view->ox = view->oy = 0.5f * (float)size;
}

/* camera space: x right, y up, z away */
static void nw_portrait_shade(const NwPortraitLook *look, const float albedo[3], const float nw[3],
                              const NwPortraitView *view, float out[3])
{
    static const float key[3] = { -0.52f, 0.62f, -0.59f }, fill[3] = { 0.78f, 0.05f, -0.62f };
    static const float rim[3] = { 0.42f, 0.5f, 0.76f }, half[3] = { -0.29f, 0.35f, -0.89f };
    float n[3] = { nw_portrait_dot(nw, view->r), nw_portrait_dot(nw, view->u), nw_portrait_dot(nw, view->f) };
    float facing = SDL_max(-n[2], 0.0f);
    float lit = 0.34f + 0.78f * SDL_max(nw_portrait_dot(n, key), 0.0f) +
                0.26f * SDL_max(nw_portrait_dot(n, fill), 0.0f);
    float spec = powf(SDL_max(nw_portrait_dot(n, half), 0.0f), 28.0f) * 0.18f;
    float edge = (1.0f - facing) * (1.0f - facing);
    float back = (0.35f + 0.65f * SDL_max(nw_portrait_dot(n, rim), 0.0f)) * edge * 0.9f;
    for (int i = 0; i < 3; i++) {
        out[i] = SDL_min(albedo[i] * lit + spec + look->rim[i] * back, 1.0f);
    }
}

static bool nw_portrait_raster(const NwPortraitMesh *m, const NwPortraitLook *look, int size, float *rgba)
{
    NwPortraitView view;
    nw_portrait_view(&view, m, look, size);
    float (*nrm)[3] = nw_portrait_normals(m, view.eye);
    float *depth = SDL_malloc((size_t)size * (size_t)size * sizeof(float));
    float (*sp)[3] = SDL_malloc((size_t)m->nv * sizeof(*sp)); /* x, y, 1/z */
    if (!nrm || !depth || !sp) {
        SDL_free(nrm);
        SDL_free(depth);
        SDL_free(sp);
        return false;
    }
    for (int i = 0; i < size * size; i++) {
        depth[i] = 1e30f;
    }
    memset(rgba, 0, (size_t)size * (size_t)size * 4 * sizeof(float));
    for (uint32_t i = 0; i < m->nv; i++) {
        float d[3];
        nw_portrait_sub(d, m->v[i].p, view.eye);
        float z = nw_portrait_dot(d, view.f);
        sp[i][0] = (nw_portrait_dot(d, view.r) / z - view.mx) * view.scale + view.ox;
        sp[i][1] = view.oy - (nw_portrait_dot(d, view.u) / z - view.my) * view.scale;
        sp[i][2] = 1.0f / z;
    }
    for (uint32_t t = 0; t < m->ntri; t++) {
        const uint32_t *x = m->tri[t];
        const float *a = sp[x[0]], *b = sp[x[1]], *c = sp[x[2]];
        float area = (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0]);
        if (fabsf(area) < 1e-9f) {
            continue;
        }
        const NwPortraitSurface *s = &m->surf[x[3]];
        const NwPortraitVert *va = &m->v[x[0]], *vb = &m->v[x[1]], *vc = &m->v[x[2]];
        const float *na = nrm[x[0]], *nb = nrm[x[1]], *nc = nrm[x[2]];
        int bx0 = SDL_max((int)floorf(SDL_min(a[0], SDL_min(b[0], c[0]))), 0);
        int bx1 = SDL_min((int)ceilf(SDL_max(a[0], SDL_max(b[0], c[0]))), size - 1);
        int by0 = SDL_max((int)floorf(SDL_min(a[1], SDL_min(b[1], c[1]))), 0);
        int by1 = SDL_min((int)ceilf(SDL_max(a[1], SDL_max(b[1], c[1]))), size - 1);
        for (int py = by0; py <= by1; py++) {
            for (int px = bx0; px <= bx1; px++) {
                float qx = (float)px + 0.5f, qy = (float)py + 0.5f;
                float w0 = ((b[0] - qx) * (c[1] - qy) - (b[1] - qy) * (c[0] - qx)) / area;
                float w1 = ((c[0] - qx) * (a[1] - qy) - (c[1] - qy) * (a[0] - qx)) / area;
                float w2 = 1.0f - w0 - w1;
                if (w0 < 0.0f || w1 < 0.0f || w2 < 0.0f) {
                    continue;
                }
                float z = 1.0f / (w0 * a[2] + w1 * b[2] + w2 * c[2]);
                float *zb = &depth[py * size + px];
                if (z >= *zb) {
                    continue;
                }
                float p0 = w0 * a[2] * z, p1 = w1 * b[2] * z, p2 = w2 * c[2] * z;
                float tex[4], n[3], albedo[3];
                nw_portrait_sample(s, p0 * va->uv[0] + p1 * vb->uv[0] + p2 * vc->uv[0],
                                   p0 * va->uv[1] + p1 * vb->uv[1] + p2 * vc->uv[1], tex);
                if (s->cutout && tex[3] < NW_PORTRAIT_CUTOUT) {
                    continue;
                }
                for (int i = 0; i < 3; i++) {
                    n[i] = p0 * na[i] + p1 * nb[i] + p2 * nc[i];
                    albedo[i] = tex[i] * s->tint[i];
                }
                nw_portrait_norm(n);
                *zb = z;
                float *o = &rgba[(py * size + px) * 4];
                nw_portrait_shade(look, albedo, n, &view, o);
                o[3] = 1.0f;
            }
        }
    }
    SDL_free(nrm);
    SDL_free(depth);
    SDL_free(sp);
    return true;
}

static uint8_t *nw_portrait_compose(const float *big, const NwPortraitLook *look, int n, int ss)
{
    int r = look->edge_px, wide = n * ss;
    float *model = SDL_malloc((size_t)n * (size_t)n * 4 * sizeof(float));
    float *edge = SDL_malloc((size_t)n * (size_t)n * sizeof(float));
    uint8_t *out = SDL_malloc((size_t)n * (size_t)n * 4);
    if (!model || !edge || !out) {
        SDL_free(model);
        SDL_free(edge);
        SDL_free(out);
        return NULL;
    }
    for (int y = 0; y < n; y++) {
        for (int x = 0; x < n; x++) {
            float *h = &model[(y * n + x) * 4];
            h[0] = h[1] = h[2] = h[3] = 0.0f;
            for (int sy = 0; sy < ss; sy++) {
                for (int sx = 0; sx < ss; sx++) {
                    const float *b = &big[((y * ss + sy) * wide + x * ss + sx) * 4];
                    for (int i = 0; i < 4; i++) {
                        h[i] += b[i] / (float)(ss * ss);
                    }
                }
            }
        }
    }
    for (int y = 0; y < n; y++) {
        for (int x = 0; x < n; x++) {
            float e = 0.0f;
            for (int dy = -r - 1; dy <= r + 1; dy++) {
                for (int dx = -r - 1; dx <= r + 1; dx++) {
                    int xx = x + dx, yy = y + dy;
                    float d = sqrtf((float)(dx * dx + dy * dy));
                    if (xx >= 0 && yy >= 0 && xx < n && yy < n && d <= (float)r + 1.0f) {
                        e = SDL_max(e, model[(yy * n + xx) * 4 + 3] * SDL_min((float)r + 1.0f - d, 1.0f));
                    }
                }
            }
            edge[y * n + x] = e;
        }
    }
    for (int k = 0; k < n * n; k++) {
        const float *h = &model[k * 4];
        float a = h[3] + edge[k] * (1.0f - h[3]);
        uint8_t *o = &out[k * 4];
        o[0] = (uint8_t)lroundf(a * 255.0f);
        for (int i = 0; i < 3; i++) {
            float e = look->edge[i] / 255.0f;
            float c = a > 0.0f ? (h[i] + e * edge[k] * (1.0f - h[3])) / a : e;
            o[3 - i] = (uint8_t)lroundf(SDL_clamp(c, 0.0f, 1.0f) * 255.0f);
        }
    }
    SDL_free(model);
    SDL_free(edge);
    return out;
}

uint8_t *nw_portrait_render(const NwPortraitMesh *m, const NwPortraitLook *look, int size, int supersample)
{
    if (!m->ntri || size <= 0 || supersample <= 0) {
        return NULL;
    }
    int wide = size * supersample;
    float *big = SDL_malloc((size_t)wide * (size_t)wide * 4 * sizeof(float));
    uint8_t *out = big && nw_portrait_raster(m, look, wide, big) ? nw_portrait_compose(big, look, size, supersample)
                                                                   : NULL;
    SDL_free(big);
    return out;
}
