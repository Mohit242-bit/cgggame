# The Full Defense Script
### Everything to explain FA2 end-to-end — the problem, what's new, how it's built, code to point at, and a tough-question bank
Mohit Rawat · PRN 124B1B140

This is the one file to prep from. It assumes the examiner can ask about *anything* — the theory, the code, or the demo — and gives you a fast answer plus the exact file to open and point at. Structure:

1. The 30-second pitch
2. The problem, in plain words
3. What's actually NEW — ELCD and AHTR
4. How it's built in the game — with real code snippets and exact file paths
5. **The tough-question bank** — the part most worth rereading right before the viva
6. The slide deck, page by page (re-verified against your actual `.pptx`)

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

## 6. The Slide Deck, Page By Page

*(Re-verified directly against your actual `FA2 — ELCD & AHTR in Gun Mayhem.pptx`, 12 slides, unchanged since last check.)*

**Slide 1 — Title.** State the full title, course code, your name and PRN. One line: "FA1 was the survey and theory; FA2 is that theory built into a real, working game, measurable live."

**Slide 2 — Context · From FA1.** Recap both fixes in one breath each: ELCD = confidence decay kills ghosting; AHTR = spend rays where variance is high, not evenly. Both quoted taglines are on the slide — say them.

**Slide 3 — FA2 · Scope.** FA1 planned a throwaway SDL2 demo; FA2 instead integrated into a real, pre-existing game (Gun Mayhem, Cocos2d-x 4.0). Say it was scoped deliberately small and correct, not sprawling.

**Slide 4 — FA2 · Test Bed.** Why this game fits: one camera, constant combat, natural ephemeral lights every shot, cheap axis-aligned occluders, and — critically — repeatable on demand for a live demo.

**Slide 5 — Architecture.** The five files of `Classes/Lighting/`, one sentence each (see Part 4 above for the actual code). Say the design goal out loud: "a research feature addable or removable without risk to the shipping game."

**Slide 6 — Integration.** Three touch points, all small: `Bullet.cpp` (2 lines), `MapBase.h` (forward decl + pointer), `MapBase.cpp` (construct, update, destroy). This is your evidence for code quality.

**Slide 7 — Algorithm → Code.** The six-step `ComputeFrame()` walkthrough (Part 4 above has the real snippets). Say each step's name and what it does in one sentence; don't read code aloud here — that's what Part 4 of this document is for if asked directly.

**Slide 8 — Before/After.** Point at the dashed line (light expiry). Before: glow persists past it — the ghost. After: glow stops exactly at it — Theorem 1, drawn.

**Slide 9 — Validation.** F1/F2/F3 described on the slide — **this is your cue to switch to the live demo.** Don't linger here; say the line, then alt-tab into the running game.

**Slide 10 — Limitations.** Exactly two bullets, verbatim: 2D/CPU/tile-granularity analogue (not a GPU-numbers claim), and single-level AHTR refinement (not a full quadtree). Say both plainly — this is rigor, not a weakness.

**Slide 11 — Conclusion.** The three numbers: 0 ghost frames, 6.25× fewer shadow rays, 1 shadow ray per leaf tile. Say "proven," not "measured."

**Slide 12 — Thank you.** Name, PRN, course code. Open the floor.

**Timing reminder:** slides 1–4 fast (~3 min), 5–7 conceptual not code-reading (~2–3 min), 8–9 then straight into the live demo (most of your remaining time), 10–12 land and stop (~2 min). The demo is your strongest evidence — don't shortchange it to get through more slides.
