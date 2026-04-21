#!/bin/bash

##############################################################################
# LRTP Test Runner - Runs all tests sequentially between two machines
# Logs individual outputs to separate files for analysis
##############################################################################

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Script directory
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
STARTERCODE_DIR="$SCRIPT_DIR/startercode"

# Create timestamped log directory
LOG_DIR="$SCRIPT_DIR/test_logs_$(date +%Y%m%d_%H%M%S)"
mkdir -p "$LOG_DIR"

echo -e "${BLUE}================================================${NC}"
echo -e "${BLUE}LRTP Test Runner${NC}"
echo -e "${BLUE}================================================${NC}"
echo ""

# Prompt for client hostname
read -p "Enter the client hostname to connect to: " CLIENT_HOST

if [ -z "$CLIENT_HOST" ]; then
    echo -e "${RED}Error: Hostname cannot be empty${NC}"
    exit 1
fi

# Validate connection to client
echo -e "${YELLOW}Validating connection to client at $CLIENT_HOST...${NC}"
if ! ping -c 1 -W 2 "$CLIENT_HOST" &> /dev/null; then
    echo -e "${RED}Error: Cannot reach $CLIENT_HOST${NC}"
    exit 1
fi
echo -e "${GREEN}Connection successful!${NC}"
echo ""

# Change to startercode directory
cd "$STARTERCODE_DIR"

# Compile all tests
echo -e "${YELLOW}Compiling tests...${NC}"
if ! make clean > /dev/null 2>&1; then
    echo -e "${YELLOW}(No previous build to clean)${NC}"
fi

if ! make all > "$LOG_DIR/build.log" 2>&1; then
    echo -e "${RED}Build failed! See $LOG_DIR/build.log${NC}"
    cat "$LOG_DIR/build.log"
    exit 1
fi
echo -e "${GREEN}Build successful!${NC}"
echo ""

# Get server hostname (this machine)
SERVER_HOST=$(hostname)

# Define test pairs with format: client:server:testname:client_args
TEST_PAIRS=(
    "test-client-0:test-server-0:0:"
    "test-client-1:test-server-1:1:"
    "test-client-2:test-server-2:2:"
    "test-client-3:test-server-3:3:"
    "test-adaptive-rto-client:test-adaptive-rto-server:adaptive-rto:$SERVER_HOST 24536 10"
    "test-large-transfer-client:test-large-transfer-server:large-transfer:$SERVER_HOST 24536 1024"
    "test-multiple-sends-client:test-multiple-sends-server:multiple-sends:$SERVER_HOST 24536"
    "test-stress-client:test-stress-server:stress:$SERVER_HOST 24536 100"
)

# Summary file
SUMMARY_FILE="$LOG_DIR/test_summary.txt"
echo "LRTP Test Run Summary" > "$SUMMARY_FILE"
echo "=====================" >> "$SUMMARY_FILE"
echo "Start Time: $(date)" >> "$SUMMARY_FILE"
echo "Client Host: $CLIENT_HOST" >> "$SUMMARY_FILE"
echo "Server Host: localhost" >> "$SUMMARY_FILE"
echo "" >> "$SUMMARY_FILE"

PASSED=0
FAILED=0
TOTAL=${#TEST_PAIRS[@]}

# Run each test pair
for i in "${!TEST_PAIRS[@]}"; do
    IFS=':' read -r CLIENT_TEST SERVER_TEST TEST_NAME CLIENT_ARGS <<< "${TEST_PAIRS[$i]}"
    
    TEST_NUM=$((i + 1))
    echo -e "${BLUE}[Test $TEST_NUM/$TOTAL] Running: $TEST_NAME${NC}"
    
    # Log files for this test
    CLIENT_LOG="$LOG_DIR/${TEST_NAME}_client.log"
    SERVER_LOG="$LOG_DIR/${TEST_NAME}_server.log"
    
    # Start server in background
    echo "Starting server test: $SERVER_TEST"
    ./$SERVER_TEST > "$SERVER_LOG" 2>&1 &
    SERVER_PID=$!
    SERVER_START_TIME=$(date +%s)
    
    # Give server time to start listening
    sleep 3
    
    # Run client test on remote machine
    echo "Starting client test on $CLIENT_HOST: $CLIENT_TEST $CLIENT_ARGS"
    CLIENT_EXIT=0
    ssh "$CLIENT_HOST" "cd $STARTERCODE_DIR && ./$CLIENT_TEST $CLIENT_ARGS" > "$CLIENT_LOG" 2>&1 || CLIENT_EXIT=$?
    
    # Wait for server to finish
    SERVER_EXIT=0
    wait $SERVER_PID 2>/dev/null || SERVER_EXIT=$?
    
    SERVER_END_TIME=$(date +%s)
    
    # Check results
    if [ $CLIENT_EXIT -eq 0 ] && [ $SERVER_EXIT -eq 0 ]; then
        echo -e "${GREEN}✓ PASSED${NC}"
        PASSED=$((PASSED + 1))
        RESULT="PASSED"
    else
        echo -e "${RED}✗ FAILED${NC}"
        FAILED=$((FAILED + 1))
        RESULT="FAILED"
        if [ $CLIENT_EXIT -ne 0 ]; then
            echo -e "${RED}  Client exit code: $CLIENT_EXIT${NC}"
        fi
        if [ $SERVER_EXIT -ne 0 ]; then
            echo -e "${RED}  Server exit code: $SERVER_EXIT${NC}"
        fi
    fi
    
    # Log to summary
    DURATION=$((SERVER_END_TIME - SERVER_START_TIME))
    echo "Test $TEST_NUM: $TEST_NAME - $RESULT (${DURATION}s)" >> "$SUMMARY_FILE"
    echo "  Client Log: ${TEST_NAME}_client.log" >> "$SUMMARY_FILE"
    echo "  Server Log: ${TEST_NAME}_server.log" >> "$SUMMARY_FILE"
    echo "" >> "$SUMMARY_FILE"
    
    # Add spacing between tests
    echo ""
    
    # Small delay between tests
    sleep 1
done

# Print and save final summary
echo -e "${BLUE}================================================${NC}"
echo -e "${BLUE}Test Run Complete${NC}"
echo -e "${BLUE}================================================${NC}"
echo ""
echo -e "Total Tests: ${TOTAL}"
echo -e "Passed: ${GREEN}${PASSED}${NC}"
echo -e "Failed: ${RED}${FAILED}${NC}"
echo ""
echo -e "Log directory: ${YELLOW}$LOG_DIR${NC}"
echo ""

# Append final stats to summary
echo "================================================" >> "$SUMMARY_FILE"
echo "Final Results" >> "$SUMMARY_FILE"
echo "Total Tests: $TOTAL" >> "$SUMMARY_FILE"
echo "Passed: $PASSED" >> "$SUMMARY_FILE"
echo "Failed: $FAILED" >> "$SUMMARY_FILE"
echo "End Time: $(date)" >> "$SUMMARY_FILE"

# List all log files
echo -e "${BLUE}Generated Log Files:${NC}"
ls -lh "$LOG_DIR"
echo ""

if [ $FAILED -eq 0 ]; then
    echo -e "${GREEN}All tests passed!${NC}"
    exit 0
else
    echo -e "${RED}Some tests failed. Review logs for details.${NC}"
    exit 1
fi
