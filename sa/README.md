# raylib-games

Two arcade games written in **C** using [raylib](https://www.raylib.com/), built from scratch.

---

## Games

### 1. Brick Breaker — Classic
**`brick_breaker/`**

Classic paddle-and-ball brick breaker with:
- 5 hand-crafted levels (Classic Wall → Diamond → Checkerboard → Fortress → Chaos)
- Ball angle physics based on paddle hit position
- 3 brick types: Normal, Tough (2-hit + crack overlay), Unbreakable
- Cumulative scoring system with level bonuses
- Synthesized sound effects (no external audio files)
- Highscore persistence (binary file)
- Full state machine: Menu → Playing → Paused → Level Complete → Win/Game Over

### 2. Brick Breaker Hit — Ballz Style
**`brick_breaker_hit/`**

Ballz-style multi-ball shooter with:
- Mouse-aimed launcher with dotted trajectory preview
- Chain of N balls fired one-by-one each round
- Bricks show HP numbers, decrement on each hit
- Bricks shift down every round — reach the bottom = Game Over
- `+1` ball pickups collected mid-flight
- **Level system**: every 10 rounds = new level with different color theme
- Ball trails, particle explosions on brick death, neon glow effects
- 6 visual themes cycling per level
- Launcher follows first-returned ball's X position each round

---

## Build

### Requirements
- [raylib 5.5](https://www.raylib.com/) — included via [w64devkit](https://github.com/skeeto/w64devkit) bundle
- GCC (MinGW w64devkit recommended on Windows)

### Windows (w64devkit)
```bash
cd brick_breaker
make

cd brick_breaker_hit
make
```

### Linux / macOS
```bash
# Install raylib first (brew install raylib / sudo apt install libraylib-dev)
cd brick_breaker && make
cd brick_breaker_hit && make
```

### CMake (all platforms)
```bash
cd brick_breaker
cmake -B build && cmake --build build
```

---

## Controls

### Brick Breaker (Classic)
| Key | Action |
|---|---|
| `←` / `A` or Mouse | Move paddle |
| `SPACE` | Launch ball |
| `P` / `ESC` | Pause |
| `R` | Restart level (paused) |
| `M` | Toggle sound |
| `H` | Highscores (menu) |

### Brick Breaker Hit
| Input | Action |
|---|---|
| Mouse move | Aim direction |
| Left click | Shoot |
| `M` | Toggle sound |
| `H` | Highscores (menu) |

---

*Built with C99 + raylib 5.5 — zero external assets, all sounds synthesized at runtime.*
