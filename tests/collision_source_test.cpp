// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Test del adaptador de colisión NGP->Ground (src/collision_source.hpp).
//   Sin argumentos: casos sintéticos (transformación, entrada válida, vacía e inválida) -> lo ejecuta ctest.
//   Con datos reales: collision_source_test nivel.NGP nivel.col  -> compara fromNgp() con Ground::load(.col)
//     (MISMAS convenciones de coordenadas, MISMO orden y precisión: memcmp de los 9 floats + surface de cada triángulo).
#include "../src/collision_source.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#define CHECK(c) do { if (!(c)) { std::printf("FALLO %s:%d %s\n", __FILE__, __LINE__, #c); std::exit(1); } } while (0)

static bool rd(const char* p, std::vector<uint8_t>& r) {
    FILE* f = std::fopen(p, "rb"); if (!f) return false;
    std::fseek(f, 0, SEEK_END); long n = std::ftell(f); std::rewind(f);
    r.resize(n < 0 ? 0 : (size_t)n);
    bool ok = std::fread(r.data(), 1, r.size(), f) == r.size(); std::fclose(f); return ok;
}

// NGP sintético mínimo: un nodo 0x2A con 1 parte, 1 triángulo y 3 vértices (espacio NGP, Z arriba).
static std::vector<uint8_t> syntheticNgp() {
    const uint32_t B = ngp::kBase;
    const size_t NODE = 0x00, TABLE = 0x40, PART = 0x60, VERT = 0x90;
    std::vector<uint8_t> d(0x100, 0);
    auto u32 = [&](size_t o, uint32_t v) { std::memcpy(&d[o], &v, 4); };
    auto u16 = [&](size_t o, uint16_t v) { std::memcpy(&d[o], &v, 2); };
    auto f32 = [&](size_t o, float v) { std::memcpy(&d[o], &v, 4); };
    u32(NODE, 42); u16(NODE + 4, 1); u16(NODE + 6, 0); u32(NODE + 8, (uint32_t)(B + TABLE)); u32(NODE + 0xc, (uint32_t)(B + PART));
    u16(TABLE, 7);                                                                     // tabla de superficies: id 7
    u32(PART, (uint32_t)(B + VERT)); u16(PART + 4, 1); u16(PART + 6, 0);                // parte: ptr vértices, 1 tri, 0 nodos BVH
    f32(PART + 8, 0); f32(PART + 0xc, 0); f32(PART + 0x10, 0); f32(PART + 0x14, 1.f);   // origen (0,0,0), escala 1
    d[PART + 0x18] = 0; d[PART + 0x19] = 1; d[PART + 0x1a] = 2; d[PART + 0x1b] = 0;     // triángulo (v0 v1 v2 mat)
    f32(VERT + 0, 0); f32(VERT + 4, 0); f32(VERT + 8, 0);                               // v0 (NGP: x,y,z)
    f32(VERT + 12, 0); f32(VERT + 16, 0); f32(VERT + 20, 10);                           // v1
    f32(VERT + 24, 10); f32(VERT + 28, 0); f32(VERT + 32, 0);                           // v2
    return d;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        // 1) transformación pura de una ColTri conocida: (x,y,z) -> (x, z, -y), orden y superficie preservados
        std::vector<ngp::ColTri> in(1); float v[9] = {1, 2, 3, 4, 5, 6, 7, 8, 9}; std::memcpy(in[0].v, v, 36); in[0].surface = 5;
        std::vector<Ground::Tri> out; collision_source::ngpToGround(in, out);
        CHECK(out.size() == 1 && out[0].surface == 5);
        CHECK(out[0].v[0] == 1 && out[0].v[1] == 3 && out[0].v[2] == -2);
        CHECK(out[0].v[3] == 4 && out[0].v[4] == 6 && out[0].v[5] == -5);
        CHECK(out[0].v[6] == 7 && out[0].v[7] == 9 && out[0].v[8] == -8);

        // 2) entrada válida: NGP sintético (sin instancias, evita la regla de huérfanos)
        auto ngp = syntheticNgp(); std::vector<Ground::Tri> tris;
        CHECK(collision_source::fromNgp(ngp, tris, false));
        CHECK(tris.size() == 1 && tris[0].surface == 7);
        CHECK(tris[0].v[0] == 0 && tris[0].v[1] == 0 && tris[0].v[2] == 0);
        CHECK(tris[0].v[3] == 0 && tris[0].v[4] == 10 && tris[0].v[5] == 0);
        CHECK(tris[0].v[6] == 10 && tris[0].v[7] == 0 && tris[0].v[8] == 0);

        // 3) entradas inválidas -> false y `out` vacío
        std::vector<Ground::Tri> e;
        CHECK(!collision_source::fromNgp({}, e) && e.empty());
        CHECK(!collision_source::fromNgp(std::vector<uint8_t>(8, 0), e) && e.empty());
        CHECK(!collision_source::fromNgp(std::vector<uint8_t>(0x100, 0), e) && e.empty());
        std::printf("collision_source_test: sintético OK\n");
        return 0;
    }

    // Datos reales: fromNgp(<NGP descomprimido>) debe dar los MISMOS triángulos (Y arriba) que Ground::load(.col)
    std::vector<uint8_t> ngpRaw, colRaw; std::vector<Ground::Tri> fromNgp;
    if (!rd(argv[1], ngpRaw) || !rd(argv[2], colRaw)) { std::fprintf(stderr, "no se pudieron leer los archivos\n"); return 2; }
    Ground g; if (!g.load(colRaw)) { std::fprintf(stderr, ".col inválido: %s\n", argv[2]); return 2; }
    if (!collision_source::fromNgp(ngpRaw, fromNgp)) { std::fprintf(stderr, "fromNgp falló (¿NGP descomprimido?): %s\n", argv[1]); return 2; }
    const std::vector<Ground::Tri>& a = g.tris();
    if (a.size() != fromNgp.size()) { std::printf("DISTINTO: %zu (col) vs %zu (ngp)\n", a.size(), fromNgp.size()); return 1; }
    size_t bad = 0;
    for (size_t i = 0; i < a.size(); i++) {
        if (a[i].surface != fromNgp[i].surface) { bad++; continue; }
        for (int k = 0; k < 9; k++) if (std::memcmp(&a[i].v[k], &fromNgp[i].v[k], 4) != 0) { bad++; break; }
    }
    std::printf("collision_source_test: %zu triángulos; %zu distintos del .col\n", a.size(), bad);
    return bad != 0;
}
