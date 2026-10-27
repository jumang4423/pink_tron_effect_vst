# pink_tron_effect_vst

Pink Trombone の声道モデルを **エフェクト系 VST / CLAP プラグイン**として再実装するための作業リポジトリです。

- 入力音声を「声帯の励起信号」として声道フィルタに通し、喋っているような共鳴・母音化・摩擦音を与えるエフェクトを目標にしています。
- 仕様の理解と移植の設計は [`docs/pink-trombone-analysis.md`](docs/pink-trombone-analysis.md) にまとめてあります。

## ドキュメント

| ファイル | 内容 |
| --- | --- |
| [`docs/pink-trombone-analysis.md`](docs/pink-trombone-analysis.md) | Pink Trombone の信号処理の全体像、パラメータ一覧、VST への実装方針 |

## 参照実装

`reference/pink-trombone/` に [zakaton/Pink-Trombone](https://github.com/zakaton/Pink-Trombone) を clone してあります（**`.gitignore` 済み / このリポジトリには含まれません**）。

```sh
git clone https://github.com/zakaton/Pink-Trombone.git reference/pink-trombone
```

オリジナルは Neil Thapen 氏の [Pink Trombone](https://dood.al/pinktrombone/)。

## ロードマップ（予定）

1. DSP コア（Glottis / Tract / Nose / Transient / SimplexNoise）の C++ 移植
2. オフライン検証（JS 版と出力を数値比較して同一性を確認）
3. プラグイン化（JUCE 8 想定、VST3 + CLAP、macOS arm64 から）
4. 声道ダイアグラム UI（ドラッグで constriction 追加）
5. プリセット / 母音マクロ（a i u e o）

## ライセンス

Pink Trombone（zakaton 版）は **GPL-3.0** です。コードを移植・参照して頒布する場合、本プラグインも GPL-3.0 互換である必要があります（本リポジトリも GPL-3.0 とします）。
