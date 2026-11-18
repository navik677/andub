cmake_minimum_required(VERSION 3.21)

project(andub VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release)
endif()

option(ANDUB_BUILD_TESTS "Build the provider/core test programs" OFF)

find_package(Qt6 6.6 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 OpenGL Concurrent)
find_package(Qt6 QUIET OPTIONAL_COMPONENTS DBus)
find_package(PkgConfig QUIET)
find_package(CURL QUIET)
find_package(Threads REQUIRED)

qt_standard_project_setup(REQUIRES 6.6)

# libmpv: prefer the system library (matches the distro's ffmpeg/libplacebo ABI);
# fall back to the bundled copy in lib/ only when none is installed.
set(ANDUB_BUNDLED_MPV OFF)
if(PkgConfig_FOUND)
    pkg_check_modules(MPV IMPORTED_TARGET mpv)
endif()
if(NOT MPV_FOUND)
    find_library(MPV_LIBRARY NAMES mpv libmpv
        HINTS ${CMAKE_SOURCE_DIR}/lib REQUIRED)
    add_library(PkgConfig::MPV UNKNOWN IMPORTED)
    set_target_properties(PkgConfig::MPV PROPERTIES
        IMPORTED_LOCATION ${MPV_LIBRARY}
        INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_SOURCE_DIR}/include)
    if(MPV_LIBRARY MATCHES "^${CMAKE_SOURCE_DIR}/lib")
        set(ANDUB_BUNDLED_MPV ON)
    endif()
endif()

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
target_link_libraries(andub_core PUBLIC Qt6::Core PkgConfig::MPV Threads::Threads)
target_compile_definitions(andub_core PUBLIC
    ANDUB_SOURCE_SHADER_DIR="${CMAKE_SOURCE_DIR}/resources/shaders")
if(CURL_FOUND)
    target_link_libraries(andub_core PUBLIC CURL::libcurl)
    target_compile_definitions(andub_core PUBLIC HAVE_LIBCURL)
endif()
if(WIN32)
    target_link_libraries(andub_core PUBLIC ws2_32)
endif()

qt_add_executable(andub WIN32 MACOSX_BUNDLE
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

if(WIN32)
    # Icon, version info and the UTF-8 / DPI manifest
    target_sources(andub PRIVATE resources/andub.rc)
    set_source_files_properties(resources/andub.rc PROPERTIES
        OBJECT_DEPENDS "${CMAKE_SOURCE_DIR}/resources/andub.ico;${CMAKE_SOURCE_DIR}/resources/andub.manifest")
    target_include_directories(andub PRIVATE resources)
endif()

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
if(TARGET Qt6::DBus AND UNIX AND NOT APPLE)
    target_link_libraries(andub PRIVATE Qt6::DBus)
    target_compile_definitions(andub PRIVATE ANDUB_HAVE_DBUS)
endif()

set_target_properties(andub PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER io.github.navik677.andub
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION})

if(ANDUB_BUNDLED_MPV)
    set_target_properties(andub PROPERTIES
        BUILD_RPATH ${CMAKE_SOURCE_DIR}/lib
        INSTALL_RPATH "$ORIGIN/../lib")
endif()

include(GNUInstallDirs)
install(TARGETS andub
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY resources/shaders DESTINATION ${CMAKE_INSTALL_DATADIR}/andub)
if(UNIX AND NOT APPLE)
    install(FILES desktop/andub.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(FILES resources/andub.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
    foreach(size 16 24 32 48 64 128 256 512)
        install(FILES resources/appicon/andub-${size}.png
            DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/${size}x${size}/apps RENAME andub.png)
    endforeach()
endif()

if(ANDUB_BUILD_TESTS)
    foreach(t test_core test_img test_providers test_shiza_search)
        add_executable(${t} tests/${t}.cpp)
        target_link_libraries(${t} PRIVATE andub_core)
    endforeach()
endif()
