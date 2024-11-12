#pragma once

#include <filesystem>

#include <imgui.h>

#include "gui/window.hpp"
#include "core/time/timestep.hpp"

namespace Toolbox::UI {
    class BMGWindow final : public ImWindow {
    public:
        BMGWindow(const std::string &name);
        ~BMGWindow() = default;
        bool onLoadData(std::filesystem::path data_path);
    protected:
        void onRenderMenuBar() override;
        void onRenderBody(TimeStep delta_time) override;
    };
}
