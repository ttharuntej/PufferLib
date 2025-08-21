#!/bin/bash
# Run puffer training from the correct directory to use local binding

cd "$(dirname "$0")"
echo "🎯 Running Tendril training from: $(pwd)"
echo "📦 Using binding: $(ls -la binding*.so)"

puffer train puffer_tendril \
  --train.device cpu \
  --tag abs-angles-slew-2deg \
  --train.name abs-2deg-r9e-boost \
  --train.checkpoint-interval 100000 \
  --vec.num-workers 4 --vec.num-envs 16 \
  --train.bptt-horizon 64 \
  --train.total-timesteps 7000000 \
  --train.batch-size 2048 --train.minibatch-size 256 \
  --train.update-epochs 3 \
  --train.learning-rate 5e-4 --train.anneal-lr True \
  --train.ent-coef 0.03 \
  --train.clip-coef 0.10 \
  --train.max-grad-norm 0.5 \
  --train.vf-coef 0.5 --train.vf-clip-coef 0.2 \
  --train.gamma 0.99 --train.gae-lambda 0.95