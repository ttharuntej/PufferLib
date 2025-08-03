#!/bin/bash
echo "🚀 TENDRIL 70 MILLION STEP EXPERT TRAINING"
echo "=========================================="
echo "🎯 MASSIVE SCALE: 70M steps (700x more than 100k)"
echo "⏱️  Time: ~59 hours (~2.5 days)"
echo "💾 Checkpoints: Every 5M steps"
echo "🏆 Target: EXPERT-level performance"
echo "=========================================="

puffer train puffer_tendril \
    --train.total-timesteps 70000000 \
    --train.device cpu \
    --vec.num-envs 4 \
    --env.num-envs 4 \
    --train.batch-size 4096 \
    --train.minibatch-size 1024 \
    --train.update-epochs 4 \
    --train.learning-rate 0.0001 \
    --train.ent-coef 0.001 \
    --train.gamma 0.99 \
    --train.gae-lambda 0.95 \
    --train.checkpoint-interval 5000000

echo "🏆 70M step EXPERT training completed!"
echo "🎯 You now have world-class tendril control!"