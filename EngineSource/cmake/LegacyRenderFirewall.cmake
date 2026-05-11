# LegacyRenderFirewall.cmake — opt-in include-firewall for the editor/player migration off
# the legacy render module (eyelib/render + api/render) onto the new render module
# (eyelib/rendering + api/rendering).
#
# Background:
#   - eyelib/rendering enforces its own one-way firewall in its CMakeLists (forbids any
#     #include "eyelib/render/" inside the new module's tree).
#   - This helper is the *consumer-side* counterpart: any target/directory that has been
#     migrated to api/rendering must not silently regress by re-introducing
#     #include "api/render/..." or #include "eyelib/render/...".
#
# Discipline:
#   - This firewall is OPT-IN per directory. Do not switch it on for editor or player
#     wholesale until the corresponding migration section closes (M2 / M3 / M4 / M5).
#   - Failures are FATAL_ERROR at configure time so a regression cannot land in CI.
#   - Generated files under build/ are ignored by virtue of scanning a source directory.
#   - The check looks at .cpp/.h/.hpp/.inl/.c files only.
#
# Usage:
#   include(${OPENEYE_ENGINE_SOURCE}/cmake/LegacyRenderFirewall.cmake)
#   # Forbid both legacy include roots in this directory tree:
#   openeye_enforce_no_legacy_render_includes(
#       DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
#       LABEL     "OpenEyeRenderingSandbox"
#   )
#   # Or scope to a specific subset of files (incremental migration in editor):
#   openeye_enforce_no_legacy_render_includes(
#       FILES ${MIGRATED_SCENE_VIEW_SOURCES}
#       LABEL "OpenEyeEditor (scene view migrated subset)"
#   )

if(DEFINED OPENEYE_LEGACY_RENDER_FIREWALL_INCLUDED)
    return()
endif()
set(OPENEYE_LEGACY_RENDER_FIREWALL_INCLUDED TRUE)

# Forbidden include patterns. These are exact substring matches against file contents
# (the same approach used by EngineSource/eyelib/rendering/CMakeLists.txt).
set(_OPENEYE_LEGACY_INCLUDES
    "#include \"api/render/"
    "#include \"eyelib/render/"
    "#include <api/render/"
    "#include <eyelib/render/"
)

# openeye_enforce_no_legacy_render_includes(
#   [DIRECTORY <dir>]            # scan all .cpp/.h/.hpp/.inl/.c under <dir> recursively
#   [FILES <file> ...]           # scan a specific list of files
#   LABEL <human readable name>  # included in the FATAL_ERROR message
# )
#
# Provide either DIRECTORY or FILES (or both). At least one is required.
function(openeye_enforce_no_legacy_render_includes)
    cmake_parse_arguments(_FW "" "DIRECTORY;LABEL" "FILES" ${ARGN})

    if(NOT _FW_LABEL)
        message(FATAL_ERROR "openeye_enforce_no_legacy_render_includes: LABEL is required.")
    endif()

    set(_scan_files "")
    if(_FW_DIRECTORY)
        if(NOT IS_DIRECTORY "${_FW_DIRECTORY}")
            message(FATAL_ERROR
                "openeye_enforce_no_legacy_render_includes(${_FW_LABEL}): "
                "DIRECTORY '${_FW_DIRECTORY}' does not exist.")
        endif()
        file(GLOB_RECURSE _dir_files CONFIGURE_DEPENDS
            "${_FW_DIRECTORY}/*.cpp"
            "${_FW_DIRECTORY}/*.c"
            "${_FW_DIRECTORY}/*.h"
            "${_FW_DIRECTORY}/*.hpp"
            "${_FW_DIRECTORY}/*.inl"
        )
        list(APPEND _scan_files ${_dir_files})
    endif()
    if(_FW_FILES)
        list(APPEND _scan_files ${_FW_FILES})
    endif()
    if(NOT _scan_files)
        message(FATAL_ERROR
            "openeye_enforce_no_legacy_render_includes(${_FW_LABEL}): "
            "no files to scan (provide DIRECTORY or FILES).")
    endif()

    list(REMOVE_DUPLICATES _scan_files)

    foreach(_src ${_scan_files})
        if(NOT EXISTS "${_src}")
            continue()
        endif()
        file(READ "${_src}" _src_content)
        foreach(_pat ${_OPENEYE_LEGACY_INCLUDES})
            string(FIND "${_src_content}" "${_pat}" _pos)
            if(NOT _pos EQUAL -1)
                message(FATAL_ERROR
                    "Legacy render firewall (${_FW_LABEL}): '${_src}' contains forbidden "
                    "include matching '${_pat}'. After migrating to api/rendering, do not "
                    "reintroduce legacy api/render or eyelib/render headers in this tree.")
            endif()
        endforeach()
    endforeach()
endfunction()
