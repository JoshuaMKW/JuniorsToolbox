#include "model/model.hpp"
#include <algorithm>
#include <unordered_set>

namespace Toolbox {

    /**
     * @brief Prunes a list of indexes, removing any index that is a descendant
     * of another index *in the same list*.
     */
    void ModelIndexListTransformer::pruneRedundantsForRecursiveTree(
        IDataModel::index_container &indexes) const {
        if (!m_model || indexes.size() < 2) {
            return;
        }

        // ---
        // Step 1: Build a fast-lookup set of all UUIDs in the list.
        // This is O(N) and has good cache locality (sequential read).
        // ---
        std::unordered_set<UUID64> list_uuids;
        list_uuids.reserve(indexes.size());
        for (const ModelIndex &index : indexes) {
            list_uuids.insert(index.getUUID());
        }

        // ---
        // Step 2: Use the "erase-remove" idiom.
        // std::remove_if partitions the vector by "shuffling" all elements
        // to be KEPT to the front. It returns an iterator to the new logical end.
        // This is one sequential pass, O(N).
        // ---
        auto new_end_it = std::remove_if(indexes.begin(), indexes.end(),
                                         // This lambda is called once for each index (N times).
                                         [this, &list_uuids](const ModelIndex &child_idx) {
                                             ModelIndex parent_idx = m_model->getParent(child_idx);

                                             while (m_model->validateIndex(parent_idx)) {
                                                 // Check if this parent is *also* in our original
                                                 // list.
                                                 if (list_uuids.count(parent_idx.getUUID())) {
                                                     return true;
                                                 }

                                                 parent_idx = m_model->getParent(parent_idx);
                                             }

                                             // We reached the root and found no parent in the
                                             // list. Keep this index.
                                             return false;
                                         });

        // ---
        // Step 3: Erase all the "removed" elements in one single operation.
        // This is O(K) where K is the number of elements removed.
        // ---
        indexes.erase(new_end_it, indexes.end());
    }

}  // namespace Toolbox