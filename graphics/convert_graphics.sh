#!/bin/sh

a=assets
ase=ase

# all assets, mask layer should not be visible
assets="mainscreen game_background enemy_little_girl water_bomb_air
	bomb_splash player_all_frames enemy_raider enemy_grandma enemy_boss
	scene_lamp numbers_3x5 weapons powerups menu_drops characters_3x4"

# assets that have a mask layer
assets_w_mask="water_bomb_air"

# batch process all aseprite files to png/json files
for asset in $assets
do
	aseprite --batch $ase/${asset}.ase --sheet-type=vertical --sheet $a/${asset}.png --data ${asset}.json
done
for asset in $assets_w_mask
do
	aseprite --all-layers --batch --layer "mask" $ase/${asset}.ase --sheet-type vertical --sheet $a/${asset}_mask.png --data ${asset}_mask.json
done

# read all json files and convert them to C code
./convert.py

# copy C code images to source directory
cp images.* ../src
