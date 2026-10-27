// An L-system renderer that writes SVG. The same program exists in Rust and
// C++ next door, so the three can be compared line by line and their output
// diffed: given the same preset and depth, all three must write identical
// bytes.
//
// Run:   goose lsystem.goose -- plant 6 ../out/plant.goose.svg
// Build: goose build lsystem.goose

import std;
import os;
import math;

// A rule rewrites one symbol into a string. The replacement is a limited
// array, so a Rule is one flat value and an array of rules is one block.
struct Rule { from: u8, to: u8[..32] }

// A preset is also flat: the axiom and rules live inline, not behind pointers.
struct Preset {
    name: u8[..16],
    axiom: u8[..16],
    rules: Rule[..4],
    angle: f64,     // degrees per + or -
    step: f64,      // length of one F
}

// The turtle state that [ and ] push and pop.
struct Turtle { x: f64, y: f64, heading: f64 }

// A line segment to draw. The SVG writer needs the bounds first, so segments
// are collected before anything is written.
struct Segment { x1: f64, y1: f64, x2: f64, y2: f64 }

fn presets() -> Preset[..4] {
    return [
        Preset { name: "plant", axiom: "X", angle: 25.0, step: 4.0,
                 rules: [Rule { 'X', "F+[[X]-X]-F[-FX]+X" }, Rule { 'F', "FF" }] },
        Preset { name: "koch", axiom: "F", angle: 90.0, step: 3.0,
                 rules: [Rule { 'F', "F+F-F-F+F" }] },
        Preset { name: "dragon", axiom: "F", angle: 90.0, step: 4.0,
                 rules: [Rule { 'F', "F+G" }, Rule { 'G', "F-G" }] },
        Preset { name: "sierpinski", axiom: "F-G-G", angle: 120.0, step: 6.0,
                 rules: [Rule { 'F', "F-G+F+G-F" }, Rule { 'G', "GG" }] },
    ];
}

// One generation: every symbol with a rule is replaced, the rest copied.
// `out` is the caller's builder, so nothing here owns growable data that
// would have to be returned.
fn rewrite(src: const u8[:], rules: const Rule[:], out: u8[>..]&) {
    for c in src {
        var replaced = false;
        for &r in rules {
            if r.from == c { out.append(r.to); replaced = true; break; }
        }
        if !replaced { out.push(c); }
    }
}

// The turtle walks the final string. F and G draw, + and - turn, [ and ]
// save and restore the state on a grow-shrink array: the stack is the array.
fn interpret(program: const u8[:], p: const Preset&, segs: Segment[>..]&) {
    var t = Turtle { 0.0, 0.0, -90.0 };     // SVG y grows downward: -90 is "up"
    var stack: Turtle[>..<] = [];
    for c in program {
        match c {
            'F', 'G' => {
                let rad = radians(t.heading);
                let nx = t.x + cos(rad) * p.step;
                let ny = t.y + sin(rad) * p.step;
                segs.push(Segment { t.x, t.y, nx, ny });
                t.x = nx; t.y = ny;
            },
            '+' => { t.heading += p.angle; },
            '-' => { t.heading -= p.angle; },
            '[' => { stack.push(t); },
            ']' => { if stack.len > 0 { t = stack.pop(); } },
            _ => {},
        }
    }
}

fn bounds(segs: const Segment[:]) -> f64, f64, f64, f64 {
    var minx = 1.0e300; var miny = 1.0e300; var maxx = -1.0e300; var maxy = -1.0e300;
    for s in segs {
        minx = min(minx, min(s.x1, s.x2)); maxx = max(maxx, max(s.x1, s.x2));
        miny = min(miny, min(s.y1, s.y2)); maxy = max(maxy, max(s.y1, s.y2));
    }
    return minx, miny, maxx, maxy;
}

fn write_svg(segs: const Segment[:], out: u8[>..]&) {
    let minx, miny, maxx, maxy = bounds(segs);
    let margin = 10.0;
    out.append("<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"");
    format_flt(out, minx - margin, 2); out.push(' ');
    format_flt(out, miny - margin, 2); out.push(' ');
    format_flt(out, maxx - minx + margin * 2.0, 2); out.push(' ');
    format_flt(out, maxy - miny + margin * 2.0, 2);
    out.append("\">\n<path fill=\"none\" stroke=\"#2a7\" stroke-width=\"1\" d=\"");
    for s, i in segs {
        if i > 0 { out.push('\n'); }
        out.append("M"); format_flt(out, s.x1, 2); out.push(' '); format_flt(out, s.y1, 2);
        out.append("L"); format_flt(out, s.x2, 2); out.push(' '); format_flt(out, s.y2, 2);
    }
    out.append("\"/>\n</svg>\n");
}

fn main() {
    let argv = args();
    guard argv.len == 4 else {
        write_stderr("usage: lsystem <plant|koch|dragon|sierpinski> <depth> <out.svg>\n");
        exit(2);
    }
    let depth, ok = parse_int(argv[2]);
    guard ok && depth >= 0 && depth <= 12 else { write_stderr("depth must be 0..12\n"); exit(2); }

    let all = presets();
    var chosen: Preset? = null;
    for &p in all { if p.name == argv[1] { chosen .= p; } }
    guard chosen else { write_stderr(str("unknown preset: ", argv[1], "\n")); exit(2); }

    let t0 = clock();
    // The program string doubles or more per generation. Each generation
    // builds the next in a scratch builder that dies at the end of the loop
    // body; `cur` is cleared and refilled, so the peak is two strings.
    var cur: u8[>..] = [];
    cur.append(chosen.axiom);
    for _ in depth {
        var next: u8[>..] = [];
        rewrite(cur, chosen.rules, next);
        cur.clear();
        cur.append(next);
    }

    var segs: Segment[>..] = [];
    interpret(cur, chosen, segs);

    var svg: u8[>..] = [];
    write_svg(segs, svg);
    let ms = (clock() - t0) * 1000.0;

    guard write_file(argv[3], svg) else { write_stderr(str("cannot write ", argv[3], "\n")); exit(1); }
    var note: u8[>..] = [];
    format(note, chosen.name, " depth ", depth, ": ", cur.len, " symbols, ", segs.len,
           " segments, ", svg.len, " bytes of svg, ");
    format_flt(note, ms, 1);
    note.append(" ms");
    print(note);
}
