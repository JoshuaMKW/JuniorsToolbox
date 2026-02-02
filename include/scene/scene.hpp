#pragma once

#include <memory>
#include <ostream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "bmg/bmg.hpp"
#include "objlib/object.hpp"
#include "rail/rail.hpp"
#include "raildata.hpp"
#include "smart_resource.hpp"
#include "scene/hierarchy.hpp"

namespace Toolbox::Scene {

    class SceneInstance : public ISmartResource {
    public:
        using validate_progress_cb =
            std::function<void(double progress, const std::string &progress_text)>;
        using validate_error_cb = std::function<void(const std::string &reason)>;

        using repair_progress_cb = std::function<void(const std::string &progress_text)>;
        using repair_change_cb   = std::function<void(const std::string &change_text)>;
        using repair_error_cb    = std::function<void(const std::string &reason)>;

    public:
        SceneInstance();
        SceneInstance(const SceneInstance &);

    public:
        ~SceneInstance();

        static Result<ScopePtr<SceneInstance>, SerialError>
        FromPath(const fs_path &root, bool include_custom_objs);

        [[nodiscard]] static ScopePtr<SceneInstance> EmptyScene() {
            SceneInstance scene;
            return make_scoped<SceneInstance>(std::move(scene));
        }
        [[nodiscard]] static ScopePtr<SceneInstance> BasicScene();

        // WARNING: This method is exhaustive and may take awhile to complete!
        [[nodiscard]] bool validate(bool check_dependencies, validate_progress_cb,
                                    validate_error_cb) const;

        // WARNING: This method is exhaustive and may take awhile to complete!
        [[nodiscard]] bool repair(bool check_dependencies, repair_progress_cb, repair_change_cb,
                                  repair_error_cb) const;

        [[nodiscard]] std::optional<fs_path> rootPath() const { return m_root_path; }

        [[nodiscard]] RefPtr<ObjectHierarchy> getObjHierarchy() const { return m_map_objects; }
        void setObjHierarchy(RefPtr<ObjectHierarchy> obj_root) { m_map_objects = obj_root; }

        [[nodiscard]] RefPtr<ObjectHierarchy> getTableHierarchy() const { return m_table_objects; }
        void setTableHierarchy(RefPtr<ObjectHierarchy> table_root) { m_table_objects = table_root; }

        [[nodiscard]] RefPtr<RailData> getRailData() { return m_rail_info; }
        [[nodiscard]] RefPtr<const RailData> getRailData() const { return m_rail_info; }
        void setRailData(RefPtr<RailData> data) { m_rail_info = data; }

        [[nodiscard]] RefPtr<BMG::MessageData> getMessageData() const { return m_message_data; }
        void setMessageData(RefPtr<BMG::MessageData> message_data) { m_message_data = message_data; }

        Result<void, SerialError> saveToPath(const fs_path &root);

        void dump(std::ostream &os, size_t indent, size_t indent_size) const;
        void dump(std::ostream &os, size_t indent) const { dump(os, indent, 4); }
        void dump(std::ostream &os) const { dump(os, 0); }

        ScopePtr<ISmartResource> clone(bool deep) const override;

    private:
        std::optional<fs_path> m_root_path = {};

        RefPtr<ObjectHierarchy> m_map_objects;
        RefPtr<ObjectHierarchy> m_table_objects;
        RefPtr<RailData> m_rail_info;
        
        RefPtr<BMG::MessageData> m_message_data;
    };

}  // namespace Toolbox