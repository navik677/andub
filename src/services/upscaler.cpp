#include "upscaler.hpp"
#include "player_service.hpp"
#include <filesystem>
#include <iostream>
#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#endif

#include <mpv/client.h>

namespace anime {

namespace fs = std::filesystem;

// Anime4K "Mode A" chain; %s slots are filled with network sizes (S/M/VL).
static std::vector<std::string> mode_a(const std::string& restore, const std::string& first, const std::string& second) {
    return {
        "Anime4K_Clamp_Highlights.glsl",
        "Anime4K_Restore_CNN_" + restore + ".glsl",
        "Anime4K_Upscale_CNN_x2_" + first + ".glsl",
        "Anime4K_AutoDownscalePre_x2.glsl",
        "Anime4K_AutoDownscalePre_x4.glsl",
        "Anime4K_Upscale_CNN_x2_" + second + ".glsl",
    };
}

// Each upscaler alone is subtle next to mpv's default scaler, so every profile
// chains it with restoration and/or sharpening. Order matters: mpv runs them in list order.
const std::vector<UpscaleProfile>& Upscaler::profiles() {
    static const std::vector<UpscaleProfile> list = {
        {"off", "Вимк", {}},
        {"fsr", "FSR", {"FSR.glsl", "adaptive-sharpen.glsl"}},
        {"fsrcnnx", "FSRCNNX", {"Anime4K_Restore_CNN_Soft_S.glsl", "FSRCNNX_x2_8-0-4-1.glsl", "adaptive-sharpen.glsl"}},
        {"anime4k", "Anime4K", mode_a("M", "M", "S")},
        {"anime4k-hq", "Anime4K HQ", mode_a("VL", "VL", "M")},
    };
    return list;
}

size_t Upscaler::index_of(const std::string& id) {
    const auto& list = profiles();
    for (size_t i = 0; i < list.size(); ++i) {
        if (list[i].id == id) return i;
    }
    return 0;
}

static fs::path executable_dir() {
#ifdef _WIN32
    wchar_t buf[MAX_PATH];
    DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    if (n > 0 && n < MAX_PATH) return fs::path(buf).parent_path();
#else
    std::error_code ec;
    auto p = fs::read_symlink("/proc/self/exe", ec);
    if (!ec) return p.parent_path();
#endif
    return {};
}

const std::string& Upscaler::shader_dir() {
    static const std::string dir = [] {
        std::vector<fs::path> candidates;
        if (const char* env = std::getenv("ANDUB_SHADER_DIR"); env && *env) candidates.emplace_back(env);

        fs::path exe = executable_dir();
        if (!exe.empty()) {
            candidates.push_back(exe / ".." / "share" / "andub" / "shaders"); // meson install, AppImage
            candidates.push_back(exe / "resources" / "shaders");              // Windows bundle
        }

        const char* xdg = std::getenv("XDG_DATA_HOME");
        const char* home = std::getenv("HOME");
        if (xdg && *xdg) candidates.push_back(fs::path(xdg) / "andub" / "shaders");
        else if (home && *home) candidates.push_back(fs::path(home) / ".local" / "share" / "andub" / "shaders");

#ifdef ANDUB_SOURCE_SHADER_DIR
        candidates.emplace_back(ANDUB_SOURCE_SHADER_DIR); // running straight from the build dir
#endif

        for (const auto& c : candidates) {
            std::error_code ec;
            if (fs::exists(c / "FSR.glsl", ec)) return fs::weakly_canonical(c, ec).string();
        }
        std::cerr << "[Upscaler] shader directory not found, upscaling unavailable\n";
        return std::string();
    }();
    return dir;
}

std::string Upscaler::get_saved_profile() {
    return PlayerService::get_setting("upscaler", "off");
}

void Upscaler::save_profile(const std::string& id) {
    PlayerService::set_setting("upscaler", id);
}

bool Upscaler::apply(mpv_handle* mpv, const UpscaleProfile& profile) {
    if (!mpv) return false;

    const char* clr[] = {"change-list", "glsl-shaders", "clr", "", nullptr};
    mpv_command(mpv, clr);

    bool ok = true;
    const std::string& dir = shader_dir();
    for (const auto& name : profile.shaders) {
        fs::path path = fs::path(dir) / name;
        std::error_code ec;
        if (dir.empty() || !fs::exists(path, ec)) {
            std::cerr << "[Upscaler] shader not found: " << path.string() << "\n";
            ok = false;
            continue;
        }
        // "append" takes a single path, so ':' / ';' inside it need no escaping
        std::string p = path.string();
        const char* add[] = {"change-list", "glsl-shaders", "append", p.c_str(), nullptr};
        mpv_command(mpv, add);
    }
    return ok;
}

} // namespace anime
