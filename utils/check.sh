#!/usr/bin/env bash
# 统一质量检查脚本
# 用法:
#   utils/check.sh                     全量检查 (format + tidy + build + test)
#   utils/check.sh --hook              pre-commit 模式 (修空白 + 修格式 + tidy，仅暂存文件)
#   utils/check.sh [--skip-tidy] [--skip-build] [--skip-test]  跳过指定步骤

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BOLD='\033[1m'
RESET='\033[0m'

pass() { echo -e "${GREEN}[PASS]${RESET} $1"; }
fail() { echo -e "${RED}[FAIL]${RESET} $1"; }
info() { echo -e "${BOLD}[....]${RESET} $1"; }
warn() { echo -e "${YELLOW}[WARN]${RESET} $1"; }
die() { echo -e "${RED}error:${RESET} $*" >&2; exit 1; }

# ============================================================
# 参数解析
# ============================================================
HOOK_MODE=false
SKIP_TIDY=false
SKIP_BUILD=false
SKIP_TEST=false

for arg in "$@"; do
    case "$arg" in
        --hook)       HOOK_MODE=true  ;;
        --skip-tidy)  SKIP_TIDY=true  ;;
        --skip-build) SKIP_BUILD=true ;;
        --skip-test)  SKIP_TEST=true  ;;
    esac
done

cd "$PROJECT_ROOT"

# ============================================================
# --hook 模式：pre-commit 行为
# ============================================================
if [ "$HOOK_MODE" = true ]; then
    CPP_PATTERNS=('*.cpp' '*.hpp')

    need_cmd() { command -v "$1" >/dev/null 2>&1 || die "$1 not found"; }
    is_text_file() { [ -f "$1" ] && file --mime "$1" | grep -q 'charset='; }
    has_unstaged_changes() { ! git diff --quiet -- "$1"; }
    has_staged_changes() { ! git diff --cached --quiet -- "$1"; }
    is_partially_staged() { has_staged_changes "$1" && has_unstaged_changes "$1"; }

    mapfile -d '' STAGED_FILES < <(
        git diff --cached --name-only -z --diff-filter=ACM
    )
    mapfile -d '' STAGED_CPP_FILES < <(
        git diff --cached --name-only -z --diff-filter=ACM -- "${CPP_PATTERNS[@]}"
    )

    # 避免把未暂存改动一起 git add 进去
    for file in "${STAGED_FILES[@]}"; do
        if is_partially_staged "$file"; then
            die "$file has both staged and unstaged changes. Please stage or stash them before commit."
        fi
    done

    FIXED=0

    # --- trailing whitespace + EOF fixer ---
    for file in "${STAGED_FILES[@]}"; do
        [ -f "$file" ] || continue
        is_text_file "$file" || continue

        before_hash="$(git hash-object "$file")"

        perl -pi -e 's/[ \t]+$//' "$file"

        if [ -s "$file" ] && [ "$(tail -c 1 "$file" | wc -l)" -eq 0 ]; then
            printf '\n' >> "$file"
        fi

        after_hash="$(git hash-object "$file")"

        if [ "$before_hash" != "$after_hash" ]; then
            echo "fix whitespace/eof: $file"
            git add -- "$file"
            FIXED=1
        fi
    done

    # --- clang-format (fix) ---
    if [ "${#STAGED_CPP_FILES[@]}" -gt 0 ]; then
        need_cmd clang-format

        for file in "${STAGED_CPP_FILES[@]}"; do
            [ -f "$file" ] || continue

            before_hash="$(git hash-object "$file")"
            clang-format -i "$file"
            after_hash="$(git hash-object "$file")"

            if [ "$before_hash" != "$after_hash" ]; then
                echo "clang-format: $file"
                git add -- "$file"
                FIXED=1
            fi
        done
    fi

    # --- clang-tidy (暂存文件) ---
    if [ "${#STAGED_CPP_FILES[@]}" -gt 0 ]; then
        need_cmd clang-tidy

        if command -v xmake >/dev/null 2>&1; then
            xmake project -k compile_commands build >/dev/null 2>&1 || true
        fi

        if [ ! -f "$PROJECT_ROOT/build/compile_commands.json" ]; then
            die "compile_commands.json not found. Run: xmake project -k compile_commands build"
        fi

        TIDY_FAILED=0
        for file in "${STAGED_CPP_FILES[@]}"; do
            [ -f "$file" ] || continue

            case "$file" in
                *.hpp) echo "tidy header: $file" ;;
                *)     echo "tidy: $file" ;;
            esac

            if ! clang-tidy "$file" -p="$PROJECT_ROOT/build"; then
                echo "error: clang-tidy failed in $file" >&2
                TIDY_FAILED=1
            fi
        done

        if [ "$TIDY_FAILED" -eq 1 ]; then
            exit 1
        fi
    fi

    echo "pre-commit checks passed."
    exit 0
fi

# ============================================================
# 默认模式：全量检查
# ============================================================

TOTAL=0
PASSED=0
FAILED=0

run_check() {
    local name="$1"
    TOTAL=$((TOTAL + 1))
    info "$name"
}

mark_pass() {
    PASSED=$((PASSED + 1))
    pass "$1"
}

mark_fail() {
    FAILED=$((FAILED + 1))
    fail "$1"
}

# ============================================================
# 1. clang-format (check)
# ============================================================
run_check "clang-format"

FORMAT_FILES=()
while IFS= read -r -d '' f; do
    FORMAT_FILES+=("$f")
done < <(find include src -type f \( -name '*.hpp' -o -name '*.cpp' \) -print0 2>/dev/null)

if [ "${#FORMAT_FILES[@]}" -eq 0 ]; then
    warn "clang-format (no source files found)"
else
    if clang-format --dry-run --Werror "${FORMAT_FILES[@]}" 2>&1; then
        mark_pass "clang-format"
    else
        mark_fail "clang-format (run: clang-format -i ${FORMAT_FILES[*]})"
        exit 1
    fi
fi

# ============================================================
# 2. clang-tidy
# ============================================================
if [ "$SKIP_TIDY" = false ]; then
    run_check "clang-tidy"

    if command -v xmake >/dev/null 2>&1; then
        xmake project -k compile_commands build >/dev/null 2>&1 || true
    fi

    if [ ! -f "$PROJECT_ROOT/build/compile_commands.json" ]; then
        mark_fail "clang-tidy (compile_commands.json not found)"
        exit 1
    fi

    TIDY_FILES=()
    while IFS= read -r -d '' f; do
        TIDY_FILES+=("$f")
    done < <(find src -type f -name '*.cpp' -print0 2>/dev/null)

    if [ "${#TIDY_FILES[@]}" -eq 0 ]; then
        warn "clang-tidy (no source files found)"
    else
        TIDY_OUTPUT=$(clang-tidy -p="$PROJECT_ROOT/build" \
            --extra-arg="-Iinclude" --extra-arg="-std=c++23" --extra-arg="-stdlib=libc++" \
            "${TIDY_FILES[@]}" 2>&1) || true

        TIDY_ISSUES=$(echo "$TIDY_OUTPUT" | grep -E "warning:|error:" | grep -v "Suppressed" || true)

        if [ -z "$TIDY_ISSUES" ]; then
            mark_pass "clang-tidy"
        else
            mark_fail "clang-tidy"
            echo "$TIDY_ISSUES" | sed 's/^/  /'
            exit 1
        fi
    fi
else
    warn "clang-tidy (skipped)"
fi

# ============================================================
# 3. build
# ============================================================
if [ "$SKIP_BUILD" = false ]; then
    run_check "build (xmake -r)"

    if xmake -r >/dev/null 2>&1; then
        mark_pass "build"
    else
        mark_fail "build"
        xmake -r 2>&1 | sed 's/^/  /' || true
        exit 1
    fi
else
    warn "build (skipped)"
fi

# ============================================================
# 4. tests
# ============================================================
if [ "$SKIP_TEST" = false ]; then
    run_check "tests"

    # TODO: 在此处添加项目特定的测试逻辑
    # 示例:
    #   for f in tests/*.txt; do
    #       if ! xmake run app "$f" >/dev/null 2>&1; then
    #           TEST_FAIL=$((TEST_FAIL + 1))
    #           echo "  FAIL: $f"
    #       fi
    #   done

    TEST_FAIL=0

    if [ "$TEST_FAIL" -eq 0 ]; then
        mark_pass "tests"
    else
        mark_fail "tests ($TEST_FAIL failed)"
        exit 1
    fi
else
    warn "tests (skipped)"
fi

# ============================================================
# Summary
# ============================================================
echo ""
echo "========================================"
if [ "$FAILED" -eq 0 ]; then
    echo -e "${GREEN}All $TOTAL checks passed.${RESET}"
else
    echo -e "${RED}$FAILED of $TOTAL checks failed.${RESET}"
fi
echo "========================================"

exit "$FAILED"
