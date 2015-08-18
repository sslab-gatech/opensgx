#!/usr/bin/python

import sys

in_name = sys.argv[1]
out_name = sys.argv[2]

input = open(in_name, "r")
output = open(out_name, "w")

for line in input:
	s = line.split(' ')
	for p in s:
		if p.find(':') != -1:
			output.write(p + '\n')

