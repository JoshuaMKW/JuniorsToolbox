#pragma once

#include <algorithm>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "bmg/bmg.hpp"
#include "core/mimedata/mimedata.hpp"
#include "core/types.hpp"
#include "fsystem.hpp"
#include "image/imagehandle.hpp"
#include "jsonlib.hpp"
#include "model/model.hpp"
#include "unique.hpp"

#include "watchdog/fswatchdog.hpp"

using namespace std::chrono;

namespace Toolbox {

    enum BMGDataRole {
        BMG_DATA_ROLE_MESSAGE_NAME = ModelDataRole::DATA_ROLE_USER,
        BMG_DATA_ROLE_MESSAGE_TEXT,
        BMG_DATA_ROLE_MESSAGE_SOUND_ID,
        BMG_DATA_ROLE_MESSAGE_START_FRAME,
        BMG_DATA_ROLE_MESSAGE_END_FRAME,
        BMG_DATA_ROLE_MESSAGE_ENTRY,
    };

    class BMGModel : public IDataModel {
    public:
        using json_t = nlohmann::ordered_json;

    public:
        BMGModel() = default;
        ~BMGModel();

        void initialize(const BMG::MessageData &message_data);
        [[nodiscard]] ScopePtr<BMG::MessageData> bakeToMessageData() const;

        [[nodiscard]] UUID64 getUUID() const override { return m_uuid; }

        [[nodiscard]] bool validateIndex(const ModelIndex &index) const override {
            return IDataModel::validateIndex(index) && m_index_map.contains(index.getUUID());
        }

        [[nodiscard]] bool isReadOnly() const override { return false; }

        [[nodiscard]] BMG::MessageFlagSize getFlagSize() const { return m_flag_size; }
        void setFlagSize(BMG::MessageFlagSize flag_size) { m_flag_size = flag_size; }

        [[nodiscard]] bool isNTSC() const { return m_ntsc; }
        void setNTSC(bool ntsc) { m_ntsc = ntsc; }

        [[nodiscard]] std::string_view getMessageName(const ModelIndex &index) const {
            return std::any_cast<std::string_view>(
                getData(index, BMGDataRole::BMG_DATA_ROLE_MESSAGE_NAME));
        }
        void setMessageName(const ModelIndex &index, const std::string &name) {
            setData(index, name, BMGDataRole::BMG_DATA_ROLE_MESSAGE_NAME);
        }

        [[nodiscard]] std::string_view getMessageText(const ModelIndex &index) const {
            return std::any_cast<std::string_view>(
                getData(index, BMGDataRole::BMG_DATA_ROLE_MESSAGE_TEXT));
        }
        void setMessageText(const ModelIndex &index, const std::string &text) {
            setData(index, text, BMGDataRole::BMG_DATA_ROLE_MESSAGE_TEXT);
        }

        [[nodiscard]] BMG::MessageSound getMessageSoundID(const ModelIndex &index) const {
            return std::any_cast<BMG::MessageSound>(
                getData(index, BMGDataRole::BMG_DATA_ROLE_MESSAGE_SOUND_ID));
        }
        void setMessageSoundID(const ModelIndex &index, BMG::MessageSound sound) {
            setData(index, sound, BMGDataRole::BMG_DATA_ROLE_MESSAGE_SOUND_ID);
        }

        [[nodiscard]] u16 getMessageStartFrame(const ModelIndex &index) const {
            return std::any_cast<u16>(
                getData(index, BMGDataRole::BMG_DATA_ROLE_MESSAGE_START_FRAME));
        }
        void setMessageStartFrame(const ModelIndex &index, u16 frame) {
            setData(index, frame, BMGDataRole::BMG_DATA_ROLE_MESSAGE_START_FRAME);
        }

        [[nodiscard]] u16 getMessageEndFrame(const ModelIndex &index) const {
            return std::any_cast<u16>(getData(index, BMGDataRole::BMG_DATA_ROLE_MESSAGE_END_FRAME));
        }
        void setMessageEndFrame(const ModelIndex &index, u16 frame) {
            setData(index, frame, BMGDataRole::BMG_DATA_ROLE_MESSAGE_END_FRAME);
        }

        [[nodiscard]] BMG::MessageData::Entry getMessageEntry(const ModelIndex &index) const {
            return std::any_cast<BMG::MessageData::Entry>(
                getData(index, BMGDataRole::BMG_DATA_ROLE_MESSAGE_ENTRY));
        }

        [[nodiscard]] std::any getData(const ModelIndex &index, int role) const override;
        void setData(const ModelIndex &index, std::any data, int role) override;

        [[nodiscard]] std::string findUniqueName(const ModelIndex &index,
                                                 const std::string &name) const;

    public:
        [[nodiscard]] ModelIndex getIndex(const UUID64 &uuid) const override;
        [[nodiscard]] ModelIndex getIndex(int64_t row, int64_t column,
                                          const ModelIndex &parent = ModelIndex()) const override;

        [[nodiscard]] virtual ModelIndex insertMessageEntry(const BMG::MessageData::Entry &entry,
                                                            int64_t row);
        [[nodiscard]] bool removeIndex(const ModelIndex &index) override;

        [[nodiscard]] ModelIndex getParent(const ModelIndex &index) const override;
        [[nodiscard]] ModelIndex getSibling(int64_t row, int64_t column,
                                            const ModelIndex &index) const override;

        [[nodiscard]] size_t getColumnCount(const ModelIndex &index) const override;
        [[nodiscard]] size_t getRowCount(const ModelIndex &index) const override;

        [[nodiscard]] int64_t getColumn(const ModelIndex &index) const override;
        [[nodiscard]] int64_t getRow(const ModelIndex &index) const override;

        [[nodiscard]] bool hasChildren(const ModelIndex &parent = ModelIndex()) const override;

        [[nodiscard]] ScopePtr<MimeData>
        createMimeData(const IDataModel::index_container &indexes) const override;
        [[nodiscard]] Result<IDataModel::index_container>
        insertMimeData(const ModelIndex &index, const MimeData &data,
                       ModelInsertPolicy policy = ModelInsertPolicy::INSERT_AFTER) override;
        [[nodiscard]] std::vector<std::string> getSupportedMimeTypes() const override;

        [[nodiscard]] bool canFetchMore(const ModelIndex &index) override;
        void fetchMore(const ModelIndex &index) override;

        void reset() override;

        void addEventListener(UUID64 uuid, event_listener_t listener, int allowed_flags) override;
        void removeEventListener(UUID64 uuid) override;

    protected:
        using Signal      = std::pair<ModelIndex, int>;
        using SignalQueue = std::vector<Signal>;

        [[nodiscard]] virtual Signal createSignalForIndex_(const ModelIndex &index,
                                                           ModelEventFlags base_event) const;

        [[nodiscard]] virtual ModelIndex
        insertMessageEntry_(const BMG::MessageData::Entry &entry, int64_t row,
                            std::optional<UUID64> index_uuid = std::nullopt);
        [[nodiscard]] virtual ModelIndex
        makeIndex(const BMG::MessageData::Entry &entry, int64_t row,
                  std::optional<UUID64> index_uuid = std::nullopt) const;

        // Implementation of public API for mutex locking reasons
        [[nodiscard]] std::any getData_(const ModelIndex &index, int role) const;
        void setData_(const ModelIndex &index, std::any data, int role) const;

        [[nodiscard]] std::string findUniqueName_(const ModelIndex &index,
                                                  const std::string &name) const;

        [[nodiscard]] ModelIndex getIndex_(const UUID64 &uuid) const;
        [[nodiscard]] ModelIndex getIndex_(int64_t row, int64_t column,
                                           const ModelIndex &parent = ModelIndex()) const;
        [[nodiscard]] bool removeIndex_(const ModelIndex &index);

        [[nodiscard]] ModelIndex getParent_(const ModelIndex &index) const;
        [[nodiscard]] ModelIndex getSibling_(int64_t row, int64_t column,
                                             const ModelIndex &index) const;

        [[nodiscard]] size_t getColumnCount_(const ModelIndex &index) const;
        [[nodiscard]] size_t getRowCount_(const ModelIndex &index) const;

        [[nodiscard]] int64_t getColumn_(const ModelIndex &index) const;
        [[nodiscard]] int64_t getRow_(const ModelIndex &index) const;

        [[nodiscard]] bool hasChildren_(const ModelIndex &parent = ModelIndex()) const;

        [[nodiscard]] ScopePtr<MimeData>
        createMimeData_(const IDataModel::index_container &indexes) const;
        [[nodiscard]] Result<IDataModel::index_container>
        insertMimeData_(const ModelIndex &index, const MimeData &data,
                        ModelInsertPolicy policy = ModelInsertPolicy::INSERT_AFTER);

        [[nodiscard]] bool canFetchMore_(const ModelIndex &index) const;
        void fetchMore_(const ModelIndex &index) const;
        // -- END -- //

        void signalEventListeners(const ModelIndex &index, int flags);

    private:
        UUID64 m_uuid;

        mutable std::mutex m_mutex;
        std::unordered_map<UUID64, std::pair<event_listener_t, int>> m_listeners;

        mutable std::vector<ModelIndex> m_message_indexes;
        mutable std::unordered_map<UUID64, ModelIndex> m_index_map;

        BMG::MessageFlagSize m_flag_size = BMG::MessageFlagSize::FLAG_SIZE_NPC;
        bool m_ntsc = true;
    };

}  // namespace Toolbox