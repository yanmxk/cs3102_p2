#!/bin/bash

# Script to analyze and display test logs
# Usage: ./analyze_logs.sh <log_directory>

if [ -z "$1" ]; then
    # If no argument, use the most recent log directory
    LOG_DIR=$(ls -dt test_logs_* 2>/dev/null | head -1)
    if [ -z "$LOG_DIR" ]; then
        echo "No test_logs directory found"
        exit 1
    fi
else
    LOG_DIR="$1"
fi

if [ ! -d "$LOG_DIR" ]; then
    echo "Directory not found: $LOG_DIR"
    exit 1
fi

echo "========================================"
echo "LRTP Test Log Analysis"
echo "========================================"
echo "Log Directory: $LOG_DIR"
echo ""

# Show summary
echo "=== Test Summary ==="
cat "$LOG_DIR/test_summary.txt"
echo ""

# List failed tests
FAILED_TESTS=$(grep "FAILED" "$LOG_DIR/test_summary.txt" | awk '{print $3}' | sed 's/-//')

if [ -n "$FAILED_TESTS" ]; then
    echo "=== Failed Test Details ==="
    echo ""
    
    for test_name in $(grep "FAILED" "$LOG_DIR/test_summary.txt" | awk '{print $3}'); do
        test_name=$(echo "$test_name" | sed 's/://')
        echo "────────────────────────────────────────"
        echo "Test: $test_name"
        echo "────────────────────────────────────────"
        
        CLIENT_LOG="$LOG_DIR/${test_name}_client.log"
        SERVER_LOG="$LOG_DIR/${test_name}_server.log"
        
        if [ -f "$CLIENT_LOG" ]; then
            echo "--- CLIENT OUTPUT ---"
            cat "$CLIENT_LOG"
            echo ""
        fi
        
        if [ -f "$SERVER_LOG" ]; then
            echo "--- SERVER OUTPUT ---"
            cat "$SERVER_LOG"
            echo ""
        fi
    done
fi

# Show any compile errors
if [ -f "$LOG_DIR/build.log" ]; then
    if grep -i "error" "$LOG_DIR/build.log" > /dev/null; then
        echo "=== Build Errors ==="
        grep -i "error" "$LOG_DIR/build.log"
    fi
fi
