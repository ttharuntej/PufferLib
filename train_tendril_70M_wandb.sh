#!/bin/bash
# 📊 WANDB-MONITORED TENDRIL TRAINING SCRIPT
echo "🚀 Starting Tendril Training with WANDB Monitoring"
echo "=================================================="

# Ensure wandb is installed
pip install wandb

# Login to wandb (run once)
# wandb login

# Run training with full monitoring
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
    --train.checkpoint-interval 5000000 \
    --wandb \
    --wandb-project "tendril-laser-pointer" \
    --wandb-group "70M-hyperparameter-fixed" \
    --tag "expert-level"

echo "🎉 Training with WANDB monitoring completed!"
