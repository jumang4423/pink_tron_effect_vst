# Pink Trombone 解析ノート（日本語）

対象: `reference/pink-trombone/` = [zakaton/Pink-Trombone](https://github.com/zakaton/Pink-Trombone)（AudioWorklet 移植版、GPL-3.0）
原典: Neil Thapen "Pink Trombone" <https://dood.al/pinktrombone/>

---

## 0. 結論：信号の流れは「oscillator → effect → out」か？

**ほぼその理解で正しいですが、正確には「音源（声帯）→ 声道フィルタ → 放射（唇）」の 3 段です。**

```
[Glottis]            有声音（声帯波形）＋ 気息ノイズ     ← ここが oscillator
    │ 1 sample
    ▼
[Tract]              直径の異なる管を 44〜88 個並べた
                     デジタル導波路（散乱接合）
    │                ← ここが effect（レゾネータ／フィルタ）
    ▼
[唇 -0.85 反射]      放射特性 + 末端出力
    │
    ▼
 output
```

- **Glottis** = 音源。`Rosenberg` 型の声帯波形（のこぎり波に近いパルス）＋ SimplexNoise による気息成分。
- **Tract** = 声道。ケリー＝ロックバウム（Kelly–Lochbaum）型の**導波管ラティス**。管を円筒セクションに分け、各セクション間で「反射」を計算して音を共鳴させる。ここがフォルマント（母音の音色）を作る本体であり、実質「フィルタ（エフェクト）」です。
- **唇 / 鼻** = 終端。唇は `-0.85` の反射係数で「ほぼ開放端」を表現し、そこから外に音が出ます。鼻は軟口蓋（velum）で分岐する副経路。

つまり「発振器 → フィルタ → 出力」という直列構造で、フィルタが**時間可変の物理モデル**である点が普通の EQ と違います。

> **重要**: `Processor.process()` は `inputSamples` が渡された場合、**内蔵 Glottis を使わずに入力信号を声帯サンプルとして使います**（`Processors/Processor.js` 18–23 行）。これが「Pink Trombone をエフェクトとして使う」ための公式の入口です（後述 §6）。

---

## 1. ファイル構成

```
script/
├── PinkTrombone.js                       … AudioContext 拡張・グラフ構築の入口
├── component.js                          … <pink-trombone> カスタム要素
├── audio/nodes/
│   ├── pinkTrombone/AudioNode.js         … AudioWorkletNode ラッパ（パラメータ公開）
│   ├── pinkTrombone/processors/
│   │   ├── WorkletProcessor.js           … AudioWorkletProcessor（ブロック処理の外側）
│   │   ├── Processor.js                  … Glottis + Tract の合成（★エフェクト化の分岐点）
│   │   ├── Glottis.js                    … 声帯（音源）
│   │   ├── Tract.js                      … 声道（導波管ラティス）★中心
│   │   ├── Nose.js                       … 鼻腔
│   │   ├── Transient.js                  … 破裂音のインパルス
│   │   ├── SimplexNoise.js               … 1D Simplex ノイズ
│   │   └── ParameterDescriptors.js       … AudioParam 定義（＝API 表面）
│   ├── noise/AudioNode.js                … ノイズ源ノード
│   └── constantSource/AudioNode.js       … ScriptProcessor フォールバック用
└── graphics/                             … TractUI / GlottisUI / ButtonsUI
```

---

## 2. 信号処理の詳細

### 2.1 ブロック処理の外側（`WorkletProcessor.js`）

`process(inputs, outputs, parameters)` は **1 サンプルずつ** 以下を回します（89–108 行）。

```js
for (sampleIndex ...) {
  const parameterSamples = this._getParameterSamples(parameters, sampleIndex); // a-rate 対応
  const seconds = currentTime + sampleIndex / sampleRate;
  outputs[..][sampleIndex] = this.processor.process(
      inputs[..][..], parameterSamples, sampleIndex, bufferLength, seconds);
}
this.processor.update(currentTime + blockLength / sampleRate, constrictions); // ブロック末尾で形状更新
```

ポイント:
- **a-rate パラメータはサンプル毎**、`length === 1` なら k-rate としてブロック定数扱い。
- **声道形状（diameter/reflection）はブロック単位で更新**（`update()`）。形状そのものは音声レートで動かさない設計。
- `constrictions`（閉鎖点）は `_getConstrictions()` で `[0]` サンプルだけ読む＝**k-rate**。

### 2.2 `Processor.js`（30 行しかないが最重要）

```js
process(inputSamples, parameterSamples, sampleIndex, bufferLength, seconds) {
    if (inputSamples) {
        parameterSamples.glottis = inputSamples[sampleIndex];   // ★外部入力を励起にする
    } else {
        parameterSamples.glottis = this.glottis.process(...arguments); // 内蔵声帯
    }
    let out = this.tract.process(...arguments);
    sampleIndex += 0.5;                                          // 2 倍オーバーサンプリング
    out += this.tract.process(inputSamples, parameterSamples, sampleIndex, bufferLength, seconds);
    if (!inputSamples) out *= 0.125;                             // 音量補正
    return out;
}
```

- `Tract.process` を **0.5 サンプルずらして 2 回**呼び、加算 ＝ 実質 2× オーバーサンプリング（エイリアシング低減）。
- 内蔵 Glottis 使用時のみ `*0.125` でノーマライズ。
- → **エフェクト版では `inputSamples` を必ず渡す**ので、`*0.125` は掛からない。出力ゲインは自前で管理する。

### 2.3 `Glottis.js`（音源＝オシレータ）

1 サンプルごとの処理（31–89 行）:

```js
vibrato  = vibratoGain * sin(2π * seconds * vibratoFrequency)
         + 0.02 * noise(seconds*4.07)        // 揺らぎ
         + 0.04 * noise(seconds*2.15)
         + wobble(0.2*n(0.98t) + 0.4*n(0.5t)) * vibratoWobble

frequency *= 1 + vibrato
tenseness += 0.1*n(0.46t) + 0.05*n(0.36t) + (3 - tenseness)*(1 - intensity)  // 声の張り

period      = 1 / frequency
interpolation = (seconds - startSeconds) / period
if (interpolation >= 1) { startSeconds を進める; _updateCoefficients(tenseness); }

noiseModulator = _getNoiseModulator(interpolation) + (1 - tenseness*intensity)*3
noise  = noise * noiseModulator * intensity² * (1 - sqrt(max(tenseness,0)))
              * (0.02*n(1.99t) + 0.2)
voice  = _getNormalizedWaveform(interpolation) * intensity * loudness
return (noise + voice) * intensity
```

- `_updateCoefficients(tenseness)` は **Rosenberg パラメータ**を毎周期計算（93–126 行）:
  `R.d = clamp(3*(1-tenseness), 0.5, 2.7)` → `R.a, R.k, R.g` → `T.p, T.e` →
  `epsilon, shift, Delta, omega, alpha, E0, Te`。
- `_getNormalizedWaveform(x)` は 2 区間の区分関数:
  - `x <= Te`: 開大期 `E0 * exp(alpha*x) * sin(omega*x)`
  - `x > Te` : 閉鎖期 `(-exp(-epsilon*(x-Te)) + shift) / Delta`
- `noiseModulator` は `max(0, sin(2πx)) * 0.2 + 0.1`（声帯が開いている間だけ息が漏れる）。

→ **VST 実装時、内蔵声帯をそのまま使うならこの 130 行を C++ に写すだけ**。位相は「サンプル数 / サンプルレート」の秒で管理しているので、移植も容易です。

### 2.4 `Tract.js`（声道＝フィルタ本体、517 行）

#### (a) 形状 → 反射係数

面積は直径の 2 乗（`A = d²`）。隣接セクション間の反射係数は

```
reflection[i] = (A[i-1] - A[i]) / (A[i-1] + A[i])          // 480–502 行
```

管の断面が変わるところで反射が起き、これがフォルマント（共鳴）になります。

#### (b) 1 サンプルの処理（`_processLips` / `_processNose`, 245–317 行）

```js
// 声門端（反射 0.75）＋ 励起
right.junction[0] = left[0] * 0.75 + glottisSample;

// 唇端（反射 -0.85 = ほぼ開放）
left.junction[length] = right[length-1] * (-0.85);

// 内部：散乱接合
for (i = 1 ... length-1) {
  r = interpolate(bufferInterpolation, reflection[i], reflection.new[i]);  // 形状の補間
  offset = r * (right[i-1] + left[i]);
  right.junction[i] = right[i-1] - offset;
  left.junction[i]  = left[i]   + offset;
}

// 鼻腔 3 ポート散乱（nose.start で分岐）
... nose.left.junction[0] / nose.right.junction[0] ...

// 減衰 & 出力
for (i = 0 ... length-1) {
  right[i] = right.junction[i] * 0.999;
  left[i]  = left.junction[i+1] * 0.999;
}
return right[length-1];       // 唇から出る波
```

- 各セクションは **1 サンプル遅延**。`length = 44` @44.1 kHz なら往復 44 サンプル ≈ 1 ms ≒ 音速 350 m/s で長さ 17.5 cm 相当。
- `0.999` が壁面損失（ダンピング）。**全体が受動素子なので発散しません**（`isNaN` チェックだけ入っている: 198 行）。
- 形状の変化は `reflection` と `reflection.new` の 2 本を持ち、ブロック内のサンプル位置で**線形補間**して滑らかに切り替える（クリック防止）。

#### (c) 形状の決定（舌と閉鎖点）

1. `_updateDiameterRest()`（333–350 行）: 舌（`tongue.index`, `tongue.diameter`）から `blade.start`〜`lip.start` の基本形状を cos カーブで作る。
2. `_updateConstrictions(constrictions)`（366–461 行）: 閉鎖点（最大 4 個）ごとに、`diameter - 0.3` を下限として半径方向に cos 窓で削る。`velum.target` を `0.4` にして鼻に抜けさせる条件もここ。
3. `_updateReflection()` で反射係数へ。
4. **更新は「前ブロックと同じならスキップ」**する最適化あり（366–384 行）。

### 2.5 `Nose.js`（鼻）

- 声道長の 28/44 の長さ。直径は 0.4→2.0→0.5 の山なり（最大 1.9 にクランプ）。
- `diameter[0]` が `velum.target`（0.01 = 閉じている / 0.4 = 開いている）＝軟口蓋。
- 声道の `nose.start = length - noseLength + 1` の位置で 3 ポート散乱接合。

### 2.6 `Transient.js`（破裂音）

閉鎖が**解けた瞬間**に声道内へインパルスを注入（`lifetime` 0.2 s、`strength` 0.3、`exp` 200 で急減衰）。
`_updateTransients` は `nose.amplitude[0] < 0.05`（＝鼻が閉じている）ときだけ作動（471–478 行）。

---

## 3. パラメータ一覧（`ParameterDescriptors.js`）

### 3.1 AudioParam（＝プラグインで自動化可能にすべきもの）

| 名前 | 既定値 | 範囲 | 意味 | 実装メモ（VST） |
| --- | --- | --- | --- | --- |
| `frequency` | 140 | 0 … (無制限) | 声帯振動数 = ピッチ (Hz) | a-rate。`period = 1/f`、位相は秒ベース。0 で発散するので `>= 1` にクランプするか LFO 扱いを禁止 |
| `tenseness` | 0.6 | 0–1 | 声帯の張り。0 = 息だけ（無声）、1 = 硬い（声門破裂的） | Rosenberg 係数を毎周期再計算。**ブロック内で急変させず平滑化**推奨 |
| `intensity` | 1 | 0–1 | 声の強さ。`voice`/`noise` 両方に二乗で効く | |
| `loudness` | 1 | 0–1 | 有声成分のみの音量 | `tenseness` とセット運用（下記 §5） |
| `noise` | 0 | −1–1 | 気息ノイズ量。負値も許容（位相反転） | 外部ノイズ源を差し込む設計。逆解析では `createNoise()` の BandPass(500Hz/Q0.5) と BandPass(1000Hz/Q0.5) を並列で `noise` に接続している（`PinkTrombone.js` 33–51 行） |
| `tongueIndex` | 12.9 | （内部 clamp） | 舌の前後位置（声道インデックス） | k-rate で良い。実範囲は `blade.start+2` 〜 `tip.start-3` |
| `tongueDiameter` | 2.43 | 2.05–3.5（内部 clamp） | 舌の高さ＝狭窄の深さ | k-rate。`diameter` が小さくなるほど「狭い母音」 |
| `vibratoFrequency` | 6 | 0 … | ビブラート Hz | |
| `vibratoGain` | 0.005 | 0 … | ビブラート深さ（相対値、0.005 = 0.5%） | |
| `vibratoWobble` | 1 | 0–1 | ランダムな揺らぎ（人間らしさ） | |
| `tractLength` | 44 | 15–88 | 声道長（セクション数） | **変更すると内部配列の再計算**が走る（`_updateTractLength`）。急変でクリック・ノイズ。声道長 15–88 で formant が大きく動く＝「声道の伸縮」エフェクト |

### 3.2 閉鎖点パラメータ（4 スロット、`numberOfConstrictions = 4`）

| 名前 | 既定値 | 意味 |
| --- | --- | --- |
| `constriction{n}index` | 0 | 声道上の位置（0 … `tractLength`） |
| `constriction{n}diameter` | 0 | その位置の直径。負値 = 閉鎖方向、`< -0.85 - nose.offset` で無効化 |

- 有効化は `enableConstriction` / `disableConstriction` を `port.postMessage` で送る（`AudioNode.js` 119–135 行）。つまり**AudioParam だけでは閉鎖点は作れない**（有効フラグが別）。
- マウスドラッグで `newConstriction(index, diameter)` を呼ぶのが UI 側の流れ（`TractUI.js` 569 行付近）。

### 3.3 内部定数（AudioParam ではない。UI からは触れない）

| 場所 | 値 | 意味 |
| --- | --- | --- |
| `Tract.glottis.reflection` | 0.75 | 声門端の反射（声帯の下側のインピーダンス） |
| `Tract.lip.reflection` | −0.85 | 唇の反射（ほぼ開放端、少し放射損失） |
| `Tract.velum.target` | 0.01 / 0.4 | 軟口蓋の開閉（鼻音） |
| `Nose.offset` | 0.8 | 鼻との結合判定に使うオフセット |
| `Nose.fade` | 1.0 | 鼻腔の減衰 |
| `Tract.grid.offset` | 1.7 | 舌カーブの形状パラメータ |
| 壁面損失 | 0.999 / sample | 声道のダンピング。**小さいほど早く減衰**（`Q` を下げる） |
| 声道形状の境界 | `blade.start=10/44`, `tip.start=32/44`, `lip.start=39/44`, 舌初期 `12.9/44` | 口腔の解剖学的な区切り |

---

## 4. 母音の作り方（UI 側の対応）

- 舌の位置（`tongueIndex` × `tongueDiameter`）だけで **a / e / i / o / u の大部分**が作れます（`README.md` の "Common Phonemes" 参照）。
- 子音は「閉鎖点（constriction）を一瞬作って離す」＝破裂音、「狭い閉鎖点＋ノイズ」＝摩擦音。
- 有声/無声の切り替えは `tenseness` と `loudness` を同時に動かす公式の式があります（`README.md` / `index.html`）:

```js
tenseness = 1 - Math.cos(voiceness * Math.PI * 0.5);
loudness  = Math.pow(tenseness, 0.25);
```

---

## 5. エフェクト（VST）として実装するには

### 5.1 3 つの設計方針

| 方式 | 入力の扱い | 特徴 | 難易度 |
| --- | --- | --- | --- |
| **A. 励起として直接注入（推奨・本命）** | 入力サンプルをそのまま `glottis` サンプルにする | `Processor.js` の `inputSamples` 分岐そのもの。入力の音色が声道共鳴で「喋る」。ドラムやギターに掛けると面白い | 低 |
| B. リ・シンセシス | 入力のピッチ・音量を解析して内蔵 Glottis を駆動 | 入力の音程で喋る／歌う。ピッチ検出が必要 | 中 |
| C. クロス／ボコーダ | 入力のエンベロープで声帯を変調 | 入力のリズムで喋る | 中 |

### 5.2 実装手順（JUCE 8 / VST3 + CLAP 想定）

1. **DSP をほぼそのまま C++ 移植**
   - `Glottis` / `Tract` / `Nose` / `Transient` / `SimplexNoise` を 1:1 で移植。
   - `Float64Array` は `std::vector<double>`、`.junction` のような配列プロパティは別メンバに分ける。
   - JS の `Math.pow` / `Math.exp` / `Math.sin` は `std::pow/exp/sin` と同じ。**`clamp` の境界（`<=`, `<`）まで一致させる**と数値比較が楽。
2. **状態をすべてメンバに持つ**（グローバル禁止）。`seconds` は「プラグイン起動からの累積サンプル数 / sampleRate」で自前管理（`currentTime` は使えない）。
3. **レート変換**: 内部は `sampleRate` 前提の係数（`0.999` の減衰、`length` のサンプル数）なので、
   - `tractLength` を「サンプル数」ではなく「長さ比」として扱い、`length = round(44 * (44100/sampleRate) * ratio)` のようにスケールする
   - 減衰 `0.999` も `pow(0.999, 44100/sampleRate)` に置き換える
   のが無難です（48 kHz / 96 kHz で音が変わるのを防ぐ）。
4. **2× オーバーサンプリング**: `Processor` の「0.5 サンプルずらして 2 回」を再現するか、`juce::dsp::Oversampling` で括る。前者の方が原典と一致します。
5. **パラメータ平滑化**: `frequency` / `tenseness` / `tongue*` / `tractLength` は 20–50 ms のスムーザを通す。`tractLength` は形状再計算を伴うので、**変更を検出したときだけ**再計算（原典もそうしている）。
6. **UI**: 声道の断面図を描き、ドラッグ位置 → `index`, `diameter` に変換して constriction を 1 つ追加 → 離したら削除。`TractUI.js` の座標変換を流用できます。
7. **デノーマル対策**: 減衰ループで値が 0 に落ちるので `juce::ScopedNoDenormals` を必ず入れる。
8. **ステレオ**: 原典はモノラル。ステレオは「2 本の Tract を別パラメータで持つ」か「モノラル処理 → 出力のみ広げる」。
9. **プリセット**: 母音 a/i/u/e/o を `tongueIndex` × `tongueDiameter` のテーブルで定義。

### 5.3 移植時の注意点

- **`_updateConstrictions` の「前回と同じならスキップ」**を忘れると、毎ブロック形状を作り直して重くなります。
- `reflection` / `reflection.new` の**2 本持ち + ブロック内補間**はクリック対策の要。省略するとパラメータを動かしたときにザラつきます。
- `velocity` 系の処理はありません。**Tract は受動回路なので安定**ですが、`reflection` の値域（−1…1）を壊すと発散します。`isNaN` チェック（198 行）に相当する `std::isfinite` ガードを入れてください。
- ライセンス: 原典は **GPL-3.0**。コードを流用して頒布するなら本プラグインも GPL-3.0 にする必要があります。

---

## 6. 参照（一次情報）

- 原典: <https://dood.al/pinktrombone/>（Neil Thapen, 2017）
- 移植版: <https://github.com/zakaton/Pink-Trombone>（GPL-3.0）
- 理論背景: Kelly–Lochbaum 声道モデル / Rosenberg 声帯波形 / Klatt 合成器
