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

/*---------------------------------------------------------------------------
 * game parameters
 *---------------------------------------------------------------------------*/
#define FPS                         30        /* frame rate */
#define MAX_ENEMIES                 2         /* max enemies on the scene */
#define ENEMIES_SPAWN_RATE          (FPS * 3) /* every 3 seconds */
#define PLAYER_REST_TIMEOUT         (FPS * 5) /* players timeout for resting */
#define PLAYER_MAX_LIFE             256       /* initial players life */
#define NR_WEAPONS                  4
#define MAX_AMMO_W1                 16
#define MAX_AMMO_W2                 12
#define MAX_AMMO_W3                 8
#define MAX_AMMO_W4                 4
#define NR_BULLETS                  \
	(MAX_AMMO_W1 + MAX_AMMO_W2 + MAX_AMMO_W3 + MAX_AMMO_W4)

/*---------------------------------------------------------------------------
 * program states
 *---------------------------------------------------------------------------*/
enum program_states {
	PROGRAM_MAIN_MENU = 0,
	PROGRAM_LOAD_GAME,
	PROGRAM_RUN_GAME,
	PROGRAM_SHOW_HELP,
};

/*---------------------------------------------------------------------------
 * graphic functions
 *---------------------------------------------------------------------------*/
#define __flag_none                  (0)
#define __flag_color_invert          (1 << 0)
#define __flag_h_mirror              (1 << 1)
#define __flag_v_mirror              (1 << 2)
#define __flag_black                 (1 << 3)
#define __flag_white                 (1 << 4)

#define img_width(i)                 pgm_read_byte_near((i) + 0)
#define img_height(i)                pgm_read_byte_near((i) + 1)

#ifndef HOST_TEST
static void
blit_image(int16_t x, int16_t y, const uint8_t *img, const uint8_t *mask,
	   uint8_t flags)
{
	arduboy.drawBitmap(x,
			   y,
			   img + 2,
			   img_width(img),
			   img_height(img),
			   WHITE);
}

static void
blit_image_frame(int16_t x, int16_t y, const uint8_t *img, const uint8_t *mask,
		 uint8_t nr, uint8_t flags)
{
	uint8_t w, h;

	w = img_width(img);
	h = img_height(img);

	arduboy.drawBitmap(x,
			   y,
			   img + (w * ((h + 7) / 8) * nr) + 2,
			   w,
			   h,
			   WHITE);
}

static void
draw_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h)
{
	arduboy.drawRect(x, y, w, h, WHITE);
}

static void
draw_filled_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h)
{
	arduboy.fillRect(x, y, w, h, WHITE);
}
#endif

/*---------------------------------------------------------------------------
 * timers
 *---------------------------------------------------------------------------*/
enum timers {
	TIMER_PLAYER_RESTS,
	TIMER_ENEMY_SPAWN,
	TIMER_500MS,
	TIMER_MAX,
};

typedef void (*timeout_fn)(void);

struct timer {
	uint16_t active:1;
	uint16_t timeout:15;
	timeout_fn fn;
};

static struct timer timers[TIMER_MAX];

static void init_timers(void)
{
	memset(timers, 0, sizeof(timers));
}

static void run_timers(void)
{
	for (uint8_t t = 0; t < TIMER_MAX; t++) {
		if (!timers[t].active)
			continue;
		if (timers[t].timeout == 0) {
			timers[t].active = 0;
			if (timers[t].fn)
				timers[t].fn();
		}
		timers[t].timeout--;
	}
}

static void setup_timer(uint8_t id, timeout_fn fn)
{
	timers[id].fn = fn;
}

static void start_timer(uint8_t id, uint16_t timeout)
{
	timers[id].timeout = timeout;
	timers[id].active = 1;
}

static void stop_timer(uint8_t id)
{
	timers[id].active = 0;
}

/*---------------------------------------------------------------------------
 * general purpose timers
 *---------------------------------------------------------------------------*/
static uint8_t timer_500ms_ticks = 0;
static void timer_500ms_counter(void)
{
	timer_500ms_ticks++;
	start_timer(TIMER_500MS, FPS / 2);
}

/*---------------------------------------------------------------------------
 * misc functions
 *---------------------------------------------------------------------------*/
static uint8_t random8(uint8_t max)
{
#ifdef HOST_TEST
	return random() % max;
#else
	return random(255) % max;
#endif
}

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
		clear_screen();
		update_inputs();
		run_timers();
	}

	{
		static unsigned int frames = 0;
		if (new_frame) {
			frames++;
			printf("\x1b[%d;%df", 65, 0);
			printf("ms per frame %u ntime %lu current_time %lu frames %u\n\r",
			       1000 / FPS,
			       ntime,
			       current_time,
			       frames);
		}
	}

	return new_frame;
#else
	if (arduboy.nextFrame()) {
		arduboy.clear();
		buttons.poll();
		run_timers();
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
	arduboy.initRandomSeed();
	arduboy.setFrameRate(FPS);
	arduboy.begin();
	arduboy.clear();
	arduboy.display();
	init_timers();
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
	MENU_ITEM_PROGRAM_LOAD_GAME,
	MENU_ITEM_PROGRAM_SHOW_HELP,
	MENU_ITEM_CONTINUE,
	MENU_ITEM_SAVE,
};

const uint8_t menu_item_xlate[] PROGMEM = {
	[MENU_ITEM_PLAY] = PROGRAM_RUN_GAME,
	[MENU_ITEM_PROGRAM_LOAD_GAME] = PROGRAM_LOAD_GAME,
	[MENU_ITEM_PROGRAM_SHOW_HELP] = PROGRAM_SHOW_HELP,
	[MENU_ITEM_CONTINUE] = PROGRAM_RUN_GAME,
	[MENU_ITEM_SAVE] = PROGRAM_MAIN_MENU,
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
	uint8_t game_state = PROGRAM_MAIN_MENU;
	uint8_t needs_update = 1;
	uint8_t menu_item = (menu_state >> 5) & 0x3;
	static uint8_t next_frame = FPS; /* updates of the animation */

	/* print main screen */
	if (!(menu_state & 1)) {
		blit_image(0, 0, mainscreen_img, NULL, __flag_none);

		/* draw new item highlighted */
		blit_image(MENU_ITEM_X,
			   menu_item * MENU_ITEM_DISTANCE,
			   menu_item_selected_img,
			   NULL,
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
			   NULL,
			   __flag_white);
		/* draw new item highlighted */
		blit_image(MENU_ITEM_X,
			   menu_item * MENU_ITEM_DISTANCE,
			   menu_item_selected_img,
			   NULL,
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
	if (game_state != PROGRAM_MAIN_MENU)
		menu_state &= ~1;
	return game_state;
}

static int
load(void)
{
	return PROGRAM_MAIN_MENU;
}

/*---------------------------------------------------------------------------
 * game handling
 *---------------------------------------------------------------------------*/
static struct character_state {
	int16_t life; /* remaining life of player */
	uint8_t x; /* current x position of player */
	uint8_t atime; /* nr of frames till next frame of sprite */
	uint8_t frame:2; /* current frame of sprite */
	uint8_t state:2; /* current player state */
	uint8_t previous_state:2; /* previous player state */
	int32_t score; /* current score */
} cs;

enum player_states {
	PLAYER_L_MOVE,
	PLAYER_R_MOVE,
	PLAYER_RESTS,
	PLAYER_THROWS,
	PLAYER_MAX_STATES,
};

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

	if (new_state != PLAYER_RESTS)
		start_timer(TIMER_PLAYER_RESTS, PLAYER_REST_TIMEOUT);

	if (cs.state != PLAYER_THROWS && cs.state != PLAYER_RESTS)
		cs.previous_state = cs.state;

	cs.state = new_state;
}

static void player_is_resting(void) {
	player_set_state(PLAYER_RESTS);
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
		if (cs.x < 116)
			cs.x++;
	}

	/* XXX might introduce timeout to reduce fire rate */
	if (throws)
		player_set_state(PLAYER_THROWS);

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

enum game_states {
	GAME_STATE_INIT = 0,
	GAME_STATE_RUN_GAME,
	GAME_STATE_OVER,
	GAME_STATE_CLEANUP,
};

// XXX
static enum game_states game_state = GAME_STATE_INIT;

/*---------------------------------------------------------------------------
 * bullet handling
 *---------------------------------------------------------------------------*/
enum bullet_state {
	BULLET_INACTIVE,
	BULLET_ACTIVE,
	BULLET_EFFECT,
	BULLET_SPLASH,
};

struct bullet {
	uint8_t x; /* x position of bullet */
	uint8_t ys; /* y start position of the bullet */
	uint8_t state:2;
	uint8_t weapon:2;
	uint8_t frame:2;
	uint8_t lane:2;
	uint8_t atime; /* nr of frames it take for the next animation frame */
	uint8_t etime; /* nr of frames a weapon has effect on the ground */
	uint8_t mtime; /* nr of frames it takes to move */
};

static struct weapon_states {
	uint8_t selected; /* selected weapon */
	uint8_t ammo[NR_WEAPONS]; /* available ammo per weapon */
	struct bullet bs[NR_BULLETS];
} ws;

/* damage per bullet */
static const uint8_t bullet_damage[NR_WEAPONS] = {1, 2, 3, 4};
/* nr of frames until next bullet animation */
static const uint8_t atime[NR_WEAPONS] = {FPS, FPS, FPS, FPS};
/* nr of frames weapon is effective on ground */
static const uint8_t etime[NR_WEAPONS] = {FPS * 2, FPS * 2, FPS * 2, FPS * 2};
/* nr of frames until next bullet movement */
static const uint8_t mtime[NR_WEAPONS] = {FPS / 20, FPS / 20, FPS / 20, FPS / 20};
/* maximum number of ammo per weapon */
static const uint8_t max_ammo[NR_WEAPONS] = {
	MAX_AMMO_W1,
	MAX_AMMO_W2,
	MAX_AMMO_W3,
	MAX_AMMO_W4,
};

#define DOOR_LANE			0
#define UPPER_LANE			1
#define LOWER_LANE			2

static const uint8_t lane_y[3] = {52, 56, 60};

static uint8_t new_bullet(uint8_t x, uint8_t lane, uint8_t weapon)
{
	uint8_t b;
	struct bullet *bs = &ws.bs[0];

	if (ws.ammo[weapon] == 0)
		return 0;

	/* create a new bullet, do nothing if not possible */
	for (b = 0; b < NR_BULLETS; b++, bs++) {
		if (bs->state != BULLET_INACTIVE)
			continue;

		bs->state = BULLET_ACTIVE;
		bs->weapon = weapon;
		bs->x = x;
		bs->ys = 5;
		bs->lane = lane;
		bs->atime = atime[weapon];
		bs->etime = etime[weapon];
		bs->mtime = mtime[weapon];
		bs->frame = 0;
		ws.ammo[weapon]--;
		break;
	}
	return 1;
}

static void update_bullets(void)
{
	uint8_t b;
	struct bullet *bs = &ws.bs[0];

	for (b = 0; b < NR_BULLETS; b++, bs++) {
		if (bs->state == BULLET_INACTIVE)
			continue;

		if (bs->state == BULLET_EFFECT || bs->state == BULLET_SPLASH) {
			if (bs->etime == 0) {
				bs->state = BULLET_INACTIVE;
				ws.ammo[bs->weapon]++;
			} else
				bs->etime--;
		} else {
			/* next movement */
			if (bs->mtime == 0) {
				bs->mtime = mtime[bs->weapon];
				bs->ys++;
				if (bs->ys == lane_y[bs->lane]) {
					bs->frame = 0;
					bs->state = BULLET_EFFECT;
				}
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

static uint8_t get_bullet_damage(uint8_t lane, uint8_t x, uint8_t y, uint8_t w, uint8_t h)
{
	uint8_t b, damage = 0, hit;
	struct bullet *bs = &ws.bs[0];

	for (b = 0; b < NR_BULLETS; b++, bs++) {
		if (bs->state != BULLET_ACTIVE)
			continue;
		if (bs->lane != lane)
			if (bs->lane != UPPER_LANE || lane != DOOR_LANE)
				continue;
		hit = 0;
		/* check if it is a hit */
		if (bs->x >= x && bs->x <= (x + w) && bs->ys >= y && bs->ys <= (y + h)) {
			hit = 1;
		}
		if ((bs->x + 4) >= x && (bs->x + 4) <= (x + w) && (bs->ys + 4) >= y && (bs->ys + 4) <= (y + h)) {
			hit = 1;
		}
		if (hit) {
			damage += bullet_damage[bs->weapon];
			bs->state = BULLET_SPLASH;
			bs->etime = FPS / 4;
		}
	}

	return damage;
}

/*---------------------------------------------------------------------------
 * enemy handling
 *---------------------------------------------------------------------------*/
enum enemy_types {
	/* vicious enemies */
	ENEMY_RAIDER,
	/* peaceful enemies */
	ENEMY_PEACEFUL,
	ENEMY_GRANDMA = ENEMY_PEACEFUL,
	ENEMY_MAX,
};

enum enemy_state {
	ENEMY_APPROACH_DOOR,
	ENEMY_WALKING_LEFT,
	ENEMY_WALKING_RIGHT,
	ENEMY_EFFECT,
	ENEMY_CHANGE_LANE,
	ENEMY_MOVE_TO_UPPER_LANE,
	ENEMY_MOVE_TO_LOWER_LANE,
	ENEMY_ATTACKING,
	ENEMY_RESTING,
	ENEMY_DYING,
	ENEMY_SWEARING,
	ENEMY_MAX_STATE,
};

static const uint8_t enemy_sprite_offsets[ENEMY_MAX_STATE] = {
	 0,
	 0,
	 4,
	 0,
	 0,
	12,
	16,
	 8,
	20,
	 0,
	 4,
};

static const uint8_t enemy_damage[ENEMY_MAX] = {
	1, 0,
};

static const int8_t enemy_life[ENEMY_MAX] = {
	16, 32
};

static const int8_t enemy_mtime[ENEMY_MAX] = {
	FPS / 20,
	FPS / 5,
};

static const int8_t enemy_rtime[ENEMY_MAX] = {
	FPS,
	FPS * 2,
};

static const int8_t enemy_atime[ENEMY_MAX] = {
	FPS / 10,
	FPS / 10,
};

static const int8_t enemy_score[ENEMY_MAX] = {
	10,
	-100,
};

static const uint8_t *enemy_sprites[ENEMY_MAX] = {
	enemy_raider_img,
	enemy_grandma_img,
};

struct enemy {
	int8_t life;
	uint8_t x;
	uint8_t dx;
	uint8_t y;
	uint8_t type;
	uint8_t atime; /* nr of frames it take for the next animation frame */
	uint8_t rtime; /* nr of frames it takes to rest */
	uint8_t mtime; /* nr of frames it takes to move */
	uint8_t damage;
	uint8_t frame:2;
	uint8_t lane:2;
	uint8_t dlane:2;
	uint8_t active:1;
	uint8_t state;
	uint8_t previous_state;
	uint8_t sprite_offset;
};

struct door {
	uint8_t under_attack:1;
	uint8_t boss:1;
	struct enemy *attacker;
};

static struct enemy enemies[MAX_ENEMIES];
static struct door door;

static void enemy_set_state(struct enemy *e, uint8_t state)
{
	if (e->state != ENEMY_CHANGE_LANE)
		e->previous_state = e->state;
	e->sprite_offset = enemy_sprite_offsets[state];
	e->frame = 0;
	e->state = state;
}

static void spawn_new_enemies(void)
{
	uint8_t i, height;
	struct enemy *e = &enemies[0];

	/* update and spawn enemies */
	for (i = 0; i < MAX_ENEMIES; i++, e++) {
		if (e->active)
			continue;
		e->active = 1;
		e->type = random8(ENEMY_MAX);
		height = enemy_sprites[e->type][1];
		e->lane = 1 + random8(2);
		e->y = lane_y[e->lane] - height;
		e->x = 127;
		e->frame = 0;
		e->mtime = enemy_mtime[e->type];
		e->rtime = enemy_rtime[e->type];
		e->atime = enemy_atime[e->type];
		e->life = enemy_life[e->type];
		e->damage = enemy_damage[e->type];
		enemy_set_state(e, ENEMY_WALKING_LEFT);
		enemy_set_state(e, ENEMY_WALKING_LEFT);
		break;
	}
	start_timer(TIMER_ENEMY_SPAWN, ENEMIES_SPAWN_RATE);
}

static void enemy_switch_lane(struct enemy *e, uint8_t lane, uint8_t y)
{
	if (e->y == y) {
		e->lane = lane;
		return;
	}

	if (e->mtime != 0)
		return;

	if (e->y > y)
		e->y--;
	else
		e->y++;
}

static void update_enemies(void)
{
	uint8_t damage;
	uint8_t i, width, height;
	struct enemy *e = &enemies[0];

	/* update and spawn enemies */
	for (i = 0; i < MAX_ENEMIES; i++, e++) {
		if (!e->active)
			continue;
		width = img_width(enemy_sprites[e->type]);
		height = img_height(enemy_sprites[e->type]);
		/* check if hit by bullet */
		damage = get_bullet_damage(e->lane, e->x, e->y, width, height);
		if (damage) {
			e->life -= damage; /* bullet damage */
			if (e->life <= 0) {
				if (door.attacker == e) {
					door.attacker = NULL;
					door.under_attack = 0;
				}
				e->atime = FPS;
				cs.score += enemy_score[e->type];
				if (cs.score < 0)
					cs.score = 0;
				enemy_set_state(e, ENEMY_DYING);
			} else {
				if (e->state != ENEMY_EFFECT && e->state != ENEMY_SWEARING) {
					if (e->damage)
						enemy_set_state(e, ENEMY_EFFECT);
					else
						enemy_set_state(e, ENEMY_SWEARING);
				}
			}
		}
		switch (e->state) {
		case ENEMY_WALKING_LEFT:
			if (e->mtime)
				break;

			if (e->damage) {
				if (e->x == 24)
					enemy_set_state(e, ENEMY_APPROACH_DOOR);
			} else {
				/* TODO peaceful enemies just pass by */
				if (e->x == 16)
					e->active = 0;
			}
			e->x--;
			break;
		case ENEMY_APPROACH_DOOR:
			if (door.under_attack && door.attacker != e) {
				/* door is already under attack, take a walk */
				e->dx = e->x + random8(WIDTH - e->x - width);
				e->dlane = 1 + random8(2);
				enemy_set_state(e, ENEMY_WALKING_RIGHT);
				break;
			}
			door.under_attack = 1;
			door.attacker = e;
			e->dlane = DOOR_LANE;
			if (e->lane != e->dlane)
				enemy_set_state(e, ENEMY_CHANGE_LANE);
			else
				enemy_set_state(e, ENEMY_ATTACKING);
			break;
		case ENEMY_WALKING_RIGHT:
			if (e->mtime)
				break;

			if (e->x == e->dx) {
				/* randomly change lane */
				if (e->lane != e->dlane)
					enemy_set_state(e, ENEMY_CHANGE_LANE);
				else
					enemy_set_state(e, ENEMY_WALKING_LEFT);
			} else
				e->x++;
			break;
		case ENEMY_EFFECT:
			if (e->atime == 0)
				enemy_set_state(e, e->previous_state);
			break;
		case ENEMY_CHANGE_LANE:
			if (e->lane != e->dlane) {
				if (e->lane > e->dlane)
					enemy_set_state(e, ENEMY_MOVE_TO_UPPER_LANE);
				else
					enemy_set_state(e, ENEMY_MOVE_TO_LOWER_LANE);
			} else
				enemy_set_state(e, e->previous_state);
			break;
		case ENEMY_MOVE_TO_UPPER_LANE:
		case ENEMY_MOVE_TO_LOWER_LANE:
			if (e->lane != e->dlane)
				enemy_switch_lane(e,
						  e->dlane,
						  lane_y[e->dlane] - height);
			else
				enemy_set_state(e, e->previous_state);
			break;
		case ENEMY_ATTACKING:
			/* do door damage */
			if (e->frame == 3 && e->atime == 0) {
				cs.life -= enemy_damage[e->type];
				enemy_set_state(e, ENEMY_RESTING);
			}
			break;
		case ENEMY_SWEARING:
		case ENEMY_RESTING:
			if (e->rtime == 0) {
				e->rtime = enemy_rtime[e->type];
				enemy_set_state(e, e->previous_state);
			} else
				e->rtime--;
			break;
		case ENEMY_DYING:
			if (e->atime == 0)
				e->active = 0;
			break;
		}
		/* next animation */
		if (e->atime == 0) {
			e->atime = enemy_atime[e->type];
			e->frame++;
		} else
			e->atime--;
		/* next movement */
		if (e->mtime == 0)
			e->mtime = enemy_mtime[e->type];
		else
			e->mtime--;
	}
}

/*---------------------------------------------------------------------------
 * scene handling
 *---------------------------------------------------------------------------*/
static uint8_t lamp_frame = 0;

static void update_scene(void)
{
	/* update scene animations */

	/* update lamp animation */
	if (timer_500ms_ticks & 1)
		lamp_frame = random8(2);
}

/*---------------------------------------------------------------------------
 * misc control functions
 *---------------------------------------------------------------------------*/
static int check_game_over(void)
{
	if (cs.life <= 0)
		return 1;
	return 0;
}

/*---------------------------------------------------------------------------
 * rendering functions
 *---------------------------------------------------------------------------*/
static void draw_player(void)
{
	blit_image_frame(cs.x,
			 0,
			 player_all_frames_img,
			 NULL,
			 player_frame_offsets[cs.state] + cs.frame,
			 __flag_none);
}

static void draw_enemies(void)
{
	uint8_t i, show;
	struct enemy *e = &enemies[0];

	for (i = 0; i < MAX_ENEMIES; i++, e++) {
		if (!e->active)
			continue;
		switch (e->state) {
		case ENEMY_DYING:
		case ENEMY_EFFECT:
			show = e->atime & 1;
			break;
		default:
			show = 1;
			break;
		}
		if (show) {
			blit_image_frame(e->x,
					 e->y,
					 enemy_sprites[e->type],
					 NULL,
					 e->frame + e->sprite_offset,
					 __flag_none);
		}
	}
}

static void draw_scene(void)
{
	int16_t life_level;

	/* draw main scene */
	blit_image(0, 0, game_background_img, NULL, __flag_none);

	/* draw selected weapon */
	draw_rect(0, 14 * ws.selected, 14, 14);

	/* draw current ammo level */
	for (uint8_t i = 0; i < NR_WEAPONS; i++) {
		uint8_t level = (ws.ammo[i] * 4 + max_ammo[i] - 1) / max_ammo[i];
		while (level--) {
			draw_filled_rect(10,
					 12 + (14 * i) - level * 2,
					 2,
					 1);
		}
	}

	/* draw current life */
	life_level = (cs.life * 4 + PLAYER_MAX_LIFE - 1) / PLAYER_MAX_LIFE;
	if (life_level > 2) {
		draw_rect(0, 57, 15, 7);
	} else {
		if (timer_500ms_ticks & 1)
			draw_rect(0, 57, 15, 7);
	}

	while (life_level--) {
		draw_filled_rect(2 + (3 * life_level),
				 59,
				 2,
				 3);
	}

	/* draw weather animation */
	/* TODO */

	/* draw lamp animation */
	blit_image_frame(70,
			 HEIGHT - img_height(scene_lamp_img),
			 scene_lamp_img,
			 NULL,
			 lamp_frame,
			 __flag_none);
}

static void draw_bullets(void)
{
	uint8_t b;
	struct bullet *bs = &ws.bs[0];

	/* create a new bullet, do nothing if not possible */
	for (b = 0; b < NR_BULLETS; b++, bs++) {
		switch (bs->state) {
		case BULLET_EFFECT:
		case BULLET_SPLASH:
			blit_image_frame(bs->x,
					 bs->ys,
					 water_bomb_impact_img,
					 NULL,
					 bs->frame,
					 __flag_none);
			break;
		case BULLET_ACTIVE:
			blit_image_frame(bs->x,
					 bs->ys,
					 water_bomb_air_img,
					 NULL,
					 bs->frame,
					 __flag_none);
			break;
		default:
			break;
		}
	}
}

static void draw_number(uint8_t x, uint8_t y, int8_t number)
{
	if (number < 0 || number > 9)
		return;
	blit_image_frame(x, y, numbers_3x5_img, NULL, number, __flag_none);
}

static void draw_score(void)
{
	uint32_t score = cs.score;

	draw_number(100, 59, score / 1000000);
	score %= 1000000;
	draw_number(104, 59, score / 100000);
	score %= 100000;
	draw_number(108, 59, score / 10000);
	score %= 10000;
	draw_number(112, 59, score / 1000);
	score %= 1000;
	draw_number(116, 59, score / 100);
	score %= 100;
	draw_number(120, 59, score / 10);
	score %= 10;
	draw_number(124, 59, score);
}

static int
run(void)
{
	uint8_t i, throws = 0;
	int8_t dx = 0;
	int rstate = PROGRAM_RUN_GAME;

	switch (game_state) {
	case GAME_STATE_INIT:
		init_timers();
		/* init character state */
		cs.life = PLAYER_MAX_LIFE;
		cs.x = 20;
		cs.frame = 0;
		cs.score = 0;
		/* setup timer for players resting animation */
		setup_timer(TIMER_PLAYER_RESTS, player_is_resting);
		start_timer(TIMER_PLAYER_RESTS, PLAYER_REST_TIMEOUT);
		/* init weapon states */
		memset(&ws, 0, sizeof(ws));
		for (i = 0; i < NR_WEAPONS; i++)
			ws.ammo[i] = max_ammo[i];
		/* setup timer for enemy spawning */
		setup_timer(TIMER_ENEMY_SPAWN, spawn_new_enemies);
		start_timer(TIMER_ENEMY_SPAWN, ENEMIES_SPAWN_RATE);
		game_state = GAME_STATE_RUN_GAME;
		/* setup general purpose 500ms counting timer */
		timer_500ms_ticks = 0;
		setup_timer(TIMER_500MS, timer_500ms_counter);
		start_timer(TIMER_500MS, FPS / 2);
		break;
	case GAME_STATE_RUN_GAME:
		/* check for game over */
		if (check_game_over()) {
			game_state = GAME_STATE_OVER;
			break;
		}

		/* back to menu */
		if (up() && a()) {
			game_state = GAME_STATE_CLEANUP;
			break;
		}

		/* check user inputs */
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
			throws = new_bullet(cs.x, LOWER_LANE, ws.selected);
		}
		if (b()) {
			/* throws bullet to the upper lane of the street */
			throws = new_bullet(cs.x, UPPER_LANE, ws.selected);
		}

		/* update bullets */
		update_bullets();
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
		break;
	case GAME_STATE_OVER:
		/* TODO */
		game_state = GAME_STATE_OVER;
		break;
	case GAME_STATE_CLEANUP:
		init_timers();
		rstate = PROGRAM_MAIN_MENU;
		break;
	}
	return rstate;
}

static int
help(void)
{
	return PROGRAM_MAIN_MENU;
}

/*---------------------------------------------------------------------------
 * loop
 *---------------------------------------------------------------------------*/
/* state of the program */
static uint8_t main_state = PROGRAM_MAIN_MENU;

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
	case PROGRAM_MAIN_MENU:
		main_state = mainscreen();
		break;
	case PROGRAM_LOAD_GAME:
		main_state = load();
		break;
	case PROGRAM_RUN_GAME:
		main_state = run();
		break;
	case PROGRAM_SHOW_HELP:
		main_state = help();
		break;
	}
	finish_frame();
}

#ifdef __cplusplus
}
#endif
