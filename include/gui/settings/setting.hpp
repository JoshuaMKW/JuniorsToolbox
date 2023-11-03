#pragma once

#include <string>
#include <string_view>

namespace Toolbox::UI {

    class ISetting {
    public:
        virtual ~ISetting() = default;

        virtual std::string_view name() = 0;
        virtual void render()           = 0;
    };

}  // namespace Toolbox::UI