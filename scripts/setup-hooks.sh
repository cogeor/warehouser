#!/bin/bash
# Setup script for development environment
# Installs git hooks to prevent CI failures

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"

cd "$ROOT_DIR"

echo -e "${YELLOW}Setting up development environment...${NC}"
echo ""

# Check for pre-commit
if ! command -v pre-commit &> /dev/null; then
    echo -e "${YELLOW}Installing pre-commit...${NC}"
    pip install pre-commit
fi

# Install pre-commit hooks
echo -e "${YELLOW}Installing pre-commit hooks...${NC}"
pre-commit install
pre-commit install --hook-type pre-push

# Make ci-check.sh executable
chmod +x scripts/ci-check.sh 2>/dev/null || true

# Ensure Python dependencies are installed
echo -e "\n${YELLOW}Installing Python dependencies...${NC}"
(cd training && uv sync)

# Ensure TypeScript dependencies are installed
echo -e "\n${YELLOW}Installing TypeScript dependencies...${NC}"
(cd web_frontend && npm ci)

echo ""
echo -e "${GREEN}======================================${NC}"
echo -e "${GREEN}  Development environment ready!${NC}"
echo -e "${GREEN}======================================${NC}"
echo ""
echo "Installed hooks:"
echo "  - pre-commit: Lint, format, type check, and test on commit"
echo "  - pre-push: Full CI validation before push"
echo ""
echo "To run CI checks manually:"
echo "  ./scripts/ci-check.sh"
echo ""
echo "To skip hooks (emergency only):"
echo "  git commit --no-verify"
echo "  git push --no-verify"
