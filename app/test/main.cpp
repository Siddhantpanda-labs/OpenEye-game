#include "api/core/All.h"
#include "CharacterTest.h"

#include <iostream>
#include <string>
#include <chrono>
#include <thread>

namespace {

bool RunCharacterTest() {
    using namespace eyelib::test;

    CharacterTest test;
    if (!test.Initialize(1280, 720)) {
        eyelib::api::core::Log::Error("Failed to initialize character test");
        return false;
    }

    // Load character model (try common paths)
    std::vector<std::string> modelPaths = {
        "assets/models/animation/Orc_Idle.fbx",
        "assets/models/animation/orc_idle.fbx",
        "assets/models/animation/Orc.fbx",
        "assets/models/animation/orc.fbx"
    };

    bool modelLoaded = false;
    for (const auto& path : modelPaths) {
        if (test.LoadCharacterModel(path)) {
            modelLoaded = true;
            eyelib::api::core::Log::InfoF("Successfully loaded model: %s", path.c_str());
            break;
        }
    }

    if (!modelLoaded) {
        eyelib::api::core::Log::Warn("Could not load character model. Trying to continue without model...");
    }

    // Try to load idle, walk, run animations (all three needed for Idle/Walk/Run state machine)
    std::vector<std::string> idlePaths = {
        "assets/models/animation/Orc_Idle.fbx",
        "assets/models/animation/orc_idle.fbx"
    };
    std::vector<std::string> walkPaths = {
        "assets/models/animation/Start_Walking.fbx",
        "assets/models/animation/start_walking.fbx"
    };
    std::vector<std::string> runPaths = {
        "assets/models/animation/Running.fbx",
        "assets/models/animation/running.fbx"
    };

    std::string idlePath = idlePaths.empty() ? "" : idlePaths[0];
    std::string walkPath = walkPaths.empty() ? "" : walkPaths[0];
    std::string runPath = runPaths.empty() ? "" : runPaths[0];

    if (!idlePath.empty() || !walkPath.empty() || !runPath.empty()) {
        test.LoadAnimations(idlePath, walkPath, runPath);
    }

    const auto startTime = std::chrono::steady_clock::now();
    auto lastTime = startTime;

    while (!test.ShouldClose()) {
        const auto now = std::chrono::steady_clock::now();
        float deltaTime = std::chrono::duration<float>(now - lastTime).count();
        if (deltaTime > 0.1f) deltaTime = 0.1f; // Cap delta time
        lastTime = now;

        test.Update(deltaTime);
        test.Render();

        const auto frameEnd = std::chrono::steady_clock::now();
        const auto frameMs = std::chrono::duration_cast<std::chrono::milliseconds>(frameEnd - lastTime);
        if (frameMs.count() < 16) {
            std::this_thread::sleep_for(std::chrono::milliseconds(16 - frameMs.count()));
        }
    }

    test.Shutdown();
    return true;
}

} // namespace

int main() {
    using eyelib::api::core::Log;
    using eyelib::api::core::LogLevel;

    Log::Initialize("OpenEyeApiTests.log");
    Log::SetLevel(LogLevel::Info);

    Log::Info("Starting Third-Person Character Test");
    Log::Info("Controls: WASD = Move, Right Mouse Button = Look around, Scroll = Zoom");

    bool success = RunCharacterTest();

    Log::Info(success ? "Character test completed successfully" : "Character test failed");
    Log::Shutdown();

    return success ? 0 : 1;
}
