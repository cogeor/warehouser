# Implementation: Standalone Training Verification

## Task 4: Verify standalone training and ONNX export

### Files Modified

1. **`training/training/scripts/export_onnx.py`**
   - Fixed PolicyWrapper to use correct SB3 policy structure
   - Added Windows console encoding workaround for Unicode
   - Changed from `policy.actor.get_action_dist_params` to proper SB3 API:
     - `features_extractor` -> `mlp_extractor` -> `action_net`

### Changes Made

#### PolicyWrapper Fix
```python
# Old (broken):
def forward(self, obs: torch.Tensor) -> Any:
    return self.policy.actor.get_action_dist_params(obs)[0]

# New (working):
def __init__(self, policy: torch.nn.Module) -> None:
    super().__init__()
    self.features_extractor = policy.features_extractor
    self.mlp_extractor = policy.mlp_extractor
    self.action_net = policy.action_net

def forward(self, obs: torch.Tensor) -> Any:
    features = self.features_extractor(obs)
    latent_pi, _ = self.mlp_extractor(features)
    return self.action_net(latent_pi)
```

### Verification

1. **Standalone Training**
   ```bash
   cd training
   python -m training.scripts.train_standalone --timesteps 1000
   ```
   - Successfully trains PPO model without ROS dependencies
   - Creates checkpoints at specified intervals
   - Evaluates model after training

2. **ONNX Export**
   ```bash
   python -m training.scripts.export_onnx checkpoints/standalone/ppo_standalone_final.zip \
     --output checkpoints/standalone/model_v1.0.0.onnx \
     --version 1.0.0 --action-dim 4 --obs-dim 5
   ```
   - Exports model with metadata embedded
   - Validates output file with ONNX checker

3. **Metadata Verification**
   ```
   Metadata:
     model_version: 1.0.0
     obs_dim: 5
     action_dim: 4
     export_timestamp: 2026-02-17T00:56:20.996486+00:00
   ```

### Notes

- Standalone env uses obs_dim=5 (simpler than full obs_dim=8)
- Export now uses opset 18 (auto-upgraded from 17)
- onnxscript package required for modern torch.onnx.export
