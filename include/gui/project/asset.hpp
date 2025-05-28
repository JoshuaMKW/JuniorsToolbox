#pragma once

#include "fsystem.hpp"
#include "gui/widget/widget.hpp"
#include "model/fsmodel.hpp"
#include "gui/image/imagepainter.hpp"

namespace Toolbox::UI {

    class ProjectAsset : public ImWidget {
    public:
        ProjectAsset() : ImWidget("Project Asset") {}
        ~ProjectAsset() = default;

        [[nodiscard]] virtual std::optional<ImVec2> defaultSize() const {
            return {
                {180, 180}
            };
        }
        [[nodiscard]] virtual std::optional<ImVec2> minSize() const {
            return {
                {180, 180}
            };
        }
        [[nodiscard]] virtual std::optional<ImVec2> maxSize() const {
            return {
                {180, 180}
            };
        }



        void setModelIndex(const ModelIndex &index) { m_index = index; }
        void setModel(RefPtr<FileSystemModel> model) { m_model = model; }

    protected:
        virtual void onRenderBody(TimeStep delta_time);

    private:
        ModelIndex m_index;
        RefPtr<FileSystemModel> m_model;
        ImagePainter m_painter;
    };

}  // namespace Toolbox::UI