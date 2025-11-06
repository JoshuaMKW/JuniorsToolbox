#pragma once

#include "core/memory.hpp"

namespace Toolbox {

    // ISmartResource provides std::enable_shared_from_this
    // necessarily, due to the needs of preserving the shared
    // functionality through the clone process.
    class ISmartResource : public Referable<ISmartResource> {
    public:
        virtual ~ISmartResource() = default;

        virtual ScopePtr<ISmartResource> clone(bool deep) const = 0;

        RefPtr<ISmartResource> ptr() { return shared_from_this(); }
    };

    template <typename _T = ISmartResource, typename _DT = std::decay_t<_T>>
    static inline RefPtr<_DT> get_shared_ptr(_T &_V) {
        static_assert(std::is_base_of_v<ISmartResource, _DT> | std::is_same_v<ISmartResource, _DT>,
                      "_T must be IClonable or a derivative of IClonable");
        return std::static_pointer_cast<_DT, ISmartResource>(_V.ptr());
    }

    template <typename _T = ISmartResource, typename _DT = std::decay_t<_T>>
    static inline ScopePtr<_T> make_clone(const _T &_V) {
        static_assert(std::is_base_of_v<ISmartResource, _DT> | std::is_same_v<ISmartResource, _DT>,
                      "_T must be IClonable or a derivative of IClonable");
        return ScopePtr<_DT>(static_cast<_DT *>(_V.clone(false).release()));
    }

    template <typename _T = ISmartResource, typename _DT = std::decay_t<_T>>
    static inline ScopePtr<_T> make_clone(const RefPtr<_T> &ptr) {
        static_assert(std::is_base_of_v<ISmartResource, _DT> | std::is_same_v<ISmartResource, _DT>,
                      "_T must be IClonable or a derivative of IClonable");
        return ScopePtr<_DT>(static_cast<_DT *>(ptr->clone(false).release()));
    }

    template <typename _T = ISmartResource, typename _DT = std::decay_t<_T>>
    static inline ScopePtr<_T> make_deep_clone(const _T &_V) {
        static_assert(std::is_base_of_v<ISmartResource, _DT> | std::is_same_v<ISmartResource, _DT>,
                      "_T must be IClonable or a derivative of IClonable");
        return ScopePtr<_DT>(static_cast<_DT *>(_V.clone(true).release()));
    }

    template <typename _T = ISmartResource, typename _DT = std::decay_t<_T>>
    static inline ScopePtr<_T> make_deep_clone(const RefPtr<_T> &ptr) {
        static_assert(std::is_base_of_v<ISmartResource, _DT> | std::is_same_v<ISmartResource, _DT>,
                      "_T must be IClonable or a derivative of IClonable");
        return ScopePtr<_DT>(static_cast<_DT *>(ptr->clone(true).release()));
    }

}  // namespace Toolbox