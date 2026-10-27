// The C++ twin of ../goose/save.goose, written the way a C++ game codebase
// usually does it: ordinary structs with std::string and std::vector, and a
// hand-written serializer. The point of comparison is not speed but what has
// to exist for the same save/load to work, and what the in-memory data costs.
//
// Build (MSVC):  cl /O2 /EHsc /std:c++20 save.cpp
// Run:           save ../out/save.cpp.bin

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

// --- the data -----------------------------------------------------------------

struct Vec2 { float x, y; };
struct Player { std::string name; int32_t hp, gold; Vec2 pos; };
struct Item { std::string name; int64_t qty, cents; };

// The tree keeps children by index into the pool, because pointers into a
// std::vector are invalidated by growth and cannot be written to a file.
struct Quest { uint32_t id; std::string title; bool done; std::vector<int32_t> children; };

static int32_t add_child(std::vector<Quest>& pool, int32_t parent, uint32_t id, std::string title, bool done) {
    pool.push_back(Quest{ id, std::move(title), done, {} });
    int32_t at = (int32_t)pool.size() - 1;
    pool[parent].children.push_back(at);
    return at;
}

static void walk(const std::vector<Quest>& pool, int32_t at, int depth, std::string& out) {
    const Quest& q = pool[at];
    for (int i = 0; i < depth; i++) out += "  ";
    out += q.done ? "[x] " : "[ ] ";
    out += std::to_string(q.id) + " " + q.title + "\n";
    for (int32_t c : q.children) walk(pool, c, depth + 1, out);
}

static int count_done(const std::vector<Quest>& pool, int32_t at) {
    const Quest& q = pool[at];
    int n = q.done ? 1 : 0;
    for (int32_t c : q.children) n += count_done(pool, c);
    return n;
}

static int64_t inventory_value(const std::vector<Item>& bag) {
    int64_t t = 0;
    for (const Item& it : bag) t += it.qty * it.cents;
    return t;
}

// --- the serializer, which Goose did not need ------------------------------------
// Every field is written by hand in a fixed order, and read back in the same
// order. Change a struct and both halves change. A Reader that trusts the
// file would index out of bounds on a bad length; this one checks, which is
// the part a FlatBuffers verifier does for you.

struct Writer {
    std::string out;
    void u8(uint8_t v) { out.push_back((char)v); }
    void u32(uint32_t v) { out.append((const char*)&v, 4); }
    void i32(int32_t v) { out.append((const char*)&v, 4); }
    void i64(int64_t v) { out.append((const char*)&v, 8); }
    void f32(float v) { out.append((const char*)&v, 4); }
    void str(const std::string& s) { u32((uint32_t)s.size()); out.append(s); }
};

struct Reader {
    const std::string& in;
    size_t at = 0;
    bool ok = true;
    bool need(size_t n) { if (at + n > in.size()) { ok = false; return false; } return true; }
    uint8_t u8() { if (!need(1)) return 0; return (uint8_t)in[at++]; }
    uint32_t u32() { uint32_t v = 0; if (!need(4)) return 0; std::memcpy(&v, in.data() + at, 4); at += 4; return v; }
    int32_t i32() { return (int32_t)u32(); }
    int64_t i64() { int64_t v = 0; if (!need(8)) return 0; std::memcpy(&v, in.data() + at, 8); at += 8; return v; }
    float f32() { float v = 0; if (!need(4)) return 0; std::memcpy(&v, in.data() + at, 4); at += 4; return v; }
    std::string str() { uint32_t n = u32(); if (!need(n)) return {}; std::string s(in.data() + at, n); at += n; return s; }
};

static void write_players(Writer& w, const std::vector<Player>& ps) {
    w.u32((uint32_t)ps.size());
    for (const Player& p : ps) { w.str(p.name); w.i32(p.hp); w.i32(p.gold); w.f32(p.pos.x); w.f32(p.pos.y); }
}
static void write_items(Writer& w, const std::vector<Item>& bag) {
    w.u32((uint32_t)bag.size());
    for (const Item& it : bag) { w.str(it.name); w.i64(it.qty); w.i64(it.cents); }
}
static void write_quests(Writer& w, const std::vector<Quest>& qs) {
    w.u32((uint32_t)qs.size());
    for (const Quest& q : qs) {
        w.u32(q.id); w.str(q.title); w.u8(q.done ? 1 : 0);
        w.u32((uint32_t)q.children.size());
        for (int32_t c : q.children) w.i32(c);
    }
}

static bool read_players(Reader& r, std::vector<Player>& ps) {
    uint32_t n = r.u32();
    for (uint32_t i = 0; i < n && r.ok; i++) {
        Player p; p.name = r.str(); p.hp = r.i32(); p.gold = r.i32(); p.pos.x = r.f32(); p.pos.y = r.f32();
        ps.push_back(std::move(p));
    }
    return r.ok;
}
static bool read_items(Reader& r, std::vector<Item>& bag) {
    uint32_t n = r.u32();
    for (uint32_t i = 0; i < n && r.ok; i++) {
        Item it; it.name = r.str(); it.qty = r.i64(); it.cents = r.i64();
        bag.push_back(std::move(it));
    }
    return r.ok;
}
static bool read_quests(Reader& r, std::vector<Quest>& qs) {
    uint32_t n = r.u32();
    for (uint32_t i = 0; i < n && r.ok; i++) {
        Quest q; q.id = r.u32(); q.title = r.str(); q.done = r.u8() != 0;
        uint32_t k = r.u32();
        for (uint32_t j = 0; j < k && r.ok; j++) q.children.push_back(r.i32());
        qs.push_back(std::move(q));
    }
    if (!r.ok) return false;
    // The verifier's job, by hand: every child index must name a node.
    for (const Quest& q : qs)
        for (int32_t c : q.children)
            if (c < 0 || (size_t)c >= qs.size()) return false;
    return true;
}

static bool load_all(const std::string& file, std::vector<Player>& ps, std::vector<Item>& bag, std::vector<Quest>& qs) {
    if (file.size() < 4 || file.compare(0, 4, "GSAV") != 0) return false;
    Reader r{ file, 4 };
    return read_players(r, ps) && read_items(r, bag) && read_quests(r, qs);
}

// --- main --------------------------------------------------------------------------

static size_t heap_bytes(const std::string& s) {
    // MSVC's std::string keeps up to 15 bytes inline; longer strings allocate.
    return s.size() > 15 ? s.capacity() + 1 : 0;
}

int main(int argc, char** argv) {
    if (argc != 2) { std::fprintf(stderr, "usage: save <out.bin>\n"); return 2; }

    std::vector<Player> players = {
        { "gus", 70, 1250, { 12.5f, -3.0f } },
        { "mallard", 100, 40, { 0.0f, 0.0f } },
    };
    std::vector<Item> bag = {
        { "potion", 3, 50 }, { "elven longbow", 1, 12000 }, { "arrow", 40, 2 },
    };
    std::vector<Quest> quests;
    quests.push_back(Quest{ 1, "Save the pond", false, {} });
    int32_t root = 0;
    int32_t gather = add_child(quests, root, 2, "Gather reeds", true);
    add_child(quests, gather, 3, "Find the sickle", true);
    add_child(quests, gather, 4, "Cut 10 reeds", true);
    int32_t fox = add_child(quests, root, 5, "Chase the fox", false);
    add_child(quests, fox, 6, "Track it to the hill", false);
    add_child(quests, root, 7, "Return to the elder", false);

    // 2. What the data costs in memory: the struct, plus whatever its
    //    strings and vectors allocated on the heap.
    size_t ph = 0; for (const Player& p : players) ph += heap_bytes(p.name);
    size_t ih = 0; for (const Item& it : bag) ih += heap_bytes(it.name);
    size_t qh = 0; int qa = 0;
    for (const Quest& q : quests) {
        qh += heap_bytes(q.title) + q.children.capacity() * sizeof(int32_t);
        qa += (heap_bytes(q.title) ? 1 : 0) + (q.children.empty() ? 0 : 1);
    }
    std::printf("players: %zu x %zu bytes = %zu, + %zu heap bytes\n", players.size(), sizeof(Player), players.size() * sizeof(Player), ph);
    std::printf("items:   %zu x %zu bytes = %zu, + %zu heap bytes\n", bag.size(), sizeof(Item), bag.size() * sizeof(Item), ih);
    std::printf("quests:  %zu x %zu bytes = %zu, + %zu heap bytes in %d allocations\n", quests.size(), sizeof(Quest), quests.size() * sizeof(Quest), qh, qa);

    // 3. Save, through the serializer above.
    Writer w;
    w.out.append("GSAV");
    write_players(w, players); write_items(w, bag); write_quests(w, quests);
    {
        std::ofstream f(argv[1], std::ios::binary);
        if (!f || !f.write(w.out.data(), (std::streamsize)w.out.size())) { std::fprintf(stderr, "cannot write %s\n", argv[1]); return 1; }
    }
    std::printf("wrote %s: %zu bytes\n", argv[1], w.out.size());

    // 4. Load.
    std::string back;
    {
        std::ifstream f(argv[1], std::ios::binary);
        back.assign((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    }
    std::vector<Player> lp; std::vector<Item> lb; std::vector<Quest> lq;
    bool ok = load_all(back, lp, lb, lq);
    std::printf("loaded: %s, %zu players %zu items %zu quests\n", ok ? "true" : "false", lp.size(), lb.size(), lq.size());

    // 5. Same questions.
    std::printf("  %s hp %d at %g,%g (same: %s)\n", lp[0].name.c_str(), lp[0].hp, lp[0].pos.x, lp[0].pos.y, lp[0].hp == players[0].hp ? "true" : "false");
    std::printf("  inventory worth %lld cents (same: %s)\n", (long long)inventory_value(lb), inventory_value(lb) == inventory_value(bag) ? "true" : "false");
    std::string tree; walk(lq, 0, 1, tree);
    std::printf("  quests done %d of %zu (same: %s)\n%s", count_done(lq, 0), lq.size(), count_done(lq, 0) == count_done(quests, 0) ? "true" : "false", tree.c_str());

    // 6. Corrupt and reload. The Reader checks lengths and indices itself;
    //    without those checks a bad length is an out-of-bounds read.
    auto reload = [&](std::string file, const char* what) {
        std::vector<Player> a; std::vector<Item> b; std::vector<Quest> c;
        bool r = load_all(file, a, b, c);
        std::printf("%-20s accepted %s, quests %zu\n", what, r ? "true" : "false", c.size());
    };
    // Find the quest section: after "GSAV", players and items.
    Reader skip{ back, 4 }; std::vector<Player> sp; std::vector<Item> si; read_players(skip, sp); read_items(skip, si);
    size_t qat = skip.at;
    // First quest: count(4) id(4) title len(4) "Save the pond"(13) done(1) children count(4) child(4)...
    std::string t1 = back; t1[qat + 4 + 4 + 4 + 13 + 1 + 4] ^= 0x40;   // a child index byte
    reload(t1, "flip a link byte:");
    std::string t2 = back; t2[qat + 4 + 4 + 4 + 13] = 1;               // the done flag
    reload(t2, "flip a data byte:");
    std::string t3 = back; t3.pop_back();
    reload(t3, "truncate by 1 byte:");
    return 0;
}
