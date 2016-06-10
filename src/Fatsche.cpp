#include <stdint.h>
#include <stdlib.h>
#include "images.h"
#include "VeritazzExtra.h"

VeritazzExtra arduboy(l1_table);

#ifndef HOST_TEST
#include <Arduino.h>
#include <avr/pgmspace.h>
#include "simple_buttons.h"

SimpleButtons buttons(arduboy);

#endif

/*---------------------------------------------------------------------------
 * game parameters
 *---------------------------------------------------------------------------*/
#define FPS                         30        /* frame rate */
#define MAX_ENEMIES                 12        /* max enemies on the scene */
#define ENEMIES_SPAWN_RATE          (FPS * 3) /* every 3 seconds */
#define PLAYER_REST_TIMEOUT         (FPS * 5) /* players timeout for resting */
#define PLAYER_MAX_LIFE             256       /* initial players life */
#define NR_WEAPONS                  4
#define MAX_AMMO_W1                 16
#define MAX_AMMO_W2                 4
#define MAX_AMMO_W3                 2
#define MAX_AMMO_W4                 1
#define NR_BULLETS                  \
	(MAX_AMMO_W1 + MAX_AMMO_W2 + MAX_AMMO_W3 + MAX_AMMO_W4)
#define MS_TO_FRAMES(m)             (((m) * FPS) / 1000)
#define WEAPON1_COOLDOWN            (FPS / 3)
#define WEAPON2_COOLDOWN            (FPS / 3)
#define WEAPON3_COOLDOWN            (FPS * 5)
#define WEAPON4_COOLDOWN            (FPS * 10)
#define MAX_FLYING_NUMBERS          (MAX_ENEMIES + MAX_POWERUPS + 10)
#define MAX_STAGES                  4
#define BULLET_FRAME_TIME           FPS / 10

#define CHAR_G                      0
#define CHAR_A                      1
#define CHAR_M                      2
#define CHAR_E                      3
#define CHAR_O                      4
#define CHAR_V                      5
#define CHAR_R                      6
#define CHAR_B                      7
#define CHAR_S                      8
#define CHAR_T                      9
#define CHAR_I                      10
#define CHAR_1                      11
#define CHAR_2                      12
#define CHAR_3                      13
#define CHAR_4                      14

static void draw_number(uint8_t x, uint8_t y, int32_t n, uint32_t divider, uint8_t flags);

/*---------------------------------------------------------------------------
 * program states
 *---------------------------------------------------------------------------*/
enum program_states {
	PROGRAM_MAIN_MENU = 0,
	PROGRAM_LOAD_GAME,
	PROGRAM_RUN_GAME,
	PROGRAM_SHOW_HELP,
	PROGRAM_STATE_MAX,
};

/*---------------------------------------------------------------------------
 * graphic functions
 *---------------------------------------------------------------------------*/
#define blit_image(a, b, c, d, e)		arduboy.drawImage(a, b, c, d, e)
#define blit_image_frame(a, b, c, d, e, f)	arduboy.drawImageFrame(a, b, c, d, e, f)

static void
draw_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h)
{
	arduboy.drawRect(x, y, w, h, WHITE);
}

static void
draw_filled_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t c)
{
	arduboy.fillRect(x, y, w, h, c);
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

/*---------------------------------------------------------------------------
 * timers
 *---------------------------------------------------------------------------*/
enum timers {
	TIMER_PLAYER_RESTS,
	TIMER_ENEMY_SPAWN,
	TIMER_POWERUP_SPAWN,
	TIMER_POWERUP_SPAWN2,
	TIMER_GP,
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
	uint8_t t = 0;
	do {
		if (!timers[t].active)
			continue;
		if (timers[t].timeout == 0) {
			timers[t].active = 0;
			if (timers[t].fn)
				timers[t].fn();
		}
		timers[t].timeout--;
	} while (++t < TIMER_MAX);
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
static uint8_t gp_timer_ticks = 0;
static void gp_timer_count_fn(void)
{
	gp_timer_ticks++;
	start_timer(TIMER_GP, FPS / 2);
}

/*---------------------------------------------------------------------------
 * misc functions
 *---------------------------------------------------------------------------*/
static uint8_t random8(uint8_t max)
{
#ifdef HOST_TEST
	return random() % max;
#else
	return random(max);
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
#define pressedA()           a()
#define pressedB()           b()
#define pressedUp()          up()
#define pressedDown()        down()
#define pressedLeft()        left()
#define pressedRight()       right()
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

uint8_t pressedA(void)
{
	return buttons.pressedA();
}
uint8_t pressedB(void)
{
	return buttons.pressedB();
}
uint8_t pressedUp(void)
{
	return buttons.pressedUp();
}
uint8_t pressedDown(void)
{
	return buttons.pressedDown();
}
uint8_t pressedLeft(void)
{
	return buttons.pressedLeft();
}
uint8_t pressedRight(void)
{
	return buttons.pressedRight();
}

#endif

/*---------------------------------------------------------------------------
 * timing
 *---------------------------------------------------------------------------*/

static uint8_t next_frame(void)
{
	if (arduboy.nextFrame()) {
#ifdef HOST_TEST
		update_inputs();
#else
		buttons.poll();
#endif
		arduboy.clear();
		run_timers();
		return 1;
	}
	return 0;
}

static void finish_frame(void)
{
	arduboy.display();
}

/*---------------------------------------------------------------------------
 * data types
 *---------------------------------------------------------------------------*/
struct rect {
	int16_t x;
	int16_t y;
	int16_t xe;
	int16_t ye;
};

struct menu_drop {
	uint8_t idx;
	uint16_t stime; /* show time of next drop */
	uint8_t atime; /* time till next frame */
	uint8_t state:4; /* state of drop */
	uint8_t frame:4; /* current frame of drop */
	uint8_t x;
	uint8_t y;
};

#define NR_OF_DROPS                           7

struct menu {
	uint8_t initialized;
	uint8_t state;
	struct menu_drop drop[NR_OF_DROPS];
};

struct player {
	int16_t life; /* remaining life of player */
	uint8_t x; /* current x position of player */
	uint8_t atime; /* nr of frames till next frame of sprite */
	uint8_t frame:2; /* current frame of sprite */
	uint8_t state:2; /* current player state */
	uint8_t previous_state:2; /* previous player state */
	uint8_t poison:1;
	uint16_t poison_timeout;
	int32_t score; /* current score */
};

enum game_states {
	GAME_STATE_INIT = 0,
	GAME_STATE_RUN_GAME,
	GAME_STATE_WON,
	GAME_STATE_OVER,
	GAME_STATE_CLEANUP,
};

struct enemy {
	int8_t life;
	int8_t x;
	uint8_t dx;
	uint8_t y;
	uint8_t type:3;
	uint8_t id:5;
	uint8_t pee_x;
	uint8_t atime; /* nr of frames it take for the next animation frame */
	uint8_t rtime; /* nr of frames it takes to rest */
	uint8_t mtime; /* nr of frames it takes to move */
	uint8_t damage;
	uint8_t frame;
	uint8_t lane:2;
	uint8_t dlane:2;
	uint8_t active:1;
	uint8_t poisoned:3;
	uint8_t poison_timeout;
	uint8_t frame_reload;
	uint8_t state;
	uint8_t previous_state[4];
	uint8_t pindex;
	uint8_t sprite_offset;
	uint8_t hit;
	uint16_t slowdown;
	uint8_t width;
	uint8_t height;
	uint16_t flags;
};

struct door {
	uint8_t under_attack:1;
	uint8_t boss:2;
	struct enemy *attacker;
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

struct weapon_states {
	uint16_t cool_down[NR_WEAPONS];
	uint8_t selected:3; /* selected weapon */
	uint8_t previous:3; /* previous selected weapon */
	uint8_t direction:1;
	uint8_t effects_active;
	uint8_t stime;
	int8_t icon_x;
	uint8_t ammo[NR_WEAPONS]; /* available ammo per weapon */
	struct bullet bs[NR_BULLETS];
};

#define MAX_POWERUPS                   2

struct power_up {
	uint8_t active;
	uint8_t type;
	struct rect r;
	uint8_t lane;
	uint8_t frame;
	uint8_t atime;
	uint16_t timeout;
};

struct stage {
	int8_t kills;
	uint8_t limits[3];
};

struct game_data {
	struct menu menu;
	uint8_t dummy[80 - sizeof(struct menu)];
	uint8_t boss_time;
	uint8_t stage_time;
	uint8_t stage_nr;
	struct stage stage;
	uint8_t ecount[3];
	struct player player;
	struct enemy enemies[MAX_ENEMIES];
	struct door door;
	struct weapon_states ws;
	struct power_up power_ups[MAX_POWERUPS];
	enum game_states game_state;
};

struct flying_number {
	uint8_t active:1;
	uint8_t y:7;
	uint8_t x;
	int16_t number;
};

static struct game_data gd;
/*---------------------------------------------------------------------------
 * setup
 *---------------------------------------------------------------------------*/
void
setup(void)
{
	arduboy.initRandomSeed();
	arduboy.setFrameRate(FPS);
	arduboy.begin();
	arduboy.clear();
	arduboy.display();
	init_timers();
}

/*---------------------------------------------------------------------------
 * bump tables
 *---------------------------------------------------------------------------*/

struct bumping_img {
	uint8_t x;
	int16_t y;
	int16_t velocity;
	int16_t gravity;
	uint8_t i:7;
	uint8_t update:1;
};

static int16_t ye;
static const int16_t velocities[] PROGMEM = {
	 8 << 8,
	-3 << 8,
	 4 << 8,
	-2 << 8,
	 2 << 8,
	-1 << 8,
	 1 << 8,
	 0,
};

static void init_bump(struct bumping_img *bi, uint8_t x, int8_t y, int16_t gravity)
{
	bi->x = x;
	bi->y = y << 8;
	bi->velocity = pgm_read_word(&velocities[0]);
	bi->gravity = gravity;
	bi->i = 0;
	bi->update = 1;
}

static void init_img_bump(int8_t y)
{
	ye = y << 8;
}

static uint8_t img_bump(struct bumping_img *bi, const uint8_t *img, uint8_t frame)
{
	int16_t yn;
	int16_t velocity = pgm_read_word(&velocities[bi->i]);

	/* TODO only draw if y changed */
	if (bi->update) {
		blit_image_frame(bi->x,
				 (int8_t)(bi->y >> 8),
				 img,
				 img,
				 frame,
				 __flag_white);
		bi->update = 0;
	}

	if (bi->i == 7) {
		bi->update = 1;
		return 1;
	}

	if (bi->y == ye && ((int8_t)(velocity >> 8)) > 0) {
		bi->i++;
		bi->velocity = velocity;
	} else if (((int8_t)(bi->velocity >> 8)) >= 0 && ((int8_t)(velocity >> 8)) < 0) {
		bi->i++;
		bi->velocity = velocity;
	}

	yn = bi->y + bi->velocity;
	if (((yn >> 8) != (bi->y >> 8)) || ((yn >> 8) != (ye >> 8)))
		bi->update = 1;
	if (yn > ye)
		bi->y = ye;
	else
		bi->y = yn;
	bi->velocity += bi->gravity;

	return 0;
}

static void init_8_char_img_bump(void)
{
	struct bumping_img *bi = (struct bumping_img *)&gd;
	init_bump(&bi[0], 5, 0, 128);
	init_bump(&bi[1], 5 + 14, -20, 100);
	init_bump(&bi[2], 5 + 2 * 14, -10, 64);
	init_bump(&bi[3], 5 + 3 * 14, -5, 80);
	init_bump(&bi[4], 10 + 4 * 14, -7, 70);
	init_bump(&bi[5], 10 + 5 * 14, -30, 110);
	init_bump(&bi[6], 10 + 6 * 14, -15, 90);
	init_bump(&bi[7], 10 + 7 * 14, -12, 120);
}

static void init_stage_text(void)
{
	struct bumping_img *bi = (struct bumping_img *)&gd;
	init_bump(&bi[0], 20, 0, 128);
	init_bump(&bi[1], 20 + 14, -20, 100);
	init_bump(&bi[2], 20 + 2 * 14, -10, 64);
	init_bump(&bi[3], 20 + 3 * 14, -5, 80);
	init_bump(&bi[4], 20 + 4 * 14, -7, 70);
	init_bump(&bi[5], 25 + 5 * 14, -30, 110);
	gd.stage_time = FPS;
}
/*---------------------------------------------------------------------------
 * text printing functions
 *---------------------------------------------------------------------------*/
#define __text_centered                (1 << 0)

static uint8_t line_length(const char *t)
{
	char c;
	uint8_t len = 0;

	for (;;) {
		c = pgm_read_byte(t++);
		if (!c || c == '\n')
			break;
		len++;
	}
	return len;
}

static void print_text(const char *t, uint8_t x, uint8_t y, uint8_t options)
{
	uint8_t cx, cy, flags, len = 0;
	char c;

	if (options & __text_centered) {
		len = line_length(t);
		x = (WIDTH - len * 4) / 2;
	}

	cx = x;
	cy = y;

	for (;;) {
		flags = 0;

		if (len == 0)
			break;

		c = pgm_read_byte(t++);
		if (!c)
			break;
		switch (c) {
		case ' ':
			flags |= 1;
			break;
		case '\n':
			if (options & __text_centered) {
				len = line_length(t);
				x = (WIDTH - len * 4) / 2;
			}
			cy += 5;
			cx = x;
			flags |= 3;
			break;
		default:
			if (c <= 57)
				c -= 48;
			else if (c <= 122)
				c -= 87;
			break;
		}
		if ((flags & 1) == 0)
			blit_image_frame(cx,
					 cy,
					 characters_3x4_img,
					 NULL,
					 c,
					 __flag_white);
		if ((flags & 2) == 0) {
			cx += 4;
			if (cx >= WIDTH - 4) {
				cx = x;
				cy += 5;
			}
		}
	}
}
/*---------------------------------------------------------------------------
 * flying numbers
 *---------------------------------------------------------------------------*/
static struct flying_number flying_numbers[MAX_FLYING_NUMBERS];

static void init_flying_numbers(void)
{
	memset(flying_numbers, 0, sizeof(flying_numbers));
}

static void add_flying_number(uint8_t x, uint8_t y, int16_t number)
{
	uint8_t n = 0;

	do {
		if (!flying_numbers[n].active) {
			flying_numbers[n].x = x;
			flying_numbers[n].y = y;
			flying_numbers[n].number = number;
			flying_numbers[n].active = 1;
			break;
		}
	} while (++n < MAX_FLYING_NUMBERS);
}

static void update_flying_numbers(void)
{
	uint8_t n = 0;

	do {
		if (flying_numbers[n].active) {
			if (flying_numbers[n].y == 0)
				flying_numbers[n].active = 0;
			else
				flying_numbers[n].y--;
		}
	} while (++n < MAX_FLYING_NUMBERS);
}

static void draw_flying_numbers(void)
{
	uint8_t n = 0;

	do {
		if (flying_numbers[n].active)
			draw_number(flying_numbers[n].x,
				    flying_numbers[n].y,
				    flying_numbers[n].number,
				    10000, 2);
	} while (++n < MAX_FLYING_NUMBERS);
}
/*---------------------------------------------------------------------------
 * main menu handling
 *---------------------------------------------------------------------------*/
enum menu_states {
	MENU_STATE_PLAY,
	MENU_STATE_LOAD,
	MENU_STATE_HELP,
};

struct menu_data {
	uint8_t n_game_state;
	uint8_t x;
};

static const struct menu_data menu_item_xlate[] = {
	[MENU_STATE_PLAY] = { .n_game_state = PROGRAM_RUN_GAME, .x = 19, },
	[MENU_STATE_LOAD] = { .n_game_state = PROGRAM_LOAD_GAME, .x = 51, },
	[MENU_STATE_HELP] = { .n_game_state = PROGRAM_SHOW_HELP, .x = 82, },
};

static const uint8_t menu_drop_x_locations[NR_OF_DROPS] = {
	9, 38, 50, 71, 85, 102, 119,
};

static const uint8_t menu_drop_y_locations[NR_OF_DROPS] = {
	39, 35, 32, 30, 33, 35, 39,
};

static const uint8_t menu_drop_state_frame_offsets[] = {
	0, 4, 0, 8,
};

static const uint8_t menu_drop_atime[] = {
	6, 3, 1, 2,
};

static void menu_drop_init(struct menu_drop *drop)
{
	drop->stime = random8(10) * FPS;
	drop->atime = menu_drop_atime[0];
	drop->state = 0;
	drop->frame = 0;
	drop->x = menu_drop_x_locations[drop->idx];
	drop->y = menu_drop_y_locations[drop->idx];
}

static uint8_t
mainscreen(void)
{
	uint8_t game_state = PROGRAM_MAIN_MENU;
	const struct menu_data *data;
	struct menu_drop *drop;
	struct menu *menu = &gd.menu;
	uint8_t i;

	if (pressedLeft()) {
		if (menu->state == MENU_STATE_PLAY)
			menu->state = MENU_STATE_HELP;
		else
			menu->state--;
	} else if (pressedRight()) {
		if (menu->state == MENU_STATE_HELP)
			menu->state = MENU_STATE_PLAY;
		else
			menu->state++;
	}

	if (!menu->initialized) {
		i = 0;
		do {
			drop = &menu->drop[i];
			drop->idx = i;
			menu_drop_init(drop);
		} while (++i < NR_OF_DROPS);
		menu->initialized = 1;
	}

	data = &menu_item_xlate[menu->state];
	if (a())
		game_state = data->n_game_state;

	blit_image(3,
		   10,
		   mainscreen_img,
		   NULL,
		   __flag_white);
	draw_rect(0, 0, WIDTH, HEIGHT);

	i = 0;
	do {
		drop = &menu->drop[i];
		if (drop->stime)
			continue;
		if (drop->state != 2) {
			blit_image_frame(drop->x,
					 drop->y,
					 menu_drops_img,
					 NULL,
					 menu_drop_state_frame_offsets[drop->state] + drop->frame,
					 __flag_white);
		} else
			draw_rect(drop->x, drop->y + 4, 2, 2);
	} while (++i < NR_OF_DROPS);

	draw_hline(data->x, 51, 26);
	draw_vline(data->x - 1, 52, 9);

	i = 0;
	do {
		drop = &menu->drop[i];

		if (drop->stime) {
			drop->stime--;
			continue;
		}
		/* show time */
		if (drop->atime) {
			drop->atime--;
			continue;
		}

		if (drop->frame == 3 && drop->state != 2) {
			drop->frame = 0;
			drop->state++;
		}

		if (drop->state == 2 && drop->y >= 55) {
			drop->frame = 0;
			drop->state++;
		}

		if (drop->state == 4)
			menu_drop_init(drop);

		switch (drop->state) {
		case 0:
			drop->x = menu_drop_x_locations[drop->idx];
			drop->y = menu_drop_y_locations[drop->idx];
			break;
		case 1:
			drop->y = menu_drop_y_locations[drop->idx] + 7;
			break;
		case 2:
			drop->x = menu_drop_x_locations[drop->idx] + 2;
			drop->y += 3;
			break;
		case 3:
			drop->x = menu_drop_x_locations[drop->idx];
			drop->y = 55;
			break;
		}
		drop->atime = menu_drop_atime[drop->state];
		drop->frame++;
	} while (++i < NR_OF_DROPS);

	return game_state;
}

static uint8_t
load(void)
{
	return PROGRAM_MAIN_MENU;
}

static uint8_t
help(void)
{
	uint8_t rstate = PROGRAM_SHOW_HELP;

	switch (gd.game_state) {
	case GAME_STATE_INIT:
		init_timers();
		/* setup general purpose timer to 1s */
		gp_timer_ticks = 0;
		setup_timer(TIMER_GP, gp_timer_count_fn);
		start_timer(TIMER_GP, FPS);
		delay(500);
		gd.game_state = GAME_STATE_RUN_GAME;
		break;
	case GAME_STATE_RUN_GAME:
		blit_image(0, 0, help_screen_img, NULL, __flag_white);
		if (gp_timer_ticks & 1)
			blit_image(118, 55, icon_a_img, NULL, __flag_white);
		if (pressedA())
			gd.game_state = GAME_STATE_CLEANUP;
		break;
	case GAME_STATE_CLEANUP:
		init_timers();
		memset(&gd, 0, sizeof(gd));
		rstate = PROGRAM_MAIN_MENU;
		delay(500);
		break;
	}
	return rstate;
}

/*---------------------------------------------------------------------------
 * game handling
 *---------------------------------------------------------------------------*/
static inline void player_set_poison(void)
{
	struct player *p = &gd.player;

	p->poison = 1;
	p->poison_timeout = 20 * FPS;
}

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
	struct player *p = &gd.player;

	if (new_state != p->state) {
		p->frame = 0;
		p->atime = player_timings[new_state];
	}

	if (new_state != PLAYER_RESTS)
		start_timer(TIMER_PLAYER_RESTS, PLAYER_REST_TIMEOUT);

	p->previous_state = p->state;
	p->state = new_state;
}

static void player_is_resting(void) {
	player_set_state(PLAYER_RESTS);
}

static void update_player(int8_t dx, uint8_t throws)
{
	struct player *p = &gd.player;

	/* update position */
	if (dx < 0) {
		player_set_state(PLAYER_L_MOVE);
		if (p->x > 0)
			p->x--;
	}
	if (dx > 0) {
		player_set_state(PLAYER_R_MOVE);
		if (p->x < 116)
			p->x++;
	}

	if (throws && p->state == PLAYER_RESTS)
		player_set_state(p->previous_state);
	else if (throws)
		start_timer(TIMER_PLAYER_RESTS, PLAYER_REST_TIMEOUT);

	/* update frames */
	if (p->atime == 0) {
		p->atime = player_timings[p->state];
		p->frame++;
	} else
		p->atime--;

	if (p->poison) {
		p->poison_timeout--;
		if (p->poison_timeout == 0)
			p->poison = 0;
	}
}

static void init_player(void)
{
	struct player *p = &gd.player;

	memset(p, 0, sizeof(*p));
	p->life = PLAYER_MAX_LIFE;
	p->x = 20;
	/* setup timer for players resting animation */
	setup_timer(TIMER_PLAYER_RESTS, player_is_resting);
	start_timer(TIMER_PLAYER_RESTS, PLAYER_REST_TIMEOUT);
}

/*---------------------------------------------------------------------------
 * bullet handling
 *---------------------------------------------------------------------------*/
enum bullet_state {
	BULLET_INACTIVE,
	BULLET_SPLASH,
	BULLET_ACTIVE,
	BULLET_EFFECT,
};

enum weapon_type {
	WEAPON_WATER,
	WEAPON_POO,
	WEAPON_OIL,
	WEAPON_MOLOTOV,
};

/* damage per bullet */
static const uint8_t bullet_damage_table[NR_WEAPONS] = {1, 4, 1, 16};

/* nr of frames weapon is effective on ground */
static const uint16_t etime[NR_WEAPONS] = {
	MS_TO_FRAMES(500), /* water */
	MS_TO_FRAMES(500), /* poo */
	FPS * 3, /* oil */
	FPS * 2, /* molotov */
};

/* maximum number of ammo per weapon */
static const uint8_t max_ammo[NR_WEAPONS] = {
	MAX_AMMO_W1,
	MAX_AMMO_W2,
	MAX_AMMO_W3,
	MAX_AMMO_W4,
};

/* maximum number of ammo per weapon */
static const uint16_t weapon_cool_down[NR_WEAPONS] = {
	WEAPON1_COOLDOWN,
	WEAPON2_COOLDOWN,
	WEAPON3_COOLDOWN,
	WEAPON4_COOLDOWN,
};

static uint8_t do_damage_from_table(struct bullet *b, struct rect *r)
{
	if (!(b->x < r->xe && (b->x + 4) > r->x && b->ys < r->ye && (b->ys + 4) > r->y))
		return 0;

	if (b->state == BULLET_ACTIVE)
		b->etime = FPS / 4;
	b->state = BULLET_SPLASH;
	return bullet_damage_table[b->weapon];
}

static uint8_t do_damage_from_explosion(struct bullet *b, struct rect *r)
{
	int16_t x1, x2;

	if (b->state != BULLET_EFFECT)
		return 0;

	if (b->etime != etime[b->weapon])
		return 0;

	x1 = b->x - 30;
	x2 = b->x + 30;

	if (r->x >= x1 && r->x <= x2)
		return bullet_damage_table[b->weapon];

	return 0;
}

typedef uint8_t (*damage_fn_t)(struct bullet *b, struct rect *r);

static const damage_fn_t bullet_damage[NR_WEAPONS] = {
	do_damage_from_table,
	do_damage_from_table,
	do_damage_from_explosion,
	do_damage_from_explosion,
};

#define DOOR_LANE			0
#define UPPER_LANE			1
#define LOWER_LANE			2

static const uint8_t lane_y[3] = {52, 56, 62};
static const uint8_t lane_xlate[3] = {UPPER_LANE, UPPER_LANE, LOWER_LANE};

static uint8_t new_bullet(uint8_t lane, uint8_t weapon)
{
	uint8_t b, x = 0;
	struct bullet *bs;
	struct player *p = &gd.player;

	if (gd.ws.ammo[weapon] == 0)
		return 0;

	if (gd.ws.cool_down[weapon])
		return 0;

	gd.ws.cool_down[weapon] = weapon_cool_down[weapon];

	if (p->state != PLAYER_RESTS)
		b = p->state;
	else
		b = p->previous_state;

	switch (b) {
	case PLAYER_L_MOVE:
		x = p->x;
		break;
	case PLAYER_R_MOVE:
		x = p->x + img_width(player_all_frames_img) -
		    img_width(water_bomb_air_img); /* TODO this might be different for other weapons */
		break;
	}

	/* create a new bullet, do nothing if not possible */
	b = 0;
	do {
		bs = &gd.ws.bs[b];
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
		gd.ws.ammo[weapon]--;
		break;
	} while (++b < NR_BULLETS);
	return 1;
}

static void update_bullets(void)
{
	uint8_t b = 0, height;
	struct bullet *bs;

	do {
		if (gd.ws.cool_down[b])
			gd.ws.cool_down[b]--;
	} while (++b < NR_WEAPONS);

	b = 0;
	do {
		bs = &gd.ws.bs[b];
		if (bs->state == BULLET_INACTIVE)
			continue;

		height = img_height(water_bomb_air_img);
		if (bs->state == BULLET_EFFECT || bs->state == BULLET_SPLASH) {
			if (bs->etime == 0) {
				if (bs->state == BULLET_EFFECT)
					gd.ws.effects_active--;
				bs->state = BULLET_INACTIVE;
				gd.ws.ammo[bs->weapon]++;
			} else
				bs->etime--;
		} else {
			/* next movement */
			if (bs->ys == lane_y[bs->lane] - height) {
				bs->frame = 0;
				bs->state = BULLET_EFFECT;
				gd.ws.effects_active++;
			}
			bs->ys++;
		}
		/* next animation */
		if (bs->atime == 0) {
			bs->atime = BULLET_FRAME_TIME;
			bs->frame++;
		} else
			bs->atime--;
	} while (++b < NR_BULLETS);
}

static uint8_t get_bullet_damage(uint8_t lane, struct rect *r)
{
	uint8_t b = 0, damage = 0;
	struct bullet *bs;

	do {
		bs = &gd.ws.bs[b];
		if (bs->state < BULLET_ACTIVE)
			continue;
		if (bs->lane != lane_xlate[lane])
			continue;
		damage += bullet_damage[bs->weapon](bs, r);
	} while (++b < NR_BULLETS);

	return damage;
}

static void get_bullet_effect(uint8_t lane, struct enemy *e)
{
	int16_t x1, x2;
	uint8_t b = 0;
	struct bullet *bs;

	do {
		bs = &gd.ws.bs[b];
		if (bs->weapon != WEAPON_OIL)
			continue;
		if (bs->state != BULLET_EFFECT)
			continue;
		if (bs->lane != lane_xlate[lane])
			continue;

		x1 = bs->x - 30;
		x2 = bs->x + 30;

		if (e->x >= x1 && e->x <= x2 && !e->slowdown) {
			e->slowdown = FPS * 5;
			break;
		}
	} while (++b < NR_BULLETS);
}

void select_weapon(int8_t up_down)
{
	struct player *p = &gd.player;

	gd.ws.previous = gd.ws.selected;
	if (p->x < 64 - img_width(player_all_frames_img) / 2) {
		gd.ws.direction = 1;
		gd.ws.icon_x = WIDTH;
	} else {
		gd.ws.direction = 0;
		gd.ws.icon_x = -img_width(weapons_img);
	}
	if (up_down > 0) {
		/* select weapon downwards */
		if (gd.ws.selected == NR_WEAPONS - 1)
			gd.ws.selected = 0;
		else
			gd.ws.selected++;
	} else {
		/* select weapon upwards */
		if (gd.ws.selected == 0)
			gd.ws.selected = NR_WEAPONS - 1;
		else
			gd.ws.selected--;
	}
}

static void init_weapons(void)
{
	uint8_t i = 0;

	memset(&gd.ws, 0, sizeof(gd.ws));
	do {
		gd.ws.ammo[i] = max_ammo[i];
	} while (++i < NR_WEAPONS);
}

/*---------------------------------------------------------------------------
 * stage handling
 *---------------------------------------------------------------------------*/
static const struct stage game_stages[MAX_STAGES] PROGMEM = {
	{ 1, 3, 2, 0},
	{ 1, 3, 3, 1},
	{ 1, 5, 3, 2},
	{ 1, 5, 4, 2},
};

static void init_stage(void)
{
	struct stage *s = &gd.stage;
#ifdef HOST_TEST
	memcpy(s, &game_stages[0], sizeof(*s));
#else
	memcpy_P(s, &game_stages[0], sizeof(*s));
#endif
	init_stage_text();
}

static void update_stage(void)
{
	struct stage *s = &gd.stage;

	if (s->kills <= 0 && !gd.door.boss) {
		init_8_char_img_bump();
		gd.boss_time = FPS;
		gd.door.boss = 3;
	}

	if (gd.door.boss == 1 && gd.stage_nr < MAX_STAGES) {
		init_stage_text();
		gd.door.boss = 0;
		gd.stage_nr++;
#ifdef HOST_TEST
		memcpy(s, &game_stages[gd.stage_nr], sizeof(*s));
#else
		memcpy_P(s, &game_stages[gd.stage_nr], sizeof(*s));
#endif
	}
}
/*---------------------------------------------------------------------------
 * enemy handling
 *---------------------------------------------------------------------------*/
enum enemy_types {
	ENEMY_VICIOUS,
	ENEMY_PEACEFUL,
	ENEMY_THIEF,
	ENEMY_BOSS,
	ENEMY_MAX_TYPES,
};

enum enemies {
	/* vicious enemies */
	ENEMY_RAIDER,
	ENEMY_DRUNKEN_PUNK,
	/* thiefs */
	ENEMY_HACKER,
	/* bosses */
	ENEMY_BOSS1,
	/* peaceful enemies */
	ENEMY_GRANDMA,
	ENEMY_LITTLE_GIRL,
	ENEMY_MAX,
};

enum enemy_state {
	ENEMY_APPROACH_DOOR,
	ENEMY_WALKING_LEFT,
	ENEMY_WALKING_RIGHT,
	ENEMY_ATTACKING,
	ENEMY_RESTING_SWEARING,
	ENEMY_SPECIAL,
	ENEMY_DYING,
	ENEMY_DEAD,
	ENEMY_MAX_STATE,
};

static const uint8_t boss_enemy_sprite_offsets[ENEMY_MAX_STATE] = {
	0, /* not used, approach door */
	0, /* walking left */
	0, /* walking right */
	4, /* attacking */
	8, /* resting/swearing */
	0, /* special */
	0, /* not used, dying */
	0, /* not used, dead */
};

static const uint8_t vicious_enemy_sprite_offsets[ENEMY_MAX_STATE] = {
	 0, /* not used, approach door */
	 0, /* walking left */
	 0, /* walking right */
	 4, /* attacking */
	 8, /* resting/swearing */
	12, /* special */
	 0, /* not used, dying */
	 0, /* not used, dead */
};

static const uint8_t peaceful_enemy_sprite_offsets[ENEMY_MAX_STATE] = {
	0, /* not used, approach door */
	0, /* walking left */
	0, /* walking right */
	0, /* attacking */
	4, /* resting/swearing */
	0, /* special */
	0, /* not used, dying */
	0, /* not used, dead */
};

static const uint8_t *enemy_sprite_offsets[ENEMY_MAX] = {
	vicious_enemy_sprite_offsets,
	vicious_enemy_sprite_offsets,
	vicious_enemy_sprite_offsets,
	boss_enemy_sprite_offsets,
	peaceful_enemy_sprite_offsets,
	peaceful_enemy_sprite_offsets,
};

static const uint8_t enemy_damage[ENEMY_MAX] = {
	1, 1, 0, 20, 0, 0,
};

static const int8_t enemy_life[ENEMY_MAX] = {
	16, 16, 16, 64, 32, 32,
};

static const int8_t enemy_mtime[ENEMY_MAX] PROGMEM = {
	MS_TO_FRAMES(100),
	MS_TO_FRAMES(150),
	MS_TO_FRAMES(170),
	MS_TO_FRAMES(200),
	MS_TO_FRAMES(250),
	MS_TO_FRAMES(350),
};

static const int8_t enemy_rtime[ENEMY_MAX] PROGMEM = {
	FPS * 2,
	FPS,
	FPS,
	FPS * 2,
	6,
	0,
};

static const int8_t enemy_atime[ENEMY_MAX] PROGMEM = {
	MS_TO_FRAMES(150),
	MS_TO_FRAMES(150),
	MS_TO_FRAMES(150),
	MS_TO_FRAMES(200),
	MS_TO_FRAMES(180),
	MS_TO_FRAMES(180),
};

static const int16_t enemy_score[ENEMY_MAX] PROGMEM = {
	100,
	150,
	50,
	4000,
	-200,
	-500,
};

static const uint8_t *enemy_sprites[ENEMY_MAX] = {
	enemy_raider_img,
	enemy_drunken_punk_img,
	enemy_hacker_img,
	enemy_boss_img,
	enemy_grandma_img,
	enemy_little_girl_img,
};

static const uint8_t *enemy_masks[ENEMY_MAX] = {
	enemy_raider_mask_img,
	enemy_drunken_punk_mask_img,
	enemy_hacker_mask_img,
	enemy_boss_mask_img,
	enemy_grandma_mask_img,
	enemy_little_girl_mask_img,
};

static const uint8_t enemy_default_frame_reloads[] = {
	4, /* ENEMY_APPROACH_DOOR */
	4, /* ENEMY_WALKING_LEFT */
	4, /* ENEMY_WALKING_RIGHT */
	4, /* ENEMY_ATTACKING */
	4, /* ENEMY_RESTING_SWEARING */
	3, /* ENEMY_SPECIAL */
	4, /* ENEMY_DYING */
	4, /* ENEMY_DEAD */
};

static const uint8_t enemy_little_girl_frame_reloads[] = {
	 4, /* ENEMY_APPROACH_DOOR */
	 4, /* ENEMY_WALKING_LEFT */
	 4, /* ENEMY_WALKING_RIGHT */
	 4, /* ENEMY_ATTACKING */
	12, /* ENEMY_RESTING_SWEARING */
	 4, /* ENEMY_SPECIAL */
	 4, /* ENEMY_DYING */
	 4, /* ENEMY_DEAD */
};

static const uint8_t *enemy_frame_reloads[ENEMY_MAX] = {
	enemy_default_frame_reloads,
	enemy_default_frame_reloads,
	enemy_default_frame_reloads,
	enemy_default_frame_reloads,
	enemy_default_frame_reloads,
	enemy_little_girl_frame_reloads,
};

static uint8_t enemy_generate_random(struct enemy *e)
{
	uint8_t r = random8(100);
	uint8_t id, type;
	struct stage *s = &gd.stage;

	if (gd.door.boss == 3) {
		gd.door.boss = 2;
		id = ENEMY_BOSS1;
		type = ENEMY_BOSS;
	} else if (r < 9 && (gd.ecount[ENEMY_PEACEFUL] < s->limits[ENEMY_PEACEFUL])) {
		id = ENEMY_GRANDMA;
		type = ENEMY_PEACEFUL;
	} else if (r < 18 && (gd.ecount[ENEMY_PEACEFUL] < s->limits[ENEMY_PEACEFUL])) {
		id = ENEMY_LITTLE_GIRL;
		type = ENEMY_PEACEFUL;
	} else if (r < 30 && (gd.ecount[ENEMY_VICIOUS] < s->limits[ENEMY_VICIOUS])) {
		id = ENEMY_DRUNKEN_PUNK;
		type = ENEMY_VICIOUS;
	} else if (r < 60 && (gd.ecount[ENEMY_THIEF] < s->limits[ENEMY_THIEF])) {
		id = ENEMY_HACKER;
		type = ENEMY_THIEF;
	} else if  (gd.ecount[ENEMY_VICIOUS] < s->limits[ENEMY_VICIOUS]) {
		id = ENEMY_RAIDER;
		type = ENEMY_VICIOUS;
	} else
		return 0; // TODO this is odd
	e->id = id;
	e->type = type;
	if (type < ENEMY_BOSS)
		gd.ecount[type]++;

	return 1;
}

static void enemy_flush_states(struct enemy *e)
{
	e->pindex = 0;
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

/* TODO create unified push/pop state functions */

static void enemy_set_state(struct enemy *e, uint8_t state, uint8_t push)
{
	const uint8_t *so = enemy_sprite_offsets[e->id];

	if (push)
		enemy_push_state(e);

	e->sprite_offset = so[state];
	e->state = state;
	e->frame_reload = enemy_frame_reloads[e->id][e->state];

	e->frame = 0;
}

static void spawn_new_enemies(void)
{
	uint8_t i = 0;
	struct enemy *e;

	/* update and spawn enemies */
	do {
		e = &gd.enemies[i];
		if (e->active)
			continue;
		memset(e, 0, sizeof(*e));
		if (!enemy_generate_random(e))
			break;
		e->active = 1;
		e->width = img_width(enemy_sprites[e->id]);
		e->height = img_height(enemy_sprites[e->id]);
		e->lane = 1 + random8(2);
		if (e->type == ENEMY_VICIOUS)
			e->pee_x = WIDTH - e->width * 2 - random8(64);
		/* calculate point where to start hacking */
		if (e->type == ENEMY_THIEF)
			e->dx = random8(WIDTH - e->width);
		e->y = lane_y[e->lane] - e->height;
		e->x = WIDTH;
		e->mtime = pgm_read_byte(&enemy_mtime[e->id]);
		e->rtime = pgm_read_byte(&enemy_rtime[e->id]);
		e->atime = pgm_read_byte(&enemy_atime[e->id]);
		e->life = enemy_life[e->id];
		e->damage = enemy_damage[e->id];
		enemy_set_state(e, ENEMY_WALKING_LEFT, 0);
		enemy_set_state(e, ENEMY_WALKING_LEFT, 1);
		e->flags = __flag_white;
		break;
	} while (++i < MAX_ENEMIES);

	start_timer(TIMER_ENEMY_SPAWN, ENEMIES_SPAWN_RATE);
}

static uint8_t enemy_switch_lane(struct enemy *e, uint8_t lane, uint8_t y)
{
	if (e->y == y) {
		e->lane = lane;
		return 1;
	}

	if (e->mtime)
		return 0;

	if (e->state == ENEMY_WALKING_RIGHT)
		e->x++;
	else
		e->x--;

	if (e->y > y)
		e->y--;
	else
		e->y++;

	return 0;
}

static void enemy_prepare_direction_change(struct enemy *e) //, uint8_t width)
{
	uint8_t min;
	e->dlane = 1 + random8(2);
	min = abs(lane_y[e->lane] - lane_y[e->dlane]) + e->width + 1;
	e->dx = e->x + min + random8(WIDTH - 2 * min - e->x);
}

static uint8_t enemy_pee_pee_done(struct enemy *e)
{
	if (e->rtime == 0)
		return 1;
	e->rtime--;
	return 0;
}

static void update_enemies(void)
{
	uint8_t damage;
	uint8_t i = 0; //, width, height;
	struct enemy *e;
	struct player *p = &gd.player;
	struct door *d = &gd.door;
	struct enemy *a;
	struct stage *s = &gd.stage;
	struct rect r; // = {x, y, x + w, y + h};

	/* update and spawn enemies */
	do {
		e = &gd.enemies[i];
		if (!e->active)
			continue;

		/* check if hit by bullet */
		damage = 0;
		if (e->life > 0) {
			r.x = e->x;
			r.y = e->y;
			r.xe = e->x + e->width;
			r.ye = e->y + e->height;
			damage = get_bullet_damage(e->lane, &r);
			if (e->poisoned) {
				if (e->poison_timeout)
					e->poison_timeout--;
				else {
					damage += 2;
					e->poison_timeout = 2 * FPS;
					e->poisoned--;
				}
			}
		}
		if (damage) {
			/* check if enemy gets poisoned */
			if (p->poison)
				e->poisoned = 4;
			e->life -= damage; /* do damage */
			if (e->type != ENEMY_PEACEFUL) {
				/* blink for 1 sec */
				e->hit = FPS;
			}
			if (e->life <= 0) {
				if (d->attacker == e) {
					d->attacker = NULL;
					d->under_attack = 0;
				}
				e->atime = FPS;

				p->score += (int16_t)pgm_read_word(&enemy_score[e->id]);
				if (p->score < 0)
					p->score = 0;
				enemy_set_state(e, ENEMY_DYING, 0);
			} else {
				if (e->state != ENEMY_SPECIAL && e->state != ENEMY_RESTING_SWEARING) {
					switch (e->type) {
					case ENEMY_PEACEFUL:
						enemy_set_state(e, ENEMY_RESTING_SWEARING, 1);
						break;
					default:
						break;
					}
				}
			}
		}

		if (gd.ws.effects_active)
			get_bullet_effect(e->lane, e);

		switch (e->state) {
		case ENEMY_WALKING_LEFT:
			e->flags &= ~__flag_h_mirror;
			if (e->mtime)
				break;

			switch (e->type) {
			case ENEMY_BOSS:
			case ENEMY_VICIOUS:
				if (e->x == (6 + lane_y[e->lane] - lane_y[DOOR_LANE]))
					enemy_set_state(e, ENEMY_APPROACH_DOOR, 1);
				if (e->type == ENEMY_VICIOUS && e->pee_x == e->x) {
					e->rtime = FPS * 3;
					enemy_set_state(e, ENEMY_SPECIAL, 1);
				}
				break;
			case ENEMY_THIEF:
				if (e->x == e->dx)
					enemy_set_state(e, ENEMY_SPECIAL, 0);
				break;
			default:
				/* peaceful enemies just pass by */
				if (e->x == -e->width)
					e->active = 0;
				break;
			}
			e->x--;
			break;
		case ENEMY_APPROACH_DOOR:
			/* peaceful enemies will not reach this state */
			a = d->attacker;
			if (d->under_attack && a != e) {
				if (e->type == ENEMY_BOSS) {
					/* move away for the boss */
					enemy_flush_states(a);
					enemy_prepare_direction_change(a);
					enemy_set_state(a, ENEMY_WALKING_LEFT, 0);
					enemy_set_state(a, ENEMY_WALKING_RIGHT, 1);
				} else {
					/* door is already under attack, take a walk */
					enemy_prepare_direction_change(e);
					enemy_set_state(e, ENEMY_WALKING_RIGHT, 0);
					break;
				}
			}
			d->under_attack = 1;
			d->attacker = e;
			e->dlane = DOOR_LANE;

			e->dx = e->x - abs(lane_y[e->lane] - lane_y[e->dlane]);
			if (enemy_switch_lane(e, e->dlane, lane_y[e->dlane] - e->height))
				enemy_set_state(e, ENEMY_ATTACKING, 0);

			break;
		case ENEMY_WALKING_RIGHT:
			e->flags |= __flag_h_mirror;
			if (e->mtime)
				break;

			if (enemy_switch_lane(e, e->dlane, lane_y[e->dlane] - e->height)) {
				if (e->pee_x == e->x) {
					e->rtime = FPS * 3;
					enemy_set_state(e, ENEMY_SPECIAL, 1);
				}
				if (e->x >= e->dx)
					enemy_set_state(e, enemy_pop_state(e), 0);
				else
					e->x++;
			}

			break;
		case ENEMY_ATTACKING:
			/* do door damage */
			if (e->frame == (e->frame_reload - 1) && e->atime == 0) {
				p->life -= enemy_damage[e->id];
				enemy_set_state(e, ENEMY_RESTING_SWEARING, 1);
			}
			break;
		case ENEMY_SPECIAL:
			switch (e->type)  {
			case ENEMY_VICIOUS:
				if (enemy_pee_pee_done(e))
					enemy_set_state(e, enemy_pop_state(e), 0);
				break;
			case ENEMY_THIEF:
				if (e->rtime == 0) {
					e->rtime = pgm_read_byte(&enemy_rtime[e->id]);
					p->score -= 5;
					add_flying_number(e->x, e->y, -5);
					if (p->score < 0)
						p->score = 0;
				} else
					e->rtime--;
				break;
			}
			break;
		case ENEMY_RESTING_SWEARING:
			if (e->type == ENEMY_PEACEFUL)
				if (e->frame < e->frame_reload - 1)
					break;
			if (e->rtime == 0) {
				e->rtime = pgm_read_byte(&enemy_rtime[e->id]);
				enemy_set_state(e, enemy_pop_state(e), 0);
			} else
				e->rtime--;
			break;
		case ENEMY_DYING:
			if (e->hit == 0)
				enemy_set_state(e, ENEMY_DEAD, 0);
			break;
		case ENEMY_DEAD:
			e->active = 0;
			if (e->type != ENEMY_BOSS && !d->boss)
				s->kills--;
			if (e->type == ENEMY_BOSS)
				d->boss = 1;
			add_flying_number(e->x, e->y, (int16_t)pgm_read_word(&enemy_score[e->id]));
			break;
		}
		/* next animation */
		if (e->atime == 0) {
			e->atime = pgm_read_byte(&enemy_atime[e->id]);
			e->frame++;
			if (e->frame == e->frame_reload)
				e->frame = 0;
		} else
			e->atime--;
		/* next movement */
		if (e->mtime == 0) {
			if (e->state < ENEMY_DYING) {
				e->mtime = pgm_read_byte(&enemy_mtime[e->id]);
				if (e->slowdown) {
					e->slowdown--;
					e->mtime *= 8;
				}
			} else
				e->mtime = 0;
		} else
			e->mtime--;
		if (e->hit)
			e->hit--;
		if (e->active == 0 && e->type != ENEMY_BOSS)
			gd.ecount[e->type]--;
	} while (++i < MAX_ENEMIES);
}

static void init_enemies(void)
{
	memset(&gd.door, 0, sizeof(gd.door));
	memset(gd.enemies, 0, sizeof(gd.enemies));

	/* setup timer for enemy spawning */
	setup_timer(TIMER_ENEMY_SPAWN, spawn_new_enemies);
	start_timer(TIMER_ENEMY_SPAWN, ENEMIES_SPAWN_RATE);
}

/*---------------------------------------------------------------------------
 * powerup handling
 *---------------------------------------------------------------------------*/
#define MAX_POWERUPS                   2

enum power_up_type {
	POWER_UP_LIFE,
	POWER_UP_POISON,
	POWER_UP_SCORE,
	POWER_UP_MAX,
};

static void spawn_new_powerup(void)
{
	uint8_t i = 0, width, height;
	struct power_up *p;

	width = img_width(powerups_img);
	height = img_height(powerups_img);

	do {
		p = &gd.power_ups[i];
		if (p->active)
			continue;
		p->active = 1;
		p->atime = FPS / 4;
		p->frame = 0;
		p->timeout = (random8(4) + 4) * FPS;
		p->lane = 1 + random8(2);
		p->r.x = random8(WIDTH - width);
		p->r.y = lane_y[p->lane] - height;
		p->r.xe = p->r.x + width;
		p->r.ye = p->r.y + height;
		/* make life and poison less often */
		p->type = random8(POWER_UP_MAX);
		break;
	} while (++i < MAX_POWERUPS);
}

static void update_powerups(void)
{
	uint8_t i = 0; //, width, height;
	struct power_up *pu;
	struct player *p = &gd.player;

	do {
		pu = &gd.power_ups[i];
		if (!pu->active)
			continue;
		pu->timeout--;
		if (!pu->timeout) {
			pu->active = 0;
			start_timer(TIMER_POWERUP_SPAWN + i, (random8(8) + 4) * FPS);
			continue;
		}
		/* check if hit by player */
		if (get_bullet_damage(pu->lane, &pu->r)) {
			pu->active = 0;
			start_timer(TIMER_POWERUP_SPAWN + i, (random8(8) + 4) * FPS);
			switch (pu->type) {
			case POWER_UP_LIFE:
				p->life += 32;
				add_flying_number(pu->r.x, pu->r.x, 32);
				if (p->life > PLAYER_MAX_LIFE)
					p->life = PLAYER_MAX_LIFE;
				break;
			case POWER_UP_POISON:
				player_set_poison();
				break;
			case POWER_UP_SCORE:
				p->score += 100;
				add_flying_number(pu->r.x, pu->r.x, 100);
				break;
			}
		}

		if (pu->atime) {
			pu->atime--;
			continue;
		}

		pu->atime = FPS / 4;
		pu->frame++;
		if (pu->frame == 4)
			pu->frame = 0;
	} while (++i < MAX_POWERUPS);
}

static void init_powerups(void)
{
	memset(&gd.power_ups, 0, sizeof(gd.power_ups));
	setup_timer(TIMER_POWERUP_SPAWN, spawn_new_powerup);
	start_timer(TIMER_POWERUP_SPAWN, random8(8) * FPS + ENEMIES_SPAWN_RATE);
	setup_timer(TIMER_POWERUP_SPAWN + 1, spawn_new_powerup);
	start_timer(TIMER_POWERUP_SPAWN + 1, random8(8) * FPS + ENEMIES_SPAWN_RATE);
}

/*---------------------------------------------------------------------------
 * scene handling
 *---------------------------------------------------------------------------*/
static uint8_t lamp_frame = 0;

static void update_scene(void)
{
	/* update scene animations */

	/* show selected weapon icon */
	if (gd.ws.selected != gd.ws.previous) {
		if (gd.ws.direction) {
			if (gd.ws.icon_x == WIDTH - img_width(weapons_img))
				gd.ws.previous = gd.ws.selected;
			else {
				gd.ws.stime = FPS;
				gd.ws.icon_x-=2;
			}
		} else {
			if (gd.ws.icon_x == 0)
				gd.ws.previous = gd.ws.selected;
			else {
				gd.ws.stime = FPS;
				gd.ws.icon_x+=2;
			}
		}
	}

	/* TODO add another lamp frame above the door */

	/* update lamp animation */
	if (gp_timer_ticks & 1)
		lamp_frame = random8(2);

	if (gd.boss_time) {
		struct bumping_img *bi = (struct bumping_img *)&gd;
		uint8_t ret = 0;
		ret += img_bump(&bi[0], characters_13x16_img, CHAR_B);
		ret += img_bump(&bi[1], characters_13x16_img, CHAR_O);
		ret += img_bump(&bi[2], characters_13x16_img, CHAR_S);
		ret += img_bump(&bi[3], characters_13x16_img, CHAR_S);
		ret += img_bump(&bi[4], characters_13x16_img, CHAR_T);
		ret += img_bump(&bi[5], characters_13x16_img, CHAR_I);
		ret += img_bump(&bi[6], characters_13x16_img, CHAR_M);
		ret += img_bump(&bi[7], characters_13x16_img, CHAR_E);
		if (ret == 8)
			gd.boss_time--;
	}

	if (gd.stage_time) {
		struct bumping_img *bi = (struct bumping_img *)&gd;
		uint8_t ret = 0;
		ret += img_bump(&bi[0], characters_13x16_img, CHAR_S);
		ret += img_bump(&bi[1], characters_13x16_img, CHAR_T);
		ret += img_bump(&bi[2], characters_13x16_img, CHAR_A);
		ret += img_bump(&bi[3], characters_13x16_img, CHAR_G);
		ret += img_bump(&bi[4], characters_13x16_img, CHAR_E);
		ret += img_bump(&bi[5], characters_13x16_img, CHAR_1 + gd.stage_nr);
		if (ret == 6)
			gd.stage_time--;
	}
}

/*---------------------------------------------------------------------------
 * misc control functions
 *---------------------------------------------------------------------------*/
static int check_game_over(void)
{
	struct player *p = &gd.player;

	if (p->life <= 0)
		return 1;
	return 0;
}

static int check_win_game(void)
{
	if (gd.stage_nr == MAX_STAGES)
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
	blit_image_frame(x, y, numbers_3x5_img, NULL, number, __flag_white);
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
	struct player *p = &gd.player;

	blit_image_frame(p->x,
			 0,
			 player_all_frames_img,
			 NULL,
			 player_frame_offsets[p->state] + p->frame,
			 __flag_white);
	if (p->poison)
		blit_image(p->x + img_width(player_all_frames_img),
			   0,
			   poison_damage_img,
			   NULL,
			   __flag_white);
}

static void draw_enemies(void)
{
	uint8_t i = 0;
	uint16_t flags;
	struct enemy *e;

	do {
		e = &gd.enemies[i];
		if (!e->active)
			continue;
		if (!(e->hit & 1)) {
			flags = __flag_white;
			blit_image_frame(e->x,
					 e->y,
					 enemy_sprites[e->id],
					 enemy_masks[e->id],
					 e->frame + e->sprite_offset,
					 e->flags);
		}
	} while (++i < MAX_ENEMIES);
}

static void draw_powerups(void)
{
	uint8_t i = 0;
	struct power_up *p;

	do {
		p = &gd.power_ups[i];
		if (p->active)
			blit_image_frame(p->r.x,
					 p->r.y,
					 powerups_img,
					 powerups_mask_img,
					 p->frame + (p->type * 4),
					 __flag_white);
	} while (++i < MAX_POWERUPS);
}

static void draw_life_bar(uint8_t x, uint8_t y, int16_t current, int16_t max,
			  uint8_t blink)
{
	int16_t life_level;

	/* draw current life */
	life_level = (current * 4 + max - 1) / max;
	if (life_level > 2 || !blink) {
		draw_rect(x, y, 15, 5);
	} else {
		if (gp_timer_ticks & 1)
			draw_rect(x, y, 15, 5);
	}

	while (life_level--)
		draw_hline(x + 2 + (3 * life_level), y + 2, 2);
}

static void draw_scene(void)
{
	struct player *p = &gd.player;
	struct door *d = &gd.door;

	draw_life_bar(0, 59, p->life, PLAYER_MAX_LIFE, 1);
	if (d->under_attack) {
		struct enemy *e = d->attacker;
		draw_life_bar(20, 59, e->life, enemy_life[e->id], 0);
	}

	if (gd.ws.stime) {
		blit_image_frame(gd.ws.icon_x,
				 0,
				 weapons_img,
				 NULL,
				 gd.ws.selected,
				 __flag_white);
		gd.ws.stime--;
	}

	/* draw weather animation */
	/* TODO */

	/* draw lamp animation */
	blit_image(56,
		   HEIGHT - img_height(scene_lamp_img),
		   scene_lamp_img,
		   NULL,
		   __flag_white);
	if (lamp_frame)
		draw_hline(59, HEIGHT - img_height(scene_lamp_img) + 1, 3);
}

static const uint8_t *bullet_effect[NR_WEAPONS] = {
	bomb_splash_img,
	bomb_splash_img,
	bomb_splash_img,
	bomb_explode_img,
};

static const uint8_t *bullet_effect_mask[NR_WEAPONS] = {
	NULL,
	NULL,
	NULL,
	bomb_explode_mask_img,
};

static void draw_bullets(void)
{
	uint8_t b = 0;
	struct bullet *bs;
	uint8_t width, height;

	do {
		bs = &gd.ws.bs[b];
		switch (bs->state) {
		case BULLET_EFFECT:
			width = img_width(bullet_effect[bs->weapon]);
			height = img_height(bullet_effect[bs->weapon]);
			blit_image_frame(bs->x - width / 2,
					 lane_y[bs->lane] - height,
					 bullet_effect[bs->weapon],
					 bullet_effect_mask[bs->weapon],
					 bs->frame,
					 __flag_white | __flag_mask_single);
			break;
		case BULLET_SPLASH:
			blit_image_frame(bs->x,
					 bs->ys,
					 bomb_splash_img,
					 NULL,
					 bs->frame,
					 __flag_white);
			break;
		case BULLET_ACTIVE:
			blit_image_frame(bs->x,
					 bs->ys,
					 water_bomb_air_img,
					 water_bomb_air_mask_img,
					 bs->frame,
					 __flag_white);
			break;
		default:
			break;
		}
	} while (++b < NR_BULLETS);
}

static void draw_score(void)
{
	struct player *p = &gd.player;
#if 1
	draw_number(100, 59, p->score, 1000000, 1);
#else
	draw_number(100, 59, arduboy.cpuLoad(), 1000000, 1);
#endif
}

static void draw_screen(void)
{
	/* draw main scene */
	blit_image(0, 13, game_background_img, NULL, __flag_white);
	/* draw player */
	draw_player();
	/* draw powerups */
	draw_powerups();
	/* draw enemies */
	draw_enemies();
	/* update animations */
	draw_bullets();
	/* update flying numbers */
	draw_flying_numbers();
	/* draw new score */
	draw_score();
	/* draw scene */
	draw_scene();
}

static const char won_str[] PROGMEM = "you won";
static const char won_story_str[] PROGMEM =
"your neighborhood is safe now\n" \
"you have beaten all the scum\n" \
"granny and the little girl\n" \
"finally come to rest";

static uint8_t
run(void)
{
	uint8_t throws = 0;
	int8_t dx = 0;
	uint8_t rstate = PROGRAM_RUN_GAME;
	struct bumping_img *bi = (struct bumping_img *)&gd;

	switch (gd.game_state) {
	case GAME_STATE_INIT:
		init_flying_numbers();
		init_timers();
		init_player();
		init_weapons();
		init_enemies();
		init_powerups();
		init_stage();
		init_img_bump(25);

		/* setup general purpose timer to 500ms */
		gp_timer_ticks = 0;
		setup_timer(TIMER_GP, gp_timer_count_fn);
		start_timer(TIMER_GP, FPS / 2);

		gd.game_state = GAME_STATE_RUN_GAME;
		delay(500);
		break;
	case GAME_STATE_RUN_GAME:
		/* check for game over */
		if (check_game_over()) {
			init_timers();
			/* setup general purpose timer to 1s */
			gp_timer_ticks = 0;
			setup_timer(TIMER_GP, gp_timer_count_fn);
			start_timer(TIMER_GP, FPS);

			gd.game_state = GAME_STATE_OVER;
			init_8_char_img_bump();
			delay(500);
			break;
		}

		if (check_win_game()) {
			init_timers();
			/* setup general purpose timer to 1s */
			gp_timer_ticks = 0;
			setup_timer(TIMER_GP, gp_timer_count_fn);
			start_timer(TIMER_GP, FPS);

			gd.game_state = GAME_STATE_WON;
			delay(500);
			break;
		}

		/* back to menu */
		if (pressedUp() && pressedA()) {
			gd.game_state = GAME_STATE_CLEANUP;
			break;
		}

		/* check user inputs */
		if (pressedUp()) {
			select_weapon(-1);
		} else if (pressedDown()) {
			select_weapon(1);
		} else if (left()) {
			/* move character to the left */
			dx = -1;
		} else if (right()) {
			/* move character to the right */
			dx = 1;
		}
		if (a()) {
			/* throws bullet to the upper lane of the street */
			throws = new_bullet(UPPER_LANE, gd.ws.selected);
		}
		if (b()) {
			/* throws bullet to the lower lane of the street */
			throws = new_bullet(LOWER_LANE, gd.ws.selected);
		}

		/* update bullets */
		update_bullets();
		/* update/spawn powerups */
		update_powerups();
		/* update/spawn enemies */
		update_enemies();
		/* update scene animations */
		update_scene();
		/* update player */
		update_player(dx, throws);
		/* update stage */
		update_stage();
		/* update flying numbers */
		update_flying_numbers();

		draw_screen();

		break;
	case GAME_STATE_WON: {
		/* print score */
		struct player *p = &gd.player;
		print_text(won_str, 0, 10, __text_centered);
		draw_number(50, 20, p->score, 1000000, 1);
		print_text(won_story_str, 10, 30, __text_centered);
		if (gp_timer_ticks & 1)
			blit_image(118, 55, icon_a_img, NULL, __flag_white);
		if (pressedA())
			gd.game_state = GAME_STATE_CLEANUP;
		break;
	}
	case GAME_STATE_OVER:
	{
		uint8_t ret = 0;
		ret += img_bump(&bi[0], characters_13x16_img, CHAR_G);
		ret += img_bump(&bi[1], characters_13x16_img, CHAR_A);
		ret += img_bump(&bi[2], characters_13x16_img, CHAR_M);
		ret += img_bump(&bi[3], characters_13x16_img, CHAR_E);
		ret += img_bump(&bi[4], characters_13x16_img, CHAR_O);
		ret += img_bump(&bi[5], characters_13x16_img, CHAR_V);
		ret += img_bump(&bi[6], characters_13x16_img, CHAR_E);
		ret += img_bump(&bi[7], characters_13x16_img, CHAR_R);
		if (ret == 8) {
			if (gp_timer_ticks & 1)
				blit_image(118, 55, icon_a_img, NULL,
					   __flag_white);
			if (pressedA())
				gd.game_state = GAME_STATE_CLEANUP;
		}
		break;
	}
	case GAME_STATE_CLEANUP:
		init_timers();
		rstate = PROGRAM_MAIN_MENU;
		memset(&gd, 0, sizeof(gd));
		delay(500);
		break;
	}
	return rstate;
}

/*---------------------------------------------------------------------------
 * loop
 *---------------------------------------------------------------------------*/
typedef uint8_t (*state_fn_t)(void);
/* state of the program */
static uint8_t main_state = PROGRAM_MAIN_MENU;

static const state_fn_t main_state_fn[PROGRAM_STATE_MAX] = {
	mainscreen,
	load,
	run,
	help,
};

void
loop(void)
{
	if (!next_frame())
		return;

	main_state = main_state_fn[main_state]();

	finish_frame();
}
