# Neutral In-Memory Representation / Interface

## What it is

A **neutral in-memory representation** is a data structure (or interface) that describes “scene / world state” in a **format-agnostic** way. All load paths (JSON file, binary file, network, etc.) produce this same structure; a **single code path** consumes it and fills `WorldState`.

- **Format-agnostic:** The shape of the data is independent of whether it came from JSON, binary, or anything else.
- **Single fill path:** One function “apply this data to `WorldState`” — no duplicated logic per format.
- **Easier to extend:** New format = new parser that produces the neutral rep; the rest of the engine stays unchanged.

---

## Current state

Today the pipeline is:

```
File (path) → read bytes → parse as JSON → nlohmann::json
nlohmann::json → Deserialize(state, root) → WorldState
```

- **Load(path)** = read file → parse JSON → **Deserialize(state, root)**.
- **Deserialize(state, const nlohmann::json&)** = the only “fill WorldState” step; it reads `root["registry"]`, `root["graph"]`, `root["components"]` and component blocks.

So:

- The **in-memory format** is currently **nlohmann::json**.
- The **fill path** is **Deserialize(state, json)** — single consumer.
- There is **no** “load from buffer” or binary path that shares that consumer; adding binary would mean either a second, parallel “DeserializeFromBinary(state, buffer)” or introducing a neutral layer.

So we are **not** format-agnostic yet: the only producer is the JSON parser, and the consumer is tied to the JSON shape.

---

## What “neutral” would look like

Conceptually:

```
[Producer A: JSON file]   →  NeutralRep  →  ApplyToState(neutral, state)  →  WorldState
[Producer B: Binary file] →      ↑
[Producer C: Network]     →      └── same type, same consumer
```

- **NeutralRep** = one type (or small set of types) that can represent everything needed to reconstruct the world: registry, graph (metadata, hierarchy, tags), and components (per-type, per-entity data).
- **ApplyToState(neutral, state)** = the only place that touches `WorldState` for loading; no format-specific branches inside it.

---

## Options

### 1. Explicit neutral struct (recommended direction)

Define a small hierarchy of structs that mirror what we need for `WorldState`:

- **Registry:** generations, free list (or equivalent).
- **Graph:** metadata (entity → name, flags), hierarchy (child → parent), tags (entity → list of strings).
- **Components:** map of component type name → list of `{ entity_id, data }` where `data` is an opaque blob (e.g. `std::vector<uint8_t>`) or a simple key-value bag.

Then:

- **JSON parser:** read file → parse JSON → build this struct (e.g. decode component JSON into blobs or into a key-value form that the consumer understands).
- **Binary parser:** read file/buffer → decode binary → build the **same** struct.
- **ApplyToState(neutral, state):** clear state; restore registry; restore graph; for each component type, ensure store exists and call existing component deserialization (e.g. from blob or from key-value) into the store.

Pros: One clear, format-agnostic type; one fill path; easy to add new formats.  
Cons: Need to define the struct and wire JSON/binary producers to it; component “data” in the neutral rep might be blob or a minimal key-value form.

### 2. Keep JSON as the “neutral” format

Treat **nlohmann::json** as the neutral in-memory format:

- Add **LoadFromBuffer(state, data)** that: if data looks like JSON, parse → **Deserialize(state, json)**; if data has a binary header, run a binary decoder that **outputs a nlohmann::json** object, then **Deserialize(state, json)**.

So the “neutral” type is still JSON; the single consumer is **Deserialize(state, json)**. Only the producer side gains a binary path (binary → json → same Deserialize).

Pros: No new types; minimal change; one fill path.  
Cons: Binary → JSON can be wasteful; in-memory layout is still JSON-shaped; not a “true” neutral type if you want to avoid JSON dependency in the core fill path later.

### 3. Generic DOM-like tree

Define a small “DOM”: tree of nodes with string keys, values = string | number | array | object (nested nodes). JSON and binary both produce this tree; one **ApplyToState(dom, state)** walks the tree and fills state.

Pros: Single format; flexible.  
Cons: Recreates something JSON-like; either a new dependency or a custom type to maintain; component data still needs a convention (e.g. nested object per component).

### 4. Hybrid: neutral topology + blob per component

Neutral struct holds:

- Registry + graph (explicit structs).
- Components: map of component type → list of `(entity_id, blob)`.

Each parser produces the same topology; component data is “opaque” (binary blob or JSON string per instance). **ApplyToState** restores registry and graph, then for each component uses **ComponentSerializer** to decode the blob into the right store.

Pros: Clear split (topology vs payload); components can be JSON or binary without changing the neutral type.  
Cons: Two concepts (topology vs blobs); need a convention for “what’s in the blob” (e.g. JSON fragment vs binary schema).

---

## Recommendation

- **Short term:** Option 2 is enough to get “load from buffer” and a second format (e.g. binary) without a new type: binary decoder produces `nlohmann::json`, then **Deserialize(state, json)** as today. You stay “one fill path” and gain buffer/binary support.
- **Medium term:** If you want a clean, format-agnostic design and to avoid tying the core to JSON, introduce an **explicit neutral struct** (Option 1 or 4): define a **WorldLoadData** (or similar) with registry, graph, and component blobs; move current **Deserialize** logic into **ApplyToState(WorldLoadData, state)**; add a **WorldSerializer::ParseJson(path)** (or from buffer) that returns **WorldLoadData**; later add **ParseBinary(buffer)** that returns the same type. Then **Load(path)** becomes “read file → parse (JSON or binary) → WorldLoadData → ApplyToState → state”.

---

## Summary

| Concept | Meaning |
|--------|--------|
| **Neutral in-memory rep** | One data shape that describes the world for loading. All parsers produce it; one path consumes it and fills `WorldState`. |
| **Current** | Only producer is JSON; consumer is **Deserialize(state, nlohmann::json)** — not yet format-agnostic. |
| **Minimal step** | Add “load from buffer” and/or binary by having a binary decoder produce **nlohmann::json** and reusing **Deserialize** (option 2). |
| **Clean step** | Introduce an explicit neutral type (e.g. **WorldLoadData**), one **ApplyToState(neutral, state)**, and JSON/binary parsers that both produce that type (option 1/4). |

No code changes in this doc; it’s a design reference. When you’re ready to implement, start with either “buffer + JSON” (option 2) or the explicit neutral type (option 1), depending on how much you want to detach the core from JSON.
