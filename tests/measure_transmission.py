#!/usr/bin/python

# TODO see how many bytes are between two markers in a file
# TODO read out two values at first marker and second marker
# TODO compute bytes per second

# Initialize variables to store byte count and time in milliseconds

files = [ 
         # Trusted Capture
         "trans_tc_untrusted_in_ms_1.txt",
         "trans_tc_untrusted_in_ms_10.txt",
         "trans_tc_untrusted_in_ms_100.txt",
         "trans_tc_trusted_in_ms_1.txt",
         "trans_tc_trusted_in_ms_10.txt",
         "trans_tc_trusted_in_ms_100.txt",

         # Trusted Delivery
         "trans_td_untrusted_in_ms_1.txt",
         "trans_td_untrusted_in_ms_10.txt",
         "trans_td_untrusted_in_ms_100.txt",
         "trans_td_trusted_in_ms_1.txt",
         "trans_td_trusted_in_ms_10.txt",
         "trans_td_trusted_in_ms_100.txt",

         # Trusted Transformation
         "trans_tt_untrusted_in_ms_1.txt",
         "trans_tt_untrusted_in_ms_10.txt",
         "trans_tt_untrusted_in_ms_100.txt",
         "trans_tt_trusted_in_ms_1.txt",
         "trans_tt_trusted_in_ms_10.txt",
         "trans_tt_trusted_in_ms_100.txt",

         # Trusted Handle
         "trans_th_untrusted_in_ms_1.txt",
         "trans_th_untrusted_in_ms_10.txt",
         "trans_th_untrusted_in_ms_100.txt",
         "trans_th_trusted_in_ms_1.txt",
         "trans_th_trusted_in_ms_10.txt",
         "trans_th_trusted_in_ms_100.txt",
         ]

# Open the text file for reading
for f in files:
    byte_count = 0
    time_ms = 0
    with open(f, 'rb') as file:
        lines = file.readlines()
    
        # Iterate through each line in the file
        for line in lines[1:-1]:
            # Increment the byte count by the length of the line
            byte_count += len(line)
    
        # get transmission tiem from the last line
        last_line = lines[-1].decode('utf-8')
        if 'MEASURING END WITH:' in last_line:
            # Extract the time value from the line using string manipulation
            time_ms = int(last_line.split()[-2])
    
        # compute bytes per second
        byte_count = byte_count - 2;
        bps = byte_count / (time_ms / 1000)
        print(f'For {f}:')
        print(f'    Bytes sent:       {byte_count}')
        print(f'    Time to send:     {time_ms}')
        print(f'    Bytes per second: {bps}')
        print("")
