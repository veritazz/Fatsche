#!/bin/sh

a=assets
ase=ase

# all assets, mask layer should not be visible
assets="mainscreen"

# assets that have a mask layer
assets="mainscreen game_background enemy_little_girl water_bomb_air
	bomb_splash player_all_frames enemy_raider enemy_grandma enemy_boss
	scene_lamp numbers_3x5 weapons powerups menu_drops characters_3x4
	poison_damage
	enemy_drunken_punk
	enemy_dummy2
	enemy_boss_dummy1
	arduboy_logo
	text_game_over
	text_boss_time
	help_screen
	characters_13x16
"

# assets that have a mask layer
assets_w_mask="water_bomb_air
	       enemy_raider
	       enemy_drunken_punk
	       enemy_little_girl
	       enemy_grandma
	       enemy_boss
	       powerups
	       enemy_dummy2
	       enemy_boss_dummy1
"

# batch process all aseprite files to png/json files
for asset in $assets
do
	aseprite --batch $ase/${asset}.ase --sheet-type=vertical --sheet $a/${asset}.png --data ${asset}.json
done
for asset in $assets_w_mask
do
	cp ${asset}.json ${asset}_mask.json
done

# read all json files and convert them to C code
./conpack.py

# copy C code images to source directory
cp images.* ../src
