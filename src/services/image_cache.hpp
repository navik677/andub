#pragma once

#include <string>
#include <future>
#include <filesystem>

namespace anime {

class ImageCache {
public:
    static std::string get_cache_dir();
    static std::string get_cached_path(const std::string& url);
    static std::string ensure_image(const std::string& url);
    static std::future<std::string> ensure_image_async(std::string url);
};

} // namespace anime
