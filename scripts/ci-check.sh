#!/bin/bash
# Local CI check script - mirrors what GitHub Actions runs
# Run this before pushing to ensure CI will pass

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"

echo -e "${YELLOW}======================================${NC}"
echo -e "${YELLOW}       Local CI Check${NC}"
echo -e "${YELLOW}======================================${NC}"

cd "$ROOT_DIR"

# Track failures
FAILED=0

# Python checks
echo -e "\n${YELLOW}[1/6] Python: ruff check${NC}"
if (cd training && uv run ruff check .); then
    echo -e "${GREEN}✓ ruff check passed${NC}"
else
    echo -e "${RED}✗ ruff check failed${NC}"
    FAILED=1
fi

echo -e "\n${YELLOW}[2/6] Python: ruff format${NC}"
if (cd training && uv run ruff format --check .); then
    echo -e "${GREEN}✓ ruff format passed${NC}"
else
    echo -e "${RED}✗ ruff format failed${NC}"
    echo -e "${YELLOW}  Run: cd training && uv run ruff format .${NC}"
    FAILED=1
fi

echo -e "\n${YELLOW}[3/6] Python: mypy${NC}"
if (cd training && uv run mypy training tests --ignore-missing-imports); then
    echo -e "${GREEN}✓ mypy passed${NC}"
else
    echo -e "${RED}✗ mypy failed${NC}"
    FAILED=1
fi

echo -e "\n${YELLOW}[4/6] Python: pytest${NC}"
if (cd training && uv run pytest tests/ -v --ignore=tests/integration -q); then
    echo -e "${GREEN}✓ pytest passed${NC}"
else
    echo -e "${RED}✗ pytest failed${NC}"
    FAILED=1
fi

# TypeScript checks
echo -e "\n${YELLOW}[5/6] TypeScript: ESLint${NC}"
if (cd web_frontend && npm run lint); then
    echo -e "${GREEN}✓ ESLint passed${NC}"
else
    echo -e "${RED}✗ ESLint failed${NC}"
    FAILED=1
fi

echo -e "\n${YELLOW}[6/6] TypeScript: tsc + vitest${NC}"
if (cd web_frontend && npx tsc --noEmit && npm test); then
    echo -e "${GREEN}✓ TypeScript checks passed${NC}"
else
    echo -e "${RED}✗ TypeScript checks failed${NC}"
    FAILED=1
fi

echo -e "\n${YELLOW}======================================${NC}"
if [ $FAILED -eq 0 ]; then
    echo -e "${GREEN}All CI checks passed!${NC}"
    exit 0
else
    echo -e "${RED}Some CI checks failed!${NC}"
    echo -e "${RED}Fix issues before pushing.${NC}"
    exit 1
fi
