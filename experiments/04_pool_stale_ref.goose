// The one hole the docs admit: a `reusable` pool recycles slots, so a
// reference to a freed slot reads whatever the next owner put there. The
// docs call this a logic bug, not memory corruption, because the slot is
// always a valid Item. Expected: compiles, and `stale` shows id 12, not 10.

struct Item { id: i32, weight: f32 }

fn main() {
    reusable var pool: Item[>..] = [];
    let stale: Item& = pool.alloc_ref(Item { 10, 1.0 });
    print("before free: id ", stale.id);
    pool.free(pool.index_of(stale));
    let next: Item& = pool.alloc_ref(Item { 12, 2.0 });   // takes the slot back
    print("after realloc: stale id ", stale.id, ", next id ", next.id,
          ", same slot ", stale .== next);
}
