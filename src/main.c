#include <math.h>
#include <stdint.h>
#include <string.h>

#include <pspctrl.h>
#include <pspdisplay.h>
#include <pspkernel.h>
#include <pspmoduleinfo.h>

PSP_MODULE_INFO("PokemonArena2D", 0, 0, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#define SCREEN_W 480
#define SCREEN_H 272
#define BUFFER_W 512
#define ARENA_LEFT 28.0f
#define ARENA_RIGHT 452.0f
#define ARENA_TOP 44.0f
#define ARENA_BOTTOM 250.0f
#define PI 3.14159265358979323846f

typedef struct {
    float x;
    float y;
    float vx;
    float vy;
    int damage;
    int life;
    int owner;
    int active;
} Projectile;

typedef struct {
    float x;
    float y;
    float knock_x;
    float knock_y;
    float aim_x;
    float aim_y;
    int hp;
    int max_hp;
    int attack_cooldown;
    int dodge_cooldown;
    int invulnerable;
    int flash;
    int profile;
} Fighter;

static uint16_t framebuffers[2][BUFFER_W * SCREEN_H] __attribute__((aligned(64)));
static uint16_t *draw_buffer;
static uint16_t *display_buffer;
static Projectile projectiles[12];
static Fighter player;
static Fighter cpu;
static int frame_number;
static int match_over;
static int previous_buttons;

static const uint8_t font3x5[36][5] = {
    {7,5,7,5,5}, {6,5,6,5,6}, {7,4,4,4,7}, {6,5,5,5,6}, {7,4,6,4,7},
    {7,4,6,4,4}, {7,4,5,5,7}, {5,5,7,5,5}, {7,2,2,2,7}, {1,1,1,5,7},
    {5,5,6,5,5}, {4,4,4,4,7}, {5,7,7,5,5}, {5,7,7,7,5}, {7,5,5,5,7},
    {7,5,7,4,4}, {7,5,5,7,3}, {7,5,7,6,5}, {7,4,7,1,7}, {7,2,2,2,2},
    {5,5,5,5,7}, {5,5,5,5,2}, {5,5,7,7,5}, {5,5,2,5,5}, {5,5,2,2,2},
    {7,1,2,4,7}, {7,5,5,5,7}, {2,6,2,2,7}, {7,1,7,4,7}, {7,1,7,1,7},
    {5,5,7,1,1}, {7,4,7,1,7}, {7,4,7,5,7}, {7,1,2,2,2}, {7,5,7,5,7},
    {7,5,7,1,7}
};

static uint16_t rgb565(unsigned int r, unsigned int g, unsigned int b) {
    return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

static void clear_screen(uint16_t color) {
    for (int y = 0; y < SCREEN_H; ++y) {
        uint16_t *row = draw_buffer + y * BUFFER_W;
        for (int x = 0; x < SCREEN_W; ++x) row[x] = color;
    }
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
    for (int row = 0; row < 5; ++row) {
        for (int col = 0; col < 3; ++col) {
            if (font3x5[index][row] & (1 << (2 - col)))
                fill_rect(x + col * scale, y + row * scale, scale, scale, color);
        }
    }
}

static void draw_text(int x, int y, const char *text, int scale, uint16_t color) {
    for (const char *p = text; *p; ++p, x += 4 * scale)
        draw_char(x, y, *p, scale, color);
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
    float length = length2(*x, *y);
    if (length > 0.001f) {
        *x /= length;
        *y /= length;
    }
}

static void reset_match(void) {
    memset(projectiles, 0, sizeof(projectiles));
    memset(&player, 0, sizeof(player));
    memset(&cpu, 0, sizeof(cpu));
    player.x = 130.0f;
    player.y = 145.0f;
    player.hp = player.max_hp = 100;
    player.aim_x = 1.0f;
    cpu.x = 350.0f;
    cpu.y = 145.0f;
    cpu.hp = cpu.max_hp = 100;
    cpu.aim_x = -1.0f;
    match_over = 0;
}

static void launch_projectile(Fighter *owner, int owner_id, int damage, float speed) {
    for (int i = 0; i < (int)(sizeof(projectiles) / sizeof(projectiles[0])); ++i) {
        Projectile *p = &projectiles[i];
        if (p->active) continue;
        float ax = owner->aim_x;
        float ay = owner->aim_y;
        normalize(&ax, &ay);
        p->x = owner->x + ax * 18.0f;
        p->y = owner->y + ay * 18.0f;
        p->vx = ax * speed;
        p->vy = ay * speed;
        p->damage = damage;
        p->life = 70;
        p->owner = owner_id;
        p->active = 1;
        return;
    }
}

static void start_dodge(Fighter *f, float dx, float dy) {
    if (f->dodge_cooldown > 0) return;
    if (length2(dx, dy) < 0.1f) {
        dx = f->x - (f == &player ? cpu.x : player.x);
        dy = f->y - (f == &player ? cpu.y : player.y);
    }
    normalize(&dx, &dy);
    f->x += dx * 22.0f;
    f->y += dy * 22.0f;
    f->x = clampf(f->x, ARENA_LEFT + 14.0f, ARENA_RIGHT - 14.0f);
    f->y = clampf(f->y, ARENA_TOP + 14.0f, ARENA_BOTTOM - 14.0f);
    f->dodge_cooldown = 42;
    f->invulnerable = 12;
}

static void apply_hit(Fighter *target, const Projectile *p) {
    if (target->invulnerable > 0) return;
    target->hp -= p->damage;
    if (target->hp < 0) target->hp = 0;
    target->flash = 8;
    float dx = target->x - p->x;
    float dy = target->y - p->y;
    normalize(&dx, &dy);
    target->knock_x += dx * 2.5f;
    target->knock_y += dy * 2.5f;
}

static void update_player(const SceCtrlData *pad) {
    if (player.attack_cooldown > 0) --player.attack_cooldown;
    if (player.dodge_cooldown > 0) --player.dodge_cooldown;
    if (player.invulnerable > 0) --player.invulnerable;
    if (player.flash > 0) --player.flash;

    float dx = 0.0f;
    float dy = 0.0f;
    int ax = (int)pad->Lx - 128;
    int ay = (int)pad->Ly - 128;
    if (ax > 22 || ax < -22 || ay > 22 || ay < -22) {
        dx = (float)ax;
        dy = (float)ay;
    }
    if (pad->Buttons & PSP_CTRL_LEFT) dx -= 128.0f;
    if (pad->Buttons & PSP_CTRL_RIGHT) dx += 128.0f;
    if (pad->Buttons & PSP_CTRL_UP) dy -= 128.0f;
    if (pad->Buttons & PSP_CTRL_DOWN) dy += 128.0f;
    if (length2(dx, dy) > 128.0f) normalize(&dx, &dy);

    float move_speed = player.profile ? 2.5f : 2.0f;
    player.x += dx / 128.0f * move_speed;
    player.y += dy / 128.0f * move_speed;
    if (length2(dx, dy) > 1.0f) {
        float axf = dx;
        float ayf = dy;
        normalize(&axf, &ayf);
        player.aim_x = axf;
        player.aim_y = ayf;
    }
    player.x += player.knock_x;
    player.y += player.knock_y;
    player.knock_x *= 0.80f;
    player.knock_y *= 0.80f;
    player.x = clampf(player.x, ARENA_LEFT + 14.0f, ARENA_RIGHT - 14.0f);
    player.y = clampf(player.y, ARENA_TOP + 14.0f, ARENA_BOTTOM - 14.0f);

    if ((pad->Buttons & PSP_CTRL_CROSS) && player.attack_cooldown == 0) {
        float tx = cpu.x - player.x;
        float ty = cpu.y - player.y;
        normalize(&tx, &ty);
        player.aim_x = tx;
        player.aim_y = ty;
        launch_projectile(&player, 0, player.profile ? 13 : 10, 5.0f);
        player.attack_cooldown = player.profile ? 24 : 18;
    }
    if ((pad->Buttons & PSP_CTRL_CIRCLE) && !(previous_buttons & PSP_CTRL_CIRCLE))
        start_dodge(&player, dx, dy);
    if ((pad->Buttons & PSP_CTRL_TRIANGLE) && !(previous_buttons & PSP_CTRL_TRIANGLE))
        player.profile = !player.profile;
}

static void update_cpu(void) {
    if (cpu.attack_cooldown > 0) --cpu.attack_cooldown;
    if (cpu.dodge_cooldown > 0) --cpu.dodge_cooldown;
    if (cpu.invulnerable > 0) --cpu.invulnerable;
    if (cpu.flash > 0) --cpu.flash;

    float dx = player.x - cpu.x;
    float dy = player.y - cpu.y;
    float distance = length2(dx, dy);
    if (distance > 0.01f) {
        cpu.aim_x = dx / distance;
        cpu.aim_y = dy / distance;
    }
    if (distance > 115.0f) {
        cpu.x += cpu.aim_x * 0.8f;
        cpu.y += cpu.aim_y * 0.8f;
    } else if (distance < 62.0f) {
        cpu.x -= cpu.aim_x * 0.65f;
        cpu.y -= cpu.aim_y * 0.65f;
    }
    cpu.x += cpu.knock_x;
    cpu.y += cpu.knock_y;
    cpu.knock_x *= 0.82f;
    cpu.knock_y *= 0.82f;
    cpu.x = clampf(cpu.x, ARENA_LEFT + 14.0f, ARENA_RIGHT - 14.0f);
    cpu.y = clampf(cpu.y, ARENA_TOP + 14.0f, ARENA_BOTTOM - 14.0f);

    if (cpu.attack_cooldown == 0 && distance < 190.0f) {
        launch_projectile(&cpu, 1, 8, 3.7f);
        cpu.attack_cooldown = 72;
    }
}

static void update_projectiles(void) {
    for (int i = 0; i < (int)(sizeof(projectiles) / sizeof(projectiles[0])); ++i) {
        Projectile *p = &projectiles[i];
        if (!p->active) continue;
        p->x += p->vx;
        p->y += p->vy;
        --p->life;
        Fighter *target = p->owner == 0 ? &cpu : &player;
        float dx = target->x - p->x;
        float dy = target->y - p->y;
        if (dx * dx + dy * dy < 19.0f * 19.0f) {
            apply_hit(target, p);
            p->active = 0;
        } else if (p->life <= 0 || p->x < ARENA_LEFT || p->x > ARENA_RIGHT ||
                   p->y < ARENA_TOP || p->y > ARENA_BOTTOM) {
            p->active = 0;
        }
    }
}

static void draw_health_bar(int x, int y, int w, const Fighter *f, uint16_t fill) {
    fill_rect(x - 1, y - 1, w + 2, 9, rgb565(20, 25, 35));
    fill_rect(x, y, w, 7, rgb565(65, 35, 45));
    int amount = (w * f->hp) / f->max_hp;
    if (amount > 0) fill_rect(x, y, amount, 7, fill);
}

static void draw_fighter(const Fighter *f, int is_player) {
    int x = (int)f->x;
    int y = (int)f->y;
    uint16_t body;
    if (f->flash > 0) body = rgb565(255, 245, 210);
    else if (is_player) body = f->profile ? rgb565(240, 170, 65) : rgb565(70, 190, 235);
    else body = rgb565(225, 90, 120);
    uint16_t outline = rgb565(22, 28, 42);
    fill_circle(x, y + 2, 15, outline);
    fill_circle(x, y, 12, body);
    fill_circle(x + (int)(f->aim_x * 5.0f), y - 4 + (int)(f->aim_y * 5.0f), 3,
                rgb565(250, 250, 245));
    fill_circle(x + (int)(f->aim_x * 6.0f), y - 4 + (int)(f->aim_y * 5.0f), 1,
                rgb565(20, 25, 35));
    if (f->invulnerable > 0 && ((frame_number / 2) & 1))
        fill_circle(x, y, 18, rgb565(225, 230, 255));
}

static void render(void) {
    clear_screen(rgb565(17, 22, 36));
    fill_rect(0, 0, SCREEN_W, SCREEN_H, rgb565(17, 22, 36));
    fill_rect((int)ARENA_LEFT, (int)ARENA_TOP,
              (int)(ARENA_RIGHT - ARENA_LEFT), (int)(ARENA_BOTTOM - ARENA_TOP),
              rgb565(31, 48, 60));
    fill_rect((int)ARENA_LEFT, (int)ARENA_TOP,
              (int)(ARENA_RIGHT - ARENA_LEFT), 2, rgb565(91, 135, 145));
    fill_rect((int)ARENA_LEFT, (int)ARENA_BOTTOM - 2,
              (int)(ARENA_RIGHT - ARENA_LEFT), 2, rgb565(91, 135, 145));
    fill_rect((int)ARENA_LEFT, (int)ARENA_TOP, 2,
              (int)(ARENA_BOTTOM - ARENA_TOP), rgb565(91, 135, 145));
    fill_rect((int)ARENA_RIGHT - 2, (int)ARENA_TOP, 2,
              (int)(ARENA_BOTTOM - ARENA_TOP), rgb565(91, 135, 145));
    fill_circle(240, 147, 34, rgb565(38, 58, 69));
    fill_circle(240, 147, 32, rgb565(31, 48, 60));

    draw_text(28, 15, "PLAYER", 2, rgb565(210, 225, 235));
    draw_text(365, 15, "CPU", 2, rgb565(210, 225, 235));
    draw_health_bar(28, 29, 150, &player, rgb565(74, 205, 145));
    draw_health_bar(302, 29, 150, &cpu, rgb565(225, 96, 112));

    for (int i = 0; i < (int)(sizeof(projectiles) / sizeof(projectiles[0])); ++i) {
        Projectile *p = &projectiles[i];
        if (!p->active) continue;
        fill_circle((int)p->x, (int)p->y, 5,
                    p->owner == 0 ? rgb565(105, 225, 255) : rgb565(255, 150, 120));
        fill_circle((int)p->x, (int)p->y, 2, rgb565(250, 250, 245));
    }
    draw_fighter(&player, 1);
    draw_fighter(&cpu, 0);

    draw_text(28, 260, "X ATTACK", 1, rgb565(185, 205, 215));
    draw_text(132, 260, "O DODGE", 1,
              player.dodge_cooldown == 0 ? rgb565(185, 205, 215) : rgb565(100, 115, 125));
    draw_text(230, 260, "TRI SWAP", 1, rgb565(185, 205, 215));

    if (match_over) {
        fill_rect(110, 106, 260, 52, rgb565(18, 23, 36));
        fill_rect(110, 106, 260, 2, rgb565(115, 185, 195));
        fill_rect(110, 156, 260, 2, rgb565(115, 185, 195));
        draw_text(152, 118, player.hp > 0 ? "PLAYER WINS" : "CPU WINS", 2,
                  rgb565(245, 235, 205));
        draw_text(157, 140, "START RESET", 1, rgb565(175, 195, 205));
    }
}

static void swap_buffers(void) {
    sceDisplaySetFrameBuf(draw_buffer, BUFFER_W, PSP_DISPLAY_PIXEL_FORMAT_565,
                          PSP_DISPLAY_SETBUF_NEXTFRAME);
    sceDisplayWaitVblankStart();
    uint16_t *old_display = display_buffer;
    display_buffer = draw_buffer;
    draw_buffer = old_display;
}

int main(void) {
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
    sceDisplaySetMode(0, SCREEN_W, SCREEN_H);

    display_buffer = framebuffers[0];
    draw_buffer = framebuffers[1];
    sceDisplaySetFrameBuf(display_buffer, BUFFER_W, PSP_DISPLAY_PIXEL_FORMAT_565,
                          PSP_DISPLAY_SETBUF_IMMEDIATE);
    reset_match();

    SceCtrlData pad;
    memset(&pad, 0, sizeof(pad));
    while (1) {
        sceCtrlPeekBufferPositive(&pad, 1);
        if (pad.Buttons & PSP_CTRL_SELECT) break;
        if (match_over && (pad.Buttons & PSP_CTRL_START) &&
            !(previous_buttons & PSP_CTRL_START)) reset_match();

        if (!match_over) {
            update_player(&pad);
            update_cpu();
            update_projectiles();
            if (player.hp == 0 || cpu.hp == 0) match_over = 1;
        }

        render();
        swap_buffers();
        previous_buttons = pad.Buttons;
        ++frame_number;
    }
    sceDisplaySetFrameBuf(NULL, 0, PSP_DISPLAY_PIXEL_FORMAT_565,
                          PSP_DISPLAY_SETBUF_IMMEDIATE);
    sceKernelExitGame();
    return 0;
}
