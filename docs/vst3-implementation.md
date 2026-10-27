# VST3 実装ノート（Pink Tron）

このドキュメントは、実際にビルド・インストールしたプラグイン **Pink Tron** の実装内容をまとめたものです。
モデルの解説は [`pink-trombone-analysis.md`](pink-trombone-analysis.md) を参照してください。

---

## 1. できたもの

| 項目 | 内容 |
| --- | --- |
| 名前 | **Pink Tron** |
| 形式 | VST3（`~/Library/Audio/Plug-Ins/VST3/Pink Tron.vst3`）／ AU（`~/Library/Audio/Plug-Ins/Components/Pink Tron.component`） |
| 種別 | エフェクト（ステレオ in / out、レイテンシ 0） |
| 動作 | **入力音を声帯（glottal source）として声道モデルに通す**。声帯波形（オシレータ）は持たない |
| 追加機能 | 気息ノイズ生成（入力レベルに対する割合 % で加算、声帯波形は含まない） |
| 実装 | JUCE 8.0.15 / C++17 / CMake |

### 信号の流れ

```
input ──┬─► (× Loudness) ──┬──► [Tract 44〜88 セクション導波管] ──► wet ──┐
        │                   │        ▲                                   │
        │            気息ノイズ        │ noiseModulator                    ├─► Mix ─► Output
        │        (Glottis の noise 部分)└─ (摩擦音ノイズもここに注入)          │
        └──────────────────── dry ─────────────────────────────────────┘
```

内部は 1 サンプルにつき `Tract.process()` を **0.5 サンプルずらして 2 回**（＝2× オーバーサンプリング、原典どおり）。

---

## 2. パラメータ対応表

原典の AudioParam をすべて公開しています（VST3 のパラメータ名 = 下記）。

| VST3 パラメータ | 原典 | 範囲 | 既定 | 備考 |
| --- | --- | --- | --- | --- |
| Frequency | `frequency` | 20–2000 Hz | 140 | 気息ノイズの脈動周期（声帯波形は無いのでピッチそのものは出ない） |
| Tenseness | `tenseness` | 0–1 | 0.6 | 声帯の張り。ノイズの通しやすさと声門反射に効く |
| Intensity | `intensity` | 0–1 | 1.0 | 原典では 3 乗で効く係数 |
| Loudness | `loudness` | 0–2 | 1.0 | 原典では有声成分の音量。**エフェクトでは入力のドライブ量**として使う |
| Noise | `noise` | 0–1 | 1.0 | 内蔵ホワイトノイズ源のゲイン（原典はノイズノードを AudioParam に接続） |
| Vibrato Freq | `vibratoFrequency` | 0–20 Hz | 6 | |
| Vibrato Gain | `vibratoGain` | 0–0.1 | 0.005 | |
| Vibrato Wobble | `vibratoWobble` | 0–1 | 1.0 | |
| Tongue Index | `tongueIndex` | 0–88 | 12.9 | 舌の前後位置（声道インデックス） |
| Tongue Size | `tongueDiameter` | 2.05–3.5 | 2.43 | 舌の高さ |
| Tract Length | `tractLength` | 15–88 | 44 | 声道長（セクション数）＝共鳴周波数が動く |
| Constriction n Index | `constrictionNindex` | 2–88 | 30 | 4 スロット |
| Constriction n Diameter | `constrictionNdiameter` | −1–3 | 1.0 | 負値で鼻に抜ける（原典の仕様） |
| Constriction n (bool) | 有効フラグ | on/off | off | 原典の `enableConstriction` に相当 |
| **Aspiration** | （追加） | 0–100 % | 0 | 入力 RMS に対する気息ノイズの割合。100 % で入力と同レベル |
| **Nasal** | （追加） | 0–1 | 0 | `velum.target` の上書き（0 = 原典の自動判定） |
| **Mix** | （追加） | 0–100 % | 100 | dry/wet |
| **Output** | （追加） | −24–+24 dB | −6 | 出力ゲイン（共鳴で持ち上がるぶんの余裕） |

---

## 3. 実装上の判断（原典との差分）

原典に**忠実**であることを優先しつつ、以下だけ意図的に変えています。

| # | 内容 | 理由 |
| --- | --- | --- |
| 1 | 声帯波形（`_getNormalizedWaveform`）を実装していない。`Glottis` は noise 部分のみ | 入力音が声帯の代わり。ノイズの脈動に必要な位相計算だけ残した |
| 2 | `Transient` の振幅を `0.3 * 2^(-200t)` に修正 | 移植版の `Math.pow(-2, ...)` は NaN になり、原典（Neil Thapen 版）は `2^(-200t)`。移植版では事実上無効化されていた破裂音クリックが正しく動く |
| 3 | 閉鎖点の「穴あきスロット」を無視 | 移植版は `constrictions[1]` が穴だと実行時例外になる。C++ では穴をスキップ |
| 4 | `obstructionNew` を毎ブロック −1 にリセット | 移植版はリセットしないため Transient が一度も発火しない |
| 5 | 声道長変更時に舌の**相対位置**を保持 | 原典は `12.9/44*length` に戻してしまうため、Tract Length を動かすと舌が飛ぶ |
| 6 | サンプルレート対応 | 1 セクション = 1 サンプルなので、`length = round(tractLength * sr/44100)`、壁面損失 `0.999^(44100/sr)` に換算。44.1 kHz では原典と完全一致 |
| 7 | 声道形状を最初のブロック前に 1 回適用（プライム） | 起動直後に形状が切り替わることで出る 2.7 倍のクリックを除去 |
| 8 | 連続パラメータに 20 ms のスムーザ | ノブ操作時のジッパーノイズ低減 |
| 9 | 閉鎖点の index を 2 以上に制限（UI 経由） | index 0/1 を完全閉鎖すると 1 サンプルの高 Q 共振器になり大音量になるため |

**検証**: `tests/faithfulness/run.sh` が、原典 JS と C++ 実装に同じ入力を通して 3072 サンプルを比較します。
閉鎖点・舌の自動化・声道長変更を含むシナリオで **最大絶対誤差 5.8e-15 / 相対 1.2e-12**（＝ double の丸め誤差のみ）。

---

## 4. 気息ノイズ（Aspiration）の設計

原典の気息成分は

```
noise = noiseSource
      * noiseModulator              // 声門の開閉で脈動 (0.1〜0.3 + 3*(1-t*i))
      * intensity²                  // 原典どおり 2 乗
      * (1 - sqrt(tenseness))       // 張りが強いと息が漏れない
      * (0.02 * simplex(t*1.99) + 0.2)
      * intensity;                  // 出力段の係数
```

これをそのまま使い、`noiseSource` に「ホワイトノイズ → BPF 500 Hz(Q0.5) と BPF 1000 Hz(Q0.5) の並列和」を入れています（原典の `createNoise()` + 2 つの Biquad と同じ構成）。

加算量は **入力の短時間 RMS に対する割合**にしています。

```
excitation = input * (1 - mixOfNothing) ... ではなく
excitation = input + breath * (aspiration * 34 * inputRms)
```

- `aspiration = 0 %` → 完全に入力のみ（原典の「入力音を励起にする」動作）
- `aspiration = 100 %` → 入力とほぼ同じ RMS の息が乗る
- 入力が無音なら息も出ない（入力に追従するエフェクトなので自然）

---

## 5. ビルドとインストール

```sh
./scripts/install.sh
```

手動で行う場合:

```sh
git clone --depth 1 --branch 8.0.15 https://github.com/juce-framework/JUCE.git libs/JUCE
cmake -B build -DCMAKE_BUILD_TYPE=Release -G Ninja
cmake --build build --target PinkTron_VST3 PinkTron_AU -j8
rsync -a --delete "build/PinkTron_artefacts/Release/VST3/Pink Tron.vst3" ~/Library/Audio/Plug-Ins/VST3/
rsync -a --delete "build/PinkTron_artefacts/Release/AU/Pink Tron.component" ~/Library/Audio/Plug-Ins/Components/
```

### 検証に使うツール

```sh
# 原典との一致（要 Node.js）
./tests/faithfulness/run.sh

# オフラインで音を書き出す（エンジン単体）
./build/pinktron-render /tmp/out

# 実際の VST3 を読み込んで 2 秒レンダリング + NaN/レベル検査
./build/pinktron-host_artefacts/Release/pinktron-host \
    "$HOME/Library/Audio/Plug-Ins/VST3/Pink Tron.vst3" /tmp/out.wav "Aspiration=1.0"

# 全パラメータを毎ブロック乱数で叩くストレステスト（NaN が出ないこと）
./build/pinktron-host_artefacts/Release/pinktron-host \
    "$HOME/Library/Audio/Plug-Ins/VST3/Pink Tron.vst3" "" "stress=1"

# AU の Apple 検証
auval -v aufx Ptpt Jmng

# UI のスクリーンショット
./build/pinktron-ui_artefacts/Release/PinkTronUI.app/Contents/MacOS/PinkTronUI /tmp/editor.png
```

> 注: `pinktron-host` は `stress=1` のようにパラメータを **正規化値 (0–1)** で指定します。

---

## 6. UI

- 上段: 声道の断面をリアルタイム表示（直径配列をオーディオスレッドからロックフリーで受け取る）。
- **声道の絵の中をドラッグすると、その位置・太さに閉鎖点ができる**（原典の操作感）。
- 舌は円で表示。`CON 1〜4` ボタンで有効/無効、`IDX` / `DIA` ノブで自動化可能。
- ネオン（ピンク/シアン）のルック。ノブはすべて `AudioProcessorValueTreeState` に接続。

---

## 7. 既知の注意点

- 声道は強い共鳴器なので、閉鎖点を極端に狭めるとレベルが大きく上がります（最大 +18 dB 程度）。`Output` で調整してください。
- `Frequency` は声帯波形を作らないため「ピッチ」ではなく、気息ノイズの脈動と摩擦音の変調に効きます。
- 入力が無音のときは気息ノイズも出ません（入力追従のエフェクトのため）。
- ステレオは 2 本の独立した Tract で処理します（入力のステレオ感は保持、共鳴は共通設定）。
