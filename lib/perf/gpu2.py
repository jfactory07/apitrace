import fileinput
import sys

def read_perf(filename):
  total = 0
  frameNo = 1
  for line in fileinput.input([filename]):
	data = line.split();
        #print data[0]
	if data[0] == "call":
	   total += int(data[3])
	elif data[0] == "frame_end":
	   print "frame " + str(frameNo) +" :"
	   print "gpu time: " + str(total)
	   total = 0
	   frameNo = frameNo +1
  
target = sys.argv[1]
read_perf(target)

