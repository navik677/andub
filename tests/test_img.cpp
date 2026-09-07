#include "services/image_cache.hpp"
#include <iostream>

int main() {
    std::string url = "https://cdn.shizaproject.com/images/a11bfffce97a7d1391092ae96ecc65c2c77b34e1.webp";
    std::string path = anime::ImageCache::ensure_image(url);
    std::cout << "Downloaded path: [" << path << "]\n";
    return 0;
}
