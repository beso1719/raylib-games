// Brick Breaker Hit — Enhanced Ballz-style shooter in C + raylib
#include "raylib.h"
#include <math.h>

// sincosf declared in UCRT math.h but not exported — provide definition
void sincosf(float x, float *s, float *c) { *s = sinf(x); *c = cosf(x); }
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------
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

#define DIFFICULTY_BASE       1.0f
#define ROUNDS_PER_LEVEL      10
#define MAX_LEVELS            999

#define TRAIL_LEN             10
#define MAX_PARTICLES         180
#define HUD_HEIGHT            48.0f

// ---------------------------------------------------------------------------
// Types
// ---------------------------------------------------------------------------
typedef enum {
    STATE_MENU,
    STATE_PLAYING,
    STATE_LEVEL_UP,
    STATE_GAME_OVER,
    STATE_HIGHSCORE_ENTRY,
    STATE_HIGHSCORE_VIEW,
} GameState;

typedef struct {
    Vector2 pos[TRAIL_LEN];
    int     head;
    int     count;
} Trail;

typedef struct {
    Vector2 position;
    Vector2 velocity;
    bool    active;
    bool    returned;
    Trail   trail;
} Ball;

typedef struct {
    int   hp;
    int   maxHp;
    bool  active;
    int   col;
    int   row;
    Color color;
    float flashTimer;
    float shakeX;       // small horizontal shake on hit
} Brick;

typedef struct {
    int   col;
    int   row;
    bool  active;
    float pulseT;       // per-pickup phase offset
} BallPickup;

typedef struct {
    Vector2 pos;
    Vector2 vel;
    Color   color;
    float   life;
    float   maxLife;
    float   size;
    float   rot;
    float   rotSpeed;
} Particle;

typedef struct {
    char name[NAME_LEN];
    int  score;
} ScoreEntry;

typedef struct {
    ScoreEntry entries[MAX_SCORES];
    int        count;
} Highscore;

// Level theme: background tint + grid accent color + ball glow color
typedef struct {
    Color bgA;       // gradient top
    Color bgB;       // gradient bottom
    Color gridLine;
    Color ballGlow;
    Color dangerLine;
} Theme;

typedef struct {
    // State
    GameState  state;
    int        round;       // 1-based round inside level
    int        level;       // 1-based level
    int        score;
    int        ballCount;
    int        ballsToFire;
    float      launchTimer;

    // Aim
    Vector2    aimDir;
    bool       roundActive;

    // Balls
    Ball       balls[BALL_POOL_SIZE];
    int        activeBallCount;
    int        returnedBallCount;
    float      firstReturnX;
    bool       firstReturned;

    // Grid
    Brick      bricks[MAX_BRICKS];
    BallPickup pickups[MAX_PICKUPS];

    // Particles
    Particle   particles[MAX_PARTICLES];

    // Sound
    Sound      sndHit;
    Sound      sndDestroy;
    Sound      sndPickup;
    Sound      sndGameOver;
    Sound      sndRoundEnd;
    Sound      sndLevelUp;
    bool       soundEnabled;
    bool       audioReady;

    // Highscore
    Highscore  highscores;
    char       inputName[NAME_LEN];
    int        inputLen;

    // Timers / animation
    float      animTimer;
    float      levelUpTimer;
    float      launcherX;
    float      launcherPulse; // launcher ring animation

    // Menu demo balls
    Vector2    demoBallPos[4];
    Vector2    demoBallVel[4];

    // Current theme
    Theme      theme;
    Theme      nextTheme;    // fade target on level up
    float      themeFadeT;   // 0..1
} Game;

// ---------------------------------------------------------------------------
// Level themes
// ---------------------------------------------------------------------------
static const Theme THEMES[] = {
    // 1 — Dark blue (default)
    {{10, 12, 35, 255}, {5, 8, 20, 255},   {60,80,160,20},  {80,160,255,255},  {200,50,50,200}},
    // 2 — Deep purple
    {{20, 8, 40, 255},  {10, 5, 25, 255},  {120,60,180,20}, {200,100,255,255}, {220,50,80,200}},
    // 3 — Dark teal
    {{5, 30, 35, 255},  {3, 18, 22, 255},  {40,160,140,20}, {60,230,200,255},  {255,80,50,200}},
    // 4 — Dark red
    {{35, 8, 8, 255},   {22, 5, 5, 255},   {180,50,40,20},  {255,120,60,255},  {255,60,60,200}},
    // 5 — Midnight green
    {{5, 35, 15, 255},  {3, 20, 8, 255},   {40,180,80,20},  {80,255,120,255},  {255,100,50,200}},
    // 6 — Gold/brown
    {{35, 25, 5, 255},  {22, 15, 3, 255},  {180,130,30,20}, {255,210,60,255},  {255,60,60,200}},
};
#define NUM_THEMES 6

// ---------------------------------------------------------------------------
// Forward declarations
// ---------------------------------------------------------------------------
void  InitGame(Game *g);
void  ResetGame(Game *g);
void  SpawnNewRow(Game *g);
void  LoadHighscores(Highscore *hs);
void  SaveHighscores(const Highscore *hs);
bool  IsNewHighscore(const Highscore *hs, int score);
void  InsertHighscore(Highscore *hs, const char *name, int score);
Sound GenerateBeep(float freq, float dur, float vol);
void  PlaySfx(const Game *g, Sound snd);
void  SpawnBrickParticles(Game *g, int col, int row, Color c);
void  UpdateParticles(Game *g, float dt);

void UpdateMenu(Game *g, float dt);
void UpdatePlaying(Game *g, float dt);
void UpdateLevelUp(Game *g, float dt);
void UpdateGameOver(Game *g);
void UpdateHighscoreEntry(Game *g);
void UpdateHighscoreView(Game *g);

void UpdateAim(Game *g);
void UpdateFiring(Game *g, float dt);
void UpdateBallPhysics(Game *g, float dt);
void CheckRoundEnd(Game *g);

void DrawBackground(const Game *g);
void DrawMenu(const Game *g);
void DrawPlaying(const Game *g);
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

Color     GetBrickColor(int hp, int level);
Rectangle GetCellRect(int col, int row);
Vector2   GetCellCenter(int col, int row);
Color     BlendColor(Color a, Color b, float t);
void      DrawGlowCircle(Vector2 center, float radius, Color inner, Color outer, int rings);
void      AddTrailPoint(Trail *t, Vector2 pos);
Theme     GetThemeForLevel(int level);

// ---------------------------------------------------------------------------
// Utilities
// ---------------------------------------------------------------------------
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

// Draw layered glow: multiple translucent circles expanding outward
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
        CELL_SIZE - 6,
        CELL_SIZE - 6
    };
}

Vector2 GetCellCenter(int col, int row) {
    return (Vector2){
        GRID_OFFSET_X + col * CELL_SIZE + CELL_SIZE * 0.5f,
        GRID_OFFSET_Y + row * CELL_SIZE + CELL_SIZE * 0.5f
    };
}

Theme GetThemeForLevel(int level) {
    return THEMES[(level - 1) % NUM_THEMES];
}

void AddTrailPoint(Trail *t, Vector2 pos) {
    t->pos[t->head] = pos;
    t->head = (t->head + 1) % TRAIL_LEN;
    if (t->count < TRAIL_LEN) t->count++;
}

// ---------------------------------------------------------------------------
// Sound
// ---------------------------------------------------------------------------
Sound GenerateBeep(float freq, float dur, float vol) {
    int sampleRate  = 44100;
    int sampleCount = (int)(sampleRate * dur);
    short *data     = (short *)malloc(sampleCount * sizeof(short));
    for (int i = 0; i < sampleCount; i++) {
        float t   = (float)i / sampleRate;
        float env = 1.0f - (t / dur);
        data[i]   = (short)(sinf(2.0f * PI * freq * t) * env * 32767.0f * vol);
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

// ---------------------------------------------------------------------------
// Particles
// ---------------------------------------------------------------------------
void SpawnBrickParticles(Game *g, int col, int row, Color c) {
    Vector2 center = GetCellCenter(col, row);
    int spawned = 0;
    for (int i = 0; i < MAX_PARTICLES && spawned < 12; i++) {
        Particle *p = &g->particles[i];
        if (p->life > 0.0f) continue;
        float angle = (float)GetRandomValue(0, 360) * DEG2RAD;
        float speed = (float)GetRandomValue(80, 280);
        p->pos      = center;
        p->vel      = (Vector2){cosf(angle) * speed, sinf(angle) * speed};
        p->color    = c;
        p->maxLife  = (float)GetRandomValue(25, 55) / 100.0f;
        p->life     = p->maxLife;
        p->size     = (float)GetRandomValue(4, 10);
        p->rot      = (float)GetRandomValue(0, 360);
        p->rotSpeed = (float)GetRandomValue(-400, 400);
        spawned++;
    }
}

void UpdateParticles(Game *g, float dt) {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        Particle *p = &g->particles[i];
        if (p->life <= 0.0f) continue;
        p->life   -= dt;
        p->pos.x  += p->vel.x * dt;
        p->pos.y  += p->vel.y * dt;
        p->vel.y  += 400.0f * dt;   // gravity
        p->vel.x  *= 0.98f;
        p->rot    += p->rotSpeed * dt;
    }
}

// ---------------------------------------------------------------------------
// Highscores
// ---------------------------------------------------------------------------
void LoadHighscores(Highscore *hs) {
    FILE *f = fopen("bbhit_scores.dat", "rb");
    if (!f) { memset(hs, 0, sizeof(*hs)); return; }
    fread(hs, sizeof(Highscore), 1, f);
    fclose(f);
}

void SaveHighscores(const Highscore *hs) {
    FILE *f = fopen("bbhit_scores.dat", "wb");
    if (!f) return;
    fwrite(hs, sizeof(Highscore), 1, f);
    fclose(f);
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

// ---------------------------------------------------------------------------
// Brick color — neon palette based on HP tier + level
// ---------------------------------------------------------------------------
Color GetBrickColor(int hp, int level) {
    float hue = fmodf((float)(level * 53 + (int)(hp * 0.3f)), 360.0f);
    if (hp <= 3)        return ColorFromHSV(hue, 0.55f, 1.00f);
    else if (hp <= 10)  return ColorFromHSV(hue, 0.70f, 0.95f);
    else if (hp <= 30)  return ColorFromHSV(hue, 0.80f, 0.85f);
    else if (hp <= 100) return ColorFromHSV(hue, 0.90f, 0.75f);
    else                return ColorFromHSV(hue, 1.00f, 0.60f);
}

// ---------------------------------------------------------------------------
// SpawnNewRow
// ---------------------------------------------------------------------------
void SpawnNewRow(Game *g) {
    // Shift existing bricks and pickups down
    for (int i = 0; i < MAX_BRICKS; i++) {
        if (g->bricks[i].active) g->bricks[i].row++;
    }
    for (int i = 0; i < MAX_PICKUPS; i++) {
        if (g->pickups[i].active) g->pickups[i].row++;
        if (g->pickups[i].active && g->pickups[i].row >= GRID_ROWS)
            g->pickups[i].active = false;
    }

    // Game over check
    for (int i = 0; i < MAX_BRICKS; i++) {
        if (g->bricks[i].active && g->bricks[i].row >= GRID_ROWS) {
            g->state = STATE_GAME_OVER;
            g->animTimer = 0.0f;
            PlaySfx(g, g->sndGameOver);
            return;
        }
    }

    // Compute HP for this row — scales with round inside level
    int globalRound = (g->level - 1) * ROUNDS_PER_LEVEL + g->round;
    int newHP = (int)((float)globalRound * (1.0f + (g->level - 1) * 0.3f))
                + GetRandomValue(0, globalRound / 3 + 1);
    if (newHP < 1) newHP = 1;

    // Shuffle columns and pick 3-6
    int bricksInRow = GetRandomValue(3, GRID_COLS - 1);
    int cols[GRID_COLS] = {0,1,2,3,4,5,6};
    for (int i = GRID_COLS - 1; i > 0; i--) {
        int j  = GetRandomValue(0, i);
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
                break;
            }
        }
    }

    // 30% chance: +1 ball pickup in an empty column
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

    // Advance round / level
    g->round++;
    if (g->round > ROUNDS_PER_LEVEL) {
        g->round  = 1;
        g->level++;
        g->theme     = GetThemeForLevel(g->level);
        g->levelUpTimer = 0.0f;
        g->state     = STATE_LEVEL_UP;
        PlaySfx(g, g->sndLevelUp);
    }
}

// ---------------------------------------------------------------------------
// Init / Reset
// ---------------------------------------------------------------------------
void InitGame(Game *g) {
    memset(g, 0, sizeof(Game));
    g->state       = STATE_MENU;
    g->round       = 1;
    g->level       = 1;
    g->ballCount   = 3;
    g->launcherX   = LAUNCHER_X;
    g->aimDir      = (Vector2){0.0f, -1.0f};
    g->soundEnabled = true;
    g->theme        = GetThemeForLevel(1);

    if (IsAudioDeviceReady()) {
        g->audioReady   = true;
        g->sndHit       = GenerateBeep(520.0f,  0.04f, 0.5f);
        g->sndDestroy   = GenerateBeep(860.0f,  0.10f, 0.65f);
        g->sndPickup    = GenerateBeep(1300.0f, 0.14f, 0.75f);
        g->sndGameOver  = GenerateBeep(110.0f,  0.90f, 0.85f);
        g->sndRoundEnd  = GenerateBeep(680.0f,  0.18f, 0.60f);
        g->sndLevelUp   = GenerateBeep(990.0f,  0.35f, 0.75f);
    }

    LoadHighscores(&g->highscores);

    // Menu demo balls
    for (int i = 0; i < 4; i++) {
        g->demoBallPos[i] = (Vector2){(float)GetRandomValue(40, SCREEN_W-40), (float)GetRandomValue(180, 500)};
        float ang = (float)GetRandomValue(20, 160) * DEG2RAD;
        g->demoBallVel[i] = (Vector2){cosf(ang) * 100.0f, -sinf(ang) * 100.0f};
    }
}

void ResetGame(Game *g) {
    Sound h = g->sndHit, d = g->sndDestroy, pk = g->sndPickup;
    Sound go = g->sndGameOver, re = g->sndRoundEnd, lu = g->sndLevelUp;
    bool snd = g->soundEnabled, aud = g->audioReady;
    Highscore hs = g->highscores;

    memset(g, 0, sizeof(Game));
    g->state       = STATE_PLAYING;
    g->round       = 1;
    g->level       = 1;
    g->ballCount   = 3;
    g->launcherX   = LAUNCHER_X;
    g->aimDir      = (Vector2){0.0f, -1.0f};
    g->soundEnabled = snd;
    g->audioReady   = aud;
    g->sndHit       = h;
    g->sndDestroy   = d;
    g->sndPickup    = pk;
    g->sndGameOver  = go;
    g->sndRoundEnd  = re;
    g->sndLevelUp   = lu;
    g->highscores   = hs;
    g->theme        = GetThemeForLevel(1);

    // Pre-fill 3 rows
    for (int r = 0; r < 3; r++) SpawnNewRow(g);
    g->round = 1;
    g->level = 1;
    g->state = STATE_PLAYING;
    g->theme = GetThemeForLevel(1);
}

// ---------------------------------------------------------------------------
// Aim
// ---------------------------------------------------------------------------
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

// ---------------------------------------------------------------------------
// Firing
// ---------------------------------------------------------------------------
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

// ---------------------------------------------------------------------------
// Ball physics
// ---------------------------------------------------------------------------
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

        // Wall bounces
        if (b->position.x <= leftWall) {
            b->position.x = leftWall;
            b->velocity.x = fabsf(b->velocity.x);
        }
        if (b->position.x >= rightWall) {
            b->position.x = rightWall;
            b->velocity.x = -fabsf(b->velocity.x);
        }
        if (b->position.y <= topWall) {
            b->position.y = topWall;
            b->velocity.y = fabsf(b->velocity.y);
        }

        // Return to bottom
        if (b->position.y >= LAUNCHER_Y) {
            b->active   = false;
            b->returned = true;
            if (!g->firstReturned) {
                g->firstReturned = true;
                g->firstReturnX  = b->position.x;
            }
            g->returnedBallCount++;
            g->activeBallCount--;
            continue;
        }

        // Brick collision — one per ball per frame
        bool hitBrick = false;
        for (int i = 0; i < MAX_BRICKS && !hitBrick; i++) {
            Brick *br = &g->bricks[i];
            if (!br->active) continue;

            Rectangle cell = {
                GRID_OFFSET_X + br->col * CELL_SIZE,
                GRID_OFFSET_Y + br->row * CELL_SIZE,
                CELL_SIZE, CELL_SIZE
            };
            if (!CheckCollisionCircleRec(b->position, BALL_RADIUS, cell)) continue;

            hitBrick = true;

            // Side detection
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
                SpawnBrickParticles(g, br->col, br->row, br->color);
                br->active = false;
                g->score  += SCORE_PER_BRICK;
                PlaySfx(g, g->sndDestroy);
            } else {
                PlaySfx(g, g->sndHit);
            }
        }

        // Pickup collision
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

    // Update brick shake/flash
    for (int i = 0; i < MAX_BRICKS; i++) {
        if (g->bricks[i].flashTimer > 0.0f) {
            g->bricks[i].flashTimer -= dt;
            if (g->bricks[i].flashTimer < 0.0f) g->bricks[i].flashTimer = 0.0f;
        }
        if (g->bricks[i].shakeX > 0.0f) {
            g->bricks[i].shakeX -= dt * 40.0f;
            if (g->bricks[i].shakeX < 0.0f) g->bricks[i].shakeX = 0.0f;
        }
    }
}

// ---------------------------------------------------------------------------
// Round end
// ---------------------------------------------------------------------------
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

    for (int i = 0; i < BALL_POOL_SIZE; i++) {
        g->balls[i].active   = false;
        g->balls[i].returned = false;
    }
    g->activeBallCount   = 0;
    g->returnedBallCount = 0;
    g->firstReturned     = false;
    g->roundActive       = false;
}

// ---------------------------------------------------------------------------
// Update functions
// ---------------------------------------------------------------------------
void UpdateMenu(Game *g, float dt) {
    for (int i = 0; i < 4; i++) {
        g->demoBallPos[i].x += g->demoBallVel[i].x * dt;
        g->demoBallPos[i].y += g->demoBallVel[i].y * dt;
        if (g->demoBallPos[i].x < 15 || g->demoBallPos[i].x > SCREEN_W-15) g->demoBallVel[i].x = -g->demoBallVel[i].x;
        if (g->demoBallPos[i].y < 15 || g->demoBallPos[i].y > SCREEN_H-15) g->demoBallVel[i].y = -g->demoBallVel[i].y;
    }
    if (IsKeyPressed(KEY_ENTER)) ResetGame(g);
    if (IsKeyPressed(KEY_H))     g->state = STATE_HIGHSCORE_VIEW;
    if (IsKeyPressed(KEY_M))     g->soundEnabled = !g->soundEnabled;
    if (IsKeyPressed(KEY_ESCAPE)) CloseWindow();
}

void UpdatePlaying(Game *g, float dt) {
    g->launcherPulse += dt * 3.0f;
    UpdateParticles(g, dt);

    // Brick timers
    for (int i = 0; i < MAX_BRICKS; i++) {
        if (g->bricks[i].flashTimer > 0.0f) {
            g->bricks[i].flashTimer -= dt;
            if (g->bricks[i].flashTimer < 0.0f) g->bricks[i].flashTimer = 0.0f;
        }
        if (g->bricks[i].shakeX > 0.0f) {
            g->bricks[i].shakeX *= 0.85f;
            if (g->bricks[i].shakeX < 0.05f) g->bricks[i].shakeX = 0.0f;
        }
    }

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
                for (int i = 0; i < BALL_POOL_SIZE; i++) {
                    g->balls[i].active   = false;
                    g->balls[i].returned = false;
                }
            }
        }
    } else {
        UpdateFiring(g, dt);
        UpdateBallPhysics(g, dt);
        if (g->ballsToFire == 0 && g->activeBallCount == 0)
            CheckRoundEnd(g);
    }
}

void UpdateLevelUp(Game *g, float dt) {
    g->levelUpTimer += dt;
    UpdateParticles(g, dt);
    if (IsKeyPressed(KEY_M)) g->soundEnabled = !g->soundEnabled;
    if (g->levelUpTimer >= 2.2f || IsKeyPressed(KEY_ENTER) || IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        // Reset ball pool for new round
        for (int i = 0; i < BALL_POOL_SIZE; i++) {
            g->balls[i].active   = false;
            g->balls[i].returned = false;
        }
        g->activeBallCount   = 0;
        g->returnedBallCount = 0;
        g->firstReturned     = false;
        g->roundActive       = false;
        g->state             = STATE_PLAYING;
    }
}

void UpdateGameOver(Game *g) {
    g->animTimer += GetFrameTime();
    if (IsKeyPressed(KEY_M)) g->soundEnabled = !g->soundEnabled;
    if (IsKeyPressed(KEY_ENTER)) {
        if (IsNewHighscore(&g->highscores, g->score)) {
            g->inputLen = 0;
            memset(g->inputName, 0, NAME_LEN);
            g->state = STATE_HIGHSCORE_ENTRY;
        } else {
            ResetGame(g);
        }
    }
    if (IsKeyPressed(KEY_Q)) g->state = STATE_MENU;
}

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
        SaveHighscores(&g->highscores);
        g->state = STATE_HIGHSCORE_VIEW;
    }
}

void UpdateHighscoreView(Game *g) {
    if (IsKeyPressed(KEY_M)) g->soundEnabled = !g->soundEnabled;
    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_ENTER)) g->state = STATE_MENU;
}

// ---------------------------------------------------------------------------
// Draw: Background gradient
// ---------------------------------------------------------------------------
void DrawBackground(const Game *g) {
    // Draw vertical gradient using horizontal strips
    int strips = 16;
    for (int i = 0; i < strips; i++) {
        float t0 = (float)i / strips;
        float t1 = (float)(i + 1) / strips;
        Color c0 = BlendColor(g->theme.bgA, g->theme.bgB, t0);
        Color c1 = BlendColor(g->theme.bgA, g->theme.bgB, t1);
        int y0 = (int)(t0 * SCREEN_H);
        int y1 = (int)(t1 * SCREEN_H);
        DrawRectangleGradientV(0, y0, SCREEN_W, y1 - y0, c0, c1);
    }
}

// ---------------------------------------------------------------------------
// Draw: HUD
// ---------------------------------------------------------------------------
void DrawHUD(const Game *g) {
    // Top bar with slight glass effect
    DrawRectangle(0, 0, SCREEN_W, (int)HUD_HEIGHT, (Color){0,0,0,140});
    DrawLine(0, (int)HUD_HEIGHT, SCREEN_W, (int)HUD_HEIGHT, (Color){255,255,255,18});

    // Round inside level
    char buf[64];
    int globalRound = (g->level - 1) * ROUNDS_PER_LEVEL + g->round - 1;
    sprintf(buf, "LV%d  R%d", g->level, globalRound > 0 ? globalRound : 1);
    DrawText(buf, 10, 14, 18, (Color){200,200,220,255});

    // Score center
    sprintf(buf, "%d", g->score);
    int sw = MeasureText(buf, 22);
    DrawText(buf, (SCREEN_W - sw) / 2, 12, 22, WHITE);

    // Ball count — right side: small circles + number
    sprintf(buf, "x%d", g->ballCount);
    int bw = MeasureText(buf, 18);
    DrawText(buf, SCREEN_W - bw - 8, 14, 18, WHITE);
    // 3 mini ball icons
    for (int i = 0; i < 3 && i < g->ballCount; i++) {
        DrawCircle(SCREEN_W - bw - 22 - i * 14, 24, 4, (Color){220,240,255,200});
    }

    // Sound indicator
    const char *sm = g->soundEnabled ? "[M]" : "[M]OFF";
    DrawText(sm, 4, SCREEN_H - 18, 13, (Color){120,120,140,200});
}

// ---------------------------------------------------------------------------
// Draw: Bricks
// ---------------------------------------------------------------------------
void DrawBricks(const Game *g) {
    for (int i = 0; i < MAX_BRICKS; i++) {
        const Brick *b = &g->bricks[i];
        if (!b->active) continue;

        float shk = b->shakeX * sinf((float)GetTime() * 80.0f);
        Rectangle base = {
            GRID_OFFSET_X + b->col * CELL_SIZE + 4 + shk,
            GRID_OFFSET_Y + b->row * CELL_SIZE + 4,
            CELL_SIZE - 8,
            CELL_SIZE - 8
        };

        Color col = (b->flashTimer > 0.0f) ? WHITE : b->color;

        // Shadow
        DrawRectangleRounded((Rectangle){base.x+3, base.y+3, base.width, base.height},
                             0.25f, 6, (Color){0,0,0,80});
        // Main body
        DrawRectangleRounded(base, 0.25f, 6, col);
        // Inner highlight (top-left bright strip)
        DrawRectangleRounded(
            (Rectangle){base.x+2, base.y+2, base.width-4, (base.height-4)*0.35f},
            0.25f, 6, Fade(WHITE, 0.18f));
        // Border glow
        Color borderCol = col;
        borderCol.a = 200;
        DrawRectangleRoundedLines(base, 0.25f, 6, Fade(borderCol, 0.55f));

        // HP bar at bottom of brick (remaining ratio)
        if (b->maxHp > 1) {
            float ratio = (float)b->hp / b->maxHp;
            float barW  = (base.width - 6) * ratio;
            DrawRectangle((int)(base.x+3), (int)(base.y + base.height - 5), (int)(base.width-6), 3,
                          (Color){0,0,0,100});
            DrawRectangle((int)(base.x+3), (int)(base.y + base.height - 5), (int)barW, 3,
                          Fade(WHITE, 0.6f));
        }

        // HP text centered
        char hpBuf[16];
        snprintf(hpBuf, sizeof(hpBuf), "%d", b->hp);
        int fs = (b->hp >= 10000) ? 11 : (b->hp >= 1000) ? 13 : (b->hp >= 100) ? 15 : (b->hp >= 10) ? 18 : 22;
        int tw = MeasureText(hpBuf, fs);
        int tx = (int)(base.x + base.width/2 - tw/2);
        int ty = (int)(base.y + base.height/2 - fs/2 - 1);
        // Subtle text shadow
        DrawText(hpBuf, tx+1, ty+1, fs, (Color){0,0,0,160});
        DrawText(hpBuf, tx, ty, fs, WHITE);
    }
}

// ---------------------------------------------------------------------------
// Draw: Pickups
// ---------------------------------------------------------------------------
void DrawPickups(const Game *g) {
    for (int i = 0; i < MAX_PICKUPS; i++) {
        const BallPickup *p = &g->pickups[i];
        if (!p->active) continue;

        Vector2 center = GetCellCenter(p->col, p->row);
        float pulse = sinf((float)GetTime() * 4.5f + p->pulseT);
        float r     = CELL_SIZE * 0.20f + pulse * 2.5f;

        // Outer glow rings
        DrawCircleV(center, r + 10, Fade(YELLOW, 0.06f));
        DrawCircleV(center, r + 6,  Fade(YELLOW, 0.12f));
        DrawCircleV(center, r + 3,  Fade(YELLOW, 0.20f));
        // Main orb
        DrawCircleV(center, r, (Color){255,220,30,255});
        // White glint
        DrawCircleV((Vector2){center.x - r*0.28f, center.y - r*0.28f}, r*0.22f, Fade(WHITE, 0.7f));
        // "+1" text
        const char *plusOne = "+1";
        int tw = MeasureText(plusOne, 14);
        DrawText(plusOne, (int)center.x - tw/2, (int)center.y - 7, 14, (Color){30,20,0,255});
    }
}

// ---------------------------------------------------------------------------
// Draw: Trajectory
// ---------------------------------------------------------------------------
void DrawTrajectory(const Game *g) {
    if (g->roundActive) return;

    Vector2 pos = {g->launcherX, LAUNCHER_Y - BALL_RADIUS - 2.0f};
    Vector2 dir = g->aimDir;
    float step   = 11.0f;
    float leftW  = GRID_OFFSET_X + 1.0f;
    float rightW = GRID_OFFSET_X + GRID_COLS * CELL_SIZE - 1.0f;
    float topW   = GRID_OFFSET_Y;
    int maxDots  = 45;

    for (int i = 0; i < maxDots; i++) {
        pos.x += dir.x * step;
        pos.y += dir.y * step;
        if (pos.y <= topW) break;
        if (pos.x <= leftW)  { pos.x = leftW;  dir.x =  fabsf(dir.x); }
        if (pos.x >= rightW) { pos.x = rightW; dir.x = -fabsf(dir.x); }

        float t     = 1.0f - (float)i / maxDots;
        float alpha = t * t * 0.85f;
        float dotR  = 2.5f + t * 1.5f;
        DrawCircleV(pos, dotR, Fade(g->theme.ballGlow, alpha));
    }
}

// ---------------------------------------------------------------------------
// Draw: Balls (with trails)
// ---------------------------------------------------------------------------
void DrawBalls(const Game *g) {
    for (int bi = 0; bi < BALL_POOL_SIZE; bi++) {
        const Ball *b = &g->balls[bi];
        if (!b->active) continue;

        // Draw trail
        int count = b->trail.count;
        for (int t = 0; t < count; t++) {
            int idx  = (b->trail.head - 1 - t + TRAIL_LEN) % TRAIL_LEN;
            float tf = 1.0f - (float)(t + 1) / (TRAIL_LEN + 1);
            float tr = BALL_RADIUS * tf * 0.75f;
            Color tc = g->theme.ballGlow;
            tc.a = (unsigned char)(tf * tf * 120.0f);
            DrawCircleV(b->trail.pos[idx], tr, tc);
        }

        // Glow layers
        Color glowOuter = g->theme.ballGlow; glowOuter.a = 30;
        Color glowInner = g->theme.ballGlow; glowInner.a = 60;
        DrawCircleV(b->position, BALL_RADIUS + 5.0f, glowOuter);
        DrawCircleV(b->position, BALL_RADIUS + 2.5f, glowInner);
        // Main ball
        DrawCircleV(b->position, BALL_RADIUS, WHITE);
        // Glint
        DrawCircleV((Vector2){b->position.x - 2.5f, b->position.y - 2.5f}, 2.8f, Fade(WHITE, 0.75f));
    }
}

// ---------------------------------------------------------------------------
// Draw: Launcher
// ---------------------------------------------------------------------------
void DrawLauncher(const Game *g) {
    float px = g->launcherX;
    float py = LAUNCHER_Y;

    // Pulsing ring under launcher
    float pulse = sinf(g->launcherPulse) * 0.5f + 0.5f;
    Color rc = g->theme.ballGlow;
    DrawCircleLines((int)px, (int)py, 18.0f + pulse * 6.0f, Fade(rc, 0.15f + pulse * 0.1f));
    DrawCircleLines((int)px, (int)py, 12.0f, Fade(rc, 0.25f));

    // Aim direction triangle
    if (!g->roundActive) {
        Vector2 tip  = {px + g->aimDir.x * 24.0f, py + g->aimDir.y * 24.0f};
        Vector2 perp = {-g->aimDir.y, g->aimDir.x};
        Vector2 lft  = {px + perp.x * 8.0f, py + perp.y * 8.0f};
        Vector2 rgt  = {px - perp.x * 8.0f, py - perp.y * 8.0f};
        DrawTriangle(tip, lft, rgt, g->theme.ballGlow);
        DrawTriangleLines(tip, lft, rgt, Fade(WHITE, 0.4f));
    }

    // Base circle
    DrawCircleV((Vector2){px, py}, 7.0f, WHITE);
    DrawCircleV((Vector2){px, py}, 4.5f, (Color){180,200,255,255});

    // Ball count label just below launcher
    char cntBuf[16];
    sprintf(cntBuf, "x%d", g->ballCount);
    int tw = MeasureText(cntBuf, 15);
    DrawText(cntBuf, (int)(px - tw/2), (int)(py + 12), 15, Fade(WHITE, 0.7f));
}

// ---------------------------------------------------------------------------
// Draw: Particles
// ---------------------------------------------------------------------------
void DrawParticles(const Game *g) {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        const Particle *p = &g->particles[i];
        if (p->life <= 0.0f) continue;
        float t = p->life / p->maxLife;
        Color c = p->color;
        c.a = (unsigned char)(t * 220.0f);
        float sz = p->size * (0.4f + t * 0.6f);

        // Rotated square particle
        DrawRectanglePro(
            (Rectangle){p->pos.x, p->pos.y, sz, sz},
            (Vector2){sz * 0.5f, sz * 0.5f},
            p->rot,
            c
        );
    }
}

// ---------------------------------------------------------------------------
// Draw: Playing
// ---------------------------------------------------------------------------
void DrawPlaying(const Game *g) {
    DrawBackground(g);

    // Subtle grid lines
    for (int c = 0; c <= GRID_COLS; c++) {
        DrawLine((int)(GRID_OFFSET_X + c * CELL_SIZE), (int)GRID_OFFSET_Y,
                 (int)(GRID_OFFSET_X + c * CELL_SIZE), (int)(GRID_OFFSET_Y + GRID_ROWS * CELL_SIZE),
                 g->theme.gridLine);
    }
    for (int r = 0; r <= GRID_ROWS; r++) {
        DrawLine((int)GRID_OFFSET_X, (int)(GRID_OFFSET_Y + r * CELL_SIZE),
                 (int)(GRID_OFFSET_X + GRID_COLS * CELL_SIZE), (int)(GRID_OFFSET_Y + r * CELL_SIZE),
                 g->theme.gridLine);
    }

    // Danger line with pulsing glow
    float dpulse = 0.5f + 0.5f * sinf((float)GetTime() * 3.0f);
    Color dl = g->theme.dangerLine;
    DrawLine((int)GRID_OFFSET_X, (int)(GRID_OFFSET_Y + GRID_ROWS * CELL_SIZE),
             (int)(GRID_OFFSET_X + GRID_COLS * CELL_SIZE), (int)(GRID_OFFSET_Y + GRID_ROWS * CELL_SIZE),
             Fade(dl, dpulse));

    DrawParticles(g);
    DrawBricks(g);
    DrawPickups(g);
    DrawTrajectory(g);
    DrawBalls(g);
    DrawLauncher(g);
    DrawHUD(g);

    // Aim hint
    if (!g->roundActive) {
        float alpha = 0.4f + 0.4f * sinf((float)GetTime() * 2.8f);
        const char *hint = "CLICK TO SHOOT";
        int hw = MeasureText(hint, 15);
        DrawText(hint, (SCREEN_W - hw) / 2, SCREEN_H - 28, 15, Fade(LIGHTGRAY, alpha));
    }
}

// ---------------------------------------------------------------------------
// Draw: Level Up
// ---------------------------------------------------------------------------
void DrawLevelUp(const Game *g) {
    DrawBackground(g);
    DrawBricks(g);
    DrawParticles(g);
    DrawHUD(g);

    // Overlay
    DrawRectangle(0, 0, SCREEN_W, SCREEN_H, Fade(BLACK, 0.55f));

    float t  = fminf(g->levelUpTimer / 0.4f, 1.0f);
    float sc = 0.7f + t * 0.3f;
    float alpha = t;

    char lvBuf[32];
    sprintf(lvBuf, "LEVEL %d", g->level);
    int fs = (int)(52 * sc);
    int tw = MeasureText(lvBuf, fs);
    float hue = fmodf((float)GetTime() * 60.0f, 360.0f);
    DrawText(lvBuf, (SCREEN_W - tw)/2, 260, fs, Fade(ColorFromHSV(hue, 0.8f, 1.0f), alpha));

    const char *sub = "KEEP GOING!";
    int sw = MeasureText(sub, 24);
    DrawText(sub, (SCREEN_W - sw)/2, 328, 24, Fade(WHITE, alpha * 0.8f));

    char scoreBuf[32];
    sprintf(scoreBuf, "Score: %d", g->score);
    int scw = MeasureText(scoreBuf, 20);
    DrawText(scoreBuf, (SCREEN_W - scw)/2, 370, 20, Fade(LIGHTGRAY, alpha));

    // Tap to continue
    if (g->levelUpTimer > 0.8f) {
        float blink = 0.5f + 0.5f * sinf((float)GetTime() * 4.0f);
        const char *tap = "TAP TO CONTINUE";
        int tpw = MeasureText(tap, 17);
        DrawText(tap, (SCREEN_W - tpw)/2, 430, 17, Fade(WHITE, blink));
    }
}

// ---------------------------------------------------------------------------
// Draw: Menu
// ---------------------------------------------------------------------------
void DrawMenu(const Game *g) {
    DrawBackground(g);

    // Demo balls
    for (int i = 0; i < 4; i++) {
        DrawGlowCircle(g->demoBallPos[i], 9, Fade(WHITE, 0.35f), Fade(g->theme.ballGlow, 0.0f), 3);
    }

    // Animated title
    float hue = fmodf((float)GetTime() * 45.0f, 360.0f);
    const char *title = "BRICK BREAKER";
    int tw = MeasureText(title, 42);
    DrawText(title, (SCREEN_W - tw)/2, 120, 42, ColorFromHSV(hue, 0.85f, 1.0f));
    const char *sub = "HIT";
    int subw = MeasureText(sub, 56);
    DrawText(sub, (SCREEN_W - subw)/2, 160, 56, ColorFromHSV(fmodf(hue + 40, 360), 0.9f, 1.0f));

    const char *tag = "Aim  *  Shoot  *  Destroy";
    int tagw = MeasureText(tag, 17);
    DrawText(tag, (SCREEN_W - tagw)/2, 226, 17, (Color){150,160,180,255});

    // Best score
    if (g->highscores.count > 0) {
        char bestBuf[32];
        sprintf(bestBuf, "BEST: %d", g->highscores.entries[0].score);
        int bw = MeasureText(bestBuf, 18);
        DrawText(bestBuf, (SCREEN_W - bw)/2, 254, 18, GOLD);
    }

    // Separator line
    DrawLine(SCREEN_W/2 - 80, 280, SCREEN_W/2 + 80, 280, Fade(WHITE, 0.15f));

    struct { const char *text; int y; Color col; } items[] = {
        {"[ENTER]  PLAY",         305, (Color){255,255,100,255}},
        {"[H]      HIGH SCORES",  345, (Color){200,220,255,255}},
        {"[M]      SOUND",        385, (Color){200,220,255,255}},
        {"[ESC]    QUIT",         425, (Color){180,180,200,200}},
    };
    for (int i = 0; i < 4; i++) {
        int iw = MeasureText(items[i].text, 21);
        DrawText(items[i].text, (SCREEN_W - iw)/2, items[i].y, 21, items[i].col);
    }

    // Sound badge
    const char *sndBadge = g->soundEnabled ? "ON" : "OFF";
    Color sndCol = g->soundEnabled ? (Color){100,255,100,255} : (Color){255,100,100,255};
    DrawText(sndBadge, SCREEN_W/2 + MeasureText("[M]      SOUND", 21)/2 + (SCREEN_W - MeasureText("[M]      SOUND", 21))/2 - MeasureText(sndBadge, 21)/2 + 4, 385, 21, sndCol);
}

// ---------------------------------------------------------------------------
// Draw: Game Over
// ---------------------------------------------------------------------------
void DrawGameOver(const Game *g) {
    DrawBackground(g);
    DrawRectangle(0, 0, SCREEN_W, SCREEN_H, Fade(BLACK, 0.72f));

    float t = fminf(g->animTimer / 0.5f, 1.0f);

    const char *go = "GAME OVER";
    int gfs = (int)(60 * (0.6f + t * 0.4f));
    int gw  = MeasureText(go, gfs);
    DrawText(go, (SCREEN_W - gw)/2, 185, gfs, Fade(RED, t));

    char buf[64];
    int globalRound = (g->level - 1) * ROUNDS_PER_LEVEL + g->round - 1;
    sprintf(buf, "Reached Level %d  (Round %d)", g->level, globalRound);
    int bw = MeasureText(buf, 18);
    DrawText(buf, (SCREEN_W - bw)/2, 268, 18, Fade(LIGHTGRAY, t));

    sprintf(buf, "%d", g->score);
    int sw = MeasureText(buf, 46);
    DrawText(buf, (SCREEN_W - sw)/2, 295, 46, Fade(WHITE, t));

    if (t >= 1.0f) {
        if (IsNewHighscore(&g->highscores, g->score)) {
            const char *hs = "NEW HIGH SCORE!";
            int hw = MeasureText(hs, 22);
            DrawText(hs, (SCREEN_W - hw)/2, 360, 22, GOLD);
            float blink = 0.5f + 0.5f * sinf((float)GetTime() * 3.5f);
            const char *sub = "[ENTER]  Enter Name";
            int sbw = MeasureText(sub, 20);
            DrawText(sub, (SCREEN_W - sbw)/2, 398, 20, Fade(YELLOW, blink));
        } else {
            const char *sub = "[ENTER] Play Again   [Q] Menu";
            int sbw = MeasureText(sub, 19);
            DrawText(sub, (SCREEN_W - sbw)/2, 370, 19, Fade(LIGHTGRAY, 0.85f));
        }
    }
}

// ---------------------------------------------------------------------------
// Draw: Highscore Entry
// ---------------------------------------------------------------------------
void DrawHighscoreEntry(const Game *g) {
    DrawBackground(g);
    DrawRectangle(0, 0, SCREEN_W, SCREEN_H, Fade(BLACK, 0.80f));

    const char *hdr = "NEW HIGH SCORE!";
    int hw = MeasureText(hdr, 34);
    DrawText(hdr, (SCREEN_W - hw)/2, 190, 34, GOLD);

    char scoreBuf[32];
    sprintf(scoreBuf, "%d", g->score);
    int sw = MeasureText(scoreBuf, 40);
    DrawText(scoreBuf, (SCREEN_W - sw)/2, 234, 40, WHITE);

    const char *prompt = "Enter your name:";
    int pw = MeasureText(prompt, 20);
    DrawText(prompt, (SCREEN_W - pw)/2, 295, 20, (Color){180,190,210,255});

    // Input box background
    DrawRectangleRounded((Rectangle){60, 322, SCREEN_W-120, 46}, 0.3f, 6, (Color){255,255,255,20});
    DrawRectangleRoundedLines((Rectangle){60, 322, SCREEN_W-120, 46}, 0.3f, 6, Fade(WHITE, 0.35f));

    char display[NAME_LEN + 2];
    strncpy(display, g->inputName, NAME_LEN);
    display[g->inputLen] = '\0';
    if (g->inputLen < NAME_LEN - 1 && (int)(GetTime() * 2) % 2 == 0) {
        display[g->inputLen]     = '|';
        display[g->inputLen + 1] = '\0';
    }
    int dw = MeasureText(display, 30);
    DrawText(display, (SCREEN_W - dw)/2, 330, 30, WHITE);

    const char *hint = "[BACKSPACE] Delete    [ENTER] Confirm";
    int hiw = MeasureText(hint, 14);
    DrawText(hint, (SCREEN_W - hiw)/2, 382, 14, (Color){140,150,170,200});
}

// ---------------------------------------------------------------------------
// Draw: Highscore View
// ---------------------------------------------------------------------------
void DrawHighscoreView(const Game *g) {
    DrawBackground(g);
    DrawRectangle(0, 0, SCREEN_W, SCREEN_H, Fade(BLACK, 0.80f));

    float hue = fmodf((float)GetTime() * 35.0f, 360.0f);
    const char *title = "TOP SCORES";
    int tw = MeasureText(title, 40);
    DrawText(title, (SCREEN_W - tw)/2, 90, 40, ColorFromHSV(hue, 0.8f, 1.0f));

    DrawLine(SCREEN_W/2 - 100, 140, SCREEN_W/2 + 100, 140, Fade(WHITE, 0.15f));

    Color rankCol[] = {
        GOLD,
        LIGHTGRAY,
        (Color){205, 127, 50, 255},
        WHITE,
        WHITE
    };
    const char *medals[] = {"#1", "#2", "#3", "#4", "#5"};

    for (int i = 0; i < g->highscores.count && i < MAX_SCORES; i++) {
        int y = 158 + i * 52;
        // Row highlight for top 3
        if (i < 3) {
            Color hl = rankCol[i]; hl.a = 18;
            DrawRectangleRounded((Rectangle){30, (float)y-4, SCREEN_W-60, 42}, 0.3f, 4, hl);
        }
        DrawText(medals[i], 38, y + 6, 20, rankCol[i]);
        DrawText(g->highscores.entries[i].name, 80, y + 6, 20, rankCol[i]);
        char sc[32]; sprintf(sc, "%d", g->highscores.entries[i].score);
        int scw = MeasureText(sc, 20);
        DrawText(sc, SCREEN_W - 38 - scw, y + 6, 20, rankCol[i]);
    }

    if (g->highscores.count == 0) {
        const char *none = "No scores yet — play to set one!";
        int nw = MeasureText(none, 18);
        DrawText(none, (SCREEN_W - nw)/2, 230, 18, (Color){130,140,160,255});
    }

    DrawLine(SCREEN_W/2 - 80, 440, SCREEN_W/2 + 80, 440, Fade(WHITE, 0.12f));
    float blink = 0.5f + 0.5f * sinf((float)GetTime() * 2.5f);
    const char *back = "[ESC] / [ENTER]  Back";
    int bw = MeasureText(back, 17);
    DrawText(back, (SCREEN_W - bw)/2, 452, 17, Fade(LIGHTGRAY, blink));
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
int main(void) {
    InitWindow(SCREEN_W, SCREEN_H, "Brick Breaker Hit");
    SetTargetFPS(60);
    InitAudioDevice();

    Game game = {0};
    InitGame(&game);

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        switch (game.state) {
            case STATE_MENU:            UpdateMenu(&game, dt);         break;
            case STATE_PLAYING:         UpdatePlaying(&game, dt);      break;
            case STATE_LEVEL_UP:        UpdateLevelUp(&game, dt);      break;
            case STATE_GAME_OVER:       UpdateGameOver(&game);         break;
            case STATE_HIGHSCORE_ENTRY: UpdateHighscoreEntry(&game);   break;
            case STATE_HIGHSCORE_VIEW:  UpdateHighscoreView(&game);    break;
        }

        BeginDrawing();
        ClearBackground((Color){10, 10, 25, 255});

        switch (game.state) {
            case STATE_MENU:            DrawMenu(&game);            break;
            case STATE_PLAYING:         DrawPlaying(&game);         break;
            case STATE_LEVEL_UP:        DrawLevelUp(&game);         break;
            case STATE_GAME_OVER:       DrawGameOver(&game);        break;
            case STATE_HIGHSCORE_ENTRY: DrawHighscoreEntry(&game);  break;
            case STATE_HIGHSCORE_VIEW:  DrawHighscoreView(&game);   break;
        }

        EndDrawing();
    }

    SaveHighscores(&game.highscores);
    if (game.audioReady) {
        UnloadSound(game.sndHit);   UnloadSound(game.sndDestroy);
        UnloadSound(game.sndPickup); UnloadSound(game.sndGameOver);
        UnloadSound(game.sndRoundEnd); UnloadSound(game.sndLevelUp);
        CloseAudioDevice();
    }
    CloseWindow();
    return 0;
}
