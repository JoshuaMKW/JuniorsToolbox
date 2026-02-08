#include <algorithm>
#include <compare>
#include <set>

#include "model/objmodel.hpp"
#include "objlib/object.hpp"
#include "project/config.hpp"
#include "scene/hierarchy.hpp"

#include <gcm.hpp>

using namespace std::chrono;

namespace Toolbox {

    struct _SceneIndexData {
        UUID64 m_self_uuid                    = 0;
        RefPtr<Object::ISceneObject> m_object = nullptr;
        RefPtr<const ImageHandle> m_icon      = nullptr;

        std::strong_ordering operator<=>(const _SceneIndexData &rhs) const {
            return m_object->getQualifiedName().toString() <=>
                   rhs.m_object->getQualifiedName().toString();
        }
    };

    static bool _SceneIndexDataCompareByName(const _SceneIndexData &lhs, const _SceneIndexData &rhs,
                                             ModelSortOrder order) {
        // Sort by name
        if (order == ModelSortOrder::SORT_ASCENDING) {
            return lhs.m_object->getNameRef().name() < rhs.m_object->getNameRef().name();
        } else {
            return lhs.m_object->getNameRef().name() > rhs.m_object->getNameRef().name();
        }
    }

    SceneObjModel::~SceneObjModel() { reset(); }

    static void ObjectForEach(RefPtr<ISceneObject> root,
                              std::function<void(RefPtr<ISceneObject> object)> fn) {
        if (!root) {
            return;
        }

        fn(root);
        for (RefPtr<ISceneObject> child : root->getChildren()) {
            ObjectForEach(child, fn);
        }
    }

    void SceneObjModel::initialize(const Scene::ObjectHierarchy &hierarchy) {
        m_index_map.clear();

        bool result = ObjectForEach(
            hierarchy.getRoot(), 0, nullptr,
            [this](RefPtr<ISceneObject> object, int64_t row, RefPtr<ISceneObject> parent) {
                ModelIndex parent_index = getIndex(parent);
                ModelIndex new_index    = makeIndex(object, row, parent_index);
                if (!validateIndex(new_index)) {
                    TOOLBOX_ERROR_V("[OBJMODEL] Failed to make index for object {} ({})",
                                    object->type(), object->getNameRef().name());
                    return false;
                }
                return true;
            });

        if (!result) {
            TOOLBOX_ERROR("[OBJMODEL] Failed to initialize model from given ObjectHierarchy!");
        }
    }

    std::any SceneObjModel::getData(const ModelIndex &index, int role) const {
        std::scoped_lock lock(m_mutex);
        return getData_(index, role);
    }

    void SceneObjModel::setData(const ModelIndex &index, std::any data, int role) {
        std::scoped_lock lock(m_mutex);
        setData_(index, data, role);
    }

    std::string SceneObjModel::findUniqueName(const ModelIndex &index,
                                              const std::string &name) const {
        std::scoped_lock lock(m_mutex);
        return findUniqueName_(index, name);
    }

    ModelIndex SceneObjModel::getIndex(RefPtr<ISceneObject> object) const {
        if (!object) {
            return ModelIndex();
        }

        std::scoped_lock lock(m_mutex);
        return getIndex_(object);
    }

    ModelIndex SceneObjModel::getIndex(const UUID64 &uuid) const {
        std::scoped_lock lock(m_mutex);
        return getIndex_(uuid);
    }

    ModelIndex SceneObjModel::getIndex(int64_t row, int64_t column,
                                       const ModelIndex &parent) const {
        std::scoped_lock lock(m_mutex);
        return getIndex_(row, column, parent);
    }

    bool SceneObjModel::removeIndex(const ModelIndex &index) {
        bool result;

        const Signal index_signal =
            createSignalForIndex_(index, ModelEventFlags::EVENT_INDEX_REMOVED);

        signalEventListeners(index_signal.first, index_signal.second | ModelEventFlags::EVENT_PRE);

        {
            std::scoped_lock lock(m_mutex);
            result = removeIndex_(index);
        }

        if (result) {
            signalEventListeners(index_signal.first, index_signal.second |
                                                         ModelEventFlags::EVENT_POST |
                                                         ModelEventFlags::EVENT_SUCCESS);
        } else {
            signalEventListeners(index_signal.first,
                                 index_signal.second | ModelEventFlags::EVENT_POST);
        }

        return result;
    }

    ModelIndex SceneObjModel::insertObject(RefPtr<ISceneObject> object, int64_t row,
                                           const ModelIndex &parent) {
        ModelIndex new_index = makeIndex(object, row, parent);
        if (!validateIndex(new_index)) {
            return ModelIndex();
        }

        _SceneIndexData *parent_data = validateIndex(parent) ? parent.data<_SceneIndexData>()
                                                             : nullptr;
        if (parent_data) {
            auto result = parent_data->m_object->insertChild(row, object);
            if (!result) {
                TOOLBOX_ERROR_V("[OBJMODEL] Failed to create inde with reason: '{}'",
                                result.error().m_message);
                (void)removeIndex(new_index);
                return ModelIndex();
            }
        }

        return new_index;
    }

    ModelIndex SceneObjModel::getParent(const ModelIndex &index) const {
        std::scoped_lock lock(m_mutex);
        return getParent_(index);
    }

    ModelIndex SceneObjModel::getSibling(int64_t row, int64_t column,
                                         const ModelIndex &index) const {
        std::scoped_lock lock(m_mutex);
        return getSibling_(row, column, index);
    }

    size_t SceneObjModel::getColumnCount(const ModelIndex &index) const {
        std::scoped_lock lock(m_mutex);
        return getColumnCount_(index);
    }

    size_t SceneObjModel::getRowCount(const ModelIndex &index) const {
        std::scoped_lock lock(m_mutex);
        return getRowCount_(index);
    }

    int64_t SceneObjModel::getColumn(const ModelIndex &index) const {
        std::scoped_lock lock(m_mutex);
        return getColumn_(index);
    }

    int64_t SceneObjModel::getRow(const ModelIndex &index) const {
        std::scoped_lock lock(m_mutex);
        return getRow_(index);
    }

    bool SceneObjModel::hasChildren(const ModelIndex &parent) const {
        std::scoped_lock lock(m_mutex);
        return hasChildren_(parent);
    }

    ScopePtr<MimeData>
    SceneObjModel::createMimeData(const IDataModel::index_container &indexes) const {
        std::scoped_lock lock(m_mutex);
        return createMimeData_(indexes);
    }

    bool SceneObjModel::insertMimeData(const ModelIndex &index, const MimeData &data,
                                       ModelInsertPolicy policy) {
        bool result;

        {
            std::scoped_lock lock(m_mutex);
            result = insertMimeData_(index, data, policy);
        }

        return result;
    }

    std::vector<std::string> SceneObjModel::getSupportedMimeTypes() const {
        return std::vector<std::string>();
    }

    bool SceneObjModel::canFetchMore(const ModelIndex &index) {
        std::scoped_lock lock(m_mutex);
        return canFetchMore_(index);
    }

    void SceneObjModel::fetchMore(const ModelIndex &index) {
        std::scoped_lock lock(m_mutex);
        return fetchMore_(index);
    }

    void SceneObjModel::reset() {
        std::scoped_lock lock(m_mutex);

        for (auto &[key, val] : m_index_map) {
            _SceneIndexData *p = val.data<_SceneIndexData>();
            if (p) {
                delete p;
            }
        }

        m_index_map.clear();
    }

    void SceneObjModel::addEventListener(UUID64 uuid, event_listener_t listener,
                                         int allowed_flags) {
        m_listeners[uuid] = {listener, allowed_flags};
    }

    void SceneObjModel::removeEventListener(UUID64 uuid) { m_listeners.erase(uuid); }

    // const ImageHandle &SceneObjModel::InvalidIcon() {
    //     static ImageHandle s_invalid_fs_icon = ImageHandle("Images/Icons/fs_invalid.png");
    //     return s_invalid_fs_icon;
    // }

    SceneObjModel::Signal SceneObjModel::createSignalForIndex_(const ModelIndex &index,
                                                               ModelEventFlags base_event) const {
        return {index, (int)base_event};
    }

    std::any SceneObjModel::getData_(const ModelIndex &index, int role) const {
        if (!validateIndex(index)) {
            return {};
        }

        RefPtr<ISceneObject> object = index.data<_SceneIndexData>()->m_object;
        if (!object) {
            return {};
        }

        switch (role) {
        case ModelDataRole::DATA_ROLE_DISPLAY:
            return std::format("{} ({})", object->type(), object->getNameRef().name());
        case ModelDataRole::DATA_ROLE_TOOLTIP:
            return "Tooltip unimplemented!";
        case ModelDataRole::DATA_ROLE_DECORATION: {
            return index.data<_SceneIndexData>()->m_icon;
        }
        case SceneObjDataRole::SCENE_DATA_ROLE_OBJ_TYPE: {
            return object->type();
        }
        case SceneObjDataRole::SCENE_DATA_ROLE_OBJ_KEY: {
            return std::string(object->getNameRef().name());
        }
        case SceneObjDataRole::SCENE_DATA_ROLE_OBJ_GAME_ADDR: {
            return object->getGamePtr();
        }
        case SceneObjDataRole::SCENE_DATA_ROLE_OBJ_REF: {
            return object;  // We do this since objects are complex and may require direct
                            // interaction
        }
        default:
            return {};
        }
    }

    void SceneObjModel::setData_(const ModelIndex &index, std::any data, int role) const {
        if (!validateIndex(index)) {
            return;
        }

        RefPtr<ISceneObject> object = index.data<_SceneIndexData>()->m_object;
        if (!object) {
            return;
        }

        switch (role) {
        case ModelDataRole::DATA_ROLE_DISPLAY:
            break;
        case ModelDataRole::DATA_ROLE_DECORATION: {
            index.data<_SceneIndexData>()->m_icon = std::any_cast<RefPtr<const ImageHandle>>(data);
            break;
        }
        case SceneObjDataRole::SCENE_DATA_ROLE_OBJ_TYPE: {
            break;
        }
        case SceneObjDataRole::SCENE_DATA_ROLE_OBJ_KEY: {
            object->setNameRef(NameRef(std::any_cast<std::string>(data)));
            break;
        }
        case SceneObjDataRole::SCENE_DATA_ROLE_OBJ_GAME_ADDR: {
            break;
        }
        case SceneObjDataRole::SCENE_DATA_ROLE_OBJ_REF: {
            break;
        }
        default:
            return;
        }
    }

    std::string SceneObjModel::findUniqueName_(const ModelIndex &index,
                                               const std::string &name) const {
        if (!validateIndex(index)) {
            return name;
        }

        std::string result_name = name;
        size_t collisions       = 0;
        std::vector<std::string> child_paths;

        for (size_t i = 0; i < getRowCount_(index); ++i) {
            ModelIndex child                  = getIndex_(i, 0, index);
            RefPtr<ISceneObject> child_object = child.data<_SceneIndexData>()->m_object;
            child_paths.emplace_back(std::move(std::string(child_object->getNameRef().name())));
        }

        for (size_t i = 0; i < child_paths.size();) {
            if (child_paths[i] == result_name) {
                collisions += 1;
                result_name = std::format("{} ({})", name, collisions);
                i           = 0;
                continue;
            }
            ++i;
        }

        return result_name;
    }

    ModelIndex SceneObjModel::getIndex_(RefPtr<ISceneObject> object) const {
        for (const auto &[k, v] : m_index_map) {
            RefPtr<ISceneObject> other = v.data<_SceneIndexData>()->m_object;
            if (other->getUUID() == object->getUUID()) {
                return v;
            }
        }
        return ModelIndex();
    }

    ModelIndex SceneObjModel::getIndex_(const UUID64 &uuid) const {
        if (!m_index_map.contains(uuid)) {
            return ModelIndex();
        }
        return m_index_map.at(uuid);
    }

    ModelIndex SceneObjModel::getIndex_(int64_t row, int64_t column,
                                        const ModelIndex &parent) const {
        if (!validateIndex(parent)) {
            if (row != 0 || column != 0) {
                return ModelIndex();
            }
            return m_index_map[m_root_index];
        }

        RefPtr<ISceneObject> parent_obj      = parent.data<_SceneIndexData>()->m_object;
        std::vector<RefPtr<ISceneObject>> children = parent_obj->getChildren();

        if (row < 0 || row >= static_cast<int64_t>(children.size()) || column != 0) {
            return ModelIndex();
        }

        RefPtr<ISceneObject> child_obj = children[row];
        return getIndex_(child_obj);
    }

    bool SceneObjModel::removeIndex_(const ModelIndex &index) {
        if (!validateIndex(index)) {
            return false;
        }

        ModelIndex parent            = getParent_(index);
        _SceneIndexData *parent_data = validateIndex(parent) ? parent.data<_SceneIndexData>()
                                                             : nullptr;
        _SceneIndexData *this_data   = index.data<_SceneIndexData>();
        if (parent_data) {
            auto result = parent_data->m_object->removeChild(this_data->m_object);
            if (!result) {
                TOOLBOX_ERROR_V("[OBJMODEL] Failed to remove index with reason: '{}'",
                                result.error().m_message);
                return false;
            }
        }

        const UUID64 uuid = index.getUUID();
        m_index_map.erase(uuid);
        return true;
    }

    ModelIndex SceneObjModel::getParent_(const ModelIndex &index) const {
        if (!validateIndex(index)) {
            return ModelIndex();
        }

        _SceneIndexData *data = index.data<_SceneIndexData>();
        RefPtr<ISceneObject> parent = get_shared_ptr(*data->m_object->getParent());

        return getIndex_(parent);
    }

    ModelIndex SceneObjModel::getSibling_(int64_t row, int64_t column,
                                          const ModelIndex &index) const {
        if (!validateIndex(index)) {
            return ModelIndex();
        }

        ModelIndex parent_index = getParent_(index);
        return getIndex_(row, column, parent_index);
    }

    size_t SceneObjModel::getColumnCount_(const ModelIndex &index) const {
        if (!validateIndex(index)) {
            return 1;
        }

        return 1;
    }

    size_t SceneObjModel::getRowCount_(const ModelIndex &index) const {
        if (!validateIndex(index)) {
            return 1;
        }

        _SceneIndexData *data = index.data<_SceneIndexData>();
        if (!data) {
            return 0;
        }

        return data->m_object->getChildren().size();
    }

    int64_t SceneObjModel::getColumn_(const ModelIndex &index) const {
        return validateIndex(index) ? 0 : -1;
    }

    int64_t SceneObjModel::getRow_(const ModelIndex &index) const {
        if (!validateIndex(index)) {
            return -1;
        }

        _SceneIndexData *data = index.data<_SceneIndexData>();
        if (!data) {
            return -1;
        }

        RefPtr<ISceneObject> self = data->m_object;
        ISceneObject *parent = self->getParent();
        if (!parent) {
            return 0;
        }

        int64_t row = 0;
        for (RefPtr<ISceneObject> child : parent->getChildren()) {
            if (child->getUUID() == self->getUUID()) {
                return row;
            }
            ++row;
        }

        return -1;
    }

    bool SceneObjModel::hasChildren_(const ModelIndex &parent) const { return false; }

    ScopePtr<MimeData>
    SceneObjModel::createMimeData_(const IDataModel::index_container &indexes) const {
        return nullptr;
    }

    bool SceneObjModel::insertMimeData_(const ModelIndex &index, const MimeData &data,
                                        ModelInsertPolicy policy) {
        return false;
    }

    bool SceneObjModel::canFetchMore_(const ModelIndex &index) const { return false; }

    void SceneObjModel::fetchMore_(const ModelIndex &index) const { return; }

    ModelIndex SceneObjModel::makeIndex(RefPtr<ISceneObject> object, int64_t row,
                                        const ModelIndex &parent) const {
        if (row < 0 || row > m_index_map.size()) {
            return ModelIndex();
        }

        ModelIndex new_index(getUUID());

        _SceneIndexData *new_data = new _SceneIndexData;
        new_data->m_self_uuid     = new_index.getUUID();
        new_data->m_object        = object;
        new_data->m_icon          = nullptr;

        new_index.setData(new_data);

        m_index_map[new_index.getUUID()] = new_index;

        if (row == 0 && !validateIndex(parent)) {
            m_root_index = new_index.getUUID();
        }

        return new_index;
    }

    void SceneObjModel::signalEventListeners(const ModelIndex &index, int flags) {
        for (const auto &[key, listener] : m_listeners) {
            if ((listener.second & flags) != ModelEventFlags::EVENT_NONE) {
                listener.first(index, (listener.second & flags));
            }
        }
    }

}  // namespace Toolbox
