#!/usr/bin/python
import numpy as np

# Read values from the file into a list
files = [
          "perf_untrusted_tc_in_ns.txt",
          "perf_trustedsfn_tc_in_ns.txt",
        ]

values = []

for f in files:
    with open(f, "r") as file:
    
        for line in file:
            value = float(line.strip())
            values.append(value)
    
        average_ns = np.mean(values)
        std_dev_ns = np.std(values)
    
        # Print the results
        print(f"For file: {f}")
        print(f"    Average: {average_ns * 10**-3} µs")
        print(f"    Std.dev: {std_dev_ns * 10**-3} µs")
        print(f"")
