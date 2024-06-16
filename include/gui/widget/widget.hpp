#pragma once

#include <functional>
#include <map>
#include <string>
#include <vector>

#include "gui/layer/imlayer.hpp"

namespace Toolbox::UI {

    class ImWidget : public ImProcessLayer {
    protected:
        virtual void onRenderBody(TimeStep delta_time) {}

    public:
        explicit ImWidget(const std::string &name) : ImProcessLayer(name) {}
        ImWidget(const std::string &name, std::optional<ImVec2> default_size)
            : ImProcessLayer(name), m_default_size(default_size) {}
        ImWidget(const std::string &name, std::optional<ImVec2> min_size,
                 std::optional<ImVec2> max_size)
            : ImProcessLayer(name), m_min_size(min_size), m_max_size(max_size) {}
        ImWidget(const std::string &name, std::optional<ImVec2> default_size,
                 std::optional<ImVec2> min_size, std::optional<ImVec2> max_size)
            : ImProcessLayer(name), m_default_size(default_size), m_min_size(min_size),
              m_max_size(max_size) {}

        virtual ~ImWidget() = default;

        [[nodiscard]] virtual ImWidget *parent() const { return m_parent; }
        virtual void setParent(ImWidget *parent) { m_parent = parent; }

        [[nodiscard]] virtual std::optional<ImVec2> defaultSize() const { return m_default_size; }
        [[nodiscard]] virtual std::optional<ImVec2> minSize() const { return m_min_size; }
        [[nodiscard]] virtual std::optional<ImVec2> maxSize() const { return m_max_size; }

        void defocus();
        void focus();

        void onAttach() override;
        void onImGuiRender(TimeStep delta_time) override final;
        void onWindowEvent(RefPtr<WindowEvent> ev) override;

        [[nodiscard]] std::string title() const { return name(); }

    protected:
        UUID64 m_UUID64;
        ImGuiID m_sibling_id = 0;

        ImWidget *m_parent = nullptr;

        ImGuiViewport *m_viewport               = nullptr;
        ImGuiWindowFlags m_flags                = 0;
        mutable ImGuiWindowClass m_window_class = ImGuiWindowClass();

        std::optional<ImVec2> m_default_size = {};
        std::optional<ImVec2> m_min_size     = {};
        std::optional<ImVec2> m_max_size     = {};

    private:
        ImGuiID m_dockspace_id   = std::numeric_limits<ImGuiID>::max();
        bool m_is_docking_set_up = false;
    };

    inline std::string ImWidgetComponentTitle(const ImWidget &window_layer,
                                              const std::string &component_name) {
        return std::format("{}##{}", component_name, window_layer.getUUID());
    }

}  // namespace Toolbox::UI