# pink_tron_effect_vst — Pink Tron

Pink Trombone の声道モデル（Tract / Nose / Transient）を **エフェクト系 VST3** として忠実に移植したプラグインです。
入力音を「声帯の励起信号」として声道フィルタに通すので、**何でもそれっぽく声（母音）に聞こえます**。声帯のオシレータは持たず、代わりに**気息ノイズを割合（%）で足せる**ジェネレータを追加しています。

```
input ─► (drive) ─► Tract (44〜88 セクションの導波管) ─► Mix ─► Output
              ▲                 ▲
        気息ノイズ ──────────────┘   ← Aspiration (%)
```

- 原典: Neil Thapen [Pink Trombone](https://dood.al/pinktrombone/) / 参照実装: [zakaton/Pink-Trombone](https://github.com/zakaton/Pink-Trombone)（GPL-3.0）
- **原典のパラメータをすべて公開**（Frequency, Tenseness, Intensity, Loudness, Noise, Vibrato ×3, Tongue ×2, Tract Length, Constriction ×4）＋ 追加 4 つ（Aspiration, Nasal, Mix, Output）
- 実装は原典 JS と **double 精度で一致**（最大絶対誤差 5.8e-15、`tests/faithfulness/run.sh`）

## インストール済みの場所（macOS）

```
~/Library/Audio/Plug-Ins/VST3/Pink Tron.vst3
~/Library/Audio/Plug-Ins/Components/Pink Tron.component
```

## ビルド

```sh
./scripts/install.sh          # JUCE 取得 → ビルド → ~/Library へコピー
```

## ドキュメント

| ファイル | 内容 |
| --- | --- |
| [`docs/pink-trombone-analysis.md`](docs/pink-trombone-analysis.md) | Pink Trombone の信号処理の全解説（信号の流れ・全パラメータ・移植方法） |
| [`docs/vst3-implementation.md`](docs/vst3-implementation.md) | このプラグインの実装内容、原典との差分、検証手順 |

## 構成

```
Source/
├── dsp/                 JUCE 非依存の DSP コア（そのまま単体テストできる）
│   ├── Tract.cpp/.h     声道（導波管ラティス）      ← Tract.js の移植
│   ├── Nose.h           鼻腔
│   ├── GlottisNoise.h   気息ノイズのみの声帯モデル  ← Glottis.js（声帯波形を除く）
│   ├── SimplexNoise.h   1D Simplex ノイズ
│   ├── TractEngine.h    1 チャンネル分のチェーン（レート換算・スムーザ）
│   └── PinkMath.h       clamp / スムーザ / Biquad / 乱数
├── PluginProcessor.*    パラメータ 28 個、ステレオ、状態保存
├── PluginEditor.*       ネオン UI ＋ 声道ドラッグ操作
└── tests/               （リポジトリ直下）原典比較・オフラインレンダ・VST3 ホスト・UI スナップショット
```

## 検証

```sh
./tests/faithfulness/run.sh                 # 原典 JS と数値比較（要 Node.js）
auval -v aufx Ptpt Jmng                     # AU の Apple 検証
```

ストレステスト（全パラメータを毎ブロック乱数で叩く）でも NaN は出ません。

## ライセンス

Pink Trombone（zakaton 版）が **GPL-3.0** のため、本プラグインも **GPL-3.0** です（`LICENSE`）。
