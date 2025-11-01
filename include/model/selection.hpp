#pragma once

#include <functional>
#include <unordered_set>

#include "model/model.hpp"

namespace Toolbox {

    class ModelSelectionState {
    public:
        ModelSelectionState()                                = default;
        ModelSelectionState(const ModelSelectionState &)     = default;
        ModelSelectionState(ModelSelectionState &&) noexcept = default;

        ~ModelSelectionState() = default;

        RefPtr<IDataModel> getModel() const { return m_ref_model; }
        void setModel(RefPtr<IDataModel> model) { m_ref_model = model; }

        size_t count() const { return m_selection.size(); }

        bool isSelected(const ModelIndex &index) const {
            return std::find_if(m_selection.begin(), m_selection.end(), [&](const ModelIndex &b) {
                       return b.inlineData() == index.inlineData() || b.data() == index.data();
                   }) != m_selection.end();
        }

        ModelIndex getLastSelected() const { return m_last_selected; }
        void setLastSelected(const ModelIndex &index);

        const IDataModel::index_container &getSelection() const { return m_selection; }

        void clearSelection() {
            m_selection.clear();
            m_last_selected = ModelIndex();
        }

        bool deselect(const ModelIndex &index);
        bool selectSingle(const ModelIndex &index, bool additive = false);
        bool selectSpan(const ModelIndex &a, const ModelIndex &b, bool additive, bool deep = false);
        bool selectAll();

        using dispatch_fn = std::function<void(RefPtr<IDataModel>, const ModelIndex &)>;
        void dispatchToSelection(dispatch_fn fn);

    public:
        ModelSelectionState &operator=(const ModelSelectionState &other) {
            m_ref_model = other.m_ref_model;
            m_selection = other.m_selection;
            return *this;
        }

    private:
        RefPtr<IDataModel> m_ref_model;
        IDataModel::index_container m_selection;
        ModelIndex m_last_selected;
    };

    class ModelSelectionManager {
    public:
        ModelSelectionManager() = default;

        bool actionDeleteSelection(ModelSelectionState &state);
        bool actionRenameSelection(const ModelSelectionState &state,
                                   const std::string &template_name);
        bool actionPasteIntoSelection(const ModelSelectionState &state, const MimeData &data);
        ScopePtr<MimeData> actionCutSelection(ModelSelectionState &state);
        ScopePtr<MimeData> actionCopySelection(const ModelSelectionState &state);

        bool actionSelectIndex(ModelSelectionState &state, const ModelIndex &index,
                               bool force_single = false);

        bool actionSelectIndexIfNew(ModelSelectionState &state, const ModelIndex &index);
        bool actionClearRequestExcIndex(ModelSelectionState &state, const ModelIndex &index,
                                        bool is_left_button);

        bool handleActionsByMouseInput(ModelSelectionState &state, const ModelIndex &index);
    };

}  // namespace Toolbox