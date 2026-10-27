// The positive case: a reference taken before 100000 pushes is still valid,
// because a grow-only array never moves its elements. Expected: compiles,
// prints 99.5.

struct Item { id: i32, weight: f32 }

fn main() {
    var items: Item[>..] = [];
    let first .= items.push(Item { 1, 0.5 });
    for i in 2..100001 { items.push(Item { i as i32, i * 0.5 }); }
    first.weight = 99.5;
    print(items.len, " items, first weight ", items[0].weight);
}
