#!/usr/bin/env python3

import psutil
import time
from datetime import datetime

def calculate_speed(interface, interval=1):
    # Get the initial number of bytes sent and received
    initial_bytes_sent = psutil.net_io_counters(pernic=True)[interface].bytes_sent
    initial_bytes_recv = psutil.net_io_counters(pernic=True)[interface].bytes_recv
    
    # Wait for the specified interval
    time.sleep(interval)
    
    # Get the number of bytes sent and received after the interval
    final_bytes_sent = psutil.net_io_counters(pernic=True)[interface].bytes_sent
    final_bytes_recv = psutil.net_io_counters(pernic=True)[interface].bytes_recv
    
    # Calculate the data speed in bytes per second
    sent_speed = final_bytes_sent - initial_bytes_sent
    recv_speed = final_bytes_recv - initial_bytes_recv
    
    return sent_speed, recv_speed

# Define the network interface
interface = 'enp6s0f0'

print("Monitoring data speed for interface: {}".format(interface))

# Continuous monitoring loop
while True:
    # Calculate the data speed for the interface
    sent_speed, recv_speed = calculate_speed(interface, 6)
    
    # Convert bytes
    sent_speed_mbps = sent_speed / 1024.
    recv_speed_mbps = recv_speed / 1024.

    current_time = datetime.now().strftime("%H:%M:%S")

    print("{}| Data speed  (sent): {:10f} KiB/s (received): {:10f} KiB/s".format(current_time, sent_speed_mbps,recv_speed_mbps))

    # Wait for one second before the next iteration
    #time.sleep(1)