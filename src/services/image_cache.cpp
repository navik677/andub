#include "image_cache.hpp"
#include "../utils/http_client.hpp"
#include <cstdlib>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <vector>
#include <algorithm>
#include <gdk-pixbuf/gdk-pixbuf.h>

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

static void box_blur_horizontal(const unsigned char* in, unsigned char* out, int w, int h, int stride, int r) {
    float inv = 1.0f / (2 * r + 1);
    for (int y = 0; y < h; ++y) {
        const unsigned char* row_in = in + y * stride;
        unsigned char* row_out = out + y * stride;
        for (int c = 0; c < 3; ++c) {
            int sum = 0;
            for (int i = -r; i <= r; ++i) {
                int px = std::clamp(i, 0, w - 1);
                sum += row_in[px * 4 + c];
            }
            row_out[0 * 4 + c] = static_cast<unsigned char>(sum * inv);
            for (int x = 1; x < w; ++x) {
                int add_px = std::clamp(x + r, 0, w - 1);
                int sub_px = std::clamp(x - r - 1, 0, w - 1);
                sum += row_in[add_px * 4 + c] - row_in[sub_px * 4 + c];
                row_out[x * 4 + c] = static_cast<unsigned char>(sum * inv);
            }
        }
        for (int x = 0; x < w; ++x) {
            row_out[x * 4 + 3] = row_in[x * 4 + 3];
        }
    }
}

static void box_blur_vertical(const unsigned char* in, unsigned char* out, int w, int h, int stride, int r) {
    float inv = 1.0f / (2 * r + 1);
    for (int x = 0; x < w; ++x) {
        for (int c = 0; c < 3; ++c) {
            int sum = 0;
            for (int i = -r; i <= r; ++i) {
                int py = std::clamp(i, 0, h - 1);
                sum += in[py * stride + x * 4 + c];
            }
            out[0 * stride + x * 4 + c] = static_cast<unsigned char>(sum * inv);
            for (int y = 1; y < h; ++y) {
                int add_py = std::clamp(y + r, 0, h - 1);
                int sub_py = std::clamp(y - r - 1, 0, h - 1);
                sum += in[add_py * stride + x * 4 + c] - in[sub_py * stride + x * 4 + c];
                out[y * stride + x * 4 + c] = static_cast<unsigned char>(sum * inv);
            }
        }
        for (int y = 0; y < h; ++y) {
            out[y * stride + x * 4 + 3] = in[y * stride + x * 4 + 3];
        }
    }
}

std::string ImageCache::ensure_blurred_backdrop(const std::string& url) {
    if (url.empty()) return "";
    std::string orig_path = ensure_image(url);
    if (orig_path.empty()) return "";

    std::string hash = simple_hash(url);
    std::string blur_path = get_cache_dir() + "/" + hash + "_backdrop.png";
    if (std::filesystem::exists(blur_path) && std::filesystem::file_size(blur_path) > 0) {
        return blur_path;
    }

    GError* err = nullptr;
    GdkPixbuf* src = gdk_pixbuf_new_from_file(orig_path.c_str(), &err);
    if (!src) {
        if (err) g_error_free(err);
        return "";
    }

    // Scale to a panoramic resolution (480x240)
    int target_w = 480;
    int target_h = 240;
    GdkPixbuf* scaled = gdk_pixbuf_scale_simple(src, target_w, target_h, GDK_INTERP_BILINEAR);
    g_object_unref(src);
    if (!scaled) return "";

    // Ensure has alpha channel
    GdkPixbuf* rgba = gdk_pixbuf_add_alpha(scaled, FALSE, 0, 0, 0);
    g_object_unref(scaled);
    if (!rgba) return "";

    int stride = gdk_pixbuf_get_rowstride(rgba);
    unsigned char* pixels = gdk_pixbuf_get_pixels(rgba);
    std::vector<unsigned char> temp(stride * target_h);

    // Multi-pass box blur for smooth, cinematic blur
    int r = 14;
    box_blur_horizontal(pixels, temp.data(), target_w, target_h, stride, r);
    box_blur_vertical(temp.data(), pixels, target_w, target_h, stride, r);
    box_blur_horizontal(pixels, temp.data(), target_w, target_h, stride, r);
    box_blur_vertical(temp.data(), pixels, target_w, target_h, stride, r);

    // Apply smooth vertical vignette fade:
    // Top is visible, bottom fades cleanly to 0% alpha so it dissolves into dark window
    for (int y = 0; y < target_h; ++y) {
        float t = static_cast<float>(y) / target_h;
        float alpha_factor = (1.0f - t);
        alpha_factor = alpha_factor * alpha_factor;
        unsigned char* row = pixels + y * stride;
        for (int x = 0; x < target_w; ++x) {
            row[x * 4 + 0] = static_cast<unsigned char>(row[x * 4 + 0] * 0.70f);
            row[x * 4 + 1] = static_cast<unsigned char>(row[x * 4 + 1] * 0.70f);
            row[x * 4 + 2] = static_cast<unsigned char>(row[x * 4 + 2] * 0.70f);
            row[x * 4 + 3] = static_cast<unsigned char>(std::clamp(static_cast<int>(255 * alpha_factor * 0.75f), 0, 255));
        }
    }

    err = nullptr;
    gdk_pixbuf_save(rgba, blur_path.c_str(), "png", &err, NULL);
    g_object_unref(rgba);
    if (err) {
        g_error_free(err);
        return "";
    }

    return blur_path;
}

std::future<std::string> ImageCache::ensure_image_async(std::string url) {
    return std::async(std::launch::async, [u = std::move(url)]() {
        return ensure_image(u);
    });
}

} // namespace anime
