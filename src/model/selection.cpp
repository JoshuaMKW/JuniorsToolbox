#include "model/selection.hpp"

namespace Toolbox {
    
    bool ModelSelectionState::deselect(const ModelIndex &index) {
        if (!m_ref_model) {
            return false;
        }

        if (!m_ref_model->validateIndex(index)) {
            return false;
        }

        m_selection.erase(index);
        return true;
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

    bool ModelSelectionState::selectSpan(const ModelIndex &a, const ModelIndex &b, bool additive, bool deep) {
        if (!m_ref_model) {
            return false;
        }

        if (!m_ref_model->validateIndex(a) || !m_ref_model->validateIndex(b)) {
            return false;
        }

        if (!additive) {
            clearSelection();
        }

        if (a == b) {
            m_selection.insert(a);
            return true;
        }

        ModelIndex parent_a = m_ref_model->getParent(a);
        ModelIndex parent_b = m_ref_model->getParent(b);

        if (parent_a != parent_b) {
            return false;
        }

        int64_t row_a = m_ref_model->getRow(a);
        int64_t row_b = m_ref_model->getRow(b);

        ModelIndex this_a = a;
        while (row_a < row_b) {
            m_selection.insert(a);
            if (deep) {
                int64_t child_count = m_ref_model->getRowCount(a);
                if (child_count > 0) {
                    ModelIndex c_a = m_ref_model->getIndex(0, 0, a);
                    ModelIndex c_b = m_ref_model->getIndex(child_count - 1, 0, a);
                    if (!selectSpan(c_a, c_b, deep)) {
                        return false;
                    }
                }
            }
            this_a = m_ref_model->getIndex(++row_a, 0, parent_a);
        }

        ModelIndex this_b = b;
        while (row_b < row_a) {
            m_selection.insert(b);
            if (deep) {
                int64_t child_count = m_ref_model->getRowCount(b);
                if (child_count > 0) {
                    ModelIndex c_a = m_ref_model->getIndex(0, 0, b);
                    ModelIndex c_b = m_ref_model->getIndex(child_count - 1, 0, b);
                    if (!selectSpan(c_a, c_b, deep)) {
                        return false;
                    }
                }
            }
            this_b = m_ref_model->getIndex(++row_b, 0, parent_b);
        }

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
            return selectSpan(c_a, c_b, true);
        }

        return true;
    }

    void ModelSelectionState::dispatchToSelection(dispatch_fn fn) {
        for (const ModelIndex &index : m_selection) {
            fn(m_ref_model, index);
        }
    }
}
