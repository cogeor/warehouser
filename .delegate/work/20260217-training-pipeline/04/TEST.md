# Test Results for Loop 04

## Test Execution

### Standalone Training
```bash
python -m training.scripts.train_standalone --timesteps 1000 --eval-episodes 2
```

Output:
```
2026-02-17 01:54:00 - INFO - Training for 1000 timesteps
Using cpu device
... [training iterations] ...
2026-02-17 01:54:03 - INFO - Saved final model to checkpoints/standalone/ppo_standalone_final
2026-02-17 01:54:03 - INFO - Evaluating model for 2 episodes
2026-02-17 01:54:03 - INFO - Episode 1: reward=-133.06, steps=200
2026-02-17 01:54:04 - INFO - Episode 2: reward=-97.69, steps=200
2026-02-17 01:54:04 - INFO - Mean reward: -115.37 (+/- 17.69)
```

### ONNX Export
```bash
python -m training.scripts.export_onnx checkpoints/standalone/ppo_standalone_final.zip \
  --output checkpoints/standalone/model_v1.0.0.onnx \
  --version 1.0.0 --action-dim 4 --obs-dim 5
```

Output:
```
2026-02-17 01:56:17 - INFO - Loading checkpoint: checkpoints/standalone/ppo_standalone_final.zip
2026-02-17 01:56:18 - INFO - Exporting model to ONNX format
2026-02-17 01:56:20 - INFO - Added metadata: version=1.0.0, obs_dim=5, action_dim=4
2026-02-17 01:56:21 - INFO - ONNX model successfully exported and validated
2026-02-17 01:56:21 - INFO - File size: 20616 bytes
```

### Model Verification
```python
import onnx
model = onnx.load('checkpoints/standalone/model_v1.0.0.onnx')
print('Metadata:', [(p.key, p.value) for p in model.metadata_props])
```

Output confirms metadata embedded correctly.

## Code Review

### Changes
- Fixed PolicyWrapper to use `features_extractor`, `mlp_extractor`, `action_net`
- Added `onnxscript` dependency (required by modern torch.onnx)
- Added Windows console encoding workaround

### Dependencies
- onnxscript: Required for torch >= 2.0 ONNX export

## Ready for Commit: yes
