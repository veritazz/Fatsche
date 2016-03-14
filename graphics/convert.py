#!/usr/bin/python

import getopt, sys
from PIL import Image

def find_all(a_str, sub):
	start = 0
	while True:
		start = a_str.find(sub, start)
		if start == -1: return
		yield start
		start += len(sub)

def write_image(width, height, data, f):
	size = (width * ((height + 7) / 8 * 8)) / 8
	f_data = []
	for b in range(size):
		f_data.append(0)
	f.write("\t")
	rows = (height + 7) / 8
	o = 0
	for row in range(rows):
		for w in range(width):
			for h in range(8):
				if (h + row * 8) >= height:
					break
				c = data[(h + row * 8) * width + w]
				if c:
					f_data[o] = f_data[o] | (0x1 << h)
			o += 1

	for b in range(size):
		f.write("0x%2.2x," % f_data[b])
		if b == size - 1:
			break
		if (b + 1) % 12 == 0:
			f.write("\n\t")
		else:
			f.write(" ")

def write_image_as_comment(width, height, data, f):
	f.write("/*")
	for h in range(height):
		f.write("\n * ")
		for w in range(width):
			c = data[(h * (width + 0)) + w]
			if c:
				f.write("*")
			else:
				f.write("_")
	f.write("\n */\n")

def usage():
	print "..."

def chunk(seq, size):
	return [seq[i:i+size] for i in range(0, len(seq), size)]

outputfilename = "code"

if __name__ == "__main__":
	nr_images = 0
	images = []
	image = [0, 0, None]
	try:
		opts, args = getopt.getopt(sys.argv[1:], "o:dv:h:f:", ["help", "output="])
	except getopt.GetoptError:
		usage()
		sys.exit(2)
	for opt, arg in opts:
		if opt == "--help":
			usage()
			sys.exit()
		elif opt == '-d':
			global _debug
			_debug = 1
		elif opt in ("-o", "--output"):
			outputfilename = arg
		elif opt == '-v':
			image[0] = int(arg)
		elif opt == '-h':
			image[1] = int(arg)
		elif opt == '-f':
			image[2] = arg
		if image[0] and image[1] and image[2]:
			nr_images += 1
			images.append(image)
			image = [0, 0, None]

	print images

	with open(outputfilename + ".c", 'w') as cfile, open(outputfilename + ".h", 'w') as hfile:
		hfile.write("#ifndef __CODE_H\n#define __CODE_H\n\n#include <stdint.h>\n\n")

		cfile.write("#ifndef HOST_TEST\n")
		cfile.write("#include <Arduino.h>\n")
		cfile.write("#else\n")
		cfile.write("#define PROGMEM\n")
		cfile.write("#endif\n")
		cfile.write("#include <stdint.h>\n")

		total_size = 0
		for v, h, filename in images:

			im = Image.open(filename)
#			print im.getcolors()
			palette= im.getpalette()
			colours = [bytes for bytes in chunk(palette, 3)]
#			print colours
			width, height = im.size
			fdata = list(im.getdata())

			print filename + ": height = %u width = %u" % (height, width)

			rwidth = width / h
			rheight = height / v

			size = ((rheight + 7) / 8) * rwidth * h * v + 2

			total_size += size

			hfile.write("extern const uint8_t %s_img[%u];\n" % (filename.split('.')[0], size))

			cfile.write("\n/* %s height = %u width = %u */\n" % (filename, height, width))
			write_image_as_comment(width, height, fdata, cfile)
			cfile.write("const uint8_t %s_img[%u] PROGMEM = {\n" % (filename.split('.')[0], size))

			cfile.write("\t0x%2.2x, /* width */\n" % (rwidth))
			cfile.write("\t0x%2.2x, /* height */\n" % (rheight))

			offset = 0
			for himages in range(h):
				for vimages in range(v):
					print offset, rwidth, rheight
					write_image(rwidth, rheight, fdata[offset:offset+rwidth*rheight], cfile)
					cfile.write("\n")
					offset += rwidth * rheight

			cfile.write("};\n")
			cfile.write("\n/* total size %u bytes */\n" % total_size);
			#nr += 1

		hfile.write("\n/* total size %u bytes */\n" % total_size);
		hfile.write("\n#endif\n")

	print "total image data = %u bytes" % total_size
