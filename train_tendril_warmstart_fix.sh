#!/bin/bash

# 🎯 WARM-START SUCCESS CRITERIA FIX - RESEARCHER'S EXACT SPECIFICATION
# Addresses clipfrac=0, approx_kl=0, hits=0 after 500k steps
# Implements warm-start success criteria (8°/0.6s → 5°/1.0s auto-tightening)

echo "🚀 TENDRIL - WARM-START SUCCESS CRITERIA FIX"
echo "================================================================="
echo "🔧 CRITICAL CHANGES IMPLEMENTED:"
echo "   • Warm-start success: 8° + 0.6s hold → 5° + 1.0s (auto-tightening)"
echo "   • Easy episodes: 800 (aligned curriculum + success + posture)"
echo "   • Learning rate: 8e-4 → 1e-3 (stronger updates)"
echo "   • Update epochs: 8 → 6 (balanced learning)"
echo "   • Entropy coef: 0.006 → 0.01 (more exploration)"
echo ""
echo "📊 EXPECTED IMPROVEMENTS IN FIRST 2-3 MINUTES:"
echo "   • clipfrac: 0.000 → 0.05-0.15 (policy updates resume)"
echo "   • approx_kl: 0.000 → 0.003-0.015 (policy divergence)"
echo "   • entropy: 1.681 → >2.5 (exploration revived)"
echo "   • forward_frac: ? → >0.5 (forward pointing in easy phase)"
echo "   • hits: 0 → starts ticking up (bootstrap success)"
echo ""
echo "🚨 FALLBACK IF STILL FLAT AFTER 90s:"
echo "   • Bump LR to 1.5e-3, OR"
echo "   • Increase ent-coef to 0.015, OR"
echo "   • Loosen to 10°/0.5s temporarily"
echo "================================================================="

# Ensure W&B is available
if ! command -v wandb &> /dev/null; then
    echo "📦 Installing wandb..."
    pip install wandb
fi

# Run PufferLib PPO training with RESEARCHER'S EXACT WARM-START PARAMETERS
puffer train puffer_tendril \
  --train.device cpu \
  --wandb \
  --wandb-project "tendril-laser-pop-A-B-log" \
  --wandb-group "phase2-easyhits" \
  --train.total-timesteps 800000 \
  --train.learning-rate 1e-3 \
  --train.clip-coef 0.2 \
  --train.update-epochs 6 \
  --train.batch-size 2048 \
  --train.minibatch-size 512 \
  --train.max-grad-norm 0.5 \
  --train.ent-coef 0.01 \
  --train.vf-coef 1.0 \
  --train.vf-clip-coef 0.2 \
  --train.seed 3 \
  --vec.num-workers 4 \
  --vec.num-envs 16

echo ""
echo "🎉 WARM-START FIX TRAINING COMPLETED!"
echo "📊 Check W&B dashboard for bootstrap success:"
echo "   ✅ clipfrac >0.05 (policy moving again)"
echo "   ✅ hits >0 (early successes achieved)"
echo "   ✅ entropy >2.5 (exploration restored)"
echo "   ✅ forward_frac >0.5 (directional learning)"
echo ""
echo "🔄 Auto-tightening at 800 episodes:"
echo "   • Success criteria: 8°/0.6s → 5°/1.0s"
echo "   • Target generation: easy cone → full workspace"
echo "   • Posture init: helpful → neutral"
