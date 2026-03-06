#include "gui/selection.hpp"
#include "core/clipboard.hpp"
#include "core/input/input.hpp"
#include "core/input/keycode.hpp"

namespace Toolbox {

    ModelSelectionManager::ModelSelectionManager(RefPtr<IDataModel> model) {
        m_selection.setModel(model);
        model->addEventListener(getUUID(), TOOLBOX_BIND_EVENT_FN(updateSelection),
                                ModelEventFlags::EVENT_ANY);
    }

    ModelSelectionManager::~ModelSelectionManager() {
        RefPtr<IDataModel> model = m_selection.getModel();
        if (model) {
            model->removeEventListener(getUUID());
        }
    }

    ModelSelectionManager &ModelSelectionManager::operator=(ModelSelectionManager &&other) noexcept {
        RefPtr<IDataModel> model = other.m_selection.getModel();
        if (model) {
            // Update the event so memory references update to the new instance
            model->removeEventListener(other.m_uuid);
            model->addEventListener(other.m_uuid, TOOLBOX_BIND_EVENT_FN(updateSelection),
                                    ModelEventFlags::EVENT_ANY);
        }
        m_uuid      = std::move(other.m_uuid);
        m_selection = std::move(other.m_selection);
        return *this;
    }

    bool ModelSelectionManager::processDragState() {
        m_is_drag_state = false;

        if (m_drag_button == Input::MouseButton::BUTTON_NONE) {
            return false;
        }

        double x, y;
        Input::GetMousePosition(x, y);

        const bool is_left_held = Input::GetMouseButton(Input::MouseButton::BUTTON_LEFT);
        const bool is_right_held = Input::GetMouseButton(Input::MouseButton::BUTTON_RIGHT);
        const bool is_drag_context_valid =
            m_selection.count() > 0 && sqrt((x - m_drag_anchor_x) * (x - m_drag_anchor_x) +
                                            (y - m_drag_anchor_y) * (y - m_drag_anchor_y)) > 25.0;

        // Process interuption inputs
        switch (m_drag_button) {
        case Input::MouseButton::BUTTON_LEFT:
            if (is_right_held || !is_left_held) {
                m_drag_button = Input::MouseButton::BUTTON_NONE;
                m_drag_anchor_x = 0.0;
                m_drag_anchor_y = 0.0;
                return false;
            }
            break;
        case Input::MouseButton::BUTTON_RIGHT:
            if (is_left_held || !is_right_held) {
                m_drag_button   = Input::MouseButton::BUTTON_NONE;
                m_drag_anchor_x = 0.0;
                m_drag_anchor_y = 0.0;
                return false;
            }
            break;
        }

        m_is_drag_state = is_drag_context_valid;
        return is_drag_context_valid;
    }

    bool ModelSelectionManager::actionDeleteSelection() {
        RefPtr<IDataModel> model = m_selection.getModel();
        if (!model) {
            return false;
        }

        if (model->isReadOnly()) {
            return false;
        }

        bool result                                = true;
        const IDataModel::index_container &indexes = m_selection.getSelection();
        for (const ModelIndex &s : indexes) {
            result &= model->removeIndex(s);
        }

        m_selection.clearSelection();

        return result;
    }

    bool ModelSelectionManager::actionRenameSelection(const std::string &template_name) {
        RefPtr<IDataModel> model = m_selection.getModel();
        if (!model) {
            return false;
        }

        if (model->isReadOnly()) {
            return false;
        }

        bool result = true;
        for (const ModelIndex &s : m_selection.getSelection()) {
            result &= model->validateIndex(s);
            model->setDisplayText(s, template_name);
        }

        return result;
    }

    Result<void, BaseError> ModelSelectionManager::actionPasteIntoSelection(const MimeData &data) {
        RefPtr<IDataModel> model = m_selection.getModel();
        if (!model) {
            return make_error<void>("ModelSelectionManager", "Model is null!");
        }

        if (model->isReadOnly()) {
            return make_error<void>("ModelSelectionManager", "Model is read-only!");
        }

        ModelInsertPolicy policy = ModelInsertPolicy::INSERT_CHILD;

        ModelIndex last_selected = m_selection.getLastSelected();
        /*if (model->getRowCount(last_selected) > 0) {
            policy = ModelInsertPolicy::INSERT_CHILD;
        }*/

        //// We do this since we update the selection to the pasted items by event
        //m_selection.clearSelection();

        auto result = model->insertMimeData(last_selected, data, policy);
        if (!result) {
            return std::unexpected(result.error());
        }

        return {};
    }

    ScopePtr<MimeData> ModelSelectionManager::actionCutSelection() {
        ScopePtr<MimeData> data = actionCopySelection();
        if (!data) {
            return nullptr;
        }

        if (!actionDeleteSelection()) {
            TOOLBOX_ERROR("Failed to cut the selection!");
            return nullptr;
        }

        return data;
    }

    ScopePtr<MimeData> ModelSelectionManager::actionCopySelection() const {
        RefPtr<IDataModel> model = m_selection.getModel();
        if (!model) {
            return nullptr;
        }

        return model->createMimeData(m_selection.getSelection());
    }

    bool ModelSelectionManager::actionSelectIndex(const ModelIndex &index, bool force_single,
                                                  bool clear_on_mouse_up, bool no_span_selections) {
        RefPtr<IDataModel> model = m_selection.getModel();
        if (!model) {
            return false;
        }

        if (force_single) {
            m_selection.selectSingle(index);
            m_selection.setLastSelected(index);
            return true;
        }

        if (Input::GetKey(Input::KeyCode::KEY_LEFTCONTROL) ||
            Input::GetKey(Input::KeyCode::KEY_RIGHTCONTROL)) {
            if (m_selection.isSelected(index)) {
                m_selection.deselect(index);
            } else {
                m_selection.selectSingle(index, true);
            }
            m_selection.setLastSelected(index);
            return true;
        }

        if (!no_span_selections) {
            if (Input::GetKey(Input::KeyCode::KEY_LEFTSHIFT) ||
                Input::GetKey(Input::KeyCode::KEY_RIGHTSHIFT)) {
                if (model->validateIndex(m_selection.getLastSelected())) {
                    m_selection.selectSpan(m_selection.getLastSelected(), index, false,
                                           m_deep_spans);
                } else {
                    m_selection.selectSingle(index, clear_on_mouse_up);
                    m_selection.setLastSelected(index);
                }
                return true;
            }
        }

        m_selection.selectSingle(index, clear_on_mouse_up);
        m_selection.setLastSelected(index);
        return true;
    }

    bool ModelSelectionManager::actionSelectIndexIfNew(const ModelIndex &index) {
        RefPtr<IDataModel> model = m_selection.getModel();
        if (!model) {
            return false;
        }

        if (m_selection.isSelected(index)) {
            m_selection.setLastSelected(index);
            return false;
        }

        if (Input::GetKey(Input::KeyCode::KEY_LEFTCONTROL) ||
            Input::GetKey(Input::KeyCode::KEY_RIGHTCONTROL)) {
            m_selection.selectSingle(index, true);
            m_selection.setLastSelected(index);
            return true;
        }

        if (Input::GetKey(Input::KeyCode::KEY_LEFTSHIFT) ||
            Input::GetKey(Input::KeyCode::KEY_RIGHTSHIFT)) {
            if (model->validateIndex(m_selection.getLastSelected())) {
                m_selection.selectSpan(m_selection.getLastSelected(), index, false, m_deep_spans);
            } else {
                m_selection.selectSingle(index);
                m_selection.setLastSelected(index);
            }
            return true;
        }

        m_selection.selectSingle(index);
        m_selection.setLastSelected(index);
        return true;
    }

    bool ModelSelectionManager::actionClearRequestExcIndex(const ModelIndex &index,
                                                           bool is_left_button) {
        RefPtr<IDataModel> model = m_selection.getModel();
        if (!model) {
            return false;
        }

        if (Input::GetKey(Input::KeyCode::KEY_LEFTCONTROL) ||
            Input::GetKey(Input::KeyCode::KEY_RIGHTCONTROL)) {
            return true;
        }

        if (Input::GetKey(Input::KeyCode::KEY_LEFTSHIFT) ||
            Input::GetKey(Input::KeyCode::KEY_RIGHTSHIFT)) {
            return true;
        }

        if (is_left_button) {
            IDataModel::index_container selection_cpy = m_selection.getSelection();
            for (const ModelIndex &selected : selection_cpy) {
                if (selected != index) {
                    m_selection.deselect(selected);
                }
            }
        } else {
            if (!model->validateIndex(index)) {
                m_selection.clearSelection();
            }
            // m_selection.setLastSelected(ModelIndex());
        }
        return true;
    }

    bool ModelSelectionManager::handleActionsByMouseInput(const ModelIndex &index,
                                                          bool clear_on_mouse_up) {
        const bool is_left_click = Input::GetMouseButtonDown(Input::MouseButton::BUTTON_LEFT);
        const bool is_left_click_release = Input::GetMouseButtonUp(Input::MouseButton::BUTTON_LEFT);
        const bool is_right_click = Input::GetMouseButtonDown(Input::MouseButton::BUTTON_RIGHT);
        const bool is_right_click_release =
            Input::GetMouseButtonUp(Input::MouseButton::BUTTON_RIGHT);

        if (is_left_click) {
            m_drag_button = Input::MouseButton::BUTTON_LEFT;
            Input::GetMousePosition(m_drag_anchor_x, m_drag_anchor_y);
            return actionSelectIndex(index, false, clear_on_mouse_up);
        }

        if (is_right_click) {
            m_drag_button = Input::MouseButton::BUTTON_RIGHT;
            Input::GetMousePosition(m_drag_anchor_x, m_drag_anchor_y);
            return actionSelectIndexIfNew(index);
        }

        if (!m_is_drag_state) {
            if ((is_left_click_release || is_right_click_release) && clear_on_mouse_up) {
                return actionClearRequestExcIndex(index, is_left_click_release);
            }
        }

        return false;
    }

    void ModelSelectionManager::updateSelection(const ModelIndex &index, int flags) {
        if ((flags & ModelEventFlags::EVENT_INSERT) == ModelEventFlags::EVENT_INSERT) {
            if ((flags & ModelEventFlags::EVENT_PRE) == ModelEventFlags::EVENT_PRE) {
                // Clear the selection in preparation for the insert since
                // those will be selected instead.
                m_selection.clearSelection();
                m_insertion_state = true;
            }

            if ((flags & ModelEventFlags::EVENT_POST) == ModelEventFlags::EVENT_POST) {
                m_insertion_state = false;
            }

            return;
        }

        if ((flags & ModelEventFlags::EVENT_INDEX_ADDED) == ModelEventFlags::EVENT_INDEX_ADDED) {
            if ((flags & ModelEventFlags::EVENT_POST) == ModelEventFlags::EVENT_POST) {
                if ((flags & ModelEventFlags::EVENT_SUCCESS) == ModelEventFlags::EVENT_NONE) {
                    return;
                }

                if (m_insertion_state) {
                    m_selection.selectSingle(index, true);
                    m_selection.setLastSelected(index);
                }
            }

            return;
        }
    }

}  // namespace Toolbox