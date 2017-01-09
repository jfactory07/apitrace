import fileinput
import sys

def read_perf(filename):
  total = 0
  frameNo = 1
  for line in fileinput.input([filename]):
	data = line.split();
        #print data[0]
	if data[0] == "call":
	   if int(data[3]) > 0:
	      print line
	elif data[0] == "frame_end":
 	   print line
 
target = sys.argv[1]
read_perf(target)

