#!/usr/bin/python

import glob
import numpy
import getopt, sys, json
from PIL import Image
from os.path import basename
from collections import OrderedDict

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
				if c == 15:
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

def write_image_as_comment(width, height, data, frame, f):
	f.write("/* [%u]" % frame)
	for h in range(height):
		f.write("\n * ")
		for w in range(width):
			c = data[(h * (width + 0)) + w]
			if c == 15:
				f.write("*")
			else:
				f.write("_")
	f.write("\n */\n")

def usage():
	print "..."

def chunk(seq, size):
	return [seq[i:i+size] for i in range(0, len(seq), size)]

def decode_json(jdata):
	frames = len(jdata["frames"])
	filename = jdata["meta"]["image"]
	img_width = jdata["meta"]["size"]["w"]
	img_height = jdata["meta"]["size"]["h"]
	fdata = jdata["frames"].values()[0]
	x = fdata["frame"]["x"]
	y = fdata["frame"]["y"]
	w = fdata["spriteSourceSize"]["w"]
	h = fdata["spriteSourceSize"]["h"]
	offset = w * h
	# assume all sprites have the same dimension
	return [filename, frames, img_width, img_height, w, h, offset]

outputfilename = "images"

if __name__ == "__main__":
	with open(outputfilename + ".c", 'w') as cfile, open(outputfilename + ".h", 'w') as hfile:
		hfile.write("#ifndef __CODE_H\n#define __CODE_H\n\n#include <stdint.h>\n\n")

		cfile.write("#ifndef HOST_TEST\n")
		cfile.write("#include <Arduino.h>\n")
		cfile.write("#else\n")
		cfile.write("#define PROGMEM\n")
		cfile.write("#endif\n")
		cfile.write("#include <stdint.h>\n")

		total_size = 0

		for json_filename in sorted(glob.glob("*.json")):
			img_name = basename(json_filename).split('.')[0] #json_filename[:-5]
			jdata = None
			with open(json_filename) as jdatafile:
				jdata = json.load(jdatafile, object_pairs_hook=OrderedDict)
			if not jdata:
				continue
			filename, frames, width, height, fw, fh, offset = decode_json(jdata)

			im = Image.open(filename)
#			print im.getcolors()
			palette= im.getpalette()
			colours = [bytes for bytes in chunk(palette, 3)]
#			print colours
#			width, height = im.size
			fdata = list(im.getdata())

			# create array of height cells each width elements
			a = numpy.reshape(numpy.asarray(fdata), (height, width))

			size = ((height + 7) / 8) * width + 2

			# print some information
			print "%-40s" % (filename),
			print "  img width: %3u img height: %3u" % (width, height),
			print "  frame width: %3u frame height: %3u" % (fw, fh),
			print "  frames: %3u" % (frames),
			print "  size: %5u" % (size)

			total_size += size

			hfile.write("extern const uint8_t %s_img[%u];\n" % (img_name, size))

			cfile.write("\n/* %s height = %u width = %u */\n" % (filename, fh, fw))
			cfile.write("const uint8_t %s_img[%u] PROGMEM = {\n" % (img_name, size))

			cfile.write("\t0x%2.2x, /* width */\n" % (fw))
			cfile.write("\t0x%2.2x, /* height */\n" % (fh))

			foffset = 0
			for frame in range(frames):
				foffset = frame * fw
				# copy each image and reshape to linear list
				img = numpy.reshape(a[:,foffset:foffset+fw], fw*fh).tolist()
				write_image_as_comment(fw, fh, img, frame, cfile)
				write_image(fw, fh, img, cfile)
				cfile.write("\n")

			cfile.write("};\n")
			cfile.write("\n/* total size %u bytes */\n" % total_size);
			#nr += 1

		hfile.write("\n/* total size %u bytes */\n" % total_size);
		hfile.write("\n#endif\n")

	print "total image data = %u bytes" % total_size
