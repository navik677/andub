#pragma once

#include <memory>
#include <string>
#include <vector>
#include "../providers/base_provider.hpp"

namespace anime::app {

// The fixed, ordered set of catalog providers shared by every view.
class ProviderRegistry {
public:
    static const std::vector<std::shared_ptr<BaseProvider>>& all();
    static std::shared_ptr<BaseProvider> by_name(const std::string& name);
    static std::string display_name(const std::string& name);
};

} // namespace anime::app
