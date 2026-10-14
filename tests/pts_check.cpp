// SPDX-FileCopyrightText: 2026 Pedro Soto
// SPDX-License-Identifier: GPL-3.0-or-later
// Test del lector nativo .PTS (src/pts.hpp).
//   Sin argumentos: casos sintéticos (válido, vacío, truncado, malformado); lo ejecuta ctest.
//   Con datos reales: pts_check NIVEL.PTS oracle.pts [--chain N]
//     compara BYTE A BYTE la salida nativa con la de tools/pts_path.py (mismo patrón que tests/ngp_col_check.cpp).
//     Sin --chain exporta TODOS los puntos (igual que pts_path.py sin --chain); con --chain N sigue el carril N.
#include "../src/pts.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#define CHECK(c) do { if (!(c)) { std::printf("FALLO %s:%d %s\n", __FILE__, __LINE__, #c); std::exit(1); } } while (0)

static void put32(std::vector<uint8_t>& v, uint32_t x) { for (int i = 0; i < 4; i++) v.push_back((uint8_t)(x >> (8 * i))); }
static void putf(std::vector<uint8_t>& v, float x) { uint32_t u = 0; std::memcpy(&u, &x, 4); put32(v, u); }
static void puti16(std::vector<uint8_t>& v, int16_t x) { v.push_back((uint8_t)(x & 0xff)); v.push_back((uint8_t)((x >> 8) & 0xff)); }
static void rec(std::vector<uint8_t>& v, float x, float y, float z,
                int16_t a0, int16_t a1, int16_t a2, int16_t a3, int16_t a4, int16_t a5, int16_t a6, int16_t a7) {
    putf(v, x); putf(v, y); putf(v, z);
    puti16(v, a0); puti16(v, a1); puti16(v, a2); puti16(v, a3); puti16(v, a4); puti16(v, a5); puti16(v, a6); puti16(v, a7);
    put32(v, 0);   // 4 B de relleno
}

static void synthetic() {
    // válido: 4 registros; cadena 0 -> 0,2,3 (el 1 nunca se visita); 3 -> 0 cierra el ciclo
    std::vector<uint8_t> b; put32(b, pts::kMagic); put32(b, 0);   // cabecera de 8 B (magia + relleno)
    rec(b, 10, 0, 0, 0, 0, 0, 0, 0, 0,  2, 0);   // 0 -> 2
    rec(b, 11, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0);   // 1 -> -1
    rec(b, 12, 0, 0, 0, 0, 0, 0, 0, 0,  3, 0);   // 2 -> 3
    rec(b, 13, 0, 0, 0, 0, 0, 0, 0, 0,  0, 0);   // 3 -> 0
    pts::File f; CHECK(f.load(b)); CHECK(f.loaded()); CHECK(f.count() == 4);
    CHECK((f.chain(0) == std::vector<size_t>{0, 2, 3}));
    CHECK((f.chain(1) == std::vector<size_t>{1}));      // a6 negativo: termina tras visitar el 1
    CHECK((f.chain(2) == std::vector<size_t>{2, 3, 0})); // ciclo detectado
    CHECK(f.chain(4).empty());                          // inicio fuera de rango
    auto p = f.chainPoints(0); CHECK(p.size() == 3 && p[0].x == 10.f && p[1].x == 12.f && p[2].x == 13.f);
    CHECK(f.allPoints().size() == 4);
    CHECK(f.at(1).a[6] == -1);

    // vacío: solo la cabecera (N = 0)
    std::vector<uint8_t> e; put32(e, pts::kMagic); put32(e, 0); pts::File fe;
    CHECK(fe.load(e)); CHECK(fe.count() == 0); CHECK(fe.chain(0).empty()); CHECK(fe.allPoints().empty());

    // truncado: un byte de menos no es múltiplo de 32
    std::vector<uint8_t> t = b; t.resize(t.size() - 1); pts::File ft; CHECK(!ft.load(t));

    // demasiado corto (< cabecera)
    std::vector<uint8_t> s = {1, 2, 3}; pts::File fs; CHECK(!fs.load(s));

    // malformado: magia incorrecta (rechazo por defecto; aceptado si checkMagic = false)
    std::vector<uint8_t> m = b; m[0] = 0; pts::File fm;
    CHECK(!fm.load(m)); CHECK(fm.load(m, false) && fm.count() == 4);

    std::printf("pts_check: sintético OK\n");
}

static bool readAll(const char* p, std::vector<uint8_t>& out) {
    FILE* f = std::fopen(p, "rb"); if (!f) return false;
    std::fseek(f, 0, SEEK_END); long n = std::ftell(f); std::rewind(f);
    out.resize(n < 0 ? 0 : (size_t)n);
    bool ok = std::fread(out.data(), 1, out.size(), f) == out.size(); std::fclose(f); return ok;
}

int main(int argc, char** argv) {
    if (argc < 3) { synthetic(); return 0; }

    std::vector<uint8_t> raw; pts::File f;
    if (!readAll(argv[1], raw) || !f.load(raw)) { std::fprintf(stderr, "no se pudo leer .PTS: %s\n", argv[1]); return 2; }

    bool hasChain = false; size_t chain = 0;
    for (int i = 3; i < argc; i++) {
        if (!std::strcmp(argv[i], "--chain") && i + 1 < argc) { chain = (size_t)std::strtoul(argv[++i], nullptr, 0); hasChain = true; }
    }
    const std::vector<pts::Point> P = hasChain ? f.chainPoints(chain) : f.allPoints();

    std::vector<uint8_t> out; out.reserve(P.size() * 12);
    for (const pts::Point& q : P) { putf(out, q.x); putf(out, q.y); putf(out, q.z); }

    std::vector<uint8_t> ref; if (!readAll(argv[2], ref)) { std::fprintf(stderr, "no se pudo leer oráculo: %s\n", argv[2]); return 2; }
    if (out.size() != ref.size()) {
        std::printf("DISTINTO: %zu bytes nativos vs %zu del oráculo (%zu vs %zu puntos)\n", out.size(), ref.size(), out.size() / 12, ref.size() / 12);
        return 1;
    }
    size_t bad = 0; for (size_t i = 0; i < out.size(); i++) if (out[i] != ref[i]) bad++;
    std::printf("pts_check: %zu puntos; %zu bytes distintos del oráculo Python\n", P.size(), bad);
    return bad != 0;
}
