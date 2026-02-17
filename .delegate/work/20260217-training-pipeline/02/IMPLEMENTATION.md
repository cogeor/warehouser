# Implementation: Model Versioning for ONNX Export

## Task: Add model versioning to ONNX export script

Completed: 2026-02-17

### Changes

- `training/training/scripts/export_onnx.py`: Added comprehensive model versioning support

#### New Features

1. **--version argument** (default "1.0.0")
   - Accepts semantic version format (X.Y.Z)
   - Validated using regex pattern before export

2. **Embedded ONNX metadata**
   - `model_version`: The version string from --version argument
   - `obs_dim`: Observation dimension (as string)
   - `action_dim`: Action dimension (as string)
   - `export_timestamp`: ISO8601 timestamp (UTC)

3. **Versioned output filename**
   - Default: `policy_v{version}.onnx` (e.g., `policy_v1.0.0.onnx`)
   - Can be overridden with --output argument

4. **VecNormalize stats export**
   - Automatically copies `{model}_vecnormalize.pkl` alongside ONNX
   - Output: `policy_v{version}.vecnormalize.pkl`
   - Gracefully handles missing stats files

5. **New --action-dim argument**
   - Default: 4 (matching warehouse robot action space)
   - Embedded in ONNX metadata

#### New Functions

- `validate_version(version: str) -> bool`: Validates semantic version format
- `add_metadata_to_model(model_path, version, obs_dim, action_dim)`: Adds metadata to ONNX
- `export_vecnormalize_stats(checkpoint_path, output_path) -> str | None`: Exports VecNormalize stats

### Verification

- [x] Syntax check: `python -m py_compile` passes
- [x] Import check: Functions can be imported successfully
- [x] Version validation: Tests pass for valid/invalid versions
- [x] CLI help: Shows all new arguments correctly
- [x] Ruff linting: All checks pass

### Usage Examples

```bash
# Basic export with default version
python -m training.scripts.export_onnx checkpoints/model.zip

# Export with specific version
python -m training.scripts.export_onnx checkpoints/model.zip --version 2.0.0

# Export with all options
python -m training.scripts.export_onnx checkpoints/model.zip \
    --version 1.2.3 \
    --obs-dim 16 \
    --action-dim 4 \
    --output custom_policy.onnx
```

### Reading Metadata from ONNX

```python
import onnx

model = onnx.load("policy_v1.0.0.onnx")
metadata = {prop.key: prop.value for prop in model.metadata_props}
print(metadata)
# {'model_version': '1.0.0', 'obs_dim': '8', 'action_dim': '4',
#  'export_timestamp': '2026-02-17T12:00:00.000000+00:00'}
```

### Notes

- Maintained backward compatibility with existing API
- Used `datetime.UTC` instead of `timezone.utc` per ruff UP017 rule
- Imports sorted per ruff I001 rule
- VecNormalize stats export follows train.py naming convention

---
