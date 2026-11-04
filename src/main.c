#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <pspctrl.h>
#include <pspdisplay.h>
#include <pspiofilemgr.h>
#include <pspkernel.h>
#include <pspmoduleinfo.h>

PSP_MODULE_INFO("PokemonArena2D", 0, 0, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#define SCREEN_W 480
#define SCREEN_H 272
#define BUFFER_W 512
#define TEAM_SIZE 3
#define ROSTER_SIZE 25
#define MAX_PROJECTILES 24
#define SPRITE_SIDE 48
#define SPRITE_ATLAS_SIDE 5
#define SPRITE_ATLAS_W (SPRITE_SIDE * SPRITE_ATLAS_SIDE)
#define SPRITE_ATLAS_H (SPRITE_SIDE * SPRITE_ATLAS_SIDE)
#define ARENA_LEFT 22.0f
#define ARENA_RIGHT 458.0f
#define ARENA_TOP 48.0f
#define ARENA_BOTTOM 244.0f
#define SAVE_DIR "ms0:/PSP/SAVEDATA/POKE2D"
#define SAVE_FILE SAVE_DIR "/save.dat"

typedef enum {
    TYPE_NORMAL, TYPE_FIRE, TYPE_WATER, TYPE_ELECTRIC, TYPE_GRASS, TYPE_ICE,
    TYPE_FIGHTING, TYPE_POISON, TYPE_GROUND, TYPE_FLYING, TYPE_PSYCHIC,
    TYPE_BUG, TYPE_ROCK, TYPE_GHOST, TYPE_DRAGON, TYPE_DARK, TYPE_STEEL,
    TYPE_FAIRY, TYPE_COUNT, TYPE_NONE = 255
} Type;

typedef enum {
    SCREEN_MENU, SCREEN_VS_SETUP, SCREEN_STORY_SELECT, SCREEN_TEAMS,
    SCREEN_SETTINGS, SCREEN_BATTLE, SCREEN_PAUSE, SCREEN_RESULTS,
    SCREEN_ONLINE_INFO
} Screen;

typedef enum { MODE_QUICK, MODE_VS_CPU, MODE_STORY } BattleMode;

typedef enum { SUPPLY_NONE, SUPPLY_HEAL, SUPPLY_ULTIMATE, SUPPLY_SPEED } SupplyKind;

typedef struct {
    const char *name;
    Type primary;
    Type secondary;
    uint8_t r, g, b;
    uint8_t physical;
    uint8_t skill_power;
    uint16_t skill_cooldown;
} Species;

typedef struct {
    float x, y, aim_x, aim_y, knock_x, knock_y;
    int species, hp, max_hp;
    int basic_cd, skill_cd, dash_cd, protect_cd, protect_frames;
    int invulnerable, flash, ultimate, cast_frames, cast_damage;
    int burn_frames, burn_tick, paralysis_frames, sleep_frames, speed_frames;
} Fighter;

typedef struct {
    Fighter members[TEAM_SIZE];
    int active;
    int switch_cd, switch_delay, switch_target;
    int count;
} Team;

typedef struct {
    float x, y, vx, vy;
    int damage, life, owner, active, heavy;
    Type type;
} Projectile;

typedef struct {
    float x, y;
    int active, frames;
    SupplyKind kind;
} Supply;

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint8_t teams[3][TEAM_SIZE];
    uint8_t selected_team, arena, difficulty, reduced_flash;
    uint8_t story_progress, story_species;
} SaveData;

static const char *type_names[TYPE_COUNT] = {
    "NORMAL", "FIRE", "WATER", "ELECTRIC", "GRASS", "ICE", "FIGHTING",
    "POISON", "GROUND", "FLYING", "PSYCHIC", "BUG", "ROCK", "GHOST",
    "DRAGON", "DARK", "STEEL", "FAIRY"
};

/* Compact starter roster with credited community Pokémon sprites. */
static const Species roster[ROSTER_SIZE] = {
    {"PIKACHU",    TYPE_ELECTRIC, TYPE_NONE, 250, 210,  55, 0,  9, 192},
    {"CHARIZARD",  TYPE_FIRE,     TYPE_FLYING,235, 100,  65, 0, 10, 192},
    {"SWAMPERT",   TYPE_WATER,    TYPE_GROUND, 65, 145, 225, 1, 10, 192},
    {"GROVYLE",    TYPE_GRASS,    TYPE_NONE,   80, 190, 105, 1,  9, 192},
    {"SAWK",       TYPE_FIGHTING, TYPE_NONE,   55, 125, 220, 1, 10, 192},
    {"LARVITAR",   TYPE_ROCK,     TYPE_GROUND,120, 170, 100, 1,  9, 192},
    {"GYARADOS",   TYPE_WATER,    TYPE_FLYING,55, 125, 215, 1, 11, 300},
    {"ALAKAZAM",   TYPE_PSYCHIC,  TYPE_NONE, 225, 185,  85, 0, 11, 192},
    {"BUTTERFREE", TYPE_BUG,      TYPE_FLYING,175, 205, 225, 0,  8, 108},
    {"GOLEM",      TYPE_ROCK,     TYPE_GROUND,165, 130,  95, 1, 12, 300},
    {"GENGAR",     TYPE_GHOST,    TYPE_POISON,150, 105, 195, 0, 11, 192},
    {"DRAGONITE",  TYPE_DRAGON,   TYPE_FLYING,230, 155,  95, 1, 12, 240},
    {"UMBREON",    TYPE_DARK,     TYPE_NONE,   90, 100, 145, 1,  8, 240},
    {"STEELIX",    TYPE_STEEL,    TYPE_GROUND,150, 175, 195, 1, 10, 240},
    {"GARDEVOIR",  TYPE_PSYCHIC,  TYPE_FAIRY, 215, 155, 205, 0, 11, 192},
    {"GLACEON",    TYPE_ICE,      TYPE_NONE,  140, 205, 240, 0, 10, 192},
    {"ROTOM",      TYPE_ELECTRIC, TYPE_GHOST,  245, 220,  70, 0, 10, 192},
    {"LUCARIO",    TYPE_FIGHTING, TYPE_STEEL,  80, 170, 230, 1, 11, 192},
    {"ZOROARK",    TYPE_DARK,     TYPE_NONE,  230, 105,  70, 0, 10, 192},
    {"SCEPTILE",   TYPE_GRASS,    TYPE_NONE,   85, 205,  95, 1, 10, 192},
    {"CRYOGONAL",  TYPE_ICE,      TYPE_NONE,  170, 215, 250, 0, 10, 240},
    {"MAMOSWINE",  TYPE_ICE,      TYPE_GROUND, 175, 150, 115, 1, 11, 240},
    {"TALONFLAME", TYPE_FIRE,     TYPE_FLYING, 225, 100,  55, 1, 12, 300},
    {"MEWTWO",     TYPE_PSYCHIC,  TYPE_NONE,  185, 115, 245, 0, 12, 192},
    {"VICTINI",    TYPE_FIRE,     TYPE_PSYCHIC,230, 105,  55, 0, 11, 192}
};

static const char *arena_names[6] = {
    "GRASS", "FOREST", "SEASIDE", "VOLCANIC", "ICE", "RUINS"
};

static const char *chapter_names[4] = {
    "EMBER TRAIL", "WILD CIRCUIT", "FROST ECHO", "FINAL SPARK"
};

static const char *story_names[16] = {
    "PORT CIRCUIT", "OLD LIGHTHOUSE", "TWIN RIVALS", "TIDAL TRIAL",
    "MOSS FIELD", "SPORE GROVE", "RIVER CROSSING", "GREENHEART",
    "GLACIER PASS", "ICE CAVERN", "COLD FRONT", "SNOW RIDGE",
    "ASH DUSK", "POWER STATION", "RIVAL DUEL", "SUMMIT BOSS"
};

static const uint8_t story_opponents[16] = {
    0, 18, 14, 6, 3, 5, 7, 19, 20, 21, 22, 15, 10, 24, 14, 23
};

static const uint8_t story_arenas[16] = {
    0, 3, 2, 5, 0, 1, 2, 1, 4, 4, 3, 4, 5, 3, 2, 5
};

static uint16_t framebuffers[2][BUFFER_W * SCREEN_H] __attribute__((aligned(64)));
static uint16_t sprite_atlas[SPRITE_ATLAS_W * SPRITE_ATLAS_H] __attribute__((aligned(64)));
static int sprites_loaded;
static uint16_t *draw_buffer, *display_buffer;
static Projectile projectiles[MAX_PROJECTILES];
static Supply supply;
static Team player_team, cpu_team;
static SaveData save_data;
static Screen screen = SCREEN_MENU;
static BattleMode battle_mode = MODE_QUICK;
static uint32_t previous_buttons;
static int frame_number, menu_cursor, team_cursor, team_slot;
static int story_stage, settings_cursor, pause_cursor;
static int match_frames, match_finished, player_won, ai_reaction;
static int supply_warning, next_supply_frame;
static int vs_setup_cursor, story_cursor;
static uint32_t rng_state = 0x2d5a19b3u;

static const uint8_t font3x5[36][5] = {
    {7,5,7,5,5},{6,5,6,5,6},{7,4,4,4,7},{6,5,5,5,6},{7,4,6,4,7},
    {7,4,6,4,4},{7,4,5,5,7},{5,5,7,5,5},{7,2,2,2,7},{1,1,1,5,7},
    {5,5,6,5,5},{4,4,4,4,7},{5,7,7,5,5},{5,7,7,7,5},{7,5,5,5,7},
    {7,5,7,4,4},{7,5,5,7,3},{7,5,7,6,5},{7,4,7,1,7},{7,2,2,2,2},
    {5,5,5,5,7},{5,5,5,5,2},{5,5,7,7,5},{5,5,2,5,5},{5,5,2,2,2},
    {7,1,2,4,7},{7,5,7,5,7},{2,6,2,2,7},{7,1,7,4,7},{7,1,7,1,7},
    {5,5,7,1,1},{7,4,7,1,7},{7,4,7,5,7},{7,1,2,2,2},{7,5,7,5,7},
    {7,5,7,1,7}
};

static uint16_t rgb565(unsigned int r, unsigned int g, unsigned int b) {
    return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

static uint32_t random_u32(void) {
    rng_state = rng_state * 1664525u + 1013904223u;
    return rng_state;
}

static int random_range(int max) {
    return max > 0 ? (int)(random_u32() % (uint32_t)max) : 0;
}

static int pressed(uint32_t buttons, uint32_t mask) {
    return (buttons & mask) && !(previous_buttons & mask);
}

static void fill_rect(int x, int y, int w, int h, uint16_t color) {
    if (w <= 0 || h <= 0) return;
    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = x + w > SCREEN_W ? SCREEN_W : x + w;
    int y1 = y + h > SCREEN_H ? SCREEN_H : y + h;
    for (int py = y0; py < y1; ++py) {
        uint16_t *row = draw_buffer + py * BUFFER_W;
        for (int px = x0; px < x1; ++px) row[px] = color;
    }
}

static void clear_screen(uint16_t color) {
    for (int y = 0; y < SCREEN_H; ++y) {
        uint16_t *row = draw_buffer + y * BUFFER_W;
        for (int x = 0; x < SCREEN_W; ++x) row[x] = color;
    }
}

static void fill_circle(int cx, int cy, int radius, uint16_t color) {
    for (int dy = -radius; dy <= radius; ++dy) {
        int span = (int)sqrtf((float)(radius * radius - dy * dy));
        fill_rect(cx - span, cy + dy, span * 2 + 1, 1, color);
    }
}

static void draw_char(int x, int y, char ch, int scale, uint16_t color) {
    int index = -1;
    if (ch >= 'A' && ch <= 'Z') index = ch - 'A';
    else if (ch >= '0' && ch <= '9') index = 26 + ch - '0';
    if (index < 0) return;
    for (int row = 0; row < 5; ++row)
        for (int col = 0; col < 3; ++col)
            if (font3x5[index][row] & (1 << (2 - col)))
                fill_rect(x + col * scale, y + row * scale, scale, scale, color);
}

static void draw_text(int x, int y, const char *text, int scale, uint16_t color) {
    for (const char *p = text; *p; ++p, x += 4 * scale)
        draw_char(x, y, *p, scale, color);
}

static void draw_center(int y, const char *text, int scale, uint16_t color) {
    int width = (int)strlen(text) * 4 * scale;
    draw_text((SCREEN_W - width) / 2, y, text, scale, color);
}

static float clampf(float value, float low, float high) {
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

static float length2(float x, float y) {
    return sqrtf(x * x + y * y);
}

static void normalize(float *x, float *y) {
    float n = length2(*x, *y);
    if (n > 0.001f) { *x /= n; *y /= n; }
}

static void save_game(void) {
    sceIoMkdir("ms0:/PSP", 0777);
    sceIoMkdir("ms0:/PSP/SAVEDATA", 0777);
    sceIoMkdir(SAVE_DIR, 0777);
    SceUID fd = sceIoOpen(SAVE_FILE, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
    if (fd >= 0) {
        sceIoWrite(fd, &save_data, sizeof(save_data));
        sceIoClose(fd);
    }
}

static void load_game(void) {
    memset(&save_data, 0, sizeof(save_data));
    SceUID fd = sceIoOpen(SAVE_FILE, PSP_O_RDONLY, 0777);
    if (fd >= 0) {
        int bytes = sceIoRead(fd, &save_data, sizeof(save_data));
        sceIoClose(fd);
        if (bytes == sizeof(save_data) && save_data.magic == 0x504f4b32u && save_data.version == 1) {
            if (save_data.selected_team > 2) save_data.selected_team = 0;
            if (save_data.arena > 5) save_data.arena = 0;
            if (save_data.difficulty > 2) save_data.difficulty = 1;
            if (save_data.story_progress > 16) save_data.story_progress = 16;
            if (save_data.story_species >= ROSTER_SIZE) save_data.story_species = 0;
            for (int t = 0; t < 3; ++t)
                for (int s = 0; s < TEAM_SIZE; ++s)
                    if (save_data.teams[t][s] >= ROSTER_SIZE) save_data.teams[t][s] = 0;
            return;
        }
    }
    memset(&save_data, 0, sizeof(save_data));
    save_data.magic = 0x504f4b32u;
    save_data.version = 1;
    save_data.teams[0][0] = 0; save_data.teams[0][1] = 3; save_data.teams[0][2] = 2;
    save_data.teams[1][0] = 18; save_data.teams[1][1] = 14; save_data.teams[1][2] = 6;
    save_data.teams[2][0] = 10; save_data.teams[2][1] = 15; save_data.teams[2][2] = 17;
    save_data.difficulty = 1;
    save_data.arena = 0;
    save_data.story_species = 0;
}

static void load_sprite_atlas(void) {
    const char *path = "ms0:/PSP/GAME/PokemonArena2D/assets/pokemon_atlas.rgb565";
    SceUID fd = sceIoOpen(path, PSP_O_RDONLY, 0777);
    if (fd < 0) return;
    int bytes = sceIoRead(fd, sprite_atlas, sizeof(sprite_atlas));
    sceIoClose(fd);
    sprites_loaded = bytes == (int)sizeof(sprite_atlas);
}

static const Species *get_species(int id) {
    if (id < 0 || id >= ROSTER_SIZE) id = 0;
    return &roster[id];
}

static void init_fighter(Fighter *f, int species_id) {
    memset(f, 0, sizeof(*f));
    f->species = species_id;
    f->hp = f->max_hp = 100;
    f->ultimate = 50;
    f->aim_x = 1.0f;
}

static void init_team(Team *team, const uint8_t ids[TEAM_SIZE], int count) {
    memset(team, 0, sizeof(*team));
    team->active = 0;
    team->count = count;
    team->switch_target = -1;
    for (int i = 0; i < TEAM_SIZE; ++i) {
        int id = ids[i] < ROSTER_SIZE ? ids[i] : 0;
        init_fighter(&team->members[i], id);
        if (i >= count) team->members[i].hp = 0;
    }
}

static Fighter *active_fighter(Team *team) {
    if (team->active < 0 || team->active >= TEAM_SIZE) team->active = 0;
    return &team->members[team->active];
}

static int team_alive_count(const Team *team) {
    int count = 0;
    for (int i = 0; i < team->count; ++i)
        if (team->members[i].hp > 0) ++count;
    return count;
}

static int next_alive(const Team *team, int from) {
    for (int step = 1; step <= team->count; ++step) {
        int i = (from + step) % team->count;
        if (team->members[i].hp > 0) return i;
    }
    return -1;
}

/* Multipliers are stored as halves: 0=immune, 1=half, 2=neutral, 4=double. */
static int type_factor(Type attack, Type defend) {
    switch (attack) {
    case TYPE_NORMAL:
        if (defend == TYPE_ROCK || defend == TYPE_STEEL) return 1;
        if (defend == TYPE_GHOST) return 0;
        break;
    case TYPE_FIRE:
        if (defend == TYPE_GRASS || defend == TYPE_ICE || defend == TYPE_BUG || defend == TYPE_STEEL) return 4;
        if (defend == TYPE_FIRE || defend == TYPE_WATER || defend == TYPE_ROCK || defend == TYPE_DRAGON) return 1;
        break;
    case TYPE_WATER:
        if (defend == TYPE_FIRE || defend == TYPE_GROUND || defend == TYPE_ROCK) return 4;
        if (defend == TYPE_WATER || defend == TYPE_GRASS || defend == TYPE_DRAGON) return 1;
        break;
    case TYPE_ELECTRIC:
        if (defend == TYPE_WATER || defend == TYPE_FLYING) return 4;
        if (defend == TYPE_ELECTRIC || defend == TYPE_GRASS || defend == TYPE_DRAGON) return 1;
        if (defend == TYPE_GROUND) return 0;
        break;
    case TYPE_GRASS:
        if (defend == TYPE_WATER || defend == TYPE_GROUND || defend == TYPE_ROCK) return 4;
        if (defend == TYPE_FIRE || defend == TYPE_GRASS || defend == TYPE_POISON || defend == TYPE_FLYING || defend == TYPE_BUG || defend == TYPE_DRAGON || defend == TYPE_STEEL) return 1;
        break;
    case TYPE_ICE:
        if (defend == TYPE_GRASS || defend == TYPE_GROUND || defend == TYPE_FLYING || defend == TYPE_DRAGON) return 4;
        if (defend == TYPE_FIRE || defend == TYPE_WATER || defend == TYPE_ICE || defend == TYPE_STEEL) return 1;
        break;
    case TYPE_FIGHTING:
        if (defend == TYPE_NORMAL || defend == TYPE_ICE || defend == TYPE_ROCK || defend == TYPE_DARK || defend == TYPE_STEEL) return 4;
        if (defend == TYPE_POISON || defend == TYPE_FLYING || defend == TYPE_PSYCHIC || defend == TYPE_BUG || defend == TYPE_FAIRY) return 1;
        if (defend == TYPE_GHOST) return 0;
        break;
    case TYPE_POISON:
        if (defend == TYPE_GRASS || defend == TYPE_FAIRY) return 4;
        if (defend == TYPE_POISON || defend == TYPE_GROUND || defend == TYPE_ROCK || defend == TYPE_GHOST) return 1;
        if (defend == TYPE_STEEL) return 0;
        break;
    case TYPE_GROUND:
        if (defend == TYPE_FIRE || defend == TYPE_ELECTRIC || defend == TYPE_POISON || defend == TYPE_ROCK || defend == TYPE_STEEL) return 4;
        if (defend == TYPE_GRASS || defend == TYPE_BUG) return 1;
        if (defend == TYPE_FLYING) return 0;
        break;
    case TYPE_FLYING:
        if (defend == TYPE_GRASS || defend == TYPE_FIGHTING || defend == TYPE_BUG) return 4;
        if (defend == TYPE_ELECTRIC || defend == TYPE_ROCK || defend == TYPE_STEEL) return 1;
        break;
    case TYPE_PSYCHIC:
        if (defend == TYPE_FIGHTING || defend == TYPE_POISON) return 4;
        if (defend == TYPE_PSYCHIC || defend == TYPE_STEEL) return 1;
        if (defend == TYPE_DARK) return 0;
        break;
    case TYPE_BUG:
        if (defend == TYPE_GRASS || defend == TYPE_PSYCHIC || defend == TYPE_DARK) return 4;
        if (defend == TYPE_FIRE || defend == TYPE_FIGHTING || defend == TYPE_POISON || defend == TYPE_FLYING || defend == TYPE_GHOST || defend == TYPE_STEEL || defend == TYPE_FAIRY) return 1;
        break;
    case TYPE_ROCK:
        if (defend == TYPE_FIRE || defend == TYPE_ICE || defend == TYPE_FLYING || defend == TYPE_BUG) return 4;
        if (defend == TYPE_FIGHTING || defend == TYPE_GROUND || defend == TYPE_STEEL) return 1;
        break;
    case TYPE_GHOST:
        if (defend == TYPE_PSYCHIC || defend == TYPE_GHOST) return 4;
        if (defend == TYPE_DARK) return 1;
        if (defend == TYPE_NORMAL) return 0;
        break;
    case TYPE_DRAGON:
        if (defend == TYPE_DRAGON) return 4;
        if (defend == TYPE_STEEL) return 1;
        if (defend == TYPE_FAIRY) return 0;
        break;
    case TYPE_DARK:
        if (defend == TYPE_PSYCHIC || defend == TYPE_GHOST) return 4;
        if (defend == TYPE_FIGHTING || defend == TYPE_DARK || defend == TYPE_FAIRY) return 1;
        break;
    case TYPE_STEEL:
        if (defend == TYPE_ICE || defend == TYPE_ROCK || defend == TYPE_FAIRY) return 4;
        if (defend == TYPE_FIRE || defend == TYPE_WATER || defend == TYPE_ELECTRIC || defend == TYPE_STEEL) return 1;
        if (defend == TYPE_POISON) return 0;
        break;
    case TYPE_FAIRY:
        if (defend == TYPE_FIGHTING || defend == TYPE_DRAGON || defend == TYPE_DARK) return 4;
        if (defend == TYPE_FIRE || defend == TYPE_POISON || defend == TYPE_STEEL) return 1;
        break;
    default: break;
    }
    return 2;
}

static int calculate_damage(int base, Type attack, const Fighter *attacker,
                            const Fighter *target, int basic) {
    const Species *a = get_species(attacker->species);
    const Species *d = get_species(target->species);
    int mult = 4;
    mult = mult * type_factor(attack, d->primary) / 2;
    if (d->secondary != TYPE_NONE) mult = mult * type_factor(attack, d->secondary) / 2;
    if (attack == a->primary || attack == a->secondary) mult = mult * 3 / 2;
    int damage = base * mult / 4;
    if (basic && mult == 0) damage = base / 2;
    if (damage < 1 && base > 0) damage = 1;
    return damage;
}

static void gain_ultimate(Fighter *f, int damage) {
    int add = damage * 30 / 100;
    if (add < 1 && damage > 0) add = 1;
    f->ultimate += add;
    if (f->ultimate > 100) f->ultimate = 100;
}

static int apply_damage(Fighter *target, Fighter *attacker, int base, Type type,
                        int basic, float from_x, float from_y) {
    if (target->hp <= 0 || target->invulnerable > 0 || target->protect_frames > 0)
        return 0;
    int damage = calculate_damage(base, type, attacker, target, basic);
    if (damage > target->hp) damage = target->hp;
    target->hp -= damage;
    if (!basic && damage > 0) {
        if (type == TYPE_FIRE && random_range(4) == 0) {
            target->burn_frames = 1200;
            target->burn_tick = 240;
        } else if (type == TYPE_ELECTRIC && random_range(4) == 0) {
            target->paralysis_frames = 180;
        } else if (type == TYPE_PSYCHIC && random_range(5) == 0) {
            target->sleep_frames = 96;
        }
    }
    target->flash = 7;
    float dx = target->x - from_x;
    float dy = target->y - from_y;
    normalize(&dx, &dy);
    target->knock_x += dx * 2.2f;
    target->knock_y += dy * 2.2f;
    gain_ultimate(attacker, damage);
    gain_ultimate(target, damage);
    return damage;
}

static Fighter *current_player(void) { return active_fighter(&player_team); }
static Fighter *current_cpu(void) { return active_fighter(&cpu_team); }

static void set_team_positions(void) {
    Fighter *p = current_player();
    Fighter *c = current_cpu();
    p->x = 118.0f; p->y = 146.0f; p->aim_x = 1.0f; p->aim_y = 0.0f;
    c->x = 362.0f; c->y = 146.0f; c->aim_x = -1.0f; c->aim_y = 0.0f;
}

static void random_team(uint8_t ids[TEAM_SIZE], int avoid_duplicates) {
    for (int i = 0; i < TEAM_SIZE; ++i) {
        int candidate;
        int attempts = 0;
        do {
            candidate = random_range(ROSTER_SIZE);
            ++attempts;
        } while (avoid_duplicates && attempts < 50 &&
                 ((i > 0 && ids[0] == candidate) || (i > 1 && ids[1] == candidate)));
        ids[i] = (uint8_t)candidate;
    }
}

static void start_match(BattleMode mode) {
    battle_mode = mode;
    memset(projectiles, 0, sizeof(projectiles));
    memset(&supply, 0, sizeof(supply));
    uint8_t cpu_ids[TEAM_SIZE];
    if (mode == MODE_QUICK) {
        uint8_t player_ids[TEAM_SIZE];
        random_team(player_ids, 1);
        random_team(cpu_ids, 1);
        init_team(&player_team, player_ids, TEAM_SIZE);
        init_team(&cpu_team, cpu_ids, TEAM_SIZE);
        save_data.arena = (uint8_t)random_range(6);
    } else if (mode == MODE_STORY) {
        uint8_t player_ids[TEAM_SIZE] = {save_data.story_species, 0, 0};
        uint8_t enemy_ids[TEAM_SIZE] = {story_opponents[story_stage], 0, 0};
        init_team(&player_team, player_ids, 1);
        init_team(&cpu_team, enemy_ids, 1);
        save_data.arena = story_arenas[story_stage];
    } else {
        uint8_t *player_ids = save_data.teams[save_data.selected_team];
        init_team(&player_team, player_ids, TEAM_SIZE);
        cpu_ids[0] = 18; cpu_ids[1] = 6; cpu_ids[2] = 10;
        init_team(&cpu_team, cpu_ids, TEAM_SIZE);
    }
    set_team_positions();
    match_frames = 0;
    match_finished = 0;
    player_won = 0;
    next_supply_frame = 600;
    supply_warning = 0;
    ai_reaction = 0;
    screen = SCREEN_BATTLE;
    save_game();
}

static void save_team_editor(void) {
    save_data.teams[save_data.selected_team][team_slot] = (uint8_t)team_cursor;
    save_game();
}

static void finish_match(int player_wins) {
    match_finished = 1;
    player_won = player_wins;
    screen = SCREEN_RESULTS;
    if (battle_mode == MODE_STORY && player_wins && save_data.story_progress <= story_stage) {
        if (save_data.story_progress < 16) save_data.story_progress = (uint8_t)(story_stage + 1);
        if (save_data.story_progress > 16) save_data.story_progress = 16;
        save_game();
    }
}

static void launch_projectile(int owner, Fighter *f, int damage, Type type,
                              float speed, int life, int heavy) {
    for (int i = 0; i < MAX_PROJECTILES; ++i) {
        Projectile *p = &projectiles[i];
        if (p->active) continue;
        float ax = f->aim_x, ay = f->aim_y;
        normalize(&ax, &ay);
        p->x = f->x + ax * 16.0f;
        p->y = f->y + ay * 16.0f;
        p->vx = ax * speed; p->vy = ay * speed;
        p->damage = damage; p->life = life;
        p->owner = owner; p->type = type;
        p->heavy = heavy; p->active = 1;
        return;
    }
}

static void schedule_team_switch(Team *team, int target, int manual) {
    if (target < 0 || target >= team->count || team->members[target].hp <= 0) return;
    Fighter *outgoing = active_fighter(team);
    if (outgoing->cast_frames > 0) outgoing->cast_frames = 0;
    team->switch_target = target;
    team->switch_delay = manual ? 48 : 47;
    if (manual) team->switch_cd = 210;
}

static void try_manual_switch(Team *team) {
    if (team->switch_cd > 0 || team->switch_delay > 0) return;
    Fighter *f = active_fighter(team);
    if (f->cast_frames > 0) return;
    int target = next_alive(team, team->active);
    if (target >= 0) schedule_team_switch(team, target, 1);
}

static void update_team_state(Team *team) {
    if (team->switch_cd > 0) --team->switch_cd;
    if (team->switch_delay > 0) {
        --team->switch_delay;
        if (team->switch_delay == 0 && team->switch_target >= 0) {
            team->active = team->switch_target;
            team->switch_target = -1;
            Fighter *incoming = active_fighter(team);
            incoming->invulnerable = 12;
            if (team == &player_team) incoming->x = 118.0f, incoming->y = 146.0f;
            else incoming->x = 362.0f, incoming->y = 146.0f;
        }
    }
    Fighter *f = active_fighter(team);
    if (f->hp <= 0 && team->switch_delay == 0) {
        int target = next_alive(team, team->active);
        if (target >= 0) schedule_team_switch(team, target, 0);
    }
}

static void update_fighter_timers(Fighter *f) {
    if (f->basic_cd > 0) --f->basic_cd;
    if (f->skill_cd > 0) --f->skill_cd;
    if (f->dash_cd > 0) --f->dash_cd;
    if (f->protect_cd > 0) --f->protect_cd;
    if (f->protect_frames > 0) --f->protect_frames;
    if (f->invulnerable > 0) --f->invulnerable;
    if (f->flash > 0) --f->flash;
    if (f->burn_frames > 0) {
        --f->burn_frames;
        if (f->burn_tick > 0) --f->burn_tick;
        if (f->burn_tick == 0 && f->hp > 0) {
            int burn = f->hp < 2 ? f->hp : 2;
            f->hp -= burn;
            f->burn_tick = 240;
        }
    }
    if (f->paralysis_frames > 0) --f->paralysis_frames;
    if (f->sleep_frames > 0) --f->sleep_frames;
    if (f->speed_frames > 0) --f->speed_frames;
}

static void keep_in_arena(Fighter *f) {
    f->x = clampf(f->x, ARENA_LEFT + 14.0f, ARENA_RIGHT - 14.0f);
    f->y = clampf(f->y, ARENA_TOP + 14.0f, ARENA_BOTTOM - 14.0f);
}

static void do_basic_attack(int side) {
    Team *team = side == 0 ? &player_team : &cpu_team;
    Team *other = side == 0 ? &cpu_team : &player_team;
    Fighter *f = active_fighter(team);
    Fighter *target = active_fighter(other);
    if (f->basic_cd > 0 || f->hp <= 0 || f->sleep_frames > 0 || team->switch_delay > 0) return;
    const Species *sp = get_species(f->species);
    Type type = sp->primary;
    float dx = target->x - f->x, dy = target->y - f->y;
    float dist = length2(dx, dy);
    if (dist > 0.01f) { f->aim_x = dx / dist; f->aim_y = dy / dist; }
    if (sp->physical) {
        if (dist < 42.0f) {
            f->x += f->aim_x * 10.0f;
            f->y += f->aim_y * 10.0f;
            keep_in_arena(f);
            apply_damage(target, f, 3, type, 1, f->x, f->y);
        }
    } else {
        launch_projectile(side, f, 2, type, 7.0f, 54, 0);
    }
    f->basic_cd = 30;
}

static void do_skill(int side) {
    Team *team = side == 0 ? &player_team : &cpu_team;
    Fighter *f = active_fighter(team);
    if (f->skill_cd > 0 || f->hp <= 0 || f->sleep_frames > 0 || team->switch_delay > 0) return;
    const Species *sp = get_species(f->species);
    Type type = sp->secondary != TYPE_NONE ? sp->secondary : sp->primary;
    int target_id = side == 0 ? cpu_team.active : player_team.active;
    Fighter *target = side == 0 ? current_cpu() : current_player();
    float dx = target->x - f->x, dy = target->y - f->y;
    float dist = length2(dx, dy);
    if (dist > 0.01f) { f->aim_x = dx / dist; f->aim_y = dy / dist; }
    (void)target_id;
    launch_projectile(side, f, sp->skill_power, type, 4.4f, 64, 0);
    f->skill_cd = sp->skill_cooldown;
}

static void do_utility(int side) {
    Team *team = side == 0 ? &player_team : &cpu_team;
    Fighter *f = active_fighter(team);
    if (f->protect_cd > 0 || f->hp <= 0 || f->sleep_frames > 0 || team->switch_delay > 0) return;
    f->protect_frames = 72;
    f->protect_cd = 270;
    if (f->cast_frames > 0) f->cast_frames = 0;
}

static void do_dash(Team *team, float dx, float dy) {
    Fighter *f = active_fighter(team);
    if (f->dash_cd > 0 || f->hp <= 0 || f->sleep_frames > 0 || team->switch_delay > 0) return;
    if (length2(dx, dy) < 0.1f) { dx = f->aim_x; dy = f->aim_y; }
    normalize(&dx, &dy);
    f->x += dx * 23.0f; f->y += dy * 23.0f;
    keep_in_arena(f);
    f->dash_cd = 150;
    f->invulnerable = 18;
    f->cast_frames = 0;
}

static void begin_ultimate(Team *team) {
    Fighter *f = active_fighter(team);
    if (f->ultimate < 100 || f->cast_frames > 0 || f->hp <= 0 ||
        f->sleep_frames > 0 || team->switch_delay > 0) return;
    f->cast_frames = 42;
    f->cast_damage = 18;
}

static void update_ultimates(void) {
    Team *teams[2] = {&player_team, &cpu_team};
    for (int side = 0; side < 2; ++side) {
        Team *team = teams[side];
        Fighter *f = active_fighter(team);
        if (f->cast_frames <= 0) continue;
        if (f->hp <= 0 || f->sleep_frames > 0 || team->switch_delay > 0) {
            f->cast_frames = 0;
            continue;
        }
        --f->cast_frames;
        if (f->cast_frames == 0) {
            Type type = get_species(f->species)->primary;
            launch_projectile(side, f, f->cast_damage, type, 3.5f, 90, 1);
            f->ultimate = 0;
        }
    }
}

static void update_player(const SceCtrlData *pad) {
    update_team_state(&player_team);
    Fighter *f = current_player();
    update_fighter_timers(f);
    if (player_team.switch_delay > 0 || f->hp <= 0 || f->sleep_frames > 0) return;

    float dx = 0.0f, dy = 0.0f;
    int ax = (int)pad->Lx - 128, ay = (int)pad->Ly - 128;
    int aim_mode = (pad->Buttons & PSP_CTRL_LTRIGGER) != 0;
    if (aim_mode) {
        if (pad->Buttons & PSP_CTRL_LEFT) dx -= 1.0f;
        if (pad->Buttons & PSP_CTRL_RIGHT) dx += 1.0f;
        if (pad->Buttons & PSP_CTRL_UP) dy -= 1.0f;
        if (pad->Buttons & PSP_CTRL_DOWN) dy += 1.0f;
        if (length2(dx, dy) > 0.1f) { normalize(&dx, &dy); f->aim_x = dx; f->aim_y = dy; }
    } else {
        if (ax > 22 || ax < -22 || ay > 22 || ay < -22) { dx = (float)ax / 128.0f; dy = (float)ay / 128.0f; }
        if (pad->Buttons & PSP_CTRL_LEFT) dx -= 1.0f;
        if (pad->Buttons & PSP_CTRL_RIGHT) dx += 1.0f;
        if (pad->Buttons & PSP_CTRL_UP) dy -= 1.0f;
        if (pad->Buttons & PSP_CTRL_DOWN) dy += 1.0f;
        if (length2(dx, dy) > 1.0f) normalize(&dx, &dy);
        float speed = f->speed_frames > 0 ? 2.6f : 2.0f;
        if (f->paralysis_frames > 0) speed *= 0.5f;
        f->x += dx * speed; f->y += dy * speed;
        if (length2(dx, dy) > 0.1f) { f->aim_x = dx; f->aim_y = dy; }
    }
    f->x += f->knock_x; f->y += f->knock_y;
    f->knock_x *= 0.80f; f->knock_y *= 0.80f;
    keep_in_arena(f);

    if (pressed(pad->Buttons, PSP_CTRL_RTRIGGER)) try_manual_switch(&player_team);
    if (pad->Buttons & PSP_CTRL_CROSS) do_basic_attack(0);
    if (pressed(pad->Buttons, PSP_CTRL_SQUARE)) {
        if (pad->Buttons & PSP_CTRL_LTRIGGER) do_utility(0);
        else do_skill(0);
    }
    if (pressed(pad->Buttons, PSP_CTRL_CIRCLE)) do_dash(&player_team, dx, dy);
    if (pressed(pad->Buttons, PSP_CTRL_TRIANGLE)) begin_ultimate(&player_team);
}

static int cpu_dodge_percent(void) {
    if (save_data.difficulty == 0) return 25;
    if (save_data.difficulty == 2) return 75;
    return 50;
}

static int cpu_reaction_frames(void) {
    if (save_data.difficulty == 0) return 13;
    if (save_data.difficulty == 2) return 5;
    return 8;
}

static void update_cpu(void) {
    update_team_state(&cpu_team);
    Fighter *f = current_cpu();
    update_fighter_timers(f);
    if (cpu_team.switch_delay > 0 || f->hp <= 0 || f->sleep_frames > 0) return;
    Fighter *target = current_player();
    float dx = target->x - f->x, dy = target->y - f->y;
    float distance = length2(dx, dy);
    if (distance > 0.01f) { f->aim_x = dx / distance; f->aim_y = dy / distance; }

    if (ai_reaction > 0) --ai_reaction;
    else {
        ai_reaction = cpu_reaction_frames();
        int threatened = 0;
        for (int i = 0; i < MAX_PROJECTILES; ++i) {
            Projectile *p = &projectiles[i];
            if (!p->active || p->owner != 0) continue;
            float px = p->x - f->x, py = p->y - f->y;
            if (px * px + py * py < 48.0f * 48.0f) { threatened = 1; break; }
        }
        if (threatened && f->dash_cd == 0 && random_range(100) < cpu_dodge_percent()) {
            do_dash(&cpu_team, -f->aim_x, -f->aim_y);
        }
        if (f->ultimate >= 100 && distance < 230.0f) begin_ultimate(&cpu_team);
        else if (distance > 92.0f) {
            f->x += f->aim_x * (save_data.difficulty == 2 ? 1.15f : 0.82f);
            f->y += f->aim_y * (save_data.difficulty == 2 ? 1.15f : 0.82f);
        } else if (distance < 50.0f) {
            f->x -= f->aim_x * 0.65f; f->y -= f->aim_y * 0.65f;
        }
    }
    f->x += f->knock_x; f->y += f->knock_y;
    f->knock_x *= 0.82f; f->knock_y *= 0.82f;
    keep_in_arena(f);
    if (distance < 190.0f && f->basic_cd == 0) do_basic_attack(1);
    if (distance < 220.0f && f->skill_cd == 0 && random_range(100) < 22) do_skill(1);
}

static void update_projectiles(void) {
    for (int i = 0; i < MAX_PROJECTILES; ++i) {
        Projectile *p = &projectiles[i];
        if (!p->active) continue;
        p->x += p->vx; p->y += p->vy; --p->life;
        Fighter *target = p->owner == 0 ? current_cpu() : current_player();
        Fighter *attacker = p->owner == 0 ? current_player() : current_cpu();
        float dx = target->x - p->x, dy = target->y - p->y;
        float radius = p->heavy ? 23.0f : 17.0f;
        if (dx * dx + dy * dy < radius * radius) {
            apply_damage(target, attacker, p->damage, p->type, 0, p->x, p->y);
            p->active = 0;
        } else if (p->life <= 0 || p->x < ARENA_LEFT || p->x > ARENA_RIGHT ||
                   p->y < ARENA_TOP || p->y > ARENA_BOTTOM) p->active = 0;
    }
}

static void drop_supply(void) {
    static const int points[8][2] = {
        {90,90},{240,82},{390,90},{80,150},{400,150},{90,210},{240,208},{390,210}
    };
    int point = random_range(8);
    supply.x = (float)points[point][0];
    supply.y = (float)points[point][1];
    supply.kind = (SupplyKind)(1 + random_range(3));
    supply.frames = 720;
    supply.active = 1;
}

static void collect_supply(Fighter *f) {
    if (!supply.active) return;
    float dx = f->x - supply.x, dy = f->y - supply.y;
    if (dx * dx + dy * dy > 22.0f * 22.0f) return;
    if (supply.kind == SUPPLY_HEAL) {
        f->hp += 20;
        if (f->hp > f->max_hp) f->hp = f->max_hp;
    } else if (supply.kind == SUPPLY_ULTIMATE) {
        f->ultimate += 20;
        if (f->ultimate > 100) f->ultimate = 100;
    } else if (supply.kind == SUPPLY_SPEED) f->speed_frames = 360;
    supply.active = 0;
}

static void update_supplies(void) {
    if (!supply.active && match_frames >= next_supply_frame) {
        drop_supply();
        next_supply_frame = match_frames + 1080;
        supply_warning = 0;
    } else if (!supply.active && next_supply_frame - match_frames <= 150) supply_warning = 1;
    if (supply.active) {
        --supply.frames;
        if (supply.frames <= 0) supply.active = 0;
        collect_supply(current_player());
        collect_supply(current_cpu());
    }
}

static int team_is_wiped(const Team *team) {
    return team_alive_count(team) == 0;
}

static void update_battle(const SceCtrlData *pad) {
    if (match_finished) return;
    ++match_frames;
    update_player(pad);
    update_cpu();
    update_ultimates();
    update_projectiles();
    update_supplies();
    if (team_is_wiped(&cpu_team)) finish_match(1);
    else if (team_is_wiped(&player_team)) finish_match(0);
}

static uint16_t color(unsigned int r, unsigned int g, unsigned int b) {
    return rgb565(r, g, b);
}

static void draw_outline(int x, int y, int w, int h, uint16_t c) {
    fill_rect(x, y, w, 2, c); fill_rect(x, y + h - 2, w, 2, c);
    fill_rect(x, y, 2, h, c); fill_rect(x + w - 2, y, 2, h, c);
}

static void draw_bar(int x, int y, int w, int h, int value, int maximum, uint16_t c) {
    if (value < 0) value = 0;
    if (value > maximum) value = maximum;
    fill_rect(x, y, w, h, color(27, 32, 43));
    fill_rect(x + 2, y + 2, (w - 4) * value / maximum, h - 4, c);
    draw_outline(x, y, w, h, color(215, 223, 224));
}

static void draw_sprite(int species_id, int x, int y, int size, int flip) {
    if (!sprites_loaded || size <= 0) return;
    if (species_id < 0 || species_id >= ROSTER_SIZE) species_id = 0;
    int tile_x = (species_id % SPRITE_ATLAS_SIDE) * SPRITE_SIDE;
    int tile_y = (species_id / SPRITE_ATLAS_SIDE) * SPRITE_SIDE;
    const uint16_t transparent = 0xF81F;
    for (int dy = 0; dy < size; ++dy) {
        int py = y + dy;
        if (py < 0 || py >= SCREEN_H) continue;
        int sy = dy * SPRITE_SIDE / size;
        uint16_t *dst = draw_buffer + py * BUFFER_W;
        for (int dx = 0; dx < size; ++dx) {
            int px = x + dx;
            if (px < 0 || px >= SCREEN_W) continue;
            int sx = dx * SPRITE_SIDE / size;
            if (flip) sx = SPRITE_SIDE - 1 - sx;
            uint16_t color565 = sprite_atlas[(tile_y + sy) * SPRITE_ATLAS_W + tile_x + sx];
            if (color565 != transparent) dst[px] = color565;
        }
    }
}

static uint16_t arena_color(int arena, int variant) {
    static const uint8_t palette[6][3][3] = {
        {{34,74,66},{55,105,83},{157,179,116}},
        {{30,53,49},{47,90,66},{110,139,91}},
        {{35,75,96},{53,127,151},{204,183,126}},
        {{76,43,39},{139,64,45},{220,135,66}},
        {{54,83,108},{106,153,178},{210,227,234}},
        {{61,58,82},{104,91,131},{179,157,113}}
    };
    if (arena < 0 || arena >= 6) arena = 0;
    return color(palette[arena][variant][0], palette[arena][variant][1], palette[arena][variant][2]);
}

static void draw_fighter(const Fighter *f, int side) {
    const Species *sp = get_species(f->species);
    uint16_t body = color(sp->r, sp->g, sp->b);
    int x = (int)f->x, y = (int)f->y;
    int r = f->flash > 0 && !save_data.reduced_flash && (frame_number & 1) ? 13 : 10;
    if (f->invulnerable > 0 && (frame_number & 2)) return;
    fill_circle(x, y + 18, 16, color(20, 28, 35));
    if (f->flash > 0 && !save_data.reduced_flash && (frame_number & 1)) {
        fill_circle(x, y, 33, color(255, 255, 255));
        fill_circle(x, y, 28, arena_color(save_data.arena, 1));
    }
    if (f->protect_frames > 0) {
        fill_circle(x, y, 34, color(79, 199, 236));
        fill_circle(x, y, 29, arena_color(save_data.arena, 1));
    }
    if (f->cast_frames > 0) {
        int ring = 32 + (f->cast_frames % 5);
        fill_circle(x, y, ring, color(253, 220, 96));
        fill_circle(x, y, ring - 5, arena_color(save_data.arena, 1));
    }
    if (sprites_loaded) draw_sprite(f->species, x - 28, y - 29, 56, side == 1);
    else {
        fill_circle(x, y, r, body);
        fill_circle(x - 6, y - 7, 4, body);
        fill_circle(x + 6, y - 7, 4, body);
        fill_circle(x - 3, y - 2, 1, color(20, 22, 24));
        fill_circle(x + 3, y - 2, 1, color(20, 22, 24));
        fill_rect(x - 3, y + 4, 6, 1, color(30, 24, 25));
    }
    if (side == 0) fill_rect(x - 2, y + 12, 4, 3, color(92, 204, 255));
    else fill_rect(x - 2, y + 12, 4, 3, color(255, 117, 120));
}

static void draw_arena(void) {
    int a = save_data.arena;
    clear_screen(arena_color(a, 0));
    fill_rect(0, 38, SCREEN_W, SCREEN_H - 38, arena_color(a, 1));
    fill_rect(0, 70, SCREEN_W, 2, arena_color(a, 2));
    for (int y = 84; y < SCREEN_H; y += 31)
        fill_rect(0, y, SCREEN_W, 1, arena_color(a, 2));
    draw_outline(9, 43, SCREEN_W - 18, SCREEN_H - 51, color(210, 221, 208));
    /* Simple original stage dressing, deliberately independent of web assets. */
    for (int i = 0; i < 7; ++i) {
        int x = 36 + i * 67;
        int y = 51 + (i % 3) * 6;
        fill_circle(x, y, 3 + (i % 2), arena_color(a, 2));
    }
}

static void draw_team_hud(const Team *team, int left) {
    int x = left ? 12 : 284;
    uint16_t fg = color(246, 245, 224);
    const Fighter *active = &team->members[team->active];
    draw_text(x, 8, get_species(active->species)->name, 1, fg);
    draw_text(x + 118, 8, type_names[get_species(active->species)->primary], 1, color(173, 211, 217));
    draw_bar(x, 17, 178, 12, active->hp, active->max_hp,
             active->hp > 50 ? color(93, 218, 128) : color(244, 96, 89));
    draw_bar(x, 31, 94, 5, active->ultimate, 100, color(248, 205, 76));
    for (int i = 0; i < team->count; ++i) {
        const Fighter *member = &team->members[i];
        uint16_t c = member->hp > 0 ? color(get_species(member->species)->r,
            get_species(member->species)->g, get_species(member->species)->b) : color(52, 55, 59);
        fill_circle(x + 108 + i * 14, 33, i == team->active ? 5 : 4, c);
        if (i == team->active) fill_circle(x + 108 + i * 14, 33, 2, color(255,255,255));
    }
    if (team->switch_cd > 0 && left)
        draw_text(x, 39, "SWITCH WAIT", 1, color(243, 210, 117));
}

static void draw_battle(void) {
    draw_arena();
    draw_team_hud(&player_team, 1);
    draw_team_hud(&cpu_team, 0);
    for (int i = 0; i < MAX_PROJECTILES; ++i) {
        Projectile *p = &projectiles[i];
        if (!p->active) continue;
        uint16_t c = p->heavy ? color(255,226,103) : color(255,255,241);
        if (p->owner) c = color(255,143,119);
        fill_circle((int)p->x, (int)p->y, p->heavy ? 7 : 4, c);
    }
    if (supply.active) {
        uint16_t c = supply.kind == SUPPLY_HEAL ? color(113,245,139) :
                     supply.kind == SUPPLY_ULTIMATE ? color(255,219,81) : color(107,207,255);
        fill_circle((int)supply.x, (int)supply.y, 9, color(27,35,42));
        fill_circle((int)supply.x, (int)supply.y, 6, c);
        fill_rect((int)supply.x - 1, (int)supply.y - 4, 2, 8, color(255,255,255));
        fill_rect((int)supply.x - 4, (int)supply.y - 1, 8, 2, color(255,255,255));
    } else if (supply_warning) {
        draw_center(43, "SUPPLY INCOMING", 1, color(255,235,158));
    }
    if (player_team.switch_delay > 0) draw_center(133, "SWITCHING", 2, color(250,240,190));
    draw_fighter(current_player(), 0);
    draw_fighter(current_cpu(), 1);
    draw_text(12, 257, "X ATTACK  SQ SKILL  O DASH  TRI ULT", 1, color(242,244,225));
    draw_text(367, 257, "START PAUSE", 1, color(242,244,225));
}

static void draw_panel(int x, int y, int w, int h) {
    fill_rect(x, y, w, h, color(14, 24, 38));
    draw_outline(x, y, w, h, color(99, 177, 202));
}

static void draw_title(void) {
    clear_screen(color(12, 23, 38));
    for (int i = 0; i < 9; ++i) {
        int cx = 35 + i * 52;
        fill_circle(cx, 39 + (i % 3) * 7, 13 + i % 4, color(20 + i * 2, 55 + i * 4, 71 + i * 3));
    }
    draw_center(24, "POKEMON ARENA", 3, color(245, 240, 220));
    draw_center(54, "PSP EDITION", 2, color(94, 209, 236));
    draw_panel(78, 78, 324, 174);
}

static void draw_menu_list(const char *title, const char **items, int count, int cursor, int y0) {
    draw_title();
    draw_center(88, title, 2, color(245, 240, 220));
    int first = 0;
    if (cursor > 5) first = cursor - 5;
    for (int i = first; i < count && i < first + 7; ++i) {
        int y = y0 + (i - first) * 21;
        if (i == cursor) {
            fill_rect(111, y - 3, 258, 18, color(40, 104, 130));
            draw_text(121, y, ">", 1, color(255, 226, 117));
        }
        draw_text(137, y, items[i], 1, color(245, 241, 221));
    }
    draw_text(101, 236, "UP DOWN MOVE   X SELECT   O BACK", 1, color(173, 197, 202));
}

static const char *difficulty_name(int d) {
    return d == 0 ? "EASY" : d == 2 ? "HARD" : "NORMAL";
}

static void draw_menu(void) {
    static const char *items[] = {"QUICK BATTLE", "VS CPU", "STORY", "YOUR TEAMS", "SETTINGS", "ONLINE", "EXIT"};
    draw_menu_list("MAIN MENU", items, 7, menu_cursor, 115);
    draw_text(122, 211, "3V3 CPU   16 STORY DUELS   6 ARENAS", 1, color(138, 207, 205));
}

static void draw_setup(void) {
    draw_title();
    draw_center(89, "VS CPU SETUP", 2, color(245,240,220));
    const char *labels[] = {"DIFFICULTY", "ARENA", "START MATCH"};
    for (int i = 0; i < 3; ++i) {
        int y = 122 + i * 31;
        if (i == vs_setup_cursor) fill_rect(115, y - 3, 250, 19, color(40,104,130));
        draw_text(130, y, labels[i], 1, color(245,241,221));
        if (i == 0) draw_text(270, y, difficulty_name(save_data.difficulty), 1, color(255,220,118));
        if (i == 1) draw_text(270, y, arena_names[save_data.arena], 1, color(255,220,118));
    }
    draw_text(121, 226, "LEFT RIGHT CHANGE   X SELECT   O BACK", 1, color(173,197,202));
}

static void draw_story(void) {
    draw_title();
    draw_center(88, "STORY CHALLENGES", 2, color(245,240,220));
    int first = story_cursor > 5 ? story_cursor - 5 : 0;
    for (int i = first; i < 16 && i < first + 7; ++i) {
        int y = 112 + (i - first) * 17;
        if (i == story_cursor) fill_rect(105, y - 2, 270, 15, color(40,104,130));
        draw_text(113, y, story_names[i], 1, color(245,241,221));
        draw_text(312, y, arena_names[story_arenas[i]], 1, color(166,211,212));
        if (i < save_data.story_progress) draw_text(359, y, "OK", 1, color(135,239,157));
        else if (i > save_data.story_progress) draw_text(359, y, "LOCK", 1, color(120,133,145));
        else draw_text(359, y, "NEW", 1, color(255,221,118));
    }
    draw_text(104, 236, "X PLAY UNLOCKED   LEFT RIGHT STARTER", 1, color(173,197,202));
    draw_text(106, 248, "STARTER", 1, color(177,202,207));
    draw_text(151, 248, get_species(save_data.story_species)->name, 1, color(255,221,118));
}

static void draw_teams(void) {
    draw_title();
    draw_center(87, "TEAM BUILDER", 2, color(245,240,220));
    draw_text(117, 108, "TEAM", 1, color(173,197,202));
    for (int t = 0; t < 3; ++t) {
        int x = 162 + t * 53;
        if (t == save_data.selected_team) fill_rect(x - 5, 103, 45, 15, color(40,104,130));
        draw_text(x, 108, t == 0 ? "ONE" : t == 1 ? "TWO" : "THREE", 1, color(245,241,221));
    }
    for (int s = 0; s < TEAM_SIZE; ++s) {
        int y = 137 + s * 25;
        if (s == team_slot) fill_rect(110, y - 3, 260, 19, color(40,104,130));
        draw_text(120, y, s == 0 ? "LEAD" : s == 1 ? "ALLY" : "ALLY", 1, color(180,205,209));
        draw_text(174, y, get_species(save_data.teams[save_data.selected_team][s])->name, 1, color(255,222,119));
        draw_text(315, y, "L R EDIT", 1, color(159,193,198));
        draw_sprite(save_data.teams[save_data.selected_team][s], 332, y - 8, 24, 0);
    }
    draw_text(108, 222, "UP DOWN SPECIES   LEFT RIGHT TEAM", 1, color(173,197,202));
    draw_text(121, 235, "X SET   SQ SLOT   O BACK", 1, color(173,197,202));
}

static void draw_settings(void) {
    draw_title();
    draw_center(88, "SETTINGS", 2, color(245,240,220));
    const char *labels[] = {"CPU LEVEL", "DEFAULT ARENA", "REDUCED FLASH", "STORY STARTER"};
    const char *values[] = {difficulty_name(save_data.difficulty), arena_names[save_data.arena],
        save_data.reduced_flash ? "ON" : "OFF", get_species(save_data.story_species)->name};
    for (int i = 0; i < 4; ++i) {
        int y = 119 + i * 24;
        if (i == settings_cursor) fill_rect(105, y - 3, 270, 18, color(40,104,130));
        draw_text(118, y, labels[i], 1, color(245,241,221));
        draw_text(279, y, values[i], 1, color(255,221,118));
    }
    draw_text(107, 226, "UP DOWN SELECT   LEFT RIGHT CHANGE", 1, color(173,197,202));
    draw_text(178, 239, "X SAVE   O BACK", 1, color(173,197,202));
}

static void draw_online_info(void) {
    draw_title();
    draw_center(91, "ONLINE MODE", 2, color(245,240,220));
    draw_center(124, "WEB VERSION USES ONLINE", 1, color(192,216,215));
    draw_center(139, "SERVICE AND 3 PLAYER BRAWLS", 1, color(192,216,215));
    draw_center(165, "PSP NETWORK PLAY IS NOT", 1, color(255,220,133));
    draw_center(180, "IMPLEMENTED IN THIS BUILD", 1, color(255,220,133));
    draw_center(218, "O BACK", 1, color(173,197,202));
}

static void draw_pause(void) {
    draw_battle();
    draw_panel(131, 82, 218, 116);
    draw_center(93, "PAUSED", 2, color(245,240,220));
    const char *items[] = {"RESUME", "RESTART", "MAIN MENU"};
    for (int i = 0; i < 3; ++i) {
        int y = 126 + i * 22;
        if (i == pause_cursor) fill_rect(169, y - 3, 143, 17, color(40,104,130));
        draw_center(y, items[i], 1, color(245,241,221));
    }
}

static void draw_results(void) {
    draw_battle();
    draw_panel(101, 69, 278, 140);
    draw_center(82, player_won ? "VICTORY" : "DEFEAT", 3,
                player_won ? color(140,244,163) : color(255,145,135));
    if (battle_mode == MODE_STORY) {
        char progress[32];
        int chapter = story_stage / 4;
        draw_center(123, chapter_names[chapter], 1, color(220,220,193));
        snprintf(progress, sizeof(progress), "CHALLENGE %02d OF 16", story_stage + 1);
        draw_center(139, progress, 1, color(182,209,211));
        if (player_won && story_stage < 15) draw_center(157, "X NEXT CHALLENGE", 1, color(255,224,125));
        else draw_center(157, "X STORY SELECT", 1, color(255,224,125));
    } else {
        draw_center(131, "X REMATCH", 1, color(255,224,125));
        draw_center(150, "O MAIN MENU", 1, color(182,209,211));
    }
    draw_center(185, "START MAIN MENU", 1, color(164,186,192));
}

static void change_species(int *species, int delta) {
    *species = (*species + delta + ROSTER_SIZE) % ROSTER_SIZE;
}

static void launch_story_stage(int stage) {
    if (stage < 0 || stage >= 16 || stage > save_data.story_progress) return;
    story_stage = stage;
    start_match(MODE_STORY);
}

static void handle_menu(const SceCtrlData *pad) {
    if (pressed(pad->Buttons, PSP_CTRL_UP)) menu_cursor = (menu_cursor + 6) % 7;
    if (pressed(pad->Buttons, PSP_CTRL_DOWN)) menu_cursor = (menu_cursor + 1) % 7;
    if (pressed(pad->Buttons, PSP_CTRL_CROSS)) {
        switch (menu_cursor) {
        case 0: start_match(MODE_QUICK); break;
        case 1: vs_setup_cursor = 0; screen = SCREEN_VS_SETUP; break;
        case 2: story_cursor = save_data.story_progress < 16 ? save_data.story_progress : 15; screen = SCREEN_STORY_SELECT; break;
        case 3: team_slot = 0; team_cursor = save_data.teams[save_data.selected_team][0]; screen = SCREEN_TEAMS; break;
        case 4: settings_cursor = 0; screen = SCREEN_SETTINGS; break;
        case 5: screen = SCREEN_ONLINE_INFO; break;
        case 6: sceKernelExitGame(); break;
        }
    }
}

static void handle_setup(const SceCtrlData *pad) {
    if (pressed(pad->Buttons, PSP_CTRL_UP)) vs_setup_cursor = (vs_setup_cursor + 2) % 3;
    if (pressed(pad->Buttons, PSP_CTRL_DOWN)) vs_setup_cursor = (vs_setup_cursor + 1) % 3;
    int dir = pressed(pad->Buttons, PSP_CTRL_LEFT) ? -1 : pressed(pad->Buttons, PSP_CTRL_RIGHT) ? 1 : 0;
    if (dir && vs_setup_cursor == 0) save_data.difficulty = (uint8_t)((save_data.difficulty + dir + 3) % 3);
    if (dir && vs_setup_cursor == 1) save_data.arena = (uint8_t)((save_data.arena + dir + 6) % 6);
    if (pressed(pad->Buttons, PSP_CTRL_CROSS)) {
        if (vs_setup_cursor == 2) start_match(MODE_VS_CPU);
        else vs_setup_cursor = (vs_setup_cursor + 1) % 3;
    }
    if (pressed(pad->Buttons, PSP_CTRL_CIRCLE)) screen = SCREEN_MENU;
}

static void handle_story(const SceCtrlData *pad) {
    if (pressed(pad->Buttons, PSP_CTRL_UP)) story_cursor = (story_cursor + 15) % 16;
    if (pressed(pad->Buttons, PSP_CTRL_DOWN)) story_cursor = (story_cursor + 1) % 16;
    int changed = 0;
    if (pressed(pad->Buttons, PSP_CTRL_LEFT)) {
        int species = save_data.story_species;
        change_species(&species, -1);
        save_data.story_species = (uint8_t)species;
        changed = 1;
    }
    if (pressed(pad->Buttons, PSP_CTRL_RIGHT)) {
        int species = save_data.story_species;
        change_species(&species, 1);
        save_data.story_species = (uint8_t)species;
        changed = 1;
    }
    if (pressed(pad->Buttons, PSP_CTRL_CROSS)) launch_story_stage(story_cursor);
    if (pressed(pad->Buttons, PSP_CTRL_CIRCLE)) screen = SCREEN_MENU;
    if (changed) save_game();
}

static void handle_teams(const SceCtrlData *pad) {
    if (pressed(pad->Buttons, PSP_CTRL_LEFT)) save_data.selected_team = (uint8_t)((save_data.selected_team + 2) % 3);
    if (pressed(pad->Buttons, PSP_CTRL_RIGHT)) save_data.selected_team = (uint8_t)((save_data.selected_team + 1) % 3);
    if (pressed(pad->Buttons, PSP_CTRL_UP)) change_species(&team_cursor, 1);
    if (pressed(pad->Buttons, PSP_CTRL_DOWN)) change_species(&team_cursor, -1);
    if (pressed(pad->Buttons, PSP_CTRL_SQUARE)) team_slot = (team_slot + 1) % TEAM_SIZE;
    if (pressed(pad->Buttons, PSP_CTRL_CROSS)) save_team_editor();
    if (pressed(pad->Buttons, PSP_CTRL_LTRIGGER)) team_slot = (team_slot + TEAM_SIZE - 1) % TEAM_SIZE;
    if (pressed(pad->Buttons, PSP_CTRL_RTRIGGER)) team_slot = (team_slot + 1) % TEAM_SIZE;
    if (pressed(pad->Buttons, PSP_CTRL_CIRCLE)) screen = SCREEN_MENU;
}

static void handle_settings(const SceCtrlData *pad) {
    if (pressed(pad->Buttons, PSP_CTRL_UP)) settings_cursor = (settings_cursor + 3) % 4;
    if (pressed(pad->Buttons, PSP_CTRL_DOWN)) settings_cursor = (settings_cursor + 1) % 4;
    int dir = pressed(pad->Buttons, PSP_CTRL_LEFT) ? -1 : pressed(pad->Buttons, PSP_CTRL_RIGHT) ? 1 : 0;
    if (dir) {
        if (settings_cursor == 0) save_data.difficulty = (uint8_t)((save_data.difficulty + dir + 3) % 3);
        if (settings_cursor == 1) save_data.arena = (uint8_t)((save_data.arena + dir + 6) % 6);
        if (settings_cursor == 2) save_data.reduced_flash = !save_data.reduced_flash;
        if (settings_cursor == 3) {
            int species = save_data.story_species;
            change_species(&species, dir);
            save_data.story_species = (uint8_t)species;
        }
    }
    if (pressed(pad->Buttons, PSP_CTRL_CROSS)) save_game();
    if (pressed(pad->Buttons, PSP_CTRL_CIRCLE)) { save_game(); screen = SCREEN_MENU; }
}

static void handle_pause(const SceCtrlData *pad) {
    if (pressed(pad->Buttons, PSP_CTRL_UP)) pause_cursor = (pause_cursor + 2) % 3;
    if (pressed(pad->Buttons, PSP_CTRL_DOWN)) pause_cursor = (pause_cursor + 1) % 3;
    if (pressed(pad->Buttons, PSP_CTRL_START) || pressed(pad->Buttons, PSP_CTRL_CIRCLE)) { screen = SCREEN_BATTLE; return; }
    if (pressed(pad->Buttons, PSP_CTRL_CROSS)) {
        if (pause_cursor == 0) screen = SCREEN_BATTLE;
        else if (pause_cursor == 1) start_match(battle_mode);
        else screen = SCREEN_MENU;
    }
}

static void handle_results(const SceCtrlData *pad) {
    if (pressed(pad->Buttons, PSP_CTRL_START)) { screen = SCREEN_MENU; return; }
    if (battle_mode == MODE_STORY) {
        if (pressed(pad->Buttons, PSP_CTRL_CROSS)) {
            if (player_won && story_stage < 15) launch_story_stage(story_stage + 1);
            else screen = SCREEN_STORY_SELECT;
        }
        if (pressed(pad->Buttons, PSP_CTRL_CIRCLE)) screen = SCREEN_STORY_SELECT;
    } else {
        if (pressed(pad->Buttons, PSP_CTRL_CROSS)) start_match(battle_mode);
        if (pressed(pad->Buttons, PSP_CTRL_CIRCLE)) screen = SCREEN_MENU;
    }
}

static void handle_screen(const SceCtrlData *pad) {
    if (pressed(pad->Buttons, PSP_CTRL_SELECT)) sceKernelExitGame();
    switch (screen) {
    case SCREEN_MENU: handle_menu(pad); break;
    case SCREEN_VS_SETUP: handle_setup(pad); break;
    case SCREEN_STORY_SELECT: handle_story(pad); break;
    case SCREEN_TEAMS: handle_teams(pad); break;
    case SCREEN_SETTINGS: handle_settings(pad); break;
    case SCREEN_ONLINE_INFO:
        if (pressed(pad->Buttons, PSP_CTRL_CIRCLE) || pressed(pad->Buttons, PSP_CTRL_CROSS)) screen = SCREEN_MENU;
        break;
    case SCREEN_BATTLE:
        if (pressed(pad->Buttons, PSP_CTRL_START)) { pause_cursor = 0; screen = SCREEN_PAUSE; }
        else update_battle(pad);
        break;
    case SCREEN_PAUSE: handle_pause(pad); break;
    case SCREEN_RESULTS: handle_results(pad); break;
    }
}

static void draw_screen(void) {
    switch (screen) {
    case SCREEN_MENU: draw_menu(); break;
    case SCREEN_VS_SETUP: draw_setup(); break;
    case SCREEN_STORY_SELECT: draw_story(); break;
    case SCREEN_TEAMS: draw_teams(); break;
    case SCREEN_SETTINGS: draw_settings(); break;
    case SCREEN_ONLINE_INFO: draw_online_info(); break;
    case SCREEN_BATTLE: draw_battle(); break;
    case SCREEN_PAUSE: draw_pause(); break;
    case SCREEN_RESULTS: draw_results(); break;
    }
}

int main(void) {
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
    sceDisplaySetMode(0, SCREEN_W, SCREEN_H);
    draw_buffer = framebuffers[0];
    display_buffer = framebuffers[1];
    sceDisplaySetFrameBuf(display_buffer, BUFFER_W, PSP_DISPLAY_PIXEL_FORMAT_565, PSP_DISPLAY_SETBUF_IMMEDIATE);
    load_game();
    load_sprite_atlas();
    for (;;) {
        SceCtrlData pad;
        sceCtrlPeekBufferPositive(&pad, 1);
        handle_screen(&pad);
        draw_screen();
        uint16_t *old_display = display_buffer;
        display_buffer = draw_buffer;
        draw_buffer = old_display;
        sceDisplayWaitVblankStart();
        sceDisplaySetFrameBuf(display_buffer, BUFFER_W, PSP_DISPLAY_PIXEL_FORMAT_565, PSP_DISPLAY_SETBUF_NEXTFRAME);
        previous_buttons = pad.Buttons;
        ++frame_number;
    }
    return 0;
}
