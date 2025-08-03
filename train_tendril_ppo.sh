#!/bin/bash

# 🎯 HYPERPARAMETER-FIXED TENDRIL PPO TRAINING
# Critical fixes for entropy convergence and explained variance

echo "🚀 TENDRIL LASER POINTER - PPO TRAINING WITH HYPERPARAMETER FIXES"
echo "================================================================="
echo "🔧 CRITICAL FIXES APPLIED:"
echo "   • Entropy coefficient: 0.001 (down from 0.01) - FORCES CONVERGENCE"
echo "   • Learning rate: 0.0001 (down from 0.0003) - STABLE UPDATES"
echo "   • Progress reward: 1.0x (down from 10.0x) - SMOOTH SIGNAL"
echo "   • Success bonus: 10.0 (down from 50.0) - BALANCED REWARD"
echo ""
echo "📊 EXPECTED IMPROVEMENTS:"
echo "   • Entropy: Should decrease from 3.985 → <2.0"
echo "   • Explained Variance: Should improve from -42.728 → >0.5"
echo "   • Policy: Focused pointing instead of random exploration"
echo "================================================================="

# Ensure W&B is available
if ! command -v wandb &> /dev/null; then
    echo "📦 Installing wandb..."
    pip install wandb
fi

# Run PufferLib PPO training with WORKING parameters
puffer train puffer_tendril \
  --wandb \
  --wandb-project "tendril-laser-pointer" \
  --wandb-group "hyperparameter-fixed-ppo" \
  --train.total-timesteps 10000000 \
  --train.device cpu \
  --vec.num-envs 4 \
  --vec.num-workers 1 \
  --train.learning-rate 0.0001 \
  --train.ent-coef 0.001 \
  --train.vf-coef 0.5 \
  --train.clip-coef 0.2 \
  --train.max-grad-norm 0.5 \
  --train.batch-size 4096 \
  --train.minibatch-size 1024 \
  --train.update-epochs 4 \
  --train.gamma 0.99 \
  --train.gae-lambda 0.95 \
  --train.checkpoint-interval 1000000

echo ""
echo "🎉 HYPERPARAMETER-FIXED TRAINING COMPLETED!"
echo "📊 Check W&B dashboard for:"
echo "   ✅ Decreasing entropy curve"
echo "   ✅ Positive explained variance"
echo "   ✅ Stable loss convergence"
echo "   ✅ Focused laser pointing behavior" 