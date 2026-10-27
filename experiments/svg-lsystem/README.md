# svg-lsystem

同じ L-system レンダラーを Goose・Rust・C++ で書き、出力の一致と速度・メモリを比べる実験です。
結果と考察は [notes/02-lsystem-in-three-languages.md](../../notes/02-lsystem-in-three-languages.md) にあります。

## ビルドと実行

すべて `experiments/svg-lsystem/` で実行します。引数は `<preset> <depth> <out.svg>` で、preset は `plant` / `koch` / `dragon` / `sierpinski` です。

Goose（プロセス内実行、またはネイティブビルド）:

```bash
../../tools/goose-v2026.3/goose.exe goose/lsystem.goose -- plant 6 out/plant.goose.svg
```

```bash
../../tools/goose-v2026.3/goose.exe build goose/lsystem.goose && goose/lsystem.exe plant 6 out/plant.goose.svg
```

Rust:

```bash
cargo build --release --manifest-path rust/Cargo.toml && rust/target/release/lsystem.exe plant 6 out/plant.rust.svg
```

C++（MSVC。Git Bash から `vcvars64.bat` を直接呼ぶと失敗したため、PowerShell の DevShell を経由します）:

```bash
powershell -NoProfile -Command "& 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\Launch-VsDevShell.ps1' -Arch amd64 -SkipAutomaticLocation | Out-Null; Set-Location cpp; cl /nologo /O2 /EHsc /std:c++20 /Fo:build\ /Fe:build\lsystem.exe lsystem.cpp"
```

## 出力の一致確認

```bash
for p in "plant 6" "koch 4" "dragon 12" "sierpinski 6"; do set -- $p; goose/lsystem.exe $1 $2 out/$1.goose.svg >/dev/null; rust/target/release/lsystem.exe $1 $2 out/$1.rust.svg >/dev/null; cpp/build/lsystem.exe $1 $2 out/$1.cpp.svg >/dev/null; cmp out/$1.goose.svg out/$1.rust.svg && cmp out/$1.goose.svg out/$1.cpp.svg && echo "$1 same"; done
```

## 計測

```bash
python -I bench.py plant 8 10
```
