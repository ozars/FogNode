#!/usr/bin/python

import os
import sys
import re

class bcolors:
    PURPLE = '\033[95m'
    BLUE = '\033[94m'
    GREEN = '\033[92m'
    YELLOW = '\033[93m'
    FAIL = '\033[91m'
    ENDC = '\033[0m'
    BOLD = '\033[1m'
    UNDERLINE = '\033[4m'

expr = sys.argv[1]
files = sys.argv[2:]
matches = []
regex = re.compile('^(?:RECEIVED: )?\[(\d{2}:\d{2}:\d{2}.\d{3}.\d{3})\](?:\[.+?\])?\s+(.*)')

for filename in files:
    with open(filename, 'r') as f:
        line = f.readline()
        while line != '':
            if line.lower().find(expr.lower()) > 0:
                line = line.strip()
                m = regex.search(line)
                if m != None:
                    timestamp = m.group(1)
                    msg = m.group(2)
                    matches.append([timestamp, os.path.basename(filename), msg])
            line = f.readline()
prev = None
matches = sorted(matches, cmp = lambda x, y: 0 if x[0] == y[0] else (-1 if x[0] < y[0] else 1))
for i in xrange(1, len(matches)-1):
    if matches[i][0] == matches[i+1][0] and matches[i-1][1] == matches[i+1][1]:
        t = matches[i]
        matches[i] = matches[i+1]
        matches[i+1] = t

for entry in matches:
    if entry[1] != prev:
        print bcolors.YELLOW + "\n=>", entry[1]
    print bcolors.PURPLE + "[" + entry[0] + "]", bcolors.GREEN+entry[2]
    prev = entry[1]
print bcolors.ENDC,
