What you are describing is the classic family of **shadow map instability + aliasing artifacts**.
The symptoms:

* Shadow edges “crawl”, “shimmer”, or “jitter” when the camera moves.
* Edges become sharper/blurry depending on movement.
* Shadows appear to vibrate when rotating the camera.
* Small camera motion causes large texel changes in the shadow map.
* PCF filtering looks unstable during motion.

This is not a single bug. It is an entire category of problems caused by:

---

# The Core Problem

A shadow map is fundamentally a **discrete texture representation** of a continuous world.

The instability happens because:

```text
World Space → Light Space → Shadow Texels
```

is not stable under camera motion.

Tiny changes in camera/light matrices cause geometry to project onto different shadow texels every frame.

That produces:

* temporal instability
* subpixel swimming
* edge flickering
* inconsistent filtering
* perspective aliasing

The root issue is:

> The shadow projection is not temporally stable relative to texel space.

---

# Why This Happens

Suppose your directional light shadow map is:

```text
4096 x 4096
```

The orthographic projection for the light changes every frame based on camera position.

Even a movement like:

```text
camera += 0.01
```

can shift projected coordinates by:

```text
0.3 texels
```

Now every shadow sample lands between different texels.

Then PCF filtering changes dynamically.

Result:

* edges flicker
* penumbra changes shape
* shadows crawl

This is especially severe for:

* cascaded shadow maps
* directional lights
* large outdoor scenes

---

# The Robust Solution Class

The industry-standard solution is:

# Texel-Stable Shadow Mapping

This is the foundation.

Without this, all filtering improvements still shimmer.

---

# The Real Fix

## 1. Snap the Light Projection to Shadow Texel Grid

This is THE major solution.

Instead of allowing arbitrary floating-point movement of the light projection:

```text
BAD:
lightProjection moves continuously
```

you quantize it:

```text
GOOD:
lightProjection moves in texel-sized increments
```

This prevents sub-texel movement.

---

# How It Works

Suppose your shadow frustum width is:

```text
200 world units
```

and shadow map resolution is:

```text
4096
```

Then one texel equals:

\text{TexelWorldSize} = \frac{200}{4096}

Now you snap the light-space origin:

```cpp
origin.x = floor(origin.x / texelWorldSize) * texelWorldSize;
origin.y = floor(origin.y / texelWorldSize) * texelWorldSize;
```

Now camera movement smaller than one texel does NOT change shadow sampling.

Result:

* stable edges
* no crawling
* no jitter
* consistent filtering

This is the single most important fix.

---

# 2. Stable Cascaded Shadow Maps (Very Important)

If using CSM:

A huge source of shimmering is:

```text
cascade bounds changing every frame
```

because frustum fitting changes dynamically.

---

## Bad Approach

“Tightly fit” cascade to camera frustum every frame.

This maximizes resolution BUT causes instability.

---

## Correct Approach

Use:

# Stable Cascades

Meaning:

* fixed-size cascade volumes
* texel snapping
* avoid rotating bounds aggressively
* keep orthographic extents constant

This sacrifices a bit of resolution for massive temporal stability.

Most AAA engines do this.

---

# 3. Use Proper Shadow Filtering

Once stability is solved:

Use:

* PCF
* PCSS
* EVSM
* MSM

But filtering alone cannot solve instability.

Filtering only hides aliasing.

It does NOT solve temporal movement.

---

# 4. Use Receiver Plane Depth Bias

If shadows “shake” differently at grazing angles:

You may also have unstable biasing.

Use:

# Receiver Plane Depth Bias

instead of constant bias.

Because constant bias changes visually depending on angle and motion.

This reduces:

* acne
* peter-panning
* edge instability

---

# 5. Use TAA-Aware Shadowing

Modern engines stabilize shadows further using:

# Temporal Anti-Aliasing Integration

The idea:

* accumulate shadow history
* reproject previous frame
* temporally filter penumbra

This significantly reduces shimmer.

Used heavily in:

* Unreal Engine
* Frostbite
* RE Engine
* id Tech

But:

TAA is not the foundational fix.

Texel stability is.

---

# 6. Increase Effective Resolution Intelligently

Shadow instability is amplified when texel density is poor.

Use:

* cascaded shadow maps
* logarithmic splits
* perspective warping
* clipmaps
* virtual shadow maps

instead of just increasing resolution blindly.

---

# The Fundamental Engineering Principle

The correct mental model is:

> Shadow maps must be stable in texel space, not merely visually correct in world space.

This is the core insight.

---

# Modern Robust Pipeline

A high-quality modern directional shadow system typically uses:

```text
Stable Cascaded Shadow Maps
+ Texel Snapping
+ PCF/PCSS
+ Receiver Plane Bias
+ TAA Stabilization
+ Contact Shadows
```

That combination removes nearly all visible shimmer.

---

# What Most Likely Fixes Your Exact Issue

Based on your description:

> “camera moves → edges fade/sharpen/jibble”

the main culprit is almost certainly:

# Non-texel-aligned light projection

especially if:

* directional light
* orthographic shadow projection
* cascades follow camera tightly

---

# Minimal Practical Fix

Implement these first:

## MUST HAVE

### A. Texel snapping

Snap orthographic projection origin to texel size.

---

### B. Stable cascade extents

Do NOT resize cascades every frame tightly.

Keep extents stable.

---

### C. Fixed shadow filtering radius

Avoid dynamic kernel variation tied to camera movement.

---

# Advanced Solutions

If you want state-of-the-art:

## Virtual Shadow Maps

Used in [Unreal Engine 5](https://www.unrealengine.com/en-US/unreal-engine-5?utm_source=chatgpt.com)

They solve many classical issues through:

* virtualized pages
* extremely high local resolution
* temporal caching
* stable sampling

But they are architecturally much more complex.

---

# Important Distinction

There are TWO different artifacts often confused:

| Artifact                        | Cause                            |
| ------------------------------- | -------------------------------- |
| Shadow acne                     | depth precision / bias           |
| Shadow shimmer                  | unstable texel projection        |
| Shadow aliasing                 | insufficient resolution          |
| Softness changing during motion | unstable filtering / texel crawl |
| Edge jitter                     | subtexel instability             |

Your issue is primarily:

# Temporal shadow instability

not just aliasing.

---

# One-Sentence Summary

The robust solution is:

> Make the light-space shadow projection temporally stable by snapping it to shadow texel space and using stable cascaded shadow maps, then layer filtering and TAA on top.
