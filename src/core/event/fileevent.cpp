#include "core/event/fileevent.hpp"

namespace Toolbox {

    FileEvent::FileEvent(const UUID64 &target_id, const std::filesystem::path &path, Action action)
        : BaseEvent(target_id, EVENT_SHORTCUT), m_path(path), m_action(action) {}

    ScopePtr<ISmartResource> FileEvent::clone(bool deep) const {
        return std::make_unique<FileEvent>(*this);
    }

}  // namespace Toolbox