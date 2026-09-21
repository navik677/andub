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

# MPV dependency: prefer the system's own libmpv (matches the distro's
# ffmpeg/lua/placebo ABI) so the bundled lib/libmpv.so (built against
# bleeding-edge Fedora package versions) doesn't break linking on other
# distros. Fall back to the bundled copy only if no system libmpv is found.
mpv_dep = dependency('mpv', required: false)
using_bundled_mpv = false
if not mpv_dep.found()
  mpv_dep = cpp.find_library('mpv',
    dirs: ['/usr/lib64', '/usr/lib', '/usr/lib/x86_64-linux-gnu', meson.current_source_dir() / 'lib'],
    required: true)
  using_bundled_mpv = true
endif

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

# Only point the runtime loader at the bundled lib/ dir when we actually had
# to fall back to it. Otherwise the dynamic linker would prefer the bundled
# libmpv.so.2 (same SONAME as the system one, built against exotic package
# versions) over the correctly-linked system libmpv, even though the build
# itself used the system library.
bundled_rpath = using_bundled_mpv ? (meson.current_source_dir() / 'lib') : ''
bundled_install_rpath = using_bundled_mpv ? ('$ORIGIN/../lib:' + meson.current_source_dir() / 'lib') : ''

executable('andub',
  sources,
  include_directories: inc_dirs,
  dependencies: deps,
  cpp_args: extra_c_args,
  build_rpath: bundled_rpath,
  install_rpath: bundled_install_rpath,
  install: true
)

install_data(
  'resources/style.css',
  install_dir: get_option('datadir') / 'andub'
)
