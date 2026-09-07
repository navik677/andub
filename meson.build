project('anime-gui', 'cpp',
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

extra_c_args = []
deps = [gtk4_dep, threads_dep]

if curl_dep.found()
  deps += curl_dep
  extra_c_args += '-DHAVE_LIBCURL'
endif

# Source files
sources = files(
  'src/main.cpp',
  'src/application.cpp',
  'src/utils/http_client.cpp',
  'src/services/image_cache.cpp',
  'src/services/player_service.cpp',
  'src/services/history_manager.cpp',
  'src/services/favorites_manager.cpp',
  'src/services/download_service.cpp',
  'src/providers/anilibria_provider.cpp',
  'src/providers/animevost_provider.cpp',
  'src/ui/anime_card.cpp',
  'src/ui/details_view.cpp',
  'src/ui/downloads_view.cpp',
  'src/ui/main_window.cpp',
)

inc_dirs = include_directories('src')

executable('anime-gui',
  sources,
  include_directories: inc_dirs,
  dependencies: deps,
  cpp_args: extra_c_args,
  install: true
)

install_data(
  'resources/style.css',
  install_dir: get_option('datadir') / 'anime-gui'
)
