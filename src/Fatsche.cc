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
#define MAX_ENEMIES                 7         /* max enemies on the scene */
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
	PROGRAM_STATE_MAX,
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
	TIMER_POWERUP_SPAWN,
	TIMER_POWERUP_SPAWN2,
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
 * data types
 *---------------------------------------------------------------------------*/
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
	uint8_t update;
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
};

struct door {
	uint8_t under_attack:1;
	uint8_t boss:1;
	struct enemy *attacker;
	uint8_t boss_countdown;
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
	uint8_t selected:3; /* selected weapon */
	uint8_t previous:3; /* previous selected weapon */
	uint8_t direction:1;
	uint8_t stime;
	int8_t icon_x;
	uint8_t ammo[NR_WEAPONS]; /* available ammo per weapon */
	struct bullet bs[NR_BULLETS];
};

#define MAX_POWERUPS                   2

struct power_up {
	uint8_t active;
	uint8_t type;
	uint8_t x;
	uint8_t y;
	uint8_t lane;
	uint8_t frame;
	uint8_t atime;
	uint16_t timeout;
};

struct game_data {
	struct menu menu;
	struct player player;
	enum game_states game_state;
	struct enemy enemies[MAX_ENEMIES];
	struct door door;
	struct weapon_states ws;
	struct power_up power_ups[MAX_POWERUPS];
};

static struct game_data gd;
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

	if (left()) {
		if (menu->state == MENU_STATE_PLAY)
			menu->state = MENU_STATE_HELP;
		else
			menu->state--;
		menu->update = 1;
	} else if (right()) {
		if (menu->state == MENU_STATE_HELP)
			menu->state = MENU_STATE_PLAY;
		else
			menu->state++;
		menu->update = 1;
	}

	if (!menu->initialized) {
		menu->update = 1;
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

	if (menu->update) {
		blit_image(0,
			   0,
			   mainscreen_img,
			   NULL,
			   __flag_white);

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
		menu->update = 0;
	}
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

		menu->update = 1;
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
	return PROGRAM_MAIN_MENU;
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

	/* XXX might introduce timeout to reduce fire rate */
	if (throws && p->state == PLAYER_RESTS)
		player_set_state(p->previous_state);

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
	BULLET_ACTIVE,
	BULLET_EFFECT,
	BULLET_SPLASH,
};

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

static uint8_t new_bullet(uint8_t lane, uint8_t weapon)
{
	uint8_t b, x = 0;
	struct bullet *bs;
	struct player *p = &gd.player;

	if (gd.ws.ammo[weapon] == 0)
		return 0;

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
		bs = &gd.ws.bs[b];
		if (bs->state == BULLET_INACTIVE)
			continue;

		height = img_height(water_bomb_air_img);
		if (bs->state >= BULLET_EFFECT) {
			if (bs->etime == 0) {
				bs->state = BULLET_INACTIVE;
				gd.ws.ammo[bs->weapon]++;
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
	} while (++b < NR_BULLETS);
}

static uint8_t get_bullet_damage(uint8_t lane, uint8_t x, uint8_t y, uint8_t w, uint8_t h)
{
	uint8_t b = 0, damage = 0, hit;
	struct bullet *bs;

	do {
		bs = &gd.ws.bs[b];
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
	} while (++b < NR_BULLETS);

	return damage;
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
 * enemy handling
 *---------------------------------------------------------------------------*/
enum enemy_types {
	ENEMY_VICIOUS,
	ENEMY_BOSS,
	ENEMY_PEACEFUL,
	ENEMY_MAX_TYPES,
};

enum enemies {
	/* vicious enemies */
	ENEMY_RAIDER,
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

static const uint8_t boss_enemy_sprite_offsets[ENEMY_MAX_STATE] = {
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

static const uint8_t vicious_enemy_sprite_offsets[ENEMY_MAX_STATE] = {
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

static const uint8_t peaceful_enemy_sprite_offsets[ENEMY_MAX_STATE] = {
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

static const uint8_t *enemy_sprite_offsets[ENEMY_MAX] = {
	vicious_enemy_sprite_offsets,
	boss_enemy_sprite_offsets,
	peaceful_enemy_sprite_offsets,
	peaceful_enemy_sprite_offsets,
};

static const uint8_t enemy_damage[ENEMY_MAX] = {
	1, 10, 0, 0,
};

static const int8_t enemy_life[ENEMY_MAX] = {
	16, 64, 32, 32,
};

static const int8_t enemy_mtime[ENEMY_MAX] = {
	FPS / 20,
	FPS / 10,
	FPS / 5,
	FPS / 10,
};

static const int8_t enemy_rtime[ENEMY_MAX] = {
	FPS,
	FPS * 2,
	6,
	0,
};

static const int8_t enemy_atime[ENEMY_MAX] = {
	FPS / 10,
	FPS / 10,
	FPS / 10,
	FPS / 15,
};

static const int16_t enemy_score[ENEMY_MAX] = {
	10,
	200,
	-100,
	-500,
};

static const uint8_t *enemy_sprites[ENEMY_MAX] = {
	enemy_raider_img,
	enemy_boss_img,
	enemy_grandma_img,
	enemy_little_girl_img,
};

static const uint8_t *enemy_masks[ENEMY_MAX] = {
	enemy_raider_mask_img,
	enemy_boss_mask_img,
	enemy_grandma_mask_img,
	enemy_little_girl_mask_img,
};

static const uint8_t enemy_default_frame_reloads[] = {
	4, 4, 4, 4,
	4, 4, 4, 4,
	4, 4, 4, 4,
};

static const uint8_t enemy_little_girl_frame_reloads[] = {
	4,  4, 4, 4,
	4,  4, 4, 4,
	4, 12, 4, 4,
};

static const uint8_t *enemy_frame_reloads[ENEMY_MAX] = {
	enemy_default_frame_reloads,
	enemy_default_frame_reloads,
	enemy_default_frame_reloads,
	enemy_little_girl_frame_reloads,
};

static void enemy_generate_random(struct enemy *e)
{
	uint8_t r = random8(100);
	uint8_t id, type;

	if (!gd.door.boss_countdown && !gd.door.boss) {
		gd.door.boss_countdown = KILLS_TILL_BOSS;
		gd.door.boss = 1;
		id = ENEMY_BOSS1;
		type = ENEMY_BOSS;
	} else if (r < 9) {
		id = ENEMY_GRANDMA;
		type = ENEMY_PEACEFUL;
	} else if (r < 18) {
		id = ENEMY_LITTLE_GIRL;
		type = ENEMY_PEACEFUL;
	} else {
		id = ENEMY_RAIDER;
		type = ENEMY_VICIOUS;
	}
	e->id = id;
	e->type = type;
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

	/* TODO do this by type so the tables can be shorter */
	/* FIXME: super strange */
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
	e->frame_reload = enemy_frame_reloads[e->id][e->state];

	if (e->state != ENEMY_EFFECT)
		e->frame = 0;
	else
		e->rtime = FPS / 2;
}

static void spawn_new_enemies(void)
{
	uint8_t i = 0, height;
	struct enemy *e;

	/* update and spawn enemies */
	do {
		e = &gd.enemies[i];
		if (e->active)
			continue;
		memset(e, 0, sizeof(*e));
		enemy_generate_random(e);
		e->active = 1;
		height = enemy_sprites[e->id][1];
		e->lane = 1 + random8(2);
		e->y = lane_y[e->lane] - height;
		e->x = 127;
		e->mtime = enemy_mtime[e->id];
		e->rtime = enemy_rtime[e->id];
		e->atime = enemy_atime[e->id];
		e->life = enemy_life[e->id];
		e->damage = enemy_damage[e->id];
		enemy_set_state(e, ENEMY_WALKING_LEFT, 0);
		enemy_set_state(e, ENEMY_WALKING_LEFT, 1);
		break;
	} while (++i < MAX_ENEMIES);

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

static uint8_t enemy_pee_pee_done(struct enemy *e)
{
	return 1;
}

static void update_enemies(void)
{
	uint8_t damage;
	uint8_t i = 0, width, height;
	struct enemy *e;
	struct player *p = &gd.player;
	struct enemy *a;

	/* update and spawn enemies */
	do {
		e = &gd.enemies[i];
		if (!e->active)
			continue;

		width = img_width(enemy_sprites[e->id]);
		height = img_height(enemy_sprites[e->id]);
		/* check if hit by bullet */
		damage = 0;
		if (e->life > 0) {
			damage = get_bullet_damage(e->lane, e->x, e->y, width, height);
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
			e->life -= damage; /* bullet damage */
			if (e->life <= 0) {
				if (gd.door.attacker == e) {
					gd.door.attacker = NULL;
					gd.door.under_attack = 0;
				}
				e->atime = FPS;
				p->score += enemy_score[e->id];

				if (p->score < 0)
					p->score = 0;
				enemy_set_state(e, ENEMY_DYING, 0);
			} else {
				if (e->state != ENEMY_EFFECT && e->state != ENEMY_SWEARING) {
					switch (e->type) {
					case ENEMY_PEACEFUL:
						enemy_set_state(e, ENEMY_SWEARING, 1);
						break;
					default:
						enemy_set_state(e, ENEMY_EFFECT, 1);
						break;
					}
				}
			}
		}
		switch (e->state) {
		case ENEMY_WALKING_LEFT:
			if (e->mtime)
				break;

			switch (e->type) {
			case ENEMY_BOSS:
			case ENEMY_VICIOUS:
				if (e->x == (6 + lane_y[e->lane] - lane_y[DOOR_LANE]))
					enemy_set_state(e, ENEMY_APPROACH_DOOR, 1);
				break;
			default:
				/* peaceful enemies just pass by */
				if (e->x == -width)
					e->active = 0;
				break;
			}
			e->x--;
			break;
		case ENEMY_APPROACH_DOOR:
			/* peaceful enemies will not reach this state */
			a = gd.door.attacker;
			if (gd.door.under_attack && a != e) {
				if (e->type == ENEMY_BOSS) {
					/* move away for the boss */
					enemy_prepare_direction_change(a, img_width(enemy_sprites[a->id]));
					enemy_set_state(a, ENEMY_WALKING_RIGHT, 1);
					enemy_set_state(a, ENEMY_CHANGE_LANE, 1);
				} else {
					/* door is already under attack, take a walk */
					enemy_prepare_direction_change(e, width);
					enemy_set_state(e, ENEMY_WALKING_RIGHT, 1);
					break;
				}
			}
			gd.door.under_attack = 1;
			gd.door.attacker = e;
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
			if (e->rtime == 0) {
				e->rtime = enemy_rtime[e->id];
				enemy_set_state(e, enemy_pop_state(e), 0);
			} else
				e->rtime--;
			break;
		case ENEMY_CHANGE_LANE:
			/* FIXME: use function here:
			 * if (enemy_change_lane())
			 *   enemy_set_state(e, enemy_pop_state(e), 0);
			 */
			if (e->lane != e->dlane) {
				if (e->lane > e->dlane)
					enemy_set_state(e, ENEMY_MOVE_TO_UPPER_LANE, 1);
				else
					enemy_set_state(e, ENEMY_MOVE_TO_LOWER_LANE, 1);
			} else
				enemy_set_state(e, enemy_pop_state(e), 0);
			break;
			/* TODO remove these */
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
			if (e->frame == (e->frame_reload - 1) && e->atime == 0) {
				p->life -= enemy_damage[e->id];
				enemy_set_state(e, ENEMY_RESTING, 1);
			}
			break;
		case ENEMY_SWEARING:
			switch (e->type)  {
			case ENEMY_VICIOUS:
				if (enemy_pee_pee_done(e))
					enemy_set_state(e, enemy_pop_state(e), 0);
				break;
			case ENEMY_PEACEFUL:
				if (e->frame == e->frame_reload - 1) {
					if (e->rtime == 0) {
						e->rtime = enemy_rtime[e->id];
						enemy_set_state(e, enemy_pop_state(e), 0);
					} else
						e->rtime--;
				}
				break;
			}
			break;
		case ENEMY_RESTING:
			/* TODO same as swearing for peaceful enemies */
			if (e->rtime == 0) {
				e->rtime = enemy_rtime[e->id];
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
				if (e->type != ENEMY_BOSS && !gd.door.boss)
					gd.door.boss_countdown--;
				if (e->type == ENEMY_BOSS)
					gd.door.boss = 0;
			} else
				e->y--;
			break;
		}
		/* next animation */
		if (e->atime == 0) {
			e->atime = enemy_atime[e->id];
			if (e->state != ENEMY_EFFECT)
				e->frame++;
			if (e->frame == e->frame_reload)
				e->frame = 0;
		} else
			e->atime--;
		/* next movement */
		if (e->mtime == 0) {
			if (e->state < ENEMY_DYING)
				e->mtime = enemy_mtime[e->id];
			else
				e->mtime = 0;
		} else
			e->mtime--;
	} while (++i < MAX_ENEMIES);
}

static void init_enemies(void)
{
	memset(&gd.door, 0, sizeof(gd.door));
	memset(gd.enemies, 0, sizeof(gd.enemies));
	gd.door.boss_countdown = KILLS_TILL_BOSS;

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
		p->atime = 2;
		p->frame = 0;
		p->timeout = (random8(4) + 4) * FPS;
		p->lane = 1 + random8(2);
		p->x = random8(WIDTH - width);
		p->y = lane_y[p->lane] - height;
		/* make life and poison less often */
		p->type = random8(POWER_UP_MAX);
		break;
	} while (++i < MAX_POWERUPS);
}

static void update_powerups(void)
{
	uint8_t i = 0, width, height;
	struct power_up *pu;
	struct player *p = &gd.player;

	width = img_width(powerups_img);
	height = img_height(powerups_img);

	do {
		pu = &gd.power_ups[i];
		switch (pu->active) {
		case 1:
			pu->timeout--;
			if (!pu->timeout) {
				pu->active = 0;
				start_timer(TIMER_POWERUP_SPAWN + i, (random8(8) + 4) * FPS);
				continue;
			}
			/* check if hit by player */
			if (get_bullet_damage(pu->lane, pu->x, pu->y, width, height)) {
				pu->active++;
				switch (pu->type) {
				case POWER_UP_LIFE:
					p->life += 32;
					if (p->life > PLAYER_MAX_LIFE)
						p->life = PLAYER_MAX_LIFE;
					break;
				case POWER_UP_POISON:
					player_set_poison();
					break;
				case POWER_UP_SCORE:
					p->score += 100;
					break;
				}
			}

			if (pu->atime) {
				pu->atime--;
				continue;
			}

			pu->atime = 2;
			pu->frame++;
			if (pu->frame == 4)
				pu->frame = 0;
			break;
		case 2:
			if (pu->y == 0) {
				start_timer(TIMER_POWERUP_SPAWN + i, (random8(8) + 4) * FPS);
				pu->active = 0;
			} else
				pu->y--;
			break;
		default:
			break;
		}
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

	/* update lamp animation */
	if (timer_500ms_ticks & 1)
		lamp_frame = random8(2);
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
	struct player *p = &gd.player;

	blit_image_frame(p->x,
			 0,
			 player_all_frames_img,
			 NULL,
			 player_frame_offsets[p->state] + p->frame,
			 __flag_none);
	if (p->poison)
		blit_image(p->x + img_width(player_all_frames_img),
			   0,
			   poison_damage_img,
			   NULL,
			   __flag_none);
}

static void draw_enemies(void)
{
	uint8_t i = 0, show;
	struct enemy *e;

	do {
		e = &gd.enemies[i];
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
			draw_number(e->x, e->y, enemy_score[e->id], 100, 2);
			break;
		default:
			show = 1;
			break;
		}
		if (show) {
			blit_image_frame(e->x,
					 e->y,
					 enemy_sprites[e->id],
					 enemy_masks[e->id],
					 e->frame + e->sprite_offset,
					 __flag_none);
		}
	} while (++i < MAX_ENEMIES);
}

static void draw_powerups(void)
{
	uint8_t i = 0, number;
	struct power_up *p;

	do {
		p = &gd.power_ups[i];
		switch (p->active) {
		case 1:
			blit_image_frame(p->x,
					 p->y,
					 powerups_img,
					 powerups_mask_img,
					 p->frame + (p->type * 4),
					 __flag_none);
			break;
		case 2:
			/* TODO remove this if numbers are fixed, then use images */
			switch (p->type) {
			case POWER_UP_LIFE:
				number = 32;
				break;
			case POWER_UP_POISON:
				number = 0;
				break;
			case POWER_UP_SCORE:
				number = 100;
				break;
			}
			if (number)
				draw_number(p->x, p->y, number, 100, 2);
			break;
		default:
			break;
		}
	} while (++i < MAX_POWERUPS);
}

static void draw_scene(void)
{
	struct player *p = &gd.player;
	int16_t life_level;

	/* draw current life */
	life_level = (p->life * 4 + PLAYER_MAX_LIFE - 1) / PLAYER_MAX_LIFE;
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

	if (gd.ws.stime) {
		blit_image_frame(gd.ws.icon_x,
				 0,
				 weapons_img,
				 NULL,
				 gd.ws.selected,
				 __flag_none);
		gd.ws.stime--;
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
	uint8_t b = 0;
	struct bullet *bs;

	/* create a new bullet, do nothing if not possible */
	do {
		bs = &gd.ws.bs[b];
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
	} while (++b < NR_BULLETS);
}

static void draw_score(void)
{
	struct player *p = &gd.player;

	draw_number(100, 59, p->score, 1000000, 1);
}

static uint8_t
run(void)
{
	uint8_t throws = 0;
	int8_t dx = 0;
	uint8_t rstate = PROGRAM_RUN_GAME;

	switch (gd.game_state) {
	case GAME_STATE_INIT:
		init_timers();
		init_player();
		init_weapons();
		init_enemies();
		init_powerups();

		/* setup general purpose 500ms counting timer */
		timer_500ms_ticks = 0;
		setup_timer(TIMER_500MS, timer_500ms_counter);
		start_timer(TIMER_500MS, FPS / 2);

		gd.game_state = GAME_STATE_RUN_GAME;
		break;
	case GAME_STATE_RUN_GAME:
		/* check for game over */
		if (check_game_over()) {
			gd.game_state = GAME_STATE_OVER;
			break;
		}

		/* back to menu */
		if (up() && a()) {
			gd.game_state = GAME_STATE_CLEANUP;
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
			throws = new_bullet(LOWER_LANE, gd.ws.selected);
		}
		if (b()) {
			/* throws bullet to the upper lane of the street */
			throws = new_bullet(UPPER_LANE, gd.ws.selected);
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

		/* draw main scene */
		blit_image(0, 0, game_background_img, NULL, __flag_none);
		/* draw player */
		draw_player();
		/* draw powerups */
		draw_powerups();
		/* draw enemies */
		draw_enemies();
		/* update animations */
		draw_bullets();
		/* draw new score */
		draw_score();
		/* draw scene */
		draw_scene();
		break;
	case GAME_STATE_OVER:
		/* TODO */
		gd.game_state = GAME_STATE_OVER;
		break;
	case GAME_STATE_CLEANUP:
		init_timers();
		rstate = PROGRAM_MAIN_MENU;
		gd.game_state = GAME_STATE_INIT;
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

#ifdef __cplusplus
extern "C"
{
#endif

void
loop(void)
{
	if (!next_frame())
		return;

	main_state = main_state_fn[main_state]();

	finish_frame();
}

#ifdef __cplusplus
}
#endif
