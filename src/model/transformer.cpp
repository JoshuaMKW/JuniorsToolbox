#include "model/model.hpp"

namespace Toolbox {

    void ModelIndexListTransformer::pruneRedundantsForRecursiveTree(
        IDataModel::index_container &indexes) const {
        if (!m_model) {
            return;
        }

        for (size_t i = 0; i < indexes.size();) {
            ModelIndex index = indexes[i];
            for (size_t j = 0; j < indexes.size();) {
                ModelIndex child_idx  = indexes[j];
                ModelIndex parent_idx = m_model->getParent(child_idx);
                while (m_model->validateIndex(parent_idx)) {
                    if (parent_idx.getUUID() == index.getUUID()) {
                        auto child_it = indexes.begin() + j;
                        child_it = indexes.erase(child_it);
                        if (i > j) {
                            i -= 1;
                        }
                        j -= 1;
                        break;
                    }
                    parent_idx = m_model->getParent(parent_idx);
                }
                j += 1;
            }
            ++i;
        }
    }

}  // namespace Toolbox