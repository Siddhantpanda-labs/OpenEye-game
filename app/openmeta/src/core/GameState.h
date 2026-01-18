#pragma once

namespace OpenMeta {

enum class GameStateType {
    None,
    MainMenu,
    Loading,
    Playing,
    Paused,
    GameOver,
    Settings
};

class GameState {
public:
    GameState() = default;
    virtual ~GameState() = default;

    virtual void OnEnter() {}
    virtual void OnExit() {}
    virtual void OnUpdate(float deltaTime) {}
    virtual void OnRender() {}
    virtual void OnPause() {}
    virtual void OnResume() {}

    GameStateType GetType() const { return m_type; }
    bool IsActive() const { return m_isActive; }

protected:
    GameStateType m_type = GameStateType::None;
    bool m_isActive = false;
};

class GameStateManager {
public:
    GameStateManager();
    ~GameStateManager();

    void PushState(GameState* state);
    void PopState();
    void ChangeState(GameState* state);
    void Clear();

    void Update(float deltaTime);
    void Render();

    GameState* GetCurrentState() const { return m_currentState; }

private:
    GameState* m_currentState = nullptr;
};

} // namespace OpenMeta

