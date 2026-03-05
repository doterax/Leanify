#!/usr/bin/env bash
#
# Portable test script for Leanify.
# Works on Linux, macOS, and Windows (MSYS2/Git Bash).
#
# Usage: ./tests/run_tests.sh [path/to/leanify]
#
# Tests:
#   1. Determinism: leanify produces identical output on two independent runs
#   2. Correctness: output matches the pre-computed expected results in tests/output
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
INPUT_DIR="$SCRIPT_DIR/input"
EXPECTED_DIR="$SCRIPT_DIR/output"

# Convert Unix paths to native when calling Windows binaries
to_native() {
    if command -v cygpath >/dev/null 2>&1; then
        cygpath -m "$1"
    elif command -v wslpath >/dev/null 2>&1; then
        wslpath -m "$1"
    else
        local p="$1"
        # Docker Desktop WSL: /mnt/host/d/... → D:/...
        if [[ "$p" =~ ^/mnt/host/([a-zA-Z])(/.*) ]]; then
            echo "${BASH_REMATCH[1]^^}:${BASH_REMATCH[2]}"
        # Standard WSL: /mnt/d/... → D:/...
        elif [[ "$p" =~ ^/mnt/([a-zA-Z])(/.*) ]]; then
            echo "${BASH_REMATCH[1]^^}:${BASH_REMATCH[2]}"
        else
            echo "$p"
        fi
    fi
}

# Portable binary file comparison (cmp may not exist in minimal MSYS2)
files_equal() {
    if command -v cmp >/dev/null 2>&1; then
        cmp -s "$1" "$2"
    elif command -v diff >/dev/null 2>&1; then
        diff -q "$1" "$2" >/dev/null 2>&1
    else
        # Fallback: compare checksums
        local h1 h2
        if command -v sha256sum >/dev/null 2>&1; then
            h1=$(sha256sum < "$1" | cut -d' ' -f1)
            h2=$(sha256sum < "$2" | cut -d' ' -f1)
        elif command -v md5sum >/dev/null 2>&1; then
            h1=$(md5sum < "$1" | cut -d' ' -f1)
            h2=$(md5sum < "$2" | cut -d' ' -f1)
        elif command -v shasum >/dev/null 2>&1; then
            h1=$(shasum < "$1" | cut -d' ' -f1)
            h2=$(shasum < "$2" | cut -d' ' -f1)
        else
            echo "ERROR: No file comparison tool found (cmp, diff, sha256sum, md5sum, shasum)" >&2
            exit 1
        fi
        [ "$h1" = "$h2" ]
    fi
}

# Resolve leanify binary
if [ $# -ge 1 ]; then
    LEANIFY="$1"
else
    # Auto-detect
    if [ -f "$REPO_DIR/leanify.exe" ]; then
        LEANIFY="$REPO_DIR/leanify.exe"
    elif [ -f "$REPO_DIR/leanify" ]; then
        LEANIFY="$REPO_DIR/leanify"
    else
        echo "ERROR: leanify binary not found. Pass path as argument or build first." >&2
        exit 1
    fi
fi

echo "=== Leanify Test Suite ==="
echo "Binary:   $LEANIFY"
echo "Input:    $INPUT_DIR"
echo "Expected: $EXPECTED_DIR"
echo ""

# Validate directories
if [ ! -d "$INPUT_DIR" ]; then
    echo "ERROR: Input directory not found: $INPUT_DIR" >&2
    exit 1
fi
if [ ! -d "$EXPECTED_DIR" ]; then
    echo "ERROR: Expected output directory not found: $EXPECTED_DIR" >&2
    exit 1
fi

# Count input files dynamically
FILE_COUNT=$(find "$INPUT_DIR" -maxdepth 1 -type f | wc -l | tr -d ' ')
if [ "$FILE_COUNT" -eq 0 ]; then
    echo "ERROR: No test files found in $INPUT_DIR" >&2
    exit 1
fi
echo "Found $FILE_COUNT test file(s)"
echo ""

# Create temp working directories under repo (avoids Unix/Windows path issues)
TEMP_BASE="$REPO_DIR/.test_tmp_$$"
TEMP1="$TEMP_BASE/temp1"
TEMP2="$TEMP_BASE/temp2"
LIB_TEMP1="$TEMP_BASE/lib1"
LIB_TEMP2="$TEMP_BASE/lib2"

cleanup() {
    rm -rf "$TEMP_BASE"
}
trap cleanup EXIT

mkdir -p "$TEMP1" "$TEMP2" "$LIB_TEMP1" "$LIB_TEMP2"

# Copy input to both temp directories
cp -r "$INPUT_DIR"/. "$TEMP1"/
cp -r "$INPUT_DIR"/. "$TEMP2"/

FAILED=0

# --- Test 1: Run leanify on temp1 ---
echo "--- Run 1: leanify -p -l $LIB_TEMP1 $TEMP1 ---"
"$LEANIFY" -p -l "$(to_native "$LIB_TEMP1")" "$(to_native "$TEMP1")"
echo ""

# --- Test 2: Run leanify on temp2 ---
echo "--- Run 2: leanify -p -l $LIB_TEMP2 $TEMP2 ---"
"$LEANIFY" -p -l "$(to_native "$LIB_TEMP2")" "$(to_native "$TEMP2")"
echo ""

# --- Compare temp1 vs temp2 (determinism) ---
echo "=== Test: Determinism (temp1 == temp2) ==="
DETERMINISM_OK=true
for f in "$TEMP1"/*; do
    fname="$(basename "$f")"
    if [ ! -f "$TEMP2/$fname" ]; then
        echo "  FAIL: $fname missing in temp2"
        DETERMINISM_OK=false
        continue
    fi
    if files_equal "$f" "$TEMP2/$fname"; then
        echo "  OK:   $fname"
    else
        echo "  FAIL: $fname differs between runs"
        DETERMINISM_OK=false
    fi
done
# Check for extra files in temp2
for f in "$TEMP2"/*; do
    fname="$(basename "$f")"
    if [ ! -f "$TEMP1/$fname" ]; then
        echo "  FAIL: $fname exists in temp2 but not temp1"
        DETERMINISM_OK=false
    fi
done

if $DETERMINISM_OK; then
    echo "PASSED: Determinism test"
else
    echo "FAILED: Determinism test"
    FAILED=1
fi
echo ""

# --- Compare temp1 vs expected output (correctness) ---
echo "=== Test: Correctness (temp1 == expected) ==="
CORRECTNESS_OK=true
for f in "$EXPECTED_DIR"/*; do
    [ -f "$f" ] || continue
    fname="$(basename "$f")"
    if [ ! -f "$TEMP1/$fname" ]; then
        echo "  FAIL: $fname missing in temp1 output"
        CORRECTNESS_OK=false
        continue
    fi
    if files_equal "$f" "$TEMP1/$fname"; then
        echo "  OK:   $fname"
    else
        EXPECTED_SIZE=$(wc -c < "$f" | tr -d ' ')
        ACTUAL_SIZE=$(wc -c < "$TEMP1/$fname" | tr -d ' ')
        echo "  FAIL: $fname size mismatch (expected=$EXPECTED_SIZE, actual=$ACTUAL_SIZE)"
        CORRECTNESS_OK=false
    fi
done
# Check for files in temp1 not in expected (new input files without expected output)
for f in "$TEMP1"/*; do
    fname="$(basename "$f")"
    if [ ! -f "$EXPECTED_DIR/$fname" ]; then
        echo "  WARN: $fname in output but no expected file (add to tests/output?)"
    fi
done

if $CORRECTNESS_OK; then
    echo "PASSED: Correctness test"
else
    echo "FAILED: Correctness test"
    FAILED=1
fi
echo ""

# --- Summary ---
if [ "$FAILED" -eq 0 ]; then
    echo "=== ALL TESTS PASSED ==="
    exit 0
else
    echo "=== SOME TESTS FAILED ==="
    exit 1
fi
