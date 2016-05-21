#!/usr/bin/python

import glob
import numpy
import getopt, sys, json
from PIL import Image
from os.path import basename
from collections import OrderedDict
from operator import itemgetter

def print_hex_array(f_data, f, width=1):
	size = len(f_data)
	f.write("\t")
	for b in range(size):
		if width == 1:
			f.write("0x%2.2x," % f_data[b])
		else:
			f.write("0x%2.2x," % (f_data[b] & 0xff))
			f.write(" ")
			f.write("0x%2.2x," % (f_data[b] >> 8))
		if b == size - 1:
			break
		if (b + 1) % (12 / width) == 0:
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

data_dictionary = {}

def update_dictionary(frame, f, dictionary):
	for b in frame:
		try:
			nr = dictionary[b]
		except KeyError:
			nr = 0
		nr += 1
		dictionary[b] = nr

def add_byte(data, bits, byte):
	if bits == 8:
		data.append(byte)
		bits = 0
		byte = 0
	return (bits, byte)

def pack_data(data, bits, byte, to_pack):
	for tp in to_pack:
		bits, byte = add_byte(data, bits, byte)
		byte = (byte << bits) | tp
		bits += 4
	bits, byte = add_byte(data, bits, byte)
	return (bits, byte)

###############################################################################
# packed format
#
# 4bits are encoded the following way:
#    0xf    : special token, next 8bit are raw data
#    0xe    : special token for repetitive patterns
#             next 8bit are the repeat count, following 8bit is the value to
#             repeat
#    0xd    : special token to indicate the next 8bit is a repeat count
#             followed by a 4bit value to repeat
#    0xc    : special token for repetitive patterns
#             next 8bit is the number of raw encoded values
#    0xc-0x0: index into a table that holds the data
#
###############################################################################
def find_single_repeats(rle_data):
	single_repeats = {}
	last_byte = rle_data[0]
	count = 1
	sindex = 0
	for index in range(1, len(rle_data)):
		byte = rle_data[index]
		if byte != last_byte:
			if count > 1:
				single_repeats[sindex] = (last_byte, count)
			count = 1
			last_byte = byte
			sindex = index
			continue
		count += 1
	if count > 1:
		single_repeats[sindex] = (last_byte, count)
	return single_repeats

def create_packed_image(rle_data, f, l1_table):
	bits = 0
	byte = 0
	packed_data = []
	single_repeats = find_single_repeats(rle_data)
	index = 0
	while index < len(rle_data):
		special = 0
		rle_byte = rle_data[index]
		try:
			encoded = l1_table.index(rle_byte)
		except ValueError:
			encoded = 0xf

		if index in single_repeats.keys():
			r = single_repeats[index]
			if encoded == 0xf:
				# repeat should be at least 2
				if r[1] >= 3:
					special = 1
					to_pack = [0xe, r[1] >> 4, r[1] & 0xf, rle_byte >> 4, rle_byte & 0xf]
					index += r[1] - 1

			if encoded != 0xf:
				if r[1] >= 4:
					special = 1
					to_pack = [0xd, r[1] >> 4, r[1] & 0xf, encoded]
					index += r[1] - 1
		else:
			# check if we can mark multiple raw values that are not
			# in the l1 table
			index2 = 0
			while index2 < (len(rle_data) - index):
				dummy_byte = rle_data[index2 + index]
				if dummy_byte in l1_table:
					break
				index2 += 1
			if index2 > 2:
				special = 1
				to_pack = [0xc, index2 >> 4, index2 & 0xf]
				for value in rle_data[index:index+index2]:
					to_pack.append(value >> 4)
					to_pack.append(value & 0xf)
				index += index2 - 1

		# no special token matches, just pack normally
		if not special:
			if encoded == 0xf:
				to_pack = [encoded, rle_byte >> 4, rle_byte & 0xf]
			else:
				to_pack = [encoded]

		bits, byte = pack_data(packed_data, bits, byte, to_pack)
		index += 1

	if bits != 0:
		packed_data.append(byte << (8 - bits))
	return packed_data


outputfilename = "images"

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
			images[img_name]["packed"] = {}

			foffset = 0
			previous_frame = None
			for frame_nr in range(frames):
				foffset = frame_nr * fw
				# copy each image and reshape to linear list
				img = numpy.reshape(a[:,foffset:foffset+fw], fw*fh).tolist()
				images[img_name]["raw"][frame_nr] = img
				frame = convert_image(fw, fh, img, color)
				update_dictionary(frame, cfile, data_dictionary)
				images[img_name]["target"][frame_nr] = frame

		sorted_dict = sorted(data_dictionary.items(), key=itemgetter(1), reverse = True)
		dict_len = len(data_dictionary.keys())

		# create tables
		l1_table = []
		table_len = 12 if dict_len > 12 else dict_len
		for l1 in range(table_len):
			l1_table.append(sorted_dict[l1][0])

		# save l1 translation table
		hfile.write("extern const uint8_t l1_table[%u];\n" % (table_len))
		cfile.write("\n")
		cfile.write("const uint8_t l1_table[%u] PROGMEM = {\n" % (table_len))
		print_hex_array(l1_table, cfile)
		cfile.write("\n};\n")

		packed_total_size = 0
		for k, v in images.iteritems():
			h = v["info"][2]
			w = v["info"][3]

			frame_offsets = []
			offset = 0
			inc = 0
			isize = 0
			offset += 2 + len(v["target"].keys()) * 2
			for k2, v2 in v["target"].iteritems():
				mask = 0
				packed = create_packed_image(v["target"][k2], cfile, l1_table)
				if len(packed) < len(v["target"][k2]):
					v["packed"][k2] = packed
					mask = 0x8000
					inc = len(packed)
				else:
					inc = len(v["target"][k2])
				frame_offsets.append(offset | mask)
				offset += inc
				isize += inc

#			frame_offsets.append((offset & ~0x8000) | mask)

			isize += 2
			isize += len(frame_offsets) * 2

			hfile.write("extern const uint8_t %s_img[%u];\n" % (k, isize)) #v["info"][1]))
			cfile.write("\n/* %s height = %u width = %u */\n" % (v["info"][0], h, w))
			cfile.write("const uint8_t %s_img[%u] PROGMEM = {\n" % (k, isize)) #v["info"][1]))
			cfile.write("\t0x%2.2x, /* width */\n" % (w))
			cfile.write("\t0x%2.2x, /* height */\n" % (h))

			print_hex_array(frame_offsets, cfile, 2)
			cfile.write("\n")

			packed_total_size += 2 + len(frame_offsets) * 2
			for k2, v2 in v["raw"].iteritems():
				write_image_as_comment(w, h, v2, k2, cfile, color)

				try:
					print_hex_array(v["packed"][k2], cfile)
					packed_total_size += len(v["packed"][k2])
				except KeyError:
					print_hex_array(v["target"][k2], cfile)
					packed_total_size += len(v["target"][k2])
				cfile.write("\n")

			cfile.write("};\n")

		cfile.write("\n/* total size %u bytes */\n" % total_size);
		hfile.write("\n/* total size %u bytes */\n" % total_size);
		hfile.write("\n#endif\n")

	print "total image data         = %u bytes" % total_size
	print "total image data packed  = %u bytes (%u%%)" % (packed_total_size, packed_total_size * 100 / total_size)
