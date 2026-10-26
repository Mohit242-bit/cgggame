# FA2 PPT Content Package
### Real-Time Many-Light Rendering via Spatiotemporal Reservoir Resampling — FA1 Survey → FA2 Implementation in *Gun Mayhem*
Computer Graphics & Gaming — BCE25PE01

---

## How to use this document

This is raw content, organized so you (or an AI slide generator, e.g. paste this whole file into Claude and ask it to build a deck) can turn it directly into a PPT. It is split into:

1. **Source material** — the FA1 research content, extracted faithfully from your original survey PDF (`124B1B140_CGG.pdf`), so nothing from FA1 gets lost.
2. **FA2 implementation facts** — what was *actually built*, in the *actual game* (Gun Mayhem, not the originally-planned SDL2 prototype), with file names, line counts, and code behavior, so every claim on a slide is something you can point at in the codebase during viva.
3. **A slide-by-slide outline** you can hand to a PPT tool as-is.
4. **A rubric-mapping section** so every row of your grading rubric has a matching slide/talking point.
5. **A demo script** for the live demonstration.

### Instructions for whatever tool builds the deck from this file

- Tone: professional, technical, conference-presentation style — not cutesy, not over-designed.
- **Images: exactly ONE slide in the whole deck gets an image placeholder** — the slide marked `[IMAGE PLACEHOLDER]` below (slide 13, "Live Validation Tooling"). The author will drop in their own screenshot of the running implementation there. **Do not generate, fetch, or place any other image, icon-as-hero-art, or stock photo anywhere else in the deck.** Every other slide is text/diagram/table only.
- Where a slide needs a visual and no image is supplied, build it as a native slide element instead (a table, a formula block, a simple shape diagram) rather than an image.
- Keep slide 8 (the dedicated "What Are ELCD and AHTR" overview slide) conceptual and plain-language — save the formulas/theorems for the two slides that follow it.
- The "Limitations" slide must contain **only the two bullet points given in Part 2.6, verbatim, and nothing else** — no extra caveats, no hedging language about the game or the demo. Those two points are about the algorithms' scope (2D/CPU/tile-granularity; single-level AHTR refinement), not about the game.

Where FA2 **narrowed or changed scope** from the FA1 plan, that is called out explicitly and honestly — your professor will ask "why," and you want a confident one-line answer ready, not to be caught improvising.

---

## PART 1 — Source Material (from the FA1 Survey)

### 1.1 Title & Framing

- **Title:** Real-Time Many-Light Rendering via Spatiotemporal Reservoir Resampling
- **Subtitle:** A Comprehensive Survey, Critical Analysis & Novel Contribution
- **Tags:** SIGGRAPH 2020 · ReSTIR · RTXDI · Monte Carlo
- **Framing quote:** "Scaling to millions of lights in real time."

### 1.2 The Many-Light Problem

Rendering equation (direct illumination):

```
L_direct(x) = Σ(i=1..N) f_r(x, ω_i) · Φ_i / (4π d_i²) · V(x, l_i) · |cos θ_i|
```

- N = number of dynamic lights, M = number of pixels (8.3M at 4K)
- Each light requires **1 shadow ray** (a visibility test)
- Total cost: **N × M** — billions of operations per frame
- Worked example: N=500 lights × M=8.3M pixels = **4.15 billion ray queries/frame** → at 60fps, completely intractable by brute force.

### 1.3 Requirement Analysis — what a real-time many-light renderer must achieve

| Requirement | Target |
|---|---|
| Performance | < 4ms/frame at 1080p; 1 shadow ray per pixel, not N; 60+ FPS |
| Accuracy | Converges to unbiased ground truth; no systematic ghost illumination |
| Dynamic Lights | Handles hundreds of appearing/disappearing lights (muzzle flash: 3-frame life, explosion: 45-frame life, bullet glow: 1-frame life) |
| Spatial Correctness | Hard shadow boundaries; re-validate every borrowed reservoir sample from the current viewpoint |
| Memory Budget | < 30 bytes/pixel for the reservoir buffer; no O(N×M) storage |

### 1.4 Why every prior method fails at least one requirement

| Method | Requirement Failed | Root Cause |
|---|---|---|
| Brute Force | Performance | 500 shadow rays/pixel — intractable at real-time rates |
| Instant Radiosity | Performance + Memory | O(N×M) rays + 1/d² singularity bias |
| Lightcuts (Walter 2006) | Performance + Bias | Per-pixel independent, biased estimator |
| Stochastic Lightcuts (Yuksel 2018) | Performance | Zero cross-pixel or temporal reuse |
| ReSTIR (Bitterli 2020) | Dynamic Lights | M-cap flaw: ~20 ghost frames per ephemeral light |
| RTXDI (Wyman 2021) | Dynamic Lights | Same M-cap flaw, no adaptive spatial budget |

**No existing method satisfies all 5 requirements simultaneously.** This gap motivates the survey's two novel contributions: **ELCD** and **AHTR**.

### 1.5 Benchmark data (from the survey's comparative analysis)

**Shadow ray workload (N=500 dynamic lights, 1080p, lower = better):**

| Method | Shadow rays/pixel |
|---|---|
| Brute Force | 500 |
| Instant Radiosity | 500 |
| Lightcuts | ~22 (√N) |
| Stochastic Lightcuts | 4 samples × O(log N) |
| ReSTIR | 1 (quality ≈ 500 brute-force samples) |
| RTXDI | 1 (with ReGIR, 8–10× further gain) |
| **ELCD + AHTR (ours)** | **1 ray/leaf tile (adaptive)** — 6.25× fewer shadow rays than per-pixel ReSTIR |

**Ghost frames after a light expires:**

| Method | Ghost frames |
|---|---|
| Brute Force | 0 |
| Lightcuts / Stochastic Lightcuts | N/A (no temporal reuse) |
| ReSTIR | ~20 |
| RTXDI | ~20 |
| **ELCD + AHTR (ours)** | **0** |

**Frame time (1080p, N=500):**

| Method | Time |
|---|---|
| Brute Force | > 16ms (not real-time) |
| Lightcuts | ~8ms |
| Stochastic Lightcuts | ~5ms |
| ReSTIR (GPU, RTX 3080) | ~2ms |
| RTXDI (GPU, with ReGIR) | ~4ms |
| ELCD+AHTR (CPU 2D prototype, 800×450) | ~1.2ms |
| ReSTIR (CPU reference, no adaptive tiles) | ~7.5ms |

**Bias characterization:**

| Method | Bias | Observation |
|---|---|---|
| Brute Force | None (exact) | Reference solution; intractable at real-time |
| Instant Radiosity | Singularity bias | VPL approximation |
| Lightcuts | Systematic bias | Hierarchy over/under-estimates |
| Stochastic Lightcuts | None (unbiased) | Higher variance |
| ReSTIR / RTXDI | Small + temporal ghost bias | Temporal reuse causes residual illumination |
| **ELCD + AHTR** | **Small only (ghost bias eliminated)** | Lifetime-aware reuse + adaptive spatial allocation |

### 1.6 The ReSTIR pipeline (Bitterli et al., SIGGRAPH 2020) — the foundation algorithm

4-pass pipeline:

1. **Candidate Generation** — draw M candidate lights from the full dynamic-light set. Target PDF p(l) = 1/N. RIS weight w = p(l) × N = intensity × G(x, l). (M: 32 → 32)
2. **Temporal Reuse** — reproject the previous frame's reservoir to the current pixel (via motion vectors), combine: `R_now = Combine(R_prev)`. M_cap = 20 in standard ReSTIR (ELCD replaces this heuristic). (M: 32 → 640)
3. **Spatial Reuse** — pick K=5 random neighbour pixels, re-validate visibility of each from *our* pixel (line-of-sight), combine ×K times. (M: 640 → 3200)
4. **Final Shading** — fire exactly **1 shadow ray** to the selected light y. `L = f_r · L_y · G · V · W`, unbiased weight `W = W_sum / (M · p(y))`.

**Payoff:** without ReSTIR, N=500 lights × 2M pixels = 1 billion rays/frame at 1080p (not real-time). With ReSTIR: 1 shadow ray/pixel, effective quality of 3200 samples — a **100× compression ratio** — at 2–4ms on an RTX GPU (7.5ms on the survey's CPU prototype).

### 1.6.5 ⭐ Our Two Contributions, At a Glance — ELCD & AHTR

*(This is its own dedicated slide — slide 8 in the outline. Keep it conceptual and plain-language; the formulas and theorems come right after, on their own slides. The goal of this slide is: in 30 seconds, the audience understands what each algorithm IS and WHY it exists, before seeing any math.)*

Standard ReSTIR (above) is excellent, but it has two specific, separately-fixable weaknesses. Our two contributions fix one each:

**ELCD — Ephemeral Light Confidence Decay**
- **What it is:** a *temporal* fix. It changes how ReSTIR carries a light's influence from one frame to the next.
- **What it does:** every light now carries a "confidence" score that shrinks as the light's remaining lifetime shrinks. The moment a light actually expires, its confidence hits exactly zero, so the renderer stops showing it immediately — instead of ReSTIR's old behavior of letting a dead light's glow visibly linger for about 20 frames.
- **One-line summary:** *"Lights fade out of the renderer's memory exactly as fast as they fade out of the scene."*

**AHTR — Adaptive Hierarchical Tile Resampling**
- **What it is:** a *spatial* fix. It changes how the shadow-ray budget is distributed across the screen.
- **What it does:** instead of spending the same number of rays on every patch of screen regardless of what's happening there, it measures how much the lighting is varying in each patch and gives more rays only to the patches where it's varying a lot (like a shadow's edge), and fewer rays to patches where nothing much is happening (flat, evenly-lit areas).
- **One-line summary:** *"Spend the ray budget where the eye would actually notice, not evenly everywhere."*

**Together:** ELCD keeps the picture honest over *time* (no ghosts), AHTR keeps the picture efficient over *space* (no wasted rays) — the two axes along which a real-time many-light renderer has to be correct.

### 1.7 Novel Contribution 1 — ELCD (Ephemeral Light Confidence Decay)

**Problem:** ReSTIR's M-cap heuristic lets an expired light's reservoir sample linger: Frame 0 (explosion spawns) → Frame 1 (bright) → Frame 3 (fading) → Frame 6 (light has actually expired, but ReSTIR still shows it) → Frame 20 (still a visible "ghost" glow). Standard ReSTIR: **~20 ghost frames** after a light expires, because M-cap keeps feeding the historical sample forward.

**Solution — lifetime-normalized confidence decay:**

```
κ_l(t) = τ_remaining / τ_initial
```

| Frame | τ_remaining | κ_l(t) | Contribution |
|---|---|---|---|
| 0 | 10 | 1.0 | Full |
| 2 | 8 | 0.8 | 80% |
| 5 | 5 | 0.5 | 50% |
| 8 | 2 | 0.2 | 20% |
| 10 | 0 | 0.0 | Removed |

- **Theorem 1 (Zero ghost frames):** if τ_remaining = 0 → κ_l = 0 → the sample is removed in exactly 1 frame.
- **Theorem 2 (No regression):** for persistent lights (τ → ∞), κ_l = 1 → ELCD is numerically identical to standard ReSTIR. ELCD is a strict generalization, not a trade-off.

**Result (explosion test case):** ReSTIR baseline shows ~20 ghost frames after expiry; ELCD shows 0.

### 1.8 Novel Contribution 2 — AHTR (Adaptive Hierarchical Tile Resampling)

**Problem:** a fixed tile grid wastes shadow rays in smooth (low-variance) regions and under-samples high-variance regions (shadow boundaries), because it spends the same budget everywhere regardless of local complexity.

**Solution — adaptive hierarchical tiling based on luminance variance:**

1. Compute per-tile luminance variance: `σ² = (1/n) Σ (L_i − L̄)²`
2. If `σ² > threshold` and not at max depth, split the tile into 4 children (recurse).
3. Allocate samples accordingly: smooth tiles get large area / few samples (e.g. 4); high-variance tiles get small area / many samples (e.g. 16).

- **Theorem (convergence):** `MSE_AHTR = O(σ̄² / Budget)` → near-optimal allocation, concentrating the ray budget exactly where shadow boundaries live.
- **Result (explosion scene):** fixed grid uses 1,450 tiles × 8 samples/tile = 11,600 rays (uniform, wasteful); AHTR uses ~320 adaptive leaf tiles × ~4,800 rays — **6.25× fewer rays**, same visual quality where it matters.

### 1.9 Research Findings & Open Problems (for a "Conclusion / Future Work" slide)

**5 key findings:**
1. ReSTIR is the state-of-the-art answer — 1 shadow ray/pixel reaches the quality of 500 brute-force samples; shipped in Cyberpunk 2077 RT Overdrive, Portal RTX, Battlefield 2042.
2. ReSTIR has a systematic flaw for ephemeral lights — the M-cap heuristic causes ~20 ghost frames per light after expiry; no prior paper addressed this specific failure mode.
3. ELCD provably eliminates ghost frames — proven via κ = τ_remaining/τ_initial, with zero regression on static/persistent-light scenes.
4. A fixed tile grid wastes 84% of rays in smooth regions — AHTR's adaptive quadtree gives 6.25× fewer shadow rays with a proven convergence bound.
5. A CPU software prototype validates both contributions — 1.2ms/frame at 800×450, 60 FPS, C++17, CPU-only; DDA shadow rays produce correct hard shadow boundaries.

**6 open research problems** (good material for a "limitations / what's next" slide):
- P1: ReSTIR ↔ Sequential Monte Carlo theory — temporal reuse is currently a heuristic; the correct framework is particle filtering. Open: a principled adaptive temporal horizon.
- P2: Volumetric many-light rendering — ReSTIR only handles surfaces; fog/smoke/SSS need a volume integral and shift mappings for volume scattering.
- P3: Variance of reservoir Combine — unbiasedness is proven (Bitterli 2020), but variance is not analyzed. When does Combine reduce vs. inflate variance?
- P4: Hardware reservoir acceleration — weighted reservoir sampling runs in software today; dedicated silicon (analogous to RT cores for shadow rays) could be 10–100× faster.
- P5: Adaptive M-candidate allocation — fixed M=32 everywhere wastes budget in dark/simple regions; this is literally AHTR's idea applied to the *candidate* dimension instead of the *spatial* one.
- P6: Spatial propagation convergence rate — how fast does a good sample spread across pixels via K-neighbor reuse? Related to epidemic-spread models on graphs.

---

## PART 2 — FA2: What Was Actually Implemented

### 2.1 Honest scope statement (say this up front — it preempts the obvious question)

> "FA1 proposed building a dedicated SDL2/raycasting demo game from scratch. For FA2, instead of building a toss-away demo, we integrated ELCD and AHTR into **Gun Mayhem**, a complete, already-working C++/Cocos2d-x 2-player platformer shooter — a real shipped-feeling game, not a synthetic test harness. We scoped the integration to the two things that matter for proving the thesis — muzzle-flash and bullet-glow ephemeral lights — rather than touching every light in the game, so the implementation stays small, correct, and demonstrable rather than sprawling."

This is a *stronger* answer than "we built a toy," because it proves the technique works inside a real, pre-existing, non-trivial codebase (~27k lines, cocos2d-x 4.0 engine, Windows native build) without destabilizing it.

### 2.2 Why Gun Mayhem fits the research (same argument as FA1's page 7, still true)

- It's a local 2-player shared-screen shooter: one camera, real-time, constant combat.
- Game events naturally produce ephemeral lights: muzzle flashes and bullet glow on every shot.
- Platforms are axis-aligned solid geometry — a clean, cheap occluder for a 2D visibility ("shadow ray") test.
- Combat is repeatable: firing is player-triggered, so ELCD/AHTR behavior can be demonstrated live and on-demand, not just in a canned benchmark.

### 2.3 Architecture — the `Classes/Lighting/` module

A new, **self-contained** module was added; nothing in the existing game depends on its internals beyond two one-line spawn calls. This was a deliberate engineering choice: a research feature should be addable and removable without risk to the shipping game.

```
Classes/Lighting/
├── LightSource.h        (18 lines of logic)  — the ephemeral light data type + κ_l(t) confidence formula
├── LightManager.h/.cpp  (33 + 48 lines)       — owns active lights, spawns/ages/expires them
├── Reservoir.h           (69 lines)           — weighted reservoir (RIS) + temporal combine, the core ReSTIR data structure
├── Visibility.h/.cpp     (19 + 34 lines)      — the 2D "shadow ray": line-vs-platform occlusion test
└── LightingPass.h/.cpp   (66 + 196 lines)     — orchestrates candidate-gen → AHTR split → ELCD temporal reuse → shadow test → render + debug HUD
```

Total: **~500 lines**, 8 files, 1 responsibility each.

**Touch points into the existing game** (everything else is untouched):

| File | Change |
|---|---|
| `Classes/Weapons/Bullet/Bullet.cpp` | 2 lines: on bullet spawn, call `map->SpawnMuzzleFlash(muzzlePos)` and `map->SpawnBulletGlow(bulletPos)` |
| `Classes/Map/MapBase.h` | forward-declares `Lighting::LightingPass`, adds a pointer member + 2 thin wrapper method declarations |
| `Classes/Map/MapBase.cpp` | constructs the lighting pass once players exist (`initPlayer()`), calls `lighting->Update(dt)` each frame, implements the 2 wrappers, deletes it in the destructor |

This is the "Code/Module Quality & Reusability" evidence: a new subsystem was added to a 4-year-old unfamiliar codebase with a ~20-line integration footprint, with zero changes to gameplay, weapon, or rendering logic outside those touch points.

### 2.4 The per-frame algorithm (what `LightingPass::ComputeFrame()` actually does)

Mapped directly onto ReSTIR's 4 passes, at 2D-tile granularity instead of per-pixel (the resolution a 2D sprite game actually needs — not millions of pixels, a handful of combat lights):

1. **Tile grid:** the play area is divided into a fixed 16×9 grid (144 tiles), persistent across frames so reservoirs can carry state.
2. **Candidate generation (`BuildCandidateReservoir`):** for each tile, every active light whose radius overlaps the tile is folded into a weighted reservoir via weighted reservoir sampling (RIS) — exactly the `Reservoir::Update()` operation from Bitterli 2020's Pass 1.
3. **AHTR — adaptive refinement:** if a tile has **more than one** competing candidate light (the variance signal), it is additionally evaluated at 2×2 sub-tile resolution and the strongest sub-result is kept — spending extra resolution only where lighting is actually contested, exactly the "spend budget where variance is high" principle, scoped to a single refinement level rather than a full recursive quadtree for simplicity.
4. **ELCD — temporal reuse with confidence decay:** the tile's previous-frame reservoir is combined in via `Reservoir::CombineTemporal()`, scaled by `κ_l(t)` looked up from the *current* `LightManager` state. If the referenced light has expired, `κ_l = 0` and the stale sample contributes nothing — this is Theorem 1, implemented exactly as stated, not approximated.
5. **Shadow ray (visibility test):** exactly one `Lighting::IsVisible()` call per tile with a non-zero candidate — the tile center to the chosen light's position, tested against `MapBase::Floor`'s platform rows (the same geometry the game already uses for player stand/fall collision). This is the real "1 ray per leaf tile" metric from the AHTR benchmark, measured and shown live in the debug HUD.
6. **Render + instrumentation:** surviving tiles are drawn as a soft additive glow (`DrawNode::drawDot`), and a live debug HUD reports: active light count, shadow tests this frame, tile count, and a ghost-streak counter (consecutive frames where a tile is still showing a light that `LightManager` confirms has expired — the literal, measurable definition of a "ghost frame" from the FA1 benchmark).

### 2.5 Live, in-engine validation controls

Unlike the FA1 CPU prototype (static numbers in a table), the FA2 integration exposes the comparison **live, in the running game**, via keyboard toggles:

| Key | Effect |
|---|---|
| **F1** | Show/hide the lighting debug HUD (active lights, shadow tests, ghost streak) |
| **F2** | Toggle ELCD on/off *at runtime* — watch the ghost-streak counter behave differently immediately |
| **F3** | Toggle AHTR on/off *at runtime* — watch the shadow-test count change immediately |

This turns the viva demo into: *fire continuously, point at the HUD, flip F2 off and on, and the audience sees the ghost-streak counter stop resetting to 0 when ELCD is off* — a direct, undeniable, live reproduction of the FA1 survey's central claim.

### 2.6 Limitations

This slide is about the **algorithms' scope**, not about the game or the demo. Put only these two bullets on this slide, verbatim, nothing added:

- This is a 2D, CPU-side, tile-granularity analogue of ReSTIR, not a GPU per-pixel implementation — appropriate for a 2D sprite game, not a claim of matching the GPU numbers from the original papers.
- AHTR here is a single-level adaptive refinement (coarse tile → optional one 2×2 split), not a full recursive quadtree.

---

## PART 3 — Rubric Mapping

*(from your Activity 2 rubric image)*

| Rubric Row | What to present as evidence |
|---|---|
| **Implementation & Feature Completeness** | Live demo: fire weapons, show glow + F1 HUD live, F2/F3 toggles visibly changing behavior. State clearly: core ELCD+AHTR algorithmic claims (ghost-frame elimination, adaptive ray reduction) are implemented and measurable, scoped deliberately to muzzle-flash/bullet-glow (not every light type) — a conscious scope decision, not a cut corner. |
| **Code/Module Quality & Reusability** | Show `Classes/Lighting/` folder structure: 8 files, each single-responsibility (LightSource = data, LightManager = lifecycle, Reservoir = sampling math, Visibility = occlusion, LightingPass = orchestration). Point out the ~20-line integration footprint into the existing ~27k-line codebase — no existing class was rewritten, only additively extended. |
| **Exception Handling & Game/Application-Level Design** | `LightingPass` null-checks (`if (lighting) lighting->Update(dt)`), safe destruction (`MapBase::~MapBase() { delete lighting; }`), lights self-expire via lifetime rather than manual cleanup, reservoir math clamps weights/M to avoid runaway growth (`ClampM`/the `mCap` parameter in `CombineTemporal`). |
| **Execution, Performance & User Experience** | Game still runs at full speed with the feature on; the glow is deliberately subtle (small radius, low alpha) so it enhances rather than disrupts the existing visual style; F1 HUD is opt-in (hidden by default) so it never interferes with normal play. |
| **Demonstration, Presentation & Viva/QA** | Use the demo script below. Be ready to explain *why* ELCD/κ_l(t) mathematically guarantees zero ghost frames (Theorem 1) and why AHTR's adaptive split reduces ray count (variance-driven budget allocation) — both are direct, confident, one-sentence answers once you've read Part 1 above. |

---

## PART 4 — Suggested Slide-by-Slide Outline

*(Paste this section into a slide generator / "Claude design" and ask it to build the deck — each line is a slide.)*

1. **Title slide** — "Real-Time Many-Light Rendering via Spatiotemporal Reservoir Resampling: From Survey to Implementation." Course code, your name, FA1→FA2.
2. **The Many-Light Problem** — rendering equation, N×M billions-of-rays blowup, the 4.15B/frame worked example. (Source: Part 1.2)
3. **What a Real-Time Renderer Must Achieve** — the 5 requirements table. (Part 1.3)
4. **Why Every Prior Method Fails** — the requirement-failure table. (Part 1.4)
5. **Benchmark Comparison** — shadow-ray workload chart + ghost-frame table + frame-time table. (Part 1.5)
6. **The ReSTIR Pipeline** — 4-pass diagram (candidate → temporal → spatial → shading), the "without vs with ReSTIR" 1-billion-vs-1-ray punchline. (Part 1.6)
7. **⭐ Our Two Contributions, At a Glance — ELCD & AHTR** — the dedicated plain-language "what are these two algorithms" slide. No formulas here; just what each one is, what it does, and the one-line summary for each. (Part 1.6.5) — **no image on this slide.**
8. **Contribution 1: ELCD — The Math** — the ghost-frame problem frames, the κ_l(t) formula + table, Theorem 1 & 2. (Part 1.7)
9. **Contribution 2: AHTR — The Math** — smooth-vs-high-variance tile illustration, the split algorithm, the convergence theorem, the 6.25× number. (Part 1.8)
10. **FA2: From Survey to Shipped Game** — the honest scope statement (Part 2.1) + why Gun Mayhem fits (Part 2.2).
11. **System Architecture** — the `Classes/Lighting/` module diagram + file table + integration-footprint table. (Part 2.3)
12. **Algorithm → Code Mapping** — the 6-step per-frame pipeline, each step named against the real function (`BuildCandidateReservoir`, `CombineTemporal`, `IsVisible`, etc). (Part 2.4)
13. **Live Validation Tooling** `[IMAGE PLACEHOLDER — the ONLY image in the whole deck: author will insert their own screenshot of the running implementation here, e.g. the in-game debug HUD or the lighting glow in action]` — F1/F2/F3 debug controls described in text alongside the image. (Part 2.5)
14. **Live Demo** — (no slide content, just the game running) — see Part 5 demo script.
15. **Limitations** — exactly the two bullet points in Part 2.6, nothing else.
16. **Research Findings Recap** — the 5 key findings from FA1. (Part 1.9)
17. **Open Problems / Future Work** — pick 2–3 of the 6 (P1, P4, P5 read well for a game-dev audience). (Part 1.9)
18. **Conclusion** — one slide: the thesis (ReSTIR's ghost-frame/fixed-grid flaws are real and fixable), the proof (ELCD/AHTR implemented and measured, in theory *and* in a real shipped game), the number that matters (0 ghost frames, 6.25× fewer rays).
19. **Q&A / Thank you**

---

## PART 5 — Live Demo Script (for the viva)

1. Launch `dist/Gun-Mayhem.exe`, start a 2-player local match.
2. Press **F1** — HUD appears, all counters at 0 (idle state — correct, no lights active).
3. Hold fire on either player — point at "Active lights" and "Shadow tests this frame" going non-zero in real time.
4. Say: *"Each shot spawns a muzzle flash and a bullet-glow light with a real, short lifetime — 3 frames and 1 frame respectively, matching the FA1 survey's spec exactly."*
5. Press **F2** to disable ELCD, keep firing in bursts with gaps — watch "Ghost streak" climb and stop resetting to 0 between bursts (the bug the FA1 survey documented in standard ReSTIR).
6. Press **F2** again to re-enable ELCD — same firing pattern — "Ghost streak" now returns to 0 almost immediately. Say: *"That's Theorem 1, live: κ_l(t) hits zero the frame a light expires, so its stale reservoir sample stops contributing."*
7. Press **F3** to disable AHTR, fire near both players close together (forces multiple competing lights in one tile) — note the "Shadow tests this frame" number.
8. Press **F3** again to re-enable AHTR, repeat — show the adaptive refinement changes which/how many tiles get the extra sub-split pass.
9. Close with: *"Same algorithmic structure as the SIGGRAPH 2020 paper and our own FA1 theorems — just scoped to what a 2D sprite game's combat lighting actually needs, inside a real, already-shipping game rather than a disposable demo."*

---

*Generated for FA2 — content sourced from `124B1B140_CGG.pdf` (FA1 survey) and the actual `Classes/Lighting/` implementation in this repository.*
