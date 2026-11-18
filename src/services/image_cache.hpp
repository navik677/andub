#pragma once

#include <string>

namespace anime {

// Disk cache for poster images. Blurred backdrops are produced on the GPU by
// the UI (MultiEffect), so only the original artwork is stored here.
class ImageCache {
public:
    static std::string get_cache_dir();
    static std::string get_cached_path(const std::string& url);
    // Downloads `url` into the cache if needed; returns the local path or "".
    static std::string ensure_image(const std::string& url);
};

} // namespace anime
