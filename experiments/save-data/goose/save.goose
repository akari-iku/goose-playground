// A game save file, the FlatBuffers way but as plain language values: the
// bytes the program computes with are the bytes on disk. Three sections --
// fixed-size players, variable-size inventory items, and a quest tree
// linked by relative offsets -- are written with `to_bytes`, read back with
// `from_bytes`, queried again, and then deliberately corrupted to see what
// the verifier accepts and rejects.
//
// Run:   goose save.goose -- ../out/save.gsav
// Build: goose build save.goose

import std;
import os;

// --- the data -----------------------------------------------------------------

// Fixed-size: the name is a limited array, so a Player is one flat value and
// an array of players is indexable.
struct Vec2 { x: f32, y: f32 }
struct Player { name: u8[..16], hp: i32, gold: i32, pos: Vec2 }

// Variable-size: the name takes exactly the bytes it needs behind a varint
// length, and the counts are varints. An array of these is walked, not
// indexed.
struct Item { name: u8[varint], qty: varint, cents: varint }

// A tree with no pointers: `first_child` and `next` are 4-byte offsets from
// their own field to the target node, which must live in the same pool.
struct Quest {
    id: u32,
    title: u8[..24],
    done: bool = false,
    first_child: Quest&<u32>?,
    next: Quest&<u32>?,
}

// The parent comes in as an index, not a reference: a relative link may only
// be stored between two references the compiler can see share one root, and
// a `Quest&` parameter could be rooted anywhere. Resolving `pool[parent]`
// here makes both ends visibly rooted at `pool`.
fn add_child(pool: Quest[>..]&, parent_at: i64, id: u32, title: u8[:], done: bool) -> i64 {
    let at = pool.len;
    let node .= pool.push(Quest { id: id, title: title, done: done });
    let parent .= pool[parent_at];
    // Append as the last sibling, so a walk prints in insertion order.
    let first = parent.first_child;
    guard first else { parent.first_child .= node; return at; }
    var cur .= first;
    loop {
        let n = cur.next;
        guard n else { break; }
        cur .= n;
    }
    cur.next .= node;
    return at;
}

recursive fn walk(q: Quest&, depth: i64, out: u8[>..]&) {
    for _ in depth { out.append("  "); }
    format(out, if q.done { "[x] " } else { "[ ] " }, q.id, " ", q.title, "\n");
    let c = q.first_child;
    if c { walk(c, depth + 1, out); }
    let n = q.next;
    if n { walk(n, depth, out); }
}

recursive fn count_done(q: Quest&) -> i64 {
    var n = if q.done { 1 } else { 0 };
    let c = q.first_child;
    if c { n += count_done(c); }
    let s = q.next;
    if s { n += count_done(s); }
    return n;
}

fn inventory_value(bag: const Item[:]) -> i64 {
    var t = 0;
    for it in bag { t += it.qty * it.cents; }
    return t;
}

// --- the save file --------------------------------------------------------------
// Layout: "GSAV" then three images back to back, each a ULEB byte count and
// the array's element region. `to_bytes(msg)` appends exactly that.

fn load_section<T>(file: const u8[:], at: i64&) -> T, bool {
    let n, k = parse_uleb(file[at..]);
    // A rejected image is an empty array and false; `from_bytes` of nothing
    // gives exactly that, built in place (a pool of relative links cannot be
    // copied out of a local, so there is no `var empty: T` to return).
    guard k > 0 && at + k + n <= file.len else { return from_bytes<T>(""); }
    let image = file[at .. at + k + n];
    at += k + n;
    return from_bytes<T>(image);
}

fn main() {
    let argv = args();
    guard argv.len == 2 else { write_stderr("usage: save <out.gsav>\n"); exit(2); }
    let path = argv[1];

    // 1. Build the world.
    var players: Player[>..] = [];
    players.push(Player { name: "gus", hp: 70, gold: 1250, pos: Vec2 { 12.5, -3.0 } });
    players.push(Player { name: "mallard", hp: 100, gold: 40, pos: Vec2 { 0.0, 0.0 } });

    var bag: Item[>..] = [];
    bag.push(Item { name: "potion", qty: 3, cents: 50 });
    bag.push(Item { name: "elven longbow", qty: 1, cents: 12000 });
    bag.push(Item { name: "arrow", qty: 40, cents: 2 });

    var quests: Quest[>..] = [];
    quests.push(Quest { id: 1, title: "Save the pond" });
    let root = 0;
    let gather = add_child(quests, root, 2, "Gather reeds", true);
    add_child(quests, gather, 3, "Find the sickle", true);
    add_child(quests, gather, 4, "Cut 10 reeds", true);
    let fox = add_child(quests, root, 5, "Chase the fox", false);
    add_child(quests, fox, 6, "Track it to the hill", false);
    add_child(quests, root, 7, "Return to the elder", false);

    // 2. What the data costs in memory, which is also what it costs on disk.
    print("players: ", players.len, " x ", bytes_of(players).len / players.len, " bytes = ", bytes_of(players).len);
    print("items:   ", bag.len, " items in ", bytes_of(bag).len, " bytes");
    print("quests:  ", quests.len, " x ", bytes_of(quests).len / quests.len, " bytes = ", bytes_of(quests).len);

    // 3. Save: a tag and three images. No serializer was written.
    var file: u8[>..] = [];
    file.append("GSAV");
    players.to_bytes(file);
    bag.to_bytes(file);
    quests.to_bytes(file);
    guard write_file(path, file) else { write_stderr(str("cannot write ", path, "\n")); exit(1); }
    print("wrote ", path, ": ", file.len, " bytes");

    // 4. Load: read, verify, and the result is an ordinary array.
    var back: u8[>..] = [];
    guard read_file(path, back) else { write_stderr("cannot re-read\n"); exit(1); }
    guard back.len >= 4 && back[0..4] == "GSAV" else { write_stderr("not a save file\n"); exit(1); }
    var at = 4;
    var lp, ok1 = load_section<Player[>..]>(back, at);
    var lb, ok2 = load_section<Item[>..]>(back, at);
    var lq, ok3 = load_section<Quest[>..]>(back, at);
    print("loaded: players ", ok1, " items ", ok2, " quests ", ok3, ", consumed ", at, " of ", back.len);

    // 5. The loaded copy answers the same questions, through the same links.
    print("  ", lp[0].name, " hp ", lp[0].hp, " at ", lp[0].pos.x, ",", lp[0].pos.y,
          " (same: ", lp[0].hp == players[0].hp, ")");
    print("  inventory worth ", inventory_value(lb), " cents (same: ",
          inventory_value(lb) == inventory_value(bag), ")");
    var tree: u8[>..] = [];
    walk(lq[0], 1, tree);
    print("  quests done ", count_done(lq[0]), " of ", lq.len, " (same: ",
          count_done(lq[0]) == count_done(quests[0]), ")\n", tree);
    // The loaded array is a normal array: it still grows and links.
    add_child(lq, 0, 8, "Added after load", false);
    print("  after push: ", lq.len, " quests, done ", count_done(lq[0]));

    // 6. Corrupt the quest section and see what the verifier says.
    // The quest image starts where `at` left off after items; find it again.
    var qat = 4;
    var skip1, s1 = load_section<Player[>..]>(back, qat);
    var skip2, s2 = load_section<Item[>..]>(back, qat);
    let n, k = parse_uleb(back[qat..]);
    let payload = qat + k;      // first byte of the first Quest
    print("quest image at ", qat, ", payload at ", payload, ", ", n, " bytes");

    // a. A link byte. id is 4 bytes, title 25 (length + 24), done 1: the
    //    first_child link starts at payload + 30.
    var t1: u8[>..] = [];
    t1.append(back);
    t1[payload + 30] = t1[payload + 30] ^ 0x40;
    var a1 = qat;
    var r1, ok_link = load_section<Quest[>..]>(t1, a1);
    print("flip a link byte:   accepted ", ok_link, ", nodes ", r1.len);

    // b. A data byte: the first quest's `done` flag. Still a valid tree.
    var t2: u8[>..] = [];
    t2.append(back);
    t2[payload + 29] = 1;
    var a2 = qat;
    var r2, ok_data = load_section<Quest[>..]>(t2, a2);
    print("flip a data byte:   accepted ", ok_data, ", root done now ", r2[0].done,
          " (was ", quests[0].done, ")");

    // c. Truncate the file by one byte.
    var t3: u8[>..] = [];
    t3.append(back);
    t3.pop();
    var a3 = qat;
    var r3, ok_cut = load_section<Quest[>..]>(t3, a3);
    print("truncate by 1 byte: accepted ", ok_cut, ", nodes ", r3.len);

    // d. Feed the item image to the quest loader: wrong type, same bytes.
    var a4 = 4;
    var skip3, s3 = load_section<Player[>..]>(back, a4);
    var r4, ok_type = load_section<Quest[>..]>(back, a4);
    print("items as quests:    accepted ", ok_type, ", nodes ", r4.len);
}
