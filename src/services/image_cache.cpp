#include "image_cache.hpp"
#include "../utils/http_client.hpp"
#include "../utils/paths.hpp"
#include <filesystem>
#include <mutex>
#include <set>
#include <condition_variable>
#include <sstream>

namespace anime {

namespace fs = std::filesystem;

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
    static const std::string dir = [] {
        fs::path p = fs::path(paths::cache_dir()) / "covers";
        std::error_code ec;
        fs::create_directories(p, ec);
        return p.string();
    }();
    return dir;
}

std::string ImageCache::get_cached_path(const std::string& url) {
    if (url.empty()) return "";
    std::string ext = ".jpg";
    if (url.find(".png") != std::string::npos) ext = ".png";
    else if (url.find(".webp") != std::string::npos) ext = ".webp";
    return (fs::path(get_cache_dir()) / (simple_hash(url) + ext)).string();
}

std::string ImageCache::ensure_image(const std::string& url) {
    if (url.empty()) return "";
    const std::string path = get_cached_path(url);

    // The same poster is often requested by several views at once (grid card,
    // details hero, backdrop). Let one thread download it while the rest wait.
    static std::mutex mtx;
    static std::condition_variable cv;
    static std::set<std::string> in_flight;

    std::unique_lock lock(mtx);
    cv.wait(lock, [&] { return !in_flight.count(path); });

    std::error_code ec;
    if (fs::exists(path, ec) && fs::file_size(path, ec) > 0) return path;

    in_flight.insert(path);
    lock.unlock();

    const std::string tmp = path + ".part";
    bool ok = http::Client::download_file(url, tmp, 15);
    if (ok) fs::rename(tmp, path, ec);
    if (!ok || ec) fs::remove(tmp, ec);

    lock.lock();
    in_flight.erase(path);
    cv.notify_all();
    return (ok && fs::exists(path, ec)) ? path : "";
}

} // namespace anime
