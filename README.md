# BonDriver_Dyna

ダイナコネクティブ（Dynaconnective）製 USB 地上デジタルチューナー（**DY-UD200**, `1c11:1004`）向けのモダンな Windows 用 **BonDriver（`IBonDriver2`）** です。  
Linux 向け録画ツール `recdyna` の設計・検証結果をもとに、**WinUSB 直接呼び出し・64bit / 32bit 両対応・外部ランタイム依存なし** で再実装しています。

## 主な特徴

- **x64 / x86 両対応**: 現代の TVTest・EDCB（64bit / 32bit）でそのまま利用可能
- **WinUSB 直接呼び出し**: 古い未署名カーネルドライバ不要。Zadig で WinUSB ドライバを当てるだけで Windows 10 / 11（HVCI 有効環境含む）で動作
- **FW `1.0.5-2` 自動検出・再初期化**: 初期 FW・再販品 FW（`1.0.5-2`）の両方に対応
- **安定化受信**: `24064` バイト（`47×512 = 128×188`）× 16 本の非同期 `OVERLAPPED` I/O ＋ 選局直後の同期揃え（`Aligner`）・過渡期除去（`Settler`）
- **Linux / Windows 両方でビルド可能**: Windows（MSVC）と Linux（`mingw-w64` クロスビルド）の両方に対応

## 使い方（Windows）

1. [Zadig](https://zadig.akeo.ie/) などで DY-UD200（`1c11:1004`）に **WinUSB** ドライバをインストールします。
2. TVTest / EDCB のアーキテクチャに合わせて `BonDriver_Dyna.dll`（x64 または x86）を配置します。
3. 必要に応じて `BonDriver_Dyna.ini.sample` を `BonDriver_Dyna.ini` にリネームして設定を変更します。

## ビルド方法

### Linux 上での単体テスト（コアロジック）

```bash
cmake -S . -B build-test
cmake --build build-test -j
ctest --test-dir build-test --output-on-failure
```

### Linux 上での Windows DLL（x64 / x86）クロスビルド

```bash
chmod +x scripts/build-win.sh
./scripts/build-win.sh
```

`dist/x64/BonDriver_Dyna.dll` と `dist/x86/BonDriver_Dyna.dll` が生成されます。

### Windows（MSVC）でのビルド（TVTest / EDCB 用・推奨）

TVTest / EDCB は MSVC でビルドされているため、BonDriver DLL も **MSVC（Visual Studio / Build Tools）** でビルドする必要があります（MinGW ビルドは C++ ABI が異なるため TVTest でクラッシュします）。

```powershell
# x64
cmake -S . -B build-x64 -A x64
cmake --build build-x64 --config Release

# x86
cmake -S . -B build-x86 -A Win32
cmake --build build-x86 --config Release
```

`v*` タグを push すると、GitHub Actions（`windows-latest` + MSVC）が自動で x64 / x86 の DLL をビルドして Release に添付します。

## ライセンス

MIT License（[recdyna](../recdyna) と同じプロトコル知見・テストベクタを使用）
