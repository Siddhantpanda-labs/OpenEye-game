Cursor’s diagnosis is **mostly correct**.
Your engine currently mixes **two coordinate spaces**:

* Rendering → **world transforms**
* Editor/API → **local transforms**

That guarantees inconsistencies for parented objects.

Let’s cleanly define the **professional rule** and the **correct fix**.

---

# 1. The Core Principle (Professional Engines)

Every transform system has **two representations**:

### Local transform

Transform relative to parent.

```
localPosition
localRotation
localScale
```

### World transform

Actual transform in the scene.

```
worldPosition
worldRotation
worldScale
```

They must always satisfy:

```
world = parentWorld ⊗ local
```

If the entity has **no parent**:

```
world = local
```

---

# 2. What Is Currently Happening In Your Engine

Your engine stores:

```
local TRS
```

But rendering uses:

```
worldMatrix cache
```

However:

```
World::GetTransform()
```

returns **local**.

So when B moves:

```
B world changes
A world changes (correct visually)
```

But:

```
GetTransform(A) → still returns local
```

So your inspector/gizmo reads **the wrong transform space**.

That’s the mismatch.

---

# 3. What a Professional Engine Does

Engines like:

* Unity
* Unreal Engine

expose **both transforms**.

Example API:

```
GetLocalTransform(entity)
GetWorldTransform(entity)
```

Editors choose which one they want.

---

# 4. The Correct Fix (Architecture)

You must treat **local and world as different data**.

### TransformLocal

```
position
rotation
scale
```

### TransformWorld (cached)

```
position
rotation
scale
```

The transform system computes world each frame.

---

# 5. Transform Update Pass

```
for entity in hierarchyOrder:

    if parent == null
        world = local
    else
        world = Compose(parent.world, local)
```

Then store:

```
worldPosition
worldRotation
worldScale
```

Rendering uses this.

---

# 6. API Design (Correct)

Your world API should look like this:

```
GetLocalTransform(entity)
GetWorldTransform(entity)
SetLocalTransform(entity)
SetWorldTransform(entity)
```

Important:

```
SetWorldTransform
```

must convert world → local.

---

# 7. How SetWorldTransform Works

If the entity has parent **B**:

```
local = inverse(parentWorld) ⊗ world
```

Then store that as local.

This keeps hierarchy consistent.

---

# 8. Gizmo Rule (Very Important)

The editor gizmo should operate in **world space**.

Steps:

### Step 1

Get world transform

```
world = GetWorldTransform(entity)
```

### Step 2

Feed that to ImGuizmo.

### Step 3

User edits → returns new world matrix.

### Step 4

Convert world → local

```
local = inverse(parentWorld) ⊗ newWorld
```

### Step 5

Call

```
SetLocalTransform(entity, local)
```

---

# 9. Why Your Current System Breaks

Right now the flow is:

```
Gizmo reads LOCAL
Rendering uses WORLD
```

For root objects:

```
local == world
```

So you never noticed.

For children:

```
local != world
```

So gizmo appears wrong.

---

# 10. What Should Always Be True

This invariant must always hold:

```
world = parentWorld ⊗ local
```

If that equation ever breaks, the system becomes inconsistent.

---

# 11. Correct Editor Behaviour

When parenting:

```
Reparent(child, parent, preserveWorld = true)
```

Algorithm:

```
childWorld = world(child)
parentWorld = world(parent)

newLocal = inverse(parentWorld) ⊗ childWorld
```

Then assign:

```
local(child) = newLocal
parent(child) = parent
```

So the object **does not visually move**.

---

# 12. Final Architecture You Should Implement

### TransformStore

```
localPosition[]
localRotation[]
localScale[]

worldPosition[]
worldRotation[]
worldScale[]

parent[]
children[]

dirty[]
```

---

### Systems

```
HierarchySystem
TransformSystem
```

---

### TransformSystem

```
UpdateWorldTransforms()
```

---

### API

```
GetLocalTransform()
GetWorldTransform()

SetLocalTransform()
SetWorldTransform()
```

---

# 13. Final Answer To Your Problem

Your rendering is correct.

Your editor and API are **reading the wrong transform space**.

Fix:

```
Gizmo → world
Inspector → show world + local
SetTransform → convert world to local
```

Once you do this, the mismatch disappears.

---

These are the **four transform mistakes that almost every custom engine makes**.
If any one of them exists, you get exactly the problems you described: **jumping children, incorrect gizmos, scale explosions, and inconsistent world transforms**.

I’ll explain them in the order professionals debug them.

---

# 1. Mixing Local and World Transforms

This is the **most common bug**.

Your system currently does something like:

```
Rendering → world
API/Gizmo → local
```

For a root object this works because:

```
local == world
```

But for a child:

```
world = parentWorld ⊗ local
```

So they diverge.

### Symptom

* Gizmo appears in wrong place
* Inspector values don’t match visual position
* Child moves incorrectly when edited

### Correct rule

You must clearly separate:

```
GetLocalTransform()
GetWorldTransform()
```

and the editor must choose which to use.

Professional engines always expose **both**.

---

# 2. Not Converting World → Local When Editing

This is the second most common bug.

If the editor manipulates **world space**, but you write the result directly into **local**, the hierarchy breaks.

Example:

```
Child world position = 10
Parent world position = 5
```

Correct local should be:

```
local = 5
```

But if you store world directly:

```
local = 10
```

Then next frame:

```
world = parentWorld + local
world = 15
```

The object jumps.

### Correct algorithm

When editing world transform:

```
local = inverse(parentWorld) ⊗ newWorld
```

Then store local.

This single step fixes many parenting bugs.

---

# 3. Wrong Hierarchy Update Order

Transforms must always update:

```
parent → child
```

Never the reverse.

If the order is random (which happens easily in ECS), children can use **old parent transforms**.

### Symptom

* child jitter
* incorrect world positions
* transforms fixing themselves next frame

### Correct solution

Maintain a **flattened hierarchy order**:

Example:

```
A
 ├ B
 │ └ D
 └ C
```

Flattened:

```
[A, B, D, C]
```

Then update transforms in that order.

This guarantees parent world is valid before computing children.

---

# 4. Using Matrix Decomposition for TRS

This one causes **the worst crashes**, especially with scaling.

Many engines do this:

```
newLocalMatrix = inverse(parentWorldMatrix) * childWorldMatrix

glm::decompose(newLocalMatrix)
```

Matrix decomposition is **numerically unstable** when:

* non-uniform scale
* parent rotation + scale
* deep hierarchies

It produces:

```
NaN rotations
invalid quaternions
skew
exploding scale
```

Exactly the type of crash you mentioned.

### Professional solution

Store TRS and compute transforms using **TRS math**, not matrices.

Instead of:

```
Mat4 * Mat4
```

use:

```
TRS ⊗ TRS
```

like the transform math used inside **Unreal Engine**.

Matrices should be generated only when rendering.

---

# The One Rule That Prevents All Transform Bugs

Professional engines enforce this invariant:

```
worldTransform = parentWorld ⊗ localTransform
```

Always.

Local is the **authoritative data**.
World is **derived**.

Never let them drift apart.

---

# What Your Engine Should Guarantee

After every transform update:

```
local → stored
world → recomputed
matrix → derived for rendering
```

Editor operations must follow:

```
gizmo edits world
engine converts world → local
store local
recompute world
```

Once that pipeline is strict, the hierarchy becomes stable.

---

✅ If these four areas are correct, your transform system becomes **Unity/Unreal level reliable**.

---
