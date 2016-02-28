#ifdef HOST_TEST
#include "host_test.h"
#else
#include <Arduino.h>
#include <avr/pgmspace.h>
#include "Arduboy.h"
#include "ArduboyExtra.h"
#include "simple_buttons.h"

Arduboy arduboy;
SimpleButtons buttons(arduboy);
#endif

#include <stdint.h>
#include "images.h"

/* frame rate of the game */
#define FPS                         30

enum main_states {
	MAIN = 0,
	LOAD,
	RUN,
	HELP,
};

/* state of the program */
static uint8_t main_state = MAIN;

/*---------------------------------------------------------------------------
 * graphic functions
 *---------------------------------------------------------------------------*/
#define __flag_none                  (0)
#define __flag_color_invert          (1 << 0)
#define __flag_h_mirror              (1 << 1)
#define __flag_v_mirror              (1 << 2)
#define __flag_0_transparent         (1 << 3)
#define __flag_1_transparent         (1 << 4)

#ifndef HOST_TEST
static void
blit_image(uint8_t x, uint8_t y, const uint8_t *img, uint8_t flags)
{
	arduboy.drawBitmap(x,
			   y,
			   img + 2,
			   pgm_read_byte_near(img), /* width */
			   pgm_read_byte_near(img + 1), /* height */
			   WHITE);
}

static void
blit_image_frame(uint8_t x, uint8_t y, const uint8_t *img, uint8_t nr, uint8_t flags)
{
	uint8_t w, h;

	w = pgm_read_byte_near(img);
	h = pgm_read_byte_near(img + 1);

	arduboy.drawBitmap(x,
			   y,
			   img + (w * ((h + 7) / 8) * nr) + 2,
			   w, /* width */
			   h, /* height */
			   WHITE);
}
#endif

/*---------------------------------------------------------------------------
 * inputs
 *---------------------------------------------------------------------------*/
#ifdef HOST_TEST
static uint8_t input_current = 0;

/* input states */
enum KEYS {
	KEY_UP = 1,
	KEY_DOWN = 2,
	KEY_LEFT = 4,
	KEY_RIGHT = 8,
	KEY_A = 16,
	KEY_B = 32,
};

static void
update_inputs(void)
{
	input_current = get_inputs();
}

uint8_t a(void)
{
	return !!(input_current & KEY_A);
}
uint8_t b(void)
{
	return !!(input_current & KEY_B);
}
uint8_t up(void)
{
	return !!(input_current & KEY_UP);
}
uint8_t down(void)
{
	return !!(input_current & KEY_DOWN);
}
uint8_t left(void)
{
	return !!(input_current & KEY_LEFT);
}
uint8_t right(void)
{
	return !!(input_current & KEY_RIGHT);
}
#else
uint8_t a(void)
{
	return buttons.a();
}
uint8_t b(void)
{
	return buttons.b();
}
uint8_t up(void)
{
	return buttons.up();
}
uint8_t down(void)
{
	return buttons.down();
}
uint8_t left(void)
{
	return buttons.left();
}
uint8_t right(void)
{
	return buttons.right();
}
#endif

/*---------------------------------------------------------------------------
 * timing
 *---------------------------------------------------------------------------*/

static uint8_t next_frame(void)
{
#ifdef HOST_TEST
	static unsigned long current_time = 0;
	uint8_t new_frame = 0;
	unsigned long ntime = millis();

	if (ntime > current_time) {
		current_time = ntime + (1000 / FPS);
		new_frame = 1;
		update_inputs();
	}

	{
		static unsigned int frames = 0;
		if (new_frame) {
			frames++;
			printf("ms per frame %u ntime %lu current_time %lu frames %u\n",
			       1000 / FPS,
			       ntime,
			       current_time,
			       frames);
		}
	}

	return new_frame;
#else
	if (arduboy.nextFrame()) {
		buttons.poll();
		return 1;
	}
	return 0;
#endif
}

static void finish_frame(void)
{
#ifndef HOST_TEST
	arduboy.display();
#endif
}

/*---------------------------------------------------------------------------
 * setup
 *---------------------------------------------------------------------------*/
#ifdef __cplusplus
extern "C"
{
#endif
void
setup(void)
{
#ifndef HOST_TEST
	arduboy.setFrameRate(FPS);
	arduboy.begin();
	/* TODO check */
	arduboy.clear();
	arduboy.display();
#else
#endif
}
#ifdef __cplusplus
}
#endif
/*---------------------------------------------------------------------------
 * main menu handling
 *---------------------------------------------------------------------------*/
/*
 *  bit[0]   = 0 mainscreen not drawn, 1 already drawn
 *  bit[1:3] = current frame number of animation
 *  bit[4]   = 0 animation frame not drawn, 1 already drawn
 *  bit[5:6] = selected menu item
 */
static uint8_t menu_state = 0;

enum menu_item {
	MENU_ITEM_PLAY  = 0,
	MENU_ITEM_LOAD,
	MENU_ITEM_HELP,
	MENU_ITEM_CONTINUE,
	MENU_ITEM_SAVE,
};

const uint8_t menu_item_xlate[] PROGMEM = {
	[MENU_ITEM_PLAY] = RUN,
	[MENU_ITEM_LOAD] = LOAD,
	[MENU_ITEM_HELP] = HELP,
	[MENU_ITEM_CONTINUE] = RUN,
	[MENU_ITEM_SAVE] = MAIN,
};

#define MENU_ITEM_X               20
#define MENU_ITEM_DISTANCE        15

const uint8_t menu_item_selected_img[18] = {
	16, /* width */
	8, /* height */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
};
const uint8_t menu_item_delete_img[18] = {
	16, /* width */
	8, /* height */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static int
mainscreen(void)
{
	uint8_t frame = (menu_state >> 1) & 0x7;
	uint8_t game_state = MAIN;
	uint8_t needs_update = 1;
	uint8_t menu_item = (menu_state >> 5) & 0x3;
	static uint8_t next_frame = FPS; /* updates of the animation */

	/* print main screen */
	if (!(menu_state & 1)) {
		blit_image(0, 0, mainscreen_img, __flag_none);

		/* draw new item highlighted */
		blit_image(MENU_ITEM_X,
			   menu_item * MENU_ITEM_DISTANCE,
			   menu_item_selected_img,
			   __flag_none);

		menu_state |= 1;
		next_frame = FPS;
	}

	/* TODO check user inputs if another menu item was selected */
	if (up()) {
		if (!menu_item)
			menu_item = 3;
		else
			menu_item--;
	} else if (down()) {
		if (menu_item == 3)
			menu_item = 0;
		else
			menu_item++;
	} else if (a()) {
		/* select/execute item */
		game_state = pgm_read_byte_near(&menu_item_xlate[menu_item]);
	} else if (b()) {
		/* cancel/back */
	} else {
		needs_update = 0;
	}

	/* highlight selected menu item */
	if (needs_update) {
		/* draw old item with no highlight */
		blit_image(MENU_ITEM_X,
			   ((menu_state >> 5) & 3) * MENU_ITEM_DISTANCE,
			   menu_item_delete_img,
			   __flag_1_transparent);
		/* draw new item highlighted */
		blit_image(MENU_ITEM_X,
			   menu_item * MENU_ITEM_DISTANCE,
			   menu_item_selected_img,
			   __flag_none);
		menu_state &= ~(0x3 << 5);
		menu_state |= menu_item << 5;
	}

	/* update animation on the right side of the screen */
	if (!(menu_state & (1 << 4))) {
	// HACK	blit_image_frame(80, 0, mainscreen_animation, frame, __flag_none);
		menu_state |= 1 << 4;
	}

	/* if a new frame begins advance the frame counter of the animation */
	if (next_frame == 0) {
		next_frame = FPS;
		frame = (frame + 1) % 8;
		menu_state &= ~(0xf << 1);
		menu_state |= (frame << 1);
	}
	next_frame--;

	/* reset menu state if we leave the main menu */
	if (game_state != MAIN)
		menu_state &= ~1;
	return game_state;
}

static int
load(void)
{
	return MAIN;
}

/*---------------------------------------------------------------------------
 * game handling
 *---------------------------------------------------------------------------*/
static struct character_state {
	uint8_t life;
	uint8_t x;

	uint16_t rest_timeout;
	uint8_t atime;
	uint8_t frame:2;
	uint8_t state:2;
	uint8_t previous_state:2;

	uint32_t score;
} cs;

static void draw_number(uint8_t x, uint8_t y, int8_t number)
{
	if (number >= 0 && number <= 9)
		blit_image_frame(x, y, numbers_img, number, __flag_none);
}

static void draw_score(void)
{
	uint32_t score = cs.score;

	draw_number(90, 57, score / 1000000);
	score %= 1000000;
	draw_number(95, 57, score / 100000);
	score %= 100000;
	draw_number(100, 57, score / 10000);
	score %= 10000;
	draw_number(105, 57, score / 1000);
	score %= 1000;
	draw_number(110, 57, score / 100);
	score %= 100;
	draw_number(115, 57, score / 10);
	score %= 10;
	draw_number(120, 57, score);
}

enum player_states {
	PLAYER_L_MOVE,
	PLAYER_R_MOVE,
	PLAYER_RESTS,
	PLAYER_THROWS,
	PLAYER_MAX_STATES,
};

#define PLAYER_REST_TIMEOUT                  FPS * 5

/* delay per frame in each state */
static const uint8_t player_timings[PLAYER_MAX_STATES] = {
	FPS / 5, /* moving left */
	FPS / 5, /* moving right */
	FPS / 1, /* resting */
	FPS / 2, /* throwing */
};
/* offsets in number of frames per state */
static const uint8_t player_frame_offsets[PLAYER_MAX_STATES] = {
	  0, /* moving left */
	  4, /* moving right */
	  8, /* resting */
	 12, /* throwing */
};

static void player_set_state(uint8_t new_state)
{
	if (new_state != cs.state || new_state == PLAYER_THROWS) {
		cs.frame = 0;
		cs.atime = player_timings[new_state];
	}

	if (cs.state != PLAYER_THROWS && cs.state != PLAYER_RESTS)
		cs.previous_state = cs.state;

	cs.state = new_state;
	switch(cs.state) {
	case PLAYER_L_MOVE:
	case PLAYER_R_MOVE:
	case PLAYER_THROWS:
		cs.rest_timeout = PLAYER_REST_TIMEOUT;
		break;
	}
}

static void update_player(int8_t dx, uint8_t throws)
{
	/* update position */
	if (dx < 0) {
		player_set_state(PLAYER_L_MOVE);
		if (cs.x > 20)
			cs.x--;
	}
	if (dx > 0) {
		player_set_state(PLAYER_R_MOVE);
		if (cs.x < 100)
			cs.x++;
	}

	if (throws)
		player_set_state(PLAYER_THROWS);

	if (!dx && !throws && cs.state != PLAYER_RESTS)
		cs.rest_timeout--;

	if (!cs.rest_timeout)
		player_set_state(PLAYER_RESTS);

	/* return to previous state after throwing */
	if (!throws &&
	    cs.state == PLAYER_THROWS && cs.atime == 0) {
		player_set_state(cs.previous_state);
	}

	/* update frames */
	if (cs.atime == 0) {
		cs.atime = player_timings[cs.state];
		cs.frame++;
	} else
		cs.atime--;
}

static void draw_player(void)
{
	blit_image_frame(cs.x,
			 0,
			 player_all_frames_img,
			 player_frame_offsets[cs.state] + cs.frame,
			 __flag_none);
}

static void draw_enemies(void)
{
}

static void draw_scene(void)
{
	blit_image(0, 0, game_background_img, __flag_none);
}

enum game_states {
	GAME_STATE_INIT = 0,
	GAME_STATE_RUN,
	GAME_STATE_OVER,
};

// XXX
static enum game_states game_state = GAME_STATE_INIT;

#define NR_BULLETS		10
#define NR_WEAPONS		4

struct bullet_state {
	uint8_t x; /* x position of bullet */
	uint8_t ys; /* y start position of the bullet */
	uint8_t ye; /* y end position of the bullet */
	uint8_t active:1;
	uint8_t weapon:2;
	uint8_t frame:2;
	uint8_t atime; /* nr of frames it take for the next animation frame */
	uint8_t etime; /* nr of frames a weapon has effect on the ground */
	uint8_t mtime; /* nr of frames it takes to move */
};

static struct weapon_states {
	uint8_t selected; /* selected weapon */
	uint8_t ammo[NR_WEAPONS]; /* available ammo per weapon */
	uint8_t rtime[NR_WEAPONS]; /* remaining time till reload */
	struct bullet_state bs[NR_BULLETS];
} ws;

/* nr of frames until ammo reloads on item */
static const uint8_t reload_time[NR_WEAPONS] = {FPS, 2 * FPS, 3 * FPS, 4 * FPS};
/* nr of frames until next bullet animation */
static const uint8_t atime[NR_WEAPONS] = {FPS, FPS, FPS, FPS};
/* nr of frames weapon is effective on ground */
static const uint8_t etime[NR_WEAPONS] = {FPS * 2, FPS * 2, FPS * 2, FPS * 2};
/* nr of frames until next bullet movement */
static const uint8_t mtime[NR_WEAPONS] = {FPS / 20, FPS / 20, FPS / 20, FPS / 20};
/* maximum number of ammo per weapon */
static const uint8_t max_ammo[NR_WEAPONS] = {8, 4, 2, 1};

static void update_ammo(void)
{
	uint8_t w;

	for (w = 0; w < NR_WEAPONS; w++) {
		if (ws.ammo[w] == max_ammo[w])
			continue;
		if (ws.rtime[w] == 0) {
			ws.ammo[w]++;
			ws.rtime[w] = reload_time[w];
		} else
			ws.rtime[w]--;
	}
}

static void new_bullet(uint8_t x, uint8_t lane, uint8_t weapon)
{
	uint8_t b;
	struct bullet_state *bs = &ws.bs[0];

	if (ws.ammo[weapon] == 0)
		return;

	/* create a new bullet, do nothing if not possible */
	for (b = 0; b < NR_BULLETS; b++, bs++) {
		if (bs->active == 0) {
			bs->active = 1;
			bs->weapon = weapon;
			bs->x = x;
			bs->ys = 5;
			if (lane)
				bs->ye = 42;
			else
				bs->ye = 56;
			bs->atime = atime[weapon];
			bs->etime = etime[weapon];
			bs->mtime = mtime[weapon];
			bs->frame = 0;
			break;
		}
	}
}

static void update_bullets(void)
{
	uint8_t b;
	struct bullet_state *bs = &ws.bs[0];

	/* create a new bullet, do nothing if not possible */
	for (b = 0; b < NR_BULLETS; b++, bs++) {
		if (bs->active == 1) {
			if (bs->ys == bs->ye) {
				if (bs->etime == 0)
					bs->active = 0;
				else
					bs->etime--;
			} else {
				/* next movement */
				if (bs->mtime == 0) {
					bs->mtime = mtime[bs->weapon];
					bs->ys++;
					if (bs->ys == bs->ye)
						bs->frame = 0;
				} else
					bs->mtime--;
			}
			/* next animation */
			if (bs->atime == 0) {
				bs->atime = atime[bs->weapon];
				bs->frame++;
			} else
				bs->atime--;
		}
	}
}

static void draw_bullets(void)
{
	uint8_t b;
	struct bullet_state *bs = &ws.bs[0];

	/* create a new bullet, do nothing if not possible */
	for (b = 0; b < NR_BULLETS; b++, bs++) {
		if (bs->active == 0)
			continue;
		if (bs->ys == bs->ye)
			blit_image_frame(bs->x,
					 bs->ys,
					 water_bomb_impact_img,
					 bs->frame,
					 __flag_none);
		else
			blit_image_frame(bs->x,
					 bs->ys,
					 water_bomb_air_img,
					 bs->frame,
					 __flag_none);
	}
}

struct enemy_state {
	uint8_t life;
	uint8_t xy;
	uint8_t frame;
	uint8_t type;
};

static void update_enemies(void)
{
	/* update and spawn enemies */
}

static void update_scene(void)
{
	/* update scene animations */
}

static int check_game_over(void)
{
	if (cs.life == 0)
		return 1;
	return 0;
}

static void check_collisions(void)
{
}

static int
run(void)
{
	uint8_t i, throws = 0;
	int8_t dx = 0;
	int rstate = RUN;

	switch (game_state) {
	case GAME_STATE_INIT:
		/* init character state */
		cs.life = 255;
		cs.x = 20;
		cs.frame = 0;
		cs.score = 0;
		cs.rest_timeout = PLAYER_REST_TIMEOUT;
		/* init weapon states */
		memset(&ws, 0, sizeof(ws));
		for (i = 0; i < NR_WEAPONS; i++)
			ws.ammo[i] = max_ammo[i];
		game_state = GAME_STATE_RUN;
		break;
	case GAME_STATE_RUN:
		/* check for game over */
		if (check_game_over()) {
			game_state = GAME_STATE_OVER;
			break;
		}

		/* check user inputs */
		if (up() && a()) {
			/* go to menu */
			rstate = MAIN;
			break;
		}

		if (up()) {
			/* select weapon upwards */
			if (ws.selected == 0)
				ws.selected = NR_WEAPONS - 1;
			else
				ws.selected--;
		} else if (down()) {
			/* select weapon downwards */
			if (ws.selected == NR_WEAPONS - 1)
				ws.selected = 0;
			else
				ws.selected++;
		} else if (left()) {
			/* move character to the left */
			dx = -1;
		} else if (right()) {
			/* move character to the right */
			dx = 1;
		}
		if (a()) {
			/* throws bullet to the lower lane of the street */
			new_bullet(cs.x, 0, ws.selected);
			throws = 1;
		}
		if (b()) {
			/* throws bullet to the upper lane of the street */
			new_bullet(cs.x, 1, ws.selected);
			throws = 1;
		}

		/* update ammo */
		update_ammo();
		/* update bullets */
		update_bullets();
		/* check for collisions */
		check_collisions();
		/* update/spawn enemies */
		update_enemies();
		/* update scene animations */
		update_scene();
		/* update player */
		update_player(dx, throws);

		/* draw scene */
		draw_scene();
		/* draw player */
		draw_player();
		/* draw enemies */
		draw_enemies();
		/* update animations */
		draw_bullets();
		/* draw new score */
		draw_score();
		//HACK
		cs.score++;
		break;
	case GAME_STATE_OVER:
		rstate = MAIN;
		break;
	}
	return rstate;
}

static int
help(void)
{
	return MAIN;
}

/*---------------------------------------------------------------------------
 * loop
 *---------------------------------------------------------------------------*/
#ifdef __cplusplus
extern "C"
{
#endif

void
loop(void)
{
	if (!next_frame())
		return;

	switch (main_state) {
	case MAIN:
		main_state = mainscreen();
		break;
	case LOAD:
		main_state = load();
		break;
	case RUN:
		main_state = run();
		break;
	case HELP:
		main_state = help();
		break;
	}
	finish_frame();
}

#ifdef __cplusplus
}
#endif
