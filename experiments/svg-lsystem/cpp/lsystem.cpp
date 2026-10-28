// The C++ twin of ../goose/lsystem.goose: same presets, same turtle, same
// SVG bytes. Idiomatic C++20 with the standard containers, kept structurally
// parallel so the three programs can be read side by side.
//
// Build (MSVC):  cl /O2 /EHsc /std:c++20 lsystem.cpp
// Run:           lsystem plant 6 ../out/plant.cpp.svg

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

struct Rule { char from; std::string_view to; };

struct Preset {
    std::string_view name;
    std::string_view axiom;
    std::vector<Rule> rules;
    double angle;
    double step;
};

struct Turtle { double x, y, heading; };
struct Segment { double x1, y1, x2, y2; };

static const std::vector<Preset> presets = {
    { "plant", "X", { { 'X', "F+[[X]-X]-F[-FX]+X" }, { 'F', "FF" } }, 25.0, 4.0 },
    { "koch", "F", { { 'F', "F+F-F-F+F" } }, 90.0, 3.0 },
    { "dragon", "F", { { 'F', "F+G" }, { 'G', "F-G" } }, 90.0, 4.0 },
    { "sierpinski", "F-G-G", { { 'F', "F-G+F+G-F" }, { 'G', "GG" } }, 120.0, 6.0 },
};

static void rewrite(const std::string& src, const std::vector<Rule>& rules, std::string& out) {
    for (char c : src) {
        bool replaced = false;
        for (const Rule& r : rules) {
            if (r.from == c) { out.append(r.to); replaced = true; break; }
        }
        if (!replaced) out.push_back(c);
    }
}

static void interpret(const std::string& program, const Preset& p, std::vector<Segment>& segs) {
    Turtle t{ 0.0, 0.0, -90.0 };
    std::vector<Turtle> stack;
    const double deg = 3.141592653589793 / 180.0;
    for (char c : program) {
        switch (c) {
        case 'F': case 'G': {
            double rad = t.heading * deg;
            double nx = t.x + std::cos(rad) * p.step;
            double ny = t.y + std::sin(rad) * p.step;
            segs.push_back({ t.x, t.y, nx, ny });
            t.x = nx; t.y = ny;
            break;
        }
        case '+': t.heading += p.angle; break;
        case '-': t.heading -= p.angle; break;
        case '[': stack.push_back(t); break;
        case ']': if (!stack.empty()) { t = stack.back(); stack.pop_back(); } break;
        default: break;
        }
    }
}

static void bounds(const std::vector<Segment>& segs, double& minx, double& miny, double& maxx, double& maxy) {
    minx = 1.0e300; miny = 1.0e300; maxx = -1.0e300; maxy = -1.0e300;
    for (const Segment& s : segs) {
        minx = std::fmin(minx, std::fmin(s.x1, s.x2)); maxx = std::fmax(maxx, std::fmax(s.x1, s.x2));
        miny = std::fmin(miny, std::fmin(s.y1, s.y2)); maxy = std::fmax(maxy, std::fmax(s.y1, s.y2));
    }
}

// Fixed two decimals, rounded half up, written digit by digit. This is what
// Goose's format_flt does; the snprintf("%.2f") it replaces was the slowest
// part of the whole program on MSVC. Build with -DUSE_SNPRINTF to compare.
static void append_flt(std::string& out, double v) {
#ifdef USE_SNPRINTF
    char buf[64];
    std::snprintf(buf, sizeof buf, "%.2f", v);
    out.append(buf);
#else
    if (v < 0) { out.push_back('-'); v = -v; }
    unsigned long long scaled = (unsigned long long)(v * 100.0 + 0.5);
    unsigned long long ip = scaled / 100, fp = scaled % 100;
    char buf[24];
    int n = 0;
    do { buf[n++] = (char)('0' + ip % 10); ip /= 10; } while (ip);
    while (n) out.push_back(buf[--n]);
    out.push_back('.');
    out.push_back((char)('0' + fp / 10));
    out.push_back((char)('0' + fp % 10));
#endif
}

static void write_svg(const std::vector<Segment>& segs, std::string& out) {
    double minx, miny, maxx, maxy;
    bounds(segs, minx, miny, maxx, maxy);
    const double margin = 10.0;
    out.append("<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"");
    append_flt(out, minx - margin); out.push_back(' ');
    append_flt(out, miny - margin); out.push_back(' ');
    append_flt(out, maxx - minx + margin * 2.0); out.push_back(' ');
    append_flt(out, maxy - miny + margin * 2.0);
    out.append("\">\n<path fill=\"none\" stroke=\"#2a7\" stroke-width=\"1\" d=\"");
    for (size_t i = 0; i < segs.size(); i++) {
        const Segment& s = segs[i];
        if (i > 0) out.push_back('\n');
        out.append("M"); append_flt(out, s.x1); out.push_back(' '); append_flt(out, s.y1);
        out.append("L"); append_flt(out, s.x2); out.push_back(' '); append_flt(out, s.y2);
    }
    out.append("\"/>\n</svg>\n");
}

int main(int argc, char** argv) {
    if (argc != 4) {
        std::fprintf(stderr, "usage: lsystem <plant|koch|dragon|sierpinski> <depth> <out.svg>\n");
        return 2;
    }
    char* end = nullptr;
    long depth = std::strtol(argv[2], &end, 10);
    if (*argv[2] == '\0' || *end != '\0' || depth < 0 || depth > 12) {
        std::fprintf(stderr, "depth must be 0..12\n");
        return 2;
    }
    const Preset* chosen = nullptr;
    for (const Preset& p : presets) if (p.name == argv[1]) chosen = &p;
    if (!chosen) {
        std::fprintf(stderr, "unknown preset: %s\n", argv[1]);
        return 2;
    }

    auto t0 = std::chrono::steady_clock::now();
    std::string cur(chosen->axiom);
    for (long i = 0; i < depth; i++) {
        std::string next;
        rewrite(cur, chosen->rules, next);
        cur.swap(next);
    }

    std::vector<Segment> segs;
    interpret(cur, *chosen, segs);

    std::string svg;
    write_svg(segs, svg);
    double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();

    std::ofstream f(argv[3], std::ios::binary);
    if (!f || !f.write(svg.data(), (std::streamsize)svg.size())) {
        std::fprintf(stderr, "cannot write %s\n", argv[3]);
        return 1;
    }
    std::printf("%.*s depth %ld: %zu symbols, %zu segments, %zu bytes of svg, %.1f ms\n",
                (int)chosen->name.size(), chosen->name.data(), depth, cur.size(), segs.size(), svg.size(), ms);
    return 0;
}
