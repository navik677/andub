cmake_minimum_required(VERSION 3.21)

project(andub VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release)
endif()

option(ANDUB_BUILD_TESTS "Build the provider/core test programs" OFF)

if(NOT WIN32 OR NOT MSVC)
    message(FATAL_ERROR "This branch is the Windows port: build with MSVC (scripts/build_windows.ps1)")
endif()

find_package(Qt6 6.6 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 OpenGL Concurrent)
find_package(Threads REQUIRED)

qt_standard_project_setup(REQUIRES 6.6)

# Source files are UTF-8 (Cyrillic UI strings); keep Windows headers lean.
add_compile_options(/utf-8 /permissive- /Zc:__cplusplus /W3)
add_compile_definitions(WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE _CRT_SECURE_NO_WARNINGS)

# libmpv: prebuilt mpv-dev package unpacked by scripts/fetch_deps.ps1
# (include/mpv/*.h, libmpv-2.dll and an MSVC import library libmpv.lib).
set(ANDUB_MPV_DIR "${CMAKE_SOURCE_DIR}/third_party/mpv" CACHE PATH "Unpacked mpv-dev package")
find_path(MPV_INCLUDE_DIR mpv/client.h PATHS "${ANDUB_MPV_DIR}/include" NO_DEFAULT_PATH REQUIRED)
find_library(MPV_IMPLIB NAMES libmpv mpv PATHS "${ANDUB_MPV_DIR}" NO_DEFAULT_PATH REQUIRED)
find_file(MPV_DLL libmpv-2.dll PATHS "${ANDUB_MPV_DIR}" NO_DEFAULT_PATH REQUIRED)
add_library(mpv::mpv SHARED IMPORTED)
set_target_properties(mpv::mpv PROPERTIES
    IMPORTED_IMPLIB "${MPV_IMPLIB}"
    IMPORTED_LOCATION "${MPV_DLL}"
    INTERFACE_INCLUDE_DIRECTORIES "${MPV_INCLUDE_DIR}")

# Backend: providers, services and utilities (no UI toolkit beyond QtCore)
add_library(andub_core STATIC
    src/utils/http_client.cpp
    src/utils/kodik_resolver.cpp
    src/services/image_cache.cpp
    src/services/player_service.cpp
    src/services/history_manager.cpp
    src/services/favorites_manager.cpp
    src/services/download_service.cpp
    src/services/upscaler.cpp
    src/providers/anilibria_provider.cpp
    src/providers/dreamcast_provider.cpp
    src/providers/shizaproject_provider.cpp
    src/providers/anibaza_provider.cpp
    src/providers/anidub_provider.cpp
    src/providers/anitube_provider.cpp
    src/providers/animevost_provider.cpp
    src/providers/anistar_provider.cpp
)
target_include_directories(andub_core PUBLIC src)
target_link_libraries(andub_core PUBLIC Qt6::Core mpv::mpv Threads::Threads winhttp)
target_compile_definitions(andub_core PUBLIC
    ANDUB_SOURCE_SHADER_DIR="${CMAKE_SOURCE_DIR}/resources/shaders")

qt_add_executable(andub WIN32
    src/main.cpp
    src/app/provider_registry.cpp src/app/provider_registry.hpp
    src/app/anime_list_model.cpp src/app/anime_list_model.hpp
    src/app/episode_list_model.cpp src/app/episode_list_model.hpp
    src/app/catalog.cpp src/app/catalog.hpp
    src/app/details.cpp src/app/details.hpp
    src/app/player_session.cpp src/app/player_session.hpp
    src/app/downloads_model.cpp src/app/downloads_model.hpp
    src/app/settings.cpp src/app/settings.hpp
    src/app/notifier.cpp src/app/notifier.hpp
    src/app/poster_provider.cpp src/app/poster_provider.hpp
    src/app/mpv_player.cpp src/app/mpv_player.hpp
    src/app/screen_inhibitor.cpp src/app/screen_inhibitor.hpp
)

# Icon, version info and the UTF-8 / DPI manifest. The manifest comes from the
# .rc file, so the linker must not generate its own (duplicate RT_MANIFEST).
target_sources(andub PRIVATE resources/andub.rc)
set_source_files_properties(resources/andub.rc PROPERTIES
    OBJECT_DEPENDS "${CMAKE_SOURCE_DIR}/resources/andub.ico;${CMAKE_SOURCE_DIR}/resources/andub.manifest")
target_include_directories(andub PRIVATE resources)
target_link_options(andub PRIVATE /MANIFEST:NO)

if(QT_KNOWN_POLICY_QTP0004)
    qt_policy(SET QTP0004 NEW)
endif()

set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(andub
    URI Andub
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/Theme.qml
        qml/DevDriver.qml
        qml/components/Icon.qml
        qml/components/AppButton.qml
        qml/components/IconButton.qml
        qml/components/AppToolTip.qml
        qml/components/Chip.qml
        qml/components/Segmented.qml
        qml/components/SearchField.qml
        qml/components/RoundedImage.qml
        qml/components/Backdrop.qml
        qml/components/Skeleton.qml
        qml/components/Spinner.qml
        qml/components/EmptyState.qml
        qml/components/AnimeCard.qml
        qml/components/EpisodeTile.qml
        qml/components/NavRail.qml
        qml/components/MenuSheet.qml
        qml/components/SeekBar.qml
        qml/components/VolumeSlider.qml
        qml/components/ToastHost.qml
        qml/pages/CatalogPage.qml
        qml/pages/DetailsPage.qml
        qml/pages/PlayerPage.qml
        qml/pages/DownloadsPage.qml
        qml/pages/SettingsPage.qml
    RESOURCES
        resources/andub.svg
        resources/fonts/Manrope.ttf
        resources/icons/arrow-clockwise.svg
        resources/icons/arrow-counter-clockwise.svg
        resources/icons/arrow-left.svg
        resources/icons/arrow-square-out.svg
        resources/icons/caret-down.svg
        resources/icons/caret-right.svg
        resources/icons/check.svg
        resources/icons/check-circle.svg
        resources/icons/check-circle-fill.svg
        resources/icons/corners-in.svg
        resources/icons/corners-out.svg
        resources/icons/download-simple.svg
        resources/icons/faders.svg
        resources/icons/fast-forward.svg
        resources/icons/film-slate.svg
        resources/icons/folder-open.svg
        resources/icons/funnel.svg
        resources/icons/gear-six.svg
        resources/icons/github-logo.svg
        resources/icons/heart.svg
        resources/icons/heart-fill.svg
        resources/icons/list-numbers.svg
        resources/icons/magnifying-glass.svg
        resources/icons/monitor-play.svg
        resources/icons/pause.svg
        resources/icons/pause-fill.svg
        resources/icons/play.svg
        resources/icons/play-fill.svg
        resources/icons/skip-back.svg
        resources/icons/skip-forward.svg
        resources/icons/sparkle.svg
        resources/icons/speaker-high.svg
        resources/icons/speaker-low.svg
        resources/icons/speaker-none.svg
        resources/icons/speaker-x.svg
        resources/icons/squares-four.svg
        resources/icons/star.svg
        resources/icons/star-fill.svg
        resources/icons/warning-circle.svg
        resources/icons/x.svg
)

# qmltyperegistrar includes the QML_ELEMENT headers by bare file name
target_include_directories(andub PRIVATE src/app)
target_compile_definitions(andub PRIVATE ANDUB_VERSION="${PROJECT_VERSION}")
target_link_libraries(andub PRIVATE
    andub_core
    Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2 Qt6::OpenGL Qt6::Concurrent)

# Next to andub.exe so it runs straight from the build directory
add_custom_command(TARGET andub POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${MPV_DLL}" "$<TARGET_FILE_DIR:andub>")

# "cmake --install" lays out the folder that windeployqt and the installer use
install(TARGETS andub RUNTIME DESTINATION .)
install(FILES "${MPV_DLL}" DESTINATION .)
install(DIRECTORY resources/shaders DESTINATION resources)
foreach(tool mpv.exe yt-dlp.exe)
    if(EXISTS "${CMAKE_SOURCE_DIR}/third_party/tools/${tool}")
        install(FILES "${CMAKE_SOURCE_DIR}/third_party/tools/${tool}" DESTINATION .)
    endif()
endforeach()

if(ANDUB_BUILD_TESTS)
    foreach(t test_core test_img test_providers test_shiza_search test_filters test_browse)
        add_executable(${t} tests/${t}.cpp)
        target_link_libraries(${t} PRIVATE andub_core)
        set_target_properties(${t} PROPERTIES WIN32_EXECUTABLE OFF)
    endforeach()
endif()
