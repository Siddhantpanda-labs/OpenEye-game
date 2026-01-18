// OpenMeta Editor Entry Point
// Development editor with ImGui panels

#include "EditorApplication.h"
#include "../openmeta/src/core/GameApplication.h"

int main() {
    // Editor needs GameApplication to access the scene
    OpenMeta::GameApplication gameApp;
    
    if (!gameApp.Initialize()) {
        return -1;
    }
    
    // Create editor with game application
    OpenMeta::Editor::EditorApplication editor(&gameApp);
    
    if (!editor.Initialize()) {
        return -1;
    }
    
    // Editor runs in the same loop as game (handled by OpenMeta main.cpp when OPENEYE_EDITOR is defined)
    // This standalone editor executable is for testing/debugging
    // In practice, editor runs integrated with game in app/openmeta/main.cpp
    gameApp.Run();
    
    editor.Shutdown();
    gameApp.Shutdown();
    
    return 0;
}

