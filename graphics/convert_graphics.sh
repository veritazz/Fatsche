#!/bin/sh

./convert.py -o images  -v 1 -h 1 -f mainscreen.png \
			-v 1 -h 1 -f game_background.png \
			-v 10 -h 1 -f numbers.png \
			-v 4 -h 1 -f water_bomb_air.png \
			-v 4 -h 1 -f water_bomb_impact.png \
			-v 4 -h 1 -f player.png \
			-v 16 -h 1 -f player_all_frames.png \
			-v 16 -h 1 -f enemy1_all_frames.png \
			-v 16 -h 1 -f enemy_raider.png \
			-v 8 -h 1 -f enemy_grandma.png \
			-v 12 -h 1 -f enemy_boss.png \
			-v 2 -h 1 -f scene_lamp.png \
			-v 10 -h 1 -f numbers_3x5.png
