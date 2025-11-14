#pragma once

#include <functional>
#include <unordered_set>

#include "core/input/input.hpp"
#include "model/model.hpp"
#include "model/selection.hpp"

namespace Toolbox {

    class ModelSelectionManager : IUnique {
    public:
        ModelSelectionManager(RefPtr<IDataModel> model);

    public:
        ModelSelectionManager()                                  = default;
        ModelSelectionManager(const ModelSelectionManager &)     = delete;
        ModelSelectionManager(ModelSelectionManager &&) noexcept = default;
        ~ModelSelectionManager();

        ModelSelectionManager &operator=(ModelSelectionManager &&other) noexcept {
            RefPtr<IDataModel> model = other.m_selection.getModel();
            if (model) {
                // Update the event so memory references update to the new instance
                model->removeEventListener(other.m_uuid);
                model->addEventListener(other.m_uuid,
                                        TOOLBOX_BIND_EVENT_FN(updateSelectionOnInsert),
                                        ModelEventFlags::EVENT_INSERT);
            }
            m_uuid      = std::move(other.m_uuid);
            m_selection = std::move(other.m_selection);
            return *this;
        }

    public:
        UUID64 getUUID() const override { return m_uuid; }

        ModelSelectionState &getState() { return m_selection; }
        const ModelSelectionState &getState() const { return m_selection; }

        bool isDragState() const { return m_is_drag_state; }
        bool processDragState();

        void setDeepSpans(bool spans_are_deep) { m_deep_spans = spans_are_deep; }

        bool actionDeleteSelection();
        bool actionRenameSelection(const std::string &template_name);
        bool actionPasteIntoSelection(const MimeData &data);
        ScopePtr<MimeData> actionCutSelection();
        ScopePtr<MimeData> actionCopySelection() const;

        bool actionSelectIndex(const ModelIndex &index, bool force_single = false,
                               bool clear_on_mouse_up = false);

        bool actionSelectIndexIfNew(const ModelIndex &index);
        bool actionClearRequestExcIndex(const ModelIndex &index, bool is_left_button);

        bool handleActionsByMouseInput(const ModelIndex &index, bool clear_on_mouse_up = false);

    protected:
        void updateSelectionOnInsert(const ModelIndex &index, int flags);

    private:
        UUID64 m_uuid;
        ModelSelectionState m_selection;

        Input::MouseButton m_drag_button;
        double m_drag_anchor_x = 0.0f;
        double m_drag_anchor_y = 0.0f;
        bool m_is_drag_state   = false;
        
        bool m_deep_spans      = true;
    };

}  // namespace Toolbox