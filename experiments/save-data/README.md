# save-data

ゲームのセーブデータ（プレイヤー、インベントリ、クエストの木）を Goose の `to_bytes` / `from_bytes` で保存・復元し、同じことを C++ の手書きシリアライザで書いて、サイズ・コード量・壊れたファイルの扱いを比べる実験です。
結果と考察は [notes/03-save-file-without-a-serializer.md](../../notes/03-save-file-without-a-serializer.md) にあります。

## ビルドと実行

すべて `experiments/save-data/` で実行します。

Goose:

```bash
../../tools/goose-v2026.3/goose.exe goose/save.goose -- out/save.gsav
```

C++（MSVC、PowerShell の DevShell 経由）:

```bash
powershell -NoProfile -Command "& 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\Launch-VsDevShell.ps1' -Arch amd64 -SkipAutomaticLocation | Out-Null; Set-Location cpp; cl /nologo /O2 /EHsc /std:c++20 /Fo:build\ /Fe:build\save.exe save.cpp" && cpp/build/save.exe out/save.cpp.bin
```

ファイルの中身を見る:

```bash
xxd out/save.gsav | head
```
