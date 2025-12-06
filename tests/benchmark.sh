#!/bin/bash
# Comprehensive Apex Performance Benchmark

APEX="./build/apex"
TEST_FILE="tests/comprehensive_test.md"
ITERATIONS=50

echo "╔══════════════════════════════════════════════════════════════╗"
echo "║         Apex Markdown Processor - Performance Benchmark      ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""

# Document stats
LINES=$(wc -l < "$TEST_FILE")
WORDS=$(wc -w < "$TEST_FILE")
BYTES=$(wc -c < "$TEST_FILE")

echo "📄 Test Document: $TEST_FILE"
echo "   Lines:  $LINES"
echo "   Words:  $WORDS"
echo "   Size:   $BYTES bytes"
echo ""

# Function to run benchmark
benchmark() {
    local mode="$1"
    local args="$2"
    local desc="$3"

    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo "🔧 $desc"
    echo "   Command: apex $args"
    echo ""

    # Warm-up run
    $APEX $args "$TEST_FILE" > /dev/null 2>&1

    # Timed runs
    local total=0
    local min=999999
    local max=0

    for i in $(seq 1 $ITERATIONS); do
        local start=$(gdate +%s%N 2>/dev/null || date +%s000000000)
        $APEX $args "$TEST_FILE" > /dev/null 2>&1
        local end=$(gdate +%s%N 2>/dev/null || date +%s000000000)
        local elapsed=$(( (end - start) / 1000000 ))

        total=$((total + elapsed))
        [ $elapsed -lt $min ] && min=$elapsed
        [ $elapsed -gt $max ] && max=$elapsed
    done

    local avg=$((total / ITERATIONS))
    local throughput=$(echo "scale=2; $WORDS / ($avg / 1000)" | bc)

    echo "   Iterations: $ITERATIONS"
    echo "   Average:    ${avg}ms"
    echo "   Min:        ${min}ms"
    echo "   Max:        ${max}ms"
    echo "   Throughput: ${throughput} words/sec"
    echo ""
}

# Run benchmarks
benchmark "fragment" "" "Fragment Mode (default HTML output)"
benchmark "pretty" "--pretty" "Pretty-Print Mode (formatted HTML)"
benchmark "standalone" "--standalone" "Standalone Mode (complete HTML document)"
benchmark "combined" "--standalone --pretty" "Standalone + Pretty (full features)"

# Feature-specific benchmarks
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "📊 Feature Breakdown"
echo ""

# Test with minimal features
echo "   Minimal (CommonMark only):  " && \
    time $APEX --mode commonmark "$TEST_FILE" > /dev/null 2>&1

# Test with GFM
echo "   GFM Extensions:             " && \
    time $APEX --mode gfm "$TEST_FILE" > /dev/null 2>&1

# Test with full Apex
echo "   Full Apex (all features):   " && \
    time $APEX "$TEST_FILE" > /dev/null 2>&1

echo ""
echo "╔══════════════════════════════════════════════════════════════╗"
echo "║                    Benchmark Complete                        ║"
echo "╚══════════════════════════════════════════════════════════════╝"

