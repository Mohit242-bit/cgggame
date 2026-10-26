# Glossary & Algorithm Walkthrough — Plain-Language Edition

Every piece of jargon from the FA1 survey and the FA2 implementation, explained simply, followed by a step-by-step walkthrough of how the algorithm actually works. Read the glossary first if a term trips you up mid-walkthrough; everything is also explained inline where it first appears.

---

## Part 1 — Glossary

### Rendering basics

**Rendering** — the process of turning a 3D (or 2D) scene description into a picture, frame by frame. "Real-time rendering" means doing this fast enough to feel interactive — typically 60 frames every second, so each frame has about **16 milliseconds** to be computed.

**Light source** — anything in the scene that emits light: a lamp, a muzzle flash, an explosion, the sun. A scene can have just a few of these, or (the problem this whole survey is about) hundreds or thousands of them moving and flickering every frame.

**Direct illumination** — the light that travels straight from a light source to a surface, with nothing bouncing in between. (As opposed to *indirect/global illumination*, which is light that bounces off other surfaces first — not what this project deals with.)

**Shadow ray** — a straight test: "is there anything solid between this point and that light?" If yes, the point is in shadow from that light; if no, it's lit. This is the single most expensive operation in the whole problem, because you'd naively want to do one per light, per pixel, per frame.

**Visibility test / occlusion test** — same thing as a shadow ray, described more generally: checking whether one point can "see" another, or whether something solid (an **occluder**) blocks the line between them.

**The rendering equation** — the formal math description of "how much light arrives at this point, from this light, accounting for angle and distance and whether anything's in the way." You don't need to memorize the formula; what matters is it has one term *per light source*, which is exactly why having many lights is expensive — you're summing this equation over every single one.

### The many-light problem

**The many-light problem** — simply put: *if a scene has N lights and the screen has M pixels, naively you need to do N shadow-ray tests for every pixel, so N×M total per frame.* With 500 lights and a few million pixels, that's billions of operations per frame — far too slow for 60fps. The entire research area (and this project) is about getting the *same visual result* for a tiny fraction of that cost.

**N, M** — just variable names used throughout the literature: **N** = number of lights, **M** = number of pixels (or, in this project, number of screen tiles).

**GPU / real-time budget** — a modern game needs to finish an entire frame (all rendering, physics, AI, everything) in about 16ms to hit 60fps. "This technique costs 2ms" means it leaves 14ms for everything else — that's why every comparison table in the survey is in milliseconds.

### Sampling & statistics terms

**Monte Carlo (method)** — a general strategy for estimating something expensive-to-compute-exactly by taking *random samples* instead of checking everything. Example: to estimate "what fraction of this circle is red," instead of checking every single point, you throw 100 random darts and see what fraction landed on red. More samples = more accurate, but each sample costs time, so the whole game is "get a good estimate with as few samples as possible."

**Importance sampling** — a smarter version of random sampling: instead of sampling *uniformly at random*, you bias your random choices toward the things that matter more. If one light is much brighter or closer than the others, you should be more likely to "pick" it when sampling — picking a dim, far-away light wastes a sample.

**PDF (probability density function)** — in this context, just "how likely is each option to be picked." A light with PDF = 1/N means "equally likely out of N lights" (uniform sampling); importance sampling uses a non-uniform PDF that favors brighter/closer lights.

**Bias / unbiased** — an estimate is **unbiased** if, averaged over infinitely many tries, it converges to the *exact correct answer*. It's **biased** if it systematically over- or under-estimates no matter how many samples you take. Unbiased is generally better (more trustworthy), but can have more random noise per individual frame; biased methods can look smoother but are provably "a bit wrong" in a consistent direction.

**Variance** — how much an estimate wobbles/flickers from one random sample (or one frame) to the next, even if it's unbiased on average. Low variance = stable, smooth-looking result; high variance = visible noise/flicker. Reducing variance without introducing bias is a core goal of all these techniques.

**Convergence** — how quickly an estimate's error shrinks as you add more samples (or more budget). "This method converges faster" means it needs fewer samples to look equally correct.

**MSE (Mean Squared Error)** — a standard way to measure "how wrong" an estimate is on average: take the error at every point, square it (so positive and negative errors don't cancel out, and big errors are punished more than small ones), and average. AHTR's theorem `MSE = O(variance / budget)` just says: *error shrinks proportionally as you throw more ray budget at a region, and shrinks faster in regions that were already low-variance (calmer, more predictable lighting).*

### Reservoir sampling & ReSTIR

**Reservoir** — in this context, a small data structure that holds *one selected candidate* (here, one chosen light) plus some bookkeeping numbers (a running total weight, and a count of how many candidates it has considered so far). Think of it like a "currently winning contestant" slot in a competition where new contestants keep showing up — the reservoir decides, each time a new one arrives, whether to keep the current winner or swap to the new one, weighted by how "good" each contestant is.

**Reservoir sampling** — the *algorithm* for maintaining that reservoir correctly: a classic computer-science technique for picking one (or a few) random, fairly-weighted item(s) out of a stream of items *without knowing in advance how many there will be, and without storing them all.* You see each candidate once, decide immediately whether to keep it or discard it, and never go back.

**Weighted reservoir sampling (WRS)** — the version used here, where candidates with a higher "weight" (importance — brighter, closer, more relevant lights) have a proportionally higher chance of being the one kept in the reservoir.

**RIS (Resampled Importance Sampling)** — the specific technique of: generate several *candidate* samples (e.g., several candidate lights) cheaply, then use weighted reservoir sampling to resample down to just the one that best represents the whole set, as if you'd done the expensive "correct" calculation on all of them. It's a mathematically principled way to turn "many cheap maybes" into "one good enough answer."

**Spatiotemporal** — a combination of two words: **spatial** (across *space* — i.e., sharing information between nearby pixels/tiles on screen right now) and **temporal** (across *time* — i.e., reusing information from the previous frame). "Spatiotemporal reservoir resampling" means: don't throw away last frame's good answer, and don't ignore what your screen-neighbors already figured out — reuse both to avoid redoing all the expensive work from scratch every single frame.

**Temporal reuse / temporal coherence** — the idea that frame N and frame N+1 usually look almost identical, so you can carry forward (reuse) last frame's computed result instead of recomputing from zero, which is much cheaper. The risk: if something *did* change (a light disappeared), a naive reuse can keep showing stale/wrong information — this is exactly the "ghost frame" problem ELCD fixes.

**Spatial reuse** — similarly, nearby pixels/tiles on the *same* frame usually have similar lighting, so you can borrow a neighbor's already-computed candidate instead of generating your own from scratch, then just double-check it's still valid for your exact position (a visibility re-check).

**ReSTIR (REservoir SpatioTemporal Importance Resampling)** — the name of the overall algorithm (Bitterli et al., SIGGRAPH 2020) that combines all of the above: generate candidates (RIS), reuse last frame's result (temporal), reuse neighboring pixels' results (spatial), then fire exactly **one real shadow ray** at the end to confirm and shade. This is what lets 1 ray per pixel look as good as hundreds of rays' worth of brute-force sampling.

**M-cap** — a safety limit on how much "weight"/history a reservoir is allowed to accumulate from temporal reuse, so old information doesn't dominate forever and new information can still get a fair chance to be picked. Standard ReSTIR sets this to a fixed number (e.g. 20) regardless of what's actually happening in the scene — which is the specific design choice that causes the ghost-frame bug this project's ELCD fixes.

**RTXDI** — NVIDIA's own production implementation/extension of ReSTIR (2021), used in real shipped games; it has the same temporal ghosting weakness the FA1 survey identifies.

**Lightcuts / Stochastic Lightcuts / Instant Radiosity** — other, older many-light algorithms the survey compares against. Briefly: *Instant Radiosity* approximates all the light bouncing around with a scattering of small fake point lights (**VPLs**, Virtual Point Lights) which can create ugly bright spots when a VPL is too close to a surface; *Lightcuts* organizes all the lights into a tree and picks a representative "cut" through it per pixel (cheaper than checking every light, but biased and still too slow for hundreds of dynamic lights); *Stochastic Lightcuts* adds randomness to Lightcuts to remove the bias, at the cost of needing more samples and still having no way to reuse results between frames or pixels.

### Ephemeral lights & the ELCD contribution

**Ephemeral** — simply means "short-lived" / "temporary" — something that exists only briefly before disappearing. An **ephemeral light** is a light source with a short, often fixed lifetime: a muzzle flash (a few frames), an explosion (tens of frames), a bullet's glow (a single frame) — as opposed to a lamp that's just always on.

**Ghost frame / ghosting** — when a light visually keeps affecting the scene (showing up as brightness) for several frames *after it has actually disappeared from the game*, because the temporal-reuse system kept feeding its old, stale sample forward instead of noticing it's gone. It looks like a faint afterimage or "ghost" of the light lingering where it shouldn't.

**Confidence (κ, "kappa")** — a single number from 0 to 1 representing "how much should we still trust/use this light's remembered sample." In this project, it's calculated as *remaining lifetime ÷ original lifetime* — a light that just spawned has confidence 1.0 (fully trust it), a light with no time left has confidence 0.0 (don't use it at all).

**Confidence decay** — the idea of gradually (and, at the end, completely) reducing that trust number as a light approaches the end of its life, instead of trusting it fully right up until the instant it vanishes and then abruptly leaving stale data behind.

**ELCD (Ephemeral Light Confidence Decay)** — this project's (and the FA1 survey's) first contribution: multiply a reservoir's carried-forward weight by that confidence number every frame. The practical effect: the moment a light's confidence hits 0 (it has expired), its old sample contributes *zero* to the next frame — the ghost-frame bug is eliminated by construction, not patched around.

### Tiles, hierarchy, and the AHTR contribution

**Tile** — rather than process every single pixel independently, the screen (or in this project, the play area) is divided into a grid of small rectangular regions called tiles, and one lighting decision is computed per tile (shared by everything inside it) instead of per pixel. Much cheaper, at the cost of some precision.

**Luminance** — a measure of how bright a given point/sample appears, used as the stand-in for "how much does the lighting vary here."

**Variance (here)** — how much the lighting *differs* between different candidate samples inside the same tile. A tile where every nearby light would produce roughly the same brightness has low variance (it's "smooth" — any single sample is a good-enough representative). A tile that straddles a shadow's edge — bright on one side, dark on the other — has high variance (one sample badly misrepresents the rest of the tile).

**Hierarchical** — organized in levels, from coarse to fine, where you only "zoom in" to a finer level where it's actually needed. (A full hierarchical structure is typically a **quadtree** — each square can be split into 4 smaller squares, recursively, as many times as needed.)

**Adaptive (sampling/allocation)** — instead of spending the same fixed amount of effort everywhere regardless of need, adjust how much effort you spend *based on what's actually happening in each specific region*. "Adaptive" is the opposite of "fixed" or "uniform."

**AHTR (Adaptive Hierarchical Tile Resampling)** — this project's (and the FA1 survey's) second contribution: measure the variance in each tile, and only subdivide (zoom into finer tiles, spending more ray budget) the tiles that actually have high variance — e.g. right at a shadow's edge — while leaving smooth, uneventful tiles coarse and cheap. This concentrates the limited shadow-ray budget exactly where the eye would actually notice a mistake, instead of spreading it evenly (and wastefully) across the whole screen.

---

## Part 2 — How the Algorithm Actually Works, Step by Step

Think of the whole system as answering one question, over and over, every frame, for every small region of the screen: ***"Out of all the lights that might affect this spot, which single one should I use to light it — and is it actually visible from here?"*** Everything below is the machinery for answering that question *well* and *cheaply*, hundreds of times a second.

### Step 0 — The naive (too slow) approach, for contrast

For every pixel, check every light, fire a shadow ray to each one, add up the result. With 500 lights and millions of pixels, this is billions of shadow rays per frame — far beyond any real-time budget. Everything from here on exists purely to avoid doing this.

### Step 1 — Candidate generation (RIS)

Instead of checking *every* light for a given region, grab a handful of **candidate** lights (the ones physically near/relevant to this region) and run **weighted reservoir sampling**: go through the candidates one at a time, and for each one, with a probability proportional to how "important" it is (bright, close), consider swapping it in as the reservoir's current pick. By the end, the reservoir holds one light, chosen fairly according to importance — as if you'd done a proper weighted lottery across all the candidates, without ever needing to store the whole list at once.

### Step 2 — Temporal reuse (spatiotemporal, part 1)

Before finalizing this frame's pick, fold in *last frame's* reservoir for this same region too — treating it as just one more very-informed "candidate" (since it already represents a good decision from a moment ago). This is the "temporal" half of spatiotemporal: reusing time. Done naively, this is where the ghost-frame bug creeps in, because a light that has since disappeared can still "win" the reservoir lottery on the strength of its stale remembered weight. **This project's ELCD fix slots in exactly here:** before folding in last frame's reservoir, its weight gets multiplied by the referenced light's current confidence (0 to 1). An expired light contributes a multiplier of exactly 0 — it simply can't win anymore, starting the very next frame.

### Step 3 — Spatial reuse (spatiotemporal, part 2) — and this project's AHTR fix

In the original ReSTIR paper, this step borrows candidates from neighboring *pixels* and re-validates them. In this project, the equivalent idea is applied at the *tile* level via AHTR: first check how much the candidates *within* a tile disagree with each other (the "variance" signal). If they're all roughly in agreement, the tile's current coarse answer is good enough — move on. If they disagree a lot (meaning a shadow edge or several competing lights are inside this one tile), split that tile into 4 smaller ones and redo the candidate-selection at that finer resolution, picking the best result. This concentrates extra resolution only where it's actually earned, instead of uniformly everywhere.

### Step 4 — The final shadow ray (shading)

Whatever light the reservoir ends up holding after all the above, fire exactly **one** real shadow ray to it — the one actual expensive visibility test for this whole region, for this whole frame. If that ray says the light is blocked, drop the result (no light from it this frame). If it's clear, use that light (its color/brightness) to light the region.

### Step 5 — Carry forward

Save this frame's final reservoir as "last frame's reservoir" for this region, so Step 2 has something to reuse next frame. The whole cycle then repeats, every frame, for every tile — continuously reusing as much of the previous work as is still valid, and only paying full price (new candidates, a real shadow ray) where reuse genuinely isn't trustworthy anymore.

### Why this is fast *and* (mostly) correct

- **Fast** because the expensive operation (the real shadow ray) only happens once per region per frame, not once per light per region — and temporal/spatial reuse means even *that* one ray's result gets amortized across many frames and neighboring tiles' worth of "effective" sampling.
- **Mostly correct** because the reservoir math (RIS + weighted reservoir sampling) is statistically principled — it doesn't just pick *a* light, it picks one with the *correct probability* to represent the group, so averaged over many frames/tiles the result converges to the same picture brute-force would have produced, just far cheaper, and far more stable than naive random sampling would be.
- **The two remaining weaknesses the original algorithm has** — stale ghosted lights from temporal reuse, and wasted uniform effort when lighting complexity isn't uniform — are exactly what **ELCD** and **AHTR** (this project's contributions) each individually patch, without touching or slowing down the rest of the pipeline.
