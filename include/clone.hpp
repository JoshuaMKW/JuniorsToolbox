#pragma once

#include <memory>

namespace Toolbox {

    // ISmartResource provides std::enable_shared_from_this
    // necessarily, due to the needs of preserving the shared
    // functionality through the clone process.
    class ISmartResource : public std::enable_shared_from_this<ISmartResource> {
    public:
        virtual ~ISmartResource() = default;

        virtual std::unique_ptr<ISmartResource> clone(bool deep) const = 0;

        std::shared_ptr<ISmartResource> ptr() { return shared_from_this(); }
    };

    template <typename T> static inline std::shared_ptr<T> get_shared_ptr(ISmartResource& _V) {
        static_assert(std::is_base_of_v<ISmartResource, T> || std::is_same_v<ISmartResource, T>,
                      "T must be IClonable or a derivative of IClonable");
        return std::static_pointer_cast<T, ISmartResource>(_V.ptr());
    }

    template <typename T> static inline std::shared_ptr<T> make_clone(const ISmartResource &_V) {
        static_assert(std::is_base_of_v<ISmartResource, T> || std::is_same_v<ISmartResource, T>,
                      "T must be IClonable or a derivative of IClonable");
        return std::static_pointer_cast<T, ISmartResource>(_V.clone(false));
    }

    template <typename T>
    static inline std::shared_ptr<T> make_clone(const std::shared_ptr<ISmartResource> &ptr) {
        static_assert(std::is_base_of_v<ISmartResource, T> || std::is_same_v<ISmartResource, T>,
                      "T must be IClonable or a derivative of IClonable");
        return std::static_pointer_cast<T, ISmartResource>(ptr->clone(false));
    }

    template <typename T> static inline std::shared_ptr<T> make_deep_clone(const ISmartResource &_V) {
        static_assert(std::is_base_of_v<ISmartResource, T> || std::is_same_v<ISmartResource, T>,
                      "T must be IClonable or a derivative of IClonable");
        return std::static_pointer_cast<T, ISmartResource>(_V.clone(true));
    }

    template <typename T>
    static inline std::shared_ptr<T> make_deep_clone(const std::shared_ptr<ISmartResource> &ptr) {
        static_assert(std::is_base_of_v<ISmartResource, T> || std::is_same_v<ISmartResource, T>,
                      "T must be IClonable or a derivative of IClonable");
        return std::static_pointer_cast<T, ISmartResource>(ptr->clone(true));
    }

}  // namespace Toolbox