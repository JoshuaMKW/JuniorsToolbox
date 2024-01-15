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

    template <typename _T = ISmartResource, typename _DT = std::decay_t<_T>>
    static inline std::shared_ptr<_DT> get_shared_ptr(_T &_V) {
        static_assert(std::is_base_of_v<ISmartResource, _DT> | std::is_same_v<ISmartResource, _DT>,
                      "_T must be IClonable or a derivative of IClonable");
        return std::static_pointer_cast<_DT, ISmartResource>(_V.ptr());
    }

    template <typename _T = ISmartResource, typename _DT = std::decay_t<_T>>
    static inline std::shared_ptr<_T> make_clone(const _T &_V) {
        static_assert(std::is_base_of_v<ISmartResource, _DT> | std::is_same_v<ISmartResource, _DT>,
                      "_T must be IClonable or a derivative of IClonable");
        return std::static_pointer_cast<_DT, ISmartResource>(_V.clone(false));
    }

    template <typename _T = ISmartResource, typename _DT = std::decay_t<_T>>
    static inline std::shared_ptr<_T> make_clone(const std::shared_ptr<_T> &ptr) {
        static_assert(std::is_base_of_v<ISmartResource, _DT> | std::is_same_v<ISmartResource, _DT>,
                      "_T must be IClonable or a derivative of IClonable");
        return std::static_pointer_cast<_DT, ISmartResource>(ptr->clone(false));
    }

    template <typename _T = ISmartResource, typename _DT = std::decay_t<_T>>
    static inline std::shared_ptr<_T> make_deep_clone(const _T &_V) {
        static_assert(std::is_base_of_v<ISmartResource, _DT> | std::is_same_v<ISmartResource, _DT>,
                      "_T must be IClonable or a derivative of IClonable");
        return std::static_pointer_cast<_DT, ISmartResource>(_V.clone(true));
    }

    template <typename _T = ISmartResource, typename _DT = std::decay_t<_T>>
    static inline std::shared_ptr<_T> make_deep_clone(const std::shared_ptr<_T> &ptr) {
        static_assert(std::is_base_of_v<ISmartResource, _DT> | std::is_same_v<ISmartResource, _DT>,
                      "_T must be IClonable or a derivative of IClonable");
        return std::static_pointer_cast<_DT, ISmartResource>(ptr->clone(true));
    }

}  // namespace Toolbox