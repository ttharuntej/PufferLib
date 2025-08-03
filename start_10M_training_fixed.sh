#!/bin/bash
echo "🚀 TENDRIL 10M STEP HYPERPARAMETER-FIXED TRAINING"
echo "================================================="
echo "🎯 CRITICAL FIXES: Entropy coef 0.001, LR 0.0001"
echo "📊 Target: Entropy <2.0, Explained Variance >0.5"
echo "⏱️  Time: ~8-10 hours"
echo "💾 Checkpoints: Every 1M steps"
echo "================================================="

puffer train puffer_tendril \
    --train.total-timesteps 10000000 \
    --train.device cpu \
    --vec.num-envs 4 \
    --vec.num-workers 4 \
    --train.batch-size 4096 \
    --train.minibatch-size 1024 \
    --train.update-epochs 4 \
    --train.learning-rate 0.0001 \
    --train.ent-coef 0.001 \
    --train.gamma 0.99 \
    --train.gae-lambda 0.95 \
    --train.checkpoint-interval 1000000 \
    --wandb \
    --wandb-project "tendril-laser-pointer" \
    --wandb-group "hyperparameter-fixed"

echo "🏆 10M step HYPERPARAMETER-FIXED training completed!"
echo "🎯 Should show: Decreasing entropy + positive explained variance!"