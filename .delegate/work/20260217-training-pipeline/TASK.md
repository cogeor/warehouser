# TASK: Training Pipeline & Model Integration

## Context
Training logic exists but trained model -> simulation integration is unclear. Need to ensure smooth, error-free deployment of trained models.

## Requirements

### Investigation (Pre-Implementation)
1. Review existing training code structure
2. Determine if ONNX Runtime is actually needed for inference
3. Analyze current inference node stub implementation
4. Identify model versioning strategy

### Implementation Goals

1. **Training Logic Verification**
   - Ensure FC (fully-connected) model with RL policy exists
   - Standard PPO/SAC algorithm implementation
   - ONNX export capability

2. **Model Versioning**
   - Models must be versioned (version in filename or metadata)
   - Simulation should be architecture-agnostic (only uses ONNX interface)
   - Model metadata (observation dim, action dim, version) embedded in export

3. **ONNX Export Pipeline**
   - Export trained PyTorch model to ONNX format
   - Include input/output shape validation
   - Version tagging in model file

4. **Simulation Integration**
   - Simulation loads ONNX without knowing architecture
   - Clear error messages for incompatible models
   - Hot-reload capability (optional)

## Best Practices
- Simulation decoupled from training implementation
- Single source of truth for observation/action dimensions
- Model files self-describing via metadata
- Clear validation at load time
