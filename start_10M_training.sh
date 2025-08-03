#!/bin/bash
echo "🚀 Starting 10 Million Step Tendril Training"
echo "================================================"
echo "📊 Training will take approximately 4-8 hours"
echo "💾 Model saved every 1M steps"
echo "📈 Expected: Much better performance than 100k"
echo "================================================"

puffer train puffer_tendril \
    --train.total-timesteps 10000000 \
    --train.device cpu \
    --vec.num-envs 8 \
    --env.num-envs 8 \
    --train.batch-size 2048 \
    --train.minibatch-size 512 \
    --train.update-epochs 4 \
    --train.learning-rate 0.0003 \
    --train.gamma 0.99 \
    --train.gae-lambda 0.95 \
    --train.checkpoint-interval 1000000

echo "🎉 10M step training completed!"