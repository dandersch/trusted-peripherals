#!/usr/bin/python
import numpy as np


# as microseconds
files1 = [
          # context switch performance
          "perf_cs_trustedsfn_in_ns.txt",
          "perf_cs_trustedipc_in_ns.txt",
          "perf_cs_untrusted_in_ns.txt",
          "perf_cs_emulated_in_ns.txt",
          "perf_cs_emulatedsfn_in_ns.txt",
          "perf_cs_emulatedipc_in_ns.txt",
         ]

# as milliseconds
files2 = [
          # trusted capture
          "perf_tc_untrusted_in_ns.txt",
          "perf_tc_trustedsfn_in_ns.txt",
          "perf_tc_trustedipc_in_ns.txt",
          "perf_tc_emulated_in_ns.txt",
          "perf_tc_emulatedsfn_in_ns.txt",
          "perf_tc_emulatedipc_in_ns.txt",

          # trusted delivery
          "perf_td_untrusted_in_ns.txt",
          "perf_td_trustedsfn_in_ns.txt",
          #"perf_td_trustedipc_in_ns.txt",
          "perf_td_emulated_in_ns.txt",
          "perf_td_emulatedsfn_in_ns.txt",
          #"perf_td_emulatedipc_in_ns.txt",
         ]


for f in files1:
    values = []
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

for f in files2:
    values = []
    with open(f, "r") as file:
    
        for line in file:
            value = float(line.strip())
            values.append(value)
    
        average_ns = np.mean(values)
        std_dev_ns = np.std(values)
    
        # Print the results
        print(f"For file: {f}")
        print(f"    Average: {average_ns * 10**-3 * 10**-3} ms")
        print(f"    Std.dev: {std_dev_ns * 10**-3 * 10**-3} ms")
        print(f"")
