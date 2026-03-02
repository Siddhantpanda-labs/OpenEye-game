# Lua Scripting System - Industry Standard Assessment

**Note:** This assessment was written when scripting used `eyelib::ecs`. The codebase has since consolidated to **eyelib::world**; bindings now use `world::World` and `world::EntityId`. The issues and solutions below still apply; read `ecs::World`/`ecs::Entity` as **world::World**/**world::EntityId** where relevant.

## Executive Summary

**Current Status**: ✅ **Foundation is solid, but needs refinement for production-grade scalability**

The Lua scripting system has a **good architectural foundation** with proper separation of concerns, but several critical areas need improvement to meet industry standards for **scalability, maintainability, and extensibility**.

---

## ✅ What's Done Well (Industry-Standard Patterns)

### 1. **Architecture & Organization** ⭐⭐⭐⭐⭐
- ✅ Clear separation: `LuaVM`, `ScriptManager`, `ScriptSystem`
- ✅ Proper namespace organization (`eyelib::scripting::bindings`)
- ✅ World/entity integration via `ScriptComponent` (world::World, world::EntityId)
- ✅ Modular binding structure (one file per API module)
- ✅ CMake integration is clean

### 2. **Security & Sandboxing** ⭐⭐⭐⭐
- ✅ Sandbox modes (Restricted/Standard/Permissive)
- ✅ Function removal for dangerous operations
- ✅ Memory usage tracking infrastructure
- ⚠️ Memory limits not fully implemented

### 3. **Script Lifecycle** ⭐⭐⭐⭐
- ✅ OnStart/OnUpdate/OnDestroy pattern (Unity-style)
- ✅ Hot-reload infrastructure
- ✅ Script caching and dependency tracking structure

---

## ⚠️ Critical Issues (Must Fix for Production)

### 1. **Binding Registration - NOT Scalable** 🔴 **CRITICAL**

**Problem:**
```cpp
// EntityBindings.cpp (historical example; now uses BindingRegistry + world::World*)
static world::World* s_world = nullptr;  // ❌ Global static - not thread-safe
void RegisterEntityBindings(lua_State* L, world::World* world) {
    s_world = world;  // ❌ Overwrites previous world
}
```

**Issues:**
- ❌ Each binding file has its own `static s_world` - conflicts if multiple worlds exist
- ❌ Not thread-safe
- ❌ Cannot support multiple worlds simultaneously
- ❌ No way to unregister or switch contexts

**Industry Standard Solution:**
```cpp
// Use Lua registry to store world pointer per VM
void RegisterEntityBindings(lua_State* L, world::World* world) {
    // Store world in Lua registry (thread-safe, per-VM)
    lua_pushlightuserdata(L, (void*)&WORLD_REGISTRY_KEY);
    lua_pushlightuserdata(L, world);
    lua_settable(L, LUA_REGISTRYINDEX);
    
    // Or use userdata with metatable for type safety
}
```

### 2. **No Type Safety for Entities** 🔴 **CRITICAL**

**Problem:**
```cpp
// Current: Entity (world::EntityId) pushed as userdata or index
inline void PushEntity(lua_State* L, world::EntityId e) {
    // e.g. lua_pushinteger(L, e.index);  // ❌ Or userdata; validation varies
}
```

**Issues:**
- ❌ Scripts can pass invalid integers as entities
- ❌ No way to detect invalid entity usage
- ❌ No garbage collection protection

**Industry Standard Solution:**
```cpp
// Use userdata with metatable
struct EntityUserdata {
    world::EntityId entity;
    world::World* world;  // For validation
};

// Register metatable with __gc, __index, __eq
// Validate entity in every binding function
```

### 3. **No Binding Macros/Helpers** 🟡 **HIGH PRIORITY**

**Problem:**
- Manual registration is verbose and error-prone
- No consistency checking
- Hard to add new bindings

**Industry Standard Solution:**
```cpp
// BindingHelpers.h - Add macros
#define LUA_BIND_FUNCTION(L, table, name, func) \
    do { \
        lua_getglobal(L, table); \
        lua_pushcfunction(L, func); \
        lua_setfield(L, -2, name); \
        lua_pop(L, 1); \
    } while(0)

#define LUA_BIND_METHOD(L, table, name, func) \
    LUA_BIND_FUNCTION(L, table, name, func)

// Usage:
LUA_BIND_FUNCTION(L, "Entity", "Create", lua_Entity_Create);
```

### 4. **File Modification Time Not Implemented** 🟡 **HIGH PRIORITY**

**Problem:**
```cpp
std::time_t GetFileModificationTime(const std::string& path) const {
    // Platform-specific implementation would go here
    return std::time(nullptr);  // ❌ Always returns current time
}
```

**Impact:**
- Hot-reload won't work correctly
- Scripts won't reload when files change

**Fix Required:**
- Implement platform-specific file time retrieval (Windows: `GetFileTime`, Linux: `stat`)

### 5. **No API Layer (PIMPL)** 🟡 **MEDIUM PRIORITY**

**Problem:**
```cpp
// api/scripting/All.h - Just a placeholder
namespace scripting {
    // Placeholder for future API
}
```

**Industry Standard:**
- Public API should use PIMPL to hide implementation
- Prevents ABI breaks
- Allows internal refactoring without breaking clients

---

## 📊 Scalability Assessment

### Current Limitations:

1. **Multiple Worlds**: ❌ Cannot support multiple `World` instances
2. **Thread Safety**: ❌ Not thread-safe (static globals)
3. **Binding Expansion**: ⚠️ Manual, error-prone (needs macros)
4. **Type Safety**: ❌ Entities are raw integers
5. **Memory Management**: ⚠️ Infrastructure exists but not enforced

### Scalability Score: **6/10**

**Blockers for Scale:**
- Static global `s_world` prevents multi-world support
- No userdata/metatables for type safety
- No binding registration system

---

## 🎯 Recommendations (Priority Order)

### **Phase 1: Critical Fixes** (Must Do)

1. **Fix Binding Registration**
   - Use Lua registry instead of static globals
   - Support multiple worlds per VM
   - Thread-safe context storage

2. **Implement Entity Userdata**
   - Create `EntityUserdata` struct with metatable
   - Add validation in all bindings
   - Garbage collection protection

3. **Implement File Modification Time**
   - Platform-specific implementation
   - Windows: `GetFileTime`
   - Linux: `stat`

### **Phase 2: Scalability** (Should Do)

4. **Add Binding Macros**
   - `LUA_BIND_FUNCTION`, `LUA_BIND_METHOD`, `LUA_BIND_TABLE`
   - Consistency checking
   - Easier to add new bindings

5. **Unified Binding Registry**
   - Central registration system
   - Automatic cleanup
   - Dependency management

6. **API Layer (PIMPL)**
   - Public API in `api/scripting/`
   - Hide implementation details
   - ABI stability

### **Phase 3: Production Features** (Nice to Have)

7. **Script Debugging**
   - Lua debugger integration
   - Breakpoints, step-through
   - Variable inspection

8. **Script Profiling**
   - Performance monitoring
   - Hot-spot detection
   - Memory profiling

9. **Coroutine Support**
   - Async script execution
   - Yield/resume patterns
   - Non-blocking operations

10. **Better Error Handling**
    - Stack traces
    - Error context
    - Recovery mechanisms

---

## 📈 Comparison to Industry Standards

| Feature | Unity | Unreal (Blueprints) | Roblox | **Our System** |
|---------|-------|---------------------|--------|----------------|
| Type Safety | ✅ | ✅ | ✅ | ❌ |
| Multiple Worlds | ✅ | ✅ | ✅ | ❌ |
| Binding Macros | ✅ | ✅ | ✅ | ❌ |
| Hot Reload | ✅ | ✅ | ✅ | ⚠️ (infrastructure only) |
| Debugging | ✅ | ✅ | ✅ | ❌ |
| Profiling | ✅ | ✅ | ✅ | ❌ |
| Thread Safety | ✅ | ✅ | ✅ | ❌ |
| API Layer | ✅ | ✅ | ✅ | ❌ |

**Overall Grade: C+ (Good foundation, needs refinement)**

---

## ✅ Conclusion

**The foundation is solid**, but the system needs **critical fixes** before it can scale to production:

1. **Binding registration** must be refactored (remove static globals)
2. **Entity type safety** must be implemented (userdata + metatables)
3. **File modification time** must be implemented (hot-reload depends on it)

**Estimated effort to reach production-grade:**
- Phase 1 (Critical): **2-3 days**
- Phase 2 (Scalability): **3-5 days**
- Phase 3 (Production): **1-2 weeks**

**Recommendation**: Fix Phase 1 issues immediately, then proceed with test game scene. Phase 2 can be done incrementally as more bindings are added.
