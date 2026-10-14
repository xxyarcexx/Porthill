// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Lector nativo de .PTS (grafo de puntos de la pista). Mismo algoritmo que tools/pts_path.py.
// Registro: 8 B de cabecera + N * 32 B = f32 x,y,z; i16 a0..a7; 4 B de relleno. a6 (índice 9) = registro SIGUIENTE.
// El .PTS viene del disco del usuario: cada lectura comprueba límites. Doc: docs/en/formats.md, docs/native-loader-plan.md.
#pragma once
#include <cstdint>
#include <cstring>
#include <vector>

namespace pts {
inline constexpr uint32_t kMagic = 0x09212001u;   // cabecera de .PTS (u32 LE)
inline constexpr size_t kHeaderSize = 8;
inline constexpr size_t kRecordSize = 32;
inline constexpr size_t kNextIndex = 6;           // a6 dentro del bloque i16 a0..a7 (equivale a R[i][9] en pts_path.py)

struct Point { float x = 0, y = 0, z = 0; };      // espacio NGP (Z arriba)
struct Record { float x = 0, y = 0, z = 0; int16_t a[8] = {}; };

class File {
public:
    // Acepta solo tamaños de la forma 8 + N*32 (N>=0) y, salvo checkMagic=false, la magia esperada.
    bool load(const std::vector<uint8_t>& d, bool checkMagic = true) {
        valid_ = false;
        if (d.size() < kHeaderSize) return false;
        if ((d.size() - kHeaderSize) % kRecordSize != 0) return false;
        if (checkMagic) { uint32_t m = 0; std::memcpy(&m, d.data(), 4); if (m != kMagic) return false; }
        data_ = d; valid_ = true; return true;
    }
    bool loaded() const { return valid_; }
    size_t count() const { return valid_ ? (data_.size() - kHeaderSize) / kRecordSize : 0; }

    Record at(size_t i) const {                       // fuera de rango: registro cero
        Record r;
        if (i >= count()) return r;
        const uint8_t* p = data_.data() + kHeaderSize + i * kRecordSize;
        std::memcpy(&r.x, p, 4); std::memcpy(&r.y, p + 4, 4); std::memcpy(&r.z, p + 8, 4);
        std::memcpy(r.a, p + 12, 16);
        return r;
    }

    // Sigue los enlaces a6 desde `start` hasta repetir un índice o salir de rango (réplica de pts_path.py --chain N).
    std::vector<size_t> chain(size_t start) const {
        const size_t n = count();
        std::vector<size_t> seen; seen.reserve(n);
        std::vector<char> visited(n, 0);
        size_t i = start;
        while (i < n && !visited[i]) {
            visited[i] = 1; seen.push_back(i);
            const int16_t nx = at(i).a[kNextIndex];
            i = nx < 0 ? n : static_cast<size_t>(nx);   // negativo => fuera de rango => fin
        }
        return seen;
    }
    std::vector<Point> points(const std::vector<size_t>& idx) const {
        std::vector<Point> out; out.reserve(idx.size());
        for (size_t i : idx) { const Record r = at(i); out.push_back({r.x, r.y, r.z}); }
        return out;
    }
    std::vector<Point> chainPoints(size_t start = 0) const { return points(chain(start)); }
    std::vector<Point> allPoints() const {               // como pts_path.py sin --chain
        std::vector<size_t> idx(count()); for (size_t i = 0; i < idx.size(); i++) idx[i] = i;
        return points(idx);
    }

private:
    std::vector<uint8_t> data_; bool valid_ = false;
};
}  // namespace pts
