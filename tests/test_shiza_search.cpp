#include "providers/shizaproject_provider.hpp"
#include <iostream>

int main() {
    anime::ShizaProjectProvider p;
    auto res = p.search("", 25);
    std::cout << "Shiza count: " << res.size() << "\n";
    for (const auto& a : res) {
        std::cout << a.title_ru << " -> poster_url: [" << a.poster_url << "]\n";
    }
    return 0;
}
