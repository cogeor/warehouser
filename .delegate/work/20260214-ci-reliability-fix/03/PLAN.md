# Loop 03: Add TypeScript linting to pre-commit hooks

## Overview

Enhance `.pre-commit-config.yaml` to run ESLint and TypeScript compiler checks on frontend code before commits. This catches TypeScript issues locally before they reach CI, matching the existing Python workflow that uses ruff and mypy pre-commit hooks.

## Prerequisites

- Loop 01 must be complete (ESLint configuration exists at `web_frontend/.eslintrc.cjs`)

## Tasks

### Task 1: Add ESLint pre-commit hook

**Goal:** Add a local pre-commit hook that runs ESLint on TypeScript files in `web_frontend/`

**Files:**
| Action | Path |
|--------|------|
| MODIFY | `.pre-commit-config.yaml` |

**Steps:**
1. Add a new repo entry using `repo: local` (preferred over mirrors-eslint since ESLint config is in subdirectory)
2. Define hook with id `eslint-frontend`
3. Set `language: system` to use the project's installed ESLint
4. Use `entry: npm run lint --prefix web_frontend`
5. Set `files: ^web_frontend/src/.*\.(ts|tsx)$` to match TypeScript files only
6. Set `pass_filenames: false` since `npm run lint` handles file discovery

**Hook configuration:**
```yaml
- repo: local
  hooks:
    - id: eslint-frontend
      name: eslint (web_frontend)
      entry: npm run lint --prefix web_frontend
      language: system
      files: ^web_frontend/src/.*\.(ts|tsx)$
      pass_filenames: false
```

**Verify:** `pre-commit run eslint-frontend --all-files`

### Task 2: Add TypeScript compiler pre-commit hook

**Goal:** Add a local pre-commit hook that runs `tsc --noEmit` to catch type errors

**Files:**
| Action | Path |
|--------|------|
| MODIFY | `.pre-commit-config.yaml` |

**Steps:**
1. Add another hook entry under the same `repo: local` section
2. Define hook with id `tsc-frontend`
3. Set `language: system` to use the project's installed TypeScript
4. Use `entry: npx --prefix web_frontend tsc --noEmit --project web_frontend/tsconfig.json`
5. Set `files: ^web_frontend/src/.*\.(ts|tsx)$` to trigger only on TypeScript changes
6. Set `pass_filenames: false` since tsc checks the whole project

**Hook configuration:**
```yaml
    - id: tsc-frontend
      name: tsc --noEmit (web_frontend)
      entry: npx --prefix web_frontend tsc --noEmit --project web_frontend/tsconfig.json
      language: system
      files: ^web_frontend/src/.*\.(ts|tsx)$
      pass_filenames: false
```

**Verify:** `pre-commit run tsc-frontend --all-files`

### Task 3: Verify complete pre-commit configuration

**Goal:** Ensure all hooks work together and the configuration is valid

**Files:**
| Action | Path |
|--------|------|
| VERIFY | `.pre-commit-config.yaml` |

**Steps:**
1. Run `pre-commit run --all-files` to test all hooks (Python and TypeScript)
2. Verify no errors in hook execution
3. Confirm ESLint catches lint issues (test with intentional violation if needed)
4. Confirm tsc catches type errors (test with intentional violation if needed)

**Verify:** `pre-commit run --all-files` exits 0 on clean codebase

## Final Configuration

After completion, `.pre-commit-config.yaml` should look like:

```yaml
repos:
  - repo: https://github.com/astral-sh/ruff-pre-commit
    rev: v0.8.4
    hooks:
      - id: ruff
        args: [--fix]
      - id: ruff-format
  - repo: https://github.com/pre-commit/mirrors-mypy
    rev: v1.14.0
    hooks:
      - id: mypy
        args: [--ignore-missing-imports]
        additional_dependencies: [pydantic, pytest, numpy, gymnasium]
        files: ^training/
  - repo: local
    hooks:
      - id: eslint-frontend
        name: eslint (web_frontend)
        entry: npm run lint --prefix web_frontend
        language: system
        files: ^web_frontend/src/.*\.(ts|tsx)$
        pass_filenames: false
      - id: tsc-frontend
        name: tsc --noEmit (web_frontend)
        entry: npx --prefix web_frontend tsc --noEmit --project web_frontend/tsconfig.json
        language: system
        files: ^web_frontend/src/.*\.(ts|tsx)$
        pass_filenames: false
```

## Acceptance Criteria

- [ ] ESLint hook runs on TypeScript file changes in `web_frontend/src/`
- [ ] TypeScript compiler hook runs on TypeScript file changes in `web_frontend/src/`
- [ ] Both hooks use `language: system` (no additional dependencies needed)
- [ ] Hooks only trigger when relevant files change (not on Python changes)
- [ ] `pre-commit run --all-files` passes on clean codebase
- [ ] Existing Python hooks (ruff, mypy) continue to work
