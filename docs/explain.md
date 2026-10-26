# Gun Mayhem — Technical Architecture & Logic Reference

This document explains, in real depth and with real code, how the game is built: the engine, the project layout, the platform/map data model, exactly how the AI opponent works (rule-based, not ML — it reads live game state every frame), the weapon/combat pipeline, the skill/pickup system, and the custom real-time lighting system added for the FA2 research integration. Every claim below points at an actual file and quotes actual code from this repository.

---

## 1. Engine & Tech Stack

- **Engine:** Cocos2d-x 4.0, vendored in-repo at `cocos2d/` and built from source alongside the game (not a separate SDK install).
- **Language:** C++17.
- **Rendering model:** a classic 2D retained-mode scene graph. Everything visible is a `cocos2d::Node` in a tree rooted at the current `Scene`; `Director::getInstance()` owns that tree and calls `draw()`/`update(dt)` on it every frame via a `Renderer`.
- **Build system:** CMake (`CMakeLists.txt`) → Visual Studio project files. The important bit:
  ```cmake
  file(GLOB_RECURSE GAME_SOURCE Classes/*.cpp)
  file(GLOB_RECURSE GAME_HEADER Classes/*.h)
  ```
  Any new `.cpp`/`.h` placed anywhere under `Classes/` is picked up automatically the next time CMake *configures* (not just builds) — which is exactly how the new `Classes/Lighting/` folder (§9) was added with zero `CMakeLists.txt` edits.
- **Platform:** Windows (Win32), built as a 32-bit executable (`Gun-Mayhem.exe`), output to `dist/` together with its `Resources/` folder and the runtime DLLs it depends on (OpenAL, libcurl, zlib, etc.).
- **Window setup** — [`AppDelegate.cpp`](../Classes/AppDelegate.cpp), `applicationDidFinishLaunching()`:
  ```cpp
  cocos2d::Size windowFrameSize(1960.f, 1080.f);
  #if (CC_TARGET_PLATFORM == CC_PLATFORM_WIN32)
      RECT workArea;
      if (SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0)) {
          float availW = static_cast<float>(workArea.right - workArea.left);
          float availH = static_cast<float>(workArea.bottom - workArea.top);
          const float margin = 0.9f; // leave room for the title bar / taskbar
          float scale = std::min(1.0f, std::min(availW * margin / windowFrameSize.width,
                                                   availH * margin / windowFrameSize.height));
          windowFrameSize.width *= scale;
          windowFrameSize.height *= scale;
      }
  #endif
      // resizable=true so the window's maximize button and drag-to-resize work.
      glview = GLViewImpl::createWithRect("Gun-Mayhem", cocos2d::Rect(0, 0, windowFrameSize.width, windowFrameSize.height), 1.0f, true);
      glview->setFrameSize(windowFrameSize.width, windowFrameSize.height);
      ...
      glview->setDesignResolutionSize(testResolutionSize.width, testResolutionSize.height, ResolutionPolicy::FIXED_HEIGHT);
  ```
  Two real bugs were fixed here during this project:
  1. The window used to be a hardcoded `1960×1080`, which on many laptop screens is **physically bigger than the screen**, leaving bottom-row UI (e.g. the AI/Human toggle buttons on the player-setup screen) off-screen and unclickable. It now measures the real monitor work area via the Win32 API and scales itself to fit with a 10% margin.
  2. `GLViewImpl::createWithRect`'s 4th parameter is `resizable`, defaulting to `false` — Cocos2d-x's GLFW backend disables the window's maximize button and resize-by-drag entirely when this is `false`. It's now passed as `true`. Cocos2d-x's own GLFW resize callback (`GLViewImpl::onGLFWWindowSizeFunCallback`, inside the engine) automatically redoes `setFrameSize` + re-applies the `FIXED_HEIGHT` resolution policy on every resize/maximize, so nothing else had to change for the UI to rescale correctly when maximized.

  `ResolutionPolicy::FIXED_HEIGHT` means: whatever the window's actual pixel size is, the *visible height in design units* always equals `testResolutionSize.height` (1440) exactly; the visible *width* stretches or shrinks to match the window's real aspect ratio. This is why every piece of UI in this codebase positions itself using `visibleSize.width * X / 2560` ratios instead of hardcoded pixels — `visibleSize.width` can legitimately differ from 2560 depending on the window's aspect ratio, but `visibleSize.height` is always 1440.

---

## 2. Project Layout

```
Classes/
├── AppDelegate.cpp/.h      Entry point: creates the window, starts the first Scene
├── Manager/                GameManager (keybinds, costume lookup tables, RNG),
│                            UserManager (per-player chosen names/skins/lives, standings)
├── Scene/                  Every full-screen "page" (StartScene, BackgroundChoise,
│                            PlayerSetup, GameScene, AfterGame, Credits, Setting,
│                            Standing, GunLibrary, Loading)
├── Map/                    MapBase + concrete arenas — the actual per-frame game loop
├── Sprite/                 CharacterBase (shared state/physics), PlayerBase/Player2
│                            (human input), AIBase/AI2 (the AI opponent)
├── Organ/                  Modular character body parts assembled at runtime
├── Weapons/                Gun/ (GunBase + per-weapon subclasses), Bullet/, others/ (melee)
├── Skill/                  SkillBase + 6 pickupable power-ups
├── Stuff/                  Package/ (crate spawning/pickup), Blood, Dust (effects)
├── Layer/                  PlayerStateMenu (in-match HUD)
└── Lighting/                FA2 addition: the ReSTIR-inspired dynamic lighting system
```

---

## 3. Scene Flow

```
Loading → StartScene → BackgroundChoise → PlayerSetup → GameScene(Map...) → AfterGame
                          (map + lives)     (AI vs human)                       │
                                                                                  ▼
                                                          Credits / Setting / Standing
```

Scenes pass data to each other through two global singletons rather than constructor arguments:

- **`GameManager`** ([`GameManager.h`](../Classes/Manager/GameManager.h)) — static keybind maps (`map1`/`map2`), the static `BackChoise` (which map index is selected), and static `std::map<std::string, const char*>` lookup tables (`Bodys`, `Heads`, `Hands`, `Feet`, `Hats`) that translate a costume-color name like `"Blue"` into a texture filename like `"bodyBlue.png"`.
- **`UserManager`** — per-player chosen name/skin/face/hat, `player_type[1]`/`[2]` (true = human, false = AI), and `Live` (the lives-per-match count the `BackgroundChoise` screen's text box writes into).

So `BackgroundChoise::applyLives()` ([`BackgroundChoise.cpp`](../Classes/Scene/BackgroundChoise.cpp)) does:
```cpp
void BackgroundChoise::applyLives() {
    std::string livesText = editbox1->getText();
    int lives = 10;
    if (!livesText.empty()) {
        lives = 0;
        for (char digit : livesText) {
            if (!isdigit(static_cast<unsigned char>(digit))) { lives = 10; break; }
            lives = lives * 10 + int(digit - '0');
        }
    }
    UserManager::getInstance()->Live = std::max(1, std::min(lives, 10000));
}
```
and `GameScene::CreateGame` later just reads `GameManager::BackChoise` and `UserManager::Live` back out — no data is passed Scene-to-Scene directly.

### 3.1 `GameScene::CreateGame` — picking the arena

```cpp
Scene* GameScene::CreateGame(int Level)
{
    auto scene = new GameScene();
    if (scene && scene->initGame(Level)) { scene->autorelease(); return scene; }
    ...
}
bool GameScene::initGame(int Level)
{
    switch (Level) {
    case(1): game = MapSpace::createGame();  break;
    case(2): game = MapSunset::createGame(); break;
    case(3): game = MapSeele::createGame();  break;   // dead code path now, see §4.4
    default: game = MapTest::createGame();
    }
    ...
}
```
`BackgroundChoise`'s map buttons set `GameManager::BackChoise` to 0/1/2 only now (Seele's case 3 was removed from the *selectable* UI, §4.4) — `MapSeele` itself is untouched code, simply unreachable from the menus.

---

## 4. The Platform / Map System

### 4.1 `Floor`: the core geometry data structure

Every map ([`MapBase.h`](../Classes/Map/MapBase.h)) stores its platforms as:
```cpp
float floor_height;                       // vertical spacing between floor "rows"
float floor_base;                         // Y of row 0
std::vector<std::vector<float>> Floor;    // Floor[row] = {x1,x2, x3,x4, ...}
```
From [`MapTest.cpp`](../Classes/Map/MapTest.cpp):
```cpp
floor_height = 270;
floor_base   = platform->getContentSize().height + 30 - 270;
Floor = { {0, 1945}, {270, 1675} };
```
Read this as: 2 horizontal rows of platform. Row 0 is at `Y = floor_base`, solid from x=0 to x=1945. Row 1 is at `Y = floor_base + floor_height`, solid from x=270 to x=1675 — a shorter platform floating above the first. Each row's array is a flat `[start, end, start, end, ...]` list, so a row *can* have multiple disconnected solid spans with gaps (holes) between them, though these particular maps only use one span per row. [`MapSpace.cpp`](../Classes/Map/MapSpace.cpp) is a good example of a row with 2 spans and a real gap:
```cpp
Floor = { {50,976,1216, 2143},{373,811,1394,1832},{877,1315},{373,811,1394,1832}, {50,976,1216, 2143}};
```
Row 0 here is solid from 50→976 **and separately** 1216→2143, with a hole in between.

**Why this representation matters:** platforms are always perfectly horizontal and stacked in fixed-height rows — this is a "layered platformer," not a free-form physics world. That constraint is what makes collision, AI pathfinding, *and* the new light occlusion test (§9.3) all cheap: nothing needs a general physics/collision engine; every question reduces to "which row, and is x inside one of its spans?"

### 4.2 `InTheBoundary` — the one function everything else is built on

Defined identically on both `MapBase` and `CharacterBase`:
```cpp
bool InTheBoundary(std::vector<float>& floor, float x) const {
    for (int i = 0; i < floor.size() - 1; i += 2)
        if (x > floor[i] && x < floor[i + 1]) return true;
    return false;
}
```
Answers: *"at this row, is there solid ground under x?"* Used for:
- **Standing/falling** — `CharacterBase::update()` calls it every frame to decide `inTheAir`.
- **Spawn/respawn placement** — reject-sampling a random X on the top row until it lands on solid ground:
  ```cpp
  float x = GameManager::Random(int(map->Floor.back().front()), int(map->Floor.back().back()));
  while (!InTheBoundary(map->Floor.back(), x))
      x = GameManager::Random(int(map->Floor.back().front()), int(map->Floor.back().back()));
  ```
- **AI movement decisions** (§5).
- **The lighting system's shadow/visibility test** (§9.3) — reusing the exact same `Floor` data for light occlusion as for player collision.

### 4.3 Camera / parallax

There's no camera object. Each map's `update(dt)` computes a `delta` — a weighted average of both players' positions, radius-clamped — and moves `background`, `backLayer`, and `platform` by different fractions of it:
```cpp
Vec2 initPlatform = Vec2(platformSize.width / 2, (floor_base + floor_base + floor_height * (Floor.size() - 1)) / 2);
Vec2 delta = Vec2(0, 0);
for (auto& player : players) { delta += player->getPosition(); }
delta = (delta + initPlatform * 3.5) / (players.size() + 3.5) - initPlatform;
...
background->setPosition(initBackgroundPosition - delta * 0.5);
backLayer->setPosition(initbackLayerPosition - delta * 0.5);
platform->setPosition(initPlatformPosition - delta * 0.7);
```
Moving the *world* in the opposite direction of where players drift (at different rates per layer: 0.5×, 0.5×, 0.7×) is what produces both the "keep both players on screen" framing and the depth-parallax illusion.

**Is the projection itself parallel (orthographic) or perspective?** Technically **perspective** — but calibrated to be visually indistinguishable from orthographic for this game. The engine's default (`Director::setProjection`, in `CCDirector.cpp`, never overridden anywhere in this project) builds a real 60°-field-of-view perspective camera:
```cpp
float zeye = this->getZEye();   // = winHeight / (2 * tan(30°))
Mat4::createPerspective(60, width/height, 10, zeye + height/2, &matrixPerspective);
Vec3 eye(width/2, height/2, zeye), center(width/2, height/2, 0.0f), up(0, 1, 0);
Mat4::createLookAt(eye, center, up, &matrixLookup);
```
`zeye` is deliberately solved so that, at `z = 0` (where every sprite in this game lives — nothing ever sets a Z position), one unit of projected space equals exactly one screen pixel, identical to true orthographic projection. The perspective distortion this matrix *would* introduce only shows up away from `z = 0`, and this game never puts anything there. So: a real 3D perspective matrix is running under the hood, every frame, but the "camera motion" described above is pure 2D sprite repositioning — there is no `Camera` object, no Z movement, and no actual 3D perspective effect visible on screen.

### 4.4 The 4 maps

| Class | Rows | Reachable from UI? |
|---|---|---|
| `MapTest` | 2 | yes (default/"weapon testing") |
| `MapSpace` | 5 | yes ("Space Station") |
| `MapSunset` | 4 | yes ("Sunset City") |
| `MapSeele` | 4 | **no** — removed from `BackgroundChoise`'s map-list buttons and its "Random" rotation (`rand() % 3` instead of `% 4`); its selection-screen preview art had unfixable placeholder text baked into the PNG itself |

---

## 5. The Character / Input System — and How the AI Actually Works

### 5.0 The mental model, in one paragraph

There is exactly **one** AI "difficulty" in this game — one concrete class, `AI2`, used for both player slots whenever `UserManager::player_type[n]` is false. It is not a finite-state machine, not a behavior tree, and not machine learning of any kind — it's what's usually called a **priority-ordered reactive rule stack**: a fixed list of checks, run top-to-bottom, every single frame (60 times a second), where the first applicable rule wins and later rules only run if earlier ones didn't already decide what to do this frame. There is no memory of previous frames, no planning horizon, no randomness in its decision-making (only in *which* crate/skill cosmetic gets rolled elsewhere in the game), and no simulated human reaction delay — it re-derives its decision from scratch, instantly, every frame, purely from the current snapshot of game state. The sections below (5.1–5.7) walk through exactly what that rule stack contains and why; §5.8 gives a worked example tracing one concrete scenario through it end to end, and §5.9 states plainly what it does *not* do.

### 5.1 One shared state machine, two different input sources

This is the single most important architectural fact for understanding the AI: **there is no special-cased "AI update path."** Every character — human or AI — is a `CharacterBase`, and `CharacterBase::update(dt)` reads one thing to decide what to do:
```cpp
std::map<std::string, bool> keyMap;   // "up","down","left","right","shot","skill","release"
```
- **Human player** ([`PlayerBase.cpp`](../Classes/Sprite/PlayerBase.cpp)) registers a real keyboard listener:
  ```cpp
  void PlayerBase::initKeyboardListener() {
      EventListenerKeyboard* listenerkeyPad = EventListenerKeyboard::create();
      listenerkeyPad->onKeyPressed  = CC_CALLBACK_2(PlayerBase::onKeyPressed, this);
      listenerkeyPad->onKeyReleased = CC_CALLBACK_2(PlayerBase::onKeyReleased, this);
      _eventDispatcher->addEventListenerWithSceneGraphPriority(listenerkeyPad, this);
  }
  void PlayerBase::onKeyPressed(EventKeyboard::KeyCode keycode, Event* event) {
      if (keycode == keymap["up"])    keyMap["up"] = true;
      if (keycode == keymap["left"])  keyMap["left"] = true;
      if (keycode == keymap["shot"])  keyMap["shot"] = true;
      ...
  }
  ```
  (`keymap` lowercase is the *keybind table* from `GameManager::getKeyMap` — e.g. player 1 uses arrow keys + `1`/`2`; `keyMap` uppercase-M is the *live input state* that `CharacterBase::update()` reads.)
- **AI player** ([`AIBase.cpp`](../Classes/Sprite/AIBase.cpp)) has **no keyboard listener at all**. Every frame:
  ```cpp
  void AIBase::update(float dt) {
      MoveEvent();
      ShotEvent();
      CharacterBase::update(dt);
  }
  ```
  `MoveEvent()`/`ShotEvent()` compute the *same* `keyMap["up"]`, `keyMap["left"]`, `keyMap["shot"]`, etc. directly from game-state heuristics, **before** the shared `CharacterBase::update(dt)` runs and consumes them.

So "the AI" is a function that pretends to be a keyboard, writing into the exact same input slot a human's keypress would — not a separate movement/physics implementation. Everything downstream (gravity, jump arcs, firing cooldowns, knockback) is identical code for both.

### 5.2 How `keyMap` actually drives physics — `CharacterBase::update()`

```cpp
if (!inTheAir) {
    y_speed = 0;
    floor_actual = floor;
    isDoubleJump = false;
    if (!InTheBoundary(map->Floor[floor], getPositionX())) {
        keyMap["down"] = true;          // walked off the edge -> start falling
        inTheAir = true;
        if (floor > 0) floor--;
    }
    else if (keyMap["up"]) {
        inTheAir = true;
        y_speed = status->y_maxSpeed;   // jump impulse
        MoveDelay(true, false);
        DustUpdate();
        keyMap["up"] = false;
    }
    else if (keyMap["down"]) {
        if (floor > 0) { inTheAir = true; floor--; }   // drop through to the row below
    }
}
if (inTheAir) {
    StopWalk();
    if (!isDoubleJump && keyMap["up"]) {                // mid-air double jump
        y_speed = status->y_maxSpeed / 1.2;
        isDoubleJump = true;
        DrawHalo();
        keyMap["up"] = false;
    }
    y_speed += status->gravitation * dt;                // gravity integration
    if (y_speed < -status->y_maxSpeed * 2.4) y_speed = -status->y_maxSpeed * 2.4; // terminal velocity
}
```
and horizontal movement:
```cpp
if ((keyMap["left"] ^ keyMap["right"])) {
    Flip(keyMap["left"]);
    accelerate = keyMap["right"] ? status->acceleration : -status->acceleration;
    x_speed += accelerate * dt;
    ...
}
```
`status` (a per-character `Status` struct: `acceleration`, `gravitation`, `resistance`, `x_maxSpeed`, `y_maxSpeed`, `recoil_speed`) is set per-concrete-class — e.g. [`AI2.cpp`](../Classes/Sprite/AI2.cpp):
```cpp
status->acceleration = 1500 * 1.5;
status->gravitation  = -2757;
status->x_maxSpeed   = 448 * 1.5;
status->y_maxSpeed   = 1275;
```
This is the only place "AI vs human" physics could *theoretically* differ, and in practice `AI2` and `Player2` use the same tuning — the AI gets no speed/jump advantage, it only gets different (synthetic) *input*.

### 5.3 `AIBase::MoveEvent()` — the movement/positioning decision tree

Run every frame, in priority order:

1. **If invalid (just died), reset all keys** — `reset()` clears stale input so a dead character doesn't keep "walking."
2. **Item-fetching override** — if the AI and its opponent are 2+ floors apart, *or* the opponent is near a map edge (`Readytodie`, §5.5), and the AI doesn't already hold a non-default gun or an active skill:
   ```cpp
   for (auto package : this->map->packageEvent->packages) {
       if (!package->intheair) {
           if (((this->floor - package->endFloor) * (this->floor - opponent->floor) <= 0 && abs(this->floor - opponent->floor) >= 2) || Readytodie(opponent)) {
               double temp_distance = sqrt(pow(getPositionX()-package->getPositionX(),2) + pow(getPositionY()-package->getPositionY(),2));
               if (distance_package > temp_distance) { distance_package = temp_distance; close_package = package; }
           }
       }
   }
   ```
   finds the nearest "useful" crate (one on a floor between the AI and the opponent) and either walks toward it (if already on its floor) or calls `jumpTofloor()` to climb/descend toward it.
3. **Combat positioning** — if the opponent isn't about to fall off:
   ```cpp
   int distance = 100 * 5; // 500px standoff
   if (this->floor_actual == opponent->floor_actual) {
       if (opponent->getPosition().x - this->getPosition().x > distance)       { right = true; }
       else if (opponent->getPosition().x - this->getPosition().x < -distance) { left = true; }
       else { /* close enough, stop */ }
   }
   else if (!this->inTheAir && this->floor != opponent->floor) FindFloor();
   ```
4. **Double-jump guard** — cancel a held "up" if airborne with the double-jump already spent.
5. **Airborne platform-edge correction** — while falling, if the AI has drifted past its current platform's edge, force movement back toward it and hold "up" (a recovery attempt) instead of falling into a gap.
6. **Speed clamp** — if `|x_speed|` exceeds the configured max (can happen from knockback), force input opposite to it to bleed the excess off.
7. **Reactive dodge** — if on the same floor and the opponent is currently firing, jump. A reflex, not a prediction.

### 5.4 Pathfinding between floors: `FindFloor()` / `jumpTofloor()`

There's no graph search — it's a greedy, one-hop heuristic:
```cpp
void AIBase::jumpTofloor(int up, int step) {
    if (step < 0) return;
    std::string keyboard = up == 1 ? "up" : "down";
    float add_width = 20;
    int target_floor = this->floor + up;
    // Commit to jumping once close enough to the target row's edge...
    if (!opponent->inTheAir && getPositionX() > Floor[target_floor][step]-60 && getPositionX() < Floor[target_floor][step+1]+60
        || /* ...or already standing right at this row's own edge */ ) {
        keyMap[keyboard] = true;
    }
    // ...otherwise walk toward whichever edge of the target row is closer.
    if (getPositionX() <= Floor[target_floor][step] + 100)      { keyMap["right"] = true; }
    else if (getPositionX() >= Floor[target_floor][step+1] - 100) { keyMap["left"] = true; }
}
```
`FindFloor()` picks the direction (`up = floor < opponent->floor ? 1 : -1`) and which `step` (platform span index) to aim for — either the specific span under the opponent (if 1 row away) or whichever span-edge on the next row is geometrically closest to the AI's current X. `find_thisStep()` is the inverse lookup: given the AI's current X, which span index is it standing on right now (needed so `jumpTofloor` can tell "am I already at my own platform's edge").

This produces believable "walks to the platform edge, then hops toward you" behavior with pure arithmetic — no search — because the row-based geometry makes "which direction and how far" a direct comparison, not a pathfinding problem.

### 5.5 `Readytodie()` — the edge-safety / kill-opportunity heuristic

```cpp
bool AIBase::Readytodie(CharacterBase* player) {
    if (player->getPositionX() <= map->Floor[player->floor].front() ||
        player->getPositionX() >= map->Floor[player->floor].back())
        return true;
    else if (player->getPositionY() < map->floor_base)
        return true;
    return false;
}
```
A cheap proxy for "about to fall off the level" — no forward physics simulation, just "is X already past this row's outer boundary, or Y below the lowest floor." Used to decide whether the AI should push an opponent who's already vulnerable rather than going to fetch an item.

### 5.6 `AIBase::ShotEvent()` — aiming and firing

```cpp
void AIBase::ShotEvent() {
    if (this->gun->releaseToShot && this->keyMap["shot"]) {
        this->keyMap["shot"] = false;      // momentarily release a charge-to-fire weapon
        Sleep(1);
    } else {
        if ((opponent->floor != map->Floor.size()-1 || !opponent->inTheAir) && this->valid) {
            if (opponent->getPosition().x - this->getPosition().x < 0 && std::abs(opponent->getPositionY()-this->getPositionY()) < 400) {
                this->Flip(true);  this->keyMap["shot"] = true;
            } else if (opponent->getPosition().x - this->getPosition().x > 0 && std::abs(opponent->getPositionY()-this->getPositionY()) < 400) {
                this->Flip(false); this->keyMap["shot"] = true;
            } else { this->keyMap["shot"] = false; }
        }
    }
    if (std::abs(opponent->getPositionY() - this->getPositionY()) > 100) this->keyMap["shot"] = false;
}
```
"Aim" is binary: if roughly level with the opponent (within 400px vertically, hard cutoff at 100px for actually committing to fire), face them and hold the fire key. There's no aim-assist, no bullet-drop compensation — accuracy comes entirely from the weapon's own ballistics, not the AI.

### 5.7 Summary — what the AI actually "reads"

Every frame, directly and only: the live `CharacterBase* opponent` pointer's current X/Y, floor index, airborne state, and currently-firing flag, plus the shared `MapBase::Floor` geometry and `packageEvent->packages` list. No prediction, no memory of past behavior, no learning — a reactive rule set re-evaluated from scratch every frame.

### 5.8 Worked example — tracing one frame through the rule stack

Concrete scenario: the AI is on floor 1 of a 4-row map, its opponent is on floor 3 (two rows up) standing near the right edge of their platform, and there's a weapon crate sitting on floor 2. Here's exactly which code path fires, in order, for `MoveEvent()`:

1. **Step 1 (reset)** — skipped; the AI is `valid` (alive), so nothing to reset.
2. **Step 2 (item-fetching override)** — `abs(this->floor - opponent->floor) = 2`, which satisfies `>= 2`, **and** the crate on floor 2 sits between the AI's floor (1) and the opponent's floor (3) (the `(this->floor - package->endFloor) * (this->floor - opponent->floor) <= 0` check passes: `(1-2)*(1-3) = (-1)*(-2) = 2`... wait — that's `> 0`, meaning it actually *fails* this particular check in this exact layout; the real condition only passes when the package floor is *between* the two in a sign-changing sense. This is deliberately shown here, not smoothed over, because it's the honest answer to "trace the actual logic": this specific geometry might fall through to step 3 instead, which is exactly the kind of edge case worth stepping through in a debugger rather than assuming from reading the formula alone — the arithmetic, not intuition, decides it.
3. **Assuming step 2 does *not* claim the frame**, step 3 (combat positioning) runs: `floor_actual` (1) `!= opponent->floor_actual` (3), `inTheAir` is false, `floor != opponent->floor` → calls `FindFloor()`.
4. **Inside `FindFloor()`**: `up = floor(1) < opponent->floor(3) ? 1 : -1` → `up = 1` (needs to climb). Since `abs(1-3) = 2 ≠ 1`, it's *not* the "one row away, target the opponent's exact span" case — it falls into the distance-comparison branch: compare "is there a closer/more useful package than just closing the gap" (here, yes — the floor-2 crate), so `colsePackage` logic may redirect the climb target toward the crate's platform edge instead of straight at the opponent, depending on the relative squared-distance comparison on that line.
5. **`jumpTofloor(1, step)`** is called with whatever `step` resulted: the AI walks horizontally toward the computed target-row span edge (within the `add_width` tolerances), and once within ~60px of that edge with the opponent not currently airborne, `keyMap["up"] = true` gets set for that frame.
6. **Back in the shared `CharacterBase::update()`** (§5.2): since the AI is `!inTheAir` and `keyMap["up"]` is now true, the jump-impulse branch fires — `y_speed = status->y_maxSpeed`, physics takes over identically to a human's jump.
7. **Independently, the same frame**, `ShotEvent()` (§5.6) also ran: opponent is on a different floor, likely outside the ±400px *vertical* window relative to this AI's current Y, so `keyMap["shot"]` stays false this frame — no firing while mid-climb, which is an emergent consequence of the vertical-distance gate, not anything explicitly coded as "don't shoot while jumping."

The takeaway: nothing here is scripted as "if climbing, don't shoot" — that behavior *emerges* from two independent, simple checks (the floor-climb logic and the vertical-aim-window check) both reading the same underlying positions. This is characteristic of the whole AI: a handful of cheap, independent rules whose *interactions* produce what looks like coordinated behavior, with no central "decide what I'm doing right now" coordinator tying them together.

### 5.9 Honest limitations — what the AI does not do

- **It cannot throw bombs.** The dedicated bomb-throw action (§6.4/§6.5 — bound to the `"skill"` key) is only ever set `true` by `PlayerBase`'s real keyboard handler; `AIBase`/`AI2` never writes to `keyMap["skill"]` anywhere. An AI opponent will never use the explosive kit, whether picked up as `Gun_Bomb` or via the dedicated key — this is a genuine, current gap, not a design choice, and the fix would be a straightforward addition to `AIBase::MoveEvent()` or a new method following the exact same pattern as `ShotEvent()`.
- **No difficulty levels.** `AI2` is the only AI class; there's no "easy/medium/hard" — every AI opponent in the game, regardless of menu selection, runs identically tuned logic with identical reaction speed (instant, every frame).
- **No learning, no memory, no prediction.** Every decision is a pure function of the *current* frame's positions — it cannot anticipate where the opponent is *about* to be, cannot recognize a repeated player strategy, and resets its entire "plan" from scratch 60 times a second.
- **No awareness of weapon type beyond "is it the default."** The item-fetching logic (§5.3 step 2) only checks `this->gun == this->initGun` — it doesn't reason about whether the newly available weapon is actually *better* than what it's already holding, only whether it's holding *something non-default* at all.
- **Aim has no skill curve.** As covered in §5.6, "aim" is a hard binary gate (within 400px vertically) — the AI is never more or less accurate; it either faces-and-fires or doesn't, with the weapon's own spread/ballistics as the only source of a miss.

---

## 6. Weapons & Combat

### 6.1 `GunBase` — shared gun behavior

[`GunBase.h/.cpp`](../Classes/Weapons/Gun/GunBase.h):
```cpp
class GunBase : public Sprite {
public:
    virtual void Shot(MapBase* map, bool right);
    virtual void SetBullet();
    void update(float dt) override;
    float shotInterval, recoilSpeed, bulletSpeed, hitSpeed;
    int bulletClip, bulletCount;
    bool shot = true, fire = false, isSniper = false, releaseToShot = false;
    ...
};
void GunBase::update(float dt) {
    deltatime += dt;
    if (deltatime < shotInterval && bulletCount != 0)   shot = false;  // still in fire-rate cooldown
    else if (bulletCount > bulletClip)                  shot = false;  // clip empty
    else                                                shot = true;
}
```
Each concrete gun (`Gun_Glock`, `Gun_Revolver`, `Gun_MG_*`, `Gun_Modern_Sniper`, `Gun_Double`, `Gun_Gatling`) overrides `SetBullet()` to spawn the actual projectile:
```cpp
// Gun_Glock.cpp
void Gun_Glock::SetBullet() {
    GunBase::SetBullet();
    BulletCase::create(map->platform, GetPositionToBackground(true), Vec2(10, 30), this->_flippedX, 400, 800);
    map->bullets.push_back(Bullet::create(player, GetPositionToBackground(true), Vec2(50, 30)));
}
```
(`BulletCase` is the purely-visual ejected-shell-casing effect; `Bullet` is the actual hit-registering projectile.)

### 6.2 `Bullet` — the projectile

[`Bullet.cpp`](../Classes/Weapons/Bullet/Bullet.cpp):
```cpp
bool Bullet::init(CharacterBase* player, Vec2 init, Vec2 offset) {
    ...
    this->bulletSpeed = player->isFlippedX() ? -player->gun->bulletSpeed : player->gun->bulletSpeed;
    currentPoint = init + offset;
    this->setPosition(currentPoint);
    back->addChild(this, 0);
    this->scheduleUpdate();

    player->map->SpawnMuzzleFlash(init);        // FA2 lighting hook (§9)
    player->map->SpawnBulletGlow(currentPoint); // FA2 lighting hook (§9)
    return true;
}
void Bullet::update(float dt) {
    currentPoint.x += bulletSpeed * dt;   // constant-velocity, horizontal only
    this->setPosition(currentPoint);
}
```

### 6.3 Hit detection — `MapBase::ShotEvent()`

Centralized, not per-bullet: every frame, every live bullet is tested against every player's body bounding box, skipping the shooter's own team:
```cpp
void MapBase::ShotEvent() {
    for (auto& bullet : bullets) {
        if (bullet->getPositionX() > 12000 || bullet->getPositionX() < -6000) { bullet->removeFromParent(); bullet=nullptr; continue; }
        for (auto& player : players) {
            auto rect = player->body->organ->getBoundingBox();
            rect.origin += player->getPosition() + player->body->getPosition();
            if (rect.containsPoint(bullet->getPosition()) && player->valid && bullet->player != nullptr) {
                if (!(bullet->player->tag == player->tag)) {
                    bullet->player->hitCount++;
                    player->x_speed += player->defense ? bullet->hitSpeed/10 : bullet->hitSpeed;  // Defense skill halves... actually 1/10s knockback
                    player->hit = true;
                    if (!player->defense) { auto blood = Blood::create(); blood->setPosition(bullet->getPosition()); platform->addChild(blood,4); }
                    bullet->removeFromParent(); bullet = nullptr;
                    break;
                }
            }
        }
    }
}
```
Melee weapons (`BaseBall_Bat`, `Samurai_Sword`, in `Weapons/others/`) don't spawn bullets at all — they run their own swing-arc hit-test logic instead.

### 6.4 The Explosive Kit — `Gun_Bomb` + `Bomb`

The project README always listed a "special weapons explosive kit" among the game's features, but no such weapon existed in the code until now. [`Gun_Bomb`](../Classes/Weapons/Gun/Gun_Bomb.h) is a `GunBase` subclass exactly like `Gun_Glock` — same hand/throw choreography, reused verbatim since holding and switching a one-handed weapon isn't bomb-specific — except its `SetBullet()` spawns a [`Bomb`](../Classes/Weapons/Bullet/Bomb.h) instead of a `Bullet`:
```cpp
void Gun_Bomb::SetBullet() {
    GunBase::SetBullet();
    Bomb::create(player, GetPositionToBackground(true), Vec2(50, 30));
}
```
Unlike `Bullet`, a `Bomb` doesn't register itself in `MapBase::bullets` — it owns its entire lifecycle itself:
```cpp
void Bomb::update(float dt) {
    vy += GRAVITY * dt;                 // simple ballistic arc, no bounce
    currentPoint.x += vx * dt;
    currentPoint.y += vy * dt;
    this->setPosition(currentPoint);
    this->setRotation(this->getRotation() + 360.f * dt);

    fuseTime -= dt;
    if (fuseTime <= 0.f || currentPoint.y < player->map->death_line) {
        Explode();
    }
}
```
`Explode()` does three things: registers an `Explosion` light with the lighting system (`map->SpawnExplosion(currentPoint)` — see §9, this is the light type added alongside muzzle flash and bullet glow), applies radius-and-falloff knockback plus `Blood` effects to any *other* player within blast radius (the thrower is excluded, matching the existing no-friendly-fire convention from `MapBase::ShotEvent`), and draws one immediate `DrawNode` flash so the blast reads instantly regardless of how the tile-based lighting glow resolves that frame. It was wired into the existing weapon-pickup rotation with a one-line change to [`GunPackage.cpp`](../Classes/Stuff/Package/GunPackage.cpp) (`rand() % 10` → `rand() % 11`, one new `case`), so it drops from crates exactly like every other gun — no new input/keybind plumbing was needed anywhere.

### 6.5 Death & Respawn — the full lifecycle

Death is **not** triggered directly by taking damage — a bullet or explosion only ever applies knockback (`x_speed`/`y_speed` changes). Death is triggered indirectly, purely by position, every frame inside `CharacterBase::update()`:
```cpp
if (getPositionY() < map->death_line) {
    GunChange(initGun->clone());   // lose whatever weapon was held, reset to the starting gun
    CallFunc* func1 = CallFunc::create(CC_CALLBACK_0(CharacterBase::Death, this));
    CallFunc* func2 = CallFunc::create([&]() {
        skills.push_back(new Defense(this, 3));
        this->setVisible(true); valid = true;
    });
    float x = GameManager::Random(int(map->Floor.back().front()), int(map->Floor.back().back()));
    while (!InTheBoundary(map->Floor.back(), x))
        x = GameManager::Random(int(map->Floor.back().front()), int(map->Floor.back().back()));
    Vec2 position = Vec2(x, map->platform->getContentSize().height + 2000);
    auto move1 = MoveTo::create(0.7, position);
    auto move2 = EaseSineOut::create(move1);
    this->runAction(Sequence::create(func1, move2, func2, nullptr));
}
```
`death_line` is a per-map constant (e.g. `MapTest.cpp` sets `death_line = 0;`) set **well below the lowest platform row** — this is a separate, lower threshold than the per-row floor checks that handle ordinary "stepped off a platform, start falling" physics. A character has to fall all the way past every platform in the stack — not just off one ledge — before this actually fires.

This is a 3-step action **sequence**, run in this exact order:

1. **`Death()`** ([`CharacterBase.cpp`](../Classes/Sprite/CharacterBase.cpp)):
   ```cpp
   void CharacterBase::Death() {
       valid = false;
       this->setVisible(false);
       this->Flip(false);
       y_speed = 0; x_speed = 0;
       floor = map->Floor.size() - 1;
       for (auto& skill : skills) { delete skill; skill = nullptr; }
       skills.clear();
       this->map->runAction(Shake::createWithStrength(0.16, 25, 10));  // camera shake, on the MapBase Layer itself
       Live--;
       if (Live == 0) this->stopAllActions();
       firstLand = true;
   }
   ```
   `valid = false` is the important flag here — it's what every other system (`MapBase::ShotEvent`'s hit test, `PackageEvent`'s pickup test, the AI's targeting logic) checks to treat a dead character as "not really there." The screen shake runs on `map` (the `MapBase` Layer) specifically — not on `platform` — so it doesn't fight with the camera-parallax code in `MapTest::update()` that repositions `platform` every frame (§4.3); two independent systems can both reposition different nodes without conflicting. `Live--` is also where "out of lives" is actually decided: `MapBase::update()`'s `if (player->Live == 0) GameOver(player)` check (§3.1/§6) is what picks this up next frame.
2. **The move** — while still invisible, the character is eased (`EaseSineOut`) over 0.7 seconds to a freshly rolled respawn position: a random X on the **top platform row** (`Floor.back()`, the same row used for the player's very first spawn at match start and for crate placement in `PackageEvent`), reject-sampled against `InTheBoundary` exactly like initial spawn, at a Y far above the arena (`platform content height + 2000`). Because `setVisible(false)` already ran, this 0.7s move happens **entirely off-screen** — there's no visible "falling from the sky" animation; the character is simply absent, then reappears already in place once step 3 runs.
3. **The re-enable callback**: grants a fresh `Defense` skill (§7.1 — a 3-second invulnerability bubble, reusing the exact same mechanic that protects a character mid-match), then `setVisible(true)` and `valid = true` together — the character is visually and logically "back" in the same frame.

Net effect: respawn is a blend of real physics-adjacent choices (why `valid` has to gate three unrelated systems, why the shake targets a different node than the camera code) and one small, honest rough edge (the fall animation is invisible) — exactly the kind of implementation detail that's easy to miss just by reading `Death()` in isolation without following the whole action sequence it's embedded in.

---

## 7. Skills & Pickups

### 7.1 `SkillBase` and an example skill

[`SkillBase.h`](../Classes/Skill/SkillBase.h):
```cpp
enum SkillType { DEFENSE, JETPACK, SPEED_UP, SUPER_JUMP, DOUBLE_TEAM, EXTRA_LIFE, NONE };
class SkillBase {
public:
    virtual void update(float dt);
    CharacterBase* player;
    SkillType skillTpye;
    bool skillEnd = false;
    float Duration, duration;   // Duration = total length, duration = elapsed
};
```
Every `CharacterBase` holds `std::vector<SkillBase*> skills`. [`Defense.cpp`](../Classes/Skill/Defense.cpp) is the simplest example — a timed invulnerability bubble, granted automatically on every respawn:
```cpp
Defense::Defense(CharacterBase* player, float time) {
    skillTpye = DEFENSE;
    this->player = player;
    Duration = time;
    player->defense = true;                        // flag MapBase::ShotEvent() checks to reduce knockback/skip blood
    cover = Sprite::create("defense_bubble.png");
    player->addChild(cover, 100);
    auto seq = Sequence::create(DelayTime::create(time-0.5), FadeOut::create(0.5), nullptr);
    cover->runAction(seq);
}
Defense::~Defense() { cover->removeFromParent(); player->defense = false; }
```
(`CharacterBase::update()`'s death handler does exactly this: `skills.push_back(new Defense(this, 3));` on respawn.)

### 7.2 `PackageEvent` — crate spawning and pickup

[`PackageEvent.cpp`](../Classes/Stuff/Package/PackageEvent.cpp):
```cpp
void PackageEvent::update(float dt) {
    if (gunUpdateTime > GunPackage::updatetime) {
        gunUpdateTime = 0;
        packages.push_back(GunPackage::createWithGun(map));
    } else gunUpdateTime += dt;

    if (skillUpdateTime > SkillPackage::updatetime) {
        skillUpdateTime = 0;
        packages.push_back(SkillPackage::createWithSkill(map));
    } else skillUpdateTime += dt;
}
void PackageEvent::PackageUpdate(std::vector<CharacterBase*>& players) {
    for (auto& package : packages) {
        if (package->getPositionY() < map->death_line || package->Disappear()) { package->removeFromParent(); package=nullptr; continue; }
        for (auto& player : players) {
            auto rect = player->body->organ->getBoundingBox();
            rect.origin += player->getPosition() + player->body->getPosition();
            if (rect.containsPoint(package->getPosition()) && player->valid && !package->intheair) {
                package->GetPackage(player);
                package->removeFromParent(); package = nullptr;
                break;
            }
        }
    }
}
```
This is the exact same `packages` list the AI scans in §5.3/§5.4 to decide whether a crate is worth detouring for — there's no separate "AI view" of the world, it reads the live game objects directly.

---

## 8. Character Composition (`Organ/`)

A character is not one sprite. `CharacterBase` assembles `Body`, `Head`, `Face`, `Hat`, two `Hand`s, two `Foot`s — all `OrganBase` subclasses — as child nodes at fixed relative offsets. Each organ class is tiny, e.g. [`Body.h`](../Classes/Organ/Body.h):
```cpp
class Body : public OrganBase {
public:
    bool initWithName(const char* name);
    static Body* CreateWithName(const char* name);
};
```
and [`AI2::initBody()`](../Classes/Sprite/AI2.cpp) shows how the pieces come together, with each organ's actual texture resolved through `GameManager`'s static lookup tables by the player's chosen costume name:
```cpp
std::string player_name = manager->player_skin[this->getTag()];
body = Body::CreateWithName(GameManager::Bodys[player_name]);   // e.g. Bodys["Blue"] == "bodyBlue.png"
head = Head::CreateWithName(GameManager::Heads[player_name]);
hat  = Hat::CreateWithName(GameManager::Hats[hat_name]);
...
head->setPosition(Vec2(65/2.2 - 8, 130/2.2 - 2));
this->addChild(head, 1);
hand1->initPosition = Vec2(-31/2.2, -55/2.2);
this->addChild(hand1, 4);
```
This is how the cosmetic picker in `PlayerSetup` can change a character's whole look by just swapping which costume-name string gets looked up — no new sprite classes are needed per color/costume.

---

## 9. The Lighting Module (`Classes/Lighting/`) — FA2 Addition

This implements the FA1 research survey's two contributions — **ELCD** (Ephemeral Light Confidence Decay) and **AHTR** (Adaptive Hierarchical Tile Resampling), from the ReSTIR/spatiotemporal-reservoir-resampling literature — scoped to this game's actual combat lighting: muzzle flashes, bullet glow, and explosions. The research-to-theory mapping lives in [`docs/FA2_PPT_Content.md`](./FA2_PPT_Content.md); this section is the code walkthrough.

**Integration footprint** (everything else in the game is untouched):

| File | What was added |
|---|---|
| `Classes/Map/MapBase.h` | forward-declares `Lighting::LightingPass`, a `Lighting::LightingPass* lighting` member, and 3 wrapper method declarations |
| `Classes/Map/MapBase.cpp` | constructs `lighting` once players exist (end of `initPlayer()`), calls `lighting->Update(dt)` in `update()`, implements the 3 wrappers, `delete`s it in the destructor |
| `Classes/Weapons/Bullet/Bullet.cpp` | 2 lines in `Bullet::init()`: `player->map->SpawnMuzzleFlash(init); player->map->SpawnBulletGlow(currentPoint);` |
| `Classes/Weapons/Bullet/Bomb.cpp` | 1 line in `Bomb::Explode()`: `map->SpawnExplosion(currentPoint);` (§6.4) |

### 9.1 `LightSource.h` — the data type and the ELCD confidence formula

```cpp
enum class LightType { MuzzleFlash, BulletGlow, Explosion };

struct LightSource {
    int id = -1;
    cocos2d::Vec2 position;
    cocos2d::Color4F color = cocos2d::Color4F::WHITE;
    float radius = 0.f;
    float initialLifetime = 0.f;
    float remainingLifetime = 0.f;
    LightType type = LightType::BulletGlow;

    // kappa_l(t) from the ELCD theorem: 1.0 when freshly spawned, 0.0 once expired.
    float confidence() const {
        if (initialLifetime <= 0.f) return 0.f;
        return cocos2d::clampf(remainingLifetime / initialLifetime, 0.f, 1.f);
    }
};
```
This `confidence()` function *is* the FA1 survey's Theorem 1/2 directly translated into code: `κ_l(t) = τ_remaining / τ_initial`.

### 9.2 `LightManager` — owns the lights for one match

```cpp
void LightManager::SpawnMuzzleFlash(const cocos2d::Vec2& position) {
    SpawnLight(position, LightType::MuzzleFlash, 3.f/60.f, 55.f, cocos2d::Color4F(1.f, 0.75f, 0.3f, 1.f));
}
void LightManager::SpawnBulletGlow(const cocos2d::Vec2& position) {
    SpawnLight(position, LightType::BulletGlow, 1.f/60.f, 22.f, cocos2d::Color4F(1.f, 0.95f, 0.6f, 1.f));
}
void LightManager::SpawnExplosion(const cocos2d::Vec2& position) {
    SpawnLight(position, LightType::Explosion, 45.f/60.f, 130.f, cocos2d::Color4F(1.f, 0.45f, 0.12f, 1.f));
}
void LightManager::Update(float dt) {
    for (auto& light : lights_) light.remainingLifetime -= dt;
    lights_.erase(std::remove_if(lights_.begin(), lights_.end(),
        [](const LightSource& l) { return l.remainingLifetime <= 0.f; }), lights_.end());
}
```
Lifetimes (3 frames for a muzzle flash, 1 frame for bullet glow, 45 frames for an explosion, all at 60fps) match the FA1 poster's spec exactly — the explosion is deliberately both the longest-lived and the largest-radius of the three, since it's also the primary ephemeral-light test case the FA1 proposal always intended (§6.4). `LightManager` is a plain member, not a singleton — each `MapBase` (each match) owns its own, so state resets cleanly between games.

### 9.3 `Visibility.h/.cpp` — the "shadow ray"

```cpp
bool IsVisible(const cocos2d::Vec2& a, const cocos2d::Vec2& b,
               const std::vector<std::vector<float>>& floorRows,
               float floorBase, float floorHeight)
{
    float yLow = std::min(a.y, b.y), yHigh = std::max(a.y, b.y);
    for (size_t row = 0; row < floorRows.size(); ++row) {
        float rowY = floorBase + floorHeight * static_cast<float>(row);
        if (rowY <= yLow || rowY >= yHigh) continue;          // this row isn't between a and b
        float t = (rowY - a.y) / (b.y - a.y);
        float x = a.x + t * (b.x - a.x);                      // where the line crosses this row's height
        if (RowBlocks(floorRows[row], x)) return false;       // solid platform there -> occluded
    }
    return true;
}
```
This reuses `MapBase::Floor`/`floor_base`/`floor_height` — the exact same data `InTheBoundary` uses for player collision (§4.2) — as the occlusion geometry for light. No separate collision representation was built.

### 9.4 `Reservoir.h` — the ReSTIR sampling core

```cpp
struct Reservoir {
    int lightId = -1;
    cocos2d::Vec2 position;
    cocos2d::Color4F color;
    float radius = 0.f;
    float weightSum = 0.f;
    float M = 0.f;   // number of samples folded in so far

    // Candidate generation pass (ReSTIR Pass 1): fold one new candidate in.
    bool Update(int id, float weight, const cocos2d::Vec2& pos, const cocos2d::Color4F& c, float r) {
        if (weight <= 0.f) return false;
        weightSum += weight; M += 1.f;
        float rnd = cocos2d::RandomHelper::random_real<float>(0.f, 1.f);
        if (rnd * weightSum <= weight) { lightId=id; position=pos; color=c; radius=r; return true; }
        return false;
    }

    // Temporal reuse pass (ReSTIR Pass 2), ELCD-aware: confidenceScale is kappa_l(t).
    void CombineTemporal(const Reservoir& prev, float confidenceScale, float mCap) {
        if (prev.lightId < 0 || prev.M <= 0.f || confidenceScale <= 0.f) return;  // <- ELCD kill switch
        float cappedM = std::min(prev.M, mCap);
        float w = prev.weightSum * (cappedM / prev.M) * confidenceScale;
        if (w <= 0.f) return;
        weightSum += w; M += cappedM;
        if (cocos2d::RandomHelper::random_real<float>(0.f,1.f) * weightSum <= w) {
            lightId = prev.lightId; position = prev.position; color = prev.color; radius = prev.radius;
        }
    }
    float FinalWeight() const { return M > 0.f ? weightSum / M : 0.f; }  // unbiased RIS estimator
};
```
The line `if (prev.lightId < 0 || prev.M <= 0.f || confidenceScale <= 0.f) return;` is where ELCD actually lives in code: when `confidenceScale` (the expired light's `κ_l(t)`) is 0, the previous frame's sample contributes **nothing**, instead of ReSTIR's standard behavior of letting a capped-but-nonzero `M` keep an expired light's influence alive for up to `mCap` (20) frames.

### 9.5 `LightingPass` — orchestration, one frame

[`LightingPass.cpp`](../Classes/Lighting/LightingPass.cpp), `ComputeFrame()`, per tile of a fixed 16×9 grid over the play area:
```cpp
// 1. Candidate generation (RIS) over lights overlapping this tile.
Reservoir combined = BuildCandidateReservoir(lights, tile.bounds, &candidates);

// 2. AHTR: >1 competing light in a tile is the "high variance" signal -- spend
//    one extra level of resolution (2x2 refinement) only there.
if (ahtrEnabled_ && candidates.size() > 1) {
    Reservoir best = combined;
    for (int q = 0; q < 4; ++q) {
        Reservoir sub = BuildCandidateReservoir(lights, Quadrant(tile.bounds, q), nullptr);
        if (sub.FinalWeight() > best.FinalWeight()) best = sub;
    }
    combined = best;
}

// 3. Temporal reuse, ELCD-decayed.
if (tile.prevReservoir.lightId >= 0) {
    float confidence = 1.f;
    if (elcdEnabled_) {
        const LightSource* prevLight = lightManager_.FindById(tile.prevReservoir.lightId);
        confidence = prevLight ? prevLight->confidence() : 0.f;   // not found == expired == 0
    }
    combined.CombineTemporal(tile.prevReservoir, confidence, 20.f);
}

// 4. The one shadow ray for this tile.
if (combined.lightId >= 0 && combined.FinalWeight() > 0.0001f) {
    ++shadowTestsThisFrame_;
    bool visible = IsVisible(tileCenter, combined.position, map_->Floor, map_->floor_base, map_->floor_height);
    if (visible) tile.hasLight = true;
    else combined.weightSum = 0.f;   // occluded -> drop, and this zero carries into next frame's prev
}
tile.reservoir = combined;
tile.prevReservoir = combined;   // persisted for next frame's temporal reuse
```
`Render()` then draws every tile with `hasLight == true` as a small soft `DrawNode::drawDot`, and (when the debug HUD is on) formats a stats string:
```cpp
std::snprintf(buf, sizeof(buf),
    "Lighting Debug (F1 toggle)\nELCD [F2]: %s   AHTR [F3]: %s\nActive lights: %d\n"
    "Shadow tests this frame: %d / %d tiles\nGhost streak: %d frames (max %d)",
    elcdEnabled_?"ON":"OFF", ahtrEnabled_?"ON":"OFF",
    activeLightsThisFrame_, shadowTestsThisFrame_, (int)tiles_.size(),
    currentGhostStreak_, maxGhostStreak_);
```
`currentGhostStreak_` increments whenever `ComputeFrame()` finds a tile displaying a light (`tile.hasLight`) whose `lightId` no longer exists in `LightManager` — i.e. a *literal, measured* ghost frame, using the same definition the FA1 benchmark used.

### 9.6 Debug controls

Registered once in `LightingPass`'s constructor via a standalone `EventListenerKeyboard` (independent of the gameplay keybind system in §5.1, so it never conflicts with either player's controls):

| Key | Effect |
|---|---|
| **F1** | toggle the debug HUD |
| **F2** | toggle ELCD on/off live |
| **F3** | toggle AHTR on/off live |

### 9.7 Known, deliberate simplifications

- CPU, 2D, **tile**-granularity (16×9 grid), not GPU per-pixel — appropriate for a 2D sprite game, not a claim of matching the original papers' GPU numbers.
- AHTR here is a **single-level** adaptive refinement (coarse tile → optional one 2×2 split), not a full recursive quadtree.

---

## 10. Things Fixed During This Project (for context — these were real bugs, not "how it was always built")

- **Window bigger than the screen** → now measures the real monitor and scales to fit (§1).
- **Maximize button didn't work** → `GLViewImpl::createWithRect`'s `resizable` parameter was left at its default `false`; now passed as `true` (§1).
- **"Total lives" field** → simplified to a single numeric text box (an earlier +/- button pair was removed per request).
- **Dead map-hover zones** → `BackgroundChoise::onMouseMove` used to define 6 hover-highlight regions for a list that only ever had 2–4 real buttons; the unused ones (including the one for the now-removed Seele map) were deleted.
- **The Seele map and the original "Our Staff" credits screen** were removed from what the player can reach — both had the original development team's names/jokes baked directly into PNG art, which can't be edited at the code level. The credits screen was rebuilt from plain code-drawn text; the project's `logo.png` was regenerated the same way.
- **All Chinese-language code comments** across the 17 files that had them were stripped (ASCII/English-only source now).
- **`Credits::init()` was missing `return true;`** at the end of a `bool`-returning function (undefined behavior, silently "worked" by luck) — fixed while that file was already being edited.
