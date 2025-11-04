#include "model/selection.hpp"
#include "core/clipboard.hpp"
#include "core/input/input.hpp"
#include "core/input/keycode.hpp"

namespace Toolbox {

    void ModelSelectionState::setLastSelected(const ModelIndex &index) {
        u64 mu          = index.getModelUUID();
      m_last_selected = index;
    }

    bool ModelSelectionState::deselect(const ModelIndex &index) {
        if (!m_ref_model) {
            return false;
        }

        if (!m_ref_model->validateIndex(index)) {
            return false;
        }

        return std::erase_if(m_selection, [&](const ModelIndex &it) { return index == it; }) > 0;
    }

    bool ModelSelectionState::selectSingle(const ModelIndex &index, bool additive) {
        if (!m_ref_model) {
            return false;
        }

        if (!m_ref_model->validateIndex(index)) {
            return false;
        }

        if (!additive) {
            clearSelection();
        }

        m_selection.emplace_back(index);
        return true;
    }

    bool ModelSelectionState::selectSpan(const ModelIndex &a, const ModelIndex &b, bool additive,
                                         bool deep) {
        if (!m_ref_model) {
            return false;
        }

        if (!m_ref_model->validateIndex(a) || !m_ref_model->validateIndex(b)) {
            return false;
        }

        if (!additive) {
            m_selection.clear();
        }

        if (a == b) {
            m_selection.emplace_back(a);
            return true;
        }

        ModelIndex this_a = a;
        ModelIndex this_b = b;

        int64_t row_a = m_ref_model->getRow(a);
        int64_t row_b = m_ref_model->getRow(b);
        if (row_a > row_b) {
            int64_t tmp = row_a;
            row_a       = row_b;
            row_b       = tmp;
            this_a      = b;
            this_b      = a;
        }

        int64_t column_a = m_ref_model->getColumn(this_a);
        int64_t column_b = m_ref_model->getColumn(this_b);
        if (column_a != column_b) {
            return false;
        }

        ModelIndex parent_a = m_ref_model->getParent(this_a);
        ModelIndex parent_b = m_ref_model->getParent(this_b);

        if (parent_a != parent_b) {
            return false;
        }

        while (row_a < row_b) {
            m_selection.emplace_back(this_a);
            if (deep) {
                int64_t child_count = m_ref_model->getRowCount(this_a);
                if (child_count > 0) {
                    ModelIndex c_a = m_ref_model->getIndex(0, column_a, this_a);
                    ModelIndex c_b = m_ref_model->getIndex(child_count - 1, column_a, this_a);
                    if (!selectSpan(c_a, c_b, true, deep)) {
                        return false;
                    }
                }
            }
            this_a = m_ref_model->getIndex(++row_a, 0, parent_a);
        }

        m_selection.emplace_back(this_b);
        return true;
    }

    bool ModelSelectionState::selectAll() {
        if (!m_ref_model) {
            return false;
        }

        int64_t root_count = m_ref_model->getRowCount(ModelIndex());
        if (root_count > 0) {
            ModelIndex c_a = m_ref_model->getIndex(0, 0);
            ModelIndex c_b = m_ref_model->getIndex(root_count - 1, 0);
            return selectSpan(c_a, c_b, false);
        }

        return true;
    }

    void ModelSelectionState::dispatchToSelection(dispatch_fn fn) {
        for (const ModelIndex &index : m_selection) {
            fn(m_ref_model, index);
        }
    }

    ModelSelectionManager::ModelSelectionManager(RefPtr<IDataModel> model) {
        m_selection.setModel(model);
        model->addEventListener(getUUID(), TOOLBOX_BIND_EVENT_FN(updateSelectionOnInsert),
                                ModelEventFlags::EVENT_INSERT);
    }

    ModelSelectionManager::~ModelSelectionManager() {
        RefPtr<IDataModel> model = m_selection.getModel();
        if (model) {
            model->removeEventListener(getUUID());
        }
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

    bool ModelSelectionManager::actionPasteIntoSelection(const MimeData &data) {
        RefPtr<IDataModel> model = m_selection.getModel();
        if (!model) {
            return false;
        }

        if (model->isReadOnly()) {
            return false;
        }

        ModelInsertPolicy policy = ModelInsertPolicy::INSERT_AFTER;

        ModelIndex last_selected = m_selection.getLastSelected();
        if (model->getRowCount(last_selected) > 0) {
            policy = ModelInsertPolicy::INSERT_CHILD;
        }

        // We do this since we update the selection to the pasted items by event
        m_selection.clearSelection();

        return model->insertMimeData(last_selected, data, policy);
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

    ScopePtr<MimeData>
    ModelSelectionManager::actionCopySelection() {
        RefPtr<IDataModel> model = m_selection.getModel();
        if (!model) {
            return nullptr;
        }

        return model->createMimeData(m_selection.getSelection());
    }

    bool ModelSelectionManager::actionSelectIndex(const ModelIndex &index, bool force_single) {
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

        if (Input::GetKey(Input::KeyCode::KEY_LEFTSHIFT) ||
            Input::GetKey(Input::KeyCode::KEY_RIGHTSHIFT)) {
            if (model->validateIndex(m_selection.getLastSelected())) {
                m_selection.selectSpan(m_selection.getLastSelected(), index, false, true);
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
                m_selection.selectSpan(m_selection.getLastSelected(), index, false, true);
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
            if (m_selection.count() > 0) {
                m_selection.setLastSelected(ModelIndex());
                if (m_selection.selectSingle(index)) {
                    m_selection.setLastSelected(index);
                }
            }
        } else {
            if (!model->validateIndex(index)) {
                m_selection.clearSelection();
            }
            m_selection.setLastSelected(ModelIndex());
        }
        return true;
    }

    bool ModelSelectionManager::handleActionsByMouseInput(const ModelIndex &index) {
        const bool is_left_click          = Input::GetMouseButtonDown(Input::MouseButton::BUTTON_LEFT);
        const bool is_left_click_release  = Input::GetMouseButtonUp(Input::MouseButton::BUTTON_LEFT);
        const bool is_right_click         = Input::GetMouseButtonDown(Input::MouseButton::BUTTON_RIGHT);
        const bool is_right_click_release = Input::GetMouseButtonUp(Input::MouseButton::BUTTON_RIGHT);

        if (is_left_click && !is_left_click_release) {
            return actionSelectIndex(index);
        }

        if (is_right_click && !is_right_click_release) {
            return actionSelectIndexIfNew(index);
        }
        
        if (is_left_click_release || is_right_click_release) {
            return actionClearRequestExcIndex(index, is_left_click_release);
        }

        return false;
    }

    void ModelSelectionManager::updateSelectionOnInsert(const ModelIndex &index,
                                                        int flags) {
        if ((flags & ModelEventFlags::EVENT_INSERT) == ModelEventFlags::EVENT_NONE) {
            return;
        }

        m_selection.selectSingle(index, true);
    }

}  // namespace Toolbox
