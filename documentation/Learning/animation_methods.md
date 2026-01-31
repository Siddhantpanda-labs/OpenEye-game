# Animation Notes (Idle/Walk/Run + Root Motion)

This doc records the concrete choices used in for the character animation setup.

## 1) State machine + inputs
- **States:** `Idle`, `Walk`, `Run`
- **Transitions:** `Idle↔Walk`, `Idle↔Run`, `Walk↔Run`
- **Input mapping:**  
  - `W/A/S/D` → Walk  
  - `W/A/S/D + Space` → Run  
  - Release all → Idle
- **Smooth facing:**  
  - Target direction = camera forward/right + input (W/S/A/D), normalized  
  - Yaw is smoothed using rotation speed and `deltaTime`

## 2) Root motion extraction
We do **not** use the blended pose to compute root motion (it causes large delta spikes during transitions).
Instead:
- Sample root motion from the **current state clip** at `stateTime`
- Delta = **position difference** (current position minus previous position)
- This prevents drift from hip rotation and avoids transition artifacts

## 3) Root motion application (Option C)
To eliminate turning drift:
- Translation is applied **in world space**, decoupled from yaw
- Movement uses the **input direction** (camera space)
- Root motion “forward step” = `delta.translation.z`

This keeps in‑place rotation stable and removes side drift.

## 4) Scale correction
The mesh is drawn at `0.02` scale.  
Root motion translation is multiplied by the same scale so world movement matches the visible size.

## 5) Pivot (rotation center)
We rotate around a model‑space pivot to prevent orbiting:
- **Pivot source:** AABB center of the mesh (`Mesh::GetAABB().GetCenter()`)
- **Transform order (GLM right‑multiply):**
  - `T(position) * R(yaw) * S(scale) * T(-pivot)`
  - This keeps the pivot fixed at the world position while rotating

## 6) Debug helpers
- **Pivot marker:** A red sphere rendered at the **actual pivot world position**  
- **Skeleton lines:** Bone line debug in world space

