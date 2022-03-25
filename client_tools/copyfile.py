#!/usr/bin/python

import os
import sys
import time

targetfile="/tmp/result.csv"

def countLines():

   count=-1
   for count,line in enumerate(open(targetfile,'rU').readlines()):
       count += 1
   print(count)
   return count


os.system ("ssh orthogonal 'rm -rf /tmp/result.csv && touch /tmp/result.csv' ")


lastLen=0
currentLen=0

cnt=0  ### if more than 5 seconds keep same, then break

while ( cnt <= 5 ) :

    currentLen = countLines()
    currentLen = currentLen-1  #### the last line might not finished with \n
    print ( currentLen )

    if ( currentLen <=0 ):
        time.sleep(1)
        continue

    if currentLen == lastLen:
        cnt = cnt+1
    else:
        os.system ("sed -n '%d,%dp'  /tmp/result.csv > /tmp/update.csv && scp /tmp/update.csv orthogonal:/tmp " % (lastLen+1, currentLen) )
        print ( 'transfered from %d to %d' % (lastLen+1, currentLen))
        os.system ("ssh orthogonal 'cat /tmp/update.csv >> /tmp/result.csv'" )
        lastLen = currentLen

    time.sleep(1)


currentLen = countLines()
os.system ("sed -n '%d,%dp'  /tmp/result.csv > /tmp/update.csv && scp /tmp/update.csv orthogonal:/tmp " % (currentLen, currentLen) )
print ( 'transfered from %d to %d' % (currentLen, currentLen))
os.system ("ssh orthogonal 'cat /tmp/update.csv >> /tmp/result.csv'" )


print (" Copy file done ")
