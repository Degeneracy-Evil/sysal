---
name: code-quality-review
description: |
  MUST USE for code quality review, quality assessment, or pre-release quality gate.
  Triggers: "代码质量评审", "quality review", "review code quality", "评审代码",
  "pre-release review", "质量检测", "quality gate".
  Executes multi-dimensional code review using parallel Oracle agents.
---

# Code Quality Review

## Overview

This skill executes a multi-dimensional code quality review using Oracle agents in parallel, following the method defined in `docs/code_quality_review_method.md`. Each Oracle agent evaluates 1-2 dimensions independently, scoring on a 1-10 integer scale with specific code evidence. Results are compiled into a weighted total score and written to a standalone quality report.

## Prerequisites

Before starting, confirm all of the following:

1. **Docs aligned**: Design docs and code are in sync (grep-verify no stale references)
2. **Build passes**: `xmake -r` exits 0 with zero warnings
3. **Tests pass**: All tests pass with zero failures
4. **Clean working tree**: No uncommitted changes (review targets committed code)
5. **Version determined**: Know which version is under review (see `include/sysal/version.hpp`)

## Process

1. **Confirm prerequisites** — verify all 5 conditions above. If any fail, abort and report what needs fixing.

2. **Determine dimensions and weights** — reference the 9-dimension table in `docs/code_quality_review_method.md` (Design Fidelity 15%, API Elegance 15%, Code Consistency 12%, Core Logic Correctness 12%, Type Safety 8%, Error Handling 10%, Module Robustness 10%, Separation of Concerns 10%, Test Quality 8%). Adjust weights if needed, keeping total at 100%.

3. **Launch Oracle agents in batches** — max 2 agents per batch. Each Oracle gets 1-2 dimensions. Typical batching:
   - Batch 1: Oracle A (dimensions 1-2), Oracle B (dimensions 3-4)
   - Batch 2: Oracle C (dimensions 5-6), Oracle D (dimensions 7-8)
   - Batch 3: Oracle E (dimension 9)
   Use `call_omo_agent` with `subagent_type="oracle"` and `run_in_background=true`.

4. **Collect results and compile weighted score** — gather all Oracle outputs via `background_output`. Compute weighted total: sum of (score x weight) for each dimension, rounded to integer.

5. **Write report** — save to `docs/quality_reports/v{version}_review.md` using the report template from the method doc (total score table, per-dimension details, key issues with P0/P1/P2 classification, version trend if historical data exists).

6. **Classify issues** — P0 (blocking, must fix before release), P1 (should fix this version), P2 (suggested for next version). Log P1 deferrals in `docs/devlog.md`.

## Oracle Prompt Template

Each Oracle agent prompt must follow this structure:

```
## TASK
You are a code quality reviewer. Score N dimensions of the sysal codebase.

## CONTEXT
- Version: [version number]
- Previous scores: [previous review scores, if any]
- Files changed since last review: [git log --oneline summary]

## SCORING RULES
- Integer scores only (1-10), no decimals
- Every score MUST have specific code evidence: file path + line number + description
- Score 10 = flawless, 8 = minor issues, 6 = notable gaps, 4 = serious problems, 2 = barely functional
- Compare with previous scores if available; explain any score change

## DIMENSION N: [dimension name] (Weight: X%)
**What to evaluate**: [what to assess]
**How to evaluate**: [specific steps, files to read, checks to perform]
**Files to read**: [file paths]

## OUTPUT FORMAT
## DIMENSION N: [dimension name]
Score: X/10 (previous: Y/10, change: +/-Z)
### Findings
- [PASS/FAIL/WARN] File: path:line -- description
### Summary
2-3 sentence assessment
```

## Reference

- Full method definition: `docs/code_quality_review_method.md`
- Past quality reports: `docs/quality_reports/`
- Version header: `include/sysal/version.hpp`
