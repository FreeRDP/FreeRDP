#!/usr/bin/python

# Copyright 2011 Anthony Tong <atong@trustedcs.com>

# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""
tool to preconvert an icon file to a x11 property as expected
by ewm hints spec:
width, length, [argb pixels]
"""

import PIL
import PIL.Image

import os
import sys

def usage():
	print "convert_to_ewm_prop <infile> <outfile>"
	return 1

def main(argv):
	if len(argv) != 3:
		return usage()

	im = PIL.Image.open(argv[1])
	fp = open(argv[2], 'w')

	var_name = os.path.basename(argv[2])
	if var_name.endswith('.h'):
		var_name = var_name[:-2]

	fp.write("static unsigned long %s_prop [] = {\n" % var_name)
	fp.write(" %d, %d\n" % im.size)

	i = 0
	for pixel in im.getdata():
		r,g,b,a = pixel
		pixel = b 
		pixel |= g << 8
		pixel |= r << 16
		pixel |= a << 24
		fp.write(" , %du" % pixel)

		i += 1
		if i % 8 == 0:
			fp.write("\n")

	fp.write("};\n")

if __name__ == '__main__':
	sys.exit(main(sys.argv))

