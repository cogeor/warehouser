#!/usr/bin/env bash
# Developer environment setup script
# Run this after cloning the repository

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"

cd "$ROOT_DIR"

echo "Setting up developer environment..."

# Install pre-commit hooks
if command -v pre-commit &> /dev/null; then
    echo "Installing pre-commit hooks..."
    pre-commit install
    pre-commit install --hook-type pre-push
    echo "Pre-commit hooks installed successfully!"
else
    echo "WARNING: pre-commit not found. Install with: pip install pre-commit"
    echo "Then run: pre-commit install && pre-commit install --hook-type pre-push"
fi

# Set up Python training environment
if [ -d "training" ]; then
    echo ""
    echo "Setting up Python training environment..."
    cd training
    if command -v uv &> /dev/null; then
        uv sync
        echo "Python environment ready!"
    else
        echo "WARNING: uv not found. Install with: pip install uv"
    fi
    cd ..
fi

# Set up TypeScript frontend
if [ -d "web_frontend" ]; then
    echo ""
    echo "Setting up TypeScript frontend..."
    cd web_frontend
    if command -v npm &> /dev/null; then
        npm install
        echo "Frontend dependencies installed!"
    else
        echo "WARNING: npm not found. Install Node.js first."
    fi
    cd ..
fi

echo ""
echo "Development environment setup complete!"
echo ""
echo "Before committing, run:"
echo "  cd training && uv run ruff check . --fix && uv run ruff format ."
echo ""
echo "Or run the full CI check:"
echo "  bash scripts/ci-check.sh"
