#!/usr/bin/python

import fileinput
import sys,getopt
import operator

def prinf_dict(dictdata):
    sorted_x = sorted(dictdata.items(), key=operator.itemgetter(1))
    sorted_x.reverse()

    for item in sorted_x:
	print item;   


def read_perf(filename, rs, re):
  total = 0
  frameNo = 1
  framedata ={}
  inRange = 0
  for line in fileinput.input([filename]):
	data = line.split();
        #print data[0]
	if data[0] == "call":
	   if int(data[5]) > 0:
	      #framedata[str(data[12])] += int(data[5]);		
	      if framedata.has_key(data[12]):
		 framedata[data[12]] += int(data[5]);		
	      else: 	
		framedata[data[12]] = int(data[5]);		
	   if int(data[1]) >= rs  and int(data[1]) <= re:
		inRange = 1;
        elif data[0] == "frame_end:":
  	   prinf_dict(framedata);
  	   framedata ={}
	   if inRange == 0:
	      print "FID " + str(frameNo) +" "+line
	   else:   
              print "FID xxx" + str(frameNo) +" "+line
	   frameNo = frameNo + 1;
	   inRange = 0

print(sys.argv)

opts, args = getopt.getopt(sys.argv[1:],'f:s:e:')

filename =''
rs = 10000000
re = -1

for opt, arg in opts:
    if opt == '-f':
	filename = arg
    elif opt == '-s':
	rs = int(arg)
    elif opt == '-e':
	re = int(arg)

read_perf(filename, rs, re)

