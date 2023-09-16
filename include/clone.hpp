#pragma once

#include <memory>

namespace Toolbox {

    class IClonable {
    public:
        virtual ~IClonable() = default;

        virtual std::unique_ptr<IClonable> clone(bool deep) const = 0;

        bool operator==(const IClonable &other) const { return true; }
    };

}  // namespace Toolbox