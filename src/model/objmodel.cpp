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

        std::string m_display_text_cache = "";

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

    using for_each_fn =
        std::function<void(RefPtr<ISceneObject> object, int64_t row, RefPtr<ISceneObject> parent)>;

    static bool ObjectForEach(RefPtr<ISceneObject> object, int64_t row, RefPtr<ISceneObject> parent,
                              for_each_fn fn) {
        if (!object) {
            return false;
        }

        fn(object, row, parent);

        int64_t i = 0;
        for (RefPtr<ISceneObject> child : object->getChildren()) {
            if (!ObjectForEach(child, i++, object, fn)) {
                return false;  // Early exit
            }
        }

        return true;
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

    ScopePtr<Scene::ObjectHierarchy> SceneObjModel::bakeToHierarchy(std::string_view name) const {
        ModelIndex root_index = getIndex(m_root_index);
        if (!validateIndex(root_index)) {
            return nullptr;
        }

        RefPtr<ISceneObject> root_obj = getObjectRef(root_index);
        if (!root_obj->isGroupObject()) {
            return nullptr;
        }

        return make_scoped<Scene::ObjectHierarchy>(name, ref_cast<GroupSceneObject>(root_obj));
    }

    Result<MetaValue, MetaError> SceneObjModel::getMemberValue(const ModelIndex &index,
                                                                    const QualifiedName &member,
                                                                    size_t array_idx) const {
        std::scoped_lock lock(m_mutex);
        return getMemberValue_(index, member, array_idx);
    }

    Result<void, MetaError> SceneObjModel::setMemberValue(const ModelIndex &index,
                                                               const QualifiedName &member,
                                                               size_t array_idx,
                                                               const MetaValue &value) {
        Result<void, MetaError> result;

        const Signal index_signal =
            createSignalForIndex_(index, ModelEventFlags::EVENT_INDEX_MODIFIED);

        signalEventListeners(index_signal.first, index_signal.second | ModelEventFlags::EVENT_PRE);

        {
            std::scoped_lock lock(m_mutex);
            result = setMemberValue_(index, member, array_idx, value);
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

    Result<u32, MetaScopeError> SceneObjModel::getMemberOffset(const ModelIndex &index,
                                                               const QualifiedName &member) const {
        std::scoped_lock lock(m_mutex);
        return getMemberOffset_(index, member);
    }

    Result<u32, MetaScopeError> SceneObjModel::getMemberSize(const ModelIndex &index,
                                                             const QualifiedName &member) const {
        std::scoped_lock lock(m_mutex);
        return getMemberSize_(index, member);
    }

    std::any SceneObjModel::getData(const ModelIndex &index, int role) const {
        std::scoped_lock lock(m_mutex);
        return getData_(index, role);
    }

    void SceneObjModel::setData(const ModelIndex &index, std::any data, int role) {
        const Signal index_signal =
            createSignalForIndex_(index, ModelEventFlags::EVENT_INDEX_MODIFIED);

        signalEventListeners(index_signal.first, index_signal.second | ModelEventFlags::EVENT_PRE);

        {
            std::scoped_lock lock(m_mutex);
            setData_(index, data, role);
        }

        signalEventListeners(index_signal.first, index_signal.second | ModelEventFlags::EVENT_POST |
                                                     ModelEventFlags::EVENT_SUCCESS);
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

    ModelIndex SceneObjModel::getIndex(const QualifiedName &qual_name,
                                       const ModelIndex &parent) const {
        std::scoped_lock lock(m_mutex);
        return getIndex_(qual_name, parent);
    }

    ModelIndex SceneObjModel::getIndex(const std::string &obj_type,
                                       std::optional<std::string> obj_name,
                                       const ModelIndex &parent) const {
        std::scoped_lock lock(m_mutex);
        return getIndex_(obj_type, obj_name, parent);
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
        ModelIndex result;

        const Signal index_signal = createSignalForIndex_(parent, ModelEventFlags::EVENT_INSERT);

        signalEventListeners(index_signal.first, index_signal.second | ModelEventFlags::EVENT_PRE);

        {
            std::scoped_lock lock(m_mutex);
            result = insertObject_(object, row, parent);
        }

        if (validateIndex(result)) {
            const Signal add_signal =
                createSignalForIndex_(result, ModelEventFlags::EVENT_INDEX_ADDED);
            signalEventListeners(index_signal.first, index_signal.second |
                                                         ModelEventFlags::EVENT_POST |
                                                         ModelEventFlags::EVENT_SUCCESS);
            signalEventListeners(add_signal.first, add_signal.second | ModelEventFlags::EVENT_POST |
                                                       ModelEventFlags::EVENT_SUCCESS);
        } else {
            signalEventListeners(index_signal.first,
                                 index_signal.second | ModelEventFlags::EVENT_POST);
        }

        return result;
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

    Result<IDataModel::index_container> SceneObjModel::insertMimeData(const ModelIndex &index,
                                                                      const MimeData &data,
                                                                      ModelInsertPolicy policy) {
        Result<IDataModel::index_container> result;

        const Signal pre_signal = createSignalForIndex_(index, ModelEventFlags::EVENT_INSERT);

        signalEventListeners(pre_signal.first, pre_signal.second | ModelEventFlags::EVENT_PRE);

        {
            std::scoped_lock lock(m_mutex);
            result = insertMimeData_(index, data, policy);
        }

        if (result) {
            for (const ModelIndex &new_index : result.value()) {
                Signal index_signal =
                    createSignalForIndex_(new_index, ModelEventFlags::EVENT_INDEX_ADDED);
                signalEventListeners(index_signal.first, index_signal.second |
                                                             ModelEventFlags::EVENT_POST |
                                                             ModelEventFlags::EVENT_SUCCESS);
            }
            signalEventListeners(pre_signal.first, pre_signal.second | ModelEventFlags::EVENT_POST |
                                                       ModelEventFlags::EVENT_SUCCESS);
        } else {
            signalEventListeners(pre_signal.first, pre_signal.second | ModelEventFlags::EVENT_POST);
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
            return index.data<_SceneIndexData>()->m_display_text_cache;
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
        case SceneObjDataRole::SCENE_DATA_ROLE_OBJ_WIZARD: {
            return object->getWizardName();
        }
        case SceneObjDataRole::SCENE_DATA_ROLE_OBJ_MEMBER_VALUE: {
            return {};
        }
        case SceneObjDataRole::SCENE_DATA_ROLE_OBJ_MEMBER_OFFSET: {
            return {};
        }
        case SceneObjDataRole::SCENE_DATA_ROLE_OBJ_MEMBER_SIZE: {
            return {};
        }
        case SceneObjDataRole::SCENE_DATA_ROLE_OBJ_TRANSFORM: {
            return object->getTransform();
        }
        case SceneObjDataRole::SCENE_DATA_ROLE_OBJ_BOUNDING_BOX: {
            return object->getBoundingBox();
        }
        case SceneObjDataRole::SCENE_DATA_ROLE_OBJ_CAN_PERFORM: {
            return object->getCanPerform();
        }
        case SceneObjDataRole::SCENE_DATA_ROLE_OBJ_IS_PERFORMING: {
            return object->getIsPerforming();
        }
        default:
            return {};
        }
    }

    bool SceneObjModel::setData_(const ModelIndex &index, std::any data, int role) const {
        if (!validateIndex(index)) {
            return false;
        }

        RefPtr<ISceneObject> object = index.data<_SceneIndexData>()->m_object;
        if (!object) {
            return false;
        }

        switch (role) {
        case ModelDataRole::DATA_ROLE_DISPLAY:
            return false;
        case ModelDataRole::DATA_ROLE_DECORATION: {
            index.data<_SceneIndexData>()->m_icon = std::any_cast<RefPtr<const ImageHandle>>(data);
            return true;
        }
        case SceneObjDataRole::SCENE_DATA_ROLE_OBJ_TYPE: {
            return false;
        }
        case SceneObjDataRole::SCENE_DATA_ROLE_OBJ_KEY: {
            object->setNameRef(NameRef(std::any_cast<std::string>(data)));
            index.data<_SceneIndexData>()->m_display_text_cache =
                std::format("{} ({})", object->type(), object->getNameRef().name());
            break;
        }
        case SceneObjDataRole::SCENE_DATA_ROLE_OBJ_GAME_ADDR: {
            return false;
        }
        case SceneObjDataRole::SCENE_DATA_ROLE_OBJ_REF: {
            return false;
        }
        case SceneObjDataRole::SCENE_DATA_ROLE_OBJ_WIZARD: {
            return false;
        }
        case SceneObjDataRole::SCENE_DATA_ROLE_OBJ_MEMBER_VALUE: {
            return false;
        }
        case SceneObjDataRole::SCENE_DATA_ROLE_OBJ_MEMBER_OFFSET: {
            return false;
        }
        case SceneObjDataRole::SCENE_DATA_ROLE_OBJ_MEMBER_SIZE: {
            return false;
        }
        case SceneObjDataRole::SCENE_DATA_ROLE_OBJ_TRANSFORM: {
            auto result = object->setTransform(std::any_cast<Transform>(data), true);
            return result.has_value();
        }
        case SceneObjDataRole::SCENE_DATA_ROLE_OBJ_BOUNDING_BOX: {
            return false;
        }
        case SceneObjDataRole::SCENE_DATA_ROLE_OBJ_CAN_PERFORM: {
            return false;
        }
        case SceneObjDataRole::SCENE_DATA_ROLE_OBJ_IS_PERFORMING: {
            return false;
        }
        default:
            return false;
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
        const UUID64 obj_uuid = object->getUUID();
        if (!m_obj_to_index_map.contains(obj_uuid)) {
            return ModelIndex();
        }
        return m_obj_to_index_map.at(obj_uuid);
    }

    ModelIndex SceneObjModel::getIndex_(const UUID64 &uuid) const {
        if (!m_index_map.contains(uuid)) {
            return ModelIndex();
        }
        return m_index_map.at(uuid);
    }

    ModelIndex SceneObjModel::getIndex_(int64_t row, int64_t column,
                                        const ModelIndex &parent) const {
        if (m_index_map.empty()) {
            return ModelIndex();
        }

        if (!validateIndex(parent)) {
            if (row != 0 || column != 0) {
                return ModelIndex();
            }
            return m_index_map.at(m_root_index);
        }

        RefPtr<ISceneObject> parent_obj = parent.data<_SceneIndexData>()->m_object;
        const std::vector<RefPtr<ISceneObject>> &children = parent_obj->getChildren();

        if (row < 0 || row >= static_cast<int64_t>(children.size()) || column != 0) {
            return ModelIndex();
        }

        RefPtr<ISceneObject> child_obj = children[row];
        return getIndex_(child_obj);
    }

    ModelIndex SceneObjModel::getIndex_(const QualifiedName &qual_name,
                                        const ModelIndex &parent) const {
        if (m_index_map.empty()) {
            return ModelIndex();
        }

        if (!validateIndex(parent)) {
            ModelIndex root_index = m_index_map.at(m_root_index);

            RefPtr<ISceneObject> root_obj = root_index.data<_SceneIndexData>()->m_object;

            std::string root_name = std::string(root_obj->getNameRef().name());
            if (root_name == qual_name[0]) {
                if (qual_name.depth() == 1) {
                    return root_index;
                }

                RefPtr<ISceneObject> child_obj =
                    root_obj->getChild(QualifiedName(qual_name.begin() + 1, qual_name.end()));
                if (!child_obj) {
                    return ModelIndex();
                }

                return getIndex_(child_obj);
            }
            return ModelIndex();
        }

        RefPtr<ISceneObject> parent_obj = parent.data<_SceneIndexData>()->m_object;
        RefPtr<ISceneObject> child_obj  = parent_obj->getChild(qual_name);
        if (!child_obj) {
            return ModelIndex();
        }

        return getIndex_(child_obj);
    }

    ModelIndex SceneObjModel::getIndex_(const std::string &obj_type,
                                        std::optional<std::string> obj_name,
                                        const ModelIndex &parent) const {
        if (!validateIndex(parent)) {
            ModelIndex root_index = m_index_map.at(m_root_index);

            RefPtr<ISceneObject> root_obj = root_index.data<_SceneIndexData>()->m_object;
            std::string root_type         = std::string(root_obj->type());
            if (root_type != obj_type) {
                RefPtr<ISceneObject> child_obj = root_obj->getChildByType(obj_type, obj_name);
                if (!child_obj) {
                    return ModelIndex();
                }

                return getIndex_(child_obj);
            }

            if (obj_name.has_value()) {
                std::string root_name = std::string(root_obj->getNameRef().name());
                if (root_name != obj_name.value()) {
                    RefPtr<ISceneObject> child_obj = root_obj->getChildByType(obj_type, obj_name);
                    if (!child_obj) {
                        return ModelIndex();
                    }

                    return getIndex_(child_obj);
                }
            }

            return root_index;
        }

        RefPtr<ISceneObject> parent_obj = parent.data<_SceneIndexData>()->m_object;
        RefPtr<ISceneObject> child_obj  = parent_obj->getChildByType(obj_type, obj_name);
        if (!child_obj) {
            return ModelIndex();
        }

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
        m_obj_to_index_map.erase(this_data->m_object->getUUID());
        m_index_map.erase(uuid);

        delete this_data;
        return true;
    }

    ModelIndex SceneObjModel::getParent_(const ModelIndex &index) const {
        if (!validateIndex(index)) {
            return ModelIndex();
        }

        _SceneIndexData *data = index.data<_SceneIndexData>();
        if (ISceneObject *parent = data->m_object->getParent()) {
            return getIndex_(get_shared_ptr(*parent));
        }

        return ModelIndex();
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
            return m_index_map.empty() ? 0 : 1;
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
        ISceneObject *parent      = self->getParent();
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
        ScopePtr<MimeData> new_data = make_scoped<MimeData>();

        IDataModel::index_container pruned_indexes = indexes;
        pruneRedundantIndexes(pruned_indexes);

        std::function<Result<void, SerialError>(const ModelIndex &, Serializer &)> serialize_index =
            [&](const ModelIndex &index, Serializer &out) -> Result<void, SerialError> {
            out.write(static_cast<u64>(index.getUUID()));

            RefPtr<ISceneObject> object = index.data<_SceneIndexData>()->m_object;

            out.write(static_cast<u64>(object->getUUID()));
            out.writeString(object->type());
            out.writeString(object->getNameRef().name());
            out.writeString(object->getWizardName());

            const std::vector<MetaStruct::MemberT> &members = object->getMembers();
            out.write<u32>(static_cast<u32>(members.size()));

            for (auto &member : members) {
                member->syncArray();
                Result<void, SerialError> result = member->serialize(out);
                if (!result) {
                    return result;
                }
            }

            size_t child_count = getRowCount_(index);
            out.write<u32>(static_cast<u32>(child_count));

            for (size_t i = 0; i < child_count; ++i) {
                ModelIndex child_index = getIndex_(i, 0, index);
                if (!validateIndex(child_index)) {
                    return make_serial_error<void>(out, "Failed to get child index for object!");
                }
                Result<void, SerialError> result = serialize_index(child_index, out);
                if (!result) {
                    return result;
                }
            }

            return {};
        };

        std::stringstream outstr;
        Serializer out(outstr.rdbuf());

        out.write<u32>(pruned_indexes.size());

        for (const ModelIndex &index : pruned_indexes) {
            if (!validateIndex(index)) {
                TOOLBOX_WARN_V("Tried to create mime data for invalid index {}!", index.getUUID());
                continue;
            }

            serialize_index(index, out);
        }

        std::string_view data_view = outstr.view();

        Buffer mime_data;
        mime_data.alloc(data_view.size());
        std::memcpy(mime_data.buf(), data_view.data(), data_view.size());

        new_data->set_data("toolbox/scene/object_model", std::move(mime_data));

        return new_data;
    }

    Result<IDataModel::index_container> SceneObjModel::insertMimeData_(const ModelIndex &index,
                                                                       const MimeData &data,
                                                                       ModelInsertPolicy policy) {
        if (!data.has_format("toolbox/scene/object_model")) {
            return make_error<std::vector<ModelIndex>>(
                "SceneObjModel", "Provided MIME data has no SceneObjModel data!");
        }

        Buffer mime_data = data.get_data("toolbox/scene/object_model").value();

        std::stringstream instr(std::string(mime_data.buf<char>(), mime_data.size()));
        Deserializer in(instr.rdbuf());

        std::vector<ModelIndex> new_indexes;

        std::function<Result<void, SerialError>(int64_t, const ModelIndex &, Deserializer &)>
            deserialize_index = [&](int64_t row, const ModelIndex &index,
                                    Deserializer &in) -> Result<void, SerialError> {
            const UUID64 uuid = in.read<u64>();

            const UUID64 obj_uuid        = in.read<u64>();
            const std::string obj_type   = in.readString();
            const std::string obj_name   = in.readString();
            const std::string obj_wizard = in.readString();

            TemplateFactory::create_t obj_template =
                TemplateFactory::create(std::string_view(obj_type), true);
            if (!obj_template) {
                return make_serial_error<void>(
                    in, std::format("Failed to find template for object type '{}'", obj_type));
            }

            RefPtr<ISceneObject> object =
                ObjectFactory::create(*obj_template.value(), obj_wizard, m_scene_path, obj_uuid);
            if (!object) {
                return make_serial_error<void>(
                    in, std::format("Failed to create object of type '{}' with wizard '{}'",
                                    obj_type, obj_wizard));
            }
            object->setNameRef(NameRef(obj_name));

            const u32 member_count                  = in.read<u32>();
            std::vector<RefPtr<MetaMember>> members = object->getMembers();
            for (RefPtr<MetaMember> member : members) {
                member->updateReferenceToList(members);
                Result<void, SerialError> result = member->deserialize(in);
                if (!result) {
                    return result;
                }
            }

            object->sync();

            std::string new_name = findUniqueName_(index, std::string(object->getNameRef().name()));
            object->setNameRef(NameRef(new_name));

            ModelIndex new_index = insertObject_(object, row, index, uuid);
            if (!validateIndex(new_index)) {
                return make_serial_error<void>(in, "Failed to insert mimedata object into model!");
            }
            new_indexes.push_back(new_index);

            u32 child_count = in.read<u32>();
            for (u32 i = 0; i < child_count; ++i) {
                Result<void, SerialError> result = deserialize_index(i, new_index, in);
                if (!result) {
                    return result;
                }
            }

            return {};
        };

        if (policy == ModelInsertPolicy::INSERT_CHILD) {
            RefPtr<ISceneObject> object = std::any_cast<RefPtr<ISceneObject>>(
                getData_(index, SceneObjDataRole::SCENE_DATA_ROLE_OBJ_REF));
            if (!object->isGroupObject()) {
                // Inserting as a child of a node is not allowed, so we treat this as an "after"
                // insertion
                policy = ModelInsertPolicy::INSERT_AFTER;
            }
        }

        ModelIndex insert_index;
        int64_t insert_row = -1;
        switch (policy) {
        case ModelInsertPolicy::INSERT_BEFORE:
            insert_row = getRow_(index);
            if (insert_row == -1) {
                return make_error<std::vector<ModelIndex>>(
                    "SceneObjModel", "Failed to retrieve the row for the insert index");
            }
            insert_index = getParent_(index);
            break;
        case ModelInsertPolicy::INSERT_AFTER:
            insert_row = getRow_(index);
            if (insert_row == -1) {
                return make_error<std::vector<ModelIndex>>(
                    "SceneObjModel", "Failed to retrieve the row for the insert index");
            }
            insert_row += 1;
            insert_index = getParent_(index);
            break;
        case ModelInsertPolicy::INSERT_CHILD:
            insert_row   = getRowCount_(index);
            insert_index = index;
            break;
        }

        const u32 obj_count = in.read<u32>();

        for (u32 i = 0; i < obj_count; ++i) {
            auto result = deserialize_index(insert_row, insert_index, in);
            if (!result) {
                return make_error<std::vector<ModelIndex>>("SceneObjModel",
                                                           result.error().m_message);
            }
        }

        return new_indexes;
    }

    bool SceneObjModel::canFetchMore_(const ModelIndex &index) const { return false; }

    void SceneObjModel::fetchMore_(const ModelIndex &index) const { return; }

    ModelIndex SceneObjModel::insertObject_(RefPtr<ISceneObject> object, int64_t row,
                                            const ModelIndex &parent, std::optional<UUID64> index_uuid) {
        ModelIndex new_index = makeIndex(object, row, parent, index_uuid);
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
                (void)removeIndex_(new_index);
                return ModelIndex();
            }
        }

        return new_index;
    }

    ModelIndex SceneObjModel::makeIndex(RefPtr<ISceneObject> object, int64_t row,
                                        const ModelIndex &parent,
                                        std::optional<UUID64> index_uuid) const {
        if (row < 0 || row > m_index_map.size()) {
            return ModelIndex();
        }

        ModelIndex new_index(getUUID(), index_uuid ? *index_uuid : UUID64());

        _SceneIndexData *new_data = new _SceneIndexData;
        new_data->m_self_uuid     = new_index.getUUID();
        new_data->m_object        = object;
        new_data->m_icon          = nullptr;
        new_data->m_display_text_cache =
            std::format("{} ({})", object->type(), object->getNameRef().name());

        new_index.setData(new_data);

        m_index_map[new_index.getUUID()]      = new_index;
        m_obj_to_index_map[object->getUUID()] = new_index;

        if (row == 0 && !validateIndex(parent)) {
            m_root_index = new_index.getUUID();
        }

        return new_index;
    }

    Result<MetaValue, MetaError> SceneObjModel::getMemberValue_(const ModelIndex &index,
                                                                     const QualifiedName &member,
                                                                     size_t array_idx) const {
        if (!validateIndex(index)) {
            return make_meta_error<MetaValue>("SCENE_OBJ_MODEL", "Missing index", "Object Index");
        }
        RefPtr<ISceneObject> object = index.data<_SceneIndexData>()->m_object;
        auto member_res             = object->getMember(member);
        if (!member_res) {
            return std::unexpected(member_res.error()); 
        }

        RefPtr<MetaMember> member_ptr = member_res.value();
        if (member_ptr->isTypeEnum()) {
            auto enum_res = member_ptr->value<MetaEnum>(array_idx);
            if (!enum_res) {
                return std::unexpected(enum_res.error());
            }
            RefPtr<MetaEnum> enum_ptr = enum_res.value();
            return *enum_ptr->value();
        }

        auto value_res = member_ptr->value<MetaValue>(array_idx);
        if (!value_res) {
            return std::unexpected(value_res.error());
        }
        return *value_res.value();
    }

    Result<void, MetaError> SceneObjModel::setMemberValue_(const ModelIndex &index,
                                                                const QualifiedName &member,
                                                                size_t array_idx,
                                                                const MetaValue &value) {
        RefPtr<ISceneObject> object = index.data<_SceneIndexData>()->m_object;
        auto member_res             = object->getMember(member);
        if (!member_res) {
            return std::unexpected(member_res.error());
        }

        RefPtr<MetaMember> member_ptr = member_res.value();

        switch (value.type()) {
        case MetaType::BOOL: {
            auto result = setMetaValue<bool>(member_ptr, array_idx, value.get<bool>().value(), MetaType::BOOL);
            if (!result) {
                return std::unexpected(result.error());
            }
            break;
        }
        case MetaType::S8: {
            auto result =
                setMetaValue<s64>(member_ptr, array_idx, value.get<s8>().value(), MetaType::S8);
            if (!result) {
                return std::unexpected(result.error());
            }
            break;
        }
        case MetaType::U8: {
            auto result =
                setMetaValue<s64>(member_ptr, array_idx, value.get<u8>().value(), MetaType::U8);
            if (!result) {
                return std::unexpected(result.error());
            }
            break;
        }
        case MetaType::S16: {
            auto result =
                setMetaValue<s64>(member_ptr, array_idx, value.get<s16>().value(), MetaType::S16);
            if (!result) {
                return std::unexpected(result.error());
            }
            break;
        }
        case MetaType::U16: {
            auto result =
                setMetaValue<s64>(member_ptr, array_idx, value.get<u16>().value(), MetaType::U16);
            if (!result) {
                return std::unexpected(result.error());
            }
            break;
        }
        case MetaType::S32: {
            auto result =
                setMetaValue<s64>(member_ptr, array_idx, value.get<s32>().value(), MetaType::S32);
            if (!result) {
                return std::unexpected(result.error());
            }
            break;
        }
        case MetaType::U32: {
            auto result =
                setMetaValue<s64>(member_ptr, array_idx, value.get<u32>().value(), MetaType::U32);
            if (!result) {
                return std::unexpected(result.error());
            }
            break;
        }
        case MetaType::F32: {
            auto result =
                setMetaValue<f32>(member_ptr, array_idx, value.get<f32>().value(), MetaType::F32);
            if (!result) {
                return std::unexpected(result.error());
            }
            break;
        }
        case MetaType::F64: {
            auto result =
                setMetaValue<f64>(member_ptr, array_idx, value.get<f64>().value(), MetaType::F64);
            if (!result) {
                return std::unexpected(result.error());
            }
            break;
        }
        case MetaType::STRING: {
            auto result = setMetaValue<std::string>(member_ptr, array_idx, value.get<std::string>().value(),
                                           MetaType::STRING);
            if (!result) {
                return std::unexpected(result.error());
            }
            break;
        }
        case MetaType::VEC3: {
            auto result = setMetaValue<glm::vec3>(member_ptr, array_idx, value.get<glm::vec3>().value(),
                                           MetaType::VEC3);
            if (!result) {
                return std::unexpected(result.error());
            }
            break;
        }
        case MetaType::TRANSFORM: {
            auto result = setMetaValue<Transform>(member_ptr, array_idx, value.get<Transform>().value(),
                                           MetaType::TRANSFORM);
            if (!result) {
                return std::unexpected(result.error());
            }
            break;
        }
        case MetaType::MTX34: {
            auto result = setMetaValue<glm::mat3x4>(member_ptr, array_idx, value.get<glm::mat3x4>().value(),
                                           MetaType::MTX34);
            if (!result) {
                return std::unexpected(result.error());
            }
            break;
        }
        case MetaType::RGB: {
            auto result = setMetaValue<Color::RGB8>(member_ptr, array_idx, value.get<Color::RGB8>().value(),
                                           MetaType::RGB);
            if (!result) {
                return std::unexpected(result.error());
            }
            break;
        }
        case MetaType::RGBA: {
            auto result = setMetaValue<Color::RGBA8>(member_ptr, array_idx, value.get<Color::RGBA8>().value(),
                                           MetaType::RGBA);
            if (!result) {
                return std::unexpected(result.error());
            }
            break;
        }
        case MetaType::RGB32: {
            auto result = setMetaValue<Color::RGB32>(member_ptr, array_idx, value.get<Color::RGB32>().value(),
                                           MetaType::RGB32);
            if (!result) {
                return std::unexpected(result.error());
            }
            break;
        }
        case MetaType::RGBA32: {
            auto result = setMetaValue<Color::RGBA32>(member_ptr, array_idx,
                                           value.get<Color::RGBA32>().value(), MetaType::RGBA32);
            if (!result) {
                return std::unexpected(result.error());
            }
            break;
        }
        default: {
            return {};
        }
        }

        member_ptr->syncArray();
        return {};
    }

    Result<u32, MetaScopeError> SceneObjModel::getMemberOffset_(const ModelIndex &index,
                                                                const QualifiedName &member) const {
        RefPtr<ISceneObject> object = index.data<_SceneIndexData>()->m_object;
        return object->getMemberOffset(member, 0);
    }

    Result<u32, MetaScopeError> SceneObjModel::getMemberSize_(const ModelIndex &index,
                                                              const QualifiedName &member) const {
        RefPtr<ISceneObject> object = index.data<_SceneIndexData>()->m_object;
        return object->getMemberSize(member, 0);
    }

    void SceneObjModel::signalEventListeners(const ModelIndex &index, int flags) {
        int this_event_flags =
            flags & ~(EVENT_SUCCESS | EVENT_PRE | EVENT_POST | EVENT_SOFT | EVENT_RESET);

        if (this_event_flags == EVENT_INDEX_MODIFIED) {
            flags |= ModelEventFlags::EVENT_SOFT;
        }

        for (const auto &[key, listener] : m_listeners) {
            if ((listener.second & flags) != ModelEventFlags::EVENT_NONE) {
                listener.first(index, (listener.second & flags));
            }
        }
    }

    void SceneObjModel::pruneRedundantIndexes(IDataModel::index_container &indexes) const {  // ---
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
                                             ModelIndex parent_idx = getParent_(child_idx);

                                             while (validateIndex(parent_idx)) {
                                                 // Check if this parent is *also* in our original
                                                 // list.
                                                 if (list_uuids.count(parent_idx.getUUID())) {
                                                     return true;
                                                 }

                                                 parent_idx = getParent_(parent_idx);
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
