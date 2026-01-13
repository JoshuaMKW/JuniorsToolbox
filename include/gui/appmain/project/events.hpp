#pragma once

#include "core/event/event.hpp"
#include "fsystem.hpp"
#include "gui/appmain/project/rarc_processor.hpp"

namespace Toolbox::UI {

    enum EProjectEvent : Toolbox::BaseEvent::TypeID {
        PROJECT_PACK_DIRECTORY = Toolbox::BaseEvent::EVENT_USER_BEGIN + 100,
        PROJECT_UNPACK_DIRECTORY,
    };

    class ProjectPackEvent : public Toolbox::BaseEvent {
    private:
        ProjectPackEvent() = default;

    public:
        ProjectPackEvent(const ProjectPackEvent &)     = default;
        ProjectPackEvent(ProjectPackEvent &&) noexcept = default;

        ProjectPackEvent(const UUID64 &target_id, const fs_path &path, bool compress, RarcProcessor::task_cb on_complete = nullptr);

        [[nodiscard]] const fs_path &getPath() const noexcept { return m_target_dir; }
        [[nodiscard]] bool wantsCompress() const noexcept { return m_compress; }
        [[nodiscard]] RarcProcessor::task_cb cb() const noexcept { return m_cb; }

        ScopePtr<ISmartResource> clone(bool deep) const override;

        ProjectPackEvent &operator=(const ProjectPackEvent &)     = default;
        ProjectPackEvent &operator=(ProjectPackEvent &&) noexcept = default;

    private:
        fs_path m_target_dir;
        RarcProcessor::task_cb m_cb;
        bool m_compress;
    };

    class ProjectUnpackEvent : public Toolbox::BaseEvent {
    private:
        ProjectUnpackEvent() = default;

    public:
        ProjectUnpackEvent(const ProjectUnpackEvent &)     = default;
        ProjectUnpackEvent(ProjectUnpackEvent &&) noexcept = default;

        ProjectUnpackEvent(const UUID64 &target_id, const fs_path &path,
                           RarcProcessor::task_cb on_complete = nullptr);

        [[nodiscard]] const fs_path &getPath() const noexcept { return m_target_pack; }
        [[nodiscard]] RarcProcessor::task_cb cb() const noexcept { return m_cb; }

        ScopePtr<ISmartResource> clone(bool deep) const override;

        ProjectUnpackEvent &operator=(const ProjectUnpackEvent &)     = default;
        ProjectUnpackEvent &operator=(ProjectUnpackEvent &&) noexcept = default;

    private:
        fs_path m_target_pack;
        RarcProcessor::task_cb m_cb;
    };

}