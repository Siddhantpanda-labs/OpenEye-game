# Scripting System Integration Plan
## Industry-Grade Lua Scripting for AI-First Simulation World

---

## 1. Executive Summary

This plan outlines the integration of a **production-grade Lua scripting system** into the OpenEye engine, designed to work alongside (not replace) the World Command API (WCA) for AI control.

### Key Principles

- **Lua is secondary to WCA**: AI uses World Command API; Lua is for human-authored gameplay logic
- **Sandboxed & secure**: Scripts cannot break the world kernel
- **Hot-reloadable**: Edit scripts while the game runs
- **ECS-integrated**: Scripts attach to entities as components
- **Industry-standard**: Follows patterns from Unity, Unreal, Roblox

---

## 2. Architecture Overview

### Control Hierarchy (Critical)

```
┌─────────────────────────────────────────┐
│         AI Model (Autonomous)           │
└──────────────┬──────────────────────────┘
               │
               ▼
┌─────────────────────────────────────────┐
│    World Command API (WCA) - PRIMARY   │  ← AI uses this
│  • CreateEntity, SetRule, etc.          │
└──────────────┬──────────────────────────┘
               │
               ▼
┌─────────────────────────────────────────┐
│         World Kernel (Deterministic)    │
└──────────────┬──────────────────────────┘
               │
               ▼
┌─────────────────────────────────────────┐
│      Lua Scripting (SECONDARY)          │  ← Humans use this
│  • NPC behaviors, weapon logic, etc.    │
└──────────────┬──────────────────────────┘
               │
               ▼
┌─────────────────────────────────────────┐
│      Engine Systems (Rendering, etc.)   │
└─────────────────────────────────────────┘
```

### What Lua IS Used For

✅ **Good Use Cases:**
- NPC local behaviors (patrol, chase, attack)
- Weapon firing logic and damage calculation
- Item effects (health potion, speed boost)
- Trigger volumes (doors, checkpoints)
- Animation state machine glue
- UI event handlers
- Game mode rules (scoring, timers)
- Designer-authored gameplay tweaks

❌ **What Lua is NOT Used For:**
- World-wide system composition
- Creating entire game systems (use WCA)
- Physics simulation control
- Rendering pipeline control
- Authority/security decisions
- AI's primary control interface

---

## 3. System Components

### 3.1 Core Scripting Engine

#### 3.1.1 Lua VM Manager (`eyelib/scripting/LuaVM.h/cpp`)

**Purpose**: Manages Lua virtual machines, sandboxing, and lifecycle

**Key Features:**
- Multiple isolated Lua states (one per script or shared)
- Sandbox configuration (allowed/disallowed functions)
- Memory limits and quotas
- Error handling and recovery
- Thread-safe script execution (optional)

**API:**
```cpp
class LuaVM {
public:
    static std::shared_ptr<LuaVM> Create();
    ~LuaVM();
    
    // State management
    lua_State* GetState() const;
    bool IsValid() const;
    
    // Sandboxing
    void SetSandboxMode(SandboxMode mode);
    void AllowFunction(const std::string& funcName);
    void DisallowFunction(const std::string& funcName);
    
    // Memory limits
    void SetMemoryLimit(size_t bytes);
    size_t GetMemoryUsage() const;
    
    // Error handling
    bool ExecuteString(const std::string& code, std::string& error);
    bool ExecuteFile(const std::string& path, std::string& error);
    
    // Callbacks
    void SetErrorCallback(std::function<void(const std::string&)> callback);
};
```

#### 3.1.2 Script Manager (`eyelib/scripting/ScriptManager.h/cpp`)

**Purpose**: Loads, caches, and manages script lifecycle

**Key Features:**
- Script file loading and caching
- Hot-reload detection (file watching)
- Script dependency tracking
- Error reporting and logging
- Script metadata (author, version, dependencies)

**API:**
```cpp
class ScriptManager {
public:
    static ScriptManager& Get();
    
    // Script loading
    ScriptHandle LoadScript(const std::string& path);
    void UnloadScript(ScriptHandle handle);
    bool ReloadScript(ScriptHandle handle);
    
    // Hot-reloading
    void EnableHotReload(bool enabled);
    void CheckForChanges(); // Call per frame
    void SetReloadCallback(ScriptHandle handle, std::function<void()> callback);
    
    // Script info
    std::string GetScriptPath(ScriptHandle handle) const;
    std::string GetScriptError(ScriptHandle handle) const;
    bool IsScriptValid(ScriptHandle handle) const;
    
    // Dependency tracking
    std::vector<ScriptHandle> GetDependencies(ScriptHandle handle) const;
};
```

#### 3.1.3 Script Component (`eyelib/ecs/components/ScriptComponent.h`)

**Purpose**: ECS component that attaches scripts to entities

**Structure:**
```cpp
struct ScriptComponent {
    ScriptHandle scriptHandle;
    std::string scriptName;        // "NPCBehavior", "WeaponController", etc.
    bool isEnabled = true;
    bool needsReload = false;
    
    // Script instance data (Lua table)
    // This is serialized and passed to Lua
    std::unordered_map<std::string, ScriptValue> properties;
    
    // Runtime state
    lua_State* luaState = nullptr;  // Per-instance Lua state (optional)
    bool isInitialized = false;
    float lastUpdateTime = 0.0f;
};
```

**Script Value Type:**
```cpp
struct ScriptValue {
    enum Type { Nil, Bool, Number, String, Vec3, Entity };
    Type type;
    union {
        bool boolValue;
        double numberValue;
        std::string stringValue;
        math::Vec3 vec3Value;
        Entity entityValue;
    };
};
```

---

### 3.2 API Bindings (C++ → Lua)

#### 3.2.1 Engine API Bindings (`eyelib/scripting/bindings/`)

**Purpose**: Expose safe engine functions to Lua

**Modules:**

1. **Entity API** (`EntityBindings.h/cpp`)
   ```lua
   -- Lua usage
   local entity = Entity.Create("NPC")
   Entity.AddComponent(entity, "Transform")
   Entity.SetPosition(entity, 10, 5, 0)
   local pos = Entity.GetPosition(entity)
   ```

2. **Component API** (`ComponentBindings.h/cpp`)
   ```lua
   -- Lua usage
   local health = Component.GetHealth(entity)
   Component.SetHealth(entity, 100)
   Component.Damage(entity, 10)
   ```

3. **Transform API** (`TransformBindings.h/cpp`)
   ```lua
   -- Lua usage
   Transform.SetPosition(entity, x, y, z)
   Transform.SetRotation(entity, x, y, z, w)
   local pos = Transform.GetPosition(entity)
   ```

4. **Physics API** (`PhysicsBindings.h/cpp`)
   ```lua
   -- Lua usage
   Physics.ApplyForce(entity, fx, fy, fz)
   Physics.SetVelocity(entity, vx, vy, vz)
   local hit = Physics.Raycast(x1, y1, z1, x2, y2, z2)
   ```

5. **Audio API** (`AudioBindings.h/cpp`)
   ```lua
   -- Lua usage
   Audio.PlaySound(entity, "explosion.wav")
   Audio.SetVolume(entity, 0.5)
   ```

6. **Input API** (`InputBindings.h/cpp`)
   ```lua
   -- Lua usage
   if Input.IsKeyPressed("Space") then
       -- jump
   end
   local mouseX, mouseY = Input.GetMousePosition()
   ```

7. **Time API** (`TimeBindings.h/cpp`)
   ```lua
   -- Lua usage
   local dt = Time.DeltaTime()
   local time = Time.Time()
   ```

8. **Math API** (`MathBindings.h/cpp`)
   ```lua
   -- Lua usage
   local dist = Math.Distance(x1, y1, z1, x2, y2, z2)
   local normalized = Math.Normalize(x, y, z)
   ```

9. **Event API** (`EventBindings.h/cpp`)
   ```lua
   -- Lua usage
   Event.Subscribe("OnCollision", function(entity, other)
       -- handle collision
   end)
   Event.Emit("PlayerDied", playerEntity)
   ```

10. **Query API** (`QueryBindings.h/cpp`)
    ```lua
    -- Lua usage
    local entities = Query.FindNearby(x, y, z, radius)
    local entities = Query.FindByComponent("Health")
    ```

**Binding Helper Macros:**
```cpp
// eyelib/scripting/bindings/BindingHelpers.h
#define LUA_BIND_FUNCTION(L, name, func) \
    lua_register(L, name, func)

#define LUA_BIND_CLASS(L, className, methods) \
    // Register class with methods

#define LUA_BIND_VECTOR3(L) \
    // Register Vec3 type
```

---

### 3.3 Script Execution System

#### 3.3.1 Script System (`eyelib/ecs/systems/ScriptSystem.h/cpp`)

**Purpose**: ECS system that executes scripts attached to entities

**Execution Flow:**
```
1. Query entities with ScriptComponent
2. For each script:
   a. Check if needs reload → reload
   b. If not initialized → call OnStart()
   c. Call OnUpdate(deltaTime)
   d. Handle events (OnCollision, etc.)
3. Cleanup destroyed entities
```

**API:**
```cpp
class ScriptSystem : public ISystem {
public:
    void Update(World& world, float deltaTime) override;
    
    // Script lifecycle
    void InitializeScript(Entity entity, ScriptComponent& script);
    void UpdateScript(Entity entity, ScriptComponent& script, float deltaTime);
    void ShutdownScript(Entity entity, ScriptComponent& script);
    
    // Event handling
    void OnCollision(Entity entity, Entity other);
    void OnTriggerEnter(Entity entity, Entity other);
    void OnTriggerExit(Entity entity, Entity other);
    
    // Hot-reload
    void ReloadScript(Entity entity);
};
```

**Lua Script Interface:**
```lua
-- Standard script interface
local MyScript = {}

function MyScript:OnStart()
    -- Called once when script is attached
    self.health = 100
    self.speed = 5.0
end

function MyScript:OnUpdate(deltaTime)
    -- Called every frame
    self:Move(deltaTime)
    self:CheckForPlayers()
end

function MyScript:OnCollision(other)
    -- Called on collision
    if Component.HasComponent(other, "Player") then
        self:Attack(other)
    end
end

function MyScript:Move(dt)
    local pos = Transform.GetPosition(self.entity)
    pos.x = pos.x + self.speed * dt
    Transform.SetPosition(self.entity, pos.x, pos.y, pos.z)
end

return MyScript
```

---

### 3.4 Hot-Reloading System

#### 3.4.1 File Watcher (`eyelib/scripting/HotReloader.h/cpp`)

**Purpose**: Monitors script files for changes and triggers reloads

**Implementation:**
- Platform-specific file watching (Windows: ReadDirectoryChangesW, Linux: inotify)
- Change detection (modification time, file hash)
- Safe reload (wait for current execution to finish)
- Dependency tracking (reload dependent scripts)

**API:**
```cpp
class HotReloader {
public:
    void WatchScript(const std::string& path);
    void UnwatchScript(const std::string& path);
    
    void Update(); // Call per frame
    std::vector<std::string> GetChangedScripts() const;
    
    void SetReloadCallback(std::function<void(const std::string&)> callback);
};
```

---

### 3.5 Sandboxing & Security

#### 3.5.1 Sandbox Configuration (`eyelib/scripting/Sandbox.h/cpp`)

**Purpose**: Restrict what scripts can do

**Default Sandbox (Restrictive):**
- ❌ No `os.execute`, `io.popen` (file system access)
- ❌ No `require` (module loading) - use provided APIs
- ❌ No `debug` library
- ❌ No `package` library
- ✅ Only engine-provided APIs
- ✅ Math, string, table libraries (standard Lua)

**API:**
```cpp
enum class SandboxMode {
    Restricted,  // Default: only engine APIs
    Standard,    // Standard Lua libraries
    Permissive   // Almost everything (dev only)
};

class Sandbox {
public:
    static void ConfigureVM(lua_State* L, SandboxMode mode);
    static void RemoveFunction(lua_State* L, const std::string& name);
    static void AddCustomFunction(lua_State* L, const std::string& name, lua_CFunction func);
};
```

---

### 3.6 Debugging & Profiling

#### 3.6.1 Script Debugger (`eyelib/scripting/debug/ScriptDebugger.h/cpp`)

**Purpose**: Debug Lua scripts (breakpoints, step, inspect variables)

**Features:**
- Breakpoint support
- Step over/into/out
- Variable inspection
- Call stack viewing
- Error stack traces

**API:**
```cpp
class ScriptDebugger {
public:
    void SetBreakpoint(const std::string& script, int line);
    void RemoveBreakpoint(const std::string& script, int line);
    
    void StepOver();
    void StepInto();
    void StepOut();
    void Continue();
    
    ScriptValue GetVariable(const std::string& name) const;
    std::vector<std::string> GetCallStack() const;
};
```

#### 3.6.2 Script Profiler (`eyelib/scripting/debug/ScriptProfiler.h/cpp`)

**Purpose**: Profile script execution time

**API:**
```cpp
class ScriptProfiler {
public:
    void StartProfiling();
    void StopProfiling();
    
    float GetScriptTime(const std::string& script) const;
    std::vector<ProfileEntry> GetProfileReport() const;
};
```

---

## 4. Implementation Phases

### Phase 1: Core Infrastructure (Week 1-2)

**Goal**: Basic Lua integration and script loading

**Tasks:**
1. ✅ Integrate Lua (LuaJIT or standard Lua 5.4)
2. ✅ Implement `LuaVM` (basic VM management)
3. ✅ Implement `ScriptManager` (load/unload scripts)
4. ✅ Create `ScriptComponent` (ECS integration)
5. ✅ Implement `ScriptSystem` (basic execution)
6. ✅ Basic API bindings (Entity, Transform, Time)

**Deliverable**: Can load and execute a simple Lua script attached to an entity

---

### Phase 2: API Bindings (Week 3-4)

**Goal**: Expose comprehensive engine APIs to Lua

**Tasks:**
1. ✅ Complete Entity API bindings
2. ✅ Component API bindings
3. ✅ Physics API bindings
4. ✅ Audio API bindings
5. ✅ Input API bindings
6. ✅ Math API bindings
7. ✅ Event API bindings
8. ✅ Query API bindings

**Deliverable**: Scripts can fully interact with the engine

---

### Phase 3: Hot-Reloading (Week 5)

**Goal**: Edit scripts while game runs

**Tasks:**
1. ✅ Implement file watcher
2. ✅ Safe script reloading
3. ✅ State preservation (save/restore script variables)
4. ✅ Dependency tracking

**Deliverable**: Change script file → see changes immediately

---

### Phase 4: Sandboxing & Security (Week 6)

**Goal**: Prevent scripts from breaking the world

**Tasks:**
1. ✅ Implement sandbox configuration
2. ✅ Remove dangerous Lua functions
3. ✅ Memory limits and quotas
4. ✅ Error handling and recovery
5. ✅ Script validation

**Deliverable**: Scripts cannot crash the engine or access unsafe operations

---

### Phase 5: Debugging Tools (Week 7-8)

**Goal**: Professional debugging experience

**Tasks:**
1. ✅ Script debugger (breakpoints, step)
2. ✅ Variable inspection
3. ✅ Call stack viewing
4. ✅ Error stack traces
5. ✅ Script profiler

**Deliverable**: Full debugging support for Lua scripts

---

### Phase 6: Advanced Features (Week 9-10)

**Goal**: Production-ready features

**Tasks:**
1. ✅ Script coroutines (async operations)
2. ✅ Script modules (shared code)
3. ✅ Script serialization (save/load state)
4. ✅ Multi-threaded script execution (optional)
5. ✅ Script performance optimization

**Deliverable**: Production-grade scripting system

---

## 5. File Structure

**Library Implementation** (`eyelib/scripting/`):
```
eyelib/
├── scripting/
│   ├── CMakeLists.txt                 # Build configuration
│   ├── LuaVM.h/cpp                    # Lua VM management
│   ├── ScriptManager.h/cpp            # Script loading/caching
│   ├── HotReloader.h/cpp              # Hot-reload system
│   ├── Sandbox.h/cpp                  # Sandboxing
│   ├── ScriptValue.h                  # Script value types
│   ├── bindings/
│   │   ├── BindingHelpers.h           # Binding macros
│   │   ├── EntityBindings.h/cpp
│   │   ├── ComponentBindings.h/cpp
│   │   ├── TransformBindings.h/cpp
│   │   ├── PhysicsBindings.h/cpp
│   │   ├── AudioBindings.h/cpp
│   │   ├── InputBindings.h/cpp
│   │   ├── TimeBindings.h/cpp
│   │   ├── MathBindings.h/cpp
│   │   ├── EventBindings.h/cpp
│   │   └── QueryBindings.h/cpp
│   └── debug/
│       ├── ScriptDebugger.h/cpp
│       └── ScriptProfiler.h/cpp

eyelib/ecs/
├── components/
│   └── ScriptComponent.h              # Script ECS component
└── systems/
    └── ScriptSystem.h/cpp              # Script execution system
```

**Public API** (`api/scripting/`):
```
api/scripting/
├── CMakeLists.txt                     # API build configuration
├── Script.h                            # Script API (PIMPL)
├── ScriptManager.h                     # Script manager API
├── ScriptValue.h                       # Script value API
└── All.h                               # Convenience header
```

**Note**: The scripting system follows the same architecture pattern as other modules:
- Implementation in `eyelib/scripting/` (library)
- Public API in `api/scripting/` (thin wrapper, PIMPL)
- ECS integration in `eyelib/ecs/` (components and systems)

---

## 6. Integration with World Kernel

### 6.1 Scripts vs World Command API

**Clear Separation:**

- **WCA (World Command API)**: AI uses this for world-wide operations
  - `CreateEntity`, `SetRule`, `ComposeGameMode`
  - Structured, auditable, reversible

- **Lua Scripts**: Humans use this for local behaviors
  - NPC AI, weapon logic, item effects
  - Fast iteration, designer-friendly

**Scripts Can Call WCA (Optional):**
```lua
-- Script can issue world commands (with proper authority)
WorldCommand.CreateEntity("Enemy")
WorldCommand.SetRule("DamageMultiplier", 1.5)
```

But this is **optional** and **sandboxed** - scripts don't have to use WCA.

---

### 6.2 Scripts in Regions

**Region-Scoped Scripts:**
- Scripts can be attached to regions
- Scripts execute only when entities are inside the region
- Useful for game mode logic (scoring, timers, win conditions)

```lua
-- Arena script (attached to region)
function ArenaScript:OnUpdate(dt)
    -- Check if players are in arena
    local players = Query.FindInRegion(self.region, "Player")
    
    if #players == 0 then
        self:EndMatch()
    end
end
```

---

## 7. Example Scripts

### 7.1 NPC Behavior Script

```lua
local NPCBehavior = {}

function NPCBehavior:OnStart()
    self.health = 100
    self.speed = 5.0
    self.state = "patrol"
    self.patrolPoints = {
        {x=0, y=0, z=0},
        {x=10, y=0, z=0},
        {x=10, y=0, z=10},
        {x=0, y=0, z=10}
    }
    self.currentPatrolIndex = 1
end

function NPCBehavior:OnUpdate(dt)
    if self.state == "patrol" then
        self:Patrol(dt)
    elseif self.state == "chase" then
        self:Chase(dt)
    elseif self.state == "attack" then
        self:Attack(dt)
    end
end

function NPCBehavior:Patrol(dt)
    local target = self.patrolPoints[self.currentPatrolIndex]
    local pos = Transform.GetPosition(self.entity)
    
    local dir = Math.Normalize(
        target.x - pos.x,
        target.y - pos.y,
        target.z - pos.z
    )
    
    pos.x = pos.x + dir.x * self.speed * dt
    pos.z = pos.z + dir.z * self.speed * dt
    Transform.SetPosition(self.entity, pos.x, pos.y, pos.z)
    
    if Math.Distance(pos.x, pos.y, pos.z, target.x, target.y, target.z) < 1.0 then
        self.currentPatrolIndex = (self.currentPatrolIndex % #self.patrolPoints) + 1
    end
    
    -- Check for players
    local players = Query.FindNearby(pos.x, pos.y, pos.z, 15.0)
    for _, player in ipairs(players) do
        if Component.HasComponent(player, "Player") then
            self.state = "chase"
            self.target = player
            Audio.PlaySound(self.entity, "alert.wav")
            break
        end
    end
end

function NPCBehavior:Chase(dt)
    if not self.target or not Entity.IsValid(self.target) then
        self.state = "patrol"
        return
    end
    
    local pos = Transform.GetPosition(self.entity)
    local targetPos = Transform.GetPosition(self.target)
    
    local dir = Math.Normalize(
        targetPos.x - pos.x,
        targetPos.y - pos.y,
        targetPos.z - pos.z
    )
    
    pos.x = pos.x + dir.x * self.speed * 1.5 * dt
    pos.z = pos.z + dir.z * self.speed * 1.5 * dt
    Transform.SetPosition(self.entity, pos.x, pos.y, pos.z)
    
    if Math.Distance(pos.x, pos.y, pos.z, targetPos.x, targetPos.y, targetPos.z) < 2.0 then
        self.state = "attack"
    end
end

function NPCBehavior:Attack(dt)
    if not self.target or not Entity.IsValid(self.target) then
        self.state = "patrol"
        return
    end
    
    -- Attack logic
    Component.Damage(self.target, 10 * dt)
    Audio.PlaySound(self.entity, "attack.wav")
end

function NPCBehavior:OnCollision(other)
    if Component.HasComponent(other, "Bullet") then
        local damage = Component.GetDamage(other)
        self.health = self.health - damage
        
        if self.health <= 0 then
            Entity.Destroy(self.entity)
            Audio.PlaySound(self.entity, "death.wav")
        end
    end
end

return NPCBehavior
```

---

### 7.2 Weapon Script

```lua
local WeaponController = {}

function WeaponController:OnStart()
    self.fireRate = 0.5  -- shots per second
    self.damage = 25
    self.range = 100.0
    self.lastFireTime = 0.0
    self.ammo = 30
    self.maxAmmo = 30
end

function WeaponController:OnUpdate(dt)
    local time = Time.Time()
    
    if Input.IsKeyPressed("MouseLeft") then
        if time - self.lastFireTime >= 1.0 / self.fireRate then
            if self.ammo > 0 then
                self:Fire()
                self.lastFireTime = time
                self.ammo = self.ammo - 1
            else
                Audio.PlaySound(self.entity, "empty.wav")
            end
        end
    end
    
    if Input.IsKeyPressed("R") then
        self:Reload()
    end
end

function WeaponController:Fire()
    local pos = Transform.GetPosition(self.entity)
    local forward = Transform.GetForward(self.entity)
    
    -- Raycast
    local endPos = {
        x = pos.x + forward.x * self.range,
        y = pos.y + forward.y * self.range,
        z = pos.z + forward.z * self.range
    }
    
    local hit = Physics.Raycast(pos.x, pos.y, pos.z, endPos.x, endPos.y, endPos.z)
    
    if hit then
        if Component.HasComponent(hit.entity, "Health") then
            Component.Damage(hit.entity, self.damage)
        end
        
        -- Spawn muzzle flash
        Event.Emit("MuzzleFlash", self.entity)
    end
    
    Audio.PlaySound(self.entity, "fire.wav")
end

function WeaponController:Reload()
    if self.ammo < self.maxAmmo then
        self.ammo = self.maxAmmo
        Audio.PlaySound(self.entity, "reload.wav")
    end
end

return WeaponController
```

---

## 8. Testing Strategy

### 8.1 Unit Tests

- Script loading/unloading
- API binding correctness
- Sandbox restrictions
- Error handling

### 8.2 Integration Tests

- Script execution in ECS
- Hot-reloading
- Event system integration
- Multi-script scenarios

### 8.3 Performance Tests

- Script execution overhead
- Memory usage
- Hot-reload performance
- Large script count (100+ scripts)

---

## 9. Dependencies

### Required

- **Lua 5.4** or **LuaJIT 2.1** (recommended for performance)
  - Include in `thirdparty/lua/`
  - CMake integration

### Optional

- **LuaBridge** or **sol2** (C++ binding helpers)
  - Makes bindings easier
  - Or write custom bindings

---

## 10. Test Game Scene

**Purpose**: Build a complete game scene to test and demonstrate the scripting system

**Scene Components**:
1. **Player Character** (FPS-style)
   - Script: `PlayerController.lua`
   - WASD movement, mouse look
   - Jump (Space)
   - Health system

2. **Weapon System**
   - Script: `WeaponController.lua`
   - Left-click to fire
   - Raycast shooting
   - Ammo management
   - Reload (R key)

3. **Enemy NPCs**
   - Script: `EnemyAI.lua`
   - Patrol behavior
   - Player detection
   - Chase and attack
   - Health and death

4. **Health & Damage System**
   - Script: `HealthComponent.lua` (or C++ component with script hooks)
   - Health points
   - Damage application
   - Death handling
   - Health UI (optional)

5. **Game Manager**
   - Script: `GameManager.lua`
   - Score tracking
   - Wave spawning
   - Win/lose conditions
   - Game state management

**Test Location**: `app/test/` or new `app/game_test/`

**Scripts Location**: `assets/scripts/`
- `PlayerController.lua`
- `WeaponController.lua`
- `EnemyAI.lua`
- `HealthComponent.lua`
- `GameManager.lua`

**Testing Goals**:
- ✅ All scripts load and execute correctly
- ✅ Hot-reload works (edit script → see changes immediately)
- ✅ Scripts interact with engine (physics, rendering, audio)
- ✅ Performance is acceptable (60+ FPS with multiple scripts)
- ✅ No crashes or memory leaks
- ✅ Scripts can be debugged (breakpoints, variable inspection)

---

## 11. Success Criteria

✅ Scripts can be attached to entities via ECS
✅ Scripts can access all engine APIs safely
✅ Scripts hot-reload without restarting game
✅ Scripts are sandboxed and cannot break the world
✅ Scripts have < 1ms execution overhead per script
✅ Debugging tools work (breakpoints, variable inspection)
✅ **Test game scene works end-to-end** (shooting, character control, health/damage)
✅ Documentation complete with examples

---

## 11. Future Enhancements

- **Visual Scripting**: Node-based editor (like Blueprint)
- **Script Templates**: Pre-made script templates
- **Script Marketplace**: Share scripts between projects
- **TypeScript/JavaScript Support**: Alternative to Lua
- **Script Compilation**: Pre-compile scripts to bytecode
- **Multi-threading**: Parallel script execution

---

## 12. Notes

- **Keep it simple initially**: Start with basic Lua, add features incrementally
- **Performance matters**: Scripts run every frame, optimize hot paths
- **Security first**: Sandbox aggressively, test edge cases
- **Documentation**: Every API binding needs Lua examples
- **AI integration**: Scripts complement WCA, don't replace it

---

**This plan provides a complete, industry-grade scripting system that integrates seamlessly with the AI-first architecture while maintaining the separation of concerns between AI control (WCA) and human-authored gameplay logic (Lua).**
