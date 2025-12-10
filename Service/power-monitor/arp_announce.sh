#!/bin/bash
# ARP Announcement Script
# Sends gratuitous ARP to keep the Pi "visible" on the network

# Get network interface and IP
INTERFACE=$(ip route | grep default | awk '{print $5}' | head -1)
IP=$(ip -4 addr show $INTERFACE | grep -oP '(?<=inet\s)\d+(\.\d+){3}' | head -1)
GATEWAY=$(ip route | grep default | awk '{print $3}')

if [ -z "$INTERFACE" ] || [ -z "$IP" ]; then
    echo "Error: Could not determine network interface or IP"
    exit 1
fi

# Send gratuitous ARP 
arping -c 1 -I $INTERFACE -U $IP > /dev/null 2>&1

# Also ping the gateway to keep routing fresh
ping -c 1 -W 1 $GATEWAY > /dev/null 2>&1

exit 0