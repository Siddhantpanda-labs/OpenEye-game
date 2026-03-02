# Serialization Authority — One Truth

## Canonical path for scene and world persistence

**All persistent world state is `WorldState`. All load/save of that state goes through `WorldSerializer` (or the public `api::world::Serializer`).**

| Use case | API | Notes |
|----------|-----|--------|
| Load scene (runtime player) | `World::LoadScene(path)` → `Serializer::Load(state, path)` | JSON scene file → WorldState |
| Save scene | `Serializer::Save(state, path)` | WorldState → JSON |
| Save game / checkpoint / AI snapshot | Same: serialize `WorldState` (optionally strip or add components) | One model, one serializer |
| Load from buffer (future) | `Serializer::Deserialize(state, data)` or `Load(state, buffer)` | Same WorldState; format can be JSON or binary |

**Rule:** There is one data model (WorldState) and one serialization path for it. No parallel “scene” format for game levels.

---

## Deprecated path (do not use for new code)

The following are **deprecated for loading or saving game scenes/levels**:

- **`api::Scene`** and **`api::SceneManager`** — loading scenes via `SceneManager::LoadScene(path)` uses a different data model (scene::Scene backed by world::World), not WorldState. Use `World::LoadScene(path)` instead.
- **`api::serialize::BinarySerializer`** for Scene — `LoadFromFile(api::Scene, path)` and `SaveToFile(api::Scene, path)` target the deprecated Scene type. For binary persistence of world state, use or add `WorldSerializer` binary support that reads/writes **WorldState**, not Scene.
- **`eyelib::scene::SceneManager`** and **`eyelib::serialize::BinarySerializer`** (Scene) — same: different model, deprecated for scene loading.

These types may remain in the codebase for legacy or editor-only tools. New runtime code and the player use **World + Serializer (WorldState)** only.

---

## Summary

- **Canonical:** `WorldState` + `WorldSerializer` (JSON today; binary later if added to same model).
- **Runtime player:** Loads via `World::LoadScene(path)` only.
- **Deprecated for scenes:** Scene + SceneManager + BinarySerializer(Scene).
