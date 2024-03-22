#pragma once

#include "core/application/layer.hpp"
#include "core/event/event.hpp"

namespace Toolbox::UI {

    class ImLayer : public Core::Layer {
    public:
        ImLayer();
        ~ImLayer() = default;

        void onAttach() override;
        void onDetach() override;
        void onUpdate() override;
        void onEvent(Core::BaseEvent &event) override;

    protected:
        void onImGuiRender();
    }