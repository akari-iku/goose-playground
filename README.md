# goose-playground

システムプログラミング言語 [Goose](https://aardappel.github.io/goose/) を触って、言語仕様と実際の挙動を確かめるための実験リポジトリです。
記事を書くための検証にも使います。

- 公式ページ: https://aardappel.github.io/goose/
- GitHub: https://github.com/aardappel/goose
- 使用バージョン: v2026.3（Windows x64 のビルド済みバイナリ）

Goose は、ヒープアロケーター・GC・参照カウント・ライフタイム注釈のどれも持たないメモリ安全な言語です。
コンパイラは参照の root（寿命を決める変数）を推論し、所有者より長く生きる参照をコンパイルエラーにします。

## 試したこと

| ファイル | 内容 | 結果 |
|---|---|---|
| [experiments/01_dangling_return.goose](experiments/01_dangling_return.goose) | ローカル配列への参照を return する | コンパイルエラー（§9.2） |
| [experiments/02_clear_while_sliced.goose](experiments/02_clear_while_sliced.goose) | スライスが生きている間に配列を clear する | コンパイルエラー（§5.1） |
| [experiments/03_stable_ref_ok.goose](experiments/03_stable_ref_ok.goose) | 10万回 push したあとも最初の参照が有効か | 有効 |
| [experiments/04_pool_stale_ref.goose](experiments/04_pool_stale_ref.goose) | reusable pool の解放済みスロットを参照し続ける | コンパイルは通り、次の所有者の値を読む |
| [experiments/svg-lsystem/](experiments/svg-lsystem/) | 同じ L-system → SVG レンダラーを Goose・Rust・C++ で書く | 4プリセットで出力がバイト一致。構文の対応表と計測は [notes/02](notes/02-lsystem-in-three-languages.md) |
| [experiments/save-data/](experiments/save-data/) | セーブデータを `to_bytes` / `from_bytes` で保存・復元し、C++ の手書きシリアライザと比べる | シリアライザなしで往復。壊したリンクは拒否、壊したデータは受理。詳細は [notes/03](notes/03-save-file-without-a-serializer.md) |

詳しい出力と気づきは [notes/](notes/) にあります。

## ディレクトリ構成

| パス | 内容 |
|---|---|
| `experiments/` | 自分で書いた `.goose` ファイルと、比較用の Rust / C++ |
| `notes/` | 検証メモ（実行結果、エラー出力、計測） |
| `tools/` | ダウンロードした Goose 本体の展開先（各自で用意） |

## セットアップ

1. [GitHub Releases](https://github.com/aardappel/goose/releases) から自分の OS 向けのアーカイブをダウンロードします。
2. `tools/goose-v2026.3/` に展開します（`goose.exe` がこの直下に来るようにします）。
3. 実行します。

```bash
tools/goose-v2026.3/goose.exe experiments/01_dangling_return.goose
```

実行ファイルを作る場合は `goose build` を使います。Windows では Visual Studio 2022 Build Tools か clang が必要です。

```bash
tools/goose-v2026.3/goose.exe build experiments/03_stable_ref_ok.goose
```
