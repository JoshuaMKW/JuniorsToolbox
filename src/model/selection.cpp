#include "model/selection.hpp"
#include "core/clipboard.hpp"
#include "core/input/input.hpp"
#include "core/input/keycode.hpp"

namespace Toolbox {

    void ModelSelectionState::setLastSelected(const ModelIndex &index) { m_last_selected = index; }

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

        m_selection.insert(index);
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
            m_selection.insert(a);
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
            m_selection.insert(this_a);
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

        m_selection.insert(this_b);
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

    bool ModelSelectionManager::actionDeleteSelection(ModelSelectionState &selection) {
        RefPtr<IDataModel> model = selection.getModel();
        if (!model) {
            return false;
        }

        if (model->isReadOnly()) {
            return false;
        }

        bool result = true;
        for (const ModelIndex &s : selection.getSelection()) {
            result &= model->removeIndex(s);
        }

        selection.clearSelection();

        return result;
    }

    bool ModelSelectionManager::actionRenameSelection(const ModelSelectionState &selection,
                                                      const std::string &template_name) {
        RefPtr<IDataModel> model = selection.getModel();
        if (!model) {
            return false;
        }

        if (model->isReadOnly()) {
            return false;
        }

        bool result = true;
        for (const ModelIndex &s : selection.getSelection()) {
            result &= model->validateIndex(s);
            model->setDisplayText(s, template_name);
        }

        return result;
    }

    bool ModelSelectionManager::actionPasteIntoSelection(const ModelSelectionState &selection,
                                                         const MimeData &data) {
        RefPtr<IDataModel> model = selection.getModel();
        if (!model) {
            return false;
        }

        if (model->isReadOnly()) {
            return false;
        }

        return model->insertMimeData(selection.getLastSelected(), data);
    }

    ScopePtr<MimeData> ModelSelectionManager::actionCutSelection(ModelSelectionState &selection) {
        ScopePtr<MimeData> data = actionCopySelection(selection);
        if (!data) {
            return nullptr;
        }
        
        if (!actionDeleteSelection(selection)) {
            TOOLBOX_ERROR("Failed to cut the selection!");
            return nullptr;
        }

        return data;
    }

    ScopePtr<MimeData>
    ModelSelectionManager::actionCopySelection(const ModelSelectionState &selection) {
        RefPtr<IDataModel> model = selection.getModel();
        if (!model) {
            return nullptr;
        }

        return model->createMimeData(selection.getSelection());
    }

    bool ModelSelectionManager::actionSelectIndex(ModelSelectionState &selection,
                                                  const ModelIndex &index, bool force_single) {
        RefPtr<IDataModel> model = selection.getModel();
        if (!model) {
            return false;
        }

        if (force_single) {
            selection.selectSingle(index);
            selection.setLastSelected(index);
            return true;
        }

        if (Input::GetKey(Input::KeyCode::KEY_LEFTCONTROL) ||
            Input::GetKey(Input::KeyCode::KEY_RIGHTCONTROL)) {
            if (selection.isSelected(index)) {
                selection.deselect(index);
            } else {
                selection.selectSingle(index, true);
            }
            selection.setLastSelected(index);
            return true;
        }

        if (Input::GetKey(Input::KeyCode::KEY_LEFTSHIFT) ||
            Input::GetKey(Input::KeyCode::KEY_RIGHTSHIFT)) {
            if (model->validateIndex(selection.getLastSelected())) {
                selection.selectSpan(selection.getLastSelected(), index, false, true);
            } else {
                selection.selectSingle(index);
                selection.setLastSelected(index);
            }
            return true;
        }

        selection.selectSingle(index);
        selection.setLastSelected(index);
        return true;
    }

    bool ModelSelectionManager::actionClearRequestExcIndex(ModelSelectionState &selection,
                                                           const ModelIndex &index,
                                                           bool is_left_button) {
        RefPtr<IDataModel> model = selection.getModel();
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
            if (selection.count() > 0) {
                selection.setLastSelected(ModelIndex());
                if (selection.selectSingle(index)) {
                    selection.setLastSelected(index);
                }
            }
        } else {
            if (!model->validateIndex(index)) {
                selection.clearSelection();
            }
            selection.setLastSelected(ModelIndex());
        }
        return true;
    }

}  // namespace Toolbox
