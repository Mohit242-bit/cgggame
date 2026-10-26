# The Logic Behind Gun Mayhem — Short Explanations

This is the "sounds like I actually understand this, not just copy-pasted code" version. No big code dumps — just the ideas, with the one or two numbers/formulas that prove you've actually looked at how it works. Good for explaining out loud, in a viva, or to anyone asking "okay but how does that actually work?"

---

## 1. The camera — parallel/flat, and why the world seems to pan when you walk

**It's parallel.** Nothing shrinks with distance, there's no vanishing point, nothing about how the game looks or plays depends on perspective. (One pure trivia aside, not worth leading with: the engine's default internally uses a matrix technically labelled "perspective," but it's tuned so precisely that it behaves as exactly flat, because nothing in this game ever has depth — functionally that's a non-fact. Call it parallel; that's the correct, useful answer.)

There's also no camera object anywhere in the code — not a flat one, not a 3D one. So why does the world seem to pan when both players walk, say, left? Here's the actual mechanism: as the players drift, the code shifts the **background and platform art in the opposite direction** — to the right. And moving the picture right while the screen stays still looks **exactly identical** to moving a camera left while the picture stays still — there's no way to tell those two apart visually, since only the *relative* motion between content and viewport matters. So the game fakes a camera-follow by sliding the world backward under the players instead of actually moving a viewpoint.

The background and the platform move at *different* fractions of that shift — the platform (where the players themselves stand) moves 70% as much as the players drifted, the background only 50%. Two consequences: first, because the players are children of the platform, the platform's own shift partially cancels their walking, so they only visibly drift about 30% as far as they actually walked — keeping them roughly centered, the "follow" effect. Second, the background lagging further behind than the platform (50% vs 70%) is what creates the parallax depth illusion — the same reason distant hills appear to crawl by slower than the road right next to your car, despite everything here being flat.

---

## 2. The AI — not a learning system, a priority list

The AI is not "smart" in the machine-learning sense — there's no training, no memory of what the opponent did last time, no planning ahead. It's what you'd call a **reactive rule stack**: every single frame (60 times a second), it re-asks itself a short, fixed list of questions, in order, and acts on the first one that applies. Nothing it decides this frame carries over to the next — it starts from scratch every time.

The rules, roughly in priority order:
1. **Is there a crate nearby, and am I still on my starting weapon (or do I have zero active buffs)?** If yes, go get it. There's no scoring or comparison here — the AI can't tell a good weapon from a bad one, it only knows "default" versus "not default." Once it's picked up literally anything, it mostly stops chasing crates, even a strictly worse one than what's still sitting there.
2. **Otherwise, is the opponent on my level?** If yes, walk toward them until within a comfortable range, then hold position — it doesn't walk into melee range, it keeps a standoff distance.
3. **Is the opponent on a different level?** Climb or drop toward their row.
4. A handful of safety reflexes run alongside all of this regardless: don't walk off a ledge mid-air without trying to recover, cancel out if knockback has pushed speed past the normal limit, and jump reflexively if the opponent is on the same level and currently shooting.

Separately, and completely independently, there's a one-line rule for aiming: if the opponent is roughly at the same height (within a fixed vertical window), face them and hold the fire button; otherwise don't. That's the entire "aim" system — no leading shots, no predicting movement, no skill curve. Accuracy comes purely from the gun's own spread, not from the AI being more or less careful.

The interesting part: none of these rules know about each other. The AI never explicitly codes "don't shoot while jumping" — that behavior just *emerges*, because the aiming rule and the movement rule are both independently checking the same positions, and a mid-jump height difference naturally fails the aiming rule's vertical window. Simple independent rules, combined, can look like coordinated behavior without anyone having coded the coordination directly.

**One important correction to how this gets described:** the AI does **not** predict where the opponent is going. It never looks at their trajectory and extrapolates forward. Every frame it just reads `opponent->floor` — a number the opponent's own movement code already calculated *for itself* the instant it last landed — with zero lookahead. "Which platform are they heading to" isn't forecast; it's just "which platform are they on, this exact frame."

**A quick word-collision worth untangling:** "vector" means two different things in this code, and they're easy to mix up. A player's or crate's *position* is a **Vec2** — the simple math sense, just an (x, y) pair. The platform layout and the list of active crates are stored in a **`std::vector`** — the C++ sense, which just means "a resizable list," nothing to do with direction at all. Positions are math vectors; the platform geometry and the crate list are just lists of things, read directly every frame — not predicted, not simulated ahead.

---

## 3. Jumping — it's just gravity, applied every frame

A jump isn't an animation or a scripted arc — it's real (simplified) physics, recalculated every single frame:

- **The jump itself is an instant velocity kick** — the moment you press jump, the character's upward speed is set to a fixed maximum value immediately. Think of it like a short, sharp push, not a gradual ramp-up.
- **From that point on, gravity (a constant downward acceleration) is subtracted from that speed every frame.** This is the same idea as the classic physics formula *v = u + at* — velocity equals the starting velocity plus acceleration times time — just applied in tiny time-slices (one per frame, "Euler integration" if you want the term for it) instead of as one clean equation. Upward speed shrinks a little every frame, eventually goes negative, and the character starts falling — all from one constant being subtracted repeatedly, no separate "falling" code needed.
- **Position is then moved by that speed, every frame too** — same idea again: *position = position + speed × time*. Speed changes the position, gravity changes the speed. Two simple update rules, run 60 times a second, is all a jump arc actually is.
- **There's a speed cap ("terminal velocity")** — falling speed is clamped so it can never exceed roughly 2.4 times the normal jump speed, so a character who's been falling for a long time doesn't pick up unrealistic, ever-increasing speed.
- **Double jump exists and is deliberately weaker** — the second jump's impulse is the normal jump speed divided by 1.2, so the second jump is noticeably shorter than the first, which is a small, deliberate balance choice (not a rounding accident).
- **A thrown bomb uses this exact same idea**, just without the "press jump again" part — it gets one sideways-and-upward velocity kick when thrown, and from then on the same "gravity subtracts from vertical speed every frame" rule curves it back down into a real, physically-reasonable arc, until a timer runs out and it explodes.

---

## 4. Platform collision — numbers, not pixels

Here's a detail that surprises people: the game does **not** look at the picture to figure out where the solid ground is. The background and platform art is just a picture — as far as the computer's concerned, it's not "aware" of its own shape at all.

Instead, every map separately stores a short list of plain numbers — "there is solid ground between X-coordinate 0 and X-coordinate 1945, at this height." Someone manually typed those numbers to roughly match where the platform art appears to be. Checking "am I standing on something" is then just a number comparison — *is my X position between this start number and this end number* — nothing to do with pixels at all. It's cheap, and it's also why, if an artist ever moved the platform picture without updating those numbers, the character would appear to float in mid-air or sink through solid-looking ground — the two things are related only because a human lined them up by eye, not because the game derives one from the other.

---

## 5. Combat — knockback, not hit points, and death is a position, not a counter

There's no health bar counting down to zero. Getting shot does exactly one thing physically: it adds a push (a velocity kick) away from where the shot came from. A character "dies" only when they've been pushed far enough, by however many hits, that they fall below a fixed line near the bottom of the map — death is literally a position check (*is my height below this threshold*), not a damage counter hitting zero. A single lucky shot near a ledge can be just as deadly as many hits in the open, because it's really about position, not accumulated damage.

Respawn afterward picks a fresh random spot on the top row of the platform stack, drops the character in from high above (while invisible, so there's no visible "falling from the sky" moment — it just reappears once it lands), and grants a few seconds of a defensive bubble that cuts incoming knockback to a tenth of normal, so you can't be re-killed the instant you reappear.

---

## 6. The dynamic lighting system — in one paragraph, no formulas

Every time a weapon fires or a bomb explodes, a short-lived "light" is registered — not just a visual sprite, but a real data object with a position, a color, and a countdown timer. Every frame, the screen is checked in small chunks (tiles), and for each chunk, whichever nearby light is most likely to actually matter gets picked (biased toward brighter/closer lights, using a technique called weighted random sampling, rather than checking every single light against every single chunk, which would be far too slow). That pick is partly carried over from the previous frame to avoid recomputing everything from zero — but its influence is scaled down in proportion to how much of its lifetime is left, so that the instant a light's timer actually hits zero, it stops affecting anything immediately instead of lingering as an afterimage. Chunks where several lights are competing get a one-level-finer re-check; calm, uneventful chunks don't. The result: a handful of genuinely "smart" lighting decisions per frame instead of brute-force checking everything against everything.

---

## 7. Why none of this needed machine learning or a physics engine

Worth saying out loud, because it's a legitimate design point: this whole game — jumping, falling, platform collision, AI decision-making, knockback — runs on plain arithmetic, re-derived from simple rules every frame. No physics engine library, no ML model, nothing learned from data. That's not a limitation so much as a reflection of what a 2D platformer actually needs: a handful of consistent, cheap, per-frame rules produce everything you see, and understanding *why* something happens on screen is always just a matter of reading a short, specific piece of arithmetic — not untangling a black box.
