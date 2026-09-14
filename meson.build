project('andub', 'cpp',
  version: '1.0.0',
  default_options: [
    'cpp_std=c++20',
    'warning_level=2',
    'buildtype=release',
  ]
)

cpp = meson.get_compiler('cpp')

# Dependencies
gtk4_dep = dependency('gtk4', version: '>= 4.6.0')
threads_dep = dependency('threads')
curl_dep = dependency('libcurl', required: false)
dl_dep = cpp.find_library('dl', required: false)

# MPV dependency
mpv_dep = cpp.find_library('mpv', dirs: [meson.current_source_dir() / 'lib', '/usr/lib64'], required: true)

extra_c_args = []
deps = [gtk4_dep, threads_dep, dl_dep, mpv_dep]

if curl_dep.found()
  deps += curl_dep
  extra_c_args += '-DHAVE_LIBCURL'
endif

# Source files
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

executable('andub',
  sources,
  include_directories: inc_dirs,
  dependencies: deps,
  cpp_args: extra_c_args,
  build_rpath: meson.current_source_dir() / 'lib',
  install_rpath: '$ORIGIN/../lib:' + meson.current_source_dir() / 'lib',
  install: true
)

install_data(
  'resources/style.css',
  install_dir: get_option('datadir') / 'andub'
)
