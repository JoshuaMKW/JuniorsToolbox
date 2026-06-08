#include "model/model.hpp"
#include <algorithm>
#include <unordered_set>

namespace Toolbox {

    ModelHistoryHandler::ModelHistoryHandler()
        : m_model(nullptr), m_history(), m_current_action(), m_keybind_used(false),
          m_undo_keybind({Input::KeyCode::KEY_LEFTCONTROL, Input::KeyCode::KEY_Z}),
          m_redo_keybind({Input::KeyCode::KEY_LEFTCONTROL, Input::KeyCode::KEY_Y}) {}

    ModelHistoryHandler::ModelHistoryHandler(RefPtr<IDataModel> model) : ModelHistoryHandler() {
        if (!model) {
            return;
        }

        model->addEventListener(getUUID(), TOOLBOX_BIND_EVENT_FN(updateHistory),
                                ModelEventFlags::EVENT_ANY);
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

        const HistoryFrame &frame = m_history[m_history_frame];
        for (auto action_it = frame.m_actions.rbegin(); action_it != frame.m_actions.rend();
             ++action_it) {
            const HistoryAction &action = *action_it;

            const int64_t sibling_count = m_model->getRowCount(action.m_parent);

            // We can only undo/redo index modifications, not insertions or resets.
            if ((action.m_action_flags & ModelEventFlags::EVENT_INDEX_MODIFIED) ==
                ModelEventFlags::EVENT_INDEX_MODIFIED) {

                ModelIndex old_index =
                    m_model->getIndex(action.m_row, action.m_column, action.m_parent);
                if (!m_model->removeIndex(old_index)) {
                    TOOLBOX_ERROR("Failed to remove index during undo/redo!");
                    return false;
                }

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

            // We can only undo/redo index modifications, not insertions or resets.
            if ((action.m_action_flags & ModelEventFlags::EVENT_INDEX_ADDED) ==
                ModelEventFlags::EVENT_INDEX_ADDED) {

                ModelIndex old_index =
                    m_model->getIndex(action.m_row, action.m_column, action.m_parent);
                if (!m_model->removeIndex(old_index)) {
                    TOOLBOX_ERROR("Failed to remove index during undo/redo!");
                    return false;
                }
            }

            // We can only undo/redo index modifications, not insertions or resets.
            if ((action.m_action_flags & ModelEventFlags::EVENT_INDEX_REMOVED) ==
                ModelEventFlags::EVENT_INDEX_REMOVED) {
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

        const HistoryFrame &frame = m_history[redo_frame];
        for (auto action_it = frame.m_actions.begin(); action_it != frame.m_actions.end();
             ++action_it) {
            const HistoryAction &action = *action_it;

            const int64_t sibling_count = m_model->getRowCount(action.m_parent);

            // We can only undo/redo index modifications, not insertions or resets.
            if ((action.m_action_flags & ModelEventFlags::EVENT_INDEX_MODIFIED) ==
                ModelEventFlags::EVENT_INDEX_MODIFIED) {

                ModelIndex old_index =
                    m_model->getIndex(action.m_row, action.m_column, action.m_parent);
                if (!m_model->removeIndex(old_index)) {
                    TOOLBOX_ERROR("Failed to remove index during undo/redo!");
                    return false;
                }

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
            if ((action.m_action_flags & ModelEventFlags::EVENT_INDEX_ADDED) ==
                ModelEventFlags::EVENT_INDEX_ADDED) {

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
            if ((action.m_action_flags & ModelEventFlags::EVENT_INDEX_REMOVED) ==
                ModelEventFlags::EVENT_INDEX_REMOVED) {
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

    bool ModelHistoryHandler::hasHistory() const { return !m_history.empty(); }

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

        if ((flags & ModelEventFlags::EVENT_RESET) == ModelEventFlags::EVENT_RESET) {
            m_history.clear();
            return;
        }

        if ((flags & ModelEventFlags::EVENT_INDEX_ANY) == ModelEventFlags::EVENT_NONE) {
            return;
        }

        if ((flags & ModelEventFlags::EVENT_PRE) == ModelEventFlags::EVENT_PRE) {
            const bool is_new_action =
                m_current_action.m_action_flags == ModelEventFlags::EVENT_NONE;
            if (!is_new_action) {
                TOOLBOX_DEBUG_LOG("[MODEL_HISTORY] Received a pre event without a corresponding "
                                  "post event. Resetting the current frame...");
            }

            // Start a new frame for this action.
            m_current_action.m_action_flags = static_cast<ModelEventFlags>(
                flags & ~(EVENT_SUCCESS | EVENT_PRE | EVENT_POST | EVENT_RESET));
            m_current_action.m_row    = m_model->getRow(index);
            m_current_action.m_column = m_model->getColumn(index);
            m_current_action.m_parent = m_model->getParent(index);

            if ((flags & ModelEventFlags::EVENT_INDEX_ADDED) == ModelEventFlags::EVENT_NONE) {
                m_current_action.m_undo_data = m_model->createMimeData({index});
            }

            TimePoint new_timestamp  = TimePoint::clock::now();
            TimePoint trim_timestamp = TimePoint::min();

            if (m_history_frame >= 0 && m_history_frame < m_history.size()) {
                const HistoryFrame &trim_frame = m_history[m_history_frame];
                trim_timestamp                 = trim_frame.m_timestamp;
            }
            
            if (m_trim_cb) {
                m_trim_cb(new_timestamp, trim_timestamp);

                if (is_new_action) {
                    m_history_frame += 1;
                }
            } else {
                if (is_new_action) {
                    m_history_frame += 1;
                }

                m_history.erase(m_history.begin() + m_history_frame, m_history.end());
            }

            m_history.emplace_back();
            m_history.back().m_timestamp = new_timestamp;
        } else {
            if (m_current_action.m_action_flags == ModelEventFlags::EVENT_NONE) {
                TOOLBOX_DEBUG_LOG("[MODEL_HISTORY] Received a post event without a corresponding "
                                  "pre event. Ignoring...");
                return;
            }
        }

        if ((flags & ModelEventFlags::EVENT_POST) == ModelEventFlags::EVENT_POST) {
            if ((flags & ModelEventFlags::EVENT_INDEX_REMOVED) == ModelEventFlags::EVENT_NONE) {
                m_current_action.m_redo_data = m_model->createMimeData({index});
            }

            m_history.back().m_actions.push_back(std::move(m_current_action));

            m_current_action = {};
        }
    }

    ModelHistoryAggregate::ModelHistoryAggregate(
        const std::vector<RefPtr<ModelHistoryHandler>> &handlers)
        : ModelHistoryHandler() {
        m_handlers.reserve(handlers.size());
        for (const RefPtr<ModelHistoryHandler> &handler : handlers) {
            addHandler(handler);
        }
    }

    bool ModelHistoryAggregate::undoFrame() {
        RefPtr<ModelHistoryHandler> most_recent_handler = nullptr;
        TimePoint most_recent_timepoint                 = TimePoint::min();

        for (RefPtr<ModelHistoryHandler> handler : m_handlers) {
            TimePoint handler_timepoint = handler->getUndoFrameTimepoint();
            if (handler_timepoint != TimePoint::min() &&
                handler_timepoint > most_recent_timepoint) {
                most_recent_timepoint = handler_timepoint;
                most_recent_handler   = handler;
            }
        }

        if (most_recent_handler) {
            if (most_recent_handler->undoFrame()) {
                m_frame_timepoint = most_recent_timepoint;
                return true;
            }
        }

        return false;
    }

    bool ModelHistoryAggregate::redoFrame() {
        RefPtr<ModelHistoryHandler> most_recent_handler = nullptr;
        TimePoint most_recent_timepoint                 = TimePoint::max();

        for (RefPtr<ModelHistoryHandler> handler : m_handlers) {
            TimePoint handler_timepoint = handler->getRedoFrameTimepoint();
            if (handler_timepoint != TimePoint::max() &&
                handler_timepoint < most_recent_timepoint) {
                most_recent_timepoint = handler_timepoint;
                most_recent_handler   = handler;
            }
        }

        if (most_recent_handler) {
            if (most_recent_handler->redoFrame()) {
                m_frame_timepoint = most_recent_timepoint;
                return true;
            }
        }

        return false;
    }

    bool ModelHistoryAggregate::hasHistory() const {
        bool has_history = false;
        for (RefPtr<ModelHistoryHandler> handler : m_handlers) {
            has_history |= handler->hasHistory();
        }
        return has_history;
    }

    TimePoint ModelHistoryAggregate::getUndoFrameTimepoint() const {
        TimePoint most_recent_timepoint = TimePoint::min();
        for (RefPtr<ModelHistoryHandler> handler : m_handlers) {
            TimePoint handler_timepoint = handler->getUndoFrameTimepoint();
            if (handler_timepoint > most_recent_timepoint) {
                most_recent_timepoint = handler_timepoint;
            }
        }
        return most_recent_timepoint;
    }

    TimePoint ModelHistoryAggregate::getRedoFrameTimepoint() const {
        TimePoint most_recent_timepoint = TimePoint::max();
        for (RefPtr<ModelHistoryHandler> handler : m_handlers) {
            TimePoint handler_timepoint = handler->getRedoFrameTimepoint();
            if (handler_timepoint < most_recent_timepoint) {
                most_recent_timepoint = handler_timepoint;
            }
        }
        return most_recent_timepoint;
    }

    void ModelHistoryAggregate::trimAllHistories(const TimePoint &new_point,
                                                 const TimePoint &) {
        TimePoint global_trim_point = getUndoFrameTimepoint();

        for (RefPtr<ModelHistoryHandler> handler : m_handlers) {
            handler->trimHistoryToTimePoint(global_trim_point);
        }

        m_frame_timepoint = new_point;
    }

}  // namespace Toolbox