# Presentation Script
### Real-Time Many-Light Rendering via Spatiotemporal Reservoir Resampling: From Survey to Implementation
Speaker: Mohit Rawat · PRN 124B1B140

This script now matches your **actual built deck**, `FA2 — ELCD & AHTR in Gun Mayhem.pptx` (12 slides), word for word against its real slide titles and content. Every technical term is defined in plain words the moment it's first said. Slides 5, 6, and 7 — the architecture/code slides — are covered in full depth below, quoting the real source files line by line, because that's the part most likely to get a "wait, show me that in the code" question. Read it a few times until it's natural; don't read it verbatim off the slide.

## Timing & Delivery Notes — Read This First

**Most of your time is the live demo, not the slides.** The slide narration below is written to run lean and theoretical — what the problem is, what's new, what you're proposing, what you're still thinking about — not a line-by-line code reading. Suggested pacing for a ~15–20 minute slot:

- **Slides 1–4 (context, recap, scope, test bed): ~3 minutes.** Fast — this is "here's what FA1 established and why FA2 exists," not a re-derivation.
- **Slides 5–7 (architecture): ~2–3 minutes, kept at the conceptual level.** Say what each piece *is for* in one sentence (the "Short version for the slide" lines below are exactly that length, by design) — do **not** read the "Deep Dive" code listings aloud here. That section exists purely as Q&A ammunition, for if someone says "show me that in the actual code" — have it ready, don't pre-empt it.
- **Slides 8–9 (before/after, validation): ~1–2 minutes**, then **go straight into the live demo** — this is where the bulk of your remaining time should go. Fire weapons, toggle F1/F2/F3 live, let the ghost-streak counter and shadow-test count visibly change as you flip ELCD/AHTR on and off. This is your strongest evidence; don't rush past it to get back to slides.
- **Slides 10–12 (limitations, conclusion, thank you): ~2 minutes.** Land the three numbers (0, 6.25×, 1) and stop.

Every slide's talking points below lean on three questions, in this order — lead with these, not with implementation mechanics: **what's actually new here** (ELCD and AHTR as original contributions, not just "I used ReSTIR"), **what's the theoretical backing** (the theorems, not just "it works"), and **what's still open / what you're thinking about next** (the six future-work items). The "how it's coded" material is real and ready if asked, but it's supporting evidence for the talk, not the talk itself.

---

## Part 1 — The Actual 12-Slide Deck, Slide by Slide

### Slide 1 — Title

"Good [morning/afternoon]. This is 'Real-Time Many-Light Rendering via Spatiotemporal Reservoir Resampling — From Survey to Implementation: ELCD and AHTR inside Gun Mayhem.' Computer Graphics and Gaming, BCE25PE01, FA2. I'm Mohit Rawat, PRN 124B1B140. FA1 was the survey and the theory; FA2 — what I'm presenting today — is that theory actually integrated into a real, working game, with the results measurable live, not just on paper."

### Slide 2 — Context · From FA1

"Quick recap of the two fixes from FA1, because everything today builds on them. Two separate problems, two separate fixes.

Temporal fix: **ELCD**, Ephemeral Light Confidence Decay. 'Ephemeral' means short-lived — a muzzle flash, an explosion, not an always-on lamp. Every light carries a confidence number, kappa, equal to its remaining lifetime divided by its initial lifetime. When the light expires, kappa hits zero, and its glow disappears that same frame — not the roughly-twenty-frame ghost that standard ReSTIR leaves behind. As the slide says: 'Lights fade out of the renderer's memory exactly as fast as they fade out of the scene.'

Spatial fix: **AHTR**, Adaptive Hierarchical Tile Resampling. Measure how much the lighting actually varies within each screen tile. Tiles near a shadow edge, where it's varying a lot, get split into smaller tiles and more rays; tiles sitting in flat, uneven-nothing-going-on regions get fewer. 'Spend the ray budget where the eye would actually notice, not evenly everywhere.'"

### Slide 3 — FA2 · Scope

"FA1's plan was a brand-new SDL2 and raycasting demo, built from scratch — a synthetic test harness, useful once to prove the idea, then thrown away. FA2 did something different and, I'd argue, stronger: ELCD and AHTR integrated directly into Gun Mayhem — a complete, already-existing, already-working C++ and Cocos2d-x 2-player platformer shooter. Cocos2d-x 4.0 engine, Windows native build. Two light types feed it: muzzle flash and bullet glow, straight off every shot fired. And this was scoped on purpose — small, correct, and demonstrable, not sprawling. I didn't light every object in the game; I lit exactly the two things that prove the thesis."

### Slide 4 — FA2 · Test Bed

"Why Gun Mayhem specifically. One camera, constant combat — it's a local 2-player shared-screen shooter running in real time, so there's always something happening on screen. It produces natural ephemeral lights for free — every single shot makes a muzzle flash and a bullet glow, no scripting needed. The platforms are cheap, clean occluders — they're axis-aligned solid rectangles, which makes the 2D 'shadow ray' visibility test a simple line-versus-rectangle check instead of a complicated general raycast. And it's repeatable on demand — firing is player-triggered, so I can show ELCD and AHTR behaving correctly *live*, in front of you, not just in a pre-recorded benchmark."

### Slide 5 — Architecture · The Self-Contained `Classes/Lighting/` Module

*(This is the first of three code-heavy slides — see the full deep-dive below. Short version for the slide itself:)*

"Everything lives in one new folder, `Classes/Lighting/`, five files, each with exactly one job. `LightSource.h` is the light data type and the confidence formula. `LightManager` owns the list of currently-alive lights. `Reservoir.h` is the actual weighted-reservoir data structure — the core ReSTIR machinery. `Visibility` is the 2D shadow ray. `LightingPass` orchestrates all of it every frame. The design goal, stated outright: a research feature that can be added or removed without risk to the shipping game — and the fact that it's five small, single-purpose files instead of one tangled one is what makes that true."

### Slide 6 — Architecture · Integration

*(Second code-heavy slide — see deep-dive below. Short version:)*

"Three touch points into the existing game, and that's genuinely all of it. `Bullet.cpp` gets two new lines on spawn. `MapBase.h` gets one forward declaration, one pointer member, two thin method declarations. `MapBase.cpp` builds the lighting pass once, updates it once a frame, and cleans it up in the destructor. This is my evidence for the 'Code and Module Quality' rubric row — a new subsystem added to an unfamiliar, pre-existing codebase with a footprint you can count on two hands."

### Slide 7 — Algorithm → Code

*(Third and deepest code slide — full deep-dive below. Short version:)*

"This is `LightingPass::ComputeFrame()`, the function that runs once a frame, mapped step by step onto ReSTIR's own passes. Tile grid, candidate generation, AHTR refinement, ELCD temporal reuse, the shadow ray, then render plus HUD. Six steps, each one a named function you can go read right now, each one doing exactly what the theory paper says it should."

### Slide 8 — Before and After: ELCD + AHTR

"This illustration contrasts standard ReSTIR against ELCD-plus-AHTR on the same scenario. Amber dots are lights, blue dots are shadow rays, the dashed line marks the exact moment a light expires. Before: the amber glow is still there well past the dashed line — that's the ghost. After: the glow stops exactly at the dashed line. That one visual difference is Theorem 1, drawn instead of proven."

### Slide 9 — Validation · Live Validation Tooling

"And this isn't just a one-time recorded result — it's live, in the running game, any time. Press F1 to show or hide a debug HUD: active light count, shadow tests this frame, ghost streak. Press F2 to toggle ELCD on and off *while playing* — the ghost-streak counter visibly changes behavior immediately. Press F3 to toggle AHTR — the shadow-test count changes immediately.

**This is your cue to switch to the live demo.** Don't linger on the slide — say the paragraph above, then alt-tab into the running game and actually do it: fire, show the HUD, flip F2, flip F3. The slide is the preview; the demo is the proof, and it should get the most airtime of anything in the whole talk."

### Slide 10 — Scope of the Algorithms · Limitations

"Two honest scope notes, and only these two — stated plainly rather than buried. This is a 2D, CPU-side, tile-granularity analogue of ReSTIR, not a GPU per-pixel implementation — the right level of abstraction for a 2D sprite game, not a claim of matching the original papers' GPU numbers. And AHTR here is a single-level adaptive refinement — a tile either stays coarse or splits once into four — not a full recursive quadtree. Both are deliberate scoping choices for a small, correct, demonstrable system, not corners cut by accident."

### Slide 11 — Conclusion

"Three numbers to leave you with. Zero — ghost frames with ELCD, proven, not measured-and-hoped. Six point two five times — fewer shadow rays with AHTR under the stated allocation model. One — shadow ray per leaf tile, the entire point of the reservoir machinery in the first place. ReSTIR's ghost-frame and fixed-grid flaws are real, and they're fixable — and the proof is both in the theory, from FA1, and live, right now, inside Gun Mayhem."

### Slide 12 — Thank You

"Thank you. Questions and discussion — Mohit Rawat, PRN 124B1B140, Computer Graphics and Gaming, BCE25PE01."

---

## Deep Dive — Slides 5, 6, 7: The Actual Code, Explained Line by Line

Use this section if anyone — professor or classmate — asks "show me that in the code," or if you want to go deeper than the slide bullets during Q&A. It's organized exactly like slides 5/6/7: architecture, integration, then the algorithm-to-code mapping.

### Deep Dive A (Slide 5) — The Five Files of `Classes/Lighting/`

**`LightSource.h`** — the data type every light is, plus the confidence formula itself:
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

    float confidence() const {
        if (initialLifetime <= 0.f) return 0.f;
        return cocos2d::clampf(remainingLifetime / initialLifetime, 0.f, 1.f);
    }
};
```
Say: "This `confidence()` function *is* kappa-of-t from the slide, typed directly as code — remaining lifetime over initial lifetime, clamped to between 0 and 1. Nothing fancier than that. Every other file in this module calls this one function; this is the one place the ELCD formula actually lives."

**`LightManager.h/.cpp`** — owns the list of currently-alive lights, spawns and ages and expires them:
```cpp
void LightManager::SpawnMuzzleFlash(const cocos2d::Vec2& position) {
    SpawnLight(position, LightType::MuzzleFlash, 3.f/60.f, 55.f, Color4F(1.f, 0.75f, 0.3f, 1.f));
}
void LightManager::Update(float dt) {
    for (auto& light : lights_) light.remainingLifetime -= dt;
    lights_.erase(std::remove_if(lights_.begin(), lights_.end(),
        [](const LightSource& l) { return l.remainingLifetime <= 0.f; }), lights_.end());
}
```
Say: "Spawn functions just construct a `LightSource` with a lifetime — three frames for a muzzle flash, matching the FA1 spec exactly. `Update()` ages every light by delta-time every frame and erases the ones that have hit zero. This is deliberately not a singleton — each match's `MapBase` owns its own `LightManager`, so state resets cleanly between games, no leftover state haunting the next round."

**`Reservoir.h`** — the actual weighted-reservoir-sampling structure, the literal implementation of the 'reservoir' in ReSTIR's name:
```cpp
bool Update(int id, float weight, const Vec2& pos, const Color4F& c, float r) {
    if (weight <= 0.f) return false;
    weightSum += weight; M += 1.f;
    float rnd = RandomHelper::random_real<float>(0.f, 1.f);
    if (rnd * weightSum <= weight) { lightId=id; position=pos; color=c; radius=r; return true; }
    return false;
}
void CombineTemporal(const Reservoir& prev, float confidenceScale, float mCap) {
    if (prev.lightId < 0 || prev.M <= 0.f || confidenceScale <= 0.f) return;  // <- ELCD lives here
    ...
}
```
Say: "`Update` is textbook weighted reservoir sampling — every new candidate has a chance of becoming the stored pick, proportional to its weight over the running total. `CombineTemporal` is where last frame gets folded in, and look at that guard clause on the first line — `confidenceScale <= 0.f` — that's ELCD's kill switch, in exactly one line of code. If the light this reservoir remembers has expired, confidence is zero, the function returns immediately, and the stale sample contributes nothing. That line is Theorem 1."

**`Visibility.h/.cpp`** — the 2D shadow ray:
```cpp
bool IsVisible(const Vec2& a, const Vec2& b, const std::vector<std::vector<float>>& floorRows,
               float floorBase, float floorHeight) {
    for (size_t row = 0; row < floorRows.size(); ++row) {
        float rowY = floorBase + floorHeight * (float)row;
        if (rowY <= min(a.y,b.y) || rowY >= max(a.y,b.y)) continue;
        float x = a.x + ((rowY - a.y)/(b.y - a.y)) * (b.x - a.x);
        if (RowBlocks(floorRows[row], x)) return false;
    }
    return true;
}
```
Say: "This is the actual shadow ray, and it reuses the game's *own* platform data — `MapBase::Floor`, the exact same array the game already uses to decide whether a player is standing on solid ground. No new occlusion geometry was built; the shadow ray just walks the platform rows between two points and checks whether any of them block the straight line."

**`LightingPass.h/.cpp`** — orchestrates all four of the above, once a frame, which is slide 7 in full — covered next.

### Deep Dive B (Slide 6) — The Three Integration Touch Points

Say: "Three files touched, and here's literally all of it."

**`Weapons/Bullet/Bullet.cpp`**, inside `Bullet::init()`:
```cpp
player->map->SpawnMuzzleFlash(init);
player->map->SpawnBulletGlow(currentPoint);
```
"Two lines. Every bullet fired by every gun in the game already calls `Bullet::create()` — that was already true before any of this — so hooking in here means every weapon in the game automatically gets lighting, with zero changes to any individual gun class."

**`Map/MapBase.h`** — the header gains a forward declaration and a member:
```cpp
namespace Lighting { class LightingPass; }
...
Lighting::LightingPass* lighting = nullptr;
...
void SpawnMuzzleFlash(const Vec2& pos);
void SpawnBulletGlow(const Vec2& pos);
void SpawnExplosion(const Vec2& pos);
```
"Forward-declaring `LightingPass` instead of including its header means `MapBase.h` — which is included by nearly everything in the game — doesn't have to know anything about the Lighting module's internals. That's a deliberate compile-time firewall."

**`Map/MapBase.cpp`** — construction, per-frame update, and the wrapper methods:
```cpp
void MapBase::update(float dt) {
    ...
    if (lighting) lighting->Update(dt);
}
void MapBase::SpawnMuzzleFlash(const Vec2& pos) {
    if (lighting) lighting->GetLightManager().SpawnMuzzleFlash(pos);
}
```
"One call added to the existing `update()` loop, and each wrapper is a one-line null-checked forward. Say this explicitly if asked about robustness: every single call site null-checks `lighting` first, so even in some hypothetical state where the lighting pass hasn't been constructed yet, nothing crashes — it just silently does nothing that frame."

### Deep Dive C (Slide 7) — `LightingPass::ComputeFrame()`, the Whole Algorithm in Six Steps

This is the function that *is* the algorithm — walk through it exactly in this order, it mirrors the slide's six numbered steps precisely.

**01 — Tile grid.** Built once, persisted across frames (so reservoirs can carry temporal state):
```cpp
cols_ = 16; rows_ = 9;   // 144 tiles, fixed for the whole match
```

**02 — Candidate generation**, `BuildCandidateReservoir()`:
```cpp
Reservoir BuildCandidateReservoir(const std::vector<LightSource>& lights, const Rect& bounds, ...) {
    Reservoir r;
    for (auto& light : lights) {
        if (!TileCatchesLight(bounds, light)) continue;
        r.Update(light.id, light.radius, light.position, light.color, light.radius);
    }
    return r;
}
```
"Every light whose radius overlaps this tile gets folded into a reservoir via `Reservoir::Update` — that's ReSTIR Pass 1, candidate generation, literally the function from Deep Dive A."

**03 — AHTR refinement:**
```cpp
if (ahtrEnabled_ && candidates.size() > 1) {
    Reservoir best = combined;
    for (int q = 0; q < 4; ++q) {
        Reservoir sub = BuildCandidateReservoir(lights, Quadrant(tile.bounds, q), nullptr);
        if (sub.FinalWeight() > best.FinalWeight()) best = sub;
    }
    combined = best;
}
```
"If a tile has more than one competing light — the variance signal — split it into four quadrants, redo candidate generation at that finer resolution, and keep whichever sub-result is strongest. Extra work only where more than one light is actually arguing over the same tile."

**04 — ELCD temporal reuse:**
```cpp
float confidence = prevLight ? prevLight->confidence() : 0.f;
combined.CombineTemporal(tile.prevReservoir, confidence, 20.f);
```
"Exactly what Deep Dive A showed in `Reservoir.h`, called here with the real confidence value looked up from the *current* `LightManager` — if the light that tile remembers no longer exists, `FindById` returns null, confidence is forced to zero, and `CombineTemporal`'s guard clause kills it."

**05 — The shadow ray:**
```cpp
bool visible = IsVisible(tileCenter, combined.position, map_->Floor, map_->floor_base, map_->floor_height);
if (visible) tile.hasLight = true;
else combined.weightSum = 0.f;
```
"One call to the `Visibility.cpp` function from Deep Dive A, tile center to the chosen light. That's the '1 ray per leaf tile' line from the conclusion slide, happening right here."

**06 — Render plus HUD:**
```cpp
overlay_->drawDot(tile.reservoir.position, tile.reservoir.radius * 0.35f, c);
```
"And finally, every tile that survived gets drawn as a soft glow. The HUD string built right after this reads out `shadowTestsThisFrame_`, `activeLightsThisFrame_`, and `currentGhostStreak_` — the exact three numbers you toggle live with F1/F2/F3 on slide 9."

**If asked "is this really ReSTIR or just inspired by it":** "It's the same four-pass structure — candidate generation, temporal reuse, spatial reuse (here, the AHTR refinement plays that role), and final shading — just applied at screen-tile granularity on the CPU instead of per-pixel on the GPU, which is the honest scope statement from slide 10."

---

## Appendix — Rapid-Fire Term Glossary (for Q&A)

Keep this page open or memorized; if asked "what does X mean" cold, here's the one-liner:

- **Rendering** — turning a scene description into a picture, fast enough to feel interactive.
- **Shadow ray** — a test: "is anything solid blocking the line between this point and that light?"
- **Direct illumination** — light that travels straight from a source to a surface, no bounces.
- **N, M** — N = number of lights, M = number of pixels (or tiles).
- **Monte Carlo** — estimating something expensive by taking random samples instead of checking everything.
- **Importance sampling** — biasing your random samples toward the things that matter more (brighter/closer lights), instead of sampling uniformly.
- **PDF (probability density function)** — how likely each option is to be picked.
- **Bias / unbiased** — unbiased means it converges to the exact right answer on average; biased means it's systematically a bit wrong, consistently, no matter how many samples.
- **Variance** — how much an estimate flickers/wobbles frame to frame, even if unbiased.
- **MSE (mean squared error)** — squared, averaged error — the standard way to measure "how wrong," used in the AHTR convergence bound.
- **Reservoir** — a small structure holding one chosen candidate plus a running weight and sample count.
- **Weighted reservoir sampling** — picking one fairly-weighted item from a stream without storing the whole stream.
- **RIS (resampled importance sampling)** — generate cheap candidates, then reservoir-resample down to one good representative.
- **Spatiotemporal** — spatial (across nearby pixels, this frame) + temporal (across time, reusing last frame).
- **Temporal reuse** — reusing last frame's computed result instead of recomputing from scratch.
- **Spatial reuse** — borrowing a neighboring pixel's/tile's already-computed candidate.
- **ReSTIR** — **RE**servoir **S**patio**T**emporal **I**mportance **R**esampling; Bitterli et al., SIGGRAPH 2020.
- **M-cap** — the fixed limit on how much temporal history a reservoir can accumulate (ReSTIR default: 20).
- **RTXDI** — NVIDIA's production implementation/extension of ReSTIR (2021).
- **Ephemeral (light)** — short-lived — a muzzle flash, an explosion, as opposed to an always-on lamp.
- **Ghost frame / ghosting** — a light's visible effect lingering after it has actually expired, from stale temporal reuse.
- **Confidence (κ)** — remaining lifetime ÷ initial lifetime; 1 = fully trust, 0 = don't use at all.
- **ELCD** — Ephemeral Light Confidence Decay — scale temporal reuse weight by confidence, so expired lights contribute zero.
- **Tile** — a small screen region processed as one unit instead of per-pixel.
- **Luminance** — a measure of brightness, used as the proxy for "how much lighting varies here."
- **Hierarchical** — organized coarse-to-fine, in levels (a quadtree splits each tile into 4, recursively).
- **Adaptive** — effort adjusted by local need, instead of fixed/uniform everywhere.
- **AHTR** — Adaptive Hierarchical Tile Resampling — split tiles by variance, concentrate ray budget at shadow edges.
- **VPL (virtual point light)** — a small fake point light used by Instant Radiosity to approximate bounced light.
- **Convergence** — how fast an estimate's error shrinks as you add more samples/budget.

---

## Part 2 — Walking Through the Written Research Paper, Section by Section

If you're asked to explain the paper itself — not just present slides — use this. It goes through `Research_Paper.docx` in the exact order a reader encounters it: title and author, then Abstract first, then each numbered section in sequence. Say this the way you'd explain your own paper to someone who's just opened it to page one.

**Title & Author.** "The title is 'Confidence-Decayed Temporal Reuse and Variance-Adaptive Hierarchical Tiling for Real-Time Many-Light Rendering' — that's ELCD and AHTR, named formally instead of by acronym, because a paper title should describe what the method *does*. I'm the sole author, Mohit Rawat."

**Abstract.** "The abstract does five things in order, and every paper abstract should: state the problem (real-time rendering with hundreds of dynamic lights is intractable by brute force), name the existing best solution (ReSTIR), state precisely what's wrong with it (two limitations — ghosting and fixed-grid inefficiency), state my two fixes by name (ELCD and AHTR) with the one-sentence guarantee each comes with, and state how they're evaluated (analytically, against closed-form cost models, not against a specific implementation). If someone only reads one paragraph of this paper, it's this one — everything after just proves these claims in detail."

**I. Introduction.** "The introduction has four paragraphs, each with a job. Paragraph one sets up the many-light problem itself — the N-times-M cost — with a concrete worked number so it's not abstract: 500 lights at 4K is over four billion ray queries a frame. Paragraph two introduces ReSTIR by name and gives it credit — it's deployed in real shipped games, so dismissing it would be dishonest — before saying it's 'not, however, complete.' Paragraph three is the contributions paragraph — every research paper needs one, stated as a numbered list, each contribution tied to the exact section where it's proven. Paragraph four is the roadmap — literally telling the reader which section is which, so they can skip ahead if they want."

**II. Related Work.** "This section earns the right to claim a gap — you can't say 'nobody has solved X' without first showing you've actually read what everybody else tried. It's split into two parts: part A covers the methods *before* reservoir resampling existed — Instant Radiosity, Lightcuts, Stochastic Lightcuts — each with its own specific flaw named, not just 'it's slower.' Part B covers ReSTIR and RTXDI themselves, establishing that they're the current best answer, which is what makes improving on them meaningful rather than trivial."

**III. Background.** "This is the one section that isn't about contributions at all — it's pure setup, teaching the reader the three things they need to know before section four's claims make sense: the actual rendering-equation math being estimated, how resampled importance sampling and weighted reservoir sampling work as an algorithm — including the full pseudocode listing, Algorithm 1 — and exactly what ReSTIR's four passes do, pass by pass."

**IV. Research Gap — say this one carefully, it's the section most likely to get a direct question.** "This is the section that turns 'ReSTIR has some issues' into something rigorous. It's structured in three parts. Part A is a requirement-coverage table: I defined five concrete requirements a real-time many-light renderer actually needs, and showed, method by method, which requirement each one fails. That table is the gap, made visible. Part B and part C then take the *specific* empty cell for ReSTIR — ghosting, and fixed-grid inefficiency — and formalize each one mathematically: a precise definition of what a ghost frame *is*, and a closed-form equation proving a fixed sampling grid's error doesn't depend on where the complexity in the scene actually is. That's what makes it a research gap and not just an opinion: it's measurable, and section five exists specifically to close exactly these two measured gaps, point for point."

**V. Proposed Method.** "This is the paper's core contribution, in two parts matching the two gaps. Part A is ELCD — I define a confidence number for every light, show the one-line change it makes to ReSTIR's temporal reuse formula, and prove two theorems: Theorem 1, ghost frames drop to exactly zero; Theorem 2, it never makes anything worse for a light that never expires. Part B is AHTR — I derive the splitting rule from scratch using Lagrange multipliers, connect it to a seventy-year-old result in classical statistics called Neyman allocation, and prove — Theorem 3 — that variance-proportional sampling can never do worse than uniform sampling, with a Cauchy-Schwarz inequality as the proof's last step."

**VI. Theoretical Complexity Analysis.** "A short section with one job: one table showing, in big-O notation, how each method's cost scales with light count and pixel count. The sentence worth memorizing: ReSTIR-family methods — including AHTR — decouple cost from the number of lights entirely."

**VII. Analytical Evaluation.** "This is the results section, and it's deliberately 100% calculated, not measured — stated explicitly in the opening paragraph. Four subsections, each with a table: shadow-ray counts computed from the Section VI formulas; the ghost-frame duration comparison, empirical-observation-from-prior-work versus my own zero-frame proof; the AHTR ray-reduction factor, worked out from an explicit, stated formula; and a memory-footprint calculation of the reservoir data structure, down to the byte."

**VIII. Discussion.** "Two short parts. Part A points out AHTR's theorem is a seventy-year-old sampling-theory result applied to a new domain — a mark of rigor, not a weakness. Part B is the honest scope statement: two sentences, stating exactly what this analysis does and doesn't claim."

**IX. Conclusion and Future Work.** "One paragraph restating the two theorems' punchlines, then six numbered open problems — the particle-filtering connection and the hardware-acceleration one are the most natural to explain quickly if asked what's next."

**References.** "Eight sources, every one a real, citable, well-known work in this exact research area. If asked 'what's your primary source,' the answer is reference one, Bitterli et al. 2020 — the ReSTIR paper everything in this work builds on and extends."
