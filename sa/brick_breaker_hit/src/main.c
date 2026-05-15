// Brick Breaker Hit — Enhanced Ballz-style shooter in C + raylib
#include "raylib.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// ── Constants ────────────────────────────────────────────────────────────────
#define SCREEN_W              540
#define SCREEN_H              720
#define GRID_COLS             8
#define GRID_ROWS             9
#define CELL_SIZE             58.0f
#define GRID_OFFSET_X         ((SCREEN_W - GRID_COLS * CELL_SIZE) / 2.0f)
#define GRID_OFFSET_Y         52.0f
#define LAUNCHER_Y            (SCREEN_H - 52.0f)
#define LAUNCHER_X            (SCREEN_W / 2.0f)
#define MIN_LAUNCH_ANGLE_DEG  8.0f
#define BALL_RADIUS           9.0f
#define BALL_SPEED            680.0f
#define BALL_LAUNCH_DELAY     0.06f
#define MAX_BRICKS            (GRID_COLS * GRID_ROWS)
#define BALL_POOL_SIZE        800   // must exceed the highest possible ballCount
#define MAX_PICKUPS           (GRID_COLS * 2)
#define SCORE_PER_BRICK       10
#define MAX_SCORES            5
#define NAME_LEN              12
#define ROUNDS_PER_LEVEL      10
#define MAX_SELECTABLE_LEVELS 20
#define TRAIL_LEN             12
#define MAX_PARTICLES         300
#define MAX_SHOCKWAVES        16
#define MAX_STARS             100
#define HUD_HEIGHT            48.0f
#define INITIAL_BALLS         5
// Per-level ball bonus scales linearly. Reduced to ~2/3 of previous values
// so games stay shorter and per-round combos matter more:
// bonus(level_just_cleared) = ((8 + 2*level) * 2) / 3.
#define LEVEL_UP_BALL_BONUS(level) (((8 + 2 * (level)) * 2) / 3)
#define MIN_VY_RATIO          0.18f   // |vy| must stay above this * BALL_SPEED to prevent stuck balls
#define ROUND_TIMEOUT_SEC     30.0f   // failsafe: force-end round if it drags on

// ── Menu button layout ────────────────────────────────────────────────────────
#define MENU_BTN_W   280
#define MENU_BTN_H   48
#define MENU_BTN_X   ((SCREEN_W - MENU_BTN_W) / 2)
#define MENU_BTN_Y0  286
#define MENU_BTN_GAP 60
#define MENU_BTN_COUNT 5

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
    STATE_CREDITS,        // menu → credits screen
    STATE_VICTORY,        // shown after clearing level 20
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
    bool  explosive;    // explodes on death — destroys 4 neighbours
    bool  superBomb;    // rare: destroys an entire row OR column on death
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

typedef struct {
    Vector2 pos;
    float   life, maxLife, maxRadius;
    Color   color;
} Shockwave;

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
    Shockwave  shockwaves[MAX_SHOCKWAVES];
    Star       stars[MAX_STARS];

    Sound  sndHit, sndDestroy, sndPickup, sndGameOver, sndRoundEnd, sndLevelUp, sndBoom, sndMegaBoom, sndCelebration;
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

    // Round timeout failsafe
    float  roundElapsed;

    // Last level-up bonus amount (for the "+N BALLS" overlay)
    int    lastLevelBonus;

    // Credits / Victory animation timers
    float  creditsTimer;
    float  victoryTimer;
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
    // 13 Arctic
    {{12,22,40,255},{7,14,28,255},{100,190,240,20},{200,240,255,255},{255,80,60,200},{160,225,255,255},{80,170,220,255}},
    // 14 Ember
    {{38,12,4,255},{24,7,2,255},{220,80,20,20},{255,140,30,255},{60,180,255,200},{255,110,20,255},{200,65,10,255}},
    // 15 Forest
    {{4,28,10,255},{2,18,6,255},{30,160,60,20},{50,220,90,255},{255,90,40,200},{30,200,70,255},{15,130,40,255}},
    // 16 Galaxy
    {{8,5,38,255},{5,3,25,255},{80,50,200,20},{160,110,255,255},{255,60,80,200},{140,80,255,255},{80,40,200,255}},
    // 17 Neon
    {{35,4,28,255},{22,2,18,255},{220,40,180,20},{255,80,240,255},{60,255,180,200},{255,40,220,255},{180,20,160,255}},
    // 18 Storm
    {{12,15,22,255},{7,9,14,255},{80,110,160,20},{140,180,230,255},{255,140,30,200},{100,150,210,255},{60,90,150,255}},
    // 19 Bronze
    {{28,18,6,255},{18,11,3,255},{160,100,30,20},{220,160,60,255},{60,180,255,200},{200,140,40,255},{140,90,20,255}},
    // 20 Crystal
    {{8,18,30,255},{5,11,20,255},{120,200,255,20},{200,255,255,255},{255,80,100,200},{160,255,240,255},{80,200,220,255}},
};
#define NUM_THEMES 20

// ── Forward declarations ───────────────────────────────────────────────────────
void  InitGame(Game *g);
void  ResetGame(Game *g, int startLevel);
void  SpawnNewRow(Game *g);
void  LoadSaveData(Game *g);
void  WriteSaveData(const Game *g);
bool  IsNewHighscore(const Highscore *hs, int score);
void  InsertHighscore(Highscore *hs, const char *name, int score);
Sound GenerateBeep(float freq, float dur, float vol);
Sound GenerateBoom(float dur, float vol);
Sound GenerateMegaBoom(float dur, float vol);
Sound GenerateCelebrationLoop(void);
Image BuildAppIcon(void);
void  PlaySfx(const Game *g, Sound snd);
void  SpawnBrickParticles(Game *g, int col, int row, Color c);
void  SpawnMenuOrb(Game *g);
void  UpdateParticles(Game *g, float dt);
void  ExplodeBrick(Game *g, int col, int row);
void  ExplodeBrickLine(Game *g, int col, int row);
void  SpawnExplosionFX(Game *g, Vector2 center);
void  SpawnLineExplosionFX(Game *g, int col, int row, bool vertical);
void  UpdateShockwaves(Game *g, float dt);
void  DrawShockwaves(const Game *g);
void  InitStars(Game *g);

void UpdateMenu(Game *g, float dt);
void UpdateLevelSelect(Game *g, float dt);
void UpdatePlaying(Game *g, float dt);
void UpdatePaused(Game *g);
void UpdateLevelUp(Game *g, float dt);
void UpdateGameOver(Game *g);
void UpdateHighscoreEntry(Game *g);
void UpdateHighscoreView(Game *g);
void UpdateCredits(Game *g, float dt);
void UpdateVictory(Game *g, float dt);
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
void DrawCredits(const Game *g);
void DrawVictory(const Game *g);
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
    // 4 columns × 5 rows, 1-based
    int i = level - 1;
    int col = i % 4, row = i / 4;
    return (Rectangle){58.0f + col * 106.0f, 140.0f + row * 88.0f, 98.0f, 78.0f};
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
        float b = 0.20f + 0.65f * (0.5f + 0.5f * sinf(t * s->speed + s->phase));
        DrawCircleV((Vector2){s->x, s->y}, s->size, Fade(WHITE, b));
        // Occasional sparkle cross on bright stars
        if (s->size > 1.6f && b > 0.75f) {
            float spk = (b - 0.75f) * 4.0f;
            float sl  = s->size * 2.8f * spk;
            DrawLineEx((Vector2){s->x - sl, s->y}, (Vector2){s->x + sl, s->y}, 0.8f, Fade(WHITE, spk * 0.6f));
            DrawLineEx((Vector2){s->x, s->y - sl}, (Vector2){s->x, s->y + sl}, 0.8f, Fade(WHITE, spk * 0.6f));
        }
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

// Explosion "boom": white-noise + low-freq sine sweep (220Hz → 40Hz) with
// exponential decay. Sounds like a small bomb, no external samples needed.
Sound GenerateBoom(float dur, float vol) {
    int sampleRate  = 44100;
    int sampleCount = (int)(sampleRate * dur);
    short *data     = (short *)malloc(sampleCount * sizeof(short));
    unsigned int rng = 0x9E3779B9u;
    for (int i = 0; i < sampleCount; i++) {
        float tSec  = (float)i / sampleRate;
        float prog  = tSec / dur;                 // 0..1
        float env   = expf(-prog * 4.5f);         // sharp decay
        float freq  = 220.0f - 180.0f * prog;     // sweep down
        float sine  = sinf(2.0f * PI * freq * tSec);
        // xorshift32 noise in [-1, 1]
        rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
        float noise = ((float)(rng & 0xFFFF) / 32768.0f) - 1.0f;
        // Early burst dominated by noise, tail by rumble
        float mix   = 0.55f * noise * (1.0f - prog * 0.7f) + 0.45f * sine;
        float s     = mix * env * vol;
        if (s >  1.0f) s =  1.0f;
        if (s < -1.0f) s = -1.0f;
        data[i] = (short)(s * 32767.0f);
    }
    Wave w = {.frameCount=(unsigned int)sampleCount, .sampleRate=44100,
              .sampleSize=16, .channels=1, .data=data};
    Sound s = LoadSoundFromWave(w);
    free(data);
    return s;
}

// Mega boom: lower-pitch rumble for super-bomb line clears.
// Triple-sine bass stack (50/75/110Hz) + noise + long decay tail.
Sound GenerateMegaBoom(float dur, float vol) {
    int sampleRate  = 44100;
    int sampleCount = (int)(sampleRate * dur);
    short *data     = (short *)malloc(sampleCount * sizeof(short));
    unsigned int rng = 0xCAFEBABEu;
    for (int i = 0; i < sampleCount; i++) {
        float tSec  = (float)i / sampleRate;
        float prog  = tSec / dur;
        // Slower decay than regular boom for a sustained "BOOOM"
        float env   = expf(-prog * 2.6f);
        // Three-layer bass sweep
        float f1    = 110.0f - 70.0f * prog;
        float f2    = 75.0f  - 45.0f * prog;
        float f3    = 50.0f  - 25.0f * prog;
        float bass  = (sinf(2.0f * PI * f1 * tSec)
                     + sinf(2.0f * PI * f2 * tSec) * 0.85f
                     + sinf(2.0f * PI * f3 * tSec) * 0.75f) / 2.6f;
        // Noise (loud early, quiet later)
        rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
        float noise = ((float)(rng & 0xFFFF) / 32768.0f) - 1.0f;
        float mix   = 0.45f * noise * (1.0f - prog) + 0.55f * bass;
        float s     = mix * env * vol;
        if (s >  1.0f) s =  1.0f;
        if (s < -1.0f) s = -1.0f;
        data[i] = (short)(s * 32767.0f);
    }
    Wave w = {.frameCount=(unsigned int)sampleCount, .sampleRate=44100,
              .sampleSize=16, .channels=1, .data=data};
    Sound s = LoadSoundFromWave(w);
    free(data);
    return s;
}

// Bright C-major celebration loop: 16 notes (~4.8s) with envelopes + 2nd
// harmonic + light vibrato. Loopable.
Sound GenerateCelebrationLoop(void) {
    const int sampleRate = 44100;
    const float noteDur  = 0.30f;
    // C5 E5 G5 C6 G5 E5 G5 C6  A5 C6 E6 A5 G5 E5 C5 rest
    const float notes[] = {
        523.25f, 659.25f, 783.99f, 1046.50f,
        783.99f, 659.25f, 783.99f, 1046.50f,
        880.00f, 1046.50f, 1318.51f, 880.00f,
        783.99f, 659.25f, 523.25f, 0.0f
    };
    const int n = (int)(sizeof(notes) / sizeof(notes[0]));
    int totalSamples = (int)(sampleRate * noteDur * n);
    short *data = (short *)malloc(totalSamples * sizeof(short));
    if (!data) return (Sound){0};

    for (int i = 0; i < n; i++) {
        int s0 = (int)(sampleRate * noteDur * i);
        int s1 = (int)(sampleRate * noteDur * (i + 1));
        if (s1 > totalSamples) s1 = totalSamples;
        float f = notes[i];
        for (int j = s0; j < s1; j++) {
            if (f <= 0.0f) { data[j] = 0; continue; }
            float tNote = (float)(j - s0) / sampleRate;
            float local = tNote / noteDur;            // 0..1 within note
            // Attack-sustain-release envelope
            float env;
            if (local < 0.06f)      env = local / 0.06f;
            else if (local > 0.78f) env = (1.0f - local) / 0.22f;
            else                    env = 1.0f;
            if (env < 0.0f) env = 0.0f;

            float vib = 1.0f + 0.005f * sinf(2.0f * PI * 5.0f * tNote);
            float fundamental = sinf(2.0f * PI * f * vib * tNote);
            float harmonic2   = sinf(2.0f * PI * f * 2.0f * tNote) * 0.30f;
            float harmonic3   = sinf(2.0f * PI * f * 3.0f * tNote) * 0.12f;
            float s = (fundamental + harmonic2 + harmonic3) * 0.28f * env;
            if (s >  1.0f) s =  1.0f;
            if (s < -1.0f) s = -1.0f;
            data[j] = (short)(s * 32767.0f);
        }
    }

    Wave w = { .frameCount = (unsigned int)totalSamples, .sampleRate = 44100,
               .sampleSize = 16, .channels = 1, .data = data };
    Sound s = LoadSoundFromWave(w);
    free(data);
    return s;
}

void PlaySfx(const Game *g, Sound snd) {
    if (g->soundEnabled && g->audioReady) PlaySound(snd);
}

static void PlayCelebrationLoop(const Game *g) {
    if (!g->soundEnabled || !g->audioReady) return;
    if (!IsSoundPlaying(g->sndCelebration)) PlaySound(g->sndCelebration);
}

static void StopCelebration(const Game *g) {
    if (g->audioReady) StopSound(g->sndCelebration);
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

// ── Explosion FX: fire particles + expanding shockwave ring ───────────────────
void SpawnExplosionFX(Game *g, Vector2 center) {
    // Heavy burst of fire-colored particles
    int spawned = 0;
    for (int i = 0; i < MAX_PARTICLES && spawned < 28; i++) {
        Particle *p = &g->particles[i];
        if (p->life > 0.0f) continue;
        float angle = (float)GetRandomValue(0, 360) * DEG2RAD;
        float speed = (float)GetRandomValue(180, 520);
        // Fire palette: white-yellow → orange → deep red
        int pick = GetRandomValue(0, 2);
        Color c = (pick == 0) ? (Color){255, 240, 180, 255}
                : (pick == 1) ? (Color){255, 150,  40, 255}
                              : (Color){230,  60,  20, 255};
        p->pos      = center;
        p->vel      = (Vector2){cosf(angle) * speed, sinf(angle) * speed};
        p->color    = c;
        p->maxLife  = (float)GetRandomValue(40, 90) / 100.0f;
        p->life     = p->maxLife;
        p->size     = (float)GetRandomValue(5, 13);
        p->rot      = (float)GetRandomValue(0, 360);
        p->rotSpeed = (float)GetRandomValue(-700, 700);
        spawned++;
    }
    // Two stacked shockwaves: bright fast + wider slow
    for (int s = 0; s < 2; s++) {
        for (int i = 0; i < MAX_SHOCKWAVES; i++) {
            Shockwave *sw = &g->shockwaves[i];
            if (sw->life > 0.0f) continue;
            sw->pos       = center;
            sw->maxLife   = (s == 0) ? 0.35f : 0.55f;
            sw->life      = sw->maxLife;
            sw->maxRadius = (s == 0) ? 78.0f : 130.0f;
            sw->color     = (s == 0) ? (Color){255, 220, 140, 255}
                                     : (Color){255, 110,  40, 255};
            break;
        }
    }
}

void UpdateShockwaves(Game *g, float dt) {
    for (int i = 0; i < MAX_SHOCKWAVES; i++) {
        if (g->shockwaves[i].life > 0.0f) {
            g->shockwaves[i].life -= dt;
            if (g->shockwaves[i].life < 0.0f) g->shockwaves[i].life = 0.0f;
        }
    }
}

void DrawShockwaves(const Game *g) {
    for (int i = 0; i < MAX_SHOCKWAVES; i++) {
        const Shockwave *sw = &g->shockwaves[i];
        if (sw->life <= 0.0f) continue;
        float t  = 1.0f - sw->life / sw->maxLife;  // 0..1 progress
        float r  = sw->maxRadius * t;
        float a  = (1.0f - t) * (1.0f - t);        // ease out
        Color c  = sw->color;
        c.a = (unsigned char)(a * 230);
        DrawRing(sw->pos, r - 3.5f, r + 1.0f, 0, 360, 32, c);
        // Inner soft fill near start
        if (t < 0.45f) {
            Color fill = sw->color;
            fill.a = (unsigned char)((1.0f - t / 0.45f) * 60);
            DrawCircleV(sw->pos, r * 0.9f, fill);
        }
    }
}

// ── Super-bomb FX: dense particles + giant shockwave along a row or column ────
void SpawnLineExplosionFX(Game *g, int col, int row, bool vertical) {
    // Big central burst + 3 shockwaves stacked at the origin
    Vector2 center = GetCellCenter(col, row);
    SpawnExplosionFX(g, center);
    for (int i = 0; i < MAX_SHOCKWAVES; i++) {
        Shockwave *sw = &g->shockwaves[i];
        if (sw->life > 0.0f) continue;
        sw->pos       = center;
        sw->maxLife   = 0.75f;
        sw->life      = sw->maxLife;
        sw->maxRadius = 220.0f;
        sw->color     = (Color){120, 220, 255, 255};   // cyan to distinguish from regular boom
        break;
    }
    // Spawn fast streak particles along the affected line so the row/col is unmistakable
    int spawned = 0;
    int limit   = vertical ? GRID_ROWS : GRID_COLS;
    for (int k = 0; k < limit; k++) {
        int cc = vertical ? col : k;
        int rr = vertical ? k   : row;
        Vector2 cp = GetCellCenter(cc, rr);
        for (int j = 0; j < MAX_PARTICLES && spawned < 80; j++) {
            Particle *p = &g->particles[j];
            if (p->life > 0.0f) continue;
            // Velocity biased along the line for a sweep look
            float vx = vertical ? (float)GetRandomValue(-90, 90)  : (float)GetRandomValue(-380, 380);
            float vy = vertical ? (float)GetRandomValue(-380,380) : (float)GetRandomValue(-90, 90);
            int pick = GetRandomValue(0, 2);
            Color c = (pick == 0) ? (Color){180, 240, 255, 255}
                    : (pick == 1) ? (Color){80,  180, 255, 255}
                                  : (Color){255, 255, 255, 255};
            p->pos      = cp;
            p->vel      = (Vector2){vx, vy};
            p->color    = c;
            p->maxLife  = (float)GetRandomValue(45, 95) / 100.0f;
            p->life     = p->maxLife;
            p->size     = (float)GetRandomValue(4, 11);
            p->rot      = (float)GetRandomValue(0, 360);
            p->rotSpeed = (float)GetRandomValue(-700, 700);
            spawned++;
            break;
        }
    }
}

// ── Super bomb: destroy every brick in a random row OR column ─────────────────
void ExplodeBrickLine(Game *g, int col, int row) {
    bool vertical = (GetRandomValue(0, 1) == 0);
    SpawnLineExplosionFX(g, col, row, vertical);
    PlaySfx(g, g->sndMegaBoom);

    for (int i = 0; i < MAX_BRICKS; i++) {
        Brick *b = &g->bricks[i];
        if (!b->active) continue;
        bool inLine = vertical ? (b->col == col) : (b->row == row);
        if (!inLine) continue;
        bool wasExp = b->explosive;
        bool wasSup = b->superBomb;
        int  bc = b->col, br = b->row;
        Color bCol = b->color;
        b->hp = 0;
        b->active = false;
        g->score += SCORE_PER_BRICK;
        g->roundCombo++;
        g->comboDisplayTimer = 1.8f;
        SpawnBrickParticles(g, bc, br, bCol);
        // Chain reactions: a super in the line triggers another line clear,
        // a regular explosive in the line triggers a neighbour burst.
        // Skip the origin cell to avoid infinite recursion.
        if (bc == col && br == row) continue;
        if (wasSup) ExplodeBrickLine(g, bc, br);
        else if (wasExp) ExplodeBrick(g, bc, br);
    }
}

// ── Explosive brick — destroys 4 neighbours, chains if they're also explosive ──
void ExplodeBrick(Game *g, int col, int row) {
    SpawnExplosionFX(g, GetCellCenter(col, row));
    PlaySfx(g, g->sndBoom);
    // 8 neighbours: 4 orthogonal + 4 diagonal
    const int dx[] = {-1, 1, 0, 0, -1, 1, -1, 1};
    const int dy[] = { 0, 0,-1, 1, -1,-1,  1, 1};
    for (int d = 0; d < 8; d++) {
        int nc = col + dx[d], nr = row + dy[d];
        if (nc < 0 || nc >= GRID_COLS || nr < 0 || nr >= GRID_ROWS) continue;
        for (int i = 0; i < MAX_BRICKS; i++) {
            Brick *b = &g->bricks[i];
            if (!b->active || b->col != nc || b->row != nr) continue;
            // Explosion destroys neighbour bricks outright (not just -1 HP)
            bool wasExp = b->explosive;
            int  bc = b->col, br2 = b->row;
            Color bCol = b->color;
            b->hp = 0;
            b->active = false;            // deactivate BEFORE recursing
            g->score += SCORE_PER_BRICK;
            g->roundCombo++;
            g->comboDisplayTimer = 1.8f;
            SpawnBrickParticles(g, bc, br2, bCol);
            if (wasExp) ExplodeBrick(g, bc, br2);
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
    // Softer scaling: was 1.0× linear + 0.3 per level → now 0.7× linear + 0.15 per level
    int newHP = (int)((float)globalRound * 0.7f * (1.0f + (g->level - 1) * 0.15f))
                + GetRandomValue(0, globalRound / 4 + 1);
    if (newHP < 1) newHP = 1;

    int bricksInRow = GetRandomValue(2, GRID_COLS - 2);
    int cols[GRID_COLS];
    for (int ci = 0; ci < GRID_COLS; ci++) cols[ci] = ci;
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
                // Brick modifiers: rare super-bomb (row/col clear) > normal explosive
                g->bricks[s].superBomb  = false;
                g->bricks[s].explosive  = false;
                if (g->level >= 3 && GetRandomValue(0, 19) == 0) {
                    g->bricks[s].superBomb = true;
                } else if (g->level >= 2 && GetRandomValue(0, 5) == 0) {
                    g->bricks[s].explosive = true;
                }
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
        // Level-up bonus: scales linearly with the level just cleared
        int bonus = LEVEL_UP_BALL_BONUS(g->level - 1);
        g->ballCount      += bonus;
        g->lastLevelBonus  = bonus;
        g->theme        = GetThemeForLevel(g->level);
        g->levelUpTimer = 0.0f;
        // Beating level 20 → victory screen instead of another level
        if (g->level > MAX_SELECTABLE_LEVELS) {
            g->state        = STATE_VICTORY;
            g->victoryTimer = 0.0f;
            PlaySfx(g, g->sndLevelUp);
        } else {
            g->state        = STATE_LEVEL_UP;
            PlaySfx(g, g->sndLevelUp);
        }
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
        g->sndBoom     = GenerateBoom(0.55f, 0.95f);
        g->sndMegaBoom = GenerateMegaBoom(0.95f, 1.0f);
        g->sndCelebration = GenerateCelebrationLoop();
    }

    LoadSaveData(g);
    InitStars(g);

    for (int i = 0; i < 8; i++) SpawnMenuOrb(g);
}

void ResetGame(Game *g, int startLevel) {
    Sound h = g->sndHit, d = g->sndDestroy, pk = g->sndPickup;
    Sound go = g->sndGameOver, re = g->sndRoundEnd, lu = g->sndLevelUp, bm = g->sndBoom, mb = g->sndMegaBoom, ce = g->sndCelebration;
    bool snd = g->soundEnabled, aud = g->audioReady;
    Highscore hs       = g->highscores;
    int unlocked       = g->unlockedLevels;
    Star savedStars[MAX_STARS];
    memcpy(savedStars, g->stars, sizeof(savedStars));

    memset(g, 0, sizeof(Game));
    g->state        = STATE_PLAYING;
    g->round        = 1;
    g->level        = startLevel;
    // Start with the ball count this level should naturally have:
    // base + sum of scaled bonuses for every level already cleared.
    g->ballCount = INITIAL_BALLS;
    for (int k = 1; k < startLevel; k++) g->ballCount += LEVEL_UP_BALL_BONUS(k);
    g->launcherX    = LAUNCHER_X;
    g->aimDir       = (Vector2){0.0f, -1.0f};
    g->soundEnabled = snd;
    g->audioReady   = aud;
    g->sndHit       = h; g->sndDestroy = d; g->sndPickup = pk;
    g->sndGameOver  = go; g->sndRoundEnd = re; g->sndLevelUp = lu; g->sndBoom = bm; g->sndMegaBoom = mb; g->sndCelebration = ce;
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
// Returns true if a slot was found and the ball was launched
static bool FireOneBall(Game *g) {
    for (int i = 0; i < BALL_POOL_SIZE; i++) {
        if (!g->balls[i].active && !g->balls[i].returned) {
            g->balls[i].active   = true;
            g->balls[i].returned = false;
            g->balls[i].position = (Vector2){g->launcherX, LAUNCHER_Y - BALL_RADIUS - 1.0f};
            g->balls[i].velocity = (Vector2){g->aimDir.x * BALL_SPEED, g->aimDir.y * BALL_SPEED};
            memset(&g->balls[i].trail, 0, sizeof(Trail));
            g->activeBallCount++;
            return true;
        }
    }
    return false;
}

void UpdateFiring(Game *g, float dt) {
    if (g->ballsToFire <= 0) return;
    g->launchTimer -= dt;
    if (g->launchTimer <= 0.0f) {
        // Only consume a queued ball if we actually launched it. Otherwise the
        // pool is full this frame — wait without losing the queued ball.
        if (FireOneBall(g)) {
            g->ballsToFire--;
            g->launchTimer = BALL_LAUNCH_DELAY;
        } else {
            g->launchTimer = 0.0f;
        }
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
            if (mX < mY) {
                b->velocity.x = -b->velocity.x;
                // Push ball out of brick along X to prevent re-collision / tunneling between adjacent bricks
                if (oL < oR) b->position.x = cell.x - BALL_RADIUS - 0.5f;
                else         b->position.x = cell.x + cell.width + BALL_RADIUS + 0.5f;
            } else {
                b->velocity.y = -b->velocity.y;
                if (oT < oB) b->position.y = cell.y - BALL_RADIUS - 0.5f;
                else         b->position.y = cell.y + cell.height + BALL_RADIUS + 0.5f;
            }
            // Prevent stuck horizontal balls: enforce minimum |vy|
            float minVy = BALL_SPEED * MIN_VY_RATIO;
            if (fabsf(b->velocity.y) < minVy) {
                float sy = (b->velocity.y >= 0.0f) ? 1.0f : -1.0f;
                b->velocity.y = sy * minVy;
                // Renormalize to preserve speed
                float sp = sqrtf(b->velocity.x * b->velocity.x + b->velocity.y * b->velocity.y);
                if (sp > 0.0001f) {
                    b->velocity.x *= BALL_SPEED / sp;
                    b->velocity.y *= BALL_SPEED / sp;
                }
            }

            br->hp--;
            br->flashTimer = 0.06f;
            br->shakeX     = 3.0f;

            if (br->hp <= 0) {
                bool wasExplosive = br->explosive;
                bool wasSuper     = br->superBomb;
                int  bCol = br->col, bRow = br->row;
                Color bColor = br->color;
                SpawnBrickParticles(g, bCol, bRow, bColor);
                br->active = false;
                g->score  += SCORE_PER_BRICK;
                g->roundCombo++;
                g->comboDisplayTimer = 1.8f;
                PlaySfx(g, g->sndDestroy);
                if (wasSuper)          ExplodeBrickLine(g, bCol, bRow);
                else if (wasExplosive) ExplodeBrick(g, bCol, bRow);
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
    if (g->state == STATE_GAME_OVER || g->state == STATE_LEVEL_UP || g->state == STATE_VICTORY) return;

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
    for (int i = 0; i < MENU_BTN_COUNT; i++) {
        if (CheckCollisionPointRec(mouse, GetMenuBtnRect(i))) { g->menuHover = i; break; }
    }

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        switch (g->menuHover) {
            case 0: ResetGame(g, 1); break;
            case 1: g->state = STATE_LEVEL_SELECT; break;
            case 2: g->state = STATE_HIGHSCORE_VIEW; break;
            case 3: g->state = STATE_CREDITS; break;
            case 4: CloseWindow(); break;
        }
    }
    if (IsKeyPressed(KEY_ENTER))  ResetGame(g, 1);
    if (IsKeyPressed(KEY_L))      g->state = STATE_LEVEL_SELECT;
    if (IsKeyPressed(KEY_H))      g->state = STATE_HIGHSCORE_VIEW;
    if (IsKeyPressed(KEY_C))      g->state = STATE_CREDITS;
    if (IsKeyPressed(KEY_M))      g->soundEnabled = !g->soundEnabled;
    // ESC on the main menu intentionally does nothing — Q / window-close quits
    if (IsKeyPressed(KEY_Q))      CloseWindow();
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
    // Reset level progress (locks everything past level 1)
    if (IsKeyPressed(KEY_R)) {
        g->unlockedLevels = 1;
        WriteSaveData(g);
    }
    // Unlock ALL levels at once
    if (IsKeyPressed(KEY_O)) {
        g->unlockedLevels = MAX_SELECTABLE_LEVELS;
        WriteSaveData(g);
    }
}

// ── Update: Paused ────────────────────────────────────────────────────────────
void UpdatePaused(Game *g) {
    if (IsKeyPressed(KEY_P))      g->state = STATE_PLAYING;
    if (IsKeyPressed(KEY_ESCAPE)) g->state = STATE_MENU;
    if (IsKeyPressed(KEY_Q))      g->state = STATE_MENU;
    if (IsKeyPressed(KEY_M))      g->soundEnabled = !g->soundEnabled;
}

// ── Update: Playing ───────────────────────────────────────────────────────────
void UpdatePlaying(Game *g, float dt) {
    if (IsKeyPressed(KEY_ESCAPE)) {
        g->state = STATE_MENU;
        return;
    }
    if (IsKeyPressed(KEY_P)) {
        g->state = STATE_PAUSED;
        return;
    }

    g->launcherPulse += dt * 3.0f;
    UpdateParticles(g, dt);
    UpdateShockwaves(g, dt);

    // Safety: if we somehow ended up in an active round with absolutely nothing
    // happening (no balls queued, none flying, none on the field), force the
    // round to end so the player can fire again. Guards against any edge case
    // where roundActive stays stuck true.
    if (g->roundActive && g->ballsToFire == 0 && g->activeBallCount == 0) {
        bool any = false;
        for (int i = 0; i < BALL_POOL_SIZE; i++) {
            if (g->balls[i].active) { any = true; break; }
        }
        if (!any) CheckRoundEnd(g);
    }

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
                g->roundElapsed      = 0.0f;
                for (int i = 0; i < BALL_POOL_SIZE; i++) { g->balls[i].active = false; g->balls[i].returned = false; }
            }
        }
    } else {
        // DOWN arrow: instantly recall every ball (cancel pending fires too)
        if (IsKeyPressed(KEY_DOWN)) {
            g->ballsToFire = 0;
            for (int i = 0; i < BALL_POOL_SIZE; i++) {
                Ball *bb = &g->balls[i];
                if (bb->active) {
                    bb->active   = false;
                    bb->returned = true;
                    if (!g->firstReturned) { g->firstReturned = true; g->firstReturnX = bb->position.x; }
                    g->returnedBallCount++;
                }
            }
            g->activeBallCount = 0;
        }
        // RIGHT arrow held = fast-forward; multiplier grows every 5 levels
        // (L1-4: 2x, L5-9: 3x, L10-14: 4x, L15-19: 5x, L20: 6x)
        int   ffMul = 2 + g->level / 5;
        float pdt = IsKeyDown(KEY_RIGHT) ? dt * (float)ffMul : dt;
        UpdateFiring(g, pdt);
        UpdateBallPhysics(g, pdt);
        g->roundElapsed += pdt;
        // Failsafe: if a round drags too long after all balls launched, force-return stragglers
        if (g->ballsToFire == 0 && g->activeBallCount > 0 && g->roundElapsed > ROUND_TIMEOUT_SEC) {
            for (int i = 0; i < BALL_POOL_SIZE; i++) {
                Ball *bb = &g->balls[i];
                if (bb->active) {
                    bb->active   = false;
                    bb->returned = true;
                    if (!g->firstReturned) { g->firstReturned = true; g->firstReturnX = bb->position.x; }
                    g->returnedBallCount++;
                }
            }
            g->activeBallCount = 0;
        }
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

// ── Update: Credits ───────────────────────────────────────────────────────────
void UpdateCredits(Game *g, float dt) {
    g->creditsTimer += dt;
    UpdateParticles(g, dt);
    PlayCelebrationLoop(g);
    // Keep a few drifting menu orbs alive in the background
    int orbCount = 0;
    for (int i = 0; i < MAX_PARTICLES; i++)
        if (g->particles[i].life > 0.0f && g->particles[i].size >= 16) orbCount++;
    if (orbCount < 6 && GetRandomValue(0, 45) == 0) SpawnMenuOrb(g);

    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_ENTER) ||
        IsKeyPressed(KEY_BACKSPACE) || IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        StopCelebration(g);
        g->state = STATE_MENU;
    }
    if (IsKeyPressed(KEY_M)) {
        g->soundEnabled = !g->soundEnabled;
        if (!g->soundEnabled) StopCelebration(g);
    }
}

// ── Update: Victory (after clearing level 20) ─────────────────────────────────
void UpdateVictory(Game *g, float dt) {
    g->victoryTimer += dt;
    UpdateParticles(g, dt);
    UpdateShockwaves(g, dt);
    PlayCelebrationLoop(g);
    // Continuous fireworks: spawn an explosion FX in a random spot every so often
    if (GetRandomValue(0, 8) == 0) {
        Vector2 p = {(float)GetRandomValue(60, SCREEN_W - 60),
                     (float)GetRandomValue(120, SCREEN_H - 200)};
        SpawnExplosionFX(g, p);
    }
    // Save highscore opportunity if applicable, then allow exit
    if (g->victoryTimer >= 1.5f &&
        (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_ESCAPE) ||
         IsMouseButtonPressed(MOUSE_LEFT_BUTTON))) {
        StopCelebration(g);
        if (IsNewHighscore(&g->highscores, g->score)) {
            g->inputLen = 0;
            memset(g->inputName, 0, NAME_LEN);
            g->state = STATE_HIGHSCORE_ENTRY;
        } else {
            g->state = STATE_MENU;
        }
    }
    if (IsKeyPressed(KEY_M)) {
        g->soundEnabled = !g->soundEnabled;
        if (!g->soundEnabled) StopCelebration(g);
    }
}

// ── Update: Highscore View ────────────────────────────────────────────────────
void UpdateHighscoreView(Game *g) {
    if (IsKeyPressed(KEY_M)) g->soundEnabled = !g->soundEnabled;
    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_ENTER)) g->state = STATE_MENU;
}

// ── Draw: Background ──────────────────────────────────────────────────────────
void DrawBackground(const Game *g) {
    // Deep gradient (48 strips for ultra-smooth blending)
    int strips = 48;
    for (int i = 0; i < strips; i++) {
        float t0 = (float)i / strips, t1 = (float)(i + 1) / strips;
        Color c0 = BlendColor(g->theme.bgA, g->theme.bgB, t0);
        Color c1 = BlendColor(g->theme.bgA, g->theme.bgB, t1);
        int y0 = (int)(t0 * SCREEN_H), y1 = (int)(t1 * SCREEN_H);
        DrawRectangleGradientV(0, y0, SCREEN_W, y1 - y0, c0, c1);
    }

    // Stars
    DrawStars(g);

    // Nebula blobs — 6 large soft circles for depth
    float t = (float)GetTime();
    Color nebA = g->theme.accentA; nebA.a = 14;
    Color nebB = g->theme.accentB; nebB.a = 10;
    Color nebC = g->theme.accentA; nebC.a = 7;
    DrawCircleV((Vector2){75.0f  + sinf(t * 0.13f) * 22.0f, 180.0f + cosf(t * 0.09f) * 12.0f}, 160.0f, nebA);
    DrawCircleV((Vector2){465.0f + cosf(t * 0.11f) * 22.0f, 500.0f + sinf(t * 0.08f) * 14.0f}, 145.0f, nebB);
    DrawCircleV((Vector2){270.0f + sinf(t * 0.07f) * 18.0f, 360.0f}, 125.0f, nebC);
    DrawCircleV((Vector2){400.0f + cosf(t * 0.16f) * 16.0f, 160.0f}, 90.0f, nebB);
    DrawCircleV((Vector2){120.0f, 540.0f + sinf(t * 0.12f) * 18.0f}, 100.0f, nebA);
    DrawCircleV((Vector2){470.0f + sinf(t * 0.09f) * 14.0f, 320.0f}, 80.0f, nebC);

    // Shooting stars — 4 periodic streaks
    for (int si = 0; si < 4; si++) {
        float cycle    = 9.0f + si * 4.3f;
        float offset   = si * 2.7f;
        float progress = fmodf(t * 0.35f + offset, cycle) / cycle;
        if (progress > 0.12f) continue;
        float streak = progress / 0.12f;
        float sx = fmodf(80.0f + si * 147.5f + t * 5.0f, (float)SCREEN_W + 80.0f) - 40.0f;
        float sy = 30.0f + si * 65.0f;
        float len = (1.0f - streak) * 70.0f;
        float alpha = (1.0f - streak) * 0.75f;
        DrawLineEx((Vector2){sx, sy}, (Vector2){sx + len * 0.85f, sy + len * 0.28f}, 2.2f, Fade(WHITE, alpha));
        DrawLineEx((Vector2){sx, sy}, (Vector2){sx + len * 0.5f,  sy + len * 0.16f}, 1.0f, Fade(WHITE, alpha * 0.4f));
    }

    // Subtle grid-area floor glow
    float gridBottom = GRID_OFFSET_Y + GRID_ROWS * CELL_SIZE;
    DrawRectangleGradientV(
        (int)GRID_OFFSET_X, (int)(gridBottom - 18), (int)(GRID_COLS * CELL_SIZE), 18,
        (Color){0,0,0,0}, (Color){g->theme.dangerLine.r, g->theme.dangerLine.g, g->theme.dangerLine.b, 22});

    // Vignette
    DrawRectangleGradientH(0, 0, 75, SCREEN_H, (Color){0,0,0,100}, (Color){0,0,0,0});
    DrawRectangleGradientH(SCREEN_W-75, 0, 75, SCREEN_H, (Color){0,0,0,0}, (Color){0,0,0,100});
    DrawRectangleGradientV(0, 0, SCREEN_W, 65, (Color){0,0,0,70}, (Color){0,0,0,0});
    DrawRectangleGradientV(0, SCREEN_H-65, SCREEN_W, 65, (Color){0,0,0,0}, (Color){0,0,0,90});

    // Subtle scanlines
    for (int y = 0; y < SCREEN_H; y += 4)
        DrawRectangle(0, y, SCREEN_W, 1, (Color){0,0,0,9});
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
    // Glass top bar — gradient + frosted edge
    DrawRectangleGradientV(0, 0, SCREEN_W, (int)HUD_HEIGHT, (Color){0,0,0,185}, (Color){0,0,0,75});
    // Subtle inner top highlight (glass rim)
    DrawRectangle(0, 0, SCREEN_W, 1, (Color){255,255,255,18});
    // Accent bottom line
    Color ac = g->theme.accentA; ac.a = 55;
    DrawLine(0, (int)HUD_HEIGHT, SCREEN_W, (int)HUD_HEIGHT, ac);
    DrawLine(0, (int)HUD_HEIGHT+1, SCREEN_W, (int)HUD_HEIGHT+1, (Color){255,255,255,12});

    // Level + round label
    char buf[64];
    int roundInLevel = g->round > 0 ? g->round - 1 : 0;
    sprintf(buf, "LV %d", g->level);
    DrawText(buf, 10, 8, 16, Fade(g->theme.accentA, 0.90f));
    sprintf(buf, "R %d/%d", roundInLevel > 0 ? roundInLevel : 1, ROUNDS_PER_LEVEL);
    DrawText(buf, 10, 27, 13, (Color){150,170,210,200});

    // Centered score
    sprintf(buf, "%d", g->score);
    int sw = MeasureText(buf, 26);
    DrawText(buf, (SCREEN_W-sw)/2 + 1, 11, 26, Fade(BLACK, 0.55f));
    DrawText(buf, (SCREEN_W-sw)/2, 10, 26, WHITE);

    // Round progress bar (thin, below score)
    float rProg = (float)(roundInLevel > 0 ? roundInLevel : 1) / ROUNDS_PER_LEVEL;
    if (rProg > 1.0f) rProg = 1.0f;
    int barX = (SCREEN_W - 90) / 2, barY = 38, barW = 90, barH = 3;
    DrawRectangle(barX, barY, barW, barH, (Color){0,0,0,80});
    Color progCol = g->theme.accentA; progCol.a = 200;
    DrawRectangle(barX, barY, (int)(barW * rProg), barH, progCol);
    DrawRectangle(barX, barY, (int)(barW * rProg), 1, Fade(WHITE, 0.35f));

    // Ball count with mini circles
    sprintf(buf, "x%d", g->ballCount);
    int bw = MeasureText(buf, 18);
    DrawText(buf, SCREEN_W - bw - 10, 8, 18, WHITE);
    int show = g->ballCount < 6 ? g->ballCount : 6;
    for (int i = 0; i < show; i++) {
        Vector2 bp = {(float)(SCREEN_W - bw - 26 - i * 12), 27.0f};
        DrawCircleV(bp, 4.0f, Fade(g->theme.ballGlow, 0.80f));
        DrawCircleV(bp, 2.0f, WHITE);
    }

    const char *sm = g->soundEnabled ? "[M]" : "[M]OFF";
    DrawText(sm, 4, SCREEN_H - 18, 12, (Color){110,110,140,170});
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

        bool flashing = (b->flashTimer > 0.0f);
        // Explosive bricks ignore the per-HP palette and use an unmistakable
        // pulsing color so the player can plan chains.
        Color baseCol = b->color;
        if (b->superBomb) {
            // Super-bomb: rapid magenta↔cyan pulse — visually distinct from regular explosive
            float sp = 0.5f + 0.5f * sinf((float)GetTime() * 8.0f + b->col * 1.1f + b->row * 0.7f);
            baseCol = (Color){
                (unsigned char)(120 + (unsigned char)(80 * sp)),
                (unsigned char)(60  + (unsigned char)(140 * sp)),
                (unsigned char)(220 + (unsigned char)(35 * sp)),
                255
            };
        } else if (b->explosive) {
            float ep = 0.5f + 0.5f * sinf((float)GetTime() * 5.5f + b->col * 0.9f + b->row * 0.6f);
            baseCol = (Color){
                (unsigned char)(230 + (unsigned char)(25 * ep)),
                (unsigned char)(80  + (unsigned char)(60 * ep)),
                (unsigned char)(20  + (unsigned char)(20 * ep)),
                255
            };
        }
        Color col = flashing ? WHITE : baseCol;
        float hpRatio = (b->maxHp > 1) ? (float)b->hp / b->maxHp : 1.0f;

        // Drop shadow (deeper)
        DrawRectangleRounded((Rectangle){base.x+3, base.y+5, base.width, base.height},
                             0.22f, 6, (Color){0,0,0,120});
        // Outer subtle glow ring when brick is nearly full health
        if (!flashing && hpRatio > 0.7f) {
            Color glowRing = col; glowRing.a = 18;
            DrawRectangleRoundedLines((Rectangle){base.x-3, base.y-3, base.width+6, base.height+6}, 0.22f, 6, glowRing);
        }

        // Main body
        DrawRectangleRounded(base, 0.22f, 6, col);

        // Inner gradient: lighter top → dark bottom (metallic depth)
        Color topTint = col;
        topTint.r = (unsigned char)fminf(255, col.r + 55);
        topTint.g = (unsigned char)fminf(255, col.g + 55);
        topTint.b = (unsigned char)fminf(255, col.b + 55);
        DrawRectangleGradientV(
            (int)(base.x + 2), (int)(base.y + 2),
            (int)(base.width - 4), (int)((base.height - 4) * 0.55f),
            Fade(topTint, 0.40f), Fade(topTint, 0.0f));

        // Inner bottom shadow for depth
        DrawRectangleGradientV(
            (int)(base.x + 2), (int)(base.y + base.height * 0.65f),
            (int)(base.width - 4), (int)(base.height * 0.30f),
            Fade(BLACK, 0.0f), Fade(BLACK, 0.22f));

        // Glass highlight — wide top strip + small glint dot
        DrawRectangleRounded(
            (Rectangle){base.x+2, base.y+2, base.width-4, (base.height-4)*0.30f},
            0.22f, 4, Fade(WHITE, 0.24f));
        DrawCircleV((Vector2){base.x + 8.0f, base.y + 6.0f}, 3.0f, Fade(WHITE, 0.35f));

        // Glowing border (brighter on flash)
        Color borderGlow = col; borderGlow.a = flashing ? 255 : 200;
        DrawRectangleRoundedLines(base, 0.22f, 6, Fade(borderGlow, flashing ? 1.0f : 0.75f));

        // Crack overlay at low HP (< 30%)
        if (!flashing && hpRatio < 0.30f && b->maxHp > 1) {
            float cx = base.x + base.width  * 0.5f;
            float cy = base.y + base.height * 0.5f;
            Color crk = (Color){0,0,0,120};
            DrawLineEx((Vector2){cx - 10, cy - 12}, (Vector2){cx + 4,  cy + 6},  1.5f, crk);
            DrawLineEx((Vector2){cx + 4,  cy + 6},  (Vector2){cx + 12, cy + 14}, 1.5f, crk);
            DrawLineEx((Vector2){cx - 4,  cy + 2},  (Vector2){cx - 12, cy + 14}, 1.5f, crk);
            DrawLineEx((Vector2){cx + 8,  cy - 10}, (Vector2){cx - 2,  cy + 4},  1.2f, crk);
        }

        // HP bar
        if (b->maxHp > 1) {
            float ratio = hpRatio;
            float barW  = (base.width - 8) * ratio;
            DrawRectangle((int)(base.x+4), (int)(base.y + base.height - 7), (int)(base.width-8), 4, (Color){0,0,0,130});
            Color barCol = (ratio > 0.5f) ? (Color){60,245,90,230} : (ratio > 0.25f) ? (Color){255,195,35,230} : (Color){255,55,55,230};
            DrawRectangle((int)(base.x+4), (int)(base.y + base.height - 7), (int)barW, 4, barCol);
            // Bar highlight
            DrawRectangle((int)(base.x+4), (int)(base.y + base.height - 7), (int)barW, 1, Fade(WHITE, 0.35f));
        }

        // HP text
        char hpBuf[16];
        snprintf(hpBuf, sizeof(hpBuf), "%d", b->hp);
        int fs = (b->hp >= 10000) ? 11 : (b->hp >= 1000) ? 13 : (b->hp >= 100) ? 15 : (b->hp >= 10) ? 18 : 22;
        int tw = MeasureText(hpBuf, fs);
        int tx = (int)(base.x + base.width/2 - tw/2);
        int ty = (int)(base.y + base.height/2 - fs/2 - 3);
        DrawText(hpBuf, tx+1, ty+2, fs, (Color){0,0,0,200});
        DrawText(hpBuf, tx, ty, fs, WHITE);

        // Super-bomb indicator — magenta/cyan glow + crosshair (row+column arrows)
        if (b->superBomb) {
            float pulse = 0.5f + 0.5f * sinf((float)GetTime() * 7.0f + b->col * 1.3f);
            Color sCol  = (Color){170, 90, 255, 255};
            // Thick neon border + 4-layer outer glow
            DrawRectangleRoundedLines(base, 0.22f, 6, Fade(sCol, 0.90f + pulse * 0.10f));
            DrawRectangleRoundedLines(
                (Rectangle){base.x-3, base.y-3, base.width+6, base.height+6},
                0.22f, 6, Fade(sCol, 0.45f + pulse * 0.45f));
            DrawRectangleRoundedLines(
                (Rectangle){base.x-7, base.y-7, base.width+14, base.height+14},
                0.22f, 6, Fade(sCol, 0.18f + pulse * 0.22f));
            DrawRectangleRoundedLines(
                (Rectangle){base.x-12, base.y-12, base.width+24, base.height+24},
                0.22f, 6, Fade(sCol, 0.06f + pulse * 0.10f));

            // Crosshair icon — small, top-right corner so the HP number stays readable
            float icx = base.x + base.width - 11.0f;
            float icy = base.y + 11.0f;
            float arm = 6.5f;
            Color iconCol = (Color){255, 240, 255, 255};
            // Dark backing disc
            DrawCircleV((Vector2){icx + 1, icy + 1}, arm + 1.5f, (Color){0,0,0,140});
            DrawCircleV((Vector2){icx, icy}, arm + 1.0f, (Color){40, 10, 70, 255});
            // Crosshair arms (full row+column hint)
            DrawLineEx((Vector2){icx - arm, icy}, (Vector2){icx + arm, icy}, 1.8f, iconCol);
            DrawLineEx((Vector2){icx, icy - arm}, (Vector2){icx, icy + arm}, 1.8f, iconCol);
            // Arrow tips
            float at = 2.5f;
            DrawTriangle((Vector2){icx + arm, icy}, (Vector2){icx + arm - at, icy - at}, (Vector2){icx + arm - at, icy + at}, iconCol);
            DrawTriangle((Vector2){icx - arm, icy}, (Vector2){icx - arm + at, icy + at}, (Vector2){icx - arm + at, icy - at}, iconCol);
            DrawTriangle((Vector2){icx, icy - arm}, (Vector2){icx + at, icy - arm + at}, (Vector2){icx - at, icy - arm + at}, iconCol);
            DrawTriangle((Vector2){icx, icy + arm}, (Vector2){icx - at, icy + arm - at}, (Vector2){icx + at, icy + arm - at}, iconCol);
            // Pulsing center
            DrawCircleV((Vector2){icx, icy}, 2.0f + pulse * 1.1f, Fade(iconCol, 0.85f));
        }
        // Explosive indicator — thick pulsing border + outer glow rings + bomb icon
        else if (b->explosive) {
            float pulse = 0.5f + 0.5f * sinf((float)GetTime() * 6.0f + b->col * 1.3f);
            Color expCol = (Color){255, 130, 25, 255};

            // Thick neon border
            DrawRectangleRoundedLines(base, 0.22f, 6, Fade(expCol, 0.85f + pulse * 0.15f));
            // Outer pulsing glow rings
            DrawRectangleRoundedLines(
                (Rectangle){base.x-3, base.y-3, base.width+6, base.height+6},
                0.22f, 6, Fade(expCol, 0.40f + pulse * 0.40f));
            DrawRectangleRoundedLines(
                (Rectangle){base.x-7, base.y-7, base.width+14, base.height+14},
                0.22f, 6, Fade(expCol, 0.12f + pulse * 0.18f));
            DrawRectangleRoundedLines(
                (Rectangle){base.x-11, base.y-11, base.width+22, base.height+22},
                0.22f, 6, Fade(expCol, 0.05f + pulse * 0.08f));

            // Big bomb icon (top-right corner): dark circle body + fuse + sparkle
            float bx = base.x + base.width  - 11.0f;
            float by = base.y + 11.0f;
            float br = 8.0f;
            DrawCircleV((Vector2){bx + 1, by + 2}, br, (Color){0, 0, 0, 140});      // shadow
            DrawCircleV((Vector2){bx, by}, br, (Color){25, 25, 30, 255});            // bomb body
            DrawCircleLines((int)bx, (int)by, br, (Color){0, 0, 0, 200});
            // Highlight on bomb
            DrawCircleV((Vector2){bx - 2.5f, by - 2.5f}, 2.2f, (Color){180, 180, 200, 220});
            // Fuse
            DrawLineEx((Vector2){bx + br * 0.55f, by - br * 0.55f},
                       (Vector2){bx + br * 1.15f, by - br * 1.25f}, 1.6f, (Color){50, 30, 10, 255});
            // Sparkle at fuse tip (pulses)
            float sp = 1.6f + pulse * 1.4f;
            Color spark = (Color){255, (unsigned char)(180 + pulse * 60), 50, 255};
            DrawCircleV((Vector2){bx + br * 1.2f, by - br * 1.3f}, sp + 1.2f, Fade(spark, 0.55f));
            DrawCircleV((Vector2){bx + br * 1.2f, by - br * 1.3f}, sp, spark);
            DrawCircleV((Vector2){bx + br * 1.2f, by - br * 1.3f}, sp * 0.45f, WHITE);
        }
    }
}

// ── Draw: Pickups ─────────────────────────────────────────────────────────────
void DrawPickups(const Game *g) {
    for (int i = 0; i < MAX_PICKUPS; i++) {
        const BallPickup *p = &g->pickups[i];
        if (!p->active) continue;
        Vector2 center = GetCellCenter(p->col, p->row);
        float t     = (float)GetTime();
        float pulse = sinf(t * 4.5f + p->pulseT);
        float r     = CELL_SIZE * 0.20f + pulse * 2.5f;
        float rot   = t * 45.0f + p->pulseT * 20.0f;  // slow rotation

        Color gold = (Color){255,215,25,255};

        // Wide outer glow rings
        DrawCircleV(center, r + 18, Fade(gold, 0.03f));
        DrawCircleV(center, r + 12, Fade(gold, 0.07f));
        DrawCircleV(center, r +  7, Fade(gold, 0.15f));
        DrawCircleV(center, r +  3, Fade(gold, 0.28f));

        // Orb body with gradient
        DrawCircleV(center, r, gold);
        DrawCircleGradient((int)center.x, (int)center.y, r,
                           Fade(WHITE, 0.30f), Fade(gold, 0.0f));

        // Diamond shape overlay (rotated square)
        float ds = r * 0.80f;
        DrawRectanglePro(
            (Rectangle){center.x, center.y, ds * 1.4f, ds * 1.4f},
            (Vector2){ds * 0.7f, ds * 0.7f},
            rot, Fade(WHITE, 0.08f));

        // Shine glint
        DrawCircleV((Vector2){center.x - r*0.28f, center.y - r*0.28f}, r*0.30f, Fade(WHITE, 0.60f));
        DrawCircleV((Vector2){center.x - r*0.18f, center.y - r*0.18f}, r*0.14f, WHITE);

        // "+1" label
        const char *plus = "+1";
        int tw = MeasureText(plus, 13);
        DrawText(plus, (int)center.x - tw/2 + 1, (int)center.y - 7 + 1, 13, Fade(BLACK, 0.6f));
        DrawText(plus, (int)center.x - tw/2, (int)center.y - 7, 13, (Color){30,20,0,255});
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

        // Trail — tapered with color tint from theme
        for (int t = 0; t < b->trail.count; t++) {
            int idx  = (b->trail.head - 1 - t + TRAIL_LEN) % TRAIL_LEN;
            float tf = 1.0f - (float)(t + 1) / (TRAIL_LEN + 1);
            float tr = BALL_RADIUS * tf * 0.75f;
            Color tc = g->theme.ballGlow;
            tc.a = (unsigned char)(tf * tf * 150.0f);
            DrawCircleV(b->trail.pos[idx], tr, tc);
            // Inner white core of trail
            Color tc2 = WHITE;
            tc2.a = (unsigned char)(tf * tf * 55.0f);
            DrawCircleV(b->trail.pos[idx], tr * 0.45f, tc2);
        }

        // Chromatic aberration glow (red offset left, blue offset right)
        DrawCircleV((Vector2){b->position.x - 2.2f, b->position.y}, BALL_RADIUS + 2.0f, (Color){255,60,60,22});
        DrawCircleV((Vector2){b->position.x + 2.2f, b->position.y}, BALL_RADIUS + 2.0f, (Color){60,130,255,22});

        // Wide outer glow halo
        Color go = g->theme.ballGlow; go.a = 20;
        Color gm = g->theme.ballGlow; gm.a = 42;
        Color gi = g->theme.ballGlow; gi.a = 70;
        DrawCircleV(b->position, BALL_RADIUS + 11.0f, go);
        DrawCircleV(b->position, BALL_RADIUS +  5.5f, gm);
        DrawCircleV(b->position, BALL_RADIUS +  2.5f, gi);

        // Ball body — slightly off-white for premium feel
        DrawCircleV(b->position, BALL_RADIUS, (Color){245, 248, 255, 255});
        // Bottom shadow on ball
        DrawCircleV((Vector2){b->position.x + 1.5f, b->position.y + 2.5f}, BALL_RADIUS * 0.65f,
                    (Color){160,170,200,40});
        // Glint (large soft + sharp point)
        DrawCircleV((Vector2){b->position.x - 3.2f, b->position.y - 3.2f}, 3.5f, Fade(WHITE, 0.75f));
        DrawCircleV((Vector2){b->position.x - 2.2f, b->position.y - 2.2f}, 1.5f, WHITE);
    }
}

// ── Draw: Launcher ────────────────────────────────────────────────────────────
void DrawLauncher(const Game *g) {
    float px = g->launcherX, py = LAUNCHER_Y;
    float pulse  = sinf(g->launcherPulse) * 0.5f + 0.5f;
    float pulse2 = sinf(g->launcherPulse * 1.7f + 1.0f) * 0.5f + 0.5f;
    Color rc = g->theme.ballGlow;

    // Floor glow beneath launcher
    DrawCircleV((Vector2){px, py + 6.0f}, 28.0f + pulse * 6.0f, (Color){rc.r, rc.g, rc.b, (unsigned char)(8 + pulse * 10)});

    // Outer expanding ring
    DrawCircleLines((int)px, (int)py, 34.0f + pulse * 10.0f, Fade(rc, 0.06f + pulse * 0.06f));
    // Mid rings
    DrawCircleLines((int)px, (int)py, 24.0f + pulse2 * 5.0f, Fade(rc, 0.13f + pulse2 * 0.08f));
    DrawCircleLines((int)px, (int)py, 17.0f, Fade(rc, 0.28f));
    DrawCircleLines((int)px, (int)py, 12.5f, Fade(rc, 0.18f));
    DrawCircleLines((int)px, (int)py,  8.5f, Fade(rc, 0.30f));

    // Aim arrow with shadow
    if (!g->roundActive) {
        Vector2 tip  = {px + g->aimDir.x * 28.0f, py + g->aimDir.y * 28.0f};
        Vector2 perp = {-g->aimDir.y, g->aimDir.x};
        Vector2 lft  = {px + perp.x * 9.0f, py + perp.y * 9.0f};
        Vector2 rgt  = {px - perp.x * 9.0f, py - perp.y * 9.0f};
        // Shadow
        DrawTriangle((Vector2){tip.x+1, tip.y+2}, (Vector2){lft.x+1, lft.y+2}, (Vector2){rgt.x+1, rgt.y+2},
                     (Color){0,0,0,80});
        DrawTriangle(tip, lft, rgt, rc);
        DrawTriangleLines(tip, lft, rgt, Fade(WHITE, 0.50f));
    }

    // Core — layered for 3D feel
    DrawCircleV((Vector2){px, py}, 9.5f, Fade(rc, 0.45f));
    DrawCircleV((Vector2){px, py}, 8.0f, WHITE);
    DrawCircleV((Vector2){px, py}, 5.5f, rc);
    // Highlight
    DrawCircleV((Vector2){px - 2.2f, py - 2.2f}, 2.2f, Fade(WHITE, 0.90f));
    DrawCircleV((Vector2){px - 1.5f, py - 1.5f}, 1.0f, WHITE);

    // Ball count below launcher
    char cntBuf[16];
    sprintf(cntBuf, "x%d", g->ballCount);
    int tw = MeasureText(cntBuf, 15);
    DrawText(cntBuf, (int)(px - tw/2) + 1, (int)(py + 14), 15, Fade(BLACK, 0.55f));
    DrawText(cntBuf, (int)(px - tw/2),     (int)(py + 13), 15, Fade(WHITE, 0.75f));
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
    DrawShockwaves(g);
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
    } else {
        // Recall + speed hints while balls are flying
        int   ffMul = 2 + g->level / 5;
        char rh[64];
        snprintf(rh, sizeof(rh), "[DOWN] RECALL    [RIGHT] %dx SPEED", ffMul);
        int rhw = MeasureText(rh, 12);
        DrawText(rh, (SCREEN_W - rhw)/2, SCREEN_H - 26, 12, Fade(LIGHTGRAY, 0.45f));
        if (IsKeyDown(KEY_RIGHT)) {
            char fx[8];
            snprintf(fx, sizeof(fx), "%dx", ffMul);
            int fxw = MeasureText(fx, 28);
            float p = 0.5f + 0.5f * sinf((float)GetTime() * 12.0f);
            DrawText(fx, SCREEN_W - fxw - 12 + 2, 56 + 2, 28, Fade(BLACK, 0.6f));
            DrawText(fx, SCREEN_W - fxw - 12,     56,     28, Fade((Color){255, 220, 60, 255}, 0.85f + p * 0.15f));
        }
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
        {"[P]   Resume",         356},
        {"[ESC] Back to Menu",   396},
        {"[M]   Toggle Sound",   436},
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
    const char *t1 = "the legend of";
    int tw1 = MeasureText(t1, 28);
    DrawText(t1, (SCREEN_W-tw1)/2 + 2, 110, 28, Fade(BLACK, 0.75f));
    DrawText(t1, (SCREEN_W-tw1)/2, 107, 28, ColorFromHSV(hue, 0.55f, 0.95f));

    const char *t2 = "NABCAN";
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
    Color accentPlay    = (Color){60, 210, 255, 255};
    Color accentLvSel   = (Color){100, 130, 230, 255};
    Color accentScores  = (Color){100, 130, 230, 255};
    Color accentCredits = (Color){200, 130, 230, 255};
    Color accentQuit    = (Color){100, 100, 140, 255};
    Color accents[]     = {accentPlay, accentLvSel, accentScores, accentCredits, accentQuit};
    bool  primaries[]   = {true, false, false, false, false};
    const char *labels[] = {"PLAY", "SELECT LEVEL", "HIGH SCORES", "CREDITS", "QUIT"};

    for (int i = 0; i < MENU_BTN_COUNT; i++) {
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
    const char *hint = "[L] Levels   [H] Scores   [C] Credits   [Q] Quit";
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
        Theme th     = THEMES[lv - 1]; // exact 1:1 since NUM_THEMES=20

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

    // Cheat / reset hints
    const char *oh = "[O] Unlock All";
    DrawText(oh, 10, SCREEN_H - 16, 11, Fade(WHITE, 0.22f));
    const char *rh = "[R] Reset Progress";
    int rhw = MeasureText(rh, 11);
    DrawText(rh, SCREEN_W - rhw - 10, SCREEN_H - 16, 11, Fade(WHITE, 0.22f));
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

    char bonusBuf[16];
    snprintf(bonusBuf, sizeof(bonusBuf), "+%d BALLS", g->lastLevelBonus > 0 ? g->lastLevelBonus : 0);
    int bbw = MeasureText(bonusBuf, 20);
    DrawText(bonusBuf, (SCREEN_W - bbw)/2 + 1, 397, 20, Fade(BLACK, alpha * 0.6f));
    DrawText(bonusBuf, (SCREEN_W - bbw)/2,     396, 20, Fade((Color){255, 215, 60, 255}, alpha));

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
    int reachedRound = (g->round > 1) ? g->round - 1 : 1;
    int globalRound  = (g->level - 1) * ROUNDS_PER_LEVEL + reachedRound;
    sprintf(buf, "Level %d  —  Round %d", g->level, globalRound);
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

// ── Draw: Credits ─────────────────────────────────────────────────────────────
static void DrawCreditCard(int yCenter, const char *name, const char *role,
                           float t, float phase, Color accent) {
    int cardW = 360;
    int cardX = (SCREEN_W - cardW) / 2;
    int cardH = 76;
    int cardY = yCenter - cardH / 2;

    // Slide-in offset
    float slide = (1.0f - t) * 60.0f;
    float xOff  = sinf(phase) * 4.0f;  // gentle horizontal bob
    cardX += (int)(slide + xOff);

    // Outer glow
    Color glow = accent; glow.a = 30;
    DrawRectangleRounded((Rectangle){cardX - 5, cardY - 5, cardW + 10, cardH + 10}, 0.25f, 8, glow);

    // Card background
    DrawRectangleRounded((Rectangle){cardX, cardY, cardW, cardH}, 0.25f, 8, (Color){15, 18, 38, 220});
    DrawRectangleRoundedLines((Rectangle){cardX, cardY, cardW, cardH}, 0.25f, 8, Fade(accent, 0.85f));
    // Top glass strip
    DrawRectangleRounded((Rectangle){cardX + 3, cardY + 3, cardW - 6, (cardH - 6) * 0.35f},
                         0.25f, 6, Fade(WHITE, 0.10f));

    // Decorative left orb
    float ox = cardX + 32.0f;
    float oy = cardY + cardH * 0.5f;
    DrawCircleV((Vector2){ox, oy}, 18.0f, Fade(accent, 0.20f));
    DrawCircleV((Vector2){ox, oy}, 13.0f, accent);
    DrawCircleV((Vector2){ox - 3.5f, oy - 3.5f}, 4.5f, Fade(WHITE, 0.85f));
    DrawCircleV((Vector2){ox - 2.0f, oy - 2.0f}, 2.0f, WHITE);

    // Name
    int nameFs = 26;
    int nameW  = MeasureText(name, nameFs);
    int nameX  = cardX + 64;
    int nameY  = cardY + 14;
    DrawText(name, nameX + 2, nameY + 2, nameFs, Fade(BLACK, 0.65f));
    DrawText(name, nameX, nameY, nameFs, Fade(WHITE, 0.70f + t * 0.30f));
    (void)nameW;

    // Role
    int roleFs = 13;
    DrawText(role, nameX, nameY + nameFs + 4, roleFs, Fade(accent, 0.95f));
}

void DrawCredits(const Game *g) {
    DrawBackground(g);
    DrawParticles(g);
    DrawRectangle(0, 0, SCREEN_W, SCREEN_H, Fade(BLACK, 0.45f));

    float t = (float)GetTime();
    // Big animated title
    float hue = fmodf(g->creditsTimer * 50.0f, 360.0f);
    const char *title = "CREDITS";
    int tfs = 56;
    int tw  = MeasureText(title, tfs);
    DrawText(title, (SCREEN_W - tw)/2 + 3, 78, tfs, Fade(BLACK, 0.80f));
    Color glowC = ColorFromHSV(hue, 0.85f, 1.0f); glowC.a = 50;
    DrawText(title, (SCREEN_W - tw)/2 - 2, 75, tfs, glowC);
    DrawText(title, (SCREEN_W - tw)/2, 75, tfs, ColorFromHSV(hue, 0.7f, 1.0f));

    // Subtitle
    const char *sub = "NABCAN";
    int sw = MeasureText(sub, 18);
    DrawText(sub, (SCREEN_W - sw)/2, 140, 18, Fade(WHITE, 0.75f));
    const char *sub2 = "built with C and raylib";
    int sw2 = MeasureText(sub2, 13);
    DrawText(sub2, (SCREEN_W - sw2)/2, 164, 13, Fade((Color){160,180,220,255}, 0.85f));

    // Divider
    float lineA = 0.20f + 0.10f * sinf(t * 1.4f);
    DrawLineEx((Vector2){(float)(SCREEN_W/2 - 130), 195.0f},
               (Vector2){(float)(SCREEN_W/2 + 130), 195.0f}, 1.5f, Fade(WHITE, lineA));
    DrawCircleV((Vector2){(float)(SCREEN_W/2), 195.0f}, 3.5f, Fade(g->theme.accentA, 0.7f));

    // Three credit cards — stagger appear
    struct { const char *name; const char *role; int y; Color accent; } credits[] = {
        {"BESOCAN",    "Game Designer & Coder",   258, (Color){ 90, 220, 255, 255}},
        {"AYSENURCAN", "Creative Direction",      352, (Color){255, 130, 200, 255}},
        {"NURCAN",     "Producer & QA",           446, (Color){180, 130, 255, 255}},
    };
    for (int i = 0; i < 3; i++) {
        float appearT = (g->creditsTimer - 0.10f - i * 0.18f) / 0.4f;
        if (appearT < 0.0f) appearT = 0.0f;
        if (appearT > 1.0f) appearT = 1.0f;
        DrawCreditCard(credits[i].y, credits[i].name, credits[i].role,
                       appearT, t * 1.3f + i * 0.9f, credits[i].accent);
    }

    // Thank-you tagline (fades in last)
    float tt = (g->creditsTimer - 1.3f) / 0.5f;
    if (tt > 1.0f) tt = 1.0f;
    if (tt > 0.0f) {
        const char *thx = "* Thanks for playing *";
        int twx = MeasureText(thx, 17);
        Color tCol = ColorFromHSV(fmodf(t * 60.0f, 360.0f), 0.6f, 1.0f);
        DrawText(thx, (SCREEN_W - twx)/2, 532, 17, Fade(tCol, tt * 0.9f));
    }

    // Footer back hint
    float blink = 0.5f + 0.5f * sinf(t * 2.5f);
    const char *back = "[ESC] / [CLICK]  Back to Menu";
    int bw = MeasureText(back, 14);
    DrawText(back, (SCREEN_W - bw)/2, SCREEN_H - 36, 14, Fade(LIGHTGRAY, 0.4f + blink * 0.4f));
}

// ── Draw: Victory ─────────────────────────────────────────────────────────────
void DrawVictory(const Game *g) {
    DrawBackground(g);
    DrawShockwaves(g);
    DrawParticles(g);
    DrawRectangle(0, 0, SCREEN_W, SCREEN_H, Fade(BLACK, 0.55f));

    float t  = (float)GetTime();
    float vt = g->victoryTimer;

    // Massive title with rainbow shimmer
    float titleScale = fminf(vt / 0.6f, 1.0f);
    int fs = (int)(64 * (0.55f + titleScale * 0.45f));
    const char *title = "YOU WIN!";
    int tw = MeasureText(title, fs);

    // Rainbow letter pass: redraw with hue offset per letter for shimmer
    DrawText(title, (SCREEN_W - tw)/2 + 4, 96, fs, Fade(BLACK, 0.80f));
    // Glow stack
    for (int g_i = 3; g_i > 0; g_i--) {
        Color c = ColorFromHSV(fmodf(t * 90.0f, 360.0f), 0.85f, 1.0f);
        c.a = (unsigned char)(40 / g_i);
        DrawText(title, (SCREEN_W - tw)/2 - g_i, 96 - g_i, fs, c);
        DrawText(title, (SCREEN_W - tw)/2 + g_i, 96 + g_i, fs, c);
    }
    DrawText(title, (SCREEN_W - tw)/2, 96, fs, ColorFromHSV(fmodf(t * 70.0f, 360.0f), 0.75f, 1.0f));

    // Subtitle
    const char *sub = "Level 20 cleared";
    int sw = MeasureText(sub, 22);
    DrawText(sub, (SCREEN_W - sw)/2, 178, 22, Fade(WHITE, 0.85f));
    char scBuf[64];
    sprintf(scBuf, "Final score: %d", g->score);
    int scW = MeasureText(scBuf, 20);
    DrawText(scBuf, (SCREEN_W - scW)/2, 212, 20, Fade((Color){255, 220, 80, 255}, 0.95f));

    // Trophy: stylized chalice using primitives
    float cx = SCREEN_W / 2.0f;
    float cy = 296.0f;
    float pulse = 0.5f + 0.5f * sinf(t * 2.4f);
    // Trophy glow
    DrawCircleV((Vector2){cx, cy + 6}, 70.0f + pulse * 8.0f, Fade((Color){255, 220, 80, 255}, 0.10f + pulse * 0.06f));
    DrawCircleV((Vector2){cx, cy + 6}, 48.0f, Fade((Color){255, 220, 80, 255}, 0.18f));
    // Cup body
    Color gold     = (Color){255, 210, 60, 255};
    Color goldDark = (Color){200, 145, 20, 255};
    DrawRectangleRounded((Rectangle){cx - 36, cy - 22, 72, 44}, 0.5f, 8, gold);
    DrawRectangleRounded((Rectangle){cx - 30, cy - 18, 60, 18}, 0.45f, 6, (Color){255, 245, 200, 255});
    // Handles
    DrawRing((Vector2){cx - 40, cy - 6}, 8.5f, 13.0f, 50, 240, 24, goldDark);
    DrawRing((Vector2){cx + 40, cy - 6}, 8.5f, 13.0f, -60, 130, 24, goldDark);
    // Stem
    DrawRectangleRounded((Rectangle){cx - 7, cy + 22, 14, 18}, 0.5f, 6, goldDark);
    // Base
    DrawRectangleRounded((Rectangle){cx - 26, cy + 40, 52, 10}, 0.5f, 6, gold);
    DrawRectangleRounded((Rectangle){cx - 32, cy + 50, 64, 8}, 0.4f, 6, goldDark);
    // Star on cup
    float sr = 11.0f + pulse * 1.5f;
    DrawPoly((Vector2){cx, cy - 2}, 5, sr, t * 30.0f, WHITE);
    DrawPoly((Vector2){cx, cy - 2}, 5, sr * 0.55f, t * 30.0f + 36.0f, gold);

    // "Made by:" line
    const char *by = "MADE BY";
    int bw = MeasureText(by, 17);
    DrawText(by, (SCREEN_W - bw)/2, 378, 17, Fade(WHITE, 0.65f));
    // Wavy underline
    for (int i = 0; i < 60; i++) {
        float xx = (SCREEN_W/2.0f) - 60.0f + i * 2.0f;
        float yy = 400.0f + sinf(t * 2.5f + i * 0.25f) * 2.0f;
        DrawCircleV((Vector2){xx, yy}, 1.2f, Fade(g->theme.accentA, 0.55f));
    }

    // Three names — animated rainbow
    const char *names[] = {"BESOCAN", "AYSENURCAN", "NURCAN"};
    float yOffsets[] = {426, 470, 514};
    for (int i = 0; i < 3; i++) {
        float wave  = sinf(t * 2.2f + i * 1.3f);
        float scale = 1.0f + 0.04f * wave;
        int nfs = (int)(28 * scale);
        int nw  = MeasureText(names[i], nfs);
        Color nc = ColorFromHSV(fmodf(t * 60.0f + i * 110.0f, 360.0f), 0.75f, 1.0f);
        // Shadow + glow
        DrawText(names[i], (SCREEN_W - nw)/2 + 3, (int)yOffsets[i] + 3, nfs, Fade(BLACK, 0.7f));
        Color glw = nc; glw.a = 60;
        DrawText(names[i], (SCREEN_W - nw)/2 - 2, (int)yOffsets[i] - 1, nfs, glw);
        DrawText(names[i], (SCREEN_W - nw)/2,     (int)yOffsets[i],     nfs, nc);
    }

    // Hint
    if (vt > 1.5f) {
        float bl = 0.5f + 0.5f * sinf(t * 3.5f);
        const char *hint = "[ENTER] Continue";
        int hw = MeasureText(hint, 16);
        DrawText(hint, (SCREEN_W - hw)/2, SCREEN_H - 36, 16, Fade(WHITE, 0.4f + bl * 0.4f));
    }
}

// ── App icon (runtime image for window/taskbar) ───────────────────────────────
// Builds a 64x64 PIXELART-style icon entirely from raylib draw calls. Used by
// SetWindowIcon so the running window + taskbar show the icon (Explorer uses
// the .exe-embedded ICO; window/taskbar uses this runtime image).
Image BuildAppIcon(void) {
    const int S = 64;
    Image img = GenImageColor(S, S, (Color){0, 0, 0, 0});

    // Rounded-ish dark blue gradient background
    for (int y = 0; y < S; y++) {
        for (int x = 0; x < S; x++) {
            // Corner mask for "rounded rect" look
            int cx = (x < 6) ? (6 - x) : (x > S - 7 ? x - (S - 7) : 0);
            int cy = (y < 6) ? (6 - y) : (y > S - 7 ? y - (S - 7) : 0);
            int dist2 = cx * cx + cy * cy;
            if (dist2 > 36) continue;
            float t = (float)y / S;
            unsigned char r = (unsigned char)(12 + 28 * t);
            unsigned char g = (unsigned char)(18 + 42 * t);
            unsigned char b = (unsigned char)(48 + 60 * t);
            ImageDrawPixel(&img, x, y, (Color){r, g, b, 255});
        }
    }

    // Bricks row 1
    ImageDrawRectangle(&img,  6, 12, 16, 10, (Color){240,  90,  90, 255});
    ImageDrawRectangle(&img,  6, 12, 16,  4, (Color){255, 200, 200, 110});
    ImageDrawRectangle(&img, 25, 12, 16, 10, (Color){255, 165,  30, 255});
    ImageDrawRectangle(&img, 25, 12, 16,  4, (Color){255, 220, 160, 110});
    ImageDrawRectangle(&img, 44, 12, 14, 10, (Color){ 90, 200, 230, 255});
    ImageDrawRectangle(&img, 44, 12, 14,  4, (Color){180, 230, 245, 110});
    // Bricks row 2
    ImageDrawRectangle(&img, 15, 24, 16, 10, (Color){180, 110, 230, 255});
    ImageDrawRectangle(&img, 15, 24, 16,  4, (Color){220, 180, 245, 110});
    ImageDrawRectangle(&img, 34, 24, 16, 10, (Color){ 90, 230, 130, 255});
    ImageDrawRectangle(&img, 34, 24, 16,  4, (Color){180, 245, 200, 110});

    // Glowing ball (bottom right)
    ImageDrawCircle(&img, 42, 46, 11, (Color){ 80, 170, 240,  40});
    ImageDrawCircle(&img, 42, 46,  8, (Color){120, 200, 255,  90});
    ImageDrawCircle(&img, 42, 46,  6, (Color){245, 250, 255, 255});
    ImageDrawPixel (&img, 39, 43,    (Color){255, 255, 255, 255});

    // Trajectory dots (heading down-right to ball)
    for (int i = 0; i < 4; i++) {
        int dx = 30 - i * 4;
        int dy = 38 + i * 2;
        ImageDrawCircle(&img, dx, dy, 1, (Color){120, 200, 255, (unsigned char)(220 - i * 50)});
    }

    // Launcher (bottom left)
    ImageDrawCircle(&img, 14, 54, 7, (Color){ 60, 200, 255,  70});
    ImageDrawCircle(&img, 14, 54, 5, (Color){255, 255, 255, 230});
    ImageDrawCircle(&img, 14, 54, 3, (Color){ 60, 200, 255, 255});

    return img;
}

// ── Main ──────────────────────────────────────────────────────────────────────
int main(void) {
    InitWindow(SCREEN_W, SCREEN_H, "NABCAN");
    SetExitKey(KEY_NULL);   // ESC must NOT close the window — game manages ESC itself
    // Set the runtime window/taskbar icon (separate from the .exe-embedded ICO
    // used by Explorer). Built from raylib primitives — no asset file needed.
    Image appIcon = BuildAppIcon();
    SetWindowIcon(appIcon);
    UnloadImage(appIcon);
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
            case STATE_CREDITS:         UpdateCredits(g, dt);      break;
            case STATE_VICTORY:         UpdateVictory(g, dt);      break;
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
            case STATE_CREDITS:         DrawCredits(g);                     break;
            case STATE_VICTORY:         DrawVictory(g);                     break;
        }
        EndDrawing();
    }

    WriteSaveData(g);
    if (g->audioReady) {
        UnloadSound(g->sndHit);    UnloadSound(g->sndDestroy);
        UnloadSound(g->sndPickup); UnloadSound(g->sndGameOver);
        UnloadSound(g->sndRoundEnd); UnloadSound(g->sndLevelUp);
        UnloadSound(g->sndBoom); UnloadSound(g->sndMegaBoom);
        UnloadSound(g->sndCelebration);
        CloseAudioDevice();
    }
    free(g);
    CloseWindow();
    return 0;
}
