#include "image_cache.hpp"
#include "../utils/http_client.hpp"
#include <cstdlib>
#include <sstream>
#include <iomanip>

namespace anime {

static std::string simple_hash(const std::string& str) {
    size_t h = 5381;
    for (char c : str) {
        h = ((h << 5) + h) + static_cast<unsigned char>(c);
    }
    std::ostringstream ss;
    ss << std::hex << h;
    return ss.str();
}

std::string ImageCache::get_cache_dir() {
    const char* xdg = std::getenv("XDG_CACHE_HOME");
    const char* home = std::getenv("HOME");
    std::string base;
    if (xdg && *xdg) base = xdg;
    else if (home && *home) base = std::string(home) + "/.cache";
    else base = "/tmp";

    std::string dir = base + "/anime-gui/covers";
    try {
        std::filesystem::create_directories(dir);
    } catch (...) {
        dir = "/tmp/anime-gui/covers";
        try { std::filesystem::create_directories(dir); } catch (...) {}
    }
    return dir;
}

std::string ImageCache::get_cached_path(const std::string& url) {
    if (url.empty()) return "";
    std::string hash = simple_hash(url);
    std::string ext = ".jpg";
    if (url.find(".png") != std::string::npos) ext = ".png";
    else if (url.find(".webp") != std::string::npos) ext = ".webp";
    return get_cache_dir() + "/" + hash + ext;
}

std::string ImageCache::ensure_image(const std::string& url) {
    if (url.empty()) return "";
    std::string path = get_cached_path(url);
    if (std::filesystem::exists(path) && std::filesystem::file_size(path) > 0) {
        return path;
    }

    if (http::Client::download_file(url, path, 15)) {
        return path;
    }
    return "";
}

std::future<std::string> ImageCache::ensure_image_async(std::string url) {
    return std::async(std::launch::async, [u = std::move(url)]() {
        return ensure_image(u);
    });
}

} // namespace anime
