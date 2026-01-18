#pragma once

namespace OpenMeta {

class GameLoop {
public:
    GameLoop();
    ~GameLoop();

    void Initialize();
    void Update(float deltaTime);
    void FixedUpdate(float fixedDeltaTime);
    void Render();
    void Shutdown();

private:
    float m_accumulator = 0.0f;
    const float m_fixedTimeStep = 1.0f / 60.0f; // 60 FPS fixed update
};

} // namespace OpenMeta

