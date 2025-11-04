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

        ModelSelectionState &operator=(const ModelSelectionState &other) {
            m_ref_model     = other.m_ref_model;
            m_selection     = other.m_selection;
            m_last_selected = other.m_last_selected;
            return *this;
        }

        ModelSelectionState &operator=(ModelSelectionState &&other) noexcept {
            m_ref_model     = std::move(other.m_ref_model);
            m_selection     = std::move(other.m_selection);
            m_last_selected = std::move(other.m_last_selected);
            return *this;
        }

    public:
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

    private:
        RefPtr<IDataModel> m_ref_model;
        IDataModel::index_container m_selection;
        ModelIndex m_last_selected;
    };

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

        bool actionDeleteSelection();
        bool actionRenameSelection(const std::string &template_name);
        bool actionPasteIntoSelection(const MimeData &data);
        ScopePtr<MimeData> actionCutSelection();
        ScopePtr<MimeData> actionCopySelection();

        bool actionSelectIndex(const ModelIndex &index, bool force_single = false);

        bool actionSelectIndexIfNew(const ModelIndex &index);
        bool actionClearRequestExcIndex(const ModelIndex &index, bool is_left_button);

        bool handleActionsByMouseInput(const ModelIndex &index);

    protected:
        void updateSelectionOnInsert(const ModelIndex &index, int flags);

    private:
        UUID64 m_uuid;
        ModelSelectionState m_selection;
    };

}  // namespace Toolbox