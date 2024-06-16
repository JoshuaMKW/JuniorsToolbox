#pragma once

#include "core/event/event.hpp"
#include "fsystem.hpp"

#include <filesystem>

namespace Toolbox {

    class FileEvent : public BaseEvent {
    private:
        FileEvent() = default;

    public:
        enum class Action {
            ACTION_LOAD,
            ACTION_SAVE,
        };

        FileEvent(const FileEvent &)     = default;
        FileEvent(FileEvent &&) noexcept = default;

        FileEvent(const UUID64 &target_id, const std::filesystem::path &path, Action action);

        [[nodiscard]] std::filesystem::path getPath() const noexcept { return m_path; }
        [[nodiscard]] bool isLoad() const noexcept { return m_action == Action::ACTION_LOAD; }
        [[nodiscard]] bool isSave() const noexcept { return m_action == Action::ACTION_SAVE; }

        ScopePtr<ISmartResource> clone(bool deep) const override;

        FileEvent &operator=(const FileEvent &)     = default;
        FileEvent &operator=(FileEvent &&) noexcept = default;

    private:
        std::filesystem::path m_path;
        Action m_action;
    };

}  // namespace Toolbox