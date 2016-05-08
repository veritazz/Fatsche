#!/usr/bin/python

import glob
import numpy
import getopt, sys, json
from PIL import Image
from os.path import basename
from collections import OrderedDict
from operator import itemgetter

def print_hex_array(f_data, f):
	size = len(f_data)
	f.write("\t")
	for b in range(size):
		f.write("0x%2.2x," % f_data[b])
		if b == size - 1:
			break
		if (b + 1) % 12 == 0:
			f.write("\n\t")
		else:
			f.write(" ")


def convert_image(width, height, data, color):
	size = (width * ((height + 7) / 8 * 8)) / 8
	f_data = []
	for b in range(size):
		f_data.append(0)
	rows = (height + 7) / 8
	o = 0
	for row in range(rows):
		for w in range(width):
			for h in range(8):
				if (h + row * 8) >= height:
					break
				c = data[(h + row * 8) * width + w]
				if c == color:
					f_data[o] = f_data[o] | (0x1 << h)
			o += 1

	return f_data

def write_image_as_comment(width, height, data, frame, f, color):
	f.write("/* [%u]" % frame)
	for h in range(height):
		f.write("\n * ")
		for w in range(width):
			c = data[(h * (width + 0)) + w]
			if c == color:
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

def diff_frame(previous, current, f):
	nr_bytes = len(previous)
	diff = [None] * nr_bytes
	for i in range(nr_bytes):
		p = previous[i]
		c = current[i];
		if p < c:
			d = c - p
		else:
			d = p - c
		diff[i] = d

def convert_image_rle_packed(width, height, data, f):
	f_data = []
	distances = []
	max_distance = 0
	distance = 0
	state = 0
	nr_bytes = len(data)
	mask = 0x1
	for i in range(nr_bytes * 8):
		c = data[i / 8]
		o = 1 if c & mask else 0
		if o == state:
			distance += 1
		else:
			distances.append(distance)
			max_distance = max_distance if distance < max_distance else distance
			distance = 1
			state = 0 if state else 1
		mask = mask << 1
		if mask == 0x100:
			mask = 0x1
	distances.append(distance)
	max_distance = max_distance if distance < max_distance else distance
	rle_size = 1 if max_distance < 255 else 2
	if rle_size == 2:
		print "max distance %u" % (max_distance)

	return distances

dictionary = {}

def update_dictionary(frame, f):
	for b in frame:
		try:
			nr = dictionary[b]
		except KeyError:
			nr = 0
		nr += 1
		dictionary[b] = nr

def create_packed_image(rle_data, f, l1_table):
	bits = 0
	byte = 0
	packed_data = []
	for rle_byte in rle_data:
		try:
			encoded = l1_table[rle_byte]
		except IndexError:
			encoded = 0xf
		if bits == 8:
			packed_data.append(byte)
			bits = 0
			byte = 0
		byte = (byte << bits) | encoded
		bits += 4
		if bits == 8:
			packed_data.append(byte)
			bits = 0
			byte = 0
		if encoded == 0xf:
			byte = (byte << bits) | ((rle_byte >> 4) & 0xf)
			bits += 4
			if bits == 8:
				packed_data.append(byte)
				bits = 0
				byte = 0
			byte = (byte << bits) | (rle_byte & 0xf)
			bits += 4
			if bits == 8:
				packed_data.append(byte)
				bits = 0
				byte = 0
	if bits != 0:
		packed_data.append(byte << 4)
	return packed_data


outputfilename = "xxx"

images = {}

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

			if "_mask" in img_name:
				color = 14
				print "%-40s" % (filename + "[mask]"),
			else:
				color = 15
				print "%-40s" % (filename),

			# print some information
			print "  img width: %3u img height: %3u" % (width, height),
			print "  frame width: %3u frame height: %3u" % (fw, fh),
			print "  frames: %3u" % (frames),
			print "  size: %5u" % (size)

			total_size += size

			images[img_name] = {"info": (filename, size, fh, fw)}
			images[img_name]["raw"] = {}
			images[img_name]["target"] = {}
			images[img_name]["rle"] = {}

			foffset = 0
			for frame_nr in range(frames):
				foffset = frame_nr * fw
				# copy each image and reshape to linear list
				img = numpy.reshape(a[:,foffset:foffset+fw], fw*fh).tolist()
				frame = convert_image(fw, fh, img, color)
				rle_frame = convert_image_rle_packed(fw, fh, frame, cfile)
				update_dictionary(rle_frame, cfile)

				images[img_name]["raw"][frame_nr] = img
				images[img_name]["target"][frame_nr] = frame
				images[img_name]["rle"][frame_nr] = rle_frame

		sorted_dict = sorted(dictionary.items(), key=itemgetter(1), reverse = True)
		for t in sorted_dict:
			print "key: 0x%2.2x value: %u" % (t[0], t[1])
		print "len of dictionary: %u" % (len(dictionary.keys()))
		dict_len = len(dictionary.keys())
		# create tables
		# XXX
		l1_table = []
		for l1 in range(15):
			l1_table.append(sorted_dict[l1][0])
		print "rest to encode: %u" % (dict_len - 15)

		packed_total_size = 0
		rle_total_size = 0
		for k, v in images.iteritems():
			h = v["info"][2]
			w = v["info"][3]
			hfile.write("extern const uint8_t %s_img[%u];\n" % (k, v["info"][1]))
			cfile.write("\n/* %s height = %u width = %u */\n" % (v["info"][0], h, w))
			cfile.write("const uint8_t %s_img[%u] PROGMEM = {\n" % (k, v["info"][1]))
			cfile.write("\t0x%2.2x, /* width */\n" % (h))
			cfile.write("\t0x%2.2x, /* height */\n" % (w))

			packed_total_size += 2
			rle_total_size += 2
			for k2, v2 in v["raw"].iteritems():
				write_image_as_comment(w, h, v2, k2, cfile, color)

				cfile.write("\n:--- raw %u\n" % len(v2))
				print_hex_array(v2, cfile)
				cfile.write("\n:--- target %u\n" % len(v["target"][k2]))
				print_hex_array(v["target"][k2], cfile)
				cfile.write("\n:--- rle %u\n" % len(v["rle"][k2]))
				print_hex_array(v["rle"][k2], cfile)
				rle_total_size += len(v["rle"][k2])

				packed = create_packed_image(v["rle"][k2], cfile, l1_table)
				cfile.write("\n:--- packed %u\n" % len(packed))
				print_hex_array(packed, cfile)
				cfile.write("\n:---:\n")
				packed_total_size += len(packed)

			cfile.write("};\n")
			cfile.write("\n/* total size %u bytes */\n" % total_size);
			cfile.write("\n")

		hfile.write("\n/* total size %u bytes */\n" % total_size);
		hfile.write("\n#endif\n")

	print "total image data        = %u bytes" % total_size
	print "total image data rle    = %u bytes" % rle_total_size
	print "total image data packed = %u bytes" % packed_total_size
