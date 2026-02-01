#pragma once

namespace openeye::player {

enum class ExitCode : int {
    Ok = 0,
    InvalidArgs = 2,
    ConfigError = 3,
    EngineInitFailed = 4,
    SceneMissing = 5,
    SceneLoadFailed = 6,
    AssetStrictFailed = 7,
    WindowInitFailed = 8,
    RenderInitFailed = 9,
    PhysicsInitFailed = 10
};

int RunPlayer(int argc, char** argv);

} // namespace openeye::player
