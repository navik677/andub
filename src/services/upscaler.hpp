#pragma once

#include <string>
#include <vector>

struct mpv_handle;

namespace anime {

// Real-time GPU upscaling for the embedded player: GLSL shader chains
// (FSR 1.0, FSRCNNX, Anime4K) from mpv-upscaler, applied via glsl-shaders.
struct UpscaleProfile {
    std::string id;
    std::string name;
    std::vector<std::string> shaders;
};

class Upscaler {
public:
    static const std::vector<UpscaleProfile>& profiles();
    static size_t index_of(const std::string& id);

    // Directory holding the bundled .glsl files, or empty if none was found.
    static const std::string& shader_dir();

    // Persisted choice (settings.json "upscaler"), "off" by default.
    static std::string get_saved_profile();
    static void save_profile(const std::string& id);

    // Replaces mpv's glsl-shaders with the profile's chain. Returns false if
    // any shader file was missing.
    static bool apply(mpv_handle* mpv, const UpscaleProfile& profile);
};

} // namespace anime
