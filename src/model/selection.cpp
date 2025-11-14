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

        if (!isSelected(index)) {
            m_selection.emplace_back(index);
        }
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

}  // namespace Toolbox
