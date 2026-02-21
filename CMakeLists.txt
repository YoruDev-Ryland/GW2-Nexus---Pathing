cmake_minimum_required(VERSION 3.20)
project(Pathing VERSION 1.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# ── Nexus API header ──────────────────────────────────────────────────────────
FetchContent_Declare(
    nexus_api
    GIT_REPOSITORY https://github.com/RaidcoreGG/RCGG-lib-nexus-api.git
    GIT_TAG        main
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(nexus_api)

# ── Dear ImGui — MUST be pinned to v1.80 to match Nexus ──────────────────────
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.80
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(imgui)

# ── nlohmann/json — settings & any JSON we parse ─────────────────────────────
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(nlohmann_json)

# ── pugixml — fast, header-available C++ XML parser ──────────────────────────
FetchContent_Declare(
    pugixml
    GIT_REPOSITORY https://github.com/zeux/pugixml.git
    GIT_TAG        v1.14
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(pugixml)

# ── miniz — single-file ZIP / deflate (for .taco pack extraction) ─────────────
FetchContent_Declare(
    miniz
    GIT_REPOSITORY https://github.com/richgel999/miniz.git
    GIT_TAG        3.0.2
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(miniz)

# ── Resource file — embeds icon.png as Win32 resource ────────────────────────
configure_file(
    src/resources.rc.in
    ${CMAKE_CURRENT_BINARY_DIR}/resources.rc
    @ONLY)

# ── Sources ───────────────────────────────────────────────────────────────────
set(SOURCES
    src/entry.cpp
    src/Shared.cpp
    src/Settings.cpp
    src/TacoParser.cpp
    src/PackManager.cpp
    src/MarkerRenderer.cpp
    src/UI.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/resources.rc

    # ImGui compiled INTO this DLL (context shared with Nexus at runtime)
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp

    # miniz — compiled into the DLL
    ${miniz_SOURCE_DIR}/miniz.c
)

add_library(Pathing SHARED ${SOURCES})

# ── Include directories ───────────────────────────────────────────────────────
target_include_directories(Pathing PRIVATE
    src
    ${nexus_api_SOURCE_DIR}
    ${imgui_SOURCE_DIR}
    ${miniz_SOURCE_DIR}
    ${pugixml_SOURCE_DIR}/src
)

# ── Link libraries ────────────────────────────────────────────────────────────
target_link_libraries(Pathing PRIVATE
    nlohmann_json::nlohmann_json
    pugixml::pugixml
    winhttp          # not used in pathing but kept for potential GW2 API calls
    shlwapi          # PathFileExistsA / path helpers (Windows built-in)
)

# ── Compiler / linker configuration ──────────────────────────────────────────
if(MSVC)
    string(REGEX REPLACE "/W[0-4]" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

    target_compile_options(Pathing PRIVATE
        /W3
        /WX-
        /MP
        /EHsc
        /permissive-
        /wd4100     # unreferenced formal parameter (common in imgui / callbacks)
        /wd4189     # local variable initialised but not referenced
        /wd4244     # conversion, possible loss of data (miniz)
        /wd4456     # declaration hides previous local declaration
    )

    # NOMINMAX prevents windows.h from defining min/max macros that break std::
    target_compile_definitions(Pathing PRIVATE
        NOMINMAX
        WIN32_LEAN_AND_MEAN
        _CRT_SECURE_NO_WARNINGS   # miniz uses sscanf / other "unsafe" CRT
    )

    target_link_options(Pathing PRIVATE /SUBSYSTEM:WINDOWS)
endif()

# ── Output name ───────────────────────────────────────────────────────────────
set_target_properties(Pathing PROPERTIES
    OUTPUT_NAME   "Pathing"
    PREFIX        ""        # no "lib" prefix on MinGW
    SUFFIX        ".dll"
)
