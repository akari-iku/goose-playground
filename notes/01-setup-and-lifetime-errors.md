# 01. セットアップと、参照の寿命エラーを踏んでみる

日付: 2026-10-28
Goose: v2026.3（`goose version` → `goose runtime 4d3da949e563`）
環境: Windows 11、ビルド済みバイナリ `goose-v2026.3-windows-x64.zip`

## セットアップ

1. GitHub Releases から zip をダウンロードし、`tools/goose-v2026.3/` に展開した。
2. `goose samples/01_tour.goose` でプロセス内実行ができた。追加のツールは不要だった。
3. `goose toolchain` を実行すると、PATH に `cl` がなくても VS 2022 Build Tools の MSVC を自動検出した。
4. `goose build experiments/03_stable_ref_ok.goose` で exe が生成できた。中間の C ファイルは `~/.goose/build/prog/` に置かれる。

## 実験

### 01. ローカル配列への参照を return する

```goose
fn make() -> i64& {
    var xs: i64[>..] = [];
    xs.push(1);
    return xs[0];
}
```

結果: コンパイルエラー。

```
01_dangling_return.goose:8: error: returning a reference rooted in xs, which dies with this function (§9.2)
    return xs[0];
  in fn make() instantiated from 01_dangling_return.goose:12
  in fn main() instantiated from 01_dangling_return.goose:11
```

エラーが参照の root（`xs`）を名指しし、仕様書の節番号（§9.2）を添えている。

### 02. スライスが生きている間に配列を clear する

```goose
var line: u8[>..] = [];
line.append("hello world");
let w = line[..5];
line.clear();
print(w);
```

結果: コンパイルエラー。

```
02_clear_while_sliced.goose:8: error: cannot clear line while w is still used: it may hold a reference or slice into it (§5.1)
```

`w` の最後の使用が `clear()` より後にあることを見ている。`print(w)` を `clear()` の前に動かせば通る。

### 03. 10万回 push したあとも最初の参照が有効

```goose
var items: Item[>..] = [];
let first .= items.push(Item { 1, 0.5 });
for i in 2..100001 { items.push(Item { i as i32, i * 0.5 }); }
first.weight = 99.5;
```

結果: `100000 items, first weight 99.5`。grow-only 配列は要素を移動しないため、`push` が返した参照はそのまま使える。

### 04. reusable pool の解放済みスロットを参照し続ける

```goose
reusable var pool: Item[>..] = [];
let stale: Item& = pool.alloc_ref(Item { 10, 1.0 });
pool.free(pool.index_of(stale));
let next: Item& = pool.alloc_ref(Item { 12, 2.0 });
print(stale.id);   // 12
```

結果: コンパイルは通り、`stale id 12, next id 12, same slot true` と表示された。
ドキュメントが認めている唯一の穴で、スロットは常に有効な `Item` のため、値は間違うがメモリ破壊は起きないという説明になっている。

## 気づき

- `.=` は参照の束縛、`=` はコピー、`.==` は同一ターゲットの判定。
- 複数の戻り値はタプル型ではなく呼び出し規約。`let q, ok = divide(7, 2);` と受ける。
- 文字列型はなく、`u8[>..]` が文字列ビルダーを兼ねる。
