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
#define KILLS_TILL_BOSS             2


#define BULLET_FRAME_TIME           FPS / 10
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
	if (mask)
		arduboy.drawBitmap(x,
				   y,
				   mask + 2,
				   img_width(mask),
				   img_height(mask),
				   BLACK);

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
	uint16_t offset;
	uint8_t w, h;

	w = img_width(img);
	h = img_height(img);
	offset = (w * ((h + 7) / 8) * nr) + 2;

	if (mask)
		arduboy.drawBitmap(x,
				   y,
				   mask + offset,
				   w,
				   h,
				   BLACK);

	arduboy.drawBitmap(x,
			   y,
			   img + offset,
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

static void
draw_hline(uint8_t x, uint8_t y, uint8_t w)
{
	arduboy.drawFastHLine(x, y, w, WHITE);
}

static void
draw_vline(uint8_t x, uint8_t y, uint8_t h)
{
	arduboy.drawFastVLine(x, y, h, WHITE);
}
static void
draw_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1)
{
	arduboy.drawLine(x0, y0, x1, y1, WHITE);
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
	PLAYER_MAX_STATES,
};

/* delay per frame in each state */
static const uint8_t player_timings[PLAYER_MAX_STATES] = {
	FPS / 5, /* moving left */
	FPS / 5, /* moving right */
	FPS / 1, /* resting */
};
/* offsets in number of frames per state */
static const uint8_t player_frame_offsets[PLAYER_MAX_STATES] = {
	  0, /* moving left */
	  4, /* moving right */
	  8, /* resting */
};

static void player_set_state(uint8_t new_state)
{
	if (new_state != cs.state) {
		cs.frame = 0;
		cs.atime = player_timings[new_state];
	}

	if (new_state != PLAYER_RESTS)
		start_timer(TIMER_PLAYER_RESTS, PLAYER_REST_TIMEOUT);

	if (cs.state != PLAYER_RESTS)
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
		if (cs.x > 0)
			cs.x--;
	}
	if (dx > 0) {
		player_set_state(PLAYER_R_MOVE);
		if (cs.x < 116)
			cs.x++;
	}

	/* XXX might introduce timeout to reduce fire rate */
	if (throws)
		player_set_state(cs.previous_state);

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
};

static struct weapon_states {
	uint8_t selected:3; /* selected weapon */
	uint8_t previous:3; /* previous selected weapon */
	uint8_t direction:1;
	uint8_t stime;
	int8_t icon_x;
	uint8_t ammo[NR_WEAPONS]; /* available ammo per weapon */
	struct bullet bs[NR_BULLETS];
} ws;

/* damage per bullet */
static const uint8_t bullet_damage[NR_WEAPONS] = {1, 2, 3, 4};
/* nr of frames weapon is effective on ground */
static const uint8_t etime[NR_WEAPONS] = {FPS / 2, FPS / 2, FPS, FPS * 2};
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

static const uint8_t lane_y[3] = {52, 56, 62};

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
		bs->atime = BULLET_FRAME_TIME;
		bs->etime = etime[weapon];
		bs->frame = 0;
		ws.ammo[weapon]--;
		break;
	}
	return 1;
}

static void update_bullets(void)
{
	uint8_t b, height;
	struct bullet *bs = &ws.bs[0];

	for (b = 0; b < NR_BULLETS; b++, bs++) {
		if (bs->state == BULLET_INACTIVE)
			continue;

		height = img_height(water_bomb_air_img);
		if (bs->state >= BULLET_EFFECT) {
			if (bs->etime == 0) {
				bs->state = BULLET_INACTIVE;
				ws.ammo[bs->weapon]++;
			} else
				bs->etime--;
		} else {
			/* next movement */
			if (bs->ys == lane_y[bs->lane] - height) {
				bs->frame = 0;
				bs->state = BULLET_EFFECT;
			}
			bs->ys++;
		}
		/* next animation */
		if (bs->atime == 0) {
			bs->atime = BULLET_FRAME_TIME;
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

void select_weapon(int8_t up_down)
{
	ws.previous = ws.selected;
	if (cs.x < 64 - img_width(player_all_frames_img) / 2) {
		ws.direction = 1;
		ws.icon_x = WIDTH;
	} else {
		ws.direction = 0;
		ws.icon_x = -img_width(weapons_img);
	}
	if (up_down > 0) {
		/* select weapon downwards */
		if (ws.selected == NR_WEAPONS - 1)
			ws.selected = 0;
		else
			ws.selected++;
	} else {
		/* select weapon upwards */
		if (ws.selected == 0)
			ws.selected = NR_WEAPONS - 1;
		else
			ws.selected--;
	}
}

/*---------------------------------------------------------------------------
 * enemy handling
 *---------------------------------------------------------------------------*/
enum enemy_types {
	/* vicious enemies */
	ENEMY_VICIOUS,
	ENEMY_RAIDER = ENEMY_VICIOUS,
	/* bosses */
	ENEMY_BOSSES,
	ENEMY_BOSS1 = ENEMY_BOSSES,
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
	ENEMY_SWEARING,
	ENEMY_DYING,
	ENEMY_DEAD,
	ENEMY_MAX_STATE,
};

static const uint8_t boss_sprite_offsets[ENEMY_MAX_STATE] = {
	0, /* not used, approach door */
	0, /* walking left */
	0, /* walking right */
	0, /* TODO, effect */
	0, /* not used, change lane */
	0, /* not used, move to upper lane */
	0, /* not used, move to lower lane */
	4, /* attacking */
	8, /* resting */
	0, /* swearing, used for peaceful enemies */
	0, /* not used, dying */
	0, /* not used, dead */
};

static const uint8_t enemy_sprite_offsets[ENEMY_MAX_STATE] = {
	 0, /* not used, approach door */
	 0, /* walking left */
	 4, /* walking right */
	 0, /* TODO, effect */
	 0, /* not used, change lane */
	 0, /* not used, move to upper lane */
	 0, /* not used, move to lower lane */
	 8, /* attacking */
	12, /* resting */
	 0, /* swearing, used for peaceful enemies */
	 0, /* not used, dying */
	 0, /* not used, dead */
};

static const uint8_t peaceful_sprite_offsets[ENEMY_MAX_STATE] = {
	0, /* not used, approach door */
	0, /* walking left */
	0, /* walking right */
	0, /* TODO, effect */
	0, /* not used, change lane */
	0, /* not used, move to upper lane */
	0, /* not used, move to lower lane */
	0, /* attacking */
	0, /* resting */
	4, /* swearing, used for peaceful enemies */
	0, /* not used, dying */
	0, /* not used, dead */
};

static const uint8_t enemy_damage[ENEMY_MAX] = {
	1, 10, 0,
};

static const int8_t enemy_life[ENEMY_MAX] = {
	16, 64, 32,
};

static const int8_t enemy_mtime[ENEMY_MAX] = {
	FPS / 20,
	FPS / 10,
	FPS / 5,
};

static const int8_t enemy_rtime[ENEMY_MAX] = {
	FPS,
	FPS * 2,
	FPS * 2,
};

static const int8_t enemy_atime[ENEMY_MAX] = {
	FPS / 10,
	FPS / 10,
	FPS / 10,
};

static const int16_t enemy_score[ENEMY_MAX] = {
	10,
	200,
	-100,
};

static const uint8_t *enemy_sprites[ENEMY_MAX] = {
	enemy_raider_img,
	enemy_boss_img,
	enemy_grandma_img,
};

struct enemy {
	int8_t life;
	int8_t x;
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
	uint8_t previous_state[4];
	uint8_t pindex;
	uint8_t sprite_offset;
};

struct door {
	uint8_t under_attack:1;
	struct enemy *attacker;
	uint8_t boss_countdown;
};

static struct enemy enemies[MAX_ENEMIES];
static struct door door;

static uint8_t enemy_random_type(void)
{
	uint8_t r = random8(100);

	if (!door.boss_countdown) {
		door.boss_countdown = KILLS_TILL_BOSS;
		return ENEMY_BOSS1;
	}

	if (r < 9)
		return ENEMY_GRANDMA;
	else
		return ENEMY_RAIDER;
}

static void enemy_push_state(struct enemy *e)
{
	e->previous_state[e->pindex] = e->state;
	e->pindex = (e->pindex + 1) % 4;
}

static uint8_t enemy_pop_state(struct enemy *e)
{
	if (e->pindex == 0)
		e->pindex = 4;
	e->pindex--;
	return e->previous_state[e->pindex];
}

static void enemy_set_state(struct enemy *e, uint8_t state, uint8_t push)
{
	const uint8_t *so;

	if (e->type >= ENEMY_PEACEFUL)
		so = peaceful_sprite_offsets;
	else if (e->type >= ENEMY_BOSSES)
		so = boss_sprite_offsets;
	else
		so = enemy_sprite_offsets;

	if (push)
		enemy_push_state(e);

	switch (e->state) {
	case ENEMY_CHANGE_LANE:
	case ENEMY_MOVE_TO_UPPER_LANE:
	case ENEMY_MOVE_TO_LOWER_LANE:
		if (e->dx < e->x)
			e->sprite_offset = so[ENEMY_WALKING_LEFT];
		else
			e->sprite_offset = so[ENEMY_WALKING_RIGHT];
		break;
	default:
		e->sprite_offset = so[state];
		break;
	}
	e->state = state;
	if (e->state != ENEMY_EFFECT)
		e->frame = 0;
}

static void spawn_new_enemies(void)
{
	uint8_t i, height;
	struct enemy *e = &enemies[0];

	/* update and spawn enemies */
	for (i = 0; i < MAX_ENEMIES; i++, e++) {
		if (e->active)
			continue;
		memset(e, 0, sizeof(*e));
		e->active = 1;
		e->type = enemy_random_type();
		height = enemy_sprites[e->type][1];
		e->lane = 1 + random8(2);
		e->y = lane_y[e->lane] - height;
		e->x = 127;
		e->mtime = enemy_mtime[e->type];
		e->rtime = enemy_rtime[e->type];
		e->atime = enemy_atime[e->type];
		e->life = enemy_life[e->type];
		e->damage = enemy_damage[e->type];
		enemy_set_state(e, ENEMY_WALKING_LEFT, 0);
		enemy_set_state(e, ENEMY_WALKING_LEFT, 1);
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

	if (e->mtime)
		return;

	if (e->dx > e->x)
		e->x++;

	if (e->dx < e->x)
		e->x--;

	if (e->y > y)
		e->y--;
	else
		e->y++;
}

static void enemy_prepare_direction_change(struct enemy *e, uint8_t width)
{
	e->dlane = 1 + random8(2);
	e->dx = 1 + e->x + random8(WIDTH - e->x - width - abs(lane_y[e->lane] - lane_y[e->dlane]));
}

static void update_enemies(void)
{
	uint8_t damage = 0;
	uint8_t i, width, height;
	struct enemy *e = &enemies[0];
	struct enemy *a;

	/* update and spawn enemies */
	for (i = 0; i < MAX_ENEMIES; i++, e++) {
		if (!e->active)
			continue;
		width = img_width(enemy_sprites[e->type]);
		height = img_height(enemy_sprites[e->type]);
		/* check if hit by bullet */
		if (e->life > 0)
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
				enemy_set_state(e, ENEMY_DYING, 0);
			} else {
				if (e->state != ENEMY_EFFECT && e->state != ENEMY_SWEARING) {
					if (e->damage)
						enemy_set_state(e, ENEMY_EFFECT, 1);
					else
						enemy_set_state(e, ENEMY_SWEARING, 1);
				}
			}
		}
		switch (e->state) {
		case ENEMY_WALKING_LEFT:
			if (e->mtime)
				break;

			if (e->damage) {
				if (e->x == (6 + lane_y[e->lane] - lane_y[DOOR_LANE]))
					enemy_set_state(e, ENEMY_APPROACH_DOOR, 1);
			} else {
				/* TODO peaceful enemies just pass by */
				if (e->x == -16)
					e->active = 0;
			}
			e->x--;
			break;
		case ENEMY_APPROACH_DOOR:
			a = door.attacker;
			if (door.under_attack && a != e) {
				if (e->type >= ENEMY_BOSSES && e->type < ENEMY_PEACEFUL) {
					enemy_prepare_direction_change(a, width);
					enemy_set_state(a, ENEMY_WALKING_RIGHT, 1);
					enemy_set_state(a, ENEMY_CHANGE_LANE, 1);
				} else {
					/* door is already under attack, take a walk */
					enemy_prepare_direction_change(e, width);
					enemy_set_state(e, ENEMY_WALKING_RIGHT, 1);
					break;
				}
			}
			door.under_attack = 1;
			door.attacker = e;
			e->dlane = DOOR_LANE;
			if (e->lane != e->dlane) {
				e->dx = e->x - abs(lane_y[e->lane] - lane_y[e->dlane]);
				enemy_set_state(e, ENEMY_CHANGE_LANE, 1);
			} else
				enemy_set_state(e, ENEMY_ATTACKING, 1);
			break;
		case ENEMY_WALKING_RIGHT:
			if (e->mtime)
				break;

			if (e->x == (e->dx - abs(lane_y[e->lane] - lane_y[e->dlane]))) {
				/* randomly change lane */
				if (e->lane != e->dlane)
					enemy_set_state(e, ENEMY_CHANGE_LANE, 1);
				else
					enemy_set_state(e, ENEMY_WALKING_LEFT, 1);
			} else
				e->x++;
			break;
		case ENEMY_EFFECT:
			if (e->atime == 0)
				enemy_set_state(e, enemy_pop_state(e), 0);
			break;
		case ENEMY_CHANGE_LANE:
			if (e->lane != e->dlane) {
				if (e->lane > e->dlane)
					enemy_set_state(e, ENEMY_MOVE_TO_UPPER_LANE, 1);
				else
					enemy_set_state(e, ENEMY_MOVE_TO_LOWER_LANE, 1);
			} else
				enemy_set_state(e, enemy_pop_state(e), 0);
			break;
		case ENEMY_MOVE_TO_UPPER_LANE:
		case ENEMY_MOVE_TO_LOWER_LANE:
			if (e->lane != e->dlane)
				enemy_switch_lane(e,
						  e->dlane,
						  lane_y[e->dlane] - height);
			else
				enemy_set_state(e, enemy_pop_state(e), 0);
			break;
		case ENEMY_ATTACKING:
			/* do door damage */
			if (e->frame == 3 && e->atime == 0) {
				cs.life -= enemy_damage[e->type];
				enemy_set_state(e, ENEMY_RESTING, 1);
			}
			break;
		case ENEMY_SWEARING:
		case ENEMY_RESTING:
			if (e->rtime == 0) {
				e->rtime = enemy_rtime[e->type];
				enemy_set_state(e, enemy_pop_state(e), 0);
			} else
				e->rtime--;
			break;
		case ENEMY_DYING:
			if (e->atime == 0)
				enemy_set_state(e, ENEMY_DEAD, 0);
			break;
		case ENEMY_DEAD:
			if (e->mtime)
				break;
			if (e->y == 0) {
				e->active = 0;
				if (e->type  < ENEMY_BOSSES)
					door.boss_countdown--;
			} else
				e->y--;
			break;
		}
		/* next animation */
		if (e->atime == 0) {
			e->atime = enemy_atime[e->type];
			e->frame++;
		} else
			e->atime--;
		/* next movement */
		if (e->mtime == 0) {
			if (e->state < ENEMY_DYING)
				e->mtime = enemy_mtime[e->type];
			else
				e->mtime = 0;
		} else
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

	/* show selected weapon icon */
	if (ws.selected != ws.previous) {
		if (ws.direction) {
			if (ws.icon_x == WIDTH - img_width(weapons_img))
				ws.previous = ws.selected;
			else {
				ws.stime = FPS;
				ws.icon_x-=2;
			}
		} else {
			if (ws.icon_x == 0)
				ws.previous = ws.selected;
			else {
				ws.stime = FPS;
				ws.icon_x+=2;
			}
		}
	}

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
static void draw_digit(uint8_t x, uint8_t y, int8_t number)
{
	if (number < 0 || number > 9)
		return;
	blit_image_frame(x, y, numbers_3x5_img, NULL, number, __flag_none);
}

static void draw_number(uint8_t x, uint8_t y, int32_t n, uint32_t divider, uint8_t flags)
{
	uint8_t digit;
	uint8_t fill = flags & 1;
	uint8_t sign = flags & 2;
	uint32_t number = abs(n);

	while (divider) {
		digit = number / divider;
		if (digit || fill) {
			if (sign) {
				if (n > 0)
					draw_vline(x + 1, y + 1, 3);
				draw_hline(x, y + 2, 3);
				sign = 0;
				x += 4;
			}
			draw_digit(x, y, digit);
			fill = 1;
		}
		number %= divider;
		divider /= 10;
		x += 4;
	}
}

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
		show = 0;
		switch (e->state) {
		case ENEMY_DYING:
		case ENEMY_EFFECT:
			show = e->atime & 1;
			break;
		case ENEMY_DEAD:
			/* draw score */
			draw_number(e->x, e->y, enemy_score[e->type], 100, 2);
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

	if (ws.stime) {
		blit_image_frame(ws.icon_x,
				 0,
				 weapons_img,
				 NULL,
				 ws.selected,
				 __flag_none);
		ws.stime--;
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
					 bomb_splash_img,
					 NULL,
					 bs->frame,
					 __flag_none);
			break;
		case BULLET_ACTIVE:
			blit_image_frame(bs->x,
					 bs->ys,
					 water_bomb_air_img,
					 water_bomb_air_mask_img,
					 bs->frame,
					 __flag_none);
			break;
		default:
			break;
		}
	}
}

static void draw_score(void)
{
	draw_number(100, 59, cs.score, 1000000, 1);
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

		memset(&door, 0, sizeof(door));
		door.boss_countdown = KILLS_TILL_BOSS;
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
			select_weapon(-1);
		} else if (down()) {
			select_weapon(1);
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
