#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pspctrl.h>
#include <pspdebug.h>
#include <pspdisplay.h>
#include <pspiofilemgr.h>
#include <pspkernel.h>
#include <pspmoduleinfo.h>
#include <psputils.h>

PSP_MODULE_INFO("PokemonArena2D", 0, 0, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);
PSP_HEAP_SIZE_KB(4096);

#define SCREEN_W 480
#define SCREEN_H 272
#define BUFFER_W 512
#define TEAM_SIZE 3
#define ROSTER_SIZE 25
#define MAX_PROJECTILES 24
#define SPRITE_SIDE 64
#define SPRITE_ATLAS_SIDE 5
#define SPRITE_ANIMATION_FRAMES 12
#define SPRITE_ATLAS_W (SPRITE_SIDE * SPRITE_ATLAS_SIDE)
#define SPRITE_ATLAS_H (SPRITE_SIDE * SPRITE_ATLAS_SIDE)
#define SPRITE_ANIMATION_BYTES (ROSTER_SIZE * SPRITE_ANIMATION_FRAMES * \
                                SPRITE_SIDE * SPRITE_SIDE * sizeof(uint16_t))
#define ARENA_LEFT 22.0f
#define ARENA_RIGHT 458.0f
#define ARENA_TOP 78.0f
#define ARENA_BOTTOM 216.0f
#define SAVE_DIR "ms0:/PSP/SAVEDATA/POKE2D"
#define SAVE_FILE SAVE_DIR "/save.dat"
#define ERROR_LOG_PATH "ms0:/PSP/GAME/PokemonArena2D/error.log"

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
typedef enum {
    EFFECT_NONE, EFFECT_BURN, EFFECT_POISON, EFFECT_PARALYSIS,
    EFFECT_SLEEP, EFFECT_BOOST, EFFECT_HEAL
} MoveEffect;

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
    const char *skill_name, *utility_name, *ultimate_name;
    Type skill_type, utility_type, ultimate_type;
    uint8_t skill_power, utility_power, ultimate_power;
    uint16_t utility_cooldown;
    uint8_t utility_effect;
} MoveSet;

typedef struct {
    float x, y, aim_x, aim_y, knock_x, knock_y;
    int species, hp, max_hp;
    int basic_cd, skill_cd, utility_cd, dash_cd, protect_cd, protect_frames;
    int invulnerable, flash, ultimate, cast_frames, cast_damage;
    int burn_frames, burn_tick, poison_frames, poison_tick;
    int paralysis_frames, sleep_frames, speed_frames;
    int attack_frames, attack_kind, anim_phase, moving, boost_frames;
} Fighter;

typedef struct {
    Fighter members[TEAM_SIZE];
    int active;
    int switch_cd, switch_delay, switch_target;
    int count;
} Team;

typedef struct {
    float x, y, prev_x, prev_y, vx, vy;
    int damage, life, owner, active, heavy, age, effect;
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
    uint8_t story_progress, story_species, quick_wins;
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

/* Per-species combat loadouts: one skill, a heavy utility and an ultimate. */
static const MoveSet move_sets[ROSTER_SIZE] = {
    {"THUNDERBOLT", "THUNDER WAVE", "THUNDERSTORM", TYPE_ELECTRIC, TYPE_ELECTRIC, TYPE_ELECTRIC, 10, 12, 25, 300, EFFECT_PARALYSIS},
    {"FLAMETHROWER", "FLARE BLITZ", "BLAST BURN", TYPE_FIRE, TYPE_FIRE, TYPE_FIRE, 11, 16, 27, 330, EFFECT_BURN},
    {"MUD SHOT", "EARTHQUAKE", "HYDRO CANNON", TYPE_GROUND, TYPE_GROUND, TYPE_WATER, 10, 15, 27, 300, EFFECT_NONE},
    {"LEAF BLADE", "QUICK ATTACK", "LEAF STORM", TYPE_GRASS, TYPE_NORMAL, TYPE_GRASS, 9, 13, 24, 270, EFFECT_NONE},
    {"BRICK BREAK", "CLOSE COMBAT", "CROSS CHOP", TYPE_FIGHTING, TYPE_FIGHTING, TYPE_FIGHTING, 11, 16, 27, 330, EFFECT_NONE},
    {"ROCK SLIDE", "EARTHQUAKE", "STONE EDGE", TYPE_ROCK, TYPE_GROUND, TYPE_ROCK, 10, 15, 27, 300, EFFECT_NONE},
    {"WATERFALL", "HURRICANE", "HYDRO PUMP", TYPE_WATER, TYPE_FLYING, TYPE_WATER, 12, 16, 28, 330, EFFECT_NONE},
    {"PSYBEAM", "CALM MIND", "PSYCHIC", TYPE_PSYCHIC, TYPE_PSYCHIC, TYPE_PSYCHIC, 11, 0, 27, 300, EFFECT_BOOST},
    {"BUG BUZZ", "SLEEP POWDER", "BUG BOMB", TYPE_BUG, TYPE_GRASS, TYPE_BUG, 9, 12, 24, 270, EFFECT_SLEEP},
    {"ROCK BLAST", "EARTHQUAKE", "STONE EDGE", TYPE_ROCK, TYPE_GROUND, TYPE_ROCK, 12, 17, 29, 330, EFFECT_NONE},
    {"SHADOW BALL", "HYPNOSIS", "SHADOW BLAST", TYPE_GHOST, TYPE_PSYCHIC, TYPE_GHOST, 11, 12, 27, 270, EFFECT_SLEEP},
    {"DRAGON CLAW", "HURRICANE", "DRACO METEOR", TYPE_DRAGON, TYPE_FLYING, TYPE_DRAGON, 12, 17, 29, 330, EFFECT_NONE},
    {"DARK PULSE", "TOXIC", "FOUL PLAY", TYPE_DARK, TYPE_POISON, TYPE_DARK, 9, 12, 25, 300, EFFECT_POISON},
    {"IRON TAIL", "EARTHQUAKE", "HEAVY SLAM", TYPE_STEEL, TYPE_GROUND, TYPE_STEEL, 10, 15, 27, 300, EFFECT_NONE},
    {"PSYCHIC", "CALM MIND", "MOONBLAST", TYPE_PSYCHIC, TYPE_PSYCHIC, TYPE_FAIRY, 11, 0, 27, 300, EFFECT_BOOST},
    {"ICE BEAM", "AURORA VEIL", "BLIZZARD", TYPE_ICE, TYPE_ICE, TYPE_ICE, 10, 13, 27, 300, EFFECT_BOOST},
    {"THUNDERBOLT", "WILL O WISP", "VOLT SWITCH", TYPE_ELECTRIC, TYPE_FIRE, TYPE_ELECTRIC, 10, 12, 26, 300, EFFECT_BURN},
    {"AURA SPHERE", "SWORDS DANCE", "CLOSE COMBAT", TYPE_FIGHTING, TYPE_FIGHTING, TYPE_FIGHTING, 11, 0, 28, 300, EFFECT_BOOST},
    {"NIGHT DAZE", "NASTY PLOT", "NIGHT SLASH", TYPE_DARK, TYPE_DARK, TYPE_DARK, 10, 0, 26, 300, EFFECT_BOOST},
    {"LEAF BLADE", "SWORDS DANCE", "LEAF STORM", TYPE_GRASS, TYPE_GRASS, TYPE_GRASS, 10, 0, 27, 300, EFFECT_BOOST},
    {"FREEZE DRY", "ICE SHARD", "ICE BEAM", TYPE_ICE, TYPE_ICE, TYPE_ICE, 10, 14, 26, 300, EFFECT_NONE},
    {"ICICLE CRASH", "EARTHQUAKE", "AVALANCHE", TYPE_ICE, TYPE_GROUND, TYPE_ICE, 11, 16, 28, 330, EFFECT_NONE},
    {"BRAVE BIRD", "FLARE BLITZ", "FLAME CHARGE", TYPE_FLYING, TYPE_FIRE, TYPE_FIRE, 12, 17, 29, 330, EFFECT_BURN},
    {"PSYSTRIKE", "CALM MIND", "PSYSTRIKE", TYPE_PSYCHIC, TYPE_PSYCHIC, TYPE_PSYCHIC, 12, 0, 30, 330, EFFECT_BOOST},
    {"V CREATE", "ZEN HEADBUTT", "SEARING SHOT", TYPE_FIRE, TYPE_PSYCHIC, TYPE_FIRE, 11, 16, 29, 330, EFFECT_NONE}
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
static uint16_t *sprite_animations;
static int sprites_loaded, animations_loaded;
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
static Screen team_return_screen = SCREEN_MENU;
static uint32_t rng_state = 0x2d5a19b3u;
static char error_trace[4096];
static int error_trace_length, error_log_created;

static void log_line(const char *message) {
    int length = (int)strlen(message);
    if (!error_log_created) {
        if (error_trace_length + length + 2 <= (int)sizeof(error_trace)) {
            memcpy(error_trace + error_trace_length, message, length);
            error_trace_length += length;
            error_trace[error_trace_length++] = '\r';
            error_trace[error_trace_length++] = '\n';
        }
        return;
    }
    SceUID fd = sceIoOpen(ERROR_LOG_PATH,
                          PSP_O_WRONLY | PSP_O_CREAT | PSP_O_APPEND, 0777);
    if (fd < 0) return;
    sceIoWrite(fd, message, length);
    sceIoWrite(fd, "\r\n", 2);
    sceIoClose(fd);
}

static void create_error_log(void) {
    static const char header[] = "Pokemon Arena 2D PSP error log\r\n";
    if (error_log_created) return;
    SceUID fd = sceIoOpen(ERROR_LOG_PATH,
                          PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
    if (fd < 0) return;
    sceIoWrite(fd, header, sizeof(header) - 1);
    if (error_trace_length > 0)
        sceIoWrite(fd, error_trace, error_trace_length);
    sceIoClose(fd);
    error_log_created = 1;
}

static void record_error(const char *message) {
    log_line(message);
    create_error_log();
}

static void log_result(const char *operation, int result) {
    char line[128];
    if (result < 0)
        snprintf(line, sizeof(line), "ERROR: %s returned %d (0x%08X)",
                 operation, result, (unsigned int)result);
    else
        snprintf(line, sizeof(line), "OK: %s returned %d", operation, result);
    if (result < 0) record_error(line);
    else log_line(line);
}

static void log_hex_register(SceUID fd, const char *name, uint32_t value) {
    static const char digits[] = "0123456789ABCDEF";
    char line[64];
    int length = 0;
    while (*name && length < 48) line[length++] = *name++;
    line[length++] = ' ';
    for (int shift = 28; shift >= 0; shift -= 4)
        line[length++] = digits[(value >> shift) & 15];
    line[length++] = '\r';
    line[length++] = '\n';
    sceIoWrite(fd, line, length);
}

static void exception_log_handler(PspDebugRegBlock *regs) {
    record_error("FATAL PSP EXCEPTION");
    if (!error_log_created) return;
    SceUID fd = sceIoOpen(ERROR_LOG_PATH,
                          PSP_O_WRONLY | PSP_O_CREAT | PSP_O_APPEND, 0777);
    if (fd < 0) return;
    if (regs) {
        log_hex_register(fd, "CAUSE", regs->cause);
        log_hex_register(fd, "EPC", regs->epc);
        log_hex_register(fd, "BADVADDR", regs->badvaddr);
        log_hex_register(fd, "STATUS", regs->status);
    }
    sceIoClose(fd);
}

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
        if (bytes == sizeof(save_data) && save_data.magic == 0x504f4b32u &&
            (save_data.version == 1 || save_data.version == 2)) {
            if (save_data.version == 1) save_data.quick_wins = 0;
            save_data.version = 2;
            if (save_data.selected_team > 2) save_data.selected_team = 0;
            if (save_data.arena > 5) save_data.arena = 0;
            if (save_data.difficulty > 2) save_data.difficulty = 1;
            if (save_data.story_progress > 16) save_data.story_progress = 16;
            if (save_data.story_species >= ROSTER_SIZE) save_data.story_species = 0;
            if (save_data.quick_wins > 2) save_data.quick_wins = 0;
            for (int t = 0; t < 3; ++t)
                for (int s = 0; s < TEAM_SIZE; ++s)
                    if (save_data.teams[t][s] >= ROSTER_SIZE) save_data.teams[t][s] = 0;
            return;
        }
    }
    memset(&save_data, 0, sizeof(save_data));
    save_data.magic = 0x504f4b32u;
    save_data.version = 2;
    save_data.teams[0][0] = 0; save_data.teams[0][1] = 3; save_data.teams[0][2] = 2;
    save_data.teams[1][0] = 18; save_data.teams[1][1] = 14; save_data.teams[1][2] = 6;
    save_data.teams[2][0] = 10; save_data.teams[2][1] = 15; save_data.teams[2][2] = 17;
    save_data.difficulty = 1;
    save_data.arena = 0;
    save_data.story_species = 0;
}

static int read_sprite_asset(const char *path, uint16_t *destination, int expected_size) {
    SceUID fd = sceIoOpen(path, PSP_O_RDONLY, 0777);
    if (fd < 0) return 0;
    int total = 0;
    while (total < expected_size) {
        int bytes = sceIoRead(fd, (uint8_t *)destination + total, expected_size - total);
        if (bytes <= 0) break;
        total += bytes;
    }
    sceIoClose(fd);
    if (total != expected_size) memset(destination, 0, expected_size);
    return total == expected_size;
}

static void load_sprite_atlas(void) {
    const char *atlas_path = "ms0:/PSP/GAME/PokemonArena2D/assets/pokemon_atlas.rgb565";
    const char *animation_path = "ms0:/PSP/GAME/PokemonArena2D/assets/pokemon_animations.rgb565";
    sprites_loaded = read_sprite_asset(atlas_path, sprite_atlas, sizeof(sprite_atlas));
    if (sprites_loaded) log_line("ASSET: static atlas loaded");
    else record_error("ERROR: static atlas missing or truncated");
    sprite_animations = (uint16_t *)malloc(SPRITE_ANIMATION_BYTES);
    if (!sprite_animations) {
        record_error("ERROR: could not allocate animation atlas; using static sprites");
    } else {
        animations_loaded = read_sprite_asset(animation_path, sprite_animations,
                                              (int)SPRITE_ANIMATION_BYTES);
        if (animations_loaded) {
            log_line("ASSET: animation atlas loaded");
        } else {
            record_error("ERROR: animation atlas missing or truncated; using static sprites");
            free(sprite_animations);
            sprite_animations = NULL;
        }
    }
}

static const Species *get_species(int id) {
    if (id < 0 || id >= ROSTER_SIZE) id = 0;
    return &roster[id];
}

static const MoveSet *get_moveset(int id) {
    if (id < 0 || id >= ROSTER_SIZE) id = 0;
    return &move_sets[id];
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
    if (attacker->boost_frames > 0) base = base * 5 / 4;
    int damage = base * mult / 4;
    if (basic && mult == 0) damage = base / 2;
    if (mult == 0 && !basic) return 0;
    if (damage < 1 && base > 0 && mult > 0) damage = 1;
    return damage;
}

static void gain_ultimate(Fighter *f, int damage) {
    int add = damage * 30 / 100;
    if (add < 1 && damage > 0) add = 1;
    f->ultimate += add;
    if (f->ultimate > 100) f->ultimate = 100;
}

static int apply_damage(Fighter *target, Fighter *attacker, int base, Type type,
                        int basic, float from_x, float from_y, int effect) {
    if (target->hp <= 0 || target->invulnerable > 0 || target->protect_frames > 0)
        return 0;
    int damage = calculate_damage(base, type, attacker, target, basic);
    if (damage <= 0) return 0;
    if (damage > target->hp) damage = target->hp;
    target->hp -= damage;
    if (!basic && damage > 0) {
        if (effect == EFFECT_BURN || (effect == EFFECT_NONE && type == TYPE_FIRE && random_range(4) == 0)) {
            target->burn_frames = 1200;
            target->burn_tick = 240;
        } else if (effect == EFFECT_POISON || (effect == EFFECT_NONE && type == TYPE_POISON && random_range(4) == 0)) {
            target->poison_frames = 1200;
            target->poison_tick = 240;
        } else if (effect == EFFECT_PARALYSIS || (effect == EFFECT_NONE && type == TYPE_ELECTRIC && random_range(4) == 0)) {
            target->paralysis_frames = 180;
        } else if (effect == EFFECT_NONE && type == TYPE_ICE && random_range(4) == 0) {
            target->paralysis_frames = 180;
        } else if (effect == EFFECT_SLEEP || (effect == EFFECT_NONE && type == TYPE_PSYCHIC && random_range(5) == 0)) {
            target->sleep_frames = 168;
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
        if (save_data.quick_wins < 2) {
            uint8_t player_ids[TEAM_SIZE] = {0, 0, 0};
            uint8_t training_opponents[2] = {6, 1};
            cpu_ids[0] = training_opponents[save_data.quick_wins];
            cpu_ids[1] = cpu_ids[2] = 0;
            init_team(&player_team, player_ids, 1);
            init_team(&cpu_team, cpu_ids, 1);
            save_data.difficulty = 0;
            save_data.arena = save_data.quick_wins == 0 ? 2 : 0;
        } else {
            uint8_t player_ids[TEAM_SIZE];
            random_team(player_ids, 1);
            random_team(cpu_ids, 1);
            init_team(&player_team, player_ids, TEAM_SIZE);
            init_team(&cpu_team, cpu_ids, TEAM_SIZE);
            save_data.difficulty = 1;
            save_data.arena = (uint8_t)random_range(6);
        }
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
    if (battle_mode == MODE_QUICK && player_wins && save_data.quick_wins < 2) {
        ++save_data.quick_wins;
        save_game();
    }
}

static void launch_projectile(int owner, Fighter *f, int damage, Type type,
                              float speed, int life, int heavy, int effect) {
    for (int i = 0; i < MAX_PROJECTILES; ++i) {
        Projectile *p = &projectiles[i];
        if (p->active) continue;
        float ax = f->aim_x, ay = f->aim_y;
        normalize(&ax, &ay);
        p->x = f->x + ax * 16.0f;
        p->y = f->y + ay * 16.0f;
        p->prev_x = p->x; p->prev_y = p->y;
        p->vx = ax * speed; p->vy = ay * speed;
        p->damage = damage; p->life = life;
        p->owner = owner; p->type = type; p->age = 0; p->effect = effect;
        p->heavy = heavy; p->active = 1;
        return;
    }
}

static void schedule_team_switch(Team *team, int target, int manual) {
    if (target < 0 || target >= team->count || team->members[target].hp <= 0) return;
    Fighter *outgoing = active_fighter(team);
    if (outgoing->cast_frames > 0) outgoing->cast_frames = 0;
    outgoing->boost_frames = 0;
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

static void try_manual_switch_to(Team *team, int target) {
    if (team->switch_cd > 0 || team->switch_delay > 0 || target == team->active)
        return;
    Fighter *f = active_fighter(team);
    if (f->cast_frames > 0) return;
    schedule_team_switch(team, target, 1);
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
    if (f->utility_cd > 0) --f->utility_cd;
    if (f->dash_cd > 0) --f->dash_cd;
    if (f->protect_cd > 0) --f->protect_cd;
    if (f->protect_frames > 0) --f->protect_frames;
    if (f->invulnerable > 0) --f->invulnerable;
    if (f->flash > 0) --f->flash;
    if (f->attack_frames > 0) --f->attack_frames;
    if (f->burn_frames > 0) {
        --f->burn_frames;
        if (f->burn_tick > 0) --f->burn_tick;
        if (f->burn_tick == 0 && f->hp > 0) {
            int burn = f->hp < 2 ? f->hp : 2;
            f->hp -= burn;
            f->burn_tick = 240;
        }
    }
    if (f->poison_frames > 0) {
        --f->poison_frames;
        if (f->poison_tick > 0) --f->poison_tick;
        if (f->poison_tick == 0 && f->hp > 0) {
            int poison = f->hp < 2 ? f->hp : 2;
            f->hp -= poison;
            f->poison_tick = 240;
        }
    }
    if (f->paralysis_frames > 0) --f->paralysis_frames;
    if (f->sleep_frames > 0) --f->sleep_frames;
    if (f->speed_frames > 0) --f->speed_frames;
    if (f->boost_frames > 0) --f->boost_frames;
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
    f->attack_frames = 8;
    f->attack_kind = 1;
    if (sp->physical) {
        if (dist < 42.0f) {
            f->x += f->aim_x * 10.0f;
            f->y += f->aim_y * 10.0f;
            keep_in_arena(f);
            apply_damage(target, f, 3, type, 1, f->x, f->y, EFFECT_NONE);
        }
    } else {
        launch_projectile(side, f, 2, type, 7.0f, 54, 0, EFFECT_NONE);
    }
    f->basic_cd = 30;
}

static void do_skill(int side) {
    Team *team = side == 0 ? &player_team : &cpu_team;
    Fighter *f = active_fighter(team);
    if (f->skill_cd > 0 || f->hp <= 0 || f->sleep_frames > 0 || team->switch_delay > 0) return;
    const Species *sp = get_species(f->species);
    const MoveSet *moves = get_moveset(f->species);
    Fighter *target = side == 0 ? current_cpu() : current_player();
    float dx = target->x - f->x, dy = target->y - f->y;
    float dist = length2(dx, dy);
    if (dist > 0.01f) { f->aim_x = dx / dist; f->aim_y = dy / dist; }
    launch_projectile(side, f, moves->skill_power ? moves->skill_power : sp->skill_power,
                      moves->skill_type, 5.0f, 72, 0, EFFECT_NONE);
    f->skill_cd = sp->skill_cooldown;
    f->attack_frames = 14;
    f->attack_kind = 2;
}

static void do_utility(int side) {
    Team *team = side == 0 ? &player_team : &cpu_team;
    Fighter *f = active_fighter(team);
    if (f->utility_cd > 0 || f->hp <= 0 || f->sleep_frames > 0 || team->switch_delay > 0) return;
    const MoveSet *moves = get_moveset(f->species);
    if (moves->utility_effect == EFFECT_BOOST) {
        f->boost_frames = 480;
    } else if (moves->utility_effect == EFFECT_HEAL) {
        f->hp += 20;
        if (f->hp > f->max_hp) f->hp = f->max_hp;
    } else {
        int damage = moves->utility_power ? moves->utility_power : 2;
        launch_projectile(side, f, damage, moves->utility_type, 3.6f, 82, 1, moves->utility_effect);
    }
    f->utility_cd = moves->utility_cooldown;
    f->attack_frames = 18;
    f->attack_kind = 3;
}

static void do_protect(Team *team) {
    Fighter *f = active_fighter(team);
    if (f->protect_cd > 0 || f->hp <= 0 || f->sleep_frames > 0 || team->switch_delay > 0) return;
    f->protect_frames = 72;
    f->protect_cd = 270;
    if (f->cast_frames > 0) f->cast_frames = 0;
    f->attack_frames = 10;
    f->attack_kind = 4;
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
    f->attack_frames = 8;
    f->attack_kind = 5;
}

static void begin_ultimate(Team *team) {
    Fighter *f = active_fighter(team);
    if (f->ultimate < 100 || f->cast_frames > 0 || f->hp <= 0 ||
        f->sleep_frames > 0 || team->switch_delay > 0) return;
    f->cast_frames = 42;
    f->cast_damage = get_moveset(f->species)->ultimate_power;
    f->attack_frames = 42;
    f->attack_kind = 6;
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
            Type type = get_moveset(f->species)->ultimate_type;
            launch_projectile(side, f, f->cast_damage, type, 3.5f, 100, 1, EFFECT_NONE);
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
    f->moving = 0;
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
        f->moving = length2(dx, dy) > 0.1f;
        if (f->moving) ++f->anim_phase;
        float speed = f->speed_frames > 0 ? 2.6f : 2.0f;
        if (f->paralysis_frames > 0) speed *= 0.5f;
        f->x += dx * speed; f->y += dy * speed;
        if (length2(dx, dy) > 0.1f) { f->aim_x = dx; f->aim_y = dy; }
    }
    f->x += f->knock_x; f->y += f->knock_y;
    f->knock_x *= 0.80f; f->knock_y *= 0.80f;
    keep_in_arena(f);

    int switch_chord = (pad->Buttons & PSP_CTRL_RTRIGGER) != 0;
    if (switch_chord && (pad->Buttons & PSP_CTRL_CROSS))
        try_manual_switch_to(&player_team, 0);
    else if (switch_chord && (pad->Buttons & PSP_CTRL_SQUARE))
        try_manual_switch_to(&player_team, 1);
    else if (switch_chord && (pad->Buttons & PSP_CTRL_CIRCLE))
        try_manual_switch_to(&player_team, 2);
    else if (pressed(pad->Buttons, PSP_CTRL_RTRIGGER))
        try_manual_switch(&player_team);
    if (!switch_chord && (pad->Buttons & PSP_CTRL_CROSS)) do_basic_attack(0);
    if (!switch_chord && pressed(pad->Buttons, PSP_CTRL_SQUARE)) {
        if (pad->Buttons & PSP_CTRL_LTRIGGER) do_protect(&player_team);
        else do_skill(0);
    }
    if (!switch_chord && pressed(pad->Buttons, PSP_CTRL_CIRCLE)) {
        if (pad->Buttons & PSP_CTRL_LTRIGGER) do_utility(0);
        else do_dash(&player_team, dx, dy);
    }
    if (!switch_chord && pressed(pad->Buttons, PSP_CTRL_TRIANGLE))
        begin_ultimate(&player_team);
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
    f->moving = 0;
    Fighter *target = current_player();
    float dx = target->x - f->x, dy = target->y - f->y;
    float distance = length2(dx, dy);
    if (distance > 0.01f) { f->aim_x = dx / distance; f->aim_y = dy / distance; }

    if (ai_reaction > 0) --ai_reaction;
    else {
        ai_reaction = cpu_reaction_frames();
        float preferred_distance = get_species(f->species)->physical ? 34.0f : 100.0f;
        int threatened = 0;
        for (int i = 0; i < MAX_PROJECTILES; ++i) {
            Projectile *p = &projectiles[i];
            if (!p->active || p->owner != 0) continue;
            float px = p->x - f->x, py = p->y - f->y;
            if (px * px + py * py < 48.0f * 48.0f) { threatened = 1; break; }
        }
        if (threatened && f->dash_cd == 0 && random_range(100) < cpu_dodge_percent()) {
            do_dash(&cpu_team, -f->aim_x, -f->aim_y);
        } else if (threatened && f->protect_cd == 0 && random_range(100) < cpu_dodge_percent()) {
            do_protect(&cpu_team);
        }
        if (f->ultimate >= 100 && distance < 230.0f) begin_ultimate(&cpu_team);
        else if (distance > preferred_distance + 12.0f) {
            float step = save_data.difficulty == 0 ? 0.72f :
                         save_data.difficulty == 2 ? 1.15f : 0.92f;
            f->x += f->aim_x * step;
            f->y += f->aim_y * step;
            f->moving = 1;
        } else if (distance < preferred_distance - 16.0f) {
            f->x -= f->aim_x * 0.65f; f->y -= f->aim_y * 0.65f;
            f->moving = 1;
        }
    }
    if (f->moving) ++f->anim_phase;
    f->x += f->knock_x; f->y += f->knock_y;
    f->knock_x *= 0.82f; f->knock_y *= 0.82f;
    keep_in_arena(f);
    if (distance < 190.0f && f->basic_cd == 0) do_basic_attack(1);
    if (distance < 220.0f && f->skill_cd == 0 && random_range(100) < 22) do_skill(1);
    if (distance < 205.0f && f->utility_cd == 0 && random_range(100) < 8) do_utility(1);
}

static void update_projectiles(void) {
    for (int i = 0; i < MAX_PROJECTILES; ++i) {
        Projectile *p = &projectiles[i];
        if (!p->active) continue;
        p->prev_x = p->x; p->prev_y = p->y;
        p->x += p->vx; p->y += p->vy; --p->life;
        ++p->age;
        Fighter *target = p->owner == 0 ? current_cpu() : current_player();
        Fighter *attacker = p->owner == 0 ? current_player() : current_cpu();
        float dx = target->x - p->x, dy = target->y - p->y;
        float radius = p->heavy ? 23.0f : 17.0f;
        if (dx * dx + dy * dy < radius * radius) {
            apply_damage(target, attacker, p->damage, p->type, 0, p->x, p->y, p->effect);
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

static uint16_t type_color(Type type) {
    static const uint8_t rgb[TYPE_COUNT][3] = {
        {200,200,188},{248,128,72},{84,176,248},{250,218,70},{112,204,93},{132,218,240},
        {214,106,93},{172,90,177},{194,156,91},{151,203,236},{210,120,219},{174,194,94},
        {184,164,122},{127,108,166},{135,116,224},{126,106,103},{170,185,191},{231,151,193}
    };
    if (type < 0 || type >= TYPE_COUNT) return color(245,245,245);
    return color(rgb[type][0], rgb[type][1], rgb[type][2]);
}

static void fill_ellipse(int cx, int cy, int rx, int ry, uint16_t c) {
    if (rx <= 0 || ry <= 0) return;
    for (int dy = -ry; dy <= ry; ++dy) {
        float t = 1.0f - (float)(dy * dy) / (float)(ry * ry);
        if (t < 0.0f) t = 0.0f;
        int span = (int)((float)rx * sqrtf(t));
        fill_rect(cx - span, cy + dy, span * 2 + 1, 1, c);
    }
}

static void draw_line(int x0, int y0, int x1, int y1, uint16_t c) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        fill_rect(x0, y0, 2, 2, c);
        if (x0 == x1 && y0 == y1) break;
        int e2 = err * 2;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
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

static void draw_sprite_tile(const uint16_t *tile, int x, int y, int size, int flip) {
    if (!tile || size <= 0) return;
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
            uint16_t color565 = tile[sy * SPRITE_SIDE + sx];
            if (color565 != transparent) dst[px] = color565;
        }
    }
}

static void draw_sprite(int species_id, int x, int y, int size, int flip) {
    if (!sprites_loaded || size <= 0) return;
    if (species_id < 0 || species_id >= ROSTER_SIZE) species_id = 0;
    const uint16_t *tile = sprite_atlas + species_id * SPRITE_SIDE * SPRITE_SIDE;
    draw_sprite_tile(tile, x, y, size, flip);
}

static void draw_animated_sprite(int species_id, int animation_frame,
                                 int x, int y, int size, int flip) {
    if (!animations_loaded || size <= 0) {
        draw_sprite(species_id, x, y, size, flip);
        return;
    }
    if (species_id < 0 || species_id >= ROSTER_SIZE) species_id = 0;
    if (animation_frame < 0 || animation_frame >= SPRITE_ANIMATION_FRAMES)
        animation_frame = 0;
    int tile_index = species_id * SPRITE_ANIMATION_FRAMES + animation_frame;
    const uint16_t *tile = sprite_animations + tile_index * SPRITE_SIDE * SPRITE_SIDE;
    draw_sprite_tile(tile, x, y, size, flip);
}

static int fighter_animation_frame(const Fighter *f) {
    int movement_frame = (f->anim_phase / 4) & 3;
    if (f->attack_frames > 0 &&
        (f->attack_kind == 1 || f->attack_kind == 2 ||
         f->attack_kind == 3 || f->attack_kind == 6)) {
        int duration = f->attack_kind == 1 ? 8 :
                       f->attack_kind == 2 ? 14 :
                       f->attack_kind == 3 ? 18 : 42;
        int elapsed = duration - f->attack_frames;
        int action_frame = elapsed * 4 / duration;
        if (action_frame < 0) action_frame = 0;
        if (action_frame > 3) action_frame = 3;
        return 8 + action_frame;
    }
    if (f->moving || (f->attack_frames > 0 && f->attack_kind == 5))
        return 4 + movement_frame;
    return (frame_number / 16) % 4;
}

static void draw_projectile(const Projectile *p) {
    uint16_t main = type_color(p->type);
    uint16_t light = color(255,250,229);
    if (p->owner) main = color(255,126,130);
    draw_line((int)p->prev_x, (int)p->prev_y, (int)p->x, (int)p->y,
              p->heavy ? main : color(114,128,157));
    if (p->type == TYPE_ELECTRIC) {
        int mx = ((int)p->prev_x + (int)p->x) / 2;
        int my = ((int)p->prev_y + (int)p->y) / 2;
        draw_line((int)p->prev_x, (int)p->prev_y, mx + 4, my - 5, main);
        draw_line(mx + 4, my - 5, (int)p->x, (int)p->y, light);
    }
    int pulse = (p->age & 1);
    fill_circle((int)p->x, (int)p->y, p->heavy ? 9 + pulse : 6, main);
    fill_circle((int)p->x, (int)p->y, p->heavy ? 4 : 2, light);
    if (p->heavy && (p->age & 3) < 2) {
        fill_circle((int)p->x, (int)p->y, 12, color(255,255,255));
        fill_circle((int)p->x, (int)p->y, 9, main);
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
    int x = (int)f->x, y = (int)f->y;
    if (f->invulnerable > 0 && (frame_number & 2)) return;
    int bob = f->moving ? ((f->anim_phase / 5) & 1 ? -2 : 1) : ((frame_number / 28) & 1);
    int push = f->attack_frames > 0 && f->attack_kind <= 3 ? 3 : 0;
    int draw_x = x + (int)(f->aim_x * push);
    int draw_y = y + bob;
    fill_ellipse(draw_x, draw_y + 23, 25, 7, color(20, 28, 35));
    if (f->flash > 0 && !save_data.reduced_flash && (frame_number & 1)) {
        fill_circle(draw_x, draw_y, 34, color(255, 255, 255));
        fill_circle(draw_x, draw_y, 30, arena_color(save_data.arena, 1));
    }
    if (f->protect_frames > 0) {
        fill_circle(draw_x, draw_y, 34, color(79, 199, 236));
        fill_circle(draw_x, draw_y, 29, arena_color(save_data.arena, 1));
    }
    if (f->cast_frames > 0) {
        int ring = 32 + (f->cast_frames % 5);
        fill_circle(draw_x, draw_y, ring, type_color(get_moveset(f->species)->ultimate_type));
        fill_circle(draw_x, draw_y, ring - 5, arena_color(save_data.arena, 1));
    }
    if (sprites_loaded)
        draw_animated_sprite(f->species, fighter_animation_frame(f),
                             draw_x - 32, draw_y - 40, 64, f->aim_x > 0.0f);
    else {
        fill_rect(draw_x - 27, draw_y - 12, 54, 24, color(24,30,42));
        draw_outline(draw_x - 27, draw_y - 12, 54, 24, color(255,120,110));
        draw_text(draw_x - (int)strlen(sp->name) * 2, draw_y - 2,
                  sp->name, 1, color(255,235,220));
    }
    if (f->attack_frames > 0 && f->attack_kind == 1 && sp->physical) {
        int reach = 34 + (f->attack_frames & 3) * 3;
        int sign = f->aim_x < 0.0f ? -1 : 1;
        draw_line(draw_x + sign * 18, draw_y, draw_x + sign * reach, draw_y - 8, type_color(sp->primary));
        draw_line(draw_x + sign * 19, draw_y + 3, draw_x + sign * (reach - 2), draw_y + 2, color(255,240,214));
    }
    if (f->burn_frames > 0) draw_text(draw_x - 7, draw_y - 47, "BRN", 1, color(255,151,76));
    else if (f->poison_frames > 0) draw_text(draw_x - 7, draw_y - 47, "PSN", 1, color(208,125,236));
    else if (f->paralysis_frames > 0) draw_text(draw_x - 7, draw_y - 47, "PAR", 1, color(255,226,91));
    else if (f->sleep_frames > 0) draw_text(draw_x - 7, draw_y - 47, "SLP", 1, color(176,210,255));
    (void)side;
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
        draw_projectile(p);
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
    char clock[16];
    snprintf(clock, sizeof(clock), "%02d:%02d", match_frames / 3600, (match_frames / 60) % 60);
    draw_center(8, clock, 1, color(255,232,143));
    fill_rect(0, 248, SCREEN_W, 24, color(13,22,32));
    draw_text(9, 251, "X BASIC SQ SKILL O DASH L+O MOVE TRI ULT", 1, color(242,244,225));
    if (player_team.count > 1)
        draw_text(39, 261, "L+SQ GUARD R+X/SQ/O SLOT 1/2/3 START PAUSE", 1, color(182,204,210));
    else
        draw_text(95, 261, "L+SQ GUARD  START PAUSE", 1, color(182,204,210));
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
    if (save_data.quick_wins < 2) {
        char training[32];
        snprintf(training, sizeof(training), "PIKACHU TRAINING %d/2 WINS", save_data.quick_wins);
        draw_text(123, 211, training, 1, color(138, 207, 205));
    } else {
        draw_text(122, 211, "3V3 CPU   16 STORY DUELS   6 ARENAS", 1, color(138, 207, 205));
    }
    if (!sprites_loaded)
        draw_center(223, "SPRITES MISSING - EXTRACT FULL ZIP", 1, color(255,145,130));
}

static void draw_setup(void) {
    draw_title();
    draw_center(89, "VS CPU SETUP", 2, color(245,240,220));
    const char *labels[] = {"PLAYER TEAM", "EDIT TEAM", "CPU LEVEL", "ARENA", "START MATCH"};
    for (int i = 0; i < 5; ++i) {
        int y = 112 + i * 23;
        if (i == vs_setup_cursor) fill_rect(105, y - 3, 270, 18, color(40,104,130));
        draw_text(118, y, labels[i], 1, color(245,241,221));
        if (i == 0) {
            int team = save_data.selected_team;
            draw_text(266, y, team == 0 ? "ONE" : team == 1 ? "TWO" : "THREE", 1, color(255,220,118));
        }
        if (i == 2) draw_text(266, y, difficulty_name(save_data.difficulty), 1, color(255,220,118));
        if (i == 3) draw_text(266, y, arena_names[save_data.arena], 1, color(255,220,118));
    }
    char lineup[64];
    snprintf(lineup, sizeof(lineup), "P1 %s  P2 %s  P3 %s",
             roster[save_data.teams[save_data.selected_team][0]].name,
             roster[save_data.teams[save_data.selected_team][1]].name,
             roster[save_data.teams[save_data.selected_team][2]].name);
    draw_center(222, lineup, 1, color(174,207,204));
    draw_text(91, 231, "LEFT RIGHT CHANGE   X EDIT OR START   O BACK", 1, color(173,197,202));
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
    draw_center(85, "CHOOSE YOUR POKEMON", 2, color(245,240,220));
    const char *team_name = save_data.selected_team == 0 ? "TEAM ONE" :
                            save_data.selected_team == 1 ? "TEAM TWO" : "TEAM THREE";
    draw_center(104, team_name, 1, color(173,197,202));
    for (int s = 0; s < TEAM_SIZE; ++s) {
        int x = 67 + s * 111;
        if (s == team_slot) fill_rect(x - 2, 112, 106, 31, color(40,104,130));
        draw_sprite(save_data.teams[save_data.selected_team][s], x, 114, 27, 0);
        draw_text(x + 29, 124, get_species(save_data.teams[save_data.selected_team][s])->name,
                  1, color(255,222,119));
    }
    draw_panel(10, 146, 138, 103);
    draw_animated_sprite(team_cursor, (frame_number / 16) % 4, 48, 147, 60, 0);
    const Species *candidate = get_species(team_cursor);
    const MoveSet *moves = get_moveset(team_cursor);
    draw_text(16, 210, candidate->name, 1, color(255,222,119));
    draw_text(16, 219, type_names[candidate->primary], 1, type_color(candidate->primary));
    draw_text(16, 228, "SQ", 1, color(173,197,202));
    draw_text(36, 228, moves->skill_name, 1, color(245,241,221));
    draw_text(16, 237, "L+O", 1, color(173,197,202));
    draw_text(36, 237, moves->utility_name, 1, color(245,241,221));
    draw_text(16, 246, "TRI", 1, color(173,197,202));
    draw_text(36, 246, moves->ultimate_name, 1, color(245,241,221));
    for (int id = 0; id < ROSTER_SIZE; ++id) {
        int col = id % 4, row = id / 4;
        int x = 155 + col * 80, y = 151 + row * 14;
        if (id == team_cursor) fill_rect(x - 2, y - 1, 78, 14, color(40,104,130));
        draw_sprite(id, x, y - 1, 14, 0);
        draw_text(x + 16, y + 4, roster[id].name, 1,
                  id == team_cursor ? color(255,222,119) : color(224,232,220));
    }
    draw_text(49, 260, "ARROWS PICK  X SET  L/R SLOT  SQ TEAM  O BACK", 1, color(173,197,202));
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
        case 3:
            team_slot = 0;
            team_cursor = save_data.teams[save_data.selected_team][0];
            team_return_screen = SCREEN_MENU;
            screen = SCREEN_TEAMS;
            break;
        case 4: settings_cursor = 0; screen = SCREEN_SETTINGS; break;
        case 5: screen = SCREEN_ONLINE_INFO; break;
        case 6: sceKernelExitGame(); break;
        }
    }
}

static void handle_setup(const SceCtrlData *pad) {
    if (pressed(pad->Buttons, PSP_CTRL_UP)) vs_setup_cursor = (vs_setup_cursor + 4) % 5;
    if (pressed(pad->Buttons, PSP_CTRL_DOWN)) vs_setup_cursor = (vs_setup_cursor + 1) % 5;
    int dir = pressed(pad->Buttons, PSP_CTRL_LEFT) ? -1 : pressed(pad->Buttons, PSP_CTRL_RIGHT) ? 1 : 0;
    if (dir && vs_setup_cursor == 0)
        save_data.selected_team = (uint8_t)((save_data.selected_team + dir + 3) % 3);
    if (dir && vs_setup_cursor == 2) save_data.difficulty = (uint8_t)((save_data.difficulty + dir + 3) % 3);
    if (dir && vs_setup_cursor == 3) save_data.arena = (uint8_t)((save_data.arena + dir + 6) % 6);
    if (pressed(pad->Buttons, PSP_CTRL_CROSS)) {
        if (vs_setup_cursor == 0 || vs_setup_cursor == 1) {
            team_slot = 0;
            team_cursor = save_data.teams[save_data.selected_team][team_slot];
            team_return_screen = SCREEN_VS_SETUP;
            screen = SCREEN_TEAMS;
        } else if (vs_setup_cursor == 4) start_match(MODE_VS_CPU);
        else vs_setup_cursor = (vs_setup_cursor + 1) % 5;
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
    if (pressed(pad->Buttons, PSP_CTRL_LEFT)) team_cursor = (team_cursor + ROSTER_SIZE - 1) % ROSTER_SIZE;
    if (pressed(pad->Buttons, PSP_CTRL_RIGHT)) team_cursor = (team_cursor + 1) % ROSTER_SIZE;
    if (pressed(pad->Buttons, PSP_CTRL_UP)) team_cursor = (team_cursor + ROSTER_SIZE - 4) % ROSTER_SIZE;
    if (pressed(pad->Buttons, PSP_CTRL_DOWN)) team_cursor = (team_cursor + 4) % ROSTER_SIZE;
    if (pressed(pad->Buttons, PSP_CTRL_CROSS)) save_team_editor();
    if (pressed(pad->Buttons, PSP_CTRL_SQUARE)) {
        save_data.selected_team = (uint8_t)((save_data.selected_team + 1) % 3);
        team_cursor = save_data.teams[save_data.selected_team][team_slot];
    }
    if (pressed(pad->Buttons, PSP_CTRL_LTRIGGER)) {
        team_slot = (team_slot + TEAM_SIZE - 1) % TEAM_SIZE;
        team_cursor = save_data.teams[save_data.selected_team][team_slot];
    }
    if (pressed(pad->Buttons, PSP_CTRL_RTRIGGER)) {
        team_slot = (team_slot + 1) % TEAM_SIZE;
        team_cursor = save_data.teams[save_data.selected_team][team_slot];
    }
    if (pressed(pad->Buttons, PSP_CTRL_CIRCLE)) {
        save_game();
        screen = team_return_screen;
    }
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
    log_line("BOOT: entered main()");
    log_result("pspDebugInstallErrorHandler",
               pspDebugInstallErrorHandler(exception_log_handler));
    log_result("sceCtrlSetSamplingCycle", sceCtrlSetSamplingCycle(0));
    log_result("sceCtrlSetSamplingMode",
               sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG));
    log_result("sceDisplaySetMode", sceDisplaySetMode(0, SCREEN_W, SCREEN_H));
    draw_buffer = framebuffers[0];
    display_buffer = framebuffers[1];
    sceKernelDcacheWritebackAll();
    log_result("sceDisplaySetFrameBuf",
               sceDisplaySetFrameBuf(display_buffer, BUFFER_W,
                                     PSP_DISPLAY_PIXEL_FORMAT_565,
                                     PSP_DISPLAY_SETBUF_IMMEDIATE));
    load_game();
    log_line("BOOT: save data initialization completed");
    load_sprite_atlas();
    log_line("BOOT: asset initialization completed");
    log_line("BOOT: entering main loop");
    int input_error_reported = 0;
    for (;;) {
        SceCtrlData pad;
        int input_result = sceCtrlPeekBufferPositive(&pad, 1);
        if (input_result < 0 && !input_error_reported) {
            log_result("sceCtrlPeekBufferPositive", input_result);
            input_error_reported = 1;
        }
        handle_screen(&pad);
        if (frame_number == 0) log_line("DRAW: first frame started");
        draw_screen();
        if (frame_number == 0) log_line("DRAW: first frame completed");
        /* The display controller reads RAM directly.  Flush the CPU cache
         * before this buffer becomes the visible framebuffer. */
        sceKernelDcacheWritebackAll();
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
