// Brick Breaker — Complete implementation in C using raylib
#include "raylib.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------
#define SCREEN_WIDTH       800
#define SCREEN_HEIGHT      600
#define PADDLE_WIDTH       100
#define PADDLE_HEIGHT      15
#define PADDLE_SPEED       400.0f
#define PADDLE_Y_OFFSET    50
#define BALL_RADIUS        8.0f
#define BALL_BASE_SPEED    300.0f
#define BRICK_ROWS         7
#define BRICK_COLS         11
#define BRICK_GAP          3.0f
#define BRICK_MARGIN       20.0f
#define BRICK_HEIGHT       20.0f
#define BRICK_START_Y      60.0f
#define MAX_LEVELS         5
#define MAX_LIVES          3
#define MAX_SCORES         5
#define NAME_LEN           10
#define HUD_HEIGHT         30
#define MAX_BRICKS         (BRICK_ROWS * BRICK_COLS)
#define MAX_PARTICLES      60
#define SPEED_BOOST_BRICKS 5
#define MAX_SPEED_FACTOR   2.5f

// ---------------------------------------------------------------------------
// Types
// ---------------------------------------------------------------------------
typedef enum {
    STATE_MENU,
    STATE_PLAYING,
    STATE_PAUSED,
    STATE_LEVEL_COMPLETE,
    STATE_GAME_OVER,
    STATE_WIN,
    STATE_HIGHSCORE
} GameState;

typedef struct {
    Vector2 position;
    Vector2 velocity;
    float   radius;
    bool    active;
    Color   color;
} Ball;

typedef struct {
    Rectangle rect;
    float     speed;
    Color     color;
} Paddle;

typedef struct {
    Rectangle rect;
    int       health;
    int       originalHealth;
    bool      active;
    Color     color;
    int       scoreValue;
} Brick;

typedef struct {
    Brick  bricks[MAX_BRICKS];
    int    brickCount;
    float  ballSpeedMultiplier;
} Level;

typedef struct {
    char name[NAME_LEN];
    int  score;
} ScoreEntry;

typedef struct {
    ScoreEntry entries[MAX_SCORES];
    int        count;
} Highscore;

typedef struct {
    Vector2 position;
    Vector2 velocity;
    Color   color;
    float   size;
    float   life;
    float   maxLife;
} Particle;

typedef struct {
    GameState  state;
    Ball       ball;
    Paddle     paddle;
    Level      levels[MAX_LEVELS];
    int        currentLevel;
    int        score;
    int        lives;
    int        maxLives;
    Highscore  highscores;
    bool       soundEnabled;
    char       inputName[NAME_LEN];
    int        inputLen;
    // Menu demo ball
    Vector2    menuBallPos;
    Vector2    menuBallVel;
    // Level complete timer
    float      levelCompleteTimer;
    // Win particles
    Particle   particles[MAX_PARTICLES];
    int        particleCount;
    bool       particlesSpawned;
    // Score tracking for level bonus
    int        scoreAtLevelStart;
    bool       lifeLostThisLevel;
    // Speed tracking
    int        bricksDestroyedSinceBoost;
    float      currentBallSpeed;
    // Sounds
    Sound      sndHitBrick;
    Sound      sndHitPaddle;
    Sound      sndBallLost;
    Sound      sndLevelComplete;
    bool       audioReady;
    // Highscore entry mode flag
    bool       enteringHighscore;
    bool       hsDisplayOnly;
} Game;

// ---------------------------------------------------------------------------
// Level grid definitions
// ---------------------------------------------------------------------------
static const int LEVEL_GRIDS[MAX_LEVELS][BRICK_ROWS][BRICK_COLS] = {
    // Level 1 — Classic Wall
    {
        {1,1,1,1,1,1,1,1,1,1,1},
        {1,1,1,1,1,1,1,1,1,1,1},
        {1,1,1,1,1,1,1,1,1,1,1},
        {1,1,1,1,1,1,1,1,1,1,1},
        {1,1,1,1,1,1,1,1,1,1,1},
        {0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0},
    },
    // Level 2 — Diamond
    {
        {0,0,0,0,0,1,0,0,0,0,0},
        {0,0,0,0,1,2,1,0,0,0,0},
        {0,0,0,1,2,2,2,1,0,0,0},
        {0,0,1,2,2,2,2,2,1,0,0},
        {0,0,0,1,2,2,2,1,0,0,0},
        {0,0,0,0,1,2,1,0,0,0,0},
        {0,0,0,0,0,1,0,0,0,0,0},
    },
    // Level 3 — Checkerboard with outer ring of 2s
    {
        {2,2,2,2,2,2,2,2,2,2,2},
        {2,1,0,1,0,1,0,1,0,1,2},
        {2,0,1,0,1,0,1,0,1,0,2},
        {2,1,0,1,0,1,0,1,0,1,2},
        {2,0,1,0,1,0,1,0,1,0,2},
        {2,1,0,1,0,1,0,1,0,1,2},
        {2,2,2,2,2,2,2,2,2,2,2},
    },
    // Level 4 — Fortress
    {
        {3,3,3,3,3,3,3,3,3,3,3},
        {3,2,2,2,2,2,2,2,2,2,3},
        {3,2,1,2,1,2,1,2,1,2,3},
        {3,2,2,2,2,2,2,2,2,2,3},
        {3,2,1,2,1,2,1,2,1,2,3},
        {3,2,2,2,2,2,2,2,2,2,3},
        {3,3,3,3,3,3,3,3,3,3,3},
    },
    // Level 5 — Chaos
    {
        {1,3,2,0,1,2,1,0,2,3,1},
        {2,1,0,2,3,1,3,2,0,1,2},
        {0,2,3,1,2,2,2,1,3,2,0},
        {1,0,1,3,1,3,1,3,1,0,1},
        {3,2,0,1,2,1,2,1,0,2,3},
        {1,3,2,0,3,2,3,0,2,3,1},
        {2,1,3,2,1,0,1,2,3,1,2},
    },
};

static const float LEVEL_SPEEDS[MAX_LEVELS] = {1.0f, 1.15f, 1.30f, 1.50f, 1.80f};

// ---------------------------------------------------------------------------
// Forward declarations
// ---------------------------------------------------------------------------
void InitGame(Game *game);
void InitLevel(Game *game, int levelIndex);
void LoadHighscores(Highscore *hs);
void SaveHighscores(const Highscore *hs);
bool IsNewHighscore(const Highscore *hs, int score);
void InsertHighscore(Highscore *hs, const char *name, int score);
Sound GenerateBeep(float frequency, float duration, float volume);

void UpdateMenu(Game *game);
void UpdatePlaying(Game *game, float dt);
void UpdatePaused(Game *game);
void UpdateLevelComplete(Game *game, float dt);
void UpdateGameOver(Game *game);
void UpdateWin(Game *game, float dt);
void UpdateHighscore(Game *game);

void DrawMenu(const Game *game);
void DrawPlaying(const Game *game);
void DrawHUD(const Game *game);
void DrawPaused(const Game *game);
void DrawLevelComplete(const Game *game);
void DrawGameOver(const Game *game);
void DrawWin(const Game *game);
void DrawHighscore(const Game *game);

void ResetBall(Game *game);
void NextLevel(Game *game);
void LoseLife(Game *game);
Color GetBrickColor(int health, int row);
void PlaySfx(const Game *game, Sound snd);
void SpawnParticles(Game *game);
void UpdateParticles(Game *game, float dt);

// ---------------------------------------------------------------------------
// Sound helpers
// ---------------------------------------------------------------------------
Sound GenerateBeep(float frequency, float duration, float volume) {
    int sampleRate  = 44100;
    int sampleCount = (int)(sampleRate * duration);
    short *samples  = (short *)malloc(sampleCount * sizeof(short));
    for (int i = 0; i < sampleCount; i++) {
        float t        = (float)i / sampleRate;
        float envelope = 1.0f - (t / duration);
        samples[i]     = (short)(sinf(2.0f * PI * frequency * t) * envelope * 32767.0f * volume);
    }
    Wave wave = {
        .frameCount = (unsigned int)sampleCount,
        .sampleRate = (unsigned int)sampleRate,
        .sampleSize = 16,
        .channels   = 1,
        .data       = samples
    };
    Sound snd = LoadSoundFromWave(wave);
    free(samples);
    return snd;
}

void PlaySfx(const Game *game, Sound snd) {
    if (game->soundEnabled && game->audioReady) PlaySound(snd);
}

// ---------------------------------------------------------------------------
// Highscore persistence
// ---------------------------------------------------------------------------
void LoadHighscores(Highscore *hs) {
    FILE *f = fopen("highscores.dat", "rb");
    if (!f) { memset(hs, 0, sizeof(*hs)); return; }
    fread(hs, sizeof(Highscore), 1, f);
    fclose(f);
}

void SaveHighscores(const Highscore *hs) {
    FILE *f = fopen("highscores.dat", "wb");
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
    int insertPos = hs->count;
    for (int i = 0; i < hs->count; i++) {
        if (score > hs->entries[i].score) { insertPos = i; break; }
    }
    int end = (hs->count < MAX_SCORES) ? hs->count : MAX_SCORES - 1;
    for (int i = end; i > insertPos; i--)
        hs->entries[i] = hs->entries[i - 1];
    strncpy(hs->entries[insertPos].name, name, NAME_LEN - 1);
    hs->entries[insertPos].name[NAME_LEN - 1] = '\0';
    hs->entries[insertPos].score = score;
    if (hs->count < MAX_SCORES) hs->count++;
}

// ---------------------------------------------------------------------------
// Color helpers
// ---------------------------------------------------------------------------
Color GetBrickColor(int health, int row) {
    if (health >= 3) return DARKGRAY;
    if (health == 2) return RED;
    // health == 1, vary by row
    Color rowColors[] = {SKYBLUE, GREEN, YELLOW, ORANGE, SKYBLUE, GREEN, YELLOW};
    return rowColors[row % 7];
}

// ---------------------------------------------------------------------------
// Init
// ---------------------------------------------------------------------------
void InitGame(Game *game) {
    memset(game, 0, sizeof(Game));
    game->state    = STATE_MENU;
    game->lives    = MAX_LIVES;
    game->maxLives = MAX_LIVES;
    game->soundEnabled = true;

    // Audio
    if (IsAudioDeviceReady()) {
        game->audioReady    = true;
        game->sndHitBrick   = GenerateBeep(440.0f, 0.08f, 0.6f);
        game->sndHitPaddle  = GenerateBeep(300.0f, 0.06f, 0.5f);
        game->sndBallLost   = GenerateBeep(150.0f, 0.40f, 0.7f);
        game->sndLevelComplete = GenerateBeep(880.0f, 0.50f, 0.6f);
    }

    LoadHighscores(&game->highscores);

    // Menu demo ball
    game->menuBallPos = (Vector2){SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT / 2.0f};
    game->menuBallVel = (Vector2){120.0f, -90.0f};

    // Paddle
    game->paddle.rect  = (Rectangle){
        (SCREEN_WIDTH - PADDLE_WIDTH) / 2.0f,
        SCREEN_HEIGHT - PADDLE_Y_OFFSET - PADDLE_HEIGHT,
        PADDLE_WIDTH, PADDLE_HEIGHT
    };
    game->paddle.speed = PADDLE_SPEED;
    game->paddle.color = (Color){80, 160, 255, 255};

    // Ball default
    game->ball.radius = BALL_RADIUS;
    game->ball.color  = WHITE;
    game->ball.active = false;

    // Init first level data (not starting it yet)
    for (int i = 0; i < MAX_LEVELS; i++) {
        game->levels[i].ballSpeedMultiplier = LEVEL_SPEEDS[i];
    }
}

void InitLevel(Game *game, int levelIndex) {
    Level *lvl = &game->levels[levelIndex];
    memset(lvl->bricks, 0, sizeof(lvl->bricks));
    lvl->brickCount = 0;
    lvl->ballSpeedMultiplier = LEVEL_SPEEDS[levelIndex];

    float brickW = (SCREEN_WIDTH - 2.0f * BRICK_MARGIN - (BRICK_COLS - 1) * BRICK_GAP) / BRICK_COLS;

    for (int r = 0; r < BRICK_ROWS; r++) {
        for (int c = 0; c < BRICK_COLS; c++) {
            int val = LEVEL_GRIDS[levelIndex][r][c];
            int idx = r * BRICK_COLS + c;
            Brick *b = &lvl->bricks[idx];

            if (val == 0) { b->active = false; continue; }

            b->active = true;
            b->health = val;
            b->originalHealth = val;
            b->rect = (Rectangle){
                BRICK_MARGIN + c * (brickW + BRICK_GAP),
                BRICK_START_Y + r * (BRICK_HEIGHT + BRICK_GAP),
                brickW,
                BRICK_HEIGHT
            };
            b->color = GetBrickColor(val, r);

            if (val == 1) b->scoreValue = 10;
            else if (val == 2) b->scoreValue = 25;
            else b->scoreValue = 0; // unbreakable

            if (val != 3) lvl->brickCount++;
        }
    }

    game->currentLevel = levelIndex;
    game->scoreAtLevelStart = game->score;
    game->lifeLostThisLevel = false;
    game->bricksDestroyedSinceBoost = 0;
    game->currentBallSpeed = BALL_BASE_SPEED * lvl->ballSpeedMultiplier;

    ResetBall(game);
}

void ResetBall(Game *game) {
    game->ball.active   = false;
    game->ball.position = (Vector2){
        game->paddle.rect.x + game->paddle.rect.width / 2.0f,
        game->paddle.rect.y - BALL_RADIUS - 1.0f
    };
    game->ball.velocity = (Vector2){0, 0};
}

// ---------------------------------------------------------------------------
// Particles
// ---------------------------------------------------------------------------
void SpawnParticles(Game *game) {
    game->particleCount = MAX_PARTICLES;
    for (int i = 0; i < MAX_PARTICLES; i++) {
        float angle = ((float)(GetRandomValue(0, 360))) * DEG2RAD;
        float speed = (float)GetRandomValue(60, 250);
        game->particles[i].position = (Vector2){
            (float)GetRandomValue(100, SCREEN_WIDTH - 100),
            (float)GetRandomValue(100, SCREEN_HEIGHT - 100)
        };
        game->particles[i].velocity = (Vector2){cosf(angle) * speed, sinf(angle) * speed};
        game->particles[i].size    = (float)GetRandomValue(5, 14);
        game->particles[i].maxLife = (float)GetRandomValue(15, 35) / 10.0f;
        game->particles[i].life    = game->particles[i].maxLife;
        Color cols[] = {GOLD, YELLOW, ORANGE, RED, GREEN, SKYBLUE, PURPLE, WHITE};
        game->particles[i].color = cols[GetRandomValue(0, 7)];
    }
}

void UpdateParticles(Game *game, float dt) {
    for (int i = 0; i < game->particleCount; i++) {
        Particle *p = &game->particles[i];
        if (p->life <= 0) continue;
        p->life -= dt;
        p->position.x += p->velocity.x * dt;
        p->position.y += p->velocity.y * dt;
        p->velocity.y += 200.0f * dt; // gravity
    }
}

// ---------------------------------------------------------------------------
// Update functions
// ---------------------------------------------------------------------------
void UpdateMenu(Game *game) {
    // Demo ball
    float dt = GetFrameTime();
    game->menuBallPos.x += game->menuBallVel.x * dt;
    game->menuBallPos.y += game->menuBallVel.y * dt;
    if (game->menuBallPos.x < 10 || game->menuBallPos.x > SCREEN_WIDTH - 10)
        game->menuBallVel.x = -game->menuBallVel.x;
    if (game->menuBallPos.y < 10 || game->menuBallPos.y > SCREEN_HEIGHT - 10)
        game->menuBallVel.y = -game->menuBallVel.y;

    if (IsKeyPressed(KEY_ENTER)) {
        game->score = 0;
        game->lives = MAX_LIVES;
        game->currentLevel = 0;
        InitLevel(game, 0);
        game->state = STATE_PLAYING;
    }
    if (IsKeyPressed(KEY_H)) {
        game->hsDisplayOnly = true;
        game->state = STATE_HIGHSCORE;
    }
    if (IsKeyPressed(KEY_M)) game->soundEnabled = !game->soundEnabled;
    if (IsKeyPressed(KEY_ESCAPE)) CloseWindow();
}

void UpdatePlaying(Game *game, float dt) {
    if (IsKeyPressed(KEY_P) || IsKeyPressed(KEY_ESCAPE)) {
        game->state = STATE_PAUSED;
        return;
    }
    if (IsKeyPressed(KEY_M)) game->soundEnabled = !game->soundEnabled;

    Paddle *pad = &game->paddle;
    Ball   *ball = &game->ball;

    // Paddle movement
    if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A))
        pad->rect.x -= PADDLE_SPEED * dt;
    if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D))
        pad->rect.x += PADDLE_SPEED * dt;

    // Mouse control
    pad->rect.x = GetMouseX() - pad->rect.width / 2.0f;

    // Clamp paddle
    if (pad->rect.x < 0) pad->rect.x = 0;
    if (pad->rect.x + pad->rect.width > SCREEN_WIDTH)
        pad->rect.x = SCREEN_WIDTH - pad->rect.width;

    // Ball stuck to paddle
    if (!ball->active) {
        ball->position.x = pad->rect.x + pad->rect.width / 2.0f;
        ball->position.y = pad->rect.y - BALL_RADIUS - 1.0f;

        if (IsKeyPressed(KEY_SPACE)) {
            ball->active = true;
            float angle = ((float)GetRandomValue(-60, -120)) * DEG2RAD;
            // Ensure we always go upward with a reasonable angle
            float spd = game->currentBallSpeed;
            ball->velocity.x = sinf(angle) * spd;
            ball->velocity.y = -fabsf(cosf(angle) * spd);
        }
        return;
    }

    // Move ball
    ball->position.x += ball->velocity.x * dt;
    ball->position.y += ball->velocity.y * dt;

    // Wall collisions
    if (ball->position.x - ball->radius < 0) {
        ball->position.x = ball->radius;
        ball->velocity.x = fabsf(ball->velocity.x);
    }
    if (ball->position.x + ball->radius > SCREEN_WIDTH) {
        ball->position.x = SCREEN_WIDTH - ball->radius;
        ball->velocity.x = -fabsf(ball->velocity.x);
    }
    if (ball->position.y - ball->radius < HUD_HEIGHT) {
        ball->position.y = HUD_HEIGHT + ball->radius;
        ball->velocity.y = fabsf(ball->velocity.y);
    }

    // Bottom edge — lose life
    if (ball->position.y - ball->radius > SCREEN_HEIGHT) {
        LoseLife(game);
        return;
    }

    // Paddle collision
    if (CheckCollisionCircleRec(ball->position, ball->radius, pad->rect) && ball->velocity.y > 0) {
        float relHit  = (ball->position.x - pad->rect.x) / pad->rect.width;
        float angle   = (relHit - 0.5f) * 2.0f * 70.0f * DEG2RAD;
        float speed   = sqrtf(ball->velocity.x * ball->velocity.x + ball->velocity.y * ball->velocity.y);
        ball->velocity.x  = sinf(angle) * speed;
        ball->velocity.y  = -cosf(angle) * speed;
        ball->position.y  = pad->rect.y - ball->radius - 1.0f;
        PlaySfx(game, game->sndHitPaddle);
    }

    // Brick collisions — one per frame
    Level *lvl = &game->levels[game->currentLevel];
    bool hit = false;
    for (int i = 0; i < MAX_BRICKS && !hit; i++) {
        Brick *b = &lvl->bricks[i];
        if (!b->active) continue;
        if (!CheckCollisionCircleRec(ball->position, ball->radius, b->rect)) continue;

        hit = true;

        // Side detection
        float ballLeft   = ball->position.x - ball->radius;
        float ballRight  = ball->position.x + ball->radius;
        float ballTop    = ball->position.y - ball->radius;
        float ballBottom = ball->position.y + ball->radius;

        float overlapLeft   = ballRight  - b->rect.x;
        float overlapRight  = (b->rect.x + b->rect.width)  - ballLeft;
        float overlapTop    = ballBottom - b->rect.y;
        float overlapBottom = (b->rect.y + b->rect.height) - ballTop;

        float minOverlapX = fminf(overlapLeft, overlapRight);
        float minOverlapY = fminf(overlapTop, overlapBottom);

        if (minOverlapX < minOverlapY)
            ball->velocity.x = -ball->velocity.x;
        else
            ball->velocity.y = -ball->velocity.y;

        // Damage brick (unbreakable = 3 skips)
        if (b->health == 3) continue; // unbreakable, just bounce

        b->health--;
        if (b->health <= 0) {
            b->active = false;
            game->score += b->scoreValue;
            lvl->brickCount--;
            PlaySfx(game, game->sndHitBrick);

            // Speed boost
            game->bricksDestroyedSinceBoost++;
            if (game->bricksDestroyedSinceBoost >= SPEED_BOOST_BRICKS) {
                game->bricksDestroyedSinceBoost = 0;
                float maxSpd = BALL_BASE_SPEED * lvl->ballSpeedMultiplier * MAX_SPEED_FACTOR;
                float curSpd = sqrtf(ball->velocity.x * ball->velocity.x +
                                     ball->velocity.y * ball->velocity.y);
                float newSpd = fminf(curSpd * 1.05f, maxSpd);
                float ratio  = newSpd / curSpd;
                ball->velocity.x *= ratio;
                ball->velocity.y *= ratio;
            }
        } else {
            // Tough brick first hit — darken color
            b->color = (Color){
                (unsigned char)(b->color.r / 2),
                (unsigned char)(b->color.g / 2),
                (unsigned char)(b->color.b / 2),
                255
            };
            PlaySfx(game, game->sndHitBrick);
        }
    }

    // Level complete?
    if (lvl->brickCount <= 0) {
        int bonus = 100 * (game->currentLevel + 1);
        game->score += bonus;
        if (!game->lifeLostThisLevel) game->score += 50;
        PlaySfx(game, game->sndLevelComplete);
        game->levelCompleteTimer = 0.0f;
        game->state = STATE_LEVEL_COMPLETE;
    }
}

void UpdatePaused(Game *game) {
    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_P)) game->state = STATE_PLAYING;
    if (IsKeyPressed(KEY_M)) game->soundEnabled = !game->soundEnabled;
    if (IsKeyPressed(KEY_R)) {
        InitLevel(game, game->currentLevel);
        game->state = STATE_PLAYING;
    }
    if (IsKeyPressed(KEY_Q)) {
        game->state = STATE_MENU;
    }
}

void UpdateLevelComplete(Game *game, float dt) {
    game->levelCompleteTimer += dt;
    if (IsKeyPressed(KEY_ENTER) || game->levelCompleteTimer >= 2.0f) {
        if (game->currentLevel + 1 >= MAX_LEVELS) {
            game->particlesSpawned = false;
            game->state = STATE_WIN;
        } else {
            NextLevel(game);
        }
    }
}

void NextLevel(Game *game) {
    InitLevel(game, game->currentLevel + 1);
    game->state = STATE_PLAYING;
}

void LoseLife(Game *game) {
    game->lives--;
    game->lifeLostThisLevel = true;
    PlaySfx(game, game->sndBallLost);
    if (game->lives <= 0) {
        game->state = STATE_GAME_OVER;
    } else {
        ResetBall(game);
    }
}

void UpdateGameOver(Game *game) {
    if (IsKeyPressed(KEY_M)) game->soundEnabled = !game->soundEnabled;
    if (IsKeyPressed(KEY_ENTER)) {
        if (IsNewHighscore(&game->highscores, game->score)) {
            game->enteringHighscore = true;
            game->hsDisplayOnly     = false;
            game->inputLen          = 0;
            memset(game->inputName, 0, NAME_LEN);
            game->state = STATE_HIGHSCORE;
        } else {
            // Restart
            game->score = 0;
            game->lives = MAX_LIVES;
            InitLevel(game, 0);
            game->state = STATE_PLAYING;
        }
    }
    if (IsKeyPressed(KEY_Q)) game->state = STATE_MENU;
}

void UpdateWin(Game *game, float dt) {
    if (!game->particlesSpawned) {
        SpawnParticles(game);
        game->particlesSpawned = true;
    }
    UpdateParticles(game, dt);

    if (IsKeyPressed(KEY_M)) game->soundEnabled = !game->soundEnabled;
    if (IsKeyPressed(KEY_ENTER)) {
        if (IsNewHighscore(&game->highscores, game->score)) {
            game->enteringHighscore = true;
            game->hsDisplayOnly     = false;
            game->inputLen          = 0;
            memset(game->inputName, 0, NAME_LEN);
            game->state = STATE_HIGHSCORE;
        } else {
            game->state = STATE_MENU;
        }
    }
    if (IsKeyPressed(KEY_Q)) game->state = STATE_MENU;
}

void UpdateHighscore(Game *game) {
    if (IsKeyPressed(KEY_M)) game->soundEnabled = !game->soundEnabled;

    if (game->hsDisplayOnly) {
        if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_ENTER))
            game->state = STATE_MENU;
        return;
    }

    // Name entry
    int key = GetCharPressed();
    while (key > 0) {
        if (((key >= 'a' && key <= 'z') || (key >= 'A' && key <= 'Z') ||
             (key >= '0' && key <= '9')) && game->inputLen < NAME_LEN - 1) {
            game->inputName[game->inputLen++] = (char)key;
            game->inputName[game->inputLen]   = '\0';
        }
        key = GetCharPressed();
    }
    if (IsKeyPressed(KEY_BACKSPACE) && game->inputLen > 0) {
        game->inputName[--game->inputLen] = '\0';
    }
    if (IsKeyPressed(KEY_ENTER) && game->inputLen > 0) {
        InsertHighscore(&game->highscores, game->inputName, game->score);
        SaveHighscores(&game->highscores);
        game->hsDisplayOnly = true;
    }
}

// ---------------------------------------------------------------------------
// Draw functions
// ---------------------------------------------------------------------------
void DrawHUD(const Game *game) {
    DrawRectangle(0, 0, SCREEN_WIDTH, HUD_HEIGHT, Fade(BLACK, 0.7f));

    char buf[64];
    sprintf(buf, "SCORE: %05d", game->score);
    DrawText(buf, 10, 7, 20, WHITE);

    char lvlBuf[32];
    sprintf(lvlBuf, "LEVEL: %d/%d", game->currentLevel + 1, MAX_LEVELS);
    int lw = MeasureText(lvlBuf, 20);
    DrawText(lvlBuf, (SCREEN_WIDTH - lw) / 2, 7, 20, WHITE);

    // Lives as circles
    for (int i = 0; i < game->lives; i++) {
        DrawCircle(SCREEN_WIDTH - 20 - i * 25, HUD_HEIGHT / 2, 8, RED);
    }

    // Sound indicator
    const char *sndTxt = game->soundEnabled ? "[M] SOUND ON" : "[M] SOUND OFF";
    DrawText(sndTxt, SCREEN_WIDTH - MeasureText(sndTxt, 14) - 5,
             SCREEN_HEIGHT - 18, 14, GRAY);
}

void DrawMenu(const Game *game) {
    // Background demo ball
    DrawCircleV(game->menuBallPos, 8, Fade(WHITE, 0.25f));

    // Animated title color
    float hue = (float)fmod(GetTime() * 40.0, 360.0);
    Color titleColor = ColorFromHSV(hue, 0.9f, 1.0f);

    const char *title = "BRICK BREAKER";
    int tw = MeasureText(title, 60);
    DrawText(title, (SCREEN_WIDTH - tw) / 2, 120, 60, titleColor);

    const char *sub = "A raylib game in C";
    int sw = MeasureText(sub, 20);
    DrawText(sub, (SCREEN_WIDTH - sw) / 2, 195, 20, GRAY);

    // Menu items
    struct { const char *text; int y; } items[] = {
        {"[ENTER]  START GAME",   290},
        {"[H]      HIGHSCORES",   330},
        {"[M]      TOGGLE SOUND", 370},
        {"[ESC]    QUIT",         410},
    };
    for (int i = 0; i < 4; i++) {
        int iw = MeasureText(items[i].text, 24);
        DrawText(items[i].text, (SCREEN_WIDTH - iw) / 2, items[i].y, 24, YELLOW);
    }

    const char *sndTxt = game->soundEnabled ? "[M] SOUND ON" : "[M] SOUND OFF";
    DrawText(sndTxt, SCREEN_WIDTH - MeasureText(sndTxt, 14) - 5,
             SCREEN_HEIGHT - 18, 14, GRAY);
}

void DrawPlaying(const Game *game) {
    DrawHUD(game);

    const Level *lvl = &game->levels[game->currentLevel];

    // Bricks
    for (int i = 0; i < MAX_BRICKS; i++) {
        const Brick *b = &lvl->bricks[i];
        if (!b->active) continue;

        DrawRectangleRec(b->rect, b->color);
        DrawRectangleLinesEx(b->rect, 1.5f, Fade(BLACK, 0.4f));

        // Crack overlay for damaged tough brick
        if (b->originalHealth == 2 && b->health == 1) {
            DrawLine(
                (int)b->rect.x + 5,
                (int)b->rect.y + 5,
                (int)(b->rect.x + b->rect.width  - 5),
                (int)(b->rect.y + b->rect.height - 5),
                Fade(BLACK, 0.55f)
            );
        }

        // Unbreakable marker
        if (b->health == 3) {
            DrawRectangleLinesEx(b->rect, 2.0f, GRAY);
        }
    }

    // Paddle
    DrawRectangleRounded(game->paddle.rect, 0.4f, 8, game->paddle.color);
    DrawRectangleRounded(
        (Rectangle){game->paddle.rect.x, game->paddle.rect.y,
                    game->paddle.rect.width, game->paddle.rect.height / 2.0f},
        0.4f, 8, Fade(WHITE, 0.2f)
    );

    // Ball
    DrawCircleV(game->ball.position, game->ball.radius, game->ball.color);
    DrawCircleV(
        (Vector2){game->ball.position.x - 2, game->ball.position.y - 2},
        2, Fade(WHITE, 0.7f)
    );

    // Launch prompt
    if (!game->ball.active) {
        float alpha = 0.5f + 0.5f * sinf((float)GetTime() * 4.0f);
        const char *msg = "PRESS SPACE TO LAUNCH";
        int mw = MeasureText(msg, 22);
        DrawText(msg, (SCREEN_WIDTH - mw) / 2, SCREEN_HEIGHT - 100, 22,
                 Fade(WHITE, alpha));
    }
}

void DrawPaused(const Game *game) {
    (void)game;
    DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Fade(BLACK, 0.5f));

    const char *paused = "PAUSED";
    int pw = MeasureText(paused, 50);
    DrawText(paused, (SCREEN_WIDTH - pw) / 2, 220, 50, WHITE);

    const char *sub = "[ESC] Resume   [R] Restart   [Q] Main Menu";
    int sw = MeasureText(sub, 22);
    DrawText(sub, (SCREEN_WIDTH - sw) / 2, 290, 22, LIGHTGRAY);
}

void DrawLevelComplete(const Game *game) {
    DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Fade(BLACK, 0.6f));

    char buf[64];
    sprintf(buf, "LEVEL %d COMPLETE!", game->currentLevel + 1);
    float scale = 1.0f + 0.04f * sinf((float)GetTime() * 3.0f);
    int fs   = (int)(48 * scale);
    int tw   = MeasureText(buf, fs);
    DrawText(buf, (SCREEN_WIDTH - tw) / 2, 200, fs, GOLD);

    char scoreBuf[64];
    sprintf(scoreBuf, "SCORE: %d", game->score);
    int sw = MeasureText(scoreBuf, 28);
    DrawText(scoreBuf, (SCREEN_WIDTH - sw) / 2, 280, 28, WHITE);

    const char *sub = "[ENTER] Continue";
    int subW = MeasureText(sub, 22);
    DrawText(sub, (SCREEN_WIDTH - subW) / 2, 340, 22, LIGHTGRAY);
}

void DrawGameOver(const Game *game) {
    DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Fade(BLACK, 0.7f));

    const char *go = "GAME OVER";
    int gw = MeasureText(go, 60);
    DrawText(go, (SCREEN_WIDTH - gw) / 2, 180, 60, RED);

    char scoreBuf[64];
    sprintf(scoreBuf, "FINAL SCORE: %d", game->score);
    int sw = MeasureText(scoreBuf, 30);
    DrawText(scoreBuf, (SCREEN_WIDTH - sw) / 2, 270, 30, WHITE);

    if (IsNewHighscore(&game->highscores, game->score)) {
        const char *hs = "NEW HIGH SCORE!";
        int hw = MeasureText(hs, 26);
        DrawText(hs, (SCREEN_WIDTH - hw) / 2, 320, 26, GOLD);
        const char *sub = "[ENTER] Enter Name";
        int subw = MeasureText(sub, 22);
        DrawText(sub, (SCREEN_WIDTH - subw) / 2, 370, 22, YELLOW);
    } else {
        const char *sub = "[ENTER] Play Again   [Q] Main Menu";
        int subw = MeasureText(sub, 22);
        DrawText(sub, (SCREEN_WIDTH - subw) / 2, 360, 22, LIGHTGRAY);
    }
}

void DrawWin(const Game *game) {
    DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, (Color){10, 10, 30, 220});

    // Particles
    for (int i = 0; i < game->particleCount; i++) {
        const Particle *p = &game->particles[i];
        if (p->life <= 0) continue;
        float alpha = p->life / p->maxLife;
        DrawRectangle(
            (int)p->position.x, (int)p->position.y,
            (int)p->size, (int)p->size,
            Fade(p->color, alpha)
        );
    }

    const char *win = "YOU WIN!";
    float scale = 1.0f + 0.05f * sinf((float)GetTime() * 2.5f);
    int fs = (int)(70 * scale);
    int ww = MeasureText(win, fs);
    DrawText(win, (SCREEN_WIDTH - ww) / 2, 160, fs, GOLD);

    char scoreBuf[64];
    sprintf(scoreBuf, "FINAL SCORE: %d", game->score);
    int sw = MeasureText(scoreBuf, 30);
    DrawText(scoreBuf, (SCREEN_WIDTH - sw) / 2, 270, 30, WHITE);

    const char *sub = "[ENTER] Continue   [Q] Menu";
    int subw = MeasureText(sub, 22);
    DrawText(sub, (SCREEN_WIDTH - subw) / 2, 340, 22, LIGHTGRAY);
}

void DrawHighscore(const Game *game) {
    DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Fade(BLACK, 0.8f));

    if (!game->hsDisplayOnly) {
        // Name entry
        const char *prompt = "NEW HIGH SCORE! Enter your name:";
        int pw = MeasureText(prompt, 26);
        DrawText(prompt, (SCREEN_WIDTH - pw) / 2, 200, 26, GOLD);

        // Input box
        char display[NAME_LEN + 2];
        strncpy(display, game->inputName, NAME_LEN);
        display[game->inputLen] = '\0';
        // Blinking caret
        if ((int)(GetTime() * 2) % 2 == 0 && game->inputLen < NAME_LEN - 1) {
            display[game->inputLen] = '|';
            display[game->inputLen + 1] = '\0';
        }
        int dw = MeasureText(display, 36);
        DrawText(display, (SCREEN_WIDTH - dw) / 2, 260, 36, WHITE);

        const char *hint = "[BACKSPACE] Delete   [ENTER] Confirm";
        int hw = MeasureText(hint, 20);
        DrawText(hint, (SCREEN_WIDTH - hw) / 2, 330, 20, GRAY);
        return;
    }

    // Display mode
    const char *title = "HIGH SCORES";
    int tw = MeasureText(title, 44);
    DrawText(title, (SCREEN_WIDTH - tw) / 2, 80, 44, GOLD);

    Color rankColors[] = {GOLD, LIGHTGRAY, (Color){205,127,50,255}, WHITE, WHITE};

    for (int i = 0; i < game->highscores.count && i < MAX_SCORES; i++) {
        char line[64];
        sprintf(line, "#%d  %-10s  %06d",
                i + 1,
                game->highscores.entries[i].name,
                game->highscores.entries[i].score);
        int lw = MeasureText(line, 28);
        DrawText(line, (SCREEN_WIDTH - lw) / 2, 160 + i * 50, 28, rankColors[i]);
    }

    if (game->highscores.count == 0) {
        const char *none = "No scores yet!";
        int nw = MeasureText(none, 24);
        DrawText(none, (SCREEN_WIDTH - nw) / 2, 200, 24, GRAY);
    }

    const char *back = "[ESC] / [ENTER] Back";
    int bw = MeasureText(back, 20);
    DrawText(back, (SCREEN_WIDTH - bw) / 2, 440, 20, GRAY);
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
int main(void) {
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Brick Breaker");
    SetTargetFPS(60);

    if (InitAudioDevice(), !IsAudioDeviceReady()) {
        // Audio failed — will be handled in InitGame
    }

    Game game = {0};
    InitGame(&game);

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        switch (game.state) {
            case STATE_MENU:           UpdateMenu(&game);            break;
            case STATE_PLAYING:        UpdatePlaying(&game, dt);     break;
            case STATE_PAUSED:         UpdatePaused(&game);          break;
            case STATE_LEVEL_COMPLETE: UpdateLevelComplete(&game, dt); break;
            case STATE_GAME_OVER:      UpdateGameOver(&game);        break;
            case STATE_WIN:            UpdateWin(&game, dt);         break;
            case STATE_HIGHSCORE:      UpdateHighscore(&game);       break;
        }

        BeginDrawing();
        ClearBackground((Color){15, 15, 40, 255});

        switch (game.state) {
            case STATE_MENU:           DrawMenu(&game);              break;
            case STATE_PLAYING:        DrawPlaying(&game);           break;
            case STATE_PAUSED:         DrawPlaying(&game); DrawPaused(&game); break;
            case STATE_LEVEL_COMPLETE: DrawLevelComplete(&game);     break;
            case STATE_GAME_OVER:      DrawGameOver(&game);          break;
            case STATE_WIN:            DrawWin(&game);               break;
            case STATE_HIGHSCORE:      DrawHighscore(&game);         break;
        }

        EndDrawing();
    }

    SaveHighscores(&game.highscores);
    if (game.audioReady) {
        UnloadSound(game.sndHitBrick);
        UnloadSound(game.sndHitPaddle);
        UnloadSound(game.sndBallLost);
        UnloadSound(game.sndLevelComplete);
        CloseAudioDevice();
    }
    CloseWindow();
    return 0;
}
