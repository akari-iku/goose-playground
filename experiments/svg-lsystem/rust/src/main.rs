// The Rust twin of ../goose/lsystem.goose: same presets, same turtle, same
// SVG bytes. Kept structurally parallel on purpose so the two can be read
// side by side; idiomatic shortcuts are taken only where they do not change
// the shape of the program.
//
// Run:   cargo run --release -- plant 6 ../out/plant.rust.svg

use std::fmt::Write as _;
use std::process::exit;
use std::time::Instant;

struct Rule {
    from: u8,
    to: &'static str,
}

struct Preset {
    name: &'static str,
    axiom: &'static str,
    rules: &'static [Rule],
    angle: f64,
    step: f64,
}

#[derive(Clone, Copy)]
struct Turtle {
    x: f64,
    y: f64,
    heading: f64,
}

struct Segment {
    x1: f64,
    y1: f64,
    x2: f64,
    y2: f64,
}

static PRESETS: &[Preset] = &[
    Preset { name: "plant", axiom: "X", angle: 25.0, step: 4.0,
             rules: &[Rule { from: b'X', to: "F+[[X]-X]-F[-FX]+X" }, Rule { from: b'F', to: "FF" }] },
    Preset { name: "koch", axiom: "F", angle: 90.0, step: 3.0,
             rules: &[Rule { from: b'F', to: "F+F-F-F+F" }] },
    Preset { name: "dragon", axiom: "F", angle: 90.0, step: 4.0,
             rules: &[Rule { from: b'F', to: "F+G" }, Rule { from: b'G', to: "F-G" }] },
    Preset { name: "sierpinski", axiom: "F-G-G", angle: 120.0, step: 6.0,
             rules: &[Rule { from: b'F', to: "F-G+F+G-F" }, Rule { from: b'G', to: "GG" }] },
];

fn rewrite(src: &[u8], rules: &[Rule], out: &mut Vec<u8>) {
    for &c in src {
        match rules.iter().find(|r| r.from == c) {
            Some(r) => out.extend_from_slice(r.to.as_bytes()),
            None => out.push(c),
        }
    }
}

fn interpret(program: &[u8], p: &Preset, segs: &mut Vec<Segment>) {
    let mut t = Turtle { x: 0.0, y: 0.0, heading: -90.0 };
    let mut stack: Vec<Turtle> = Vec::new();
    for &c in program {
        match c {
            b'F' | b'G' => {
                let rad = t.heading.to_radians();
                let nx = t.x + rad.cos() * p.step;
                let ny = t.y + rad.sin() * p.step;
                segs.push(Segment { x1: t.x, y1: t.y, x2: nx, y2: ny });
                t.x = nx;
                t.y = ny;
            }
            b'+' => t.heading += p.angle,
            b'-' => t.heading -= p.angle,
            b'[' => stack.push(t),
            b']' => {
                if let Some(s) = stack.pop() {
                    t = s;
                }
            }
            _ => {}
        }
    }
}

fn bounds(segs: &[Segment]) -> (f64, f64, f64, f64) {
    let (mut minx, mut miny, mut maxx, mut maxy): (f64, f64, f64, f64) = (1.0e300, 1.0e300, -1.0e300, -1.0e300);
    for s in segs {
        minx = minx.min(s.x1.min(s.x2));
        maxx = maxx.max(s.x1.max(s.x2));
        miny = miny.min(s.y1.min(s.y2));
        maxy = maxy.max(s.y1.max(s.y2));
    }
    (minx, miny, maxx, maxy)
}

fn write_svg(segs: &[Segment], out: &mut String) {
    let (minx, miny, maxx, maxy) = bounds(segs);
    let margin = 10.0;
    write!(out, "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"{:.2} {:.2} {:.2} {:.2}\">\n",
           minx - margin, miny - margin, maxx - minx + margin * 2.0, maxy - miny + margin * 2.0).unwrap();
    out.push_str("<path fill=\"none\" stroke=\"#2a7\" stroke-width=\"1\" d=\"");
    for (i, s) in segs.iter().enumerate() {
        if i > 0 {
            out.push('\n');
        }
        write!(out, "M{:.2} {:.2}L{:.2} {:.2}", s.x1, s.y1, s.x2, s.y2).unwrap();
    }
    out.push_str("\"/>\n</svg>\n");
}

fn main() {
    let argv: Vec<String> = std::env::args().collect();
    if argv.len() != 4 {
        eprintln!("usage: lsystem <plant|koch|dragon|sierpinski> <depth> <out.svg>");
        exit(2);
    }
    let depth: usize = match argv[2].parse::<i64>() {
        Ok(d) if (0..=12).contains(&d) => d as usize,
        _ => {
            eprintln!("depth must be 0..12");
            exit(2);
        }
    };
    let Some(chosen) = PRESETS.iter().find(|p| p.name == argv[1]) else {
        eprintln!("unknown preset: {}", argv[1]);
        exit(2);
    };

    let t0 = Instant::now();
    let mut cur: Vec<u8> = chosen.axiom.as_bytes().to_vec();
    for _ in 0..depth {
        let mut next: Vec<u8> = Vec::new();
        rewrite(&cur, chosen.rules, &mut next);
        cur = next;
    }

    let mut segs: Vec<Segment> = Vec::new();
    interpret(&cur, chosen, &mut segs);

    let mut svg = String::new();
    write_svg(&segs, &mut svg);
    let ms = t0.elapsed().as_secs_f64() * 1000.0;

    if std::fs::write(&argv[3], &svg).is_err() {
        eprintln!("cannot write {}", argv[3]);
        exit(1);
    }
    println!("{} depth {}: {} symbols, {} segments, {} bytes of svg, {:.1} ms",
             chosen.name, depth, cur.len(), segs.len(), svg.len(), ms);
}
