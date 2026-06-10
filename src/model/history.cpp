#include "model/model.hpp"
#include <algorithm>
#include <unordered_set>

struct CollapseInfo {
    ModelIndex m_parent;
    int64_t m_row;
    int64_t m_column;  // Optional but highly recommended: Provide an equality operator
    // if you plan to use this struct in a std::unordered_map or std::unordered_set
    bool operator==(const CollapseInfo &other) const {
        // Assumes ModelIndex has operator== defined
        return m_parent == other.m_parent && m_row == other.m_row && m_column == other.m_column;
    }
};

namespace std {

    template <> struct hash<CollapseInfo> {
        size_t operator()(const CollapseInfo &info) const {
            size_t h1 = std::hash<ModelIndex>{}(info.m_parent);
            size_t h2 = std::hash<int64_t>{}(info.m_row);
            size_t h3 = std::hash<int64_t>{}(info.m_column);

            size_t seed = h1;
            seed ^= h2 + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            seed ^= h3 + 0x9e3779b9 + (seed << 6) + (seed >> 2);

            return seed;
        }
    };

}  // namespace std

namespace Toolbox {

    ModelHistoryHandler::ModelHistoryHandler()
        : m_model(nullptr), m_history(), m_current_action(), m_keybind_used(false),
          m_explicit_finalized(false),
          m_undo_keybind({Input::KeyCode::KEY_LEFTCONTROL, Input::KeyCode::KEY_Z}),
          m_redo_keybind({Input::KeyCode::KEY_LEFTCONTROL, Input::KeyCode::KEY_Y}) {}

    ModelHistoryHandler::ModelHistoryHandler(RefPtr<IDataModel> model) : ModelHistoryHandler() {
        if (!model) {
            return;
        }

        model->addEventListener(getUUID(), TOOLBOX_BIND_EVENT_FN(updateHistory), EVENT_ANY);
        m_model = model;
    }

    ModelHistoryHandler::~ModelHistoryHandler() {
        if (!m_model) {
            return;
        }

        m_model->removeEventListener(getUUID());
    }

    void ModelHistoryHandler::handleInputs() {
        if (m_keybind_used) {
            m_keybind_used = m_undo_keybind.isInputMatching() || m_redo_keybind.isInputMatching();
            return;
        }

        if (m_undo_keybind.isInputMatching()) {
            undoFrame();
            m_keybind_used = true;
        }

        if (m_redo_keybind.isInputMatching()) {
            redoFrame();
            m_keybind_used = true;
        }
    }

    bool ModelHistoryHandler::undoFrame() {
        if (m_history_frame < 0) {
            return false;
        }

        m_is_performing_undo_redo = true;

        HistoryFrame &frame = m_history[m_history_frame];
        tryCollapseFrame(frame);

        for (auto action_it = frame.m_actions.rbegin(); action_it != frame.m_actions.rend();
             ++action_it) {
            const HistoryAction &action = *action_it;

            // We can only undo/redo index modifications, not insertions or resets.
            if ((action.m_action_flags & EVENT_INDEX_MODIFIED) == EVENT_INDEX_MODIFIED) {

                // Debug log the model
                std::function<void(RefPtr<IDataModel>, ModelIndex, size_t)> printTree =
                    [&printTree](RefPtr<IDataModel> model, ModelIndex index, size_t depth) {
                        if (model->validateIndex(index)) {
                            TOOLBOX_DEBUG_LOG_V("[MODEL_HISTORY] ({}) - Index({}, {}): `{}'", depth,
                                                model->getRow(index), model->getColumn(index),
                                                model->getDisplayText(index));
                        }

                        const size_t row_count = model->getRowCount(index);
                        for (size_t i = 0; i < row_count; ++i) {
                            ModelIndex child = model->getIndex(i, 0, index);
                            printTree(model, child, depth + 1);
                        }
                    };

                TOOLBOX_DEBUG_LOG_V("[MODEL_HISTORY] ===============");
                printTree(m_model, ModelIndex(), 0);
                TOOLBOX_DEBUG_LOG_V("[MODEL_HISTORY] ===============");

                ModelIndex old_index =
                    m_model->getIndex(action.m_row, action.m_column, action.m_parent);
                if (!m_model->removeIndex(old_index)) {
                    TOOLBOX_ERROR("Failed to remove index during undo/redo!");
                    return false;
                }

                TOOLBOX_DEBUG_LOG_V("[MODEL_HISTORY] ===============");
                printTree(m_model, ModelIndex(), 0);
                TOOLBOX_DEBUG_LOG_V("[MODEL_HISTORY] ===============");

                const int64_t sibling_count = m_model->getRowCount(action.m_parent);
                if (sibling_count == 0) {
                    auto result = m_model->insertMimeData(action.m_parent, *action.m_undo_data,
                                                          ModelInsertPolicy::INSERT_CHILD);
                    if (!result) {
                        TOOLBOX_ERROR_V("Failed to insert mime data during undo/redo: {}",
                                        result.error().m_message[0]);
                        return false;
                    }
                } else {
                    int64_t sibling_row;
                    ModelInsertPolicy policy;

                    if (action.m_row < sibling_count) {
                        sibling_row = action.m_row;
                        policy      = ModelInsertPolicy::INSERT_BEFORE;
                    } else {
                        // Appending to the end
                        sibling_row = sibling_count - 1;
                        policy      = ModelInsertPolicy::INSERT_AFTER;
                    }

                    ModelIndex sibling_index =
                        m_model->getIndex(sibling_row, action.m_column, action.m_parent);

                    auto result =
                        m_model->insertMimeData(sibling_index, *action.m_undo_data, policy);
                    if (!result) {
                        TOOLBOX_ERROR_V("Failed to insert mime data during undo/redo: {}",
                                        result.error().m_message[0]);
                        return false;
                    }
                }

                TOOLBOX_DEBUG_LOG_V("[MODEL_HISTORY] ===============");
                printTree(m_model, ModelIndex(), 0);
                TOOLBOX_DEBUG_LOG_V("[MODEL_HISTORY] ===============");
            }

            // We can only undo/redo index modifications, not insertions or resets.
            if ((action.m_action_flags & EVENT_INDEX_ADDED) == EVENT_INDEX_ADDED) {

                ModelIndex old_index =
                    m_model->getIndex(action.m_row, action.m_column, action.m_parent);
                if (!m_model->removeIndex(old_index)) {
                    TOOLBOX_ERROR("Failed to remove index during undo/redo!");
                    return false;
                }
            }

            // We can only undo/redo index modifications, not insertions or resets.
            if ((action.m_action_flags & EVENT_INDEX_REMOVED) == EVENT_INDEX_REMOVED) {
                const int64_t sibling_count = m_model->getRowCount(action.m_parent);
                if (sibling_count == 0) {
                    auto result = m_model->insertMimeData(action.m_parent, *action.m_undo_data,
                                                          ModelInsertPolicy::INSERT_CHILD);
                    if (!result) {
                        TOOLBOX_ERROR_V("Failed to insert mime data during undo/redo: {}",
                                        result.error().m_message[0]);
                        return false;
                    }
                } else {
                    int64_t sibling_row;
                    ModelInsertPolicy policy;

                    if (action.m_row < sibling_count) {
                        sibling_row = action.m_row;
                        policy      = ModelInsertPolicy::INSERT_BEFORE;
                    } else {
                        // Appending to the end
                        sibling_row = sibling_count - 1;
                        policy      = ModelInsertPolicy::INSERT_AFTER;
                    }

                    ModelIndex sibling_index =
                        m_model->getIndex(sibling_row, action.m_column, action.m_parent);

                    auto result =
                        m_model->insertMimeData(sibling_index, *action.m_undo_data, policy);
                    if (!result) {
                        TOOLBOX_ERROR_V("Failed to insert mime data during undo/redo: {}",
                                        result.error().m_message[0]);
                        return false;
                    }
                }
            }
        }

        m_history_frame -= 1;

        m_is_performing_undo_redo = false;
    }

    bool ModelHistoryHandler::redoFrame() {
        int64_t redo_frame = m_history_frame + 1;

        if (redo_frame >= m_history.size()) {
            return false;
        }

        m_is_performing_undo_redo = true;

        HistoryFrame &frame = m_history[redo_frame];
        tryCollapseFrame(frame);

        for (auto action_it = frame.m_actions.begin(); action_it != frame.m_actions.end();
             ++action_it) {
            const HistoryAction &action = *action_it;

            // We can only undo/redo index modifications, not insertions or resets.
            if ((action.m_action_flags & EVENT_INDEX_MODIFIED) == EVENT_INDEX_MODIFIED) {

                ModelIndex old_index =
                    m_model->getIndex(action.m_row, action.m_column, action.m_parent);
                if (!m_model->removeIndex(old_index)) {
                    TOOLBOX_ERROR("Failed to remove index during undo/redo!");
                    return false;
                }

                const int64_t sibling_count = m_model->getRowCount(action.m_parent);
                if (sibling_count == 0) {
                    auto result = m_model->insertMimeData(action.m_parent, *action.m_redo_data,
                                                          ModelInsertPolicy::INSERT_CHILD);
                    if (!result) {
                        TOOLBOX_ERROR_V("Failed to insert mime data during undo/redo: {}",
                                        result.error().m_message[0]);
                        return false;
                    }
                } else {
                    int64_t sibling_row;
                    ModelInsertPolicy policy;

                    if (action.m_row < sibling_count) {
                        sibling_row = action.m_row;
                        policy      = ModelInsertPolicy::INSERT_BEFORE;
                    } else {
                        // Appending to the end
                        sibling_row = sibling_count - 1;
                        policy      = ModelInsertPolicy::INSERT_AFTER;
                    }

                    ModelIndex sibling_index =
                        m_model->getIndex(sibling_row, action.m_column, action.m_parent);

                    auto result =
                        m_model->insertMimeData(sibling_index, *action.m_redo_data, policy);
                    if (!result) {
                        TOOLBOX_ERROR_V("Failed to insert mime data during undo/redo: {}",
                                        result.error().m_message[0]);
                        return false;
                    }
                }
            }

            // We can only undo/redo index modifications, not insertions or resets.
            if ((action.m_action_flags & EVENT_INDEX_ADDED) == EVENT_INDEX_ADDED) {
                const int64_t sibling_count = m_model->getRowCount(action.m_parent);
                if (sibling_count == 0) {
                    auto result = m_model->insertMimeData(action.m_parent, *action.m_redo_data,
                                                          ModelInsertPolicy::INSERT_CHILD);
                    if (!result) {
                        TOOLBOX_ERROR_V("Failed to insert mime data during undo/redo: {}",
                                        result.error().m_message[0]);
                        return false;
                    }
                } else {
                    int64_t sibling_row;
                    ModelInsertPolicy policy;

                    if (action.m_row < sibling_count) {
                        sibling_row = action.m_row;
                        policy      = ModelInsertPolicy::INSERT_BEFORE;
                    } else {
                        // Appending to the end
                        sibling_row = sibling_count - 1;
                        policy      = ModelInsertPolicy::INSERT_AFTER;
                    }

                    ModelIndex sibling_index =
                        m_model->getIndex(sibling_row, action.m_column, action.m_parent);

                    auto result =
                        m_model->insertMimeData(sibling_index, *action.m_redo_data, policy);
                    if (!result) {
                        TOOLBOX_ERROR_V("Failed to insert mime data during undo/redo: {}",
                                        result.error().m_message[0]);
                        return false;
                    }
                }
            }

            // We can only undo/redo index modifications, not insertions or resets.
            if ((action.m_action_flags & EVENT_INDEX_REMOVED) == EVENT_INDEX_REMOVED) {
                ModelIndex old_index =
                    m_model->getIndex(action.m_row, action.m_column, action.m_parent);
                if (!m_model->removeIndex(old_index)) {
                    TOOLBOX_ERROR("Failed to remove index during undo/redo!");
                    return false;
                }
            }
        }

        m_history_frame += 1;

        m_is_performing_undo_redo = false;
    }

    bool ModelHistoryHandler::hasUndoHistory() const {
        return m_history_frame >= 0 && m_history_frame < m_history.size();
    }

    bool ModelHistoryHandler::hasRedoHistory() const {
        return m_history_frame >= -1 &&
               m_history_frame < static_cast<int64_t>(m_history.size()) - 1;
    }

    void ModelHistoryHandler::startExplicitFrame() { m_explicit_started = true; }
    void ModelHistoryHandler::endExplicitFrame() { m_explicit_finalized = true; }

    TimePoint ModelHistoryHandler::getUndoFrameTimepoint() const {
        if (m_history_frame >= 0 && m_history_frame < m_history.size()) {
            return m_history[m_history_frame].m_timestamp;
        }
        return TimePoint::min();
    }

    TimePoint ModelHistoryHandler::getRedoFrameTimepoint() const {
        int64_t redo_frame = m_history_frame + 1;
        if (redo_frame >= 0 && redo_frame < m_history.size()) {
            return m_history[redo_frame].m_timestamp;
        }
        return TimePoint::max();
    }

    bool ModelHistoryHandler::tryCollapseFrame(HistoryFrame &frame) {
        if (frame.m_collapsed || frame.m_actions.size() < 2) {
            return true;
        }

        // Maps a unique index to the iterator of its first active modification.
        std::unordered_map<CollapseInfo, std::list<HistoryAction>::iterator> active_chains;

        auto it = frame.m_actions.begin();
        while (it != frame.m_actions.end()) {
            CollapseInfo current_info{it->m_parent, it->m_row, it->m_column};

            // Is this a structural change (Add/Remove) or a non-soft modification?
            const bool is_structural =
                (it->m_action_flags & (EVENT_INDEX_ADDED | EVENT_INDEX_REMOVED)) != EVENT_NONE;
            const bool is_hard_mod =
                (it->m_action_flags & EVENT_INDEX_MODIFIED) == EVENT_INDEX_MODIFIED &&
                (it->m_action_flags & EVENT_SOFT) != EVENT_SOFT;

            if (is_structural || is_hard_mod) {
                active_chains.erase(current_info);
                ++it;
                continue;
            }

            // This is an soft modification.
            auto chain_it = active_chains.find(current_info);

            if (chain_it == active_chains.end()) {
                // Start of a new soft-chain for this index.
                active_chains[current_info] = it;
                ++it;
            } else {
                auto first_mod_it = chain_it->second;

                // Steal the latest redo state
                first_mod_it->m_redo_data = std::move(it->m_redo_data);

                // Erase the soft action and shift everything back one
                it = frame.m_actions.erase(it);
            }
        }

        frame.m_collapsed = true;
        return true;
    }

    void ModelHistoryHandler::trimHistoryToTimePoint(const TimePoint &point) {
        auto frame_it =
            std::remove_if(m_history.begin(), m_history.end(), [point](const HistoryFrame &frame) {
                return frame.m_timestamp > point;
            });

        if (frame_it != m_history.end()) {
            m_history.erase(frame_it, m_history.end());
        }

        if (m_history_frame >= static_cast<int64_t>(m_history.size())) {
            m_history_frame = static_cast<int64_t>(m_history.size()) - 1;
        }
    }

    void ModelHistoryHandler::updateHistory(const ModelIndex &index, int flags) {
        if (m_is_performing_undo_redo) {
            return;
        }

        if ((flags & EVENT_RESET) == EVENT_RESET) {
            m_history.clear();
            return;
        }

        if ((flags & EVENT_INDEX_ANY) == EVENT_NONE) {
            return;
        }

        if ((flags & EVENT_PRE) == EVENT_PRE) {
            const bool is_frame_init = m_current_action.m_action_flags == EVENT_NONE;
            if (!is_frame_init) {
                TOOLBOX_DEBUG_LOG("[MODEL_HISTORY] Received a pre event without a corresponding "
                                  "post event. Resetting the current frame...");
            }

            // Start a new frame for this action.
            m_current_action.m_action_flags = static_cast<ModelEventFlags>(
                flags & ~(EVENT_SUCCESS | EVENT_PRE | EVENT_POST | EVENT_RESET));
            m_current_action.m_row    = m_model->getRow(index);
            m_current_action.m_column = m_model->getColumn(index);
            m_current_action.m_parent = m_model->getParent(index);

            if (m_explicit_continuing &&
                (m_current_action.m_action_flags & EVENT_INDEX_MODIFIED) == EVENT_INDEX_MODIFIED) {
                m_current_action.m_action_flags =
                    static_cast<ModelEventFlags>(m_current_action.m_action_flags | EVENT_SOFT);
            }

            if ((flags & EVENT_INDEX_ADDED) == EVENT_NONE) {
                m_current_action.m_undo_data = m_model->createMimeData({index});
            }

            TimePoint new_timestamp = TimePoint::clock::now();

            if (m_trim_cb) {
                TimePoint trim_timestamp = TimePoint::min();

                if (m_history_frame >= 0 && m_history_frame < m_history.size()) {
                    const HistoryFrame &trim_frame = m_history[m_history_frame];
                    trim_timestamp                 = trim_frame.m_timestamp;
                }

                m_trim_cb(new_timestamp, trim_timestamp);
            } else {
                m_history.erase(m_history.begin() + m_history_frame + 1, m_history.end());
            }

            const bool is_action_soft =
                (m_current_action.m_action_flags & EVENT_SOFT) == EVENT_SOFT;

            const bool should_piggyback = is_action_soft && [&]() {
                if (m_history.empty()) {
                    return false;
                }

                const HistoryFrame &last_frame = m_history.back();
                if (!last_frame.m_actions.empty()) {
                    const HistoryAction &last_action = last_frame.m_actions.back();

                    if (last_action.m_row == m_current_action.m_row &&
                        last_action.m_column == m_current_action.m_column &&
                        last_action.m_parent == m_current_action.m_parent) {
                        return true;
                    }
                }
                return false;
            }();

            const bool is_new_frame =
                m_explicit_started || m_explicit_finalized ||
                (is_frame_init && !should_piggyback && !m_explicit_continuing);

            if (is_new_frame) {
                if (!m_history.empty()) {
                    tryCollapseFrame(m_history.back());
                }

                m_history.emplace_back();
                m_history.back().m_timestamp = new_timestamp;
                m_history_frame += 1;
            }

            if (m_explicit_started) {
                m_explicit_continuing = true;
            } else if (m_explicit_finalized) {
                m_explicit_continuing = false;
            }

            m_explicit_started   = false;
            m_explicit_finalized = false;
        } else {
            if (m_current_action.m_action_flags == EVENT_NONE) {
                TOOLBOX_DEBUG_LOG("[MODEL_HISTORY] Received a post event without a corresponding "
                                  "pre event. Ignoring...");
                return;
            }
        }

        if ((flags & EVENT_POST) == EVENT_POST) {
            if ((flags & EVENT_INDEX_REMOVED) == EVENT_NONE) {
                m_current_action.m_redo_data = m_model->createMimeData({index});
            }

            m_history.back().m_actions.push_back(std::move(m_current_action));

            m_current_action = {};
        }
    }

    ModelHistoryAggregate::ModelHistoryAggregate(
        std::vector<ScopePtr<ModelHistoryHandler>> &&handlers)
        : ModelHistoryHandler() {
        m_handlers.reserve(handlers.size());
        for (ScopePtr<ModelHistoryHandler> &handler : handlers) {
            addHandler(std::move(handler));
        }
    }

    bool ModelHistoryAggregate::undoFrame() {
        // Helper lambda to find the most recent handler
        auto getMostRecent = [&]() -> std::pair<ModelHistoryHandler *, TimePoint> {
            ModelHistoryHandler *best_handler = nullptr;
            TimePoint best_time               = TimePoint::min();

            for (const ScopePtr<ModelHistoryHandler> &handler : m_handlers) {
                TimePoint t = handler->getUndoFrameTimepoint();
                if (t != TimePoint::min() && t > best_time) {
                    best_time    = t;
                    best_handler = handler.get();
                }
            }
            return {best_handler, best_time};
        };

        auto [most_recent_handler, most_recent_timepoint] = getMostRecent();

        if (!most_recent_handler) {
            return false;  // Nothing to undo
        }

        // Check if this action belongs to an explicit aggregate frame
        TimePoint transaction_start = TimePoint::min();
        bool is_in_transaction      = false;

        // Search backwards through frames to find the matching transaction
        for (auto it = m_frames.rbegin(); it != m_frames.rend(); ++it) {
            if (most_recent_timepoint >= it->m_first_frame &&
                most_recent_timepoint <= it->m_last_frame) {
                transaction_start = it->m_first_frame;
                is_in_transaction = true;
                break;
            }
        }

        if (!is_in_transaction) {
            // Standard single undo
            if (most_recent_handler->undoFrame()) {
                m_frame_timepoint = getUndoFrameTimepoint();
                return true;
            }
            return false;
        }

        // Macro-Transaction Undo: Keep undoing until we hit the start boundary
        bool success = false;
        while (most_recent_handler && most_recent_timepoint >= transaction_start) {
            if (!most_recent_handler->undoFrame()) {
                break;  // Sanity check, should not happen if logic holds
            }
            success = true;

            // Find the *next* most recent action to undo
            auto next_target      = getMostRecent();
            most_recent_handler   = next_target.first;
            most_recent_timepoint = next_target.second;
        }

        if (success) {
            m_frame_timepoint = getUndoFrameTimepoint();
        }

        return success;
    }

    bool ModelHistoryAggregate::redoFrame() {
        // Helper lambda to find the earliest redo handler
        auto getEarliestRedo = [&]() -> std::pair<ModelHistoryHandler *, TimePoint> {
            ModelHistoryHandler *best_handler = nullptr;
            TimePoint best_time               = TimePoint::max();

            for (const ScopePtr<ModelHistoryHandler> &handler : m_handlers) {
                TimePoint t = handler->getRedoFrameTimepoint();
                if (t != TimePoint::max() && t < best_time) {
                    best_time    = t;
                    best_handler = handler.get();
                }
            }
            return {best_handler, best_time};
        };

        auto [earliest_handler, earliest_timepoint] = getEarliestRedo();

        if (!earliest_handler) {
            return false;  // Nothing to redo
        }

        // Check if this action belongs to an explicit aggregate frame
        TimePoint transaction_end = TimePoint::max();
        bool is_in_transaction    = false;

        // Search forwards through frames to find the matching transaction
        for (const auto &frame : m_frames) {
            if (earliest_timepoint >= frame.m_first_frame &&
                earliest_timepoint <= frame.m_last_frame) {
                transaction_end   = frame.m_last_frame;
                is_in_transaction = true;
                break;
            }
        }

        if (!is_in_transaction) {
            // Standard single redo
            if (earliest_handler->redoFrame()) {
                m_frame_timepoint = earliest_timepoint;
                return true;
            }
            return false;
        }

        // Macro-Transaction Redo: Keep redoing until we hit the end boundary
        bool success = false;
        while (earliest_handler && earliest_timepoint <= transaction_end) {
            if (!earliest_handler->redoFrame()) {
                break;
            }
            success           = true;
            m_frame_timepoint = earliest_timepoint;  // Track the latest valid redo point

            // Find the *next* action to redo
            auto next_target   = getEarliestRedo();
            earliest_handler   = next_target.first;
            earliest_timepoint = next_target.second;
        }

        return success;
    }

    bool ModelHistoryAggregate::hasUndoHistory() const {
        bool has_history = false;
        for (const ScopePtr<ModelHistoryHandler> &handler : m_handlers) {
            has_history |= handler->hasUndoHistory();
        }
        return has_history;
    }

    bool ModelHistoryAggregate::hasRedoHistory() const {
        bool has_history = false;
        for (const ScopePtr<ModelHistoryHandler> &handler : m_handlers) {
            has_history |= handler->hasRedoHistory();
        }
        return has_history;
    }

    void ModelHistoryAggregate::startExplicitFrame() {
        ModelHistoryHandler::startExplicitFrame();

        for (ScopePtr<ModelHistoryHandler> &handler : m_handlers) {
            handler->startExplicitFrame();
        }

        m_wip_frame.m_first_frame = TimePoint::clock::now();
    }

    void ModelHistoryAggregate::endExplicitFrame() {
        ModelHistoryHandler::endExplicitFrame();

        for (ScopePtr<ModelHistoryHandler> &handler : m_handlers) {
            handler->endExplicitFrame();
        }

        m_wip_frame.m_last_frame = TimePoint::clock::now();

        m_frames.push_back(m_wip_frame);

        m_wip_frame = {};
    }

    TimePoint ModelHistoryAggregate::getUndoFrameTimepoint() const {
        TimePoint most_recent_timepoint = TimePoint::min();
        for (const ScopePtr<ModelHistoryHandler> &handler : m_handlers) {
            TimePoint handler_timepoint = handler->getUndoFrameTimepoint();
            if (handler_timepoint > most_recent_timepoint) {
                most_recent_timepoint = handler_timepoint;
            }
        }
        return most_recent_timepoint;
    }

    TimePoint ModelHistoryAggregate::getRedoFrameTimepoint() const {
        TimePoint most_recent_timepoint = TimePoint::max();
        for (const ScopePtr<ModelHistoryHandler> &handler : m_handlers) {
            TimePoint handler_timepoint = handler->getRedoFrameTimepoint();
            if (handler_timepoint < most_recent_timepoint) {
                most_recent_timepoint = handler_timepoint;
            }
        }
        return most_recent_timepoint;
    }

    void ModelHistoryAggregate::trimAllHistories(const TimePoint &new_point, const TimePoint &) {
        TimePoint global_trim_point = getUndoFrameTimepoint();

        for (ScopePtr<ModelHistoryHandler> &handler : m_handlers) {
            handler->trimHistoryToTimePoint(global_trim_point);
        }

        auto it = std::remove_if(m_frames.begin(), m_frames.end(),
                                 [global_trim_point](const auto &frame) {
                                     return frame.m_first_frame > global_trim_point;
                                 });

        if (it != m_frames.end()) {
            m_frames.erase(it, m_frames.end());
        }

        m_frame_timepoint = new_point;
    }

}  // namespace Toolbox