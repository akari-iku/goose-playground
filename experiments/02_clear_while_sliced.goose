// A slice into a growable array is still live when the array shrinks.
// Expected: a compile error, because `w` is used after `line.clear()`.

fn main() {
    var line: u8[>..] = [];
    line.append("hello world");
    let w = line[..5];
    line.clear();
    print(w);
}
