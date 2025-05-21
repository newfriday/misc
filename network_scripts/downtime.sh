#!/bin/bash

# VM Live Migration Downtime Test Script
# This script monitors network connectivity during VM live migration
# and calculates the downtime based on ping packet loss

set -e

# Configuration
VM_IP=""
PING_INTERVAL=0.01  # 10ms between pings
LOG_FILE="migration_test.log"
PING_LOG="ping_migration.log"
RESULTS_FILE="migration_results.txt"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Usage function
usage() {
    echo "Usage: $0 -ip <VM_IP> [-i <interval>] [-h]"
    echo "  -ip <VM_IP>    : IP address of the VM to monitor"
    echo "  -i <interval>  : Ping interval in seconds (default: 0.01)"
    echo "  -h             : Show this help message"
    exit 1
}

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -ip)
            VM_IP="$2"
            shift 2
            ;;
        -i)
            PING_INTERVAL="$2"
            shift 2
            ;;
        -h|--help)
            usage
            ;;
        *)
            echo "Unknown option: $1"
            usage
            ;;
    esac
done

# Validate required arguments
if [[ -z "$VM_IP" ]]; then
    echo -e "${RED}Error: VM IP address is required${NC}"
    usage
fi

# Cleanup function
cleanup() {
    echo -e "\n${YELLOW}Cleaning up...${NC}"
    if [[ -n "$PING_PID" ]]; then
        kill $PING_PID 2>/dev/null || true
    fi
    calculate_downtime
    exit 0
}

# Calculate downtime from ping logs
calculate_downtime() {
    echo -e "\n${YELLOW}#######################${NC}"
    echo -e "${YELLOW}Calculating downtime...${NC}"
    echo -e "${YELLOW}#######################${NC}"
    
    if [[ ! -f "$PING_LOG" ]]; then
        echo -e "${RED}Error: Ping log file not found${NC}"
        return 1
    fi
    
    # Extract statistics from ping output
    local stats_line=$(grep 'packets transmitted' "$PING_LOG" | tail -n 1)
    
    if [[ -z "$stats_line" ]]; then
        echo -e "${RED}Error: No ping statistics found${NC}"
        return 1
    fi
    
    # Parse transmitted and received packets
    local transmitted=$(echo "$stats_line" | awk '{print $1}')
    local received=$(echo "$stats_line" | awk '{print $4}')
    
    echo "Transmitted: $transmitted packets"
    echo "Received: $received packets"
    
    # Calculate lost packets and downtime
    local lost=$((transmitted - received))
    local loss_percentage=$(echo "scale=2; $lost * 100 / $transmitted" | bc)
    local downtime=$(echo "scale=3; $lost * $PING_INTERVAL" | bc)
    
    # Log results
    local timestamp=$(date '+%Y-%m-%d %H:%M:%S')
    {
        echo "=== Migration Test Results - $timestamp ==="
        echo "VM IP: $VM_IP"
        echo "Ping Interval: ${PING_INTERVAL}s"
        echo "Packets Transmitted: $transmitted"
        echo "Packets Received: $received"
        echo "Packets Lost: $lost"
        echo "Loss Percentage: ${loss_percentage}%"
        echo "Measured Downtime: ${downtime}s"
        echo ""
    } >> "$RESULTS_FILE"
    
    # Display results
    echo -e "\n${GREEN}=== Results ===${NC}"
    echo -e "Packets Lost: ${RED}$lost${NC}"
    echo -e "Loss Percentage: ${RED}${loss_percentage}%${NC}"
    echo -e "Measured Downtime: ${RED}${downtime}s${NC}"
    
    if (( $(echo "$downtime > 0" | bc -l) )); then
        if (( $(echo "$downtime <= 0.1" | bc -l) )); then
            echo -e "${GREEN}✓ Excellent downtime (<= 100ms)${NC}"
        elif (( $(echo "$downtime <= 0.5" | bc -l) )); then
            echo -e "${YELLOW}⚠ Good downtime (<= 500ms)${NC}"
        else
            echo -e "${RED}⚠ High downtime (> 500ms)${NC}"
        fi
    else
        echo -e "${GREEN}✓ No downtime detected${NC}"
    fi
    
    echo -e "\nResults saved to: ${GREEN}$RESULTS_FILE${NC}"
    echo -e "Ping log saved to: ${GREEN}$PING_LOG${NC}"
}

# Main function
main() {
    echo -e "${GREEN}VM Live Migration Downtime Test${NC}"
    echo -e "================================"
    echo "VM IP: $VM_IP"
    echo "Ping Interval: ${PING_INTERVAL}s"
    echo -e "\n${YELLOW}Starting continuous ping...${NC}"
    echo -e "${YELLOW}Press Ctrl+C to stop and calculate downtime${NC}"
    
    # Setup trap for cleanup
    trap cleanup SIGINT SIGTERM
    
    # Initialize log files
    echo "=== Migration Test Log - $(date) ===" > "$LOG_FILE"
    rm -f "$PING_LOG"
    
    # Start continuous ping
    ping "$VM_IP" -i "$PING_INTERVAL" > "$PING_LOG" 2>&1 &
    PING_PID=$!
    
    # Monitor ping process
    echo "Ping process started (PID: $PING_PID)"
    echo -e "\n${GREEN}Monitoring network connectivity...${NC}"
    
    # Wait for ping process or user interrupt
    wait $PING_PID 2>/dev/null || true
}

# Validate ping interval
if ! echo "$PING_INTERVAL" | grep -Eq '^[0-9]*\.?[0-9]+$'; then
    echo -e "${RED}Error: Invalid ping interval${NC}"
    exit 1
fi

# Check if ping is available
if ! command -v ping &> /dev/null; then
    echo -e "${RED}Error: ping command not found${NC}"
    exit 1
fi

# Check if bc is available for calculations
if ! command -v bc &> /dev/null; then
    echo -e "${RED}Error: bc command not found. Please install bc for calculations${NC}"
    exit 1
fi

# Run main function
main
