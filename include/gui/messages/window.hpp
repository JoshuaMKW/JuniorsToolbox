#pragma once

#include <filesystem>

#include <imgui.h>

#include "gui/window.hpp"
#include "core/time/timestep.hpp"
#include "gui/image/imagepainter.hpp"

namespace Toolbox::UI {
    class BMGWindow final : public ImWindow {
    public:
        BMGWindow(const std::string &name);
        ~BMGWindow() = default;
        bool onLoadData(std::filesystem::path data_path);
        std::optional<ImVec2> minSize() const override {
            return {
                {710, 400}
            };
        }
        ImGuiWindowFlags flags() const override {
            return ImWindow::flags() | ImGuiWindowFlags_MenuBar;
        }
    protected:
        void onRenderMenuBar() override;
        void onRenderBody(TimeStep delta_time) override;
    private:
        void renderIndexPanel();
        void renderSoundFrame();
        void renderBackgroundPanel();
        void renderDialogText();
        void renderDialogMockup();

        char m_search_buffer[256] = "";
        int m_start_frame_val = 0;
        int m_end_frame_val = 0;

        ImagePainter m_image_painter;
        RefPtr<const ImageHandle> m_background_image;
    };
}
