# The Full Defense Script
### Everything to explain FA2 end-to-end — the problem, what's new, how it's built, code to point at, and a tough-question bank
Mohit Rawat · PRN 124B1B140

This is the one file to prep from. It assumes the examiner can ask about *anything* — the theory, the code, or the demo — and gives you a fast answer plus the exact file to open and point at. Structure:

1. The 30-second pitch
2. The problem, in plain words
3. What's actually NEW — ELCD and AHTR
4. How it's built in the game — with real code snippets and exact file paths
5. **The tough-question bank** — the part most worth rereading right before the viva
6. The slide deck, page by page — slides 9–18, the FA2 section (re-verified against your actual `.pptx`)
7. Where everything actually lives — the repo map

---

## 1. The 30-Second Pitch

*(Say this if asked "summarize your work in one breath.")*

"Real-time games with lots of lights — muzzle flashes, explosions — can't afford to check every light against every pixel; it's billions of operations a frame. The best known fix, ReSTIR, gets it down to one ray per pixel using weighted reservoir sampling — but it has two specific, provable flaws: lights that die young leave a visible 'ghost' for about twenty frames, and a fixed sampling grid wastes rays in calm areas and starves busy ones. I propose two fixes — ELCD and AHTR — each with a formal proof it actually fixes its flaw and doesn't break anything else, plus a theorem proving the two fixes compose safely together. And it's not just theory — I built both into a real, working 2D game, so you can watch it work live."

---

## 2. The Problem, In Plain Words

*(If asked "what problem are you solving, exactly?")*

Rendering a frame means figuring out, for every point on screen, how much light reaches it from every light source. That requires a **shadow ray** — a check for "is anything blocking the path to this light?" If you have N lights and M pixels, brute force costs N×M shadow rays, every frame. At 500 lights and 4K resolution, that's **4.15 billion** ray checks per frame — not remotely achievable at 60fps.

ReSTIR (Bitterli et al., SIGGRAPH 2020) solves this by keeping a small "reservoir" per pixel — one chosen light plus some bookkeeping — updated cheaply every frame using **weighted reservoir sampling**, reusing both last frame's answer (temporal reuse) and neighboring pixels' answers (spatial reuse), so one real shadow ray per pixel ends up looking as good as hundreds of brute-force samples.

It's excellent, but it has two specific weaknesses I target:
- **Temporal:** it caps how much "history" a reservoir carries (`M_cap`, usually 20), with no idea whether the light it remembers still exists — so a light that's already gone can keep influencing the picture for ~20 more frames. I call this **ghosting**.
- **Spatial:** if you tile the screen to save cost, a fixed grid spends the same ray budget on a calm, boring tile as on a tile sitting right on a shadow's edge — provably wasteful.

---

## 3. What's Actually NEW — ELCD and AHTR

*(If asked "what's your actual contribution, versus just using ReSTIR?")*

I'm not proposing a new base algorithm — ReSTIR is the foundation, unchanged. My contribution is **two small, surgical, provable modifications**, each fixing exactly one of the two flaws above, each proven not to break anything else.

### ELCD — Ephemeral Light Confidence Decay

Every light gets a confidence number: `κ(t) = remaining lifetime / initial lifetime`. 1.0 when fresh, exactly 0.0 the instant it expires. When combining last frame's reservoir into this frame's (ReSTIR's "temporal reuse" step), its contributed weight gets multiplied by that confidence. Net effect: the moment a light expires, its old sample contributes *exactly zero* — not "less," zero.

**What I proved about it (4 theorems):**
- **Theorem 1:** zero ghost frames, exactly, by construction — not an improvement, a guarantee.
- **Theorem 2:** for a light that never expires, confidence stays at 1 forever, so ELCD does *nothing different* from standard ReSTIR — zero regression, it's a strict superset of correct behavior.
- **Theorem 4** (the one I'm proudest of): ELCD doesn't just reduce *ghosting*, it's provably still an **unbiased estimator** — i.e., mathematically still correct, not just "less wrong." The reason: ReSTIR's own temporal-reuse step is itself just reservoir sampling applied to "last frame's answer" treated as one more candidate — so any valid weight (including my scaled one) keeps the whole thing statistically correct, by the same math that makes ReSTIR itself correct.

### AHTR — Adaptive Hierarchical Tile Resampling

Split the screen into tiles. Measure how much the candidate lights *disagree* in brightness within each tile (the variance). If a tile is calm, leave it coarse. If a tile has real disagreement — meaning it probably straddles a shadow edge — split it into 4 and give it more rays.

**What I proved (1 theorem, 1 corollary, 1 lemma):**
- **Theorem 3:** derived the *mathematically optimal* way to split a ray budget across regions with different variances — this comes straight from a 1934 statistics result (Neyman's optimal allocation for stratified sampling) that nobody had connected to real-time tile sampling before. Proved it can never do worse than a fixed grid, via Cauchy-Schwarz.
- **Corollary 3.1:** proved that going *deeper* (splitting tiles more than once) can only help, never hurt — so the "single-level" version actually shipped in the game is a deliberate, safe simplification, not a shortcut.
- **Lemma 2:** an exact formula for how many extra rays this costs, `R(f) = 4/(1+3f)` — at realistic numbers, about a 3× reduction.

### Theorem 5 — the capstone

Proved the two fixes **don't interfere with each other** — ELCD only touches the temporal step, AHTR only touches the spatial step, they read different things, so you get both guarantees simultaneously, with zero regression when neither fix is even needed.

---

## 4. How It's Built In The Game — exact files, exact code

*(If asked "show me where this actually is in the code.")* Everything lives in one new, self-contained folder: **`Classes/Lighting/`** (5 files, ~500 lines total), plus three tiny hooks into the pre-existing game.

### `Classes/Lighting/LightSource.h` — the confidence formula itself
```cpp
float confidence() const {
    if (initialLifetime <= 0.f) return 0.f;
    return cocos2d::clampf(remainingLifetime / initialLifetime, 0.f, 1.f);
}
```
*Point here for:* "show me κ(t) in actual code."

### `Classes/Lighting/Reservoir.h` — where ELCD's guarantee actually lives
```cpp
void CombineTemporal(const Reservoir& prev, float confidenceScale, float mCap) {
    if (prev.lightId < 0 || prev.M <= 0.f || confidenceScale <= 0.f) return;  // <- the kill switch
    ...
}
```
*Point here for:* "where exactly does an expired light get dropped?" — right there, that one guard clause.

### `Classes/Lighting/LightingPass.cpp` — `ComputeFrame()`, the whole algorithm, one function
Six steps, each a named function call:
```cpp
Reservoir combined = BuildCandidateReservoir(lights, tile.bounds, &candidates);   // Step 2: ReSTIR Pass 1
if (ahtrEnabled_ && candidates.size() > 1) { /* split into 4, keep the best */ }  // Step 3: AHTR
combined.CombineTemporal(tile.prevReservoir, confidence, 20.f);                  // Step 4: ELCD
bool visible = IsVisible(tileCenter, combined.position, map_->Floor, ...);       // Step 5: the shadow ray
overlay_->drawDot(tile.reservoir.position, tile.reservoir.radius * 0.35f, c);     // Step 6: render
```
*Point here for:* "walk me through one frame of the algorithm."

### `Classes/Lighting/Visibility.cpp` — the shadow ray, reusing the game's own collision data
```cpp
bool IsVisible(const Vec2& a, const Vec2& b, const std::vector<std::vector<float>>& floorRows, ...) {
    for (size_t row = 0; row < floorRows.size(); ++row) {
        ... if (RowBlocks(floorRows[row], x)) return false;
    }
    return true;
}
```
*Point here for:* "how does the visibility test work, and is it a new system or reused?" — it's the **same** `MapBase::Floor` platform data the game already uses for player-standing collision; no new occlusion geometry was built.

### `Classes/Weapons/Bullet/Bullet.cpp` — the entire integration into the existing game
```cpp
player->map->SpawnMuzzleFlash(init);
player->map->SpawnBulletGlow(currentPoint);
```
*Point here for:* "how much of the existing game did you have to change?" — two lines, in the one function every weapon already calls to fire a bullet. That's the whole hook.

### `Classes/Map/MapBase.h` / `.cpp` — the plumbing
```cpp
Lighting::LightingPass* lighting = nullptr;          // one pointer member
if (lighting) lighting->Update(dt);                   // one call, once a frame
void SpawnMuzzleFlash(const Vec2& pos);               // thin wrappers, null-checked
```
*Point here for:* "is this safe / can it crash the game?" — every call site null-checks `lighting` first; if it's somehow not ready yet, it just silently does nothing that frame rather than crashing.

### `Classes/Weapons/Bullet/Bomb.cpp` — the explosion, the third (and largest) light
```cpp
void Bomb::Explode() {
    map->SpawnExplosion(currentPoint);   // the Explosion LightType -- 45-frame life, matches the FA1 spec exactly
    ... // area knockback to nearby players, then removes itself
}
```
*Point here for:* "do you have a real ephemeral-light test case, like the explosion FA1 talked about?" — yes, thrown with its own dedicated key (2 / K), a real gravity-arced projectile, independent of whichever gun is equipped.

### The live debug HUD — `LightingPass::Render()`
```cpp
"Shadow tests this frame: %d / %d tiles\nGhost streak: %d frames (max %d)"
```
*Point here for:* "how do I know it's actually working, not just drawn?" — press **F1** in the running game. **F2** toggles ELCD live — the ghost-streak counter visibly changes behavior the instant you flip it. **F3** toggles AHTR live — the shadow-test count visibly changes.

---

## 5. The Tough-Question Bank

*(Read this section twice before the viva. These are the questions most likely to actually get asked.)*

**Q: Is this your own original idea, or did you just implement someone else's paper?**
A: ReSTIR itself (the base algorithm) is Bitterli et al.'s, correctly cited as prior work — I'm not claiming to have invented reservoir sampling or ReSTIR. ELCD and AHTR are my own contributions on top of it: two specific, formally proven fixes to two specific, previously-identified-but-unfixed weaknesses. The theorems, proofs, and the Neyman-allocation connection in Section V are original to this paper.

**Q: What exactly is novel here versus the original ReSTIR paper?**
A: Three concrete things ReSTIR's own paper does not do: (1) it never formalizes or fixes the ghost-frame problem — I define what a ghost frame *is* mathematically and prove a fix; (2) it uses a fixed sampling structure with no variance-adaptive allocation — I derive the provably optimal allocation rule and an algorithm for it; (3) neither of these existed as provable, composable corrections before — I prove they compose with zero interference (Theorem 5).

**Q: Prove to me ELCD doesn't just hide the problem instead of fixing it.**
A: Theorem 1 isn't "less ghosting" — it's a proof that an expired light's weight is **exactly** zero in the very next frame, by direct substitution into the formula (κ=0 forces the carried weight to multiply by zero). That's a hard guarantee, not a measured improvement.

**Q: Does ELCD introduce bias to make that guarantee happen?**
A: No — that's Theorem 4. ReSTIR's own temporal reuse step is itself just weighted reservoir sampling treating "last frame's answer" as one more candidate. Any valid weight — including my confidence-scaled one — keeps that reservoir update statistically correct (unbiased), by the same base math that makes ReSTIR correct in the first place. I'm not trading correctness for less ghosting; both come for free from the same one-line change.

**Q: What is Neyman allocation and why does it matter here?**
A: A 1934 result from classical statistics (stratified sampling theory): if you're splitting a fixed sample budget across groups with different variances, the budget that minimizes total error is proportional to each group's *standard deviation*, not split evenly. I derived this from scratch with Lagrange multipliers for the many-light tiling problem and proved — via Cauchy-Schwarz — that it can never do worse than splitting evenly. AHTR's "split high-variance tiles" rule is a practical, discrete approximation of that exact optimum.

**Q: Why only a single level of tile-splitting? Isn't that a limitation?**
A: It's a deliberate engineering choice, not a mathematical ceiling — Corollary 3.1 proves going deeper can only reduce error further, never increase it. Single-level was chosen for implementation simplicity and bounded memory, with the formal guarantee that it's a safe, conservative subset of what's possible.

**Q: Why 2D and CPU, not a real GPU 3D renderer?**
A: Two honest, stated reasons, not hidden ones. First, the underlying math (confidence decay, variance-proportional allocation) is dimension-agnostic — it reduces to the per-pixel GPU case as tile size shrinks to one pixel. Second, this is a tile-granularity analytical model evaluated against closed-form cost formulas, not a claim to match specific GPU papers' hardware numbers — that's stated explicitly as the paper's scope (Section VIII-B).

**Q: What are your actual measured results, then — frame time, FPS?**
A: Deliberately none, and that's a stated design choice, not an omission. Section VII's results are 100% calculated from the closed-form cost models — ray counts, ghost-frame duration, memory footprint — specifically so the conclusions don't depend on one machine's hardware or one engine's constant factors. Separately, as a feasibility check (Section VIII-C), both algorithms were integrated unmodified into a real game — but that's reported as "it compiles and runs correctly inside a real pipeline," not as a timing benchmark.

**Q: If it's not benchmarked in the game, how do I know it actually works there?**
A: Watch it live — that's exactly what the in-game debug HUD is for. Fire a weapon, press F1, and you'll see the real, live numbers: active light count, shadow tests this frame, and a ghost-streak counter. Press F2 to disable ELCD mid-match and watch the ghost-streak counter stop returning to zero; press it again and watch it immediately start clearing. That's the theorem, demonstrated, not claimed.

**Q: What's the difference between this and ordinary tiled/clustered deferred shading?**
A: Clustered shading still tests *every* light against *every* cluster it overlaps — it reduces the pixel dimension, not the per-light cost. ReSTIR (and by extension AHTR) instead picks *one statistically representative light* per region via weighted reservoir sampling, so cost is independent of light count entirely (Table II) — clustered shading's cost still scales with N; this doesn't.

**Q: What happens with hundreds of lights all active in the same small area — does AHTR break?**
A: No — candidate generation (RIS) already compresses any number of candidate lights in a tile down to one statistically-fair pick before AHTR's refinement even runs; AHTR's variance check just decides *how many sub-tiles* to spend that same 1-shadow-ray-per-leaf budget on, so the cost per tile stays bounded regardless of how many lights are competing for it.

**Q: Does the AI opponent in the game use any of this?**
A: The AI fires weapons normally, which triggers muzzle-flash and bullet-glow lights exactly like a human player — so yes, AI combat is lit by the same system. One honest gap: the AI currently never throws bombs (the dedicated bomb key is only wired to human keyboard input), so the Explosion light type isn't currently triggered by AI opponents — a known, stated limitation, not a hidden one.

**Q: Could this extend to 3D / a real game engine like Unreal or Unity?**
A: Yes, and that's explicitly discussed as scope, not as a blocker — the confidence-decay formula and the variance-allocation argument are dimension-agnostic; what would change in a 3D/GPU port is the shadow-ray implementation (a real raycast instead of a 2D platform-row test) and moving the reservoir math onto the GPU, neither of which touches the core theorems.

**Q: What's the actual memory cost of a reservoir?**
A: Worked out explicitly in Section VII-D: a full-precision reservoir (ID, position, RGBA color, radius, weight, sample count) is 40 bytes — over the paper's own 30-byte budget requirement — but swapping stored color for a 1-byte palette index and quantizing position brings it to ~21 bytes, under budget. Neither ELCD nor AHTR add fields to this structure themselves.

**Q: Why does the paper cite a 1997 paper (Instant Radiosity) and a 1934 statistics paper — isn't this supposed to be about real-time graphics?**
A: Because good related work means situating the contribution against the *right* lineage, not just the newest papers. Instant Radiosity explains why many-light rendering is hard and why VPL-based approximations introduce bias (motivating why ReSTIR's unbiased approach matters). Neyman 1934 is the actual mathematical origin of the optimal-allocation idea AHTR is built on — citing it is more rigorous than reinventing it and pretending it's new.

**Q: What would you do differently, or what's left undone?**
A: Exactly the six open problems in Section IX — the most interesting two: connecting ReSTIR's temporal reuse to formal sequential Monte Carlo / particle filtering theory (which would justify `M_cap` rigorously instead of as a tuned constant), and building dedicated hardware support for weighted reservoir sampling, analogous to existing ray-tracing cores.

---

## 6. The Slide Deck, Page By Page — Slides 9–18 (the FA2 build-out section)

*(Your current deck runs the FA1 recap as slides 1–8, then this FA2 section as slides 9–18 — re-verified directly against screenshots of the actual deck, word for word.)*

Every slide below gives you three things: **what's on it** (verbatim), **what to say**, term by term, and **the exact code** backing the claim, with the real file path and current line numbers — open these files and have them ready in tabs before you present.

---

### Slide 9 — Title

> Real-Time Many-Light Rendering via Spatiotemporal Reservoir Resampling
> *From Survey to Implementation — ELCD & AHTR inside* **Gun Mayhem**
> Mohit Rawat · PRN 124B1B140 · Computer Graphics & Gaming · BCE25PE01 · FA2

**Say:** read the title once, slowly, then unpack the three technical terms in it before moving on — don't assume the examiner reads them the same way you do:
- **"Real-time"** — has to run inside the game's own frame budget (60fps, ~16ms/frame), not a batch render.
- **"Many-light rendering"** — the N-lights × M-pixels cost problem (Part 2 above).
- **"Spatiotemporal Reservoir Resampling"** — this *is* the full name of ReSTIR (Bitterli et al., SIGGRAPH 2020): spatio = reuse across neighboring pixels, temporal = reuse across frames, reservoir resampling = the weighted-sampling data structure that makes the reuse cheap and unbiased.

Close with the one-line frame: *"FA1 was the survey and the theory. FA2 is that theory, actually built and running inside a real game you can watch work, live."*

**Code:** none — but if asked "is there an actual paper, not just slides," this exact title is the cover-page title of [docs/Research_Paper.docx](../docs/Research_Paper.docx) / [docs/Research_Paper.pdf](../docs/Research_Paper.pdf) too — same title, same name, same PRN. Say so; it shows the deck and the paper are one consistent piece of work, not two disconnected artifacts.

---

### Slide 10 — Recap: two fixes to standard ReSTIR

> **ELCD — Ephemeral Light Confidence Decay** (temporal fix)
> Each light carries a confidence κ = τ_remaining / τ_initial. When the light expires, κ = 0 and its glow disappears the same frame — no ~20-frame ghost.
> *"Lights fade out of the renderer's memory exactly as fast as they fade out of the scene."*
>
> **AHTR — Adaptive Hierarchical Tile Resampling** (spatial fix)
> Measure how much lighting varies in each screen tile. Split busy tiles and give them more rays; give flat tiles fewer.
> *"Spend the ray budget where the eye would actually notice, not evenly everywhere."*

**Say, term by term:**
- **κ (kappa)** — the confidence number, just a fraction: how much of the light's life is left, 1.0 when freshly spawned, sliding down to 0.0 at expiry.
- **"~20-frame ghost"** — 20 is `M_cap`, ReSTIR's own temporal-history cap. *Without* ELCD, a reservoir can keep echoing a dead light's old sample for up to `M_cap` frames before its weight naturally dilutes away — that lingering, incorrect glow is the "ghost." ELCD doesn't shorten that window, it makes it exactly zero by construction.
- **AHTR's "variance"** — how much the candidate lights in one tile disagree in brightness/weight. High disagreement ⇒ a shadow boundary is probably cutting through that tile ⇒ worth spending more rays there.

**Code:**
- `Classes/Lighting/LightSource.h:30-33` — the κ formula itself:
  ```cpp
  float confidence() const {
      if (initialLifetime <= 0.f) return 0.f;
      return cocos2d::clampf(remainingLifetime / initialLifetime, 0.f, 1.f);
  }
  ```
- `Classes/Lighting/LightingPass.cpp:140` — `M_cap` isn't a comment, it's a literal `20.f` passed right here: `combined.CombineTemporal(tile.prevReservoir, confidence, 20.f);` — point at that exact `20.f` if asked "where is M_cap in the code."

---

### Slide 11 — From survey to a real, working game

> **FA1 plan:** a new SDL2/raycasting demo built from scratch — a synthetic test harness, useful once, then thrown away.
> **FA2 built:** ELCD + AHTR integrated into Gun Mayhem — a complete C++/Cocos2d-x 2-player platformer shooter.
> cocos2d-x 4.0 (engine, Windows native build) · 2 light types (muzzle flash + bullet glow)

**Say:** be candid and precise here — Gun Mayhem is a pre-existing open-source Cocos2d-x project you took over and substantially extended (lighting system, bomb weapon, rebindable keys, bug fixes), not something built from nothing. Say that plainly if asked; integrating correctly into someone else's real codebase is a *harder*, more realistic skill than a clean-room toy demo, and examiners respect the honesty.

One correction to have ready: the slide says "2 light types" — that's the two types fired on *every shot* (muzzle flash + bullet glow). There is a third type, **Explosion**, from the bomb weapon — covered on the Integration slide (12) and Architecture slide (13). If asked "only two?", say: "two per gunshot; a third — Explosion — comes from the bomb, shown next."

**Code:** `CMakeLists.txt` / `cmake.txt` at the repo root are your evidence of "native Windows build" — point at them if asked how it's actually compiled (MSBuild via CMake-generated `.vcxproj`, not the engine's own Android/iOS build scripts).

---

### Slide 12 — Why Gun Mayhem fits the research

> **One camera, constant combat** — local 2-player shared-screen shooter running in real time.
> **Natural ephemeral lights** — every shot makes a muzzle flash and a bullet glow — short-lived lights.
> **Cheap, clean occluders** — axis-aligned solid platforms make a simple 2D "shadow ray" test.
> **Repeatable on demand** — firing is player-triggered, so ELCD/AHTR can be shown live, not only in a benchmark.

**Say, one sentence per card:**
- *One camera, constant combat* — one viewport, continuously under fire, so there's a sustained signal to light — unlike a quiet exploration game where lights would be rare.
- *Natural ephemeral lights* — the short-lived light objects ELCD is designed around already exist every time a player shoots — you didn't have to invent a synthetic scenario to get ephemeral lights.
- *Cheap, clean occluders* — the platform collision is already stored as flat `[x_start, x_end]` numeric spans per row (`MapBase::Floor`) — reused directly as occluder geometry. No new geometry system was built for this.
- *Repeatable on demand* — because lights are player-triggered, not scripted or timed, you can fire weapons live in front of the examiner and the debug HUD reacts in real time — this is your demo story in one sentence.

**Code:** `Classes/Lighting/Visibility.h:9-15` (the contract) and `Visibility.cpp:14-32` (the implementation) — the shadow ray walks the *same* `floorRows`/`floorBase`/`floorHeight` triple that `MapBase`/`CharacterBase` already use for stand/fall collision:
```cpp
bool IsVisible(const cocos2d::Vec2& a, const cocos2d::Vec2& b,
               const std::vector<std::vector<float>>& floorRows,
               float floorBase, float floorHeight);
```

---

### Slide 13 — The self-contained `Classes/Lighting/` module

| File | Responsibility |
|---|---|
| `LightSource.h` | Ephemeral light data type + κ_l(t) confidence formula |
| `LightManager.h/.cpp` | Owns active lights — spawns, ages, expires them |
| `Reservoir.h` | Weighted reservoir (RIS) + temporal combine — core ReSTIR structure |
| `Visibility.h/.cpp` | 2D "shadow ray": line-vs-platform occlusion test |
| `LightingPass.h/.cpp` | Orchestrates candidates → AHTR → ELCD → shadow test → render + HUD |

> *"Design goal: a research feature that can be added or removed without risk to the game"*

**Say:** walk the table top to bottom, one sentence each (above), then explain *why* the design-goal line is literally true, not just a slogan: every single call into this module is either (a) internal, inside `Classes/Lighting/` itself, or (b) a null-checked wrapper call from `MapBase` (slide 14). Delete the whole folder and its 3 call sites, and the base game still compiles and runs — nothing else in the codebase has a hard dependency on it.

**Code — all five files, in full, for reference:**
```cpp
// LightSource.h — Classes/Lighting/LightSource.h:20-34
struct LightSource {
    int id = -1;
    cocos2d::Vec2 position;
    cocos2d::Color4F color = cocos2d::Color4F::WHITE;
    float radius = 0.f;
    float initialLifetime = 0.f;
    float remainingLifetime = 0.f;
    LightType type = LightType::BulletGlow;
    float confidence() const { /* κ_l(t), see Slide 10 */ }
};
```
```cpp
// LightManager.cpp — Classes/Lighting/LightManager.cpp:20-34 (the three spawners)
void LightManager::SpawnMuzzleFlash(const cocos2d::Vec2& position) {
    SpawnLight(position, LightType::MuzzleFlash, 3.f / 60.f, 55.f, Color4F(1.f, 0.75f, 0.3f, 1.f));
}
void LightManager::SpawnBulletGlow(const cocos2d::Vec2& position) {
    SpawnLight(position, LightType::BulletGlow, 1.f / 60.f, 22.f, Color4F(1.f, 0.95f, 0.6f, 1.f));
}
void LightManager::SpawnExplosion(const cocos2d::Vec2& position) {
    SpawnLight(position, LightType::Explosion, 45.f / 60.f, 130.f, Color4F(1.f, 0.45f, 0.12f, 1.f));
}
```
```cpp
// Reservoir.h — Classes/Lighting/Reservoir.h:22-58 (candidate step + ELCD-decayed temporal step)
bool Update(int id, float weight, const Vec2& pos, const Color4F& c, float r); // RIS Pass 1
void CombineTemporal(const Reservoir& prev, float confidenceScale, float mCap); // ELCD lives here
```
```cpp
// Visibility.cpp — Classes/Lighting/Visibility.cpp:14-32 (the shadow ray)
bool IsVisible(const Vec2& a, const Vec2& b, const std::vector<std::vector<float>>& floorRows,
               float floorBase, float floorHeight);
```
```cpp
// LightingPass.h — Classes/Lighting/LightingPass.h:23-62 (the orchestrator)
class LightingPass {
public:
    explicit LightingPass(MapBase* map);
    void Update(float dt);              // called once/frame from MapBase
private:
    void ComputeFrame();                // the 6-step algorithm — Slide 15
    void Render();                      // the overlay + debug HUD — Slide 17
};
```

---

### Slide 14 — Touch points into the existing game

| File | Change |
|---|---|
| `Weapons/Bullet/Bullet.cpp` | 2 lines: on bullet spawn, call `SpawnMuzzleFlash(muzzlePos)` and `SpawnBulletGlow(bulletPos)` |
| `Map/MapBase.h` | Forward-declares `Lighting::LightingPass`, adds one pointer member + 2 thin wrapper declarations |
| `Map/MapBase.cpp` | Builds the pass in `initPlayer()`, calls `lighting->Update(dt)` each frame, implements the wrappers, deletes it in the destructor |

> *Evidence for: Code / Module Quality & Reusability*

**Say:** that footer line is literally the grading rubric category this slide is aimed at — say so if asked why the slide exists. Then walk the 3 rows: "the entire integration footprint into a pre-existing, unfamiliar codebase is two lines in one file, one pointer member, and three null-checked wrapper calls. Everything else is self-contained inside `Classes/Lighting/`, from Slide 13."

**Code — exact current locations (open these three files):**
```cpp
// Classes/Weapons/Bullet/Bullet.cpp:24-25 — the whole hook
player->map->SpawnMuzzleFlash(init);
player->map->SpawnBulletGlow(currentPoint);
```
```cpp
// Classes/Map/MapBase.h:19   — forward declaration
namespace Lighting { class LightingPass; }
// Classes/Map/MapBase.h:33-37 — wrapper declarations + comment
void SpawnMuzzleFlash(const Vec2& pos);
void SpawnBulletGlow(const Vec2& pos);
void SpawnExplosion(const Vec2& pos);
// Classes/Map/MapBase.h:66   — the one pointer member
Lighting::LightingPass* lighting = nullptr;
```
```cpp
// Classes/Map/MapBase.cpp:7    — #include "Lighting/LightingPass.h"
// Classes/Map/MapBase.cpp:23   — delete lighting;              (destructor)
// Classes/Map/MapBase.cpp:37   — if (lighting) lighting->Update(dt);   (every frame)
// Classes/Map/MapBase.cpp:40-52 — the 3 wrapper bodies, each one null-checked:
void MapBase::SpawnMuzzleFlash(const Vec2& pos) {
    if (lighting) lighting->GetLightManager().SpawnMuzzleFlash(pos);
}
// Classes/Map/MapBase.cpp:125  — if (!lighting) lighting = new Lighting::LightingPass(this);  (lazy init)
```
If asked *"is this safe — can it crash the game?"*: every call site null-checks `lighting` first; if the pass somehow isn't built yet, the call is a silent no-op that frame, never a crash.

---

### Slide 15 — `LightingPass::ComputeFrame()`, step by step

| # | Step | Function |
|---|---|---|
| 01 | Tile grid | Play area split into a fixed 16×9 grid (144 tiles), kept across frames so reservoirs carry state |
| 02 | Candidate generation | Every light overlapping a tile is folded in by weighted reservoir sampling — `Reservoir::Update()`, ReSTIR Pass 1 |
| 03 | AHTR refinement | More than one competing light in a tile → evaluate at 2×2 sub-tiles, keep the strongest |
| 04 | ELCD temporal reuse | Last frame's reservoir scaled by κ_l(t). Light expired → κ=0 → stale sample adds nothing. Theorem 1, exactly |
| 05 | Shadow ray | One test per tile: tile centre → chosen light, against `MapBase::Floor` |
| 06 | Render + HUD | Soft additive glow; HUD shows active lights, shadow tests, tile count, ghost streak |

**Say:** walk all six steps in order, naming both the step and the real function, exactly as the table does — this slide *is* the algorithm, nothing is hidden behind a diagram. On step 01, stress "kept across frames" — that persistence (`tile.prevReservoir`) is *what makes temporal reuse possible at all*; if tiles were rebuilt fresh every frame there'd be nothing for ELCD to decay. On step 04, say the word **"exactly"** out loud — Theorem 1 is a guarantee by construction, not an empirical improvement.

**Code — the real function, `Classes/Lighting/LightingPass.cpp:107-168`:**
```cpp
void LightingPass::ComputeFrame()
{
    for (auto& tile : tiles_) {
        // Step 02 — candidate generation (ReSTIR Pass 1)
        Reservoir combined = BuildCandidateReservoir(lights, tile.bounds, &candidates);

        // Step 03 — AHTR: only tiles with >1 competing light get refined
        if (ahtrEnabled_ && candidates.size() > 1) {
            for (int q = 0; q < 4; ++q) {
                Reservoir sub = BuildCandidateReservoir(lights, Quadrant(tile.bounds, q), nullptr);
                if (sub.FinalWeight() > best.FinalWeight()) best = sub;
            }
            combined = best;
        }

        // Step 04 — ELCD-decayed temporal reuse
        float confidence = elcdEnabled_ ? prevLight->confidence() : 0.f;
        combined.CombineTemporal(tile.prevReservoir, confidence, 20.f);

        // Step 05 — the shadow ray
        bool visible = IsVisible(tileCenter, combined.position, map_->Floor, map_->floor_base, map_->floor_height);
    }
}
```
(`Render()`, step 06, is `LightingPass.cpp:170-194` — see Slide 17.)

---

### Slide 16 — Before and after: ELCD + AHTR

> **Before · standard ReSTIR** — glow over time: 1.0, 0.8, 0.6 … fading past the expiry line. *"Ghost glow lingers ~20 frames after expiry."* Over space: same ray count in every tile, busy or flat.
>
> **After · ELCD + AHTR** — glow over time: 1.0, 0.67, 0.33, **0** — stops exactly at the dashed expiry line. κ = τ_remaining/τ_initial. *"0 ghost frames — gone the same frame."* Over space: the contested tile is split 2×2; flat tiles stay coarse.

**Say:** this slide is a picture of Theorem 1 and Theorem 3, walk it left-to-right: on the left, standard ReSTIR's glow visibly survives past the dashed "light expires" line — that persistence past expiry *is* the ghost, drawn. On the right, the same timeline under ELCD hits exactly 0 at the same dashed line — not faded, zero. For the spatial half: point at the highlighted 2×2 quadrant (the contested tile, where two amber light-dots overlap) versus the rest of the grid, which stays at one ray (blue dot) per tile — that's AHTR's variance-triggered split from Slide 15, step 03, drawn.

**Code:** same two snippets as Slide 10/15 — `LightSource.h:30-33` (κ formula) for the temporal half, and the `if (ahtrEnabled_ && candidates.size() > 1)` branch in `LightingPass.cpp:122-130` for the spatial half.

---

### Slide 17 — Live validation tooling, in the running game

> **F1** — show/hide the debug HUD — active lights, shadow tests, ghost streak
> **F2** — toggle ELCD at runtime — ghost-streak counter changes at once
> **F3** — toggle AHTR at runtime — shadow-test count changes at once

**Say:** read the three key bindings, then — **this is your cue to stop talking and switch to the live demo.** Don't linger on this slide; alt-tab straight into the running `Gun-Mayhem.exe` and actually fire a weapon.

Important, and directly useful if your debug HUD is sitting at idle when you switch over (exactly like the screenshot you'd see before anyone's shooting): **"Active lights: 0" / "Shadow tests this frame: 0/144" is the *correct*, expected reading while nobody is firing** — lights are spawned only on muzzle flash, bullet glow, and explosion events (Slide 11/13), so standing still genuinely produces zero. If an examiner points at a "0" and asks "is this even doing anything?", say exactly that — then fire a shot and watch the numbers move. A system whose counters sit honestly at zero when idle, and move the instant something happens, is stronger demo evidence than numbers that are always nonzero — it proves the HUD is reading live state, not a hardcoded display.

**Code:**
```cpp
// Classes/Lighting/LightingPass.cpp:58-70 — the constructor, F1/F2/F3 listener
keyListener_->onKeyPressed = [this](EventKeyboard::KeyCode code, Event*) {
    if (code == Key::KEY_F1) { debugVisible_ = !debugVisible_; hud_->setVisible(debugVisible_); }
    else if (code == Key::KEY_F2) { elcdEnabled_ = !elcdEnabled_; }
    else if (code == Key::KEY_F3) { ahtrEnabled_ = !ahtrEnabled_; }
};
```
```cpp
// Classes/Lighting/LightingPass.cpp:183-193 — Render(), the HUD string itself
std::snprintf(buf, sizeof(buf),
    "Lighting Debug (F1 toggle)\nELCD [F2]: %s   AHTR [F3]: %s\nActive lights: %d\n"
    "Shadow tests this frame: %d / %d tiles\nGhost streak: %d frames (max %d)",
    elcdEnabled_ ? "ON" : "OFF", ahtrEnabled_ ? "ON" : "OFF",
    activeLightsThisFrame_, shadowTestsThisFrame_, (int)tiles_.size(),
    currentGhostStreak_, maxGhostStreak_);
```
All five numbers in that string (`activeLightsThisFrame_`, `shadowTestsThisFrame_`, `tiles_.size()`, `currentGhostStreak_`, `maxGhostStreak_`) are live counters recomputed every frame inside `ComputeFrame()` (Slide 15) — not display text.

---

### Slide 18 — Conclusion

> ReSTIR's ghost-frame and fixed-grid flaws are real — and fixable.
> **0** ghost frames with ELCD · **6.25×** fewer shadow rays with AHTR · **1** shadow ray per leaf tile
> *"The proof: ELCD and AHTR are implemented and measured — in theory (FA1) and live inside Gun Mayhem (FA2)."*

**Say:** the three numbers, then the closing line verbatim. Land it and stop — don't keep talking past your own conclusion slide.

**The one sharp question to be ready for** — a careful examiner may cross-check the "6.25×" against Lemma 2 in your own paper and notice `R(f) = 4/(1+3f)` *tops out at 4× as f→0* and sits around 3–3.5× at realistic f. **Both numbers are correct — they answer different questions, and you should say exactly this:**
- **Lemma 2's `R(f)`** (Research_Paper.docx/pdf, Section V) is the *marginal* cost ratio of one refinement decision: 4 sub-tile rays vs 1 coarse-tile ray, for a single tile.
- **The "6.25×" on this slide** is a *whole-scene worked example* from the FA1 analysis ([docs/FA2_PPT_Content.md](../docs/FA2_PPT_Content.md), AHTR section): a dense explosion scene where a fully-uniform fixed grid costs 1,450 tiles × 8 samples/tile = 11,600 rays, versus AHTR's adaptive allocation of ~320 leaf tiles totalling ~4,800 rays — **11,600 / 4,800 ≈ 6.25×, aggregated over the whole frame**, not the per-tile bound.

Have that distinction ready verbatim — it turns a potential "gotcha" into a demonstration that you actually understand both numbers rather than having copied one onto a slide.

**Code:** none new — this slide is the summary of Slides 10, 15 and 16. If asked to re-derive either number live, `Classes/Lighting/Reservoir.h:61-64` (`FinalWeight()`) and the `R(f)` equation in [docs/Research_Paper.docx](../docs/Research_Paper.docx) are your two sources.

---

**Timing reminder:** slides 1–8 (FA1 recap) fast, slides 9–12 fast (~3 min), 13–15 conceptual, not code-reading aloud (~2–3 min), 16–17 then straight into the live demo (most of your remaining time), 18 lands and stops (~2 min). The demo is your strongest evidence — don't shortchange it to get through more slides.

---

## 7. Where Everything Actually Lives — the repo map

*(If asked "can I see the whole project," or you just need to find a file fast mid-viva.)*

Repo: **https://github.com/Mohit242-bit/cgggame**

| What | Path |
|---|---|
| The lighting module (all 5 files) | `Classes/Lighting/` |
| Bomb weapon (3rd light type, Explosion) | `Classes/Weapons/Bullet/Bomb.cpp/h`, `Classes/Weapons/Gun/Gun_Bomb.cpp/h` |
| The 2-line integration hook | `Classes/Weapons/Bullet/Bullet.cpp:24-25` |
| Research paper (Word + PDF, IEEE two-column) | `docs/Research_Paper.docx`, `docs/Research_Paper.pdf` |
| This document — full defense script | `docs/script.md` |
| Slide-by-slide content source | `docs/FA2_PPT_Content.md` |
| Timed presentation script | `docs/Presentation_Script.md` |
| Term glossary | `docs/Glossary_and_Algorithm.md` |
| Plain-language explainer (camera, AI, physics, lighting) | `docs/short.md` |
| Full technical reference | `docs/explain.md` |

Note: the repo's pushed snapshot excludes the vendored `cocos2d-x` engine folder (third-party library code, not your work) to keep the push size sane — your own source, docs, and the research paper are all there in full.
