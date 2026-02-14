# Loop 01: Create ESLint Configuration for TypeScript Frontend

## Overview

The TypeScript CI is failing because ESLint cannot find a configuration file. The `package.json` includes all necessary ESLint dependencies (`eslint@8.55.0`, `@typescript-eslint/eslint-plugin@6.14.0`, `@typescript-eslint/parser@6.14.0`, `eslint-plugin-react-hooks@4.6.0`) but no `.eslintrc.*` file exists.

This loop creates a proper ESLint configuration for the React/TypeScript frontend that:
- Uses TypeScript parser with the existing `tsconfig.json`
- Enables React hooks linting rules
- Matches the project's strict TypeScript settings
- Works with the Vite-based build system

## Tasks

### Task 1: Create ESLint Configuration File

**Goal:** Create `.eslintrc.cjs` with TypeScript and React hooks rules matching the installed dependencies.

**Files:**
| Action | Path |
|--------|------|
| CREATE | `web_frontend/.eslintrc.cjs` |

**Steps:**
1. Create `.eslintrc.cjs` (CommonJS format for ESLint 8.x compatibility with ES modules project)
2. Configure parser as `@typescript-eslint/parser` with project reference to `tsconfig.json`
3. Extend recommended configs:
   - `eslint:recommended`
   - `plugin:@typescript-eslint/recommended`
   - `plugin:react-hooks/recommended`
4. Set environment: `browser: true`, `es2022: true`
5. Configure rules to match existing TypeScript strictness:
   - Error on `@typescript-eslint/no-explicit-any` (per CLAUDE.md: "No any types -- ever")
   - Error on unused variables with underscore exception
   - Warn on React hooks exhaustive-deps
6. Ignore patterns: `dist/`, `node_modules/`, `*.config.ts`

**Configuration content:**
```javascript
module.exports = {
  root: true,
  env: {
    browser: true,
    es2022: true,
  },
  extends: [
    'eslint:recommended',
    'plugin:@typescript-eslint/recommended',
    'plugin:react-hooks/recommended',
  ],
  ignorePatterns: ['dist', 'node_modules', '*.config.ts', '*.config.js'],
  parser: '@typescript-eslint/parser',
  parserOptions: {
    ecmaVersion: 'latest',
    sourceType: 'module',
    project: './tsconfig.json',
  },
  plugins: ['@typescript-eslint', 'react-hooks'],
  rules: {
    '@typescript-eslint/no-explicit-any': 'error',
    '@typescript-eslint/no-unused-vars': ['error', { argsIgnorePattern: '^_' }],
    'react-hooks/rules-of-hooks': 'error',
    'react-hooks/exhaustive-deps': 'warn',
  },
};
```

**Verify:**
```bash
cd web_frontend && npx eslint --print-config src/App.tsx
```

### Task 2: Run ESLint and Fix Any Initial Errors

**Goal:** Run ESLint on the codebase to identify and potentially auto-fix any existing lint errors.

**Files:**
| Action | Path |
|--------|------|
| MODIFY | `web_frontend/src/**/*.{ts,tsx}` (if auto-fixable errors exist) |

**Steps:**
1. Run `npm run lint` to see current lint errors
2. If errors are minor and auto-fixable, run `npx eslint src --ext ts,tsx --fix`
3. Review any remaining errors that require manual attention
4. Fix remaining errors (expected to be minimal given existing TypeScript strictness)

**Verify:**
```bash
cd web_frontend && npm run lint
```

### Task 3: Verify Full Build Pipeline

**Goal:** Ensure the complete build pipeline passes including lint, type check, and tests.

**Files:**
| Action | Path |
|--------|------|
| - | (verification only, no changes expected) |

**Steps:**
1. Run TypeScript type checking: `npx tsc --noEmit`
2. Run full lint: `npm run lint`
3. Run tests: `npm test`
4. Run build: `npm run build`

**Verify:**
```bash
cd web_frontend && npx tsc --noEmit && npm run lint && npm test && npm run build
```

## Acceptance Criteria

- [ ] `.eslintrc.cjs` exists in `web_frontend/` directory
- [ ] `npm run lint` completes without errors
- [ ] `npx tsc --noEmit` completes without errors
- [ ] `npm test` passes all tests
- [ ] `npm run build` succeeds
- [ ] No `any` types used in source files (enforced by lint rule)
- [ ] React hooks rules are enforced (rules-of-hooks, exhaustive-deps)

## Notes

- Using `.eslintrc.cjs` (CommonJS) because the project uses `"type": "module"` in package.json, and ESLint 8.x config files need explicit CommonJS format
- The existing `tsconfig.json` already has strict settings that complement ESLint rules
- Source files in `src/` directory are already well-typed based on inspection of `App.tsx`
