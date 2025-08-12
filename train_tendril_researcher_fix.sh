#!/bin/bash

# 🎯 RESEARCHER'S RECOMMENDED PPO PARAMETERS FOR FLAT REWARD SURFACE FIX
# Based on analysis showing clipfrac=0, approx_kl≈0, and 0 hits
# These parameters are designed to unlock variance and create policy movement

echo "🚀 TENDRIL - RESEARCHER'S FLAT REWARD SURFACE FIX"
echo "================================================================="
echo "🔧 CRITICAL CHANGES FROM RESEARCHER:"
echo "   • Learning rate: 3e-4 → 8e-4 (hotter updates)"
echo "   • Update epochs: 4 → 8 (more learning per batch)"
echo "   • Entropy coef: 0.01 → 0.006 (more exploration)"
echo "   • Batch size: 4096 → 2048 (more gradient noise)"
echo "   • Minibatch: 1024 → 512 (smaller update chunks)"
echo ""
echo "📊 EXPECTED IMPROVEMENTS IN FIRST 60-90s:"
echo "   • clipfrac: 0 → >0.02 (policy actually updating)"
echo "   • approx_kl: ~0 → 0.001-0.008 (policy divergence)"
echo "   • forward_frac: <0.35 → >0.45 (forward pointing)"
echo "   • angular_error: 130° → 110° (better accuracy)"
echo "   • d_perp_mm: 40s → mid-20s (tighter pointing)"
echo "================================================================="

# Ensure W&B is available
if ! command -v wandb &> /dev/null; then
    echo "📦 Installing wandb..."
    pip install wandb
fi

# Run PufferLib PPO training with RESEARCHER'S EXACT PARAMETERS
puffer train puffer_tendril \
  --train.device cpu \
  --wandb \
  --wandb-project "tendril-laser-pop-A-B-log" \
  --wandb-group "cpu-16x4-expLR" \
  --train.total-timesteps 500000 \
  --train.learning-rate 8e-4 \
  --train.clip-coef 0.2 \
  --train.update-epochs 8 \
  --train.batch-size 2048 \
  --train.minibatch-size 512 \
  --train.max-grad-norm 0.5 \
  --train.ent-coef 0.006 \
  --train.vf-coef 1.0 \
  --train.vf-clip-coef 0.2 \
  --train.seed 2 \
  --vec.num-workers 4 \
  --vec.num-envs 16

echo ""
echo "🎉 RESEARCHER'S FIX TRAINING COMPLETED!"
echo "📊 Check W&B dashboard for immediate improvements:"
echo "   ✅ clipfrac >0.02 within 30-60s"
echo "   ✅ forward_frac climbing past 0.35"
echo "   ✅ cosang_mean drifting toward positive"
echo "   ✅ angular_error_mean_deg: 130° → 110°"
echo ""
echo "🚨 If clipfrac stays ~0 after 1 minute:"
echo "   • Bump LR to 1e-3, or"
echo "   • Reduce batch to 1024"
