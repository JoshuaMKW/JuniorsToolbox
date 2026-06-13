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

#include "core/mimedata/mimedata.hpp"
#include "core/types.hpp"
#include "fsystem.hpp"
#include "image/imagehandle.hpp"
#include "jsonlib.hpp"
#include "model/model.hpp"
#include "scene/hierarchy.hpp"
#include "unique.hpp"

#include "watchdog/fswatchdog.hpp"

using namespace std::chrono;

namespace Toolbox {

    enum SceneObjDataRole {
        SCENE_DATA_ROLE_OBJ_TYPE = ModelDataRole::DATA_ROLE_USER,
        SCENE_DATA_ROLE_OBJ_KEY,
        SCENE_DATA_ROLE_OBJ_GAME_ADDR,
        SCENE_DATA_ROLE_OBJ_REF,
        SCENE_DATA_ROLE_OBJ_WIZARD,
        SCENE_DATA_ROLE_OBJ_MEMBER_VALUE,
        SCENE_DATA_ROLE_OBJ_MEMBER_OFFSET,
        SCENE_DATA_ROLE_OBJ_MEMBER_SIZE,
        SCENE_DATA_ROLE_OBJ_TRANSFORM,
        SCENE_DATA_ROLE_OBJ_BOUNDING_BOX,
        SCENE_DATA_ROLE_OBJ_CAN_PERFORM,
        SCENE_DATA_ROLE_OBJ_IS_PERFORMING,
    };

    class SceneObjModel : public IDataModel {
    public:
        using json_t = nlohmann::ordered_json;

    public:
        SceneObjModel() = default;
        ~SceneObjModel();

        void initialize(const Scene::ObjectHierarchy &obj_tree);

        const fs_path &getScenePath() const { return m_scene_path; }
        void setScenePath(fs_path &&path) { m_scene_path = std::move(path); }

        [[nodiscard]] ScopePtr<Scene::ObjectHierarchy> bakeToHierarchy(std::string_view name) const;

        [[nodiscard]] UUID64 getUUID() const override { return m_uuid; }

        [[nodiscard]] size_t getObjectCount() const { return m_index_map.size(); }

        [[nodiscard]] bool validateIndex(const ModelIndex &index) const override {
            return IDataModel::validateIndex(index) && m_index_map.contains(index.getUUID());
        }

        [[nodiscard]] bool isReadOnly() const override { return false; }

        [[nodiscard]] std::string getObjectType(const ModelIndex &index) const {
            return std::any_cast<std::string>(
                getData(index, SceneObjDataRole::SCENE_DATA_ROLE_OBJ_TYPE));
        }

        [[nodiscard]] std::string getObjectKey(const ModelIndex &index) const {
            return std::any_cast<std::string>(
                getData(index, SceneObjDataRole::SCENE_DATA_ROLE_OBJ_KEY));
        }
        void setObjectKey(const ModelIndex &index, const std::string &key) {
            setData(index, key, SceneObjDataRole::SCENE_DATA_ROLE_OBJ_KEY);
        }

        [[nodiscard]] u32 getObjectGameAddress(const ModelIndex &index) const {
            return std::any_cast<u32>(
                getData(index, SceneObjDataRole::SCENE_DATA_ROLE_OBJ_GAME_ADDR));
        }

        [[nodiscard]] RefPtr<ISceneObject> getObjectRef(const ModelIndex &index) const {
            return std::any_cast<RefPtr<ISceneObject>>(
                getData(index, SceneObjDataRole::SCENE_DATA_ROLE_OBJ_REF));
        }

        [[nodiscard]] std::string getWizardName(const ModelIndex &index) const {
            return std::any_cast<std::string>(
                getData(index, SceneObjDataRole::SCENE_DATA_ROLE_OBJ_WIZARD));
        }
        void setWizardName(const ModelIndex &index, const std::string &wizard_name) {
            setData(index, wizard_name, SceneObjDataRole::SCENE_DATA_ROLE_OBJ_WIZARD);
        }

        [[nodiscard]] Result<MetaValue, MetaError> getMemberValue(const ModelIndex &index,
                                                                       const QualifiedName &member,
                                                                       size_t array_idx) const;
        Result<void, MetaError> setMemberValue(const ModelIndex &index,
                                                    const QualifiedName &member, size_t array_idx,
                                                    const MetaValue &value);
        [[nodiscard]] Result<u32, MetaScopeError>
        getMemberOffset(const ModelIndex &index, const QualifiedName &member) const;
        [[nodiscard]] Result<u32, MetaScopeError> getMemberSize(const ModelIndex &index,
                                                                const QualifiedName &member) const;

        [[nodiscard]] std::optional<Transform> getObjectTransform(const ModelIndex &index) const {
            return std::any_cast<std::optional<Transform>>(
                getData(index, SceneObjDataRole::SCENE_DATA_ROLE_OBJ_TRANSFORM));
        }
        void setObjectTransform(const ModelIndex &index, const Transform &transform) {
            setData(index, transform, SceneObjDataRole::SCENE_DATA_ROLE_OBJ_TRANSFORM);
        }

        [[nodiscard]] std::optional<BoundingBox>
        getObjectBoundingBox(const ModelIndex &index) const {
            return std::any_cast<BoundingBox>(
                getData(index, SceneObjDataRole::SCENE_DATA_ROLE_OBJ_BOUNDING_BOX));
        }

        [[nodiscard]] bool getObjectCanPerform(const ModelIndex &index) const {
            return std::any_cast<bool>(
                getData(index, SceneObjDataRole::SCENE_DATA_ROLE_OBJ_CAN_PERFORM));
        }

        [[nodiscard]] bool getObjectIsPerforming(const ModelIndex &index) const {
            return std::any_cast<bool>(
                getData(index, SceneObjDataRole::SCENE_DATA_ROLE_OBJ_IS_PERFORMING));
        }

        [[nodiscard]] std::any getData(const ModelIndex &index, int role) const override;
        void setData(const ModelIndex &index, std::any data, int role) override;

        [[nodiscard]] std::string findUniqueName(const ModelIndex &index,
                                                 const std::string &name) const;

    public:
        [[nodiscard]] ModelIndex getIndex(RefPtr<ISceneObject> object) const;
        [[nodiscard]] ModelIndex getIndex(const UUID64 &uuid) const override;
        [[nodiscard]] ModelIndex getIndex(int64_t row, int64_t column,
                                          const ModelIndex &parent = ModelIndex()) const override;
        [[nodiscard]] ModelIndex getIndex(const QualifiedName &qual_name,
                                          const ModelIndex &parent = ModelIndex()) const;
        [[nodiscard]] ModelIndex getIndex(const std::string &obj_type,
                                          std::optional<std::string> obj_name,
                                          const ModelIndex &parent = ModelIndex()) const;

        [[nodiscard]] virtual ModelIndex insertObject(RefPtr<ISceneObject> object, int64_t row,
                                                      const ModelIndex &parent);
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

        [[nodiscard]] virtual ModelIndex insertObject_(RefPtr<ISceneObject> object, int64_t row,
                                                       const ModelIndex &parent);
        [[nodiscard]] virtual ModelIndex makeIndex(RefPtr<ISceneObject> object, int64_t row,
                                                   const ModelIndex &parent) const;

        [[nodiscard]] Result<MetaValue, MetaError> getMemberValue_(const ModelIndex &index,
                                                                        const QualifiedName &member,
                                                                        size_t array_idx) const;
        Result<void, MetaError> setMemberValue_(const ModelIndex &index,
                                                     const QualifiedName &member, size_t array_idx,
                                                     const MetaValue &value);
        [[nodiscard]] Result<u32, MetaScopeError>
        getMemberOffset_(const ModelIndex &index, const QualifiedName &member) const;
        [[nodiscard]] Result<u32, MetaScopeError> getMemberSize_(const ModelIndex &index,
                                                                 const QualifiedName &member) const;

        // Implementation of public API for mutex locking reasons
        [[nodiscard]] std::any getData_(const ModelIndex &index, int role) const;
        bool setData_(const ModelIndex &index, std::any data, int role) const;

        [[nodiscard]] std::string findUniqueName_(const ModelIndex &index,
                                                  const std::string &name) const;

        [[nodiscard]] ModelIndex getIndex_(RefPtr<ISceneObject> object) const;
        [[nodiscard]] ModelIndex getIndex_(const UUID64 &uuid) const;
        [[nodiscard]] ModelIndex getIndex_(int64_t row, int64_t column,
                                           const ModelIndex &parent = ModelIndex()) const;
        [[nodiscard]] ModelIndex getIndex_(const QualifiedName &qual_name,
                                           const ModelIndex &parent = ModelIndex()) const;
        [[nodiscard]] ModelIndex getIndex_(const std::string &obj_type,
                                           std::optional<std::string> obj_name,
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

        void pruneRedundantIndexes(IDataModel::index_container &indexes) const;

    private:
        UUID64 m_uuid;

        mutable std::mutex m_mutex;
        std::unordered_map<UUID64, std::pair<event_listener_t, int>> m_listeners;

        mutable UUID64 m_root_index;
        mutable std::map<UUID64, ModelIndex> m_index_map;
        mutable std::map<UUID64, ModelIndex> m_obj_to_index_map;

        fs_path m_scene_path;

        ModelIndex m_last_event_index;
        int m_last_event_flags = static_cast<int>(ModelEventFlags::EVENT_NONE);
    };

}  // namespace Toolbox