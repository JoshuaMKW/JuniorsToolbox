#pragma once

#include <imgui.h>

#include "gui/window.hpp"
#include "core/time/timestep.hpp"
#include "gui/image/imagepainter.hpp"
#include "fsystem.hpp"

namespace Toolbox::UI {
    class MeditWindow final : public ImWindow {
    public:
        MeditWindow(const std::string &name);
        ~MeditWindow() = default;
        bool onLoadData(fs_path data_path);
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

        bool openBMG(fs_path data_path);

        char m_search_buffer[256] = "";
        int m_start_frame_val = 0;
        int m_end_frame_val = 0;

        ImagePainter m_image_painter;
        std::unordered_map<std::string, RefPtr<const ImageHandle>> m_background_images;
        std::string m_selected_background;

        static const std::vector<std::pair<std::string, fs_path>> &BackgroundMap();

        enum Region {
            NTSCU,
            PAL,
        };
        Region m_region = NTSCU;
        int m_packet_size = 12;

        inline static const char MAGIC[9] = "MESGbmg1";
    };
}
