#!/bin/sh

a=assets

./convert.py -o images  -v 1  -h 1 -f $a/mainscreen.png \
			-v 1  -h 1 -f $a/game_background.png \
			-v 4  -h 1 -f $a/water_bomb_air.png \
			-v 4  -h 1 -f $a/water_bomb_air_mask.png \
			-v 4  -h 1 -f $a/bomb_splash.png \
			-v 12 -h 1 -f $a/player_all_frames.png \
			-v 16 -h 1 -f $a/enemy_raider.png \
			-v 8  -h 1 -f $a/enemy_grandma.png \
			-v 12 -h 1 -f $a/enemy_boss.png \
			-v 2  -h 1 -f $a/scene_lamp.png \
			-v 10 -h 1 -f $a/numbers_3x5.png \
			-v 4  -h 1 -f $a/weapons.png \
			-v 8  -h 1 -f $a/powerups.png \
			-v 12 -h 1 -f $a/menu_drops.png \
			-v 1  -h 1 -f $a/characters_3x4.png
