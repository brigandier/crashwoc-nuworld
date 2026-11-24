#ifndef NUWORLD_GFX_PORTRAIT_H
#define NUWORLD_GFX_PORTRAIT_H

#include <stdbool.h>
#include <stdint.h>

typedef struct NwPortraitSurface {
    float tint[3];
    uint8_t *rgba; /* NULL: untextured */
    int w, h;
    bool cutout;
} NwPortraitSurface;

typedef struct NwPortraitVert {
    float p[3], uv[2];
} NwPortraitVert;

typedef struct NwPortraitMesh {
    NwPortraitVert *v;
    uint32_t (*tri)[4]; /* three vertices, the surface */
    NwPortraitSurface *surf;
    uint32_t nv, ntri, nsurf;
} NwPortraitMesh;

typedef struct NwPortraitLook {
    float yaw, pitch; /* degrees, 0 0 faces a model looking down -z */
    float fit;
    float rim[3];
    uint8_t edge[3];
    int edge_px;
} NwPortraitLook;

/* Type 3 texels (a, b, g, r). */
uint8_t *nw_portrait_render(const NwPortraitMesh *m, const NwPortraitLook *look, int size, int supersample);

#endif
