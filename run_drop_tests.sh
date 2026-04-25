#!/bin/bash

# Drop Simulator Test Harness
# Tests the pseudo-random packet drop mechanism with multiple runs
# Logs results for analysis and visualization

set -o pipefail

STARTERCODE_DIR="./startercode"
DROP_TEST_DIR="drop_logs_$(date +%Y%m%d_%H%M%S)"
LOCAL_HOST=$(hostname)

# Color codes for output
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m' # No Color

echo -e "========================================"
echo -e "Drop Simulator Test Suite$"
echo -e "========================================"
echo -e "Host: $LOCAL_HOST"

# Create log directory
mkdir -p "$DROP_TEST_DIR"
echo "Log directory: $DROP_TEST_DIR"

# Compile drop if needed
if [ ! -f "$STARTERCODE_DIR/drop" ]; then
    echo -e "Compiling drop..."
    clang -o "$STARTERCODE_DIR/drop" drop.c
    if [ $? -ne 0 ]; then
        echo -e "${RED}✗ Compilation failed${NC}"
        exit 1
    fi
    echo -e "${GREEN}✓ Compilation successful${NC}"
fi

# Test configurations: (drop_percentage, packet_count, label)
TEST_CONFIGS=(
    "5:100:small-5pct"
    "5:1000:medium-5pct"
    "5:10000:large-5pct"
    "10:100:small-10pct"
    "10:1000:medium-10pct"
    "10:10000:large-10pct"
    "20:100:small-20pct"
    "20:1000:medium-20pct"
    "20:10000:large-20pct"
)

# Create summary file
SUMMARY_FILE="$DROP_TEST_DIR/drop_summary.txt"
cat > "$SUMMARY_FILE" << EOF
Drop Simulator Test Results
===========================
Hostname: $LOCAL_HOST
Date: $(date '+%Y-%m-%d %H:%M:%S')

This file contains the summary of drop simulator tests.
Each test runs the drop program to simulate packet loss with various parameters.

Test Summary:
EOF

echo "" >> "$SUMMARY_FILE"
echo "Start Time: $(date '+%Y-%m-%d %H:%M:%S')" >> "$SUMMARY_FILE"
echo "" >> "$SUMMARY_FILE"

# Run tests
TEST_NUM=0
PASSED=0
FAILED=0

for config in "${TEST_CONFIGS[@]}"; do
    TEST_NUM=$((TEST_NUM + 1))
    
    # Parse config
    IFS=':' read -r DROP_PCT PACKET_COUNT LABEL <<< "$config"
    
    # Create test-specific log file
    LOG_FILE="$DROP_TEST_DIR/test_${TEST_NUM}_${LABEL}.log"
    
    echo -e "Test $TEST_NUM: drop $DROP_PCT% with $PACKET_COUNT packets ($LABEL)"
    
    # Run drop simulator
    START_TIME=$(date +%s%N)
    "$STARTERCODE_DIR/drop" "$DROP_PCT" "$PACKET_COUNT" > "$LOG_FILE" 2>&1
    EXIT_CODE=$?
    END_TIME=$(date +%s%N)
    DURATION_MS=$(( (END_TIME - START_TIME) / 1000000 ))
    
    if [ $EXIT_CODE -eq 0 ]; then
        # Extract actual drop percentage from output
        ACTUAL_DROP=$(grep "dropped" "$LOG_FILE" | grep -oE '[0-9]+\.[0-9]+%' | head -1 | sed 's/%//')
        
        echo -e "${GREEN}✓ PASSED${NC} (${DURATION_MS}ms, actual drop: ${ACTUAL_DROP}%)"
        
        echo "Test $TEST_NUM: $LABEL - PASSED (${DURATION_MS}ms, actual_drop: ${ACTUAL_DROP}%)" >> "$SUMMARY_FILE"
        PASSED=$((PASSED + 1))
    else
        echo -e "${RED}✗ FAILED${NC} (exit code: $EXIT_CODE)"
        echo "Test $TEST_NUM: $LABEL - FAILED (exit code: $EXIT_CODE)" >> "$SUMMARY_FILE"
        FAILED=$((FAILED + 1))
    fi
done

# Final summary
echo "" >> "$SUMMARY_FILE"
echo "End Time: $(date '+%Y-%m-%d %H:%M:%S')" >> "$SUMMARY_FILE"
echo "" >> "$SUMMARY_FILE"
echo "Test Summary:" >> "$SUMMARY_FILE"
echo "  Total Tests: $TEST_NUM" >> "$SUMMARY_FILE"
echo "  Passed: $PASSED" >> "$SUMMARY_FILE"
echo "  Failed: $FAILED" >> "$SUMMARY_FILE"
echo "  Success Rate: $(awk "BEGIN {printf \"%.1f\", 100*$PASSED/$TEST_NUM}")%" >> "$SUMMARY_FILE"

# Print final summary
echo ""
echo -e "========================================"
echo -e "Test Summary${NC}"
echo -e "========================================"
echo "Total Tests: $TEST_NUM"
echo -e "Passed: ${GREEN}$PASSED${NC}"
echo -e "Failed: ${RED}$FAILED${NC}"
echo "Success Rate: $(awk "BEGIN {printf \"%.1f\", 100*$PASSED/$TEST_NUM}")%"
echo ""
echo "Results saved to: $DROP_TEST_DIR/"
echo "Summary file: $SUMMARY_FILE"
