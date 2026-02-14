# Implementation: Loop 06 - Clean up tracked cache files and update .gitignore

## Task 1: Add explicit `.coverage` pattern

Completed: 2026-02-13

### Changes

- `C:\Users\costa\src\warehouser\.gitignore`: Added explicit `.coverage` pattern on line 15

### Details

The existing pattern `*.coverage` does NOT match the literal file `.coverage` because:
- In glob patterns, `*` does not match leading dots
- `*.coverage` matches `foo.coverage`, `bar.coverage`, etc.
- But it does NOT match `.coverage` (a hidden file starting with a dot)

Added `.coverage` as the first entry in the Coverage section to explicitly ignore the pytest-cov output file.

**Before (line 14-19):**
```gitignore
# Coverage
*.coverage
coverage.xml
htmlcov/
.coverage.*
```

**After (line 14-20):**
```gitignore
# Coverage
.coverage
*.coverage
coverage.xml
htmlcov/
.coverage.*
```

### Verification

- [x] `.gitignore` syntax is valid
- [x] Pattern `.coverage` is present on line 15
- [x] Existing patterns preserved

### Notes

The actual removal of tracked files from git (using `git rm --cached`) is NOT part of this task and will be handled separately. This change only ensures that future `.coverage` files will be properly ignored.

---
