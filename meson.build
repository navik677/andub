project(
  'andub',
  'cpp',
  version: '1.1.0-linux',
  default_options: [
    'cpp_std=c++20',
    'warning_level=2',
    'buildtype=release',
  ],
)

gtk4_dep = dependency('gtk4', version: '>=4.6.0', required: true)
threads_dep = dependency('threads')
curl_dep = dependency('libcurl', required: true)
mpv_dep = dependency('mpv', required: true)

sources = files(
  'src/main.cpp',
  'src/application.cpp',
  'src/utils/http_client.cpp',
  'src/utils/kodik_resolver.cpp',
  'src/services/image_cache.cpp',
  'src/services/player_service.cpp',
  'src/services/history_manager.cpp',
  'src/services/favorites_manager.cpp',
  'src/services/download_service.cpp',
  'src/services/theme_manager.cpp',
  'src/providers/anilibria_provider.cpp',
  'src/providers/dreamcast_provider.cpp',
  'src/providers/shizaproject_provider.cpp',
  'src/providers/anibaza_provider.cpp',
  'src/providers/anidub_provider.cpp',
  'src/providers/anitube_provider.cpp',
  'src/providers/animevost_provider.cpp',
  'src/providers/anistar_provider.cpp',
  'src/ui/anime_card.cpp',
  'src/ui/details_view.cpp',
  'src/ui/player_view.cpp',
  'src/ui/downloads_view.cpp',
  'src/ui/main_window.cpp',
)

inc_dirs = include_directories('src', 'include')

executable(
  'andub',
  sources,
  include_directories: inc_dirs,
  dependencies: [
    gtk4_dep,
    threads_dep,
    curl_dep,
    mpv_dep,
  ],
  cpp_args: ['-DHAVE_LIBCURL'],
  install: true,
)

install_data(
  'resources/style.css',
  install_dir: get_option('datadir') / 'andub',
)

install_data(
  'resources/anime-gui.png',
  install_dir: get_option('datadir') / 'icons/hicolor/256x256/apps',
  rename: 'andub.png',
)

install_data(
  'desktop/andub.desktop',
  install_dir: get_option('datadir') / 'applications',
)
