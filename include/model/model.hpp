#pragma once

#include <algorithm>
#include <any>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <unordered_set>
#include <variant>
#include <vector>

#include "core/keybind/keybind.hpp"
#include "core/mimedata/mimedata.hpp"
#include "core/time/timepoint.hpp"
#include "core/types.hpp"
#include "fsystem.hpp"
#include "image/imagehandle.hpp"
#include "unique.hpp"

namespace Toolbox {

    enum ModelSortOrder {
        SORT_ASCENDING,
        SORT_DESCENDING,
    };

    enum ModelDataRole {
        DATA_ROLE_NONE,
        DATA_ROLE_DISPLAY,
        DATA_ROLE_TOOLTIP,
        DATA_ROLE_DECORATION,
        DATA_ROLE_USER,
    };

    enum ModelInsertPolicy {
        INSERT_BEFORE,
        INSERT_AFTER,
        INSERT_CHILD,
    };

    enum ModelEventFlags {
        EVENT_NONE           = 0,
        EVENT_SUCCESS        = BIT(0),
        EVENT_PRE            = BIT(1),
        EVENT_POST           = BIT(2),
        EVENT_SOFT           = BIT(3),
        EVENT_RESET          = BIT(4),
        EVENT_INSERT         = BIT(5),
        EVENT_INDEX_ADDED    = BIT(6),
        EVENT_INDEX_MODIFIED = BIT(7),
        EVENT_INDEX_REMOVED  = BIT(8),
        EVENT_INDEX_ANY      = EVENT_INDEX_ADDED | EVENT_INDEX_MODIFIED | EVENT_INDEX_REMOVED,
        EVENT_ANY =
            EVENT_SUCCESS | EVENT_PRE | EVENT_POST | EVENT_SOFT | EVENT_RESET | EVENT_INSERT | EVENT_INDEX_ANY,
    };

    class ModelIndex final : public IUnique {
    public:
        friend class IDataModel;

        ModelIndex() {
            m_uuid       = 0;
            m_model_uuid = 0;
            m_data.m_ptr = nullptr;
        }

        ModelIndex(UUID64 model_uuid) : m_model_uuid(model_uuid) {}
        ModelIndex(UUID64 model_uuid, UUID64 self_uuid)
            : m_model_uuid(model_uuid), m_uuid(self_uuid) {}
        ModelIndex(const ModelIndex &other)
            : m_uuid(other.m_uuid), m_model_uuid(other.m_model_uuid) {
            m_data.m_inline = other.m_data.m_inline;
        }

        template <typename T = void> [[nodiscard]] T *data() const {
            return reinterpret_cast<T *>(m_data.m_ptr);
        }
        void setData(void *data) { m_data.m_ptr = data; }

        // If using inline data, it is 8 bytes and overwrites the data ptr.
        [[nodiscard]] u64 inlineData() const { return m_data.m_inline; }
        // If using inline data, it is 8 bytes and overwrites the data ptr.
        void setInlineData(u64 data) const { m_data.m_inline = data; }

        [[nodiscard]] UUID64 getUUID() const override { return m_uuid; }
        [[nodiscard]] UUID64 getModelUUID() const { return m_model_uuid; }

        ModelIndex &operator=(const ModelIndex &other) {
            m_uuid          = other.m_uuid;
            m_model_uuid    = other.m_model_uuid;
            m_data.m_inline = other.m_data.m_inline;
            return *this;
        }

        [[nodiscard]] bool operator==(const ModelIndex &other) const {
            return (m_uuid == other.m_uuid) ||
                   (m_model_uuid == other.m_model_uuid &&
                    (data() == other.data() || inlineData() == other.inlineData()));
        }

    private:
        UUID64 m_uuid;
        UUID64 m_model_uuid = 0;

        mutable union {
            void *m_ptr;
            u64 m_inline;
        } m_data = {};
    };

    class IDataModel : public IUnique {
    public:
        using index_container  = std::vector<ModelIndex>;
        using event_listener_t = std::function<void(const ModelIndex &index, int flags)>;

    public:
        [[nodiscard]] virtual bool validateIndex(const ModelIndex &index) const {
            return index.getModelUUID() == getUUID();
        }

        [[nodiscard]] virtual bool isReadOnly() const = 0;

        [[nodiscard]] virtual std::string getDisplayText(const ModelIndex &index) const {
            return std::any_cast<std::string>(getData(index, ModelDataRole::DATA_ROLE_DISPLAY));
        }

        virtual void setDisplayText(const ModelIndex &index, const std::string &text) {
            setData(index, text, ModelDataRole::DATA_ROLE_DISPLAY);
        }

        [[nodiscard]] virtual std::string getToolTip(const ModelIndex &index) const {
            return std::any_cast<std::string>(getData(index, ModelDataRole::DATA_ROLE_TOOLTIP));
        }

        virtual void setToolTip(const ModelIndex &index, const std::string &text) {
            setData(index, text, ModelDataRole::DATA_ROLE_TOOLTIP);
        }

        [[nodiscard]] virtual RefPtr<const ImageHandle>
        getDecoration(const ModelIndex &index) const {
            return std::any_cast<RefPtr<const ImageHandle>>(
                getData(index, ModelDataRole::DATA_ROLE_DECORATION));
        }

        virtual void setDecoration(const ModelIndex &index, RefPtr<const ImageHandle> decoration) {
            setData(index, decoration, ModelDataRole::DATA_ROLE_DECORATION);
        }

        [[nodiscard]] virtual std::any getData(const ModelIndex &index, int role) const      = 0;
        [[nodiscard]] virtual void setData(const ModelIndex &index, std::any data, int role) = 0;

        [[nodiscard]] virtual ModelIndex getIndex(const UUID64 &uuid) const = 0;
        [[nodiscard]] virtual ModelIndex
        getIndex(int64_t row, int64_t column, const ModelIndex &parent = ModelIndex()) const = 0;

        [[nodiscard]] virtual bool removeIndex(const ModelIndex &index) = 0;

        [[nodiscard]] virtual ModelIndex getParent(const ModelIndex &index) const  = 0;
        [[nodiscard]] virtual ModelIndex getSibling(int64_t row, int64_t column,
                                                    const ModelIndex &index) const = 0;

        [[nodiscard]] virtual size_t getColumnCount(const ModelIndex &index) const = 0;
        [[nodiscard]] virtual size_t getRowCount(const ModelIndex &index) const    = 0;

        [[nodiscard]] virtual int64_t getColumn(const ModelIndex &index) const = 0;
        [[nodiscard]] virtual int64_t getRow(const ModelIndex &index) const    = 0;

        [[nodiscard]] virtual bool hasChildren(const ModelIndex &parent = ModelIndex()) const = 0;

        [[nodiscard]] virtual ScopePtr<MimeData>
        createMimeData(const index_container &indexes) const = 0;
        [[nodiscard]] virtual Result<IDataModel::index_container>
        insertMimeData(const ModelIndex &index, const MimeData &data,
                       ModelInsertPolicy policy = ModelInsertPolicy::INSERT_AFTER)   = 0;
        [[nodiscard]] virtual std::vector<std::string> getSupportedMimeTypes() const = 0;

        [[nodiscard]] virtual bool canFetchMore(const ModelIndex &index) = 0;
        virtual void fetchMore(const ModelIndex &index)                  = 0;

        virtual void reset() = 0;

        virtual void addEventListener(UUID64 uuid, event_listener_t listener,
                                      int allowed_flags) = 0;
        virtual void removeEventListener(UUID64 uuid)    = 0;

    protected:
        //[[nodiscard]] virtual int getEventFlagsForIndex(const ModelIndex &index) = 0;

        static void setIndexUUID(ModelIndex &index, UUID64 uuid) { index.m_uuid = uuid; }
    };

    class ModelIndexListTransformer {
    public:
        ModelIndexListTransformer() = delete;
        ModelIndexListTransformer(RefPtr<IDataModel> model) : m_model(model) {}

        void pruneRedundantsForRecursiveTree(IDataModel::index_container &indexes) const;

    private:
        RefPtr<IDataModel> m_model;
    };

    class ModelHistoryHandler : public IUnique {
    private:
        friend class ModelHistoryAggregate;

        struct HistoryAction {
            ModelIndex m_parent;
            int64_t m_row                  = -1;
            int64_t m_column               = -1;
            ScopePtr<MimeData> m_undo_data = nullptr;
            ScopePtr<MimeData> m_redo_data = nullptr;
            ModelEventFlags m_action_flags                      = ModelEventFlags::EVENT_NONE;

            HistoryAction()                                     = default;
            HistoryAction(HistoryAction &&) noexcept            = default;
            HistoryAction &operator=(HistoryAction &&) noexcept = default;

            HistoryAction(const HistoryAction &)            = delete;
            HistoryAction &operator=(const HistoryAction &) = delete;
        };

        // A frame may contain many actions, similar to a textbox undo/redo where each character
        // typed is an action, but they are all undone/redone together.
        struct HistoryFrame {
            std::list<HistoryAction> m_actions;
            TimePoint m_timestamp;
            bool m_collapsed                                  = false;

            HistoryFrame()                                    = default;
            HistoryFrame(HistoryFrame &&) noexcept            = default;
            HistoryFrame &operator=(HistoryFrame &&) noexcept = default;

            HistoryFrame(const HistoryFrame &)            = delete;
            HistoryFrame &operator=(const HistoryFrame &) = delete;

            bool operator<(const HistoryFrame &other) const {
                return m_timestamp < other.m_timestamp;
            }
        };

    protected:
        ModelHistoryHandler();

    public:
        ModelHistoryHandler(RefPtr<IDataModel> model);
        virtual ~ModelHistoryHandler();

        UUID64 getUUID() const override { return m_uuid; }

        virtual int64_t getCurrentFrame() const;

        virtual void handleInputs();
        virtual bool undoFrame();
        virtual bool redoFrame();
        [[nodiscard]] virtual bool hasUndoHistory() const;
        [[nodiscard]] virtual bool hasRedoHistory() const;

        virtual void startExplicitFrame();
        virtual void endExplicitFrame();

    protected:
        virtual TimePoint getUndoFrameTimepoint() const;
        virtual TimePoint getRedoFrameTimepoint() const;

        bool tryCollapseFrame(HistoryFrame &frame);

        void trimHistoryToTimePoint(const TimePoint &point);
        void updateHistory(const ModelIndex &index, int flags);

        using trim_cb =
            std::function<void(const TimePoint &new_point, const TimePoint &trim_point)>;
        void onHistoryTrimmed(trim_cb cb) { m_trim_cb = cb; }

    private:
        UUID64 m_uuid;

        RefPtr<IDataModel> m_model;
        std::vector<HistoryFrame> m_history;
        int64_t m_history_frame = -1;

        KeyBind m_undo_keybind;
        KeyBind m_redo_keybind;
        bool m_keybind_used;

        bool m_is_performing_undo_redo = false;
        HistoryAction m_current_action;

        trim_cb m_trim_cb;

        bool m_explicit_started   = false;
        bool m_explicit_continuing = false;
        bool m_explicit_finalized = false;
    };

    class ModelHistoryAggregate : public ModelHistoryHandler {
    private:
        struct AggregateFrame {
            TimePoint m_first_frame = TimePoint::min();
            TimePoint m_last_frame = TimePoint::max();
        };
    
    public:
        ModelHistoryAggregate() = delete;
        ModelHistoryAggregate(std::vector<ScopePtr<ModelHistoryHandler>> &&handlers);
        ~ModelHistoryAggregate() override = default;

        void addHandler(ScopePtr<ModelHistoryHandler> handler) {
            handler->onHistoryTrimmed(TOOLBOX_BIND_EVENT_FN(trimAllHistories));
            m_handlers.push_back(std::move(handler));
        }

        int64_t getCurrentFrame() const override;

        bool undoFrame() override;
        bool redoFrame() override;
        [[nodiscard]] bool hasUndoHistory() const override;
        [[nodiscard]] bool hasRedoHistory() const override;

        void startExplicitFrame() override;
        void endExplicitFrame() override;

    protected:
        TimePoint getUndoFrameTimepoint() const override;
        TimePoint getRedoFrameTimepoint() const override;

        void trimAllHistories(const TimePoint &new_point, const TimePoint &trim_point);

    private: 
        std::vector<ScopePtr<ModelHistoryHandler>> m_handlers;
        std::vector<AggregateFrame> m_frames;
        AggregateFrame m_wip_frame;

        // Merged timepoint between the handlers that the history aggregate is focused on
        TimePoint m_frame_timepoint = TimePoint::min();
    };

}  // namespace Toolbox

namespace std {

    template <> struct hash<Toolbox::ModelIndex> {
        Toolbox::u64 operator()(const Toolbox::ModelIndex &index) const {
            Toolbox::UUID64 uuid = index.getUUID();
            return hash<Toolbox::UUID64>()(uuid);
        }
    };

}  // namespace std