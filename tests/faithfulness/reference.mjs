// Faithfulness check: renders a deterministic scenario through the ORIGINAL
// Pink Trombone JS (reference/pink-trombone) and prints one sample per line.
//
//   node tests/faithfulness/reference.mjs > /tmp/ref.txt
//   (see tests/faithfulness/run.sh)
import Processor from "../../reference/pink-trombone/script/audio/nodes/pinkTrombone/processors/Processor.js";

const sr = 44100;
const blocks = 24;
const bufferLength = 128;

const processor = new Processor();

let seed = 12345;
const rnd = () => {
  seed = (seed * 16807) % 2147483647;   // Lehmer, exact in doubles
  return (seed / 2147483647) * 2 - 1;
};

const input = new Float64Array(bufferLength);
for (let i = 0; i < bufferLength; i++) input[i] = rnd() * 0.3;

const out = [];
let seconds = 0;

for (let block = 0; block < blocks; block++) {
  const constrictions = [];
  if (block >= 4) {
    constrictions[0] = {
      index: 20 + block * 0.5,
      diameter: Math.max(0.45, 1.4 - block * 0.1),
    };
  }
  if (block >= 12) {
    constrictions[1] = { index: 30, diameter: 2.5 };
  }

  const tractLength = block < 8 ? 44 : block < 16 ? 30 : 60;

  for (let i = 0; i < bufferLength; i++) {
    const params = {
      tractLength,
      tongueIndex: 12.9 + block * 0.4,
      tongueDiameter: 2.43 + 0.03 * block,
      noiseModulator: 1.3,
    };

    out.push(processor.process(input, params, i, bufferLength, seconds));
    seconds += 1 / sr;
  }

  processor.update(seconds, constrictions);
}

for (const v of out) console.log(v.toPrecision(17));
