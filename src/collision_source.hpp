// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Adaptador de geometría de colisión: convierte los triángulos del lector nativo del .NGP (src/ngp.hpp) al
// formato de Ground (src/ground.hpp). NO reinterpreta .PTS. La conversión de coordenadas es EXACTAMENTE la
// misma que Ground::load(raw, zup=true): (x,y,z) -> (x, z, -y). Doc: docs/formats/collision.md, docs/native-loader-plan.md.
#pragma once
#include <cstdint>
#include <vector>
#include "ground.hpp"
#include "ngp.hpp"

namespace collision_source {

// Convierte triángulos en espacio NGP (Z arriba, ngp::ColTri) a espacio Ground (Y arriba, Ground::Tri).
// Preserva el orden de vértices y el id de superficie. Misma permutación que Ground::load.
inline void ngpToGround(const std::vector<ngp::ColTri>& in, std::vector<Ground::Tri>& out) {
    out.clear(); out.reserve(in.size());
    for (const ngp::ColTri& t : in) {
        Ground::Tri g; g.surface = t.surface;
        for (int k = 0; k < 3; k++) {
            const float x = t.v[3 * k], y = t.v[3 * k + 1], z = t.v[3 * k + 2];
            g.v[3 * k] = x; g.v[3 * k + 1] = z; g.v[3 * k + 2] = -y;
        }
        out.push_back(g);
    }
}

// Construye triángulos Ground (Y arriba) a partir de una imagen .NGP YA DESCOMPRIMIDA (NUNCA el contenedor IE).
// Devuelve false si la entrada es inválida/demasiado corta o no produce triángulos; en ese caso `out` queda vacío.
// `instanced=true` usa la colocación por instancias (equivale a `collision.py --instances`); false, el export antiguo.
inline bool fromNgp(const std::vector<uint8_t>& ngp, std::vector<Ground::Tri>& out, bool instanced = true) {
    out.clear();
    if (ngp.size() < 0x20) return false;                 // cabecera mínima (u32 4 + 4 punteros)
    std::vector<ngp::ColTri> tris;
    const bool ok = instanced ? ngp::collisionTrisInstanced(ngp, tris) : ngp::collisionTris(ngp, tris);
    if (!ok || tris.empty()) { out.clear(); return false; }
    ngpToGround(tris, out);
    return true;
}
}  // namespace collision_source
