#pragma once

#include <memory>

namespace Toolbox {

    // IClonable provides std::enable_shared_from_this
    // necessarily, due to the needs of preserving the shared
    // functionality through the clone process.
    class IClonable : public std::enable_shared_from_this<IClonable> {
    public:
        virtual ~IClonable() = default;

        virtual std::unique_ptr<IClonable> clone(bool deep) const = 0;

        std::shared_ptr<IClonable> ptr() { return shared_from_this(); }
    };

    template <typename T> static inline std::shared_ptr<T> get_shared_ptr(IClonable& _V) {
        static_assert(std::is_base_of_v<IClonable, T> || std::is_same_v<IClonable, T>,
                      "T must be IClonable or a derivative of IClonable");
        return std::static_pointer_cast<T, IClonable>(_V.ptr());
    }

    template <typename T> static inline std::shared_ptr<T> make_clone(const IClonable &_V) {
        static_assert(std::is_base_of_v<IClonable, T> || std::is_same_v<IClonable, T>,
                      "T must be IClonable or a derivative of IClonable");
        return std::static_pointer_cast<T, IClonable>(_V.clone(false));
    }

    template <typename T>
    static inline std::shared_ptr<T> make_clone(const std::shared_ptr<IClonable> &ptr) {
        static_assert(std::is_base_of_v<IClonable, T> || std::is_same_v<IClonable, T>,
                      "T must be IClonable or a derivative of IClonable");
        return std::static_pointer_cast<T, IClonable>(ptr->clone(false));
    }

    template <typename T> static inline std::shared_ptr<T> make_deep_clone(const IClonable &_V) {
        static_assert(std::is_base_of_v<IClonable, T> || std::is_same_v<IClonable, T>,
                      "T must be IClonable or a derivative of IClonable");
        return std::static_pointer_cast<T, IClonable>(_V.clone(true));
    }

    template <typename T>
    static inline std::shared_ptr<T> make_deep_clone(const std::shared_ptr<IClonable> &ptr) {
        static_assert(std::is_base_of_v<IClonable, T> || std::is_same_v<IClonable, T>,
                      "T must be IClonable or a derivative of IClonable");
        return std::static_pointer_cast<T, IClonable>(ptr->clone(true));
    }

}  // namespace Toolbox