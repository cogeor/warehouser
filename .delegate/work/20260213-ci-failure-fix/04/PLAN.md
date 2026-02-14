# Loop 04: Create TypeScript CI Workflow

## Objective

Create a dedicated GitHub Actions workflow for the web frontend TypeScript/React project that runs independently from the main CI workflow.

## Context

- The web frontend uses Vite + React 18 + TypeScript 5 + Vitest
- Package manager: npm
- Build command: `npm run build` (runs `tsc && vite build`)
- Test command: `npm test` (runs `vitest run`)
- Lint command: `npm run lint` (runs eslint)
- The existing ci.yml does not include web_frontend testing

## Tasks

### Task 1: Create TypeScript CI Workflow

**File:** `.github/workflows/typescript-ci.yaml`

**Requirements:**
- Node.js 20 (LTS)
- Trigger on push/PR to main when web_frontend/** changes
- Cache npm dependencies for faster builds
- Run lint, build, and test steps
- Use consistent naming with existing workflows

**Steps:**
1. Checkout code
2. Setup Node.js 20 with npm cache
3. Install dependencies (npm ci for reproducible builds)
4. Run lint check
5. Run TypeScript type check and build
6. Run Vitest tests

## Verification

- [ ] Workflow file exists at `.github/workflows/typescript-ci.yaml`
- [ ] Valid YAML syntax
- [ ] Triggers on web_frontend/** path changes
- [ ] Uses Node.js 20
- [ ] Runs npm ci, lint, build, and test

## Dependencies

None - this is a standalone workflow file addition.
