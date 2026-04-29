// Brick Breaker Hit — Enhanced Ballz-style shooter in C + raylib
#include "raylib.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// ── Constants ────────────────────────────────────────────────────────────────
#define SCREEN_W              480
#define SCREEN_H              720
#define GRID_COLS             7
#define GRID_ROWS             9
#define CELL_SIZE             60.0f
#define GRID_OFFSET_X         ((SCREEN_W - GRID_COLS * CELL_SIZE) / 2.0f)
#define GRID_OFFSET_Y         52.0f
#define LAUNCHER_Y            (SCREEN_H - 52.0f)
#define LAUNCHER_X            (SCREEN_W / 2.0f)
#define MIN_LAUNCH_ANGLE_DEG  8.0f
#define BALL_RADIUS           9.0f
#define BALL_SPEED            680.0f
#define BALL_LAUNCH_DELAY     0.06f
#define MAX_BRICKS            (GRID_COLS * GRID_ROWS)
#define BALL_POOL_SIZE        200
#define MAX_PICKUPS           (GRID_COLS * 2)
#define SCORE_PER_BRICK       10
#define MAX_SCORES            5
#define NAME_LEN              12
#define ROUNDS_PER_LEVEL      10
#define MAX_SELECTABLE_LEVELS 12
#define TRAIL_LEN             12
#define MAX_PARTICLES         300
#define MAX_STARS             100
#define HUD_HEIGHT            48.0f
#define INITIAL_BALLS         7

// ── Menu button layout ────────────────────────────────────────────────────────
#define MENU_BTN_W   260
#define MENU_BTN_H   54
#define MENU_BTN_X   ((SCREEN_W - MENU_BTN_W) / 2)
#define MENU_BTN_Y0  296
#define MENU_BTN_GAP 68

// ── Types ─────────────────────────────────────────────────────────────────────
typedef enum {
    STATE_MENU,
    STATE_LEVEL_SELECT,
    STATE_PLAYING,
    STATE_PAUSED,
    STATE_LEVEL_UP,
    STATE_GAME_OVER,
    STATE_HIGHSCORE_ENTRY,
    STATE_HIGHSCORE_VIEW,
} GameState;

typedef struct { Vector2 pos[TRAIL_LEN]; int head, count; } Trail;

typedef struct {
    Vector2 position, velocity;
    bool    active, returned;
    Trail   trail;
} Ball;

typedef struct {
    int   hp, maxHp;
    bool  active;
    bool  explosive;    // explodes on death — damages 4 neighbours
    int   col, row;
    Color color;
    float flashTimer, shakeX;
} Brick;

typedef struct {
    int   col, row;
    bool  active;
    float pulseT;
} BallPickup;

typedef struct {
    Vector2 pos, vel;
    Color   color;
    float   life, maxLife, size, rot, rotSpeed;
} Particle;

typedef struct {
    float x, y, size, phase, speed;
} Star;

typedef struct { char name[NAME_LEN]; int score; } ScoreEntry;
typedef struct { ScoreEntry entries[MAX_SCORES]; int count; } Highscore;

typedef struct {
    Color bgA, bgB, gridLine, ballGlow, dangerLine, accentA, accentB;
} Theme;

typedef struct {
    Highscore highscores;
    int       unlockedLevels;
    int       reserved[7];
} SaveData;

typedef struct {
    GameState  state;
    int        round, level, score;
    int        ballCount, ballsToFire;
    float      launchTimer;

    Vector2    aimDir;
    bool       roundActive;

    Ball       balls[BALL_POOL_SIZE];
    int        activeBallCount, returnedBallCount;
    float      firstReturnX;
    bool       firstReturned;

    Brick      bricks[MAX_BRICKS];
    BallPickup pickups[MAX_PICKUPS];
    Particle   particles[MAX_PARTICLES];
    Star       stars[MAX_STARS];

    Sound  sndHit, sndDestroy, sndPickup, sndGameOver, sndRoundEnd, sndLevelUp;
    bool   soundEnabled, audioReady;

    Highscore highscores;
    int       unlockedLevels;
    char      inputName[NAME_LEN];
    int       inputLen;

    float  animTimer, levelUpTimer, launcherX, launcherPulse;
    float  menuAnimT;
    int    menuHover;        // -1 = none
    int    lvSelHover;       // 0-based level index, -1 = none

    Theme  theme, nextTheme;
    float  themeFadeT;

    // Round combo
    int    roundCombo;
    float  comboDisplayTimer;
} Game;

// ── Level themes (12 unique) ──────────────────────────────────────────────────
static const Theme THEMES[] = {
    // 1 Dark Blue
    {{10,12,35,255},{5,8,20,255},{60,80,160,20},{80,160,255,255},{200,50,50,200},{60,140,255,255},{30,70,180,255}},
    // 2 Deep Purple
    {{20,8,40,255},{10,5,25,255},{120,60,180,20},{200,100,255,255},{220,50,80,200},{180,80,255,255},{100,40,180,255}},
    // 3 Dark Teal
    {{5,30,35,255},{3,18,22,255},{40,160,140,20},{60,230,200,255},{255,80,50,200},{40,210,180,255},{20,120,100,255}},
    // 4 Dark Red
    {{35,8,8,255},{22,5,5,255},{180,50,40,20},{255,120,60,255},{255,60,60,200},{255,80,50,255},{180,40,30,255}},
    // 5 Midnight Green
    {{5,35,15,255},{3,20,8,255},{40,180,80,20},{80,255,120,255},{255,100,50,200},{60,240,100,255},{30,150,60,255}},
    // 6 Gold
    {{35,25,5,255},{22,15,3,255},{180,130,30,20},{255,210,60,255},{255,60,60,200},{255,200,40,255},{180,130,20,255}},
    // 7 Cyan
    {{5,20,40,255},{3,12,28,255},{40,160,200,20},{60,220,255,255},{255,80,80,200},{40,210,255,255},{20,130,180,255}},
    // 8 Rose
    {{40,5,20,255},{25,3,12,255},{200,60,120,20},{255,100,180,255},{255,80,50,200},{255,80,160,255},{180,40,100,255}},
    // 9 Lime
    {{15,35,5,255},{8,22,3,255},{120,200,40,20},{180,255,60,255},{255,80,50,200},{200,255,40,255},{120,180,20,255}},
    // 10 Ice
    {{10,20,45,255},{5,12,30,255},{80,160,220,20},{180,230,255,255},{255,60,80,200},{140,210,255,255},{80,150,220,255}},
    // 11 Volcano
    {{40,15,5,255},{28,10,3,255},{220,100,30,20},{255,160,40,255},{255,50,50,200},{255,130,30,255},{180,80,15,255}},
    // 12 Void
    {{15,5,30,255},{8,3,20,255},{100,40,160,20},{180,80,255,255},{255,60,100,200},{200,60,255,255},{120,30,180,255}},
};
#define NUM_THEMES 12

// ── Forward declarations ───────────────────────────────────────────────────────
void  InitGame(Game *g);
void  ResetGame(Game *g, int startLevel);
void  SpawnNewRow(Game *g);
void  LoadSaveData(Game *g);
void  WriteSaveData(const Game *g);
bool  IsNewHighscore(const Highscore *hs, int score);
void  InsertHighscore(Highscore *hs, const char *name, int score);
Sound GenerateBeep(float freq, float dur, float vol);
void  PlaySfx(const Game *g, Sound snd);
void  SpawnBrickParticles(Game *g, int col, int row, Color c);
void  SpawnMenuOrb(Game *g);
void  UpdateParticles(Game *g, float dt);
void  ExplodeBrick(Game *g, int col, int row);
void  InitStars(Game *g);

void UpdateMenu(Game *g, float dt);
void UpdateLevelSelect(Game *g, float dt);
void UpdatePlaying(Game *g, float dt);
void UpdatePaused(Game *g);
void UpdateLevelUp(Game *g, float dt);
void UpdateGameOver(Game *g);
void UpdateHighscoreEntry(Game *g);
void UpdateHighscoreView(Game *g);
void UpdateAim(Game *g);
void UpdateFiring(Game *g, float dt);
void UpdateBallPhysics(Game *g, float dt);
void CheckRoundEnd(Game *g);

void DrawBackground(const Game *g);
void DrawStars(const Game *g);
void DrawMenu(const Game *g);
void DrawLevelSelect(const Game *g);
void DrawPlaying(const Game *g);
void DrawPaused(const Game *g);
void DrawHUD(const Game *g);
void DrawBricks(const Game *g);
void DrawBalls(const Game *g);
void DrawPickups(const Game *g);
void DrawLauncher(const Game *g);
void DrawTrajectory(const Game *g);
void DrawParticles(const Game *g);
void DrawLevelUp(const Game *g);
void DrawGameOver(const Game *g);
void DrawHighscoreEntry(const Game *g);
void DrawHighscoreView(const Game *g);
void DrawMenuButton(const char *label, Rectangle rect, bool hovered, bool primary, Color accent);

Color     GetBrickColor(int hp, int level);
Rectangle GetCellRect(int col, int row);
Vector2   GetCellCenter(int col, int row);
Rectangle GetMenuBtnRect(int idx);
Rectangle GetLevelTileRect(int level);
Color     BlendColor(Color a, Color b, float t);
void      DrawGlowCircle(Vector2 center, float radius, Color inner, Color outer, int rings);
void      AddTrailPoint(Trail *t, Vector2 pos);
Theme     GetThemeForLevel(int level);

// ── Utilities ─────────────────────────────────────────────────────────────────
Color BlendColor(Color a, Color b, float t) {
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return (Color){
        (unsigned char)(a.r + (b.r - a.r) * t),
        (unsigned char)(a.g + (b.g - a.g) * t),
        (unsigned char)(a.b + (b.b - a.b) * t),
        (unsigned char)(a.a + (b.a - a.a) * t),
    };
}

void DrawGlowCircle(Vector2 c, float radius, Color inner, Color outer, int rings) {
    for (int i = rings; i >= 0; i--) {
        float t   = (float)i / rings;
        float r   = radius + t * radius * 1.6f;
        Color col = BlendColor(inner, outer, t);
        col.a     = (unsigned char)((float)col.a * (1.0f - t) * 0.6f);
        DrawCircleV(c, r, col);
    }
    DrawCircleV(c, radius, inner);
}

Rectangle GetCellRect(int col, int row) {
    return (Rectangle){
        GRID_OFFSET_X + col * CELL_SIZE + 3,
        GRID_OFFSET_Y + row * CELL_SIZE + 3,
        CELL_SIZE - 6, CELL_SIZE - 6
    };
}

Vector2 GetCellCenter(int col, int row) {
    return (Vector2){
        GRID_OFFSET_X + col * CELL_SIZE + CELL_SIZE * 0.5f,
        GRID_OFFSET_Y + row * CELL_SIZE + CELL_SIZE * 0.5f
    };
}

Rectangle GetMenuBtnRect(int idx) {
    return (Rectangle){MENU_BTN_X, MENU_BTN_Y0 + idx * MENU_BTN_GAP, MENU_BTN_W, MENU_BTN_H};
}

Rectangle GetLevelTileRect(int level) {
    // 4 columns × 3 rows, 1-based
    int i = level - 1;
    int col = i % 4, row = i / 4;
    return (Rectangle){28.0f + col * 108.0f, 145.0f + row * 92.0f, 100.0f, 82.0f};
}

Theme GetThemeForLevel(int level) {
    return THEMES[(level - 1) % NUM_THEMES];
}

void AddTrailPoint(Trail *t, Vector2 pos) {
    t->pos[t->head] = pos;
    t->head = (t->head + 1) % TRAIL_LEN;
    if (t->count < TRAIL_LEN) t->count++;
}

// ── Stars ─────────────────────────────────────────────────────────────────────
void InitStars(Game *g) {
    for (int i = 0; i < MAX_STARS; i++) {
        g->stars[i].x     = (float)GetRandomValue(0, SCREEN_W);
        g->stars[i].y     = (float)GetRandomValue(0, SCREEN_H);
        g->stars[i].size  = (float)GetRandomValue(3, 22) / 10.0f;
        g->stars[i].phase = (float)GetRandomValue(0, 628) / 100.0f;
        g->stars[i].speed = (float)GetRandomValue(8, 30) / 10.0f;
    }
}

void DrawStars(const Game *g) {
    float t = (float)GetTime();
    for (int i = 0; i < MAX_STARS; i++) {
        const Star *s = &g->stars[i];
        float b = 0.25f + 0.6f * (0.5f + 0.5f * sinf(t * s->speed + s->phase));
        DrawCircleV((Vector2){s->x, s->y}, s->size, Fade(WHITE, b));
    }
}

// ── Sound ─────────────────────────────────────────────────────────────────────
Sound GenerateBeep(float freq, float dur, float vol) {
    int sampleRate  = 44100;
    int sampleCount = (int)(sampleRate * dur);
    short *data     = (short *)malloc(sampleCount * sizeof(short));
    for (int i = 0; i < sampleCount; i++) {
        float tSec = (float)i / sampleRate;
        float env  = 1.0f - (tSec / dur);
        data[i]    = (short)(sinf(2.0f * PI * freq * tSec) * env * 32767.0f * vol);
    }
    Wave w = {.frameCount=(unsigned int)sampleCount, .sampleRate=44100,
              .sampleSize=16, .channels=1, .data=data};
    Sound s = LoadSoundFromWave(w);
    free(data);
    return s;
}

void PlaySfx(const Game *g, Sound snd) {
    if (g->soundEnabled && g->audioReady) PlaySound(snd);
}

// ── Particles ─────────────────────────────────────────────────────────────────
void SpawnBrickParticles(Game *g, int col, int row, Color c) {
    Vector2 center = GetCellCenter(col, row);
    int spawned = 0;
    for (int i = 0; i < MAX_PARTICLES && spawned < 16; i++) {
        Particle *p = &g->particles[i];
        if (p->life > 0.0f) continue;
        float angle = (float)GetRandomValue(0, 360) * DEG2RAD;
        float speed = (float)GetRandomValue(60, 320);
        p->pos      = center;
        p->vel      = (Vector2){cosf(angle) * speed, sinf(angle) * speed};
        p->color    = c;
        p->maxLife  = (float)GetRandomValue(25, 60) / 100.0f;
        p->life     = p->maxLife;
        p->size     = (float)GetRandomValue(3, 11);
        p->rot      = (float)GetRandomValue(0, 360);
        p->rotSpeed = (float)GetRandomValue(-500, 500);
        spawned++;
    }
}

void SpawnMenuOrb(Game *g) {
    // Spawn a slow glowing orb that drifts across the menu background
    for (int i = 0; i < MAX_PARTICLES; i++) {
        Particle *p = &g->particles[i];
        if (p->life > 0.0f) continue;
        float hue = (float)GetRandomValue(160, 280);
        Color c   = ColorFromHSV(hue, 0.7f, 1.0f);
        p->pos      = (Vector2){(float)GetRandomValue(0, SCREEN_W), (float)GetRandomValue(100, SCREEN_H - 50)};
        float ang   = (float)GetRandomValue(0, 360) * DEG2RAD;
        float spd   = (float)GetRandomValue(15, 50);
        p->vel      = (Vector2){cosf(ang) * spd, sinf(ang) * spd};
        p->color    = c;
        p->maxLife  = (float)GetRandomValue(180, 350) / 100.0f;
        p->life     = p->maxLife;
        p->size     = (float)GetRandomValue(18, 48);
        p->rot      = 0;
        p->rotSpeed = 0;
        return;
    }
}

void UpdateParticles(Game *g, float dt) {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        Particle *p = &g->particles[i];
        if (p->life <= 0.0f) continue;
        p->life   -= dt;
        p->pos.x  += p->vel.x * dt;
        p->pos.y  += p->vel.y * dt;
        // Game particles get gravity; menu orbs do not
        if (p->size < 16) p->vel.y += 400.0f * dt;
        p->vel.x  *= 0.98f;
        p->rot    += p->rotSpeed * dt;
    }
}

// ── Explosive brick — damages 4 neighbours, chains if they're also explosive ───
void ExplodeBrick(Game *g, int col, int row) {
    const int dx[] = {-1, 1, 0, 0};
    const int dy[] = {0, 0, -1, 1};
    for (int d = 0; d < 4; d++) {
        int nc = col + dx[d], nr = row + dy[d];
        if (nc < 0 || nc >= GRID_COLS || nr < 0 || nr >= GRID_ROWS) continue;
        for (int i = 0; i < MAX_BRICKS; i++) {
            Brick *b = &g->bricks[i];
            if (!b->active || b->col != nc || b->row != nr) continue;
            b->hp--;
            b->flashTimer = 0.12f;
            b->shakeX     = 6.0f;
            if (b->hp <= 0) {
                bool wasExp = b->explosive;
                int  bc = b->col, br2 = b->row;
                Color bCol = b->color;
                b->active = false;          // deactivate BEFORE recursing
                g->score += SCORE_PER_BRICK;
                g->roundCombo++;
                g->comboDisplayTimer = 1.8f;
                SpawnBrickParticles(g, bc, br2, bCol);
                if (wasExp) ExplodeBrick(g, bc, br2);
            }
            break;
        }
    }
}

// ── Highscores / Save ─────────────────────────────────────────────────────────
void LoadSaveData(Game *g) {
    SaveData sd = {0};
    FILE *f = fopen("bbhit_save.dat", "rb");
    if (f) { fread(&sd, sizeof(SaveData), 1, f); fclose(f); }
    g->highscores     = sd.highscores;
    g->unlockedLevels = (sd.unlockedLevels >= 1) ? sd.unlockedLevels : 1;
    if (g->unlockedLevels > MAX_SELECTABLE_LEVELS) g->unlockedLevels = MAX_SELECTABLE_LEVELS;
}

void WriteSaveData(const Game *g) {
    SaveData sd = {0};
    sd.highscores     = g->highscores;
    sd.unlockedLevels = g->unlockedLevels;
    FILE *f = fopen("bbhit_save.dat", "wb");
    if (f) { fwrite(&sd, sizeof(SaveData), 1, f); fclose(f); }
}

bool IsNewHighscore(const Highscore *hs, int score) {
    if (score <= 0) return false;
    if (hs->count < MAX_SCORES) return true;
    for (int i = 0; i < hs->count; i++)
        if (score > hs->entries[i].score) return true;
    return false;
}

void InsertHighscore(Highscore *hs, const char *name, int score) {
    int pos = hs->count;
    for (int i = 0; i < hs->count; i++) {
        if (score > hs->entries[i].score) { pos = i; break; }
    }
    int end = (hs->count < MAX_SCORES) ? hs->count : MAX_SCORES - 1;
    for (int i = end; i > pos; i--) hs->entries[i] = hs->entries[i - 1];
    strncpy(hs->entries[pos].name, name, NAME_LEN - 1);
    hs->entries[pos].name[NAME_LEN - 1] = '\0';
    hs->entries[pos].score = score;
    if (hs->count < MAX_SCORES) hs->count++;
}

// ── Brick color ───────────────────────────────────────────────────────────────
Color GetBrickColor(int hp, int level) {
    float hue = fmodf((float)(level * 53 + (int)(hp * 0.3f)), 360.0f);
    if (hp <= 3)        return ColorFromHSV(hue, 0.55f, 1.00f);
    else if (hp <= 10)  return ColorFromHSV(hue, 0.70f, 0.95f);
    else if (hp <= 30)  return ColorFromHSV(hue, 0.80f, 0.88f);
    else if (hp <= 100) return ColorFromHSV(hue, 0.90f, 0.78f);
    else                return ColorFromHSV(hue, 1.00f, 0.65f);
}

// ── SpawnNewRow ───────────────────────────────────────────────────────────────
void SpawnNewRow(Game *g) {
    for (int i = 0; i < MAX_BRICKS; i++)
        if (g->bricks[i].active) g->bricks[i].row++;
    for (int i = 0; i < MAX_PICKUPS; i++) {
        if (g->pickups[i].active) g->pickups[i].row++;
        if (g->pickups[i].active && g->pickups[i].row >= GRID_ROWS)
            g->pickups[i].active = false;
    }

    for (int i = 0; i < MAX_BRICKS; i++) {
        if (g->bricks[i].active && g->bricks[i].row >= GRID_ROWS) {
            g->state     = STATE_GAME_OVER;
            g->animTimer = 0.0f;
            PlaySfx(g, g->sndGameOver);
            return;
        }
    }

    int globalRound = (g->level - 1) * ROUNDS_PER_LEVEL + g->round;
    int newHP = (int)((float)globalRound * (1.0f + (g->level - 1) * 0.3f))
                + GetRandomValue(0, globalRound / 3 + 1);
    if (newHP < 1) newHP = 1;

    int bricksInRow = GetRandomValue(3, GRID_COLS - 1);
    int cols[GRID_COLS] = {0,1,2,3,4,5,6};
    for (int i = GRID_COLS - 1; i > 0; i--) {
        int j = GetRandomValue(0, i);
        int tmp = cols[i]; cols[i] = cols[j]; cols[j] = tmp;
    }

    bool occupied[GRID_COLS] = {false};
    for (int i = 0; i < bricksInRow; i++) {
        occupied[cols[i]] = true;
        for (int s = 0; s < MAX_BRICKS; s++) {
            if (!g->bricks[s].active) {
                int hp = newHP + GetRandomValue(-newHP/4, newHP/4);
                if (hp < 1) hp = 1;
                g->bricks[s].active     = true;
                g->bricks[s].col        = cols[i];
                g->bricks[s].row        = 0;
                g->bricks[s].hp         = hp;
                g->bricks[s].maxHp      = hp;
                g->bricks[s].color      = GetBrickColor(hp, g->level);
                g->bricks[s].flashTimer = 0.0f;
                g->bricks[s].shakeX     = 0.0f;
                g->bricks[s].explosive  = (GetRandomValue(0, 4) == 0); // 20% chance
                break;
            }
        }
    }

    if (GetRandomValue(0, 2) == 0) {
        int freeCols[GRID_COLS]; int fc = 0;
        for (int c = 0; c < GRID_COLS; c++) if (!occupied[c]) freeCols[fc++] = c;
        if (fc > 0) {
            int chosen = freeCols[GetRandomValue(0, fc - 1)];
            for (int s = 0; s < MAX_PICKUPS; s++) {
                if (!g->pickups[s].active) {
                    g->pickups[s].active = true;
                    g->pickups[s].col    = chosen;
                    g->pickups[s].row    = 0;
                    g->pickups[s].pulseT = (float)GetRandomValue(0, 628) / 100.0f;
                    break;
                }
            }
        }
    }

    g->round++;
    if (g->round > ROUNDS_PER_LEVEL) {
        g->round = 1;
        g->level++;
        // Unlock level for select screen
        if (g->level <= MAX_SELECTABLE_LEVELS && g->level > g->unlockedLevels) {
            g->unlockedLevels = g->level;
            WriteSaveData(g);
        }
        g->theme        = GetThemeForLevel(g->level);
        g->levelUpTimer = 0.0f;
        g->state        = STATE_LEVEL_UP;
        PlaySfx(g, g->sndLevelUp);
    }
}

// ── Init / Reset ──────────────────────────────────────────────────────────────
void InitGame(Game *g) {
    memset(g, 0, sizeof(Game));
    g->state        = STATE_MENU;
    g->round        = 1;
    g->level        = 1;
    g->ballCount    = INITIAL_BALLS;
    g->launcherX    = LAUNCHER_X;
    g->aimDir       = (Vector2){0.0f, -1.0f};
    g->soundEnabled = true;
    g->theme        = GetThemeForLevel(1);
    g->menuHover    = -1;
    g->lvSelHover   = -1;

    if (IsAudioDeviceReady()) {
        g->audioReady  = true;
        g->sndHit      = GenerateBeep(520.0f,  0.04f, 0.5f);
        g->sndDestroy  = GenerateBeep(860.0f,  0.10f, 0.65f);
        g->sndPickup   = GenerateBeep(1300.0f, 0.14f, 0.75f);
        g->sndGameOver = GenerateBeep(110.0f,  0.90f, 0.85f);
        g->sndRoundEnd = GenerateBeep(680.0f,  0.18f, 0.60f);
        g->sndLevelUp  = GenerateBeep(990.0f,  0.35f, 0.75f);
    }

    LoadSaveData(g);
    InitStars(g);

    for (int i = 0; i < 8; i++) SpawnMenuOrb(g);
}

void ResetGame(Game *g, int startLevel) {
    Sound h = g->sndHit, d = g->sndDestroy, pk = g->sndPickup;
    Sound go = g->sndGameOver, re = g->sndRoundEnd, lu = g->sndLevelUp;
    bool snd = g->soundEnabled, aud = g->audioReady;
    Highscore hs       = g->highscores;
    int unlocked       = g->unlockedLevels;
    Star savedStars[MAX_STARS];
    memcpy(savedStars, g->stars, sizeof(savedStars));

    memset(g, 0, sizeof(Game));
    g->state        = STATE_PLAYING;
    g->round        = 1;
    g->level        = startLevel;
    g->ballCount    = INITIAL_BALLS;
    g->launcherX    = LAUNCHER_X;
    g->aimDir       = (Vector2){0.0f, -1.0f};
    g->soundEnabled = snd;
    g->audioReady   = aud;
    g->sndHit       = h; g->sndDestroy = d; g->sndPickup = pk;
    g->sndGameOver  = go; g->sndRoundEnd = re; g->sndLevelUp = lu;
    g->highscores   = hs;
    g->unlockedLevels = unlocked;
    g->theme        = GetThemeForLevel(startLevel);
    g->menuHover    = -1;
    g->lvSelHover   = -1;
    memcpy(g->stars, savedStars, sizeof(savedStars));

    // Pre-fill 3 rows at startLevel difficulty
    for (int r = 0; r < 3; r++) SpawnNewRow(g);
    g->round = 1;
    g->level = startLevel;
    g->state = STATE_PLAYING;
    g->theme = GetThemeForLevel(startLevel);
}

// ── Aim ───────────────────────────────────────────────────────────────────────
void UpdateAim(Game *g) {
    Vector2 mouse    = GetMousePosition();
    Vector2 launcher = {g->launcherX, LAUNCHER_Y};
    Vector2 diff     = {mouse.x - launcher.x, mouse.y - launcher.y};
    if (diff.y >= -8.0f) return;

    float angle = atan2f(-diff.y, diff.x);
    float minR  = MIN_LAUNCH_ANGLE_DEG * DEG2RAD;
    if (angle < minR)      angle = minR;
    if (angle > PI - minR) angle = PI - minR;
    g->aimDir = (Vector2){cosf(angle), -sinf(angle)};
}

// ── Firing ────────────────────────────────────────────────────────────────────
static void FireOneBall(Game *g) {
    for (int i = 0; i < BALL_POOL_SIZE; i++) {
        if (!g->balls[i].active && !g->balls[i].returned) {
            g->balls[i].active   = true;
            g->balls[i].returned = false;
            g->balls[i].position = (Vector2){g->launcherX, LAUNCHER_Y - BALL_RADIUS - 1.0f};
            g->balls[i].velocity = (Vector2){g->aimDir.x * BALL_SPEED, g->aimDir.y * BALL_SPEED};
            memset(&g->balls[i].trail, 0, sizeof(Trail));
            g->activeBallCount++;
            return;
        }
    }
}

void UpdateFiring(Game *g, float dt) {
    if (g->ballsToFire <= 0) return;
    g->launchTimer -= dt;
    if (g->launchTimer <= 0.0f) {
        FireOneBall(g);
        g->ballsToFire--;
        g->launchTimer = BALL_LAUNCH_DELAY;
    }
}

// ── Ball physics ──────────────────────────────────────────────────────────────
void UpdateBallPhysics(Game *g, float dt) {
    float leftWall  = GRID_OFFSET_X + BALL_RADIUS;
    float rightWall = GRID_OFFSET_X + GRID_COLS * CELL_SIZE - BALL_RADIUS;
    float topWall   = GRID_OFFSET_Y + BALL_RADIUS;

    for (int bi = 0; bi < BALL_POOL_SIZE; bi++) {
        Ball *b = &g->balls[bi];
        if (!b->active) continue;

        AddTrailPoint(&b->trail, b->position);
        b->position.x += b->velocity.x * dt;
        b->position.y += b->velocity.y * dt;

        if (b->position.x <= leftWall)  { b->position.x = leftWall;  b->velocity.x =  fabsf(b->velocity.x); }
        if (b->position.x >= rightWall) { b->position.x = rightWall; b->velocity.x = -fabsf(b->velocity.x); }
        if (b->position.y <= topWall)   { b->position.y = topWall;   b->velocity.y =  fabsf(b->velocity.y); }

        if (b->position.y >= LAUNCHER_Y) {
            b->active   = false;
            b->returned = true;
            if (!g->firstReturned) { g->firstReturned = true; g->firstReturnX = b->position.x; }
            g->returnedBallCount++;
            g->activeBallCount--;
            continue;
        }

        bool hitBrick = false;
        for (int i = 0; i < MAX_BRICKS && !hitBrick; i++) {
            Brick *br = &g->bricks[i];
            if (!br->active) continue;
            Rectangle cell = {
                GRID_OFFSET_X + br->col * CELL_SIZE, GRID_OFFSET_Y + br->row * CELL_SIZE,
                CELL_SIZE, CELL_SIZE
            };
            if (!CheckCollisionCircleRec(b->position, BALL_RADIUS, cell)) continue;
            hitBrick = true;

            float bL = b->position.x - BALL_RADIUS, bR = b->position.x + BALL_RADIUS;
            float bT = b->position.y - BALL_RADIUS, bB = b->position.y + BALL_RADIUS;
            float oL = bR - cell.x, oR = (cell.x + cell.width)  - bL;
            float oT = bB - cell.y, oB = (cell.y + cell.height) - bT;
            float mX = fminf(oL, oR), mY = fminf(oT, oB);
            if (mX < mY) b->velocity.x = -b->velocity.x;
            else         b->velocity.y = -b->velocity.y;

            br->hp--;
            br->flashTimer = 0.06f;
            br->shakeX     = 3.0f;

            if (br->hp <= 0) {
                bool wasExplosive = br->explosive;
                int  bCol = br->col, bRow = br->row;
                Color bColor = br->color;
                SpawnBrickParticles(g, bCol, bRow, bColor);
                br->active = false;
                g->score  += SCORE_PER_BRICK;
                g->roundCombo++;
                g->comboDisplayTimer = 1.8f;
                PlaySfx(g, g->sndDestroy);
                if (wasExplosive) ExplodeBrick(g, bCol, bRow);
            } else {
                PlaySfx(g, g->sndHit);
            }
        }

        for (int i = 0; i < MAX_PICKUPS; i++) {
            BallPickup *p = &g->pickups[i];
            if (!p->active) continue;
            Vector2 center = GetCellCenter(p->col, p->row);
            if (CheckCollisionCircles(b->position, BALL_RADIUS, center, CELL_SIZE * 0.28f)) {
                p->active = false;
                g->ballCount++;
                PlaySfx(g, g->sndPickup);
            }
        }
    }

    for (int i = 0; i < MAX_BRICKS; i++) {
        if (g->bricks[i].flashTimer > 0.0f) {
            g->bricks[i].flashTimer -= dt;
            if (g->bricks[i].flashTimer < 0.0f) g->bricks[i].flashTimer = 0.0f;
        }
        if (g->bricks[i].shakeX > 0.0f) {
            g->bricks[i].shakeX *= 0.82f;
            if (g->bricks[i].shakeX < 0.05f) g->bricks[i].shakeX = 0.0f;
        }
    }
}

// ── Round end ─────────────────────────────────────────────────────────────────
void CheckRoundEnd(Game *g) {
    if (g->ballsToFire > 0 || g->activeBallCount > 0) return;

    float cx = g->firstReturnX;
    float margin = GRID_OFFSET_X + BALL_RADIUS + 4.0f;
    if (cx < margin) cx = margin;
    if (cx > SCREEN_W - margin) cx = SCREEN_W - margin;
    g->launcherX = cx;

    PlaySfx(g, g->sndRoundEnd);
    SpawnNewRow(g);
    if (g->state == STATE_GAME_OVER || g->state == STATE_LEVEL_UP) return;

    for (int i = 0; i < BALL_POOL_SIZE; i++) { g->balls[i].active = false; g->balls[i].returned = false; }
    g->activeBallCount = g->returnedBallCount = 0;
    g->firstReturned   = false;
    g->roundActive     = false;
}

// ── Update: Menu ──────────────────────────────────────────────────────────────
void UpdateMenu(Game *g, float dt) {
    g->menuAnimT += dt;
    UpdateParticles(g, dt);

    // Replenish menu orbs
    int orbCount = 0;
    for (int i = 0; i < MAX_PARTICLES; i++) if (g->particles[i].life > 0.0f && g->particles[i].size >= 16) orbCount++;
    if (orbCount < 7 && GetRandomValue(0, 60) == 0) SpawnMenuOrb(g);

    Vector2 mouse = GetMousePosition();
    g->menuHover = -1;
    for (int i = 0; i < 4; i++) {
        if (CheckCollisionPointRec(mouse, GetMenuBtnRect(i))) { g->menuHover = i; break; }
    }

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        switch (g->menuHover) {
            case 0: ResetGame(g, 1); break;
            case 1: g->state = STATE_LEVEL_SELECT; break;
            case 2: g->state = STATE_HIGHSCORE_VIEW; break;
            case 3: CloseWindow(); break;
        }
    }
    if (IsKeyPressed(KEY_ENTER))  ResetGame(g, 1);
    if (IsKeyPressed(KEY_L))      g->state = STATE_LEVEL_SELECT;
    if (IsKeyPressed(KEY_H))      g->state = STATE_HIGHSCORE_VIEW;
    if (IsKeyPressed(KEY_M))      g->soundEnabled = !g->soundEnabled;
    if (IsKeyPressed(KEY_ESCAPE)) CloseWindow();
}

// ── Update: Level Select ──────────────────────────────────────────────────────
void UpdateLevelSelect(Game *g, float dt) {
    g->menuAnimT += dt;

    Vector2 mouse = GetMousePosition();
    g->lvSelHover = -1;
    for (int lv = 1; lv <= MAX_SELECTABLE_LEVELS; lv++) {
        if (lv > g->unlockedLevels) continue;
        if (CheckCollisionPointRec(mouse, GetLevelTileRect(lv))) { g->lvSelHover = lv; break; }
    }

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && g->lvSelHover >= 1) {
        ResetGame(g, g->lvSelHover);
    }
    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_BACKSPACE)) g->state = STATE_MENU;
    if (IsKeyPressed(KEY_M)) g->soundEnabled = !g->soundEnabled;
}

// ── Update: Paused ────────────────────────────────────────────────────────────
void UpdatePaused(Game *g) {
    if (IsKeyPressed(KEY_P) || IsKeyPressed(KEY_ESCAPE)) g->state = STATE_PLAYING;
    if (IsKeyPressed(KEY_Q)) g->state = STATE_MENU;
    if (IsKeyPressed(KEY_M)) g->soundEnabled = !g->soundEnabled;
}

// ── Update: Playing ───────────────────────────────────────────────────────────
void UpdatePlaying(Game *g, float dt) {
    if (IsKeyPressed(KEY_P) || IsKeyPressed(KEY_ESCAPE)) {
        g->state = STATE_PAUSED;
        return;
    }

    g->launcherPulse += dt * 3.0f;
    UpdateParticles(g, dt);

    if (g->comboDisplayTimer > 0.0f) g->comboDisplayTimer -= dt;

    if (IsKeyPressed(KEY_M)) g->soundEnabled = !g->soundEnabled;

    if (!g->roundActive) {
        UpdateAim(g);
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            Vector2 mouse = GetMousePosition();
            if (mouse.y < LAUNCHER_Y - 12.0f) {
                g->roundActive       = true;
                g->ballsToFire       = g->ballCount;
                g->activeBallCount   = 0;
                g->returnedBallCount = 0;
                g->firstReturned     = false;
                g->launchTimer       = 0.0f;
                g->roundCombo        = 0;
                for (int i = 0; i < BALL_POOL_SIZE; i++) { g->balls[i].active = false; g->balls[i].returned = false; }
            }
        }
    } else {
        UpdateFiring(g, dt);
        UpdateBallPhysics(g, dt);
        if (g->ballsToFire == 0 && g->activeBallCount == 0) CheckRoundEnd(g);
    }
}

// ── Update: Level Up ──────────────────────────────────────────────────────────
void UpdateLevelUp(Game *g, float dt) {
    g->levelUpTimer += dt;
    UpdateParticles(g, dt);
    if (IsKeyPressed(KEY_M)) g->soundEnabled = !g->soundEnabled;
    if (g->levelUpTimer >= 2.2f || IsKeyPressed(KEY_ENTER) || IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        for (int i = 0; i < BALL_POOL_SIZE; i++) { g->balls[i].active = false; g->balls[i].returned = false; }
        g->activeBallCount = g->returnedBallCount = 0;
        g->firstReturned   = false;
        g->roundActive     = false;
        g->state           = STATE_PLAYING;
    }
}

// ── Update: Game Over ─────────────────────────────────────────────────────────
void UpdateGameOver(Game *g) {
    g->animTimer += GetFrameTime();
    if (IsKeyPressed(KEY_M)) g->soundEnabled = !g->soundEnabled;
    if (IsKeyPressed(KEY_ENTER)) {
        if (IsNewHighscore(&g->highscores, g->score)) {
            g->inputLen = 0;
            memset(g->inputName, 0, NAME_LEN);
            g->state = STATE_HIGHSCORE_ENTRY;
        } else {
            ResetGame(g, 1);
        }
    }
    if (IsKeyPressed(KEY_Q)) g->state = STATE_MENU;
}

// ── Update: Highscore Entry ───────────────────────────────────────────────────
void UpdateHighscoreEntry(Game *g) {
    if (IsKeyPressed(KEY_M)) g->soundEnabled = !g->soundEnabled;
    int key = GetCharPressed();
    while (key > 0) {
        if (((key >= 'a' && key <= 'z') || (key >= 'A' && key <= 'Z') || (key >= '0' && key <= '9'))
            && g->inputLen < NAME_LEN - 1) {
            g->inputName[g->inputLen++] = (char)key;
            g->inputName[g->inputLen]   = '\0';
        }
        key = GetCharPressed();
    }
    if (IsKeyPressed(KEY_BACKSPACE) && g->inputLen > 0) g->inputName[--g->inputLen] = '\0';
    if (IsKeyPressed(KEY_ENTER) && g->inputLen > 0) {
        InsertHighscore(&g->highscores, g->inputName, g->score);
        WriteSaveData(g);
        g->state = STATE_HIGHSCORE_VIEW;
    }
}

// ── Update: Highscore View ────────────────────────────────────────────────────
void UpdateHighscoreView(Game *g) {
    if (IsKeyPressed(KEY_M)) g->soundEnabled = !g->soundEnabled;
    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_ENTER)) g->state = STATE_MENU;
}

// ── Draw: Background ──────────────────────────────────────────────────────────
void DrawBackground(const Game *g) {
    // Deep gradient (32 strips for smoothness)
    int strips = 32;
    for (int i = 0; i < strips; i++) {
        float t0 = (float)i / strips, t1 = (float)(i + 1) / strips;
        Color c0 = BlendColor(g->theme.bgA, g->theme.bgB, t0);
        Color c1 = BlendColor(g->theme.bgA, g->theme.bgB, t1);
        int y0 = (int)(t0 * SCREEN_H), y1 = (int)(t1 * SCREEN_H);
        DrawRectangleGradientV(0, y0, SCREEN_W, y1 - y0, c0, c1);
    }

    // Stars
    DrawStars(g);

    // Nebula blobs (large soft circles)
    float t = (float)GetTime();
    Color nebA = g->theme.accentA; nebA.a = 12;
    Color nebB = g->theme.accentB; nebB.a = 8;
    DrawCircleV((Vector2){70.0f + sinf(t * 0.15f) * 20.0f, 200.0f}, 140.0f, nebA);
    DrawCircleV((Vector2){410.0f + cosf(t * 0.12f) * 20.0f, 480.0f}, 120.0f, nebB);
    DrawCircleV((Vector2){240.0f, 350.0f + sinf(t * 0.10f) * 15.0f}, 100.0f, nebA);

    // Vignette
    DrawRectangleGradientH(0, 0, 70, SCREEN_H, (Color){0,0,0,90}, (Color){0,0,0,0});
    DrawRectangleGradientH(SCREEN_W-70, 0, 70, SCREEN_H, (Color){0,0,0,0}, (Color){0,0,0,90});
    DrawRectangleGradientV(0, 0, SCREEN_W, 60, (Color){0,0,0,60}, (Color){0,0,0,0});
    DrawRectangleGradientV(0, SCREEN_H-60, SCREEN_W, 60, (Color){0,0,0,0}, (Color){0,0,0,80});

    // Subtle scanlines
    for (int y = 0; y < SCREEN_H; y += 4)
        DrawRectangle(0, y, SCREEN_W, 1, (Color){0,0,0,10});
}

// ── Draw: Menu Button ─────────────────────────────────────────────────────────
void DrawMenuButton(const char *label, Rectangle r, bool hovered, bool primary, Color accent) {
    // Background
    Color bg = hovered
        ? (Color){(unsigned char)(accent.r/4), (unsigned char)(accent.g/4), (unsigned char)(accent.b/4), 210}
        : (Color){12, 16, 40, 190};
    DrawRectangleRounded(r, 0.28f, 8, bg);

    // Glow rings on hover
    if (hovered) {
        DrawRectangleRoundedLines((Rectangle){r.x-2, r.y-2, r.width+4, r.height+4}, 0.28f, 8, Fade(accent, 0.25f));
        DrawRectangleRoundedLines((Rectangle){r.x-5, r.y-5, r.width+10, r.height+10}, 0.28f, 8, Fade(accent, 0.08f));
    }

    // Border
    DrawRectangleRoundedLines(r, 0.28f, 8, Fade(accent, hovered ? 0.95f : 0.35f));

    // Highlight strip at top (glass shimmer)
    DrawRectangleRounded((Rectangle){r.x+3, r.y+3, r.width-6, r.height*0.3f}, 0.28f, 6, Fade(WHITE, 0.06f));

    // Label
    int fs = primary ? 23 : 20;
    int tw = MeasureText(label, fs);
    int tx = (int)(r.x + r.width/2 - tw/2);
    int ty = (int)(r.y + r.height/2 - fs/2);
    DrawText(label, tx+1, ty+1, fs, Fade(BLACK, 0.5f));
    DrawText(label, tx, ty, fs, hovered ? WHITE : Fade(WHITE, 0.82f));
}

// ── Draw: HUD ─────────────────────────────────────────────────────────────────
void DrawHUD(const Game *g) {
    // Glass top bar
    DrawRectangleGradientV(0, 0, SCREEN_W, (int)HUD_HEIGHT, (Color){0,0,0,170}, (Color){0,0,0,80});
    DrawLine(0, (int)HUD_HEIGHT, SCREEN_W, (int)HUD_HEIGHT, (Color){255,255,255,20});
    // Subtle bottom accent line
    Color ac = g->theme.accentA; ac.a = 60;
    DrawLine(0, (int)HUD_HEIGHT, SCREEN_W, (int)HUD_HEIGHT, ac);

    char buf[64];
    int globalRound = (g->level - 1) * ROUNDS_PER_LEVEL + g->round - 1;
    sprintf(buf, "LV%d  R%d", g->level, globalRound > 0 ? globalRound : 1);
    DrawText(buf, 10, 15, 18, (Color){180,200,240,255});

    sprintf(buf, "%d", g->score);
    int sw = MeasureText(buf, 24);
    DrawText(buf, (SCREEN_W-sw)/2 + 1, 13, 24, Fade(BLACK, 0.5f));
    DrawText(buf, (SCREEN_W-sw)/2, 12, 24, WHITE);

    // Ball count with mini circles
    sprintf(buf, "x%d", g->ballCount);
    int bw = MeasureText(buf, 18);
    DrawText(buf, SCREEN_W - bw - 10, 15, 18, WHITE);
    int show = g->ballCount < 5 ? g->ballCount : 5;
    for (int i = 0; i < show; i++) {
        Vector2 bp = {(float)(SCREEN_W - bw - 24 - i * 13), 24.0f};
        DrawCircleV(bp, 4.5f, Fade(g->theme.ballGlow, 0.85f));
        DrawCircleV(bp, 2.5f, WHITE);
    }

    const char *sm = g->soundEnabled ? "[M]" : "[M]OFF";
    DrawText(sm, 4, SCREEN_H - 18, 12, (Color){110,110,140,180});
}

// ── Draw: Bricks ──────────────────────────────────────────────────────────────
void DrawBricks(const Game *g) {
    for (int i = 0; i < MAX_BRICKS; i++) {
        const Brick *b = &g->bricks[i];
        if (!b->active) continue;

        float shk = b->shakeX * sinf((float)GetTime() * 80.0f);
        Rectangle base = {
            GRID_OFFSET_X + b->col * CELL_SIZE + 4 + shk,
            GRID_OFFSET_Y + b->row * CELL_SIZE + 4,
            CELL_SIZE - 8, CELL_SIZE - 8
        };

        Color col = (b->flashTimer > 0.0f) ? WHITE : b->color;

        // Drop shadow
        DrawRectangleRounded((Rectangle){base.x+3, base.y+4, base.width, base.height},
                             0.22f, 6, (Color){0,0,0,100});
        // Main body
        DrawRectangleRounded(base, 0.22f, 6, col);

        // Inner gradient: slightly lighter top half
        Color topTint = col;
        topTint.r = (unsigned char)fminf(255, col.r + 40);
        topTint.g = (unsigned char)fminf(255, col.g + 40);
        topTint.b = (unsigned char)fminf(255, col.b + 40);
        DrawRectangleGradientV(
            (int)(base.x + 2), (int)(base.y + 2),
            (int)(base.width - 4), (int)((base.height - 4) * 0.5f),
            Fade(topTint, 0.35f), Fade(topTint, 0.0f));

        // Glass highlight (top-left strip)
        DrawRectangleRounded(
            (Rectangle){base.x+2, base.y+2, base.width-4, (base.height-4)*0.30f},
            0.22f, 4, Fade(WHITE, 0.20f));

        // Glowing border
        Color borderGlow = col; borderGlow.a = (b->flashTimer > 0.0f) ? 255 : 180;
        DrawRectangleRoundedLines(base, 0.22f, 6, Fade(borderGlow, 0.7f));

        // HP bar
        if (b->maxHp > 1) {
            float ratio = (float)b->hp / b->maxHp;
            float barW  = (base.width - 8) * ratio;
            DrawRectangle((int)(base.x+4), (int)(base.y + base.height - 6), (int)(base.width-8), 3, (Color){0,0,0,120});
            Color barCol = (ratio > 0.5f) ? (Color){80,255,100,220} : (ratio > 0.25f) ? (Color){255,200,40,220} : (Color){255,70,70,220};
            DrawRectangle((int)(base.x+4), (int)(base.y + base.height - 6), (int)barW, 3, barCol);
        }

        // HP text
        char hpBuf[16];
        snprintf(hpBuf, sizeof(hpBuf), "%d", b->hp);
        int fs = (b->hp >= 10000) ? 11 : (b->hp >= 1000) ? 13 : (b->hp >= 100) ? 15 : (b->hp >= 10) ? 18 : 22;
        int tw = MeasureText(hpBuf, fs);
        int tx = (int)(base.x + base.width/2 - tw/2);
        int ty = (int)(base.y + base.height/2 - fs/2 - 2);
        DrawText(hpBuf, tx+1, ty+1, fs, (Color){0,0,0,180});
        DrawText(hpBuf, tx, ty, fs, WHITE);

        // Explosive indicator — pulsing orange border + corner "!"
        if (b->explosive) {
            float pulse = 0.5f + 0.5f * sinf((float)GetTime() * 6.0f + b->col * 1.3f);
            Color expCol = (Color){255, 110, 20, 255};
            DrawRectangleRoundedLines(
                (Rectangle){base.x-2, base.y-2, base.width+4, base.height+4},
                0.22f, 6, Fade(expCol, 0.45f + pulse * 0.40f));
            DrawRectangleRoundedLines(
                (Rectangle){base.x-5, base.y-5, base.width+10, base.height+10},
                0.22f, 6, Fade(expCol, 0.12f + pulse * 0.12f));
            DrawText("!", (int)(base.x + base.width - 11), (int)(base.y + 3), 13,
                     Fade(expCol, 0.85f + pulse * 0.15f));
        }
    }
}

// ── Draw: Pickups ─────────────────────────────────────────────────────────────
void DrawPickups(const Game *g) {
    for (int i = 0; i < MAX_PICKUPS; i++) {
        const BallPickup *p = &g->pickups[i];
        if (!p->active) continue;
        Vector2 center = GetCellCenter(p->col, p->row);
        float pulse = sinf((float)GetTime() * 4.5f + p->pulseT);
        float r     = CELL_SIZE * 0.20f + pulse * 2.5f;

        // Glow layers
        DrawCircleV(center, r + 14, Fade((Color){255,220,30,255}, 0.04f));
        DrawCircleV(center, r + 9,  Fade((Color){255,220,30,255}, 0.08f));
        DrawCircleV(center, r + 5,  Fade((Color){255,220,30,255}, 0.16f));
        // Orb body
        DrawCircleV(center, r, (Color){255,220,30,255});
        // Shine gradient
        DrawCircleV((Vector2){center.x - r*0.25f, center.y - r*0.25f}, r*0.35f, Fade(WHITE, 0.55f));
        // "+1" label
        const char *plus = "+1";
        int tw = MeasureText(plus, 14);
        DrawText(plus, (int)center.x - tw/2 + 1, (int)center.y - 7 + 1, 14, Fade(BLACK, 0.5f));
        DrawText(plus, (int)center.x - tw/2, (int)center.y - 7, 14, (Color){30,20,0,255});
    }
}

// ── Draw: Trajectory ──────────────────────────────────────────────────────────
void DrawTrajectory(const Game *g) {
    if (g->roundActive) return;
    Vector2 pos = {g->launcherX, LAUNCHER_Y - BALL_RADIUS - 2.0f};
    Vector2 dir = g->aimDir;
    float leftW  = GRID_OFFSET_X + 1.0f;
    float rightW = GRID_OFFSET_X + GRID_COLS * CELL_SIZE - 1.0f;
    float topW   = GRID_OFFSET_Y;
    int maxDots  = 50;
    float step   = 11.0f;

    for (int i = 0; i < maxDots; i++) {
        pos.x += dir.x * step;
        pos.y += dir.y * step;
        if (pos.y <= topW) break;
        if (pos.x <= leftW)  { pos.x = leftW;  dir.x =  fabsf(dir.x); }
        if (pos.x >= rightW) { pos.x = rightW; dir.x = -fabsf(dir.x); }

        float t     = 1.0f - (float)i / maxDots;
        float alpha = t * t * 0.9f;
        float dotR  = 2.8f + t * 1.8f;
        DrawCircleV(pos, dotR + 2, Fade(g->theme.ballGlow, alpha * 0.25f));
        DrawCircleV(pos, dotR, Fade(g->theme.ballGlow, alpha));
    }
}

// ── Draw: Balls ───────────────────────────────────────────────────────────────
void DrawBalls(const Game *g) {
    for (int bi = 0; bi < BALL_POOL_SIZE; bi++) {
        const Ball *b = &g->balls[bi];
        if (!b->active) continue;

        // Trail
        for (int t = 0; t < b->trail.count; t++) {
            int idx  = (b->trail.head - 1 - t + TRAIL_LEN) % TRAIL_LEN;
            float tf = 1.0f - (float)(t + 1) / (TRAIL_LEN + 1);
            float tr = BALL_RADIUS * tf * 0.70f;
            Color tc = g->theme.ballGlow;
            tc.a = (unsigned char)(tf * tf * 130.0f);
            DrawCircleV(b->trail.pos[idx], tr, tc);
        }

        // Glow halo
        Color go = g->theme.ballGlow; go.a = 28;
        Color gi = g->theme.ballGlow; gi.a = 55;
        DrawCircleV(b->position, BALL_RADIUS + 7.0f, go);
        DrawCircleV(b->position, BALL_RADIUS + 3.5f, gi);
        // Ball body
        DrawCircleV(b->position, BALL_RADIUS, WHITE);
        // Glint
        DrawCircleV((Vector2){b->position.x - 2.8f, b->position.y - 2.8f}, 3.0f, Fade(WHITE, 0.80f));
        DrawCircleV((Vector2){b->position.x - 2.0f, b->position.y - 2.0f}, 1.4f, WHITE);
    }
}

// ── Draw: Launcher ────────────────────────────────────────────────────────────
void DrawLauncher(const Game *g) {
    float px = g->launcherX, py = LAUNCHER_Y;
    float pulse = sinf(g->launcherPulse) * 0.5f + 0.5f;
    Color rc = g->theme.ballGlow;

    // Outer rings
    DrawCircleLines((int)px, (int)py, 22.0f + pulse * 8.0f, Fade(rc, 0.10f + pulse * 0.08f));
    DrawCircleLines((int)px, (int)py, 16.0f, Fade(rc, 0.22f));
    DrawCircleLines((int)px, (int)py, 12.0f, Fade(rc, 0.14f));

    // Aim arrow
    if (!g->roundActive) {
        Vector2 tip  = {px + g->aimDir.x * 26.0f, py + g->aimDir.y * 26.0f};
        Vector2 perp = {-g->aimDir.y, g->aimDir.x};
        Vector2 lft  = {px + perp.x * 8.0f, py + perp.y * 8.0f};
        Vector2 rgt  = {px - perp.x * 8.0f, py - perp.y * 8.0f};
        DrawTriangle(tip, lft, rgt, rc);
        DrawTriangleLines(tip, lft, rgt, Fade(WHITE, 0.45f));
    }

    // Core circle
    DrawCircleV((Vector2){px, py}, 8.0f, WHITE);
    DrawCircleV((Vector2){px, py}, 5.0f, rc);
    DrawCircleV((Vector2){px - 2.0f, py - 2.0f}, 1.8f, WHITE);

    // Ball count below launcher
    char cntBuf[16];
    sprintf(cntBuf, "x%d", g->ballCount);
    int tw = MeasureText(cntBuf, 15);
    DrawText(cntBuf, (int)(px - tw/2), (int)(py + 13), 15, Fade(WHITE, 0.70f));
}

// ── Draw: Particles ───────────────────────────────────────────────────────────
void DrawParticles(const Game *g) {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        const Particle *p = &g->particles[i];
        if (p->life <= 0.0f) continue;
        float t = p->life / p->maxLife;
        Color c = p->color;

        if (p->size >= 16) {
            // Menu orb: large translucent circle
            c.a = (unsigned char)(t * (1.0f - t) * 4.0f * 60.0f);
            DrawCircleV(p->pos, p->size * (0.5f + t * 0.5f), c);
        } else {
            // Brick particle: rotated square
            c.a = (unsigned char)(t * 220.0f);
            float sz = p->size * (0.3f + t * 0.7f);
            DrawRectanglePro(
                (Rectangle){p->pos.x, p->pos.y, sz, sz},
                (Vector2){sz * 0.5f, sz * 0.5f},
                p->rot, c
            );
        }
    }
}

// ── Draw: Playing ─────────────────────────────────────────────────────────────
void DrawPlaying(const Game *g) {
    DrawBackground(g);

    // Grid lines
    for (int c = 0; c <= GRID_COLS; c++)
        DrawLine((int)(GRID_OFFSET_X + c * CELL_SIZE), (int)GRID_OFFSET_Y,
                 (int)(GRID_OFFSET_X + c * CELL_SIZE), (int)(GRID_OFFSET_Y + GRID_ROWS * CELL_SIZE),
                 g->theme.gridLine);
    for (int r = 0; r <= GRID_ROWS; r++)
        DrawLine((int)GRID_OFFSET_X, (int)(GRID_OFFSET_Y + r * CELL_SIZE),
                 (int)(GRID_OFFSET_X + GRID_COLS * CELL_SIZE), (int)(GRID_OFFSET_Y + r * CELL_SIZE),
                 g->theme.gridLine);

    // Danger line
    float dp = 0.5f + 0.5f * sinf((float)GetTime() * 3.0f);
    DrawLine((int)GRID_OFFSET_X, (int)(GRID_OFFSET_Y + GRID_ROWS * CELL_SIZE),
             (int)(GRID_OFFSET_X + GRID_COLS * CELL_SIZE), (int)(GRID_OFFSET_Y + GRID_ROWS * CELL_SIZE),
             Fade(g->theme.dangerLine, dp));
    // Double danger line glow
    DrawLine((int)GRID_OFFSET_X, (int)(GRID_OFFSET_Y + GRID_ROWS * CELL_SIZE) + 1,
             (int)(GRID_OFFSET_X + GRID_COLS * CELL_SIZE), (int)(GRID_OFFSET_Y + GRID_ROWS * CELL_SIZE) + 1,
             Fade(g->theme.dangerLine, dp * 0.4f));

    DrawParticles(g);
    DrawBricks(g);
    DrawPickups(g);
    DrawTrajectory(g);
    DrawBalls(g);
    DrawLauncher(g);
    DrawHUD(g);

    // Round combo display
    if (g->comboDisplayTimer > 0.0f && g->roundCombo >= 3) {
        float alpha = fminf(g->comboDisplayTimer / 0.5f, 1.0f);
        char cBuf[32];
        if (g->roundCombo >= 10)
            sprintf(cBuf, "MEGA COMBO x%d!", g->roundCombo);
        else
            sprintf(cBuf, "COMBO x%d", g->roundCombo);
        int cw = MeasureText(cBuf, 26);
        Color cCol = ColorFromHSV(fmodf((float)GetTime() * 90.0f, 360.0f), 0.9f, 1.0f);
        DrawText(cBuf, (SCREEN_W - cw)/2 + 2, SCREEN_H/2 - 44, 26, Fade(BLACK, 0.65f * alpha));
        DrawText(cBuf, (SCREEN_W - cw)/2,     SCREEN_H/2 - 46, 26, Fade(cCol, alpha));
    }

    if (!g->roundActive) {
        float alpha = 0.35f + 0.35f * sinf((float)GetTime() * 2.8f);
        const char *hint = "CLICK TO SHOOT";
        int hw = MeasureText(hint, 14);
        DrawText(hint, (SCREEN_W - hw)/2, SCREEN_H - 28, 14, Fade(LIGHTGRAY, alpha));
    }
}

// ── Draw: Paused ──────────────────────────────────────────────────────────────
void DrawPaused(const Game *g) {
    (void)g;
    DrawRectangle(0, 0, SCREEN_W, SCREEN_H, Fade(BLACK, 0.58f));

    float hue = fmodf((float)GetTime() * 30.0f, 360.0f);
    const char *title = "PAUSED";
    int tw = MeasureText(title, 54);
    DrawText(title, (SCREEN_W - tw)/2 + 2, 270, 54, Fade(BLACK, 0.70f));
    DrawText(title, (SCREEN_W - tw)/2,     268, 54, ColorFromHSV(hue, 0.50f, 1.0f));

    struct { const char *text; int y; } items[] = {
        {"[P] / [ESC]  Resume",  356},
        {"[Q]  Back to Menu",    396},
        {"[M]  Toggle Sound",    436},
    };
    for (int i = 0; i < 3; i++) {
        int iw = MeasureText(items[i].text, 19);
        DrawText(items[i].text, (SCREEN_W - iw)/2, items[i].y, 19, (Color){180,200,240,255});
    }
}

// ── Draw: Menu ────────────────────────────────────────────────────────────────
void DrawMenu(const Game *g) {
    DrawBackground(g);
    DrawParticles(g);

    // Semi-transparent center panel
    DrawRectangleRounded((Rectangle){30, 55, SCREEN_W-60, SCREEN_H-110}, 0.04f, 8, (Color){0,0,15,55});
    DrawRectangleRoundedLines((Rectangle){30, 55, SCREEN_W-60, SCREEN_H-110}, 0.04f, 8, Fade(WHITE, 0.05f));

    // Animated title
    float hue = fmodf(g->menuAnimT * 42.0f, 360.0f);
    const char *t1 = "BRICK BREAKER";
    int tw1 = MeasureText(t1, 36);
    DrawText(t1, (SCREEN_W-tw1)/2 + 2, 102, 36, Fade(BLACK, 0.75f));
    DrawText(t1, (SCREEN_W-tw1)/2, 99, 36, ColorFromHSV(hue, 0.55f, 0.95f));

    const char *t2 = "HIT";
    int tw2 = MeasureText(t2, 72);
    // Text shadow + glow
    DrawText(t2, (SCREEN_W-tw2)/2 + 3, 143, 72, Fade(BLACK, 0.85f));
    Color titleGlow = ColorFromHSV(fmodf(hue + 30, 360), 0.90f, 1.0f);
    titleGlow.a = 40;
    DrawText(t2, (SCREEN_W-tw2)/2 - 1, 140, 72, titleGlow);
    titleGlow.a = 20;
    DrawText(t2, (SCREEN_W-tw2)/2 - 2, 140, 72, titleGlow);
    DrawText(t2, (SCREEN_W-tw2)/2, 140, 72, ColorFromHSV(fmodf(hue + 30, 360), 0.90f, 1.0f));

    // Tagline
    const char *tag = "Aim  *  Shoot  *  Destroy";
    int tagw = MeasureText(tag, 16);
    DrawText(tag, (SCREEN_W-tagw)/2, 225, 16, (Color){130,155,200,200});

    // Best score
    if (g->highscores.count > 0) {
        char bb[32];
        sprintf(bb, "BEST: %d", g->highscores.entries[0].score);
        int bw = MeasureText(bb, 17);
        DrawText(bb, (SCREEN_W-bw)/2 + 1, 249, 17, Fade(BLACK, 0.5f));
        DrawText(bb, (SCREEN_W-bw)/2, 248, 17, GOLD);
    }

    // Divider
    float lineAlpha = 0.10f + 0.05f * sinf(g->menuAnimT * 1.5f);
    DrawLineEx((Vector2){(float)(SCREEN_W/2 - 110), 278.0f}, (Vector2){(float)(SCREEN_W/2 + 110), 278.0f}, 1.5f, Fade(WHITE, lineAlpha));
    // Dot accent on divider
    DrawCircleV((Vector2){(float)(SCREEN_W/2), 278.0f}, 3.0f, Fade(g->theme.accentA, 0.5f));

    // Buttons
    Color accentPlay   = (Color){60, 210, 255, 255};
    Color accentLvSel  = (Color){100, 130, 230, 255};
    Color accentScores = (Color){100, 130, 230, 255};
    Color accentQuit   = (Color){100, 100, 140, 255};
    Color accents[]    = {accentPlay, accentLvSel, accentScores, accentQuit};
    bool  primaries[]  = {true, false, false, false};
    const char *labels[] = {"PLAY", "SELECT LEVEL", "HIGH SCORES", "QUIT"};

    for (int i = 0; i < 4; i++) {
        Rectangle r = GetMenuBtnRect(i);
        DrawMenuButton(labels[i], r, g->menuHover == i, primaries[i], accents[i]);
    }

    // Sound indicator
    char sndBuf[32];
    sprintf(sndBuf, "[M] SOUND: %s", g->soundEnabled ? "ON" : "OFF");
    Color sndCol = g->soundEnabled ? (Color){100,245,120,200} : (Color){220,80,80,180};
    int sndW = MeasureText(sndBuf, 13);
    DrawText(sndBuf, (SCREEN_W - sndW)/2, SCREEN_H - 48, 13, sndCol);

    // Hint
    const char *hint = "[L] Level Select   [H] Scores   [ESC] Quit";
    int hw = MeasureText(hint, 11);
    DrawText(hint, (SCREEN_W - hw)/2, SCREEN_H - 28, 11, Fade(WHITE, 0.18f));
}

// ── Draw: Level Select ────────────────────────────────────────────────────────
void DrawLevelSelect(const Game *g) {
    DrawBackground(g);
    DrawRectangle(0, 0, SCREEN_W, SCREEN_H, Fade(BLACK, 0.35f));

    // Title
    float hue = fmodf(g->menuAnimT * 40.0f, 360.0f);
    const char *title = "SELECT LEVEL";
    int tw = MeasureText(title, 36);
    DrawText(title, (SCREEN_W - tw)/2 + 2, 62, 36, Fade(BLACK, 0.7f));
    DrawText(title, (SCREEN_W - tw)/2, 60, 36, ColorFromHSV(hue, 0.6f, 1.0f));

    const char *sub = "Complete levels to unlock the next";
    int sw = MeasureText(sub, 14);
    DrawText(sub, (SCREEN_W - sw)/2, 104, 14, (Color){140, 160, 200, 200});

    DrawLineEx((Vector2){(float)(SCREEN_W/2 - 120), 128.0f}, (Vector2){(float)(SCREEN_W/2 + 120), 128.0f},
               1.5f, Fade(WHITE, 0.12f));

    // Level tiles
    for (int lv = 1; lv <= MAX_SELECTABLE_LEVELS; lv++) {
        Rectangle tile = GetLevelTileRect(lv);
        bool locked  = (lv > g->unlockedLevels);
        bool hovered = (g->lvSelHover == lv);
        Theme th     = THEMES[lv - 1]; // exact 1:1 since NUM_THEMES=12

        // Tile background
        Color tileBg;
        if (locked) {
            tileBg = (Color){10, 12, 28, 210};
        } else if (hovered) {
            tileBg = (Color){
                (unsigned char)(th.accentA.r / 3),
                (unsigned char)(th.accentA.g / 3),
                (unsigned char)(th.accentA.b / 3), 230
            };
        } else {
            tileBg = (Color){
                (unsigned char)fminf(255, th.bgA.r + 15),
                (unsigned char)fminf(255, th.bgA.g + 15),
                (unsigned char)fminf(255, th.bgA.b + 15), 210
            };
        }
        DrawRectangleRounded(tile, 0.18f, 6, tileBg);

        // Border
        Color borderCol = locked
            ? Fade(WHITE, 0.08f)
            : Fade(th.accentA, hovered ? 0.90f : 0.40f);
        DrawRectangleRoundedLines(tile, 0.18f, 6, borderCol);

        // Glow on hover
        if (!locked && hovered) {
            DrawRectangleRoundedLines(
                (Rectangle){tile.x-3, tile.y-3, tile.width+6, tile.height+6},
                0.18f, 6, Fade(th.accentA, 0.22f));
            DrawRectangleRoundedLines(
                (Rectangle){tile.x-6, tile.y-6, tile.width+12, tile.height+12},
                0.18f, 6, Fade(th.accentA, 0.08f));
        }

        // Shimmer top strip
        if (!locked)
            DrawRectangleRounded(
                (Rectangle){tile.x+2, tile.y+2, tile.width-4, (tile.height-4)*0.28f},
                0.18f, 4, Fade(WHITE, 0.10f));

        float cx = tile.x + tile.width / 2.0f;
        float cy = tile.y + tile.height / 2.0f;

        if (locked) {
            // Simple lock icon
            float lbx = cx - 9, lby = cy - 2;
            DrawRectangleRounded((Rectangle){lbx, lby, 18, 14}, 0.2f, 4, Fade(WHITE, 0.12f));
            DrawRectangleRoundedLines((Rectangle){lbx, lby, 18, 14}, 0.2f, 4, Fade(WHITE, 0.18f));
            // Shackle
            DrawCircleLines((int)cx, (int)(lby), 8, Fade(WHITE, 0.15f));
            DrawRectangle((int)(cx - 8), (int)(lby - 1), 16, 5, tileBg); // clip bottom of circle

            const char *lk = "LOCK";
            int lkw = MeasureText(lk, 10);
            DrawText(lk, (int)(cx - lkw/2), (int)(tile.y + tile.height - 18), 10, Fade(WHITE, 0.22f));
        } else {
            // Level number
            char lvBuf[8];
            sprintf(lvBuf, "%d", lv);
            int fs = 30;
            int ltw = MeasureText(lvBuf, fs);
            Color lvCol = hovered ? WHITE : th.accentA;
            DrawText(lvBuf, (int)(cx - ltw/2) + 1, (int)(cy - fs/2 - 3) + 1, fs, Fade(BLACK, 0.5f));
            DrawText(lvBuf, (int)(cx - ltw/2), (int)(cy - fs/2 - 3), fs, lvCol);

            // "LEVEL" label above
            const char *lvl = "LEVEL";
            int llw = MeasureText(lvl, 10);
            DrawText(lvl, (int)(cx - llw/2), (int)(tile.y + 8), 10, Fade(lvCol, 0.55f));
        }
    }

    // Back button
    float blink = 0.5f + 0.5f * sinf((float)GetTime() * 2.2f);
    const char *back = "[ESC]  Back to Menu";
    int bw = MeasureText(back, 16);
    DrawText(back, (SCREEN_W - bw)/2, SCREEN_H - 46, 16, Fade(LIGHTGRAY, 0.4f + blink * 0.3f));

    // Unlock progress
    char progBuf[48];
    sprintf(progBuf, "Unlocked: %d / %d", g->unlockedLevels, MAX_SELECTABLE_LEVELS);
    int pgw = MeasureText(progBuf, 13);
    DrawText(progBuf, (SCREEN_W - pgw)/2, SCREEN_H - 24, 13, Fade(WHITE, 0.28f));
}

// ── Draw: Level Up ────────────────────────────────────────────────────────────
void DrawLevelUp(const Game *g) {
    DrawBackground(g);
    DrawBricks(g);
    DrawParticles(g);
    DrawHUD(g);
    DrawRectangle(0, 0, SCREEN_W, SCREEN_H, Fade(BLACK, 0.60f));

    float t  = fminf(g->levelUpTimer / 0.45f, 1.0f);
    float alpha = t;
    float hue = fmodf((float)GetTime() * 55.0f, 360.0f);

    // Flash ring
    if (g->levelUpTimer < 0.35f) {
        float ringT = 1.0f - g->levelUpTimer / 0.35f;
        DrawCircleLines(SCREEN_W/2, SCREEN_H/2, 60.0f + (1.0f - ringT) * 160.0f, Fade(WHITE, ringT * 0.3f));
    }

    char lvBuf[32];
    sprintf(lvBuf, "LEVEL %d", g->level);
    int fs = (int)(54 * (0.7f + t * 0.3f));
    int tw = MeasureText(lvBuf, fs);
    Color lvColor = ColorFromHSV(hue, 0.85f, 1.0f);

    // Shadow + glow
    DrawText(lvBuf, (SCREEN_W - tw)/2 + 3, 258, fs, Fade(BLACK, 0.8f));
    lvColor.a = 35;
    DrawText(lvBuf, (SCREEN_W - tw)/2 - 2, 256, fs, lvColor);
    lvColor.a = 255;
    DrawText(lvBuf, (SCREEN_W - tw)/2, 256, fs, Fade(lvColor, alpha));

    const char *sub = "KEEP GOING!";
    int sbw = MeasureText(sub, 22);
    DrawText(sub, (SCREEN_W - sbw)/2, 330, 22, Fade(WHITE, alpha * 0.85f));

    char scoreBuf[32];
    sprintf(scoreBuf, "Score: %d", g->score);
    int scw = MeasureText(scoreBuf, 19);
    DrawText(scoreBuf, (SCREEN_W - scw)/2, 366, 19, Fade(LIGHTGRAY, alpha * 0.9f));

    if (g->levelUpTimer > 0.8f) {
        float bl = 0.5f + 0.5f * sinf((float)GetTime() * 4.2f);
        const char *tap = "TAP TO CONTINUE";
        int tpw = MeasureText(tap, 16);
        DrawText(tap, (SCREEN_W - tpw)/2, 428, 16, Fade(WHITE, bl));
    }
}

// ── Draw: Game Over ───────────────────────────────────────────────────────────
void DrawGameOver(const Game *g) {
    DrawBackground(g);
    DrawRectangle(0, 0, SCREEN_W, SCREEN_H, Fade(BLACK, 0.75f));

    float t = fminf(g->animTimer / 0.5f, 1.0f);
    const char *go = "GAME OVER";
    int gfs = (int)(60 * (0.55f + t * 0.45f));
    int gw  = MeasureText(go, gfs);
    DrawText(go, (SCREEN_W - gw)/2 + 3, 188, gfs, Fade((Color){180,0,0,255}, t));
    DrawText(go, (SCREEN_W - gw)/2, 185, gfs, Fade(RED, t));

    char buf[64];
    int globalRound = (g->level - 1) * ROUNDS_PER_LEVEL + g->round - 1;
    sprintf(buf, "Level %d  —  Round %d", g->level, globalRound > 0 ? globalRound : 1);
    int bw = MeasureText(buf, 18);
    DrawText(buf, (SCREEN_W - bw)/2, 270, 18, Fade(LIGHTGRAY, t));

    sprintf(buf, "%d", g->score);
    int sw = MeasureText(buf, 50);
    DrawText(buf, (SCREEN_W - sw)/2 + 2, 298, 50, Fade(BLACK, t * 0.6f));
    DrawText(buf, (SCREEN_W - sw)/2, 295, 50, Fade(WHITE, t));

    if (t >= 1.0f) {
        if (IsNewHighscore(&g->highscores, g->score)) {
            const char *hs = "NEW HIGH SCORE!";
            int hw = MeasureText(hs, 24);
            DrawText(hs, (SCREEN_W - hw)/2, 366, 24, GOLD);
            float bl = 0.5f + 0.5f * sinf((float)GetTime() * 3.5f);
            const char *enter = "[ENTER]  Enter Name";
            int ew = MeasureText(enter, 19);
            DrawText(enter, (SCREEN_W - ew)/2, 405, 19, Fade(YELLOW, bl));
        } else {
            const char *ops = "[ENTER] Play Again   [Q] Menu";
            int ow = MeasureText(ops, 18);
            DrawText(ops, (SCREEN_W - ow)/2, 372, 18, Fade(LIGHTGRAY, 0.85f));
        }
    }
}

// ── Draw: Highscore Entry ─────────────────────────────────────────────────────
void DrawHighscoreEntry(const Game *g) {
    DrawBackground(g);
    DrawRectangle(0, 0, SCREEN_W, SCREEN_H, Fade(BLACK, 0.82f));

    const char *hdr = "NEW HIGH SCORE!";
    int hw = MeasureText(hdr, 34);
    DrawText(hdr, (SCREEN_W - hw)/2, 192, 34, GOLD);

    char scoreBuf[32];
    sprintf(scoreBuf, "%d", g->score);
    int sw = MeasureText(scoreBuf, 42);
    DrawText(scoreBuf, (SCREEN_W - sw)/2, 238, 42, WHITE);

    const char *prompt = "Enter your name:";
    int pw = MeasureText(prompt, 20);
    DrawText(prompt, (SCREEN_W - pw)/2, 300, 20, (Color){170,185,215,255});

    DrawRectangleRounded((Rectangle){55, 328, SCREEN_W-110, 48}, 0.3f, 6, (Color){255,255,255,18});
    DrawRectangleRoundedLines((Rectangle){55, 328, SCREEN_W-110, 48}, 0.3f, 6, Fade(WHITE, 0.30f));

    char display[NAME_LEN + 2];
    strncpy(display, g->inputName, NAME_LEN);
    display[g->inputLen] = '\0';
    if (g->inputLen < NAME_LEN - 1 && (int)(GetTime() * 2) % 2 == 0) {
        display[g->inputLen]     = '|';
        display[g->inputLen + 1] = '\0';
    }
    int dw = MeasureText(display, 30);
    DrawText(display, (SCREEN_W - dw)/2, 337, 30, WHITE);

    const char *hint = "[BACKSPACE] Delete    [ENTER] Confirm";
    int hiw = MeasureText(hint, 13);
    DrawText(hint, (SCREEN_W - hiw)/2, 390, 13, (Color){130,145,175,200});
}

// ── Draw: Highscore View ──────────────────────────────────────────────────────
void DrawHighscoreView(const Game *g) {
    DrawBackground(g);
    DrawRectangle(0, 0, SCREEN_W, SCREEN_H, Fade(BLACK, 0.82f));

    float hue = fmodf((float)GetTime() * 35.0f, 360.0f);
    const char *title = "TOP SCORES";
    int tw = MeasureText(title, 42);
    DrawText(title, (SCREEN_W - tw)/2 + 2, 92, 42, Fade(BLACK, 0.7f));
    DrawText(title, (SCREEN_W - tw)/2, 90, 42, ColorFromHSV(hue, 0.8f, 1.0f));

    DrawLineEx((Vector2){(float)(SCREEN_W/2 - 120), 142.0f},
               (Vector2){(float)(SCREEN_W/2 + 120), 142.0f}, 1.5f, Fade(WHITE, 0.15f));

    Color rankCol[] = { GOLD, LIGHTGRAY, (Color){205,127,50,255}, WHITE, WHITE };
    const char *medals[] = {"#1", "#2", "#3", "#4", "#5"};

    for (int i = 0; i < g->highscores.count && i < MAX_SCORES; i++) {
        int y = 158 + i * 54;
        // Row glow for top 3
        if (i < 3) {
            Color hl = rankCol[i]; hl.a = 22;
            DrawRectangleRounded((Rectangle){28, (float)y - 5, SCREEN_W-56, 44}, 0.25f, 4, hl);
            DrawRectangleRoundedLines((Rectangle){28, (float)y - 5, SCREEN_W-56, 44}, 0.25f, 4, Fade(rankCol[i], 0.15f));
        }
        DrawText(medals[i], 38, y + 7, 20, rankCol[i]);
        DrawText(g->highscores.entries[i].name, 80, y + 7, 20, rankCol[i]);
        char sc[32]; sprintf(sc, "%d", g->highscores.entries[i].score);
        int scw = MeasureText(sc, 20);
        DrawText(sc, SCREEN_W - 38 - scw, y + 7, 20, rankCol[i]);
    }

    if (g->highscores.count == 0) {
        const char *none = "No scores yet — play to set one!";
        int nw = MeasureText(none, 17);
        DrawText(none, (SCREEN_W - nw)/2, 230, 17, (Color){120,135,165,255});
    }

    DrawLineEx((Vector2){(float)(SCREEN_W/2 - 100), 446.0f},
               (Vector2){(float)(SCREEN_W/2 + 100), 446.0f}, 1.5f, Fade(WHITE, 0.10f));
    float blink = 0.5f + 0.5f * sinf((float)GetTime() * 2.5f);
    const char *back = "[ESC] / [ENTER]  Back";
    int bw = MeasureText(back, 16);
    DrawText(back, (SCREEN_W - bw)/2, 458, 16, Fade(LIGHTGRAY, blink));
}

// ── Main ──────────────────────────────────────────────────────────────────────
int main(void) {
    InitWindow(SCREEN_W, SCREEN_H, "Brick Breaker Hit");
    SetTargetFPS(60);
    InitAudioDevice();

    Game *g = (Game *)calloc(1, sizeof(Game));
    if (!g) { CloseWindow(); return 1; }
    InitGame(g);

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        switch (g->state) {
            case STATE_MENU:            UpdateMenu(g, dt);         break;
            case STATE_LEVEL_SELECT:    UpdateLevelSelect(g, dt);  break;
            case STATE_PLAYING:         UpdatePlaying(g, dt);      break;
            case STATE_PAUSED:          UpdatePaused(g);           break;
            case STATE_LEVEL_UP:        UpdateLevelUp(g, dt);      break;
            case STATE_GAME_OVER:       UpdateGameOver(g);         break;
            case STATE_HIGHSCORE_ENTRY: UpdateHighscoreEntry(g);   break;
            case STATE_HIGHSCORE_VIEW:  UpdateHighscoreView(g);    break;
        }
        BeginDrawing();
        ClearBackground((Color){8, 8, 20, 255});
        switch (g->state) {
            case STATE_MENU:            DrawMenu(g);                        break;
            case STATE_LEVEL_SELECT:    DrawLevelSelect(g);                 break;
            case STATE_PLAYING:         DrawPlaying(g);                     break;
            case STATE_PAUSED:          DrawPlaying(g); DrawPaused(g);      break;
            case STATE_LEVEL_UP:        DrawLevelUp(g);                     break;
            case STATE_GAME_OVER:       DrawGameOver(g);                    break;
            case STATE_HIGHSCORE_ENTRY: DrawHighscoreEntry(g);              break;
            case STATE_HIGHSCORE_VIEW:  DrawHighscoreView(g);               break;
        }
        EndDrawing();
    }

    WriteSaveData(g);
    if (g->audioReady) {
        UnloadSound(g->sndHit);    UnloadSound(g->sndDestroy);
        UnloadSound(g->sndPickup); UnloadSound(g->sndGameOver);
        UnloadSound(g->sndRoundEnd); UnloadSound(g->sndLevelUp);
        CloseAudioDevice();
    }
    free(g);
    CloseWindow();
    return 0;
}
