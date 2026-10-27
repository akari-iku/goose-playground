// Returning a reference into a local growable array. The array dies when
// `make` returns, so the reference would dangle. Expected: a compile error
// that names the root (`xs`) and where the reference tries to go.

fn make() -> i64& {
    var xs: i64[>..] = [];
    xs.push(1);
    return xs[0];
}

fn main() {
    let r = make();
    print(r);
}
