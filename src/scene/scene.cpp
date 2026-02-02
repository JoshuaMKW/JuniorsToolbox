#include <fstream>
#include <stack>

#include "objlib/utf8_to_sjis.hpp"
#include "scene/scene.hpp"

namespace Toolbox::Scene {

    class SceneValidator {
    public:
        using size_fn    = std::function<bool(size_t)>;
        using foreach_fn = std::function<void(RefPtr<ISceneObject>)>;

    public:
        SceneValidator(RefPtr<const SceneInstance> scene, bool check_dependencies = true)
            : m_scene_instance(scene), m_check_dependencies(check_dependencies), m_valid(true) {
            RefPtr<ObjectHierarchy> obj_hierarchy = scene->getObjHierarchy();
            m_total_objects = obj_hierarchy ? static_cast<double>(obj_hierarchy->getSize()) : 0.0;
        }

    public:
        SceneValidator()                           = delete;
        SceneValidator(const SceneValidator &)     = delete;
        SceneValidator(SceneValidator &&) noexcept = delete;

    public:
        void setProgressCallback(SceneInstance::validate_progress_cb cb) {
            m_progress_callback = cb;
        }
        void setErrorCallback(SceneInstance::validate_error_cb cb) { m_error_callback = cb; }

        // Test that an object exists
        SceneValidator &scopeAssertObject(const std::string &obj_type, const std::string &obj_name);

        // Test that the group scope is a given size
        SceneValidator &scopeAssertSize(size_fn predicate);
        SceneValidator &scopeEnter();
        SceneValidator &scopeEscape();

        SceneValidator &scopeValidateDependencies(RefPtr<ISceneObject> object);
        SceneValidator &scopeForAll(foreach_fn);

        explicit operator bool() const { return m_valid; }

    private:
        SceneInstance::validate_progress_cb m_progress_callback;
        SceneInstance::validate_error_cb m_error_callback;

        std::stack<RefPtr<GroupSceneObject>> m_parent_stack;
        RefPtr<ISceneObject> m_last_obj;

        RefPtr<const SceneInstance> m_scene_instance;
        bool m_check_dependencies;

        bool m_valid;

        double m_processed_objects = 0;
        double m_total_objects     = 0;
    };

#define VALIDATOR_LT(x) [](size_t v) -> bool { return v < (x); }
#define VALIDATOR_LE(x) [](size_t v) -> bool { return v <= (x); }
#define VALIDATOR_GT(x) [](size_t v) -> bool { return v > (x); }
#define VALIDATOR_GE(x) [](size_t v) -> bool { return v >= (x); }
#define VALIDATOR_NE(x) [](size_t v) -> bool { return v != (x); }
#define VALIDATOR_EQ(x) [](size_t v) -> bool { return v == (x); }

    class SceneMender {
    public:
        using size_fn    = std::function<bool(size_t)>;
        using foreach_fn = std::function<void(RefPtr<ISceneObject>)>;

    public:
        SceneMender(RefPtr<const SceneInstance> scene, bool check_dependencies = true)
            : m_scene_instance(scene), m_check_dependencies(check_dependencies), m_valid(true) {}

    public:
        SceneMender()                        = delete;
        SceneMender(const SceneMender &)     = delete;
        SceneMender(SceneMender &&) noexcept = delete;

    public:
        void setProgressCallback(SceneInstance::repair_progress_cb cb) { m_progress_callback = cb; }
        void setChangeCallback(SceneInstance::repair_change_cb cb) { m_change_callback = cb; }
        void setErrorCallback(SceneInstance::repair_error_cb cb) { m_error_callback = cb; }

        // Test that an object exists
        SceneMender &scopeTryCreateObjectDefault(const std::string &obj_type,
                                                 const std::string &obj_name);

        SceneMender &scopeEnter();
        SceneMender &scopeEscape();

        SceneMender &scopeFulfillDependencies(RefPtr<ISceneObject> object);
        SceneMender &scopeForAll(foreach_fn);

        explicit operator bool() const { return m_valid; }

    private:
        SceneInstance::repair_progress_cb m_progress_callback;
        SceneInstance::repair_change_cb m_change_callback;
        SceneInstance::repair_error_cb m_error_callback;

        std::stack<RefPtr<GroupSceneObject>> m_parent_stack;
        RefPtr<ISceneObject> m_last_obj;

        RefPtr<const SceneInstance> m_scene_instance;
        bool m_check_dependencies;

        bool m_valid;

        int m_processed_objects = 0;
    };

    void ObjectHierarchy::dump(std::ostream &os, size_t indent, size_t indent_size) const {
        std::string indent_str(indent * indent_size, ' ');
        os << indent_str << "ObjectHierarchy (" << m_name << ") {\n";
        m_root->dump(os, indent + 1, indent_size);
        os << indent_str << "}\n";
    }

    Result<ScopePtr<SceneInstance>, SerialError>
    SceneInstance::FromPath(const fs_path &root, bool include_custom_objs) {
        ScopePtr<SceneInstance> scene = make_scoped<SceneInstance>();
        scene->m_root_path            = root;

        scene->m_map_objects   = make_referable<ObjectHierarchy>("Map");
        scene->m_table_objects = make_referable<ObjectHierarchy>("Table");
        scene->m_rail_info     = make_referable<RailData>();
        scene->m_message_data  = make_referable<BMG::MessageData>();

        scene->m_map_objects->setIncludeCustomObjects(include_custom_objs);
        scene->m_table_objects->setIncludeCustomObjects(include_custom_objs);

        fs_path scene_bin   = root / "map/scene.bin";
        fs_path tables_bin  = root / "map/tables.bin";
        fs_path rail_bin    = root / "map/scene.ral";
        fs_path message_bin = root / "map/message.bmg";

        // Load the scene data
        {
            std::ifstream file(scene_bin, std::ios::in | std::ios::binary);
            Deserializer in(file.rdbuf(), scene_bin.string());

            Result<void, SerialError> result = scene->m_map_objects->deserialize(in);
            if (!result) {
                return std::unexpected(result.error());
            }
        }

        // Load the table data
        {
            std::ifstream file(tables_bin, std::ios::in | std::ios::binary);
            Deserializer in(file.rdbuf());

            Result<void, SerialError> result = scene->m_table_objects->deserialize(in);
            if (!result) {
                return std::unexpected(result.error());
            }
        }

        // Load the rail data
        {
            std::ifstream file(rail_bin, std::ios::in | std::ios::binary);
            Deserializer in(file.rdbuf());

            Result<void, SerialError> result = scene->m_rail_info->deserialize(in);
            if (!result) {
                return std::unexpected(result.error());
            }
        }

        // Load the message data
        {
            std::ifstream file(message_bin, std::ios::in | std::ios::binary);
            if (file.is_open()) {  // scene.bmg is optional
                Deserializer in(file.rdbuf());

                Result<void, SerialError> result = scene->m_message_data->deserialize(in);
                if (!result) {
                    return std::unexpected(result.error());
                }
            }
        }

        return scene;
    }

    SceneInstance::SceneInstance() {}

    SceneInstance::SceneInstance(const SceneInstance &) {}

    SceneInstance::~SceneInstance() {}

    ScopePtr<SceneInstance> SceneInstance::BasicScene() { return nullptr; }

    bool SceneInstance::validate(bool check_dependencies, validate_progress_cb progress_cb,
                                 validate_error_cb error_cb) const {
        // The goal here is to test that the scene is loadable and all objects are valid.
        SceneValidator validate(ref_cast<const SceneInstance>(this->ptr()));
        validate.setProgressCallback(progress_cb);
        validate.setErrorCallback(error_cb);

        // clang-format off

        validate.scopeAssertObject("GroupObj", to_str(u8"全体シーン"))
                .scopeEnter()
                    .scopeAssertSize(VALIDATOR_GE(6))
                    .scopeAssertObject("GroupObj", to_str(u8"コンダクター初期化用"))
                    .scopeEnter()
                        .scopeAssertSize(VALIDATOR_GE(7))
                        .scopeAssertObject("MapObjBaseManager", to_str(u8"シャインマネージャー"))
                        .scopeAssertObject("MapObjManager", to_str(u8"地形オブジェマネージャー"))
                        .scopeAssertObject("MapObjBaseManager", to_str(u8"木マネージャー"))
                        .scopeAssertObject("MapObjBaseManager", to_str(u8"大型地形オブジェマネージャー"))
                        .scopeAssertObject("MapObjBaseManager", to_str(u8"ファークリップ地形オブジェマネージャー"))
                        .scopeAssertObject("MapObjBaseManager", to_str(u8"乗り物マネージャー"))
                        .scopeAssertObject("ItemManager", to_str(u8"アイテムマネージャー"))
                    .scopeEscape()
                    .scopeAssertObject("GroupObj", to_str(u8"鏡シーン"))
                    .scopeEnter()
                        .scopeAssertSize(VALIDATOR_GE(1))
                        .scopeAssertObject("MirrorCamera", to_str(u8"鏡カメラ"))
                    .scopeEscape()
                    .scopeAssertObject("MirrorModelManager", to_str(u8"鏡表示モデル管理"))
                    .scopeAssertObject("GroupObj", to_str(u8"スペキュラシーン"))
                    .scopeAssertObject("GroupObj", to_str(u8"インダイレクトシーン"))
                    .scopeAssertObject("MarScene", to_str(u8"通常シーン"))
                    .scopeEnter()
                        .scopeAssertSize(VALIDATOR_GE(4))
                        .scopeAssertObject("AmbAry", to_str(u8"Ambient Group"))
                        .scopeEnter()
                            .scopeAssertSize(VALIDATOR_GE(6))
                            .scopeAssertObject("AmbColor", to_str(u8"太陽アンビエント（プレイヤー）"))
                            .scopeAssertObject("AmbColor", to_str(u8"影アンビエント（プレイヤー）"))
                            .scopeAssertObject("AmbColor", to_str(u8"太陽アンビエント（オブジェクト）"))
                            .scopeAssertObject("AmbColor", to_str(u8"影アンビエント（オブジェクト）"))
                            .scopeAssertObject("AmbColor", to_str(u8"太陽アンビエント（敵）"))
                            .scopeAssertObject("AmbColor", to_str(u8"影アンビエント（敵）"))
                        .scopeEscape()
                        .scopeAssertObject("LightAry", to_str(u8"Light Group"))
                        .scopeEnter()
                            .scopeAssertSize(VALIDATOR_GE(15))
                            .scopeAssertObject("Light", to_str(u8"太陽（プレイヤー）"))
                            .scopeAssertObject("Light", to_str(u8"太陽サブ（プレイヤー）"))
                            .scopeAssertObject("Light", to_str(u8"影（プレイヤー）"))
                            .scopeAssertObject("Light", to_str(u8"影サブ（プレイヤー）"))
                            .scopeAssertObject("Light", to_str(u8"太陽スペキュラ（プレイヤー）"))
                            .scopeAssertObject("Light", to_str(u8"太陽（オブジェクト）"))
                            .scopeAssertObject("Light", to_str(u8"太陽サブ（オブジェクト）"))
                            .scopeAssertObject("Light", to_str(u8"影（オブジェクト）"))
                            .scopeAssertObject("Light", to_str(u8"影サブ（オブジェクト）"))
                            .scopeAssertObject("Light", to_str(u8"太陽スペキュラ（オブジェクト）"))
                            .scopeAssertObject("Light", to_str(u8"太陽（敵）"))
                            .scopeAssertObject("Light", to_str(u8"太陽サブ（敵）"))
                            .scopeAssertObject("Light", to_str(u8"影（敵）"))
                            .scopeAssertObject("Light", to_str(u8"影サブ（敵）"))
                            .scopeAssertObject("Light", to_str(u8"太陽スペキュラ（敵）"))
                        .scopeEscape()
                        .scopeAssertObject("Strategy", to_str(u8"ストラテジ"))
                        .scopeEnter()
                            .scopeAssertSize(VALIDATOR_GE(12))
                            .scopeAssertObject("IdxGroup", to_str(u8"マップグループ"))
                            .scopeEnter()
                                .scopeAssertSize(VALIDATOR_GE(1))
                                .scopeAssertObject("Map", to_str(u8"マップ"))
                            .scopeEscape()
                            .scopeAssertObject("IdxGroup", to_str(u8"空グループ"))
                            .scopeEnter()
                                .scopeAssertSize(VALIDATOR_GE(1))
                                .scopeAssertObject("Sky", to_str(u8"空"))
                            .scopeEscape()
                            .scopeAssertObject("IdxGroup", to_str(u8"マネージャーグループ"))
                            .scopeEnter()
                                .scopeAssertSize(VALIDATOR_GE(12))
                                .scopeAssertObject("SunMgr", to_str(u8"太陽マネージャー"))
                                .scopeAssertObject("CubeCamera", to_str(u8"キューブ（カメラ）"))
                                .scopeAssertObject("CubeMirror", to_str(u8"キューブ（鏡）"))
                                .scopeAssertObject("CubeWire", to_str(u8"キューブ（ワイヤー）"))
                                .scopeAssertObject("CubeStream", to_str(u8"キューブ（流れ）"))
                                .scopeAssertObject("CubeShadow", to_str(u8"キューブ（影）"))
                                .scopeAssertObject("CubeArea", to_str(u8"キューブ（エリア）"))
                                .scopeAssertObject("CubeFastA", to_str(u8"キューブ（高速Ａ）"))
                                .scopeAssertObject("CubeFastB", to_str(u8"キューブ（高速Ｂ）"))
                                .scopeAssertObject("CubeFastC", to_str(u8"キューブ（高速Ｃ）"))
                                .scopeAssertObject("CubeSoundChange", to_str(u8"キューブ（サウンド切り替え）"))
                                .scopeAssertObject("CubeSoundEffect", to_str(u8"キューブ（サウンドエフェクト）"))
                            .scopeEscape()
                            .scopeAssertObject("IdxGroup", to_str(u8"オブジェクトグループ"))
                            .scopeAssertObject("IdxGroup", to_str(u8"落書きグループ"))
                            .scopeEnter()
                                .scopeAssertSize(VALIDATOR_GE(1))
                                .scopeAssertObject("Pollution", to_str(u8"落書き管理"))
                            .scopeEscape()
                            .scopeAssertObject("IdxGroup", to_str(u8"アイテムグループ"))
                            .scopeAssertObject("IdxGroup", to_str(u8"プレーヤーグループ"))
                            .scopeEnter()
                                .scopeAssertSize(VALIDATOR_GE(1))
                                .scopeAssertObject("Mario", to_str(u8"マリオ"))
                            .scopeEscape()
                            .scopeAssertObject("IdxGroup", to_str(u8"敵グループ"))
                            .scopeAssertObject("IdxGroup", to_str(u8"ボスグループ"))
                            .scopeAssertObject("IdxGroup", to_str(u8"ＮＰＣグループ"))
                            .scopeAssertObject("IdxGroup", to_str(u8"水パーティクルグループ"))
                            .scopeAssertObject("IdxGroup", to_str(u8"初期化用グループ"))
                        .scopeEscape()
                        .scopeAssertObject("GroupObj", to_str(u8"Cameras"))
                        .scopeEnter()
                            .scopeAssertSize(VALIDATOR_GE(1))
                            .scopeAssertObject("PolarSubCamera", to_str(u8"camera 1"))
                        .scopeEscape()
                    .scopeEscape()
                .scopeEscape();

        // clang-format on

        // Now we check for dependencies on ALL objects
        validate.scopeForAll(
            [&validate](RefPtr<ISceneObject> obj) { validate.scopeValidateDependencies(obj); });

        return static_cast<bool>(validate);
    }

    bool SceneInstance::repair(bool check_dependencies, repair_progress_cb progress_cb,
                               repair_change_cb change_cb, repair_error_cb error_cb) const {
        // The goal here is to test that the scene is loadable and all objects are valid.
        SceneMender mender(ref_cast<const SceneInstance>(this->ptr()));
        mender.setProgressCallback(progress_cb);
        mender.setChangeCallback(change_cb);
        mender.setErrorCallback(error_cb);

        // clang-format off

        mender.scopeTryCreateObjectDefault("GroupObj", to_str(u8"全体シーン"))
                .scopeEnter()
                    .scopeTryCreateObjectDefault("GroupObj", to_str(u8"コンダクター初期化用"))
                    .scopeEnter()
                        .scopeTryCreateObjectDefault("MapObjBaseManager", to_str(u8"シャインマネージャー"))
                        .scopeTryCreateObjectDefault("MapObjManager", to_str(u8"地形オブジェマネージャー"))
                        .scopeTryCreateObjectDefault("MapObjBaseManager", to_str(u8"木マネージャー"))
                        .scopeTryCreateObjectDefault("MapObjBaseManager", to_str(u8"大型地形オブジェマネージャー"))
                        .scopeTryCreateObjectDefault("MapObjBaseManager", to_str(u8"ファークリップ地形オブジェマネージャー"))
                        .scopeTryCreateObjectDefault("MapObjBaseManager", to_str(u8"乗り物マネージャー"))
                        .scopeTryCreateObjectDefault("ItemManager", to_str(u8"アイテムマネージャー"))
                    .scopeEscape()
                    .scopeTryCreateObjectDefault("GroupObj", to_str(u8"鏡シーン"))
                    .scopeEnter()
                        .scopeTryCreateObjectDefault("MirrorCamera", to_str(u8"鏡カメラ"))
                    .scopeEscape()
                    .scopeTryCreateObjectDefault("MirrorModelManager", to_str(u8"鏡表示モデル管理"))
                    .scopeTryCreateObjectDefault("GroupObj", to_str(u8"スペキュラシーン"))
                    .scopeTryCreateObjectDefault("GroupObj", to_str(u8"インダイレクトシーン"))
                    .scopeTryCreateObjectDefault("MarScene", to_str(u8"通常シーン"))
                    .scopeEnter()
                        .scopeTryCreateObjectDefault("AmbAry", to_str(u8"Ambient Group"))
                        .scopeEnter()
                            .scopeTryCreateObjectDefault("AmbColor", to_str(u8"太陽アンビエント（プレイヤー）"))
                            .scopeTryCreateObjectDefault("AmbColor", to_str(u8"影アンビエント（プレイヤー）"))
                            .scopeTryCreateObjectDefault("AmbColor", to_str(u8"太陽アンビエント（オブジェクト）"))
                            .scopeTryCreateObjectDefault("AmbColor", to_str(u8"影アンビエント（オブジェクト）"))
                            .scopeTryCreateObjectDefault("AmbColor", to_str(u8"太陽アンビエント（敵）"))
                            .scopeTryCreateObjectDefault("AmbColor", to_str(u8"影アンビエント（敵）"))
                        .scopeEscape()
                        .scopeTryCreateObjectDefault("LightAry", to_str(u8"Light Group"))
                        .scopeEnter()
                            .scopeTryCreateObjectDefault("Light", to_str(u8"太陽（プレイヤー）"))
                            .scopeTryCreateObjectDefault("Light", to_str(u8"太陽サブ（プレイヤー）"))
                            .scopeTryCreateObjectDefault("Light", to_str(u8"影（プレイヤー）"))
                            .scopeTryCreateObjectDefault("Light", to_str(u8"影サブ（プレイヤー）"))
                            .scopeTryCreateObjectDefault("Light", to_str(u8"太陽スペキュラ（プレイヤー）"))
                            .scopeTryCreateObjectDefault("Light", to_str(u8"太陽（オブジェクト）"))
                            .scopeTryCreateObjectDefault("Light", to_str(u8"太陽サブ（オブジェクト）"))
                            .scopeTryCreateObjectDefault("Light", to_str(u8"影（オブジェクト）"))
                            .scopeTryCreateObjectDefault("Light", to_str(u8"影サブ（オブジェクト）"))
                            .scopeTryCreateObjectDefault("Light", to_str(u8"太陽スペキュラ（オブジェクト）"))
                            .scopeTryCreateObjectDefault("Light", to_str(u8"太陽（敵）"))
                            .scopeTryCreateObjectDefault("Light", to_str(u8"太陽サブ（敵）"))
                            .scopeTryCreateObjectDefault("Light", to_str(u8"影（敵）"))
                            .scopeTryCreateObjectDefault("Light", to_str(u8"影サブ（敵）"))
                            .scopeTryCreateObjectDefault("Light", to_str(u8"太陽スペキュラ（敵）"))
                        .scopeEscape()
                        .scopeTryCreateObjectDefault("Strategy", to_str(u8"ストラテジ"))
                        .scopeEnter()
                            .scopeTryCreateObjectDefault("IdxGroup", to_str(u8"マップグループ"))
                            .scopeEnter()
                                .scopeTryCreateObjectDefault("Map", to_str(u8"マップ"))
                            .scopeEscape()
                            .scopeTryCreateObjectDefault("IdxGroup", to_str(u8"空グループ"))
                            .scopeEnter()
                                .scopeTryCreateObjectDefault("Sky", to_str(u8"空"))
                            .scopeEscape()
                            .scopeTryCreateObjectDefault("IdxGroup", to_str(u8"マネージャーグループ"))
                            .scopeEnter()
                                .scopeTryCreateObjectDefault("SunMgr", to_str(u8"太陽マネージャー"))
                                .scopeTryCreateObjectDefault("CubeCamera", to_str(u8"キューブ（カメラ）"))
                                .scopeTryCreateObjectDefault("CubeMirror", to_str(u8"キューブ（鏡）"))
                                .scopeTryCreateObjectDefault("CubeWire", to_str(u8"キューブ（ワイヤー）"))
                                .scopeTryCreateObjectDefault("CubeStream", to_str(u8"キューブ（流れ）"))
                                .scopeTryCreateObjectDefault("CubeShadow", to_str(u8"キューブ（影）"))
                                .scopeTryCreateObjectDefault("CubeArea", to_str(u8"キューブ（エリア）"))
                                .scopeTryCreateObjectDefault("CubeFastA", to_str(u8"キューブ（高速Ａ）"))
                                .scopeTryCreateObjectDefault("CubeFastB", to_str(u8"キューブ（高速Ｂ）"))
                                .scopeTryCreateObjectDefault("CubeFastC", to_str(u8"キューブ（高速Ｃ）"))
                                .scopeTryCreateObjectDefault("CubeSoundChange", to_str(u8"キューブ（サウンド切り替え）"))
                                .scopeTryCreateObjectDefault("CubeSoundEffect", to_str(u8"キューブ（サウンドエフェクト）"))
                            .scopeEscape()
                            .scopeTryCreateObjectDefault("IdxGroup", to_str(u8"オブジェクトグループ"))
                            .scopeTryCreateObjectDefault("IdxGroup", to_str(u8"落書きグループ"))
                            .scopeEnter()
                                .scopeTryCreateObjectDefault("Pollution", to_str(u8"落書き管理"))
                            .scopeEscape()
                            .scopeTryCreateObjectDefault("IdxGroup", to_str(u8"アイテムグループ"))
                            .scopeTryCreateObjectDefault("IdxGroup", to_str(u8"プレーヤーグループ"))
                            .scopeEnter()
                                .scopeTryCreateObjectDefault("Mario", to_str(u8"マリオ"))
                            .scopeEscape()
                            .scopeTryCreateObjectDefault("IdxGroup", to_str(u8"敵グループ"))
                            .scopeTryCreateObjectDefault("IdxGroup", to_str(u8"ボスグループ"))
                            .scopeTryCreateObjectDefault("IdxGroup", to_str(u8"ＮＰＣグループ"))
                            .scopeTryCreateObjectDefault("IdxGroup", to_str(u8"水パーティクルグループ"))
                            .scopeTryCreateObjectDefault("IdxGroup", to_str(u8"初期化用グループ"))
                        .scopeEscape()
                        .scopeTryCreateObjectDefault("GroupObj", to_str(u8"Cameras"))
                        .scopeEnter()
                            .scopeTryCreateObjectDefault("PolarSubCamera", to_str(u8"camera 1"))
                        .scopeEscape()
                    .scopeEscape()
                .scopeEscape();

        // clang-format on

        // Now we check for dependencies on ALL objects
        mender.scopeForAll(
            [&mender](RefPtr<ISceneObject> obj) { mender.scopeFulfillDependencies(obj); });

        return static_cast<bool>(mender);
    }

    Result<void, SerialError> SceneInstance::saveToPath(const fs_path &root) {
        auto scene_bin   = root / "map/scene.bin";
        auto tables_bin  = root / "map/tables.bin";
        auto rail_bin    = root / "map/scene.ral";
        auto message_bin = root / "map/message.bmg";

        {
            std::ofstream file(scene_bin, std::ios::out | std::ios::binary);
            Serializer out(file.rdbuf(), scene_bin.string());

            auto result = m_map_objects->serialize(out);
            if (!result) {
                return std::unexpected(result.error());
            }
        }

        {
            std::ofstream file(tables_bin, std::ios::out | std::ios::binary);
            Serializer out(file.rdbuf(), scene_bin.string());

            auto result = m_table_objects->serialize(out);
            if (!result) {
                return std::unexpected(result.error());
            }
        }

        {
            std::ofstream file(rail_bin, std::ios::out | std::ios::binary);
            Serializer out(file.rdbuf(), scene_bin.string());

            auto result = m_rail_info->serialize(out);
            if (!result) {
                return std::unexpected(result.error());
            }
        }

        {
            std::ofstream file(message_bin, std::ios::out | std::ios::binary);
            Serializer out(file.rdbuf(), scene_bin.string());

            auto result = m_message_data->serialize(out);
            if (!result) {
                return std::unexpected(result.error());
            }
        }

        m_root_path = root;
        return {};
    }

    void SceneInstance::dump(std::ostream &os, size_t indent, size_t indent_size) const {
        std::string indent_str(indent * indent_size, ' ');
        if (!m_root_path) {
            os << indent_str << "SceneInstance (null) {" << std::endl;
        } else {
            os << indent_str << "SceneInstance (" << m_root_path->parent_path().filename() << ") {"
               << std::endl;
        }
        m_map_objects->dump(os, indent + 1, indent_size);
        m_table_objects->dump(os, indent + 1, indent_size);
        m_rail_info->dump(os, indent + 1, indent_size);
        m_message_data->dump(os, indent + 1, indent_size);
        os << indent_str << "}" << std::endl;
    }

    ScopePtr<ISmartResource> SceneInstance::clone(bool deep) const {
        auto scene_instance         = make_scoped<SceneInstance>();
        scene_instance->m_root_path = m_root_path;

        if (deep) {
            scene_instance->m_map_objects   = make_deep_clone(m_map_objects);
            scene_instance->m_table_objects = make_deep_clone(m_table_objects);
            scene_instance->m_rail_info     = make_deep_clone(m_rail_info);
            scene_instance->m_message_data  = m_message_data;
            return scene_instance;
        }

        scene_instance->m_map_objects   = make_clone(m_map_objects);
        scene_instance->m_table_objects = make_clone(m_table_objects);
        scene_instance->m_rail_info     = make_clone(m_rail_info);
        scene_instance->m_message_data  = m_message_data;
        return scene_instance;
    }

#undef VALIDATOR_LT
#undef VALIDATOR_LE
#undef VALIDATOR_GT
#undef VALIDATOR_GE
#undef VALIDATOR_NE
#undef VALIDATOR_EQ

    SceneValidator &SceneValidator::scopeAssertObject(const std::string &obj_type,
                                                      const std::string &obj_name) {
        if (!m_error_callback) {
            m_valid = false;
            return *this;
        }

        if (m_parent_stack.empty()) {
            RefPtr<GroupSceneObject> root = m_scene_instance->getObjHierarchy()->getRoot();
            if (!root || root->type() != obj_type || root->getNameRef().name() != obj_name) {
                m_valid = false;
                m_error_callback(std::format(
                    "Failed to find root object of type '{}' with name '{}'", obj_type, obj_name));
            }
            m_last_obj = root;
            m_processed_objects += 1;
            if (m_progress_callback) {
                std::string progress_text = std::format(
                    "{} ({}) [{}/{}]", root->type(), root->getNameRef().name(),
                    static_cast<int>(m_processed_objects), static_cast<int>(m_total_objects));
                m_progress_callback(m_processed_objects / m_total_objects, progress_text);
            }
            return *this;
        }

        RefPtr<GroupSceneObject> group_obj = m_parent_stack.top();
        if (!group_obj) {
            m_valid = false;
            m_error_callback("Broken parent chain!");
            return *this;
        }

        RefPtr<ISceneObject> child_obj = group_obj->getChild(obj_name);
        if (!child_obj || child_obj->type() != obj_type) {
            m_valid = false;
            m_error_callback(std::format(
                "Failed to find child object of type '{}' with name '{}' in parent '{}'", obj_type,
                obj_name, group_obj->getNameRef().name()));
            return *this;
        }

        m_last_obj = child_obj;
        m_processed_objects += 1;
        if (m_progress_callback) {
            std::string progress_text = std::format(
                "{} ({}) [{}/{}]", child_obj->type(), child_obj->getNameRef().name(),
                static_cast<int>(m_processed_objects), static_cast<int>(m_total_objects));
            m_progress_callback(m_processed_objects / m_total_objects, progress_text);
        }
        return *this;
    }

    SceneValidator &SceneValidator::scopeAssertSize(size_fn predicate) {
        if (!m_error_callback) {
            m_valid = false;
            return *this;
        }

        if (m_parent_stack.empty()) {
            m_valid = false;
            return *this;
        }

        RefPtr<GroupSceneObject> group_obj = m_parent_stack.top();
        if (!group_obj) {
            m_valid = false;
            return *this;
        }

        m_valid = predicate(group_obj->getChildren().size());
        return *this;
    }

    SceneValidator &SceneValidator::scopeEnter() {
        if (!m_error_callback) {
            m_valid = false;
            return *this;
        }

        if (!m_last_obj || !m_last_obj->isGroupObject()) {
            m_valid = false;
            return *this;
        }

        m_parent_stack.push(ref_cast<GroupSceneObject>(m_last_obj));
        return *this;
    }

    SceneValidator &SceneValidator::scopeEscape() {
        if (!m_error_callback) {
            m_valid = false;
            return *this;
        }

        if (m_parent_stack.empty()) {
            m_valid = false;
            return *this;
        }

        m_parent_stack.pop();
        return *this;
    }

    SceneValidator &SceneValidator::scopeValidateDependencies(RefPtr<ISceneObject> object) {
        if (!m_check_dependencies) {
            return *this;
        }

        const std::string &obj_type = object->type();

        auto template_ = TemplateFactory::create(obj_type, true);
        if (!template_) {
            m_valid = false;
            m_error_callback(
                std::format("Failed to load template for object type '{}'!", obj_type));
            return *this;
        }

        std::optional<TemplateWizard> wizard =
            template_.value()->getWizard(object->getWizardName());
        if (!wizard) {
            wizard = template_.value()->getWizard("Default");
            if (!wizard) {
                m_valid = false;
                m_error_callback(std::format("Failed to load the wizard '{}' for object type '{}'!",
                                             object->getWizardName(), obj_type));
                return *this;
            }
        }

        const TemplateDependencies &dependencies = wizard->m_dependencies;
        for (const TemplateDependencies::ObjectInfo &manager : dependencies.m_managers) {
            RefPtr<ISceneObject> group_obj =
                m_scene_instance->getObjHierarchy()->findObject(manager.m_ancestry);
            if (!group_obj || !group_obj->isGroupObject()) {
                m_error_callback(
                    std::format("Provided manager ancestry '{}' is not a group object!",
                                manager.m_ancestry.toString()));
                continue;
            }
            RefPtr<ISceneObject> manager_obj =
                group_obj->getChildByType(manager.m_type, manager.m_name);
            if (!manager_obj) {
                m_valid = false;
                m_error_callback(std::format("Failed to find required manager object of type '{}'!",
                                             manager.m_type));
            }
        }

        for (const std::string &asset_path : dependencies.m_asset_paths) {
            // Assets for an object are found in the subdirectory at SceneAssets/{asset_path}/...
            const fs_path abs_asset_path =
                (Filesystem::current_path().value_or(".") / "SceneAssets" / asset_path)
                    .lexically_normal();
            for (const Filesystem::directory_entry dir_entry :
                 Filesystem::recursive_directory_iterator(abs_asset_path)) {
                if (dir_entry.is_directory()) {
                    continue;
                }

                const fs_path relative_path = Filesystem::relative(dir_entry.path(), abs_asset_path)
                                                  .value_or(dir_entry.path());
                const fs_path scene_path =
                    m_scene_instance->rootPath().value_or(".") / relative_path;

                if (!Filesystem::exists(scene_path).value_or(false)) {
                    m_valid = false;
                    m_error_callback(
                        std::format("Failed to find required asset '{}' for object of type '{}'!",
                                    relative_path.string(), obj_type));
                }
            }
        }

        for (const TemplateDependencies::ObjectInfo &obj : dependencies.m_table_objs) {
            RefPtr<ISceneObject> group_obj =
                m_scene_instance->getObjHierarchy()->findObject(obj.m_ancestry);
            if (!group_obj || !group_obj->isGroupObject()) {
                m_error_callback(
                    std::format("Provided tables.bin ancestry '{}' is not a group object!",
                                obj.m_ancestry.toString()));
                continue;
            }
            RefPtr<ISceneObject> table_obj = group_obj->getChildByType(obj.m_type, obj.m_name);
            if (!table_obj) {
                m_valid = false;
                m_error_callback(std::format(
                    "Failed to find required tables.bin object of type '{}'!", obj.m_type));
            }
        }

        return *this;
    }

    static void forEach(RefPtr<ISceneObject> parent, SceneValidator::foreach_fn fn) {
        for (RefPtr<ISceneObject> obj : parent->getChildren()) {
            fn(obj);
            forEach(obj, fn);
        }
    }

    SceneValidator &SceneValidator::scopeForAll(foreach_fn fn) {
        forEach(m_parent_stack.empty() ? m_scene_instance->getObjHierarchy()->getRoot()
                                       : m_parent_stack.top(),
                fn);
        return *this;
    }

    SceneMender &SceneMender::scopeTryCreateObjectDefault(const std::string &obj_type,
                                                          const std::string &obj_name) {
        if (!m_error_callback) {
            m_valid = false;
            return *this;
        }

        if (m_parent_stack.empty()) {
            RefPtr<GroupSceneObject> root = m_scene_instance->getObjHierarchy()->getRoot();
            if (!root || root->type() != obj_type || root->getNameRef().name() != obj_name) {
                m_valid = false;
                m_error_callback(std::format(
                    "Failed to find root object of type '{}' with name '{}'", obj_type, obj_name));
            }
            m_last_obj = root;
            m_processed_objects += 1;
            if (m_progress_callback) {
                std::string progress_text = std::format(
                    "{} ({}) [{}]", root->type(), root->getNameRef().name(), m_processed_objects);
                m_progress_callback(progress_text);
            }
            return *this;
        }

        RefPtr<GroupSceneObject> group_obj = m_parent_stack.top();
        if (!group_obj) {
            m_valid = false;
            m_error_callback("Broken parent chain!");
            return *this;
        }

        RefPtr<ISceneObject> child_obj = group_obj->getChild(obj_name);
        if (!child_obj || child_obj->type() != obj_type) {
            auto template_ = TemplateFactory::create(obj_type, true);
            if (!template_) {
                m_valid = false;
                m_error_callback(
                    std::format("Failed to load template for object type '{}'!", obj_type));
                return *this;
            }

            RefPtr<ISceneObject> new_obj = ObjectFactory::create(
                *template_.value(), "Default", m_scene_instance->rootPath().value_or(""));

            if (!new_obj) {
                m_valid = false;
                m_error_callback(
                    std::format("Failed to create default object of type '{}' with name '{}'!",
                                obj_type, obj_name));
                return *this;
            }

            new_obj->setNameRef(NameRef(obj_name));
            auto new_result = group_obj->addChild(new_obj);
            if (!new_result) {
                m_valid = false;
                m_error_callback(new_result.error().m_message);
                return *this;
            }

            std::string change_text =
                std::format("Added object [{} ({})] to [{} ({})]", obj_type, obj_name,
                            group_obj->type(), group_obj->getNameRef().name());
            m_change_callback(change_text);
        }

        m_last_obj = child_obj;
        m_processed_objects += 1;
        if (m_progress_callback) {
            std::string progress_text =
                std::format("{} ({}) [{}]", child_obj->type(), child_obj->getNameRef().name(),
                            m_processed_objects);
            m_progress_callback(progress_text);
        }
        return *this;
    }

    SceneMender &SceneMender::scopeEnter() {
        if (!m_error_callback) {
            m_valid = false;
            return *this;
        }

        if (!m_last_obj || !m_last_obj->isGroupObject()) {
            m_valid = false;
            return *this;
        }

        m_parent_stack.push(ref_cast<GroupSceneObject>(m_last_obj));
        return *this;
    }

    SceneMender &SceneMender::scopeEscape() {
        if (!m_error_callback) {
            m_valid = false;
            return *this;
        }

        if (m_parent_stack.empty()) {
            m_valid = false;
            return *this;
        }

        m_parent_stack.pop();
        return *this;
    }

    static void forEachM(RefPtr<ISceneObject> parent, SceneMender::foreach_fn fn) {
        for (RefPtr<ISceneObject> obj : parent->getChildren()) {
            fn(obj);
            forEachM(obj, fn);
        }
    }

    SceneMender &SceneMender::scopeFulfillDependencies(RefPtr<ISceneObject> object) {
        if (!m_check_dependencies) {
            return *this;
        }

        const std::string &obj_type = object->type();

        auto template_ = TemplateFactory::create(obj_type, true);
        if (!template_) {
            m_valid = false;
            m_error_callback(
                std::format("Failed to load template for object type '{}'!", obj_type));
            return *this;
        }

        std::optional<TemplateWizard> wizard =
            template_.value()->getWizard(object->getWizardName());
        if (!wizard) {
            wizard = template_.value()->getWizard("Default");
            if (!wizard) {
                m_valid = false;
                m_error_callback(std::format("Failed to load the wizard '{}' for object type '{}'!",
                                             object->getWizardName(), obj_type));
                return *this;
            }
        }

        const TemplateDependencies &dependencies = wizard->m_dependencies;
        for (const TemplateDependencies::ObjectInfo &manager : dependencies.m_managers) {
            RefPtr<ISceneObject> group_obj =
                m_scene_instance->getObjHierarchy()->findObject(manager.m_ancestry);
            if (!group_obj || !group_obj->isGroupObject()) {
                m_valid = false;
                m_error_callback(
                    std::format("Provided manager ancestry '{}' is not a group object!",
                                manager.m_ancestry.toString()));
                continue;
            }
            RefPtr<ISceneObject> manager_obj =
                group_obj->getChildByType(manager.m_type, manager.m_name);
            if (!manager_obj) {
                manager_obj = ObjectFactory::create(*template_.value(), "Default",
                                                    m_scene_instance->rootPath().value_or("."));
                if (!manager_obj) {
                    m_valid = false;
                    m_error_callback(std::format("Failed to create manager object {} ({})",
                                                 manager.m_type, manager.m_name));
                    continue;
                }
                auto res = group_obj->addChild(manager_obj);
                if (!res) {
                    m_valid = false;
                    m_error_callback(std::format("Failed to add manager to ancestry group: '{}'",
                                                 res.error().m_message));
                }

                std::string change_text =
                    std::format("Added manager [{} ({})] to [{} ({})]", manager.m_type,
                                manager.m_name,
                                group_obj->type(), group_obj->getNameRef().name());
                m_change_callback(change_text);
            }
        }

        for (const std::string &asset_path : dependencies.m_asset_paths) {
            // Assets for an object are found in the subdirectory at SceneAssets/{asset_path}/...
            const fs_path abs_asset_path =
                (Filesystem::current_path().value_or(".") / "SceneAssets" / asset_path)
                    .lexically_normal();
            for (const Filesystem::directory_entry dir_entry :
                 Filesystem::recursive_directory_iterator(abs_asset_path)) {
                const fs_path relative_path = Filesystem::relative(dir_entry.path(), abs_asset_path)
                                                  .value_or(dir_entry.path());
                const fs_path scene_path =
                    m_scene_instance->rootPath().value_or(".") / relative_path;

                if (dir_entry.is_directory()) {
                    if (!Filesystem::exists(scene_path).value_or(false)) {
                        auto res = Filesystem::create_directories(scene_path);
                        if (!res) {
                            m_valid = false;
                            m_error_callback(
                                std::format("Failed to create directories for asset path '{}'",
                                            scene_path.string()));
                        }
                    }
                    continue;
                }

                auto res = Filesystem::copy_file(dir_entry.path(), scene_path,
                                                 Filesystem::copy_options::overwrite_existing);
                if (!res) {
                    m_valid = false;
                    m_error_callback(std::format("Failed to copy file for asset path '{}'",
                                                 scene_path.string()));
                }
            }
        }

        for (const TemplateDependencies::ObjectInfo &obj : dependencies.m_table_objs) {
            RefPtr<ISceneObject> group_obj =
                m_scene_instance->getObjHierarchy()->findObject(obj.m_ancestry);
            if (!group_obj || !group_obj->isGroupObject()) {
                m_valid = false;
                m_error_callback(
                    std::format("Provided manager ancestry '{}' is not a group object!",
                                obj.m_ancestry.toString()));
                continue;
            }
            RefPtr<ISceneObject> table_obj = group_obj->getChildByType(obj.m_type, obj.m_name);
            if (!table_obj) {
                table_obj = ObjectFactory::create(*template_.value(), "Default",
                                                  m_scene_instance->rootPath().value_or("."));
                if (!table_obj) {
                    m_valid = false;
                    m_error_callback(std::format("Failed to create tables.bin object {} ({})",
                                                 obj.m_type, obj.m_name));
                    continue;
                }
                auto res = group_obj->addChild(table_obj);
                if (!res) {
                    m_valid = false;
                    m_error_callback(
                        std::format("Failed to add tables.bin object to ancestry group: '{}'",
                                    res.error().m_message));
                }

                std::string change_text =
                    std::format("Added tables.bin object [{} ({})] to [{} ({})]", obj.m_type, obj.m_name,
                                group_obj->type(), group_obj->getNameRef().name());
                m_change_callback(change_text);
            }
        }

        return *this;
    }

    SceneMender &SceneMender::scopeForAll(foreach_fn fn) {
        forEachM(m_parent_stack.empty() ? m_scene_instance->getObjHierarchy()->getRoot()
                                        : m_parent_stack.top(),
                 fn);
        return *this;
    }

}  // namespace Toolbox