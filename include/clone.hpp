#pragma once

#include <memory>

namespace Toolbox {

    class IClonable {
    public:
        virtual ~IClonable() = default;

        virtual std::unique_ptr<IClonable> clone(bool deep) const = 0;
    };

    template <typename T> static inline std::shared_ptr<T> make_clone(const IClonable &ptr) {
        return std::static_pointer_cast<T, IClonable>(ptr.clone(false));
    }

    template <typename T>
    static inline std::shared_ptr<T> make_clone(const std::shared_ptr<IClonable> &ptr) {
        return std::static_pointer_cast<T, IClonable>(ptr->clone(false));
    }

    template <typename T> static inline std::shared_ptr<T> make_deep_clone(const IClonable &ptr) {
        return std::static_pointer_cast<T, IClonable>(ptr.clone(true));
    }

    template <typename T>
    static inline std::shared_ptr<T> make_deep_clone(const std::shared_ptr<IClonable> &ptr) {
        return std::static_pointer_cast<T, IClonable>(ptr->clone(true));
    }

}  // namespace Toolbox