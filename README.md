# BonDriver_Dyna

ダイナコネクティブ（Dynaconnective）の USB 地デジチューナー DY-UD200 を、Windows の TVTest・EDCB で使う BonDriver（`IBonDriver2`）です。  
Linux の録画コマンド [recdyna](https://github.com/waya0125/recdyna) の調査と検証結果をもとに、WinUSB で機器を直接操作する形で書き直しました。  
無改造ファームウェア（初期版・`1.0.5-2`）の TS の XOR 復号と本体スロットの B-CAS 復号に既定で対応しています。  
古い未署名のカーネルドライバーは不要で、WinUSB ベースのため HVCI が有効な Windows 10・11 でも動作する設計です（実機確認済みは Windows 7 x64）。

## 対応機種

| 機種 | USB ID | 放送波 | B-CAS | 対応 Windows | 詳細・動作確認 |
|---|---|---|---|---|---|
| Dynaconnective DY-UD200 | `1c11:1004` | 地デジ・CATV（1 チューナー） | 本体のスロット | 7（x64、VM で実機確認済み）。10・11 は未確認（WinUSB のため動く想定）。x86 はビルドのみ | [docs/DY-UD200.md](docs/DY-UD200.md) |

## 関連プロジェクト

- Linux の録画コマンド: [recdyna](https://github.com/waya0125/recdyna)
- 手元のチューナーの動作確認の一覧: [チューナー動作確認一覧](https://waya0125.github.io/tuners/)

## 必要なもの

- [Zadig](https://zadig.akeo.ie/)（WinUSB ドライバーの割り当てに使う）
- B-CAS カード（本体のスロットに挿す）
- ファームウェアやメーカーのドライバーは不要です。

メーカーは 2010 年 10 月に自己破産を申請し、公式のサイトはなくなっています。  
機器の情報は、有志の BonDriver_dyud の配布ページ（[Wayback Machine](http://web.archive.org/web/20130527152129/http://dyud200.zxq.net/)）と、有志のまとめ Wiki「[USB地デジチューナ DY-UD200 まとめwiki](https://w.atwiki.jp/dy-ud200/)」にあります。

## 入手

[Releases](https://github.com/waya0125/BonDriver_Dyna/releases) から、MSVC でビルドした x64 版と x86 版の DLL を入手できます。

## 準備

1. Zadig で DY-UD200（`1c11:1004`）に **WinUSB** ドライバーを割り当てます。
2. TVTest・EDCB に合わせて、`BonDriver_Dyna.dll`（x64 か x86）を BonDriver のフォルダーに置きます。
3. 設定を変える場合は、`BonDriver_Dyna.ini.sample` を `BonDriver_Dyna.ini` に名前を変えて、DLL と同じフォルダーに置きます。

## 使い方

- チャンネル空間は `UHF`（13〜62ch）・`CATV`（C13〜C63ch）・`VHF`（1〜12ch）の 3 つです。  
  TVTest でチャンネルスキャンをしてから使ってください。
- 本体のスロットのカードで復号するので、TVTest・EDCB 側の B-CAS カードリーダーの設定は「なし」にします。

### 設定（`BonDriver_Dyna.ini`）

```ini
[DYUD200]
; 使用する DY-UD200 のインデックス（0 始まり。複数台接続時に指定）
DeviceIndex=0

; 本体内蔵 B-CAS スロットで ARIB STD-B25 復号を行う（1=有効 / 0=無効、既定 1）
; カード未挿入時は自動的に無効となり生 TS を出力します
EnableB25=1

; パッチ済みファームウェアで TS XOR 復号を無効化する場合は 1
NoXor=0

; 選局直後の安定化フィルター（Settler）を無効化する場合は 1
NoSettle=0

; ワンセグモードで受信する場合は 1
OneSeg=0

; 選局後のロック待機タイムアウト（ミリ秒）
LockTimeoutMs=3000
```

## ビルド

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

## 注意事項

- 地デジ専用です。BS・CS は受けられません。
- TVTest・EDCB では、MSVC でビルドした DLL を使ってください。  
  MinGW でビルドした DLL は C++ の ABI が違うので、TVTest が落ちます。

## ライセンス

MIT License（[recdyna](https://github.com/waya0125/recdyna) と同じプロトコルの知見とテストベクタを使っています）
