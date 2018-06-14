#! /usr/bin/python
import sys
fh=sys.stdin
rtt_list = []
for line in fh.readlines() :
  if ( len(line.split()) == 0 ) :
    continue
  elif ( line.split()[1] == "bytes" ):
    rtt=float(line.split()[6].split("=")[1])
    rtt_list += [ rtt ]
rtt_list.sort()
print sys.argv[1],"th percentile", rtt_list [ int( (float(sys.argv[1])/100.0) * len(rtt_list) ) ]
