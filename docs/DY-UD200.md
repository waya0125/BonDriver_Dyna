# Dynaconnective DY-UD200

## 1. 概要

| 項目 | 内容 |
|---|---|
| USB ID | `1c11:1004`（USB 2.0 High-Speed、バスパワー 500mA） |
| 放送波 | 地デジ（UHF 13〜62ch・CATV C13〜C63ch・VHF 1〜12ch） |
| チューナー数 | 1 |
| B-CAS | 本体のスロット（カードが無いときは、スクランブルのかかった TS をそのまま出す） |
| ファームウェア | 本体に入っている。初期の版と再販品の `1.0.5-2` の両方に対応 |
| 対応 Windows | 7〜11（x64・x86） |
| Windows | [BonDriver_Dyna](../README.md) |
| Linux | [recdyna](https://github.com/waya0125/recdyna) |
| 公式ページ | なし（メーカーは 2010 年 10 月に自己破産を申請） |
| 有志の資料 | BonDriver_dyud の配布ページ（[Wayback Machine](http://web.archive.org/web/20130527152129/http://dyud200.zxq.net/)）・[まとめ Wiki](https://w.atwiki.jp/dy-ud200/) |

## 2. 準備

1. Zadig で `1c11:1004` に WinUSB を割り当てます。
2. `BonDriver_Dyna.dll`（x64 か x86）を TVTest・EDCB の BonDriver のフォルダーに置きます。
3. 必要なら `BonDriver_Dyna.ini` を置きます（設定の中身は [README](../README.md#設定bondriver_dynaini)）。

## 3. 使い方

- チャンネル空間は `UHF`・`CATV`・`VHF` の 3 つです。
- TVTest・EDCB 側の B-CAS カードリーダーの設定は「なし」にします。
- 付属の `bon_test.exe` で、TVTest を使わずに受信を試せます。

```powershell
# DLL のパス、チャンネル空間（0=UHF）、チャンネルの番号（8 = 21ch）、秒数
bon_test.exe BonDriver_Dyna.dll 0 8 5
```

## 4. 動作確認の記録

2026-11-22 時点で確かめました。  
環境は Windows 7 x64（VM）、Zadig 2.7（`WinUSB v6.1.7600.16385`）、DY-UD200（FW `1.0.5-2`、本体のスロットの B-CAS）です。

| 確かめたこと | 結果 |
|---|---|
| コアの単体テスト（AES のテストベクタ・鍵の切り替え・XOR・MULTI2・Settler・周波数の変換） | 全項目 PASS（Linux の `ctest`・GitHub Actions） |
| `bon_test.exe`（UHF 21ch、5 秒） | `OpenTuner` / `SetChannel` OK、75,585 パケット、`sync_err=0` / `tei=0` / `cc_err=0` |
| TVTest 0.10.0（64bit）+ MSVC でビルドした `BonDriver_Dyna.dll` | チャンネルスキャン・選局・本体の B-CAS での復号・録画した TS の再生がすべて OK。`22.95 dB / 30.42 Mbps`、`D 0 / E 0 / S 0` |

## 5. 注意事項

- 地デジ専用です。BS・CS は受けられません。
- TVTest・EDCB では、MSVC でビルドした DLL を使ってください（MinGW でビルドした DLL では TVTest が落ちます）。

## 6. 技術メモ

機器の中身（USB の命令、初期化の手順、本体の B-CAS スロットの操作）と、Linux での動作確認の記録は、recdyna の [docs/DY-UD200.md](https://github.com/waya0125/recdyna/blob/main/docs/DY-UD200.md) にまとめています。  
BonDriver_Dyna は、24064 バイト（`47×512 = 128×188`）× 16 本の非同期の `OVERLAPPED` I/O で受け、選局直後の同期合わせ（`Aligner`）と乱れの除去（`Settler`）をしてから TS を渡します。
