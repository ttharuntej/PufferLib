# PufferLib Tendril Training Commands

Yep—you can make checkpoints totally explicit so there's no guessing. Do this:

## 1) Train with periodic checkpoints to a known folder

Pick a folder and interval, then run:

```bash
puffer train puffer_tendril \
  --train.device cpu --wandb --wandb-project tendril-prod --tag 3deg-long \
  --train.name 3deg-long-r1 \
  --train.data-dir ./checkpoints \
  --train.checkpoint-interval 100000 \
  --vec.num-workers 4 --vec.num-envs 16 \
  --train.total-timesteps 3000000 \
  --train.batch-size 4096 --train.minibatch-size 512 \
  --train.update-epochs 3 \
  --train.learning-rate 6e-4 --train.anneal-lr True \
  --train.ent-coef 0.015 \
  --train.clip-coef 0.2 --train.max-grad-norm 0.5 \
  --train.vf-coef 1.0 --train.vf-clip-coef 0.2
```

* `--train.data-dir ./checkpoints` → where files go.
* `--train.checkpoint-interval 100000` → saves every 100k steps.
* With your fixed `close()` the trainer will also save one on graceful exit.

## 2) Verify they're being written

While it's running (or after), check:

```bash
find ./checkpoints -name "*.pt" -maxdepth 5
```

You should see something like:

```
./checkpoints/3deg-long-r1/puffer_tendril/step_100000.pt
```

## 3) Resume from a checkpoint

```bash
puffer train puffer_tendril \
  --load-model-path ./checkpoints/3deg-long-r1/puffer_tendril/step_100000.pt \
  --train.device cpu --wandb --wandb-project tendril-prod --tag 3deg-long-resume \
  --train.name 3deg-long-r2 \
  --train.data-dir ./checkpoints \
  --train.checkpoint-interval 100000 \
  ... (same training flags as before)
```

## 4) (Optional) Quick smoke test that saving works

Do a tiny run that guarantees a checkpoint fast:

```bash
puffer train puffer_tendril \
  --train.total-timesteps 60000 \
  --train.checkpoint-interval 20000 \
  --train.data-dir ./checkpoints/smoke \
  --vec.num-envs 4 --vec.num-workers 2 \
  --train.batch-size 512 --train.minibatch-size 256
```

Then `find ./checkpoints/smoke -name "*.pt"` should list `step_20000.pt`, `step_40000.pt`, etc.

## 5) Evaluate a saved model

```bash
puffer eval puffer_tendril \
  --load-model-path ./checkpoints/3deg-long-r1/puffer_tendril/step_100000.pt \
  --render-mode human --max-runs 10
```

If you don't see `.pt` files after adding `--train.data-dir` and `--train.checkpoint-interval`, tell me—either the logger's path builder is different in your fork or an exception is stopping `save_checkpoint()` before it runs.