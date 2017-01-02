import fileinput
import sys

def read_perf(filename, st, ed):
  total = 0
  for line in fileinput.input([filename]):
     if fileinput.lineno() >= int(st) and fileinput.lineno() <= int(ed):
        data = line.split();
        total += int(data[3])
  return total

  
st = sys.argv[1]
ed = sys.argv[2]
print 'check: ' + str(st) +' '+str(ed);
total = read_perf('/media/temp/frame_amd.txt', st, ed)

print 'total_amd  ' + str(total) 

total = read_perf('/media/temp/frame_mesa.txt', st, ed)

print 'total_mesa ' + str(total) 
