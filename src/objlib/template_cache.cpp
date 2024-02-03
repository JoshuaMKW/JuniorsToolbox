#include "objlib/template.hpp"

namespace Toolbox::Object {

    extern std::unordered_map<std::string, Template> g_template_cache;

    /* FILE FORMAT
    /  -----------
    /  - Header
    /  --------
    /  u32 magic
    /  u32 size
    /  u32
    /
    */

    struct EnumFlagDataLow : public ISerializable {
        u32 m_name_offset;
        u32 m_value;

        std::expected<void, SerialError> deserialize(Deserializer &in) {
            m_name_offset  = in.read<u32>();
            m_value        = in.read<u32>();
        }

        std::expected<void, SerialError> serialize(Serializer &out) {
            out.write(m_name_offset);
            out.write(m_value);
        }
    };

    struct EnumCacheDataLow : public ISerializable {
        MetaType m_type;
        u32 m_flags_count;
        u32 m_flags_offset;
        bool m_multi;

        std::expected<void, SerialError> deserialize(Deserializer &in) {
            m_type          = in.read<MetaType>();
            m_flags_count   = in.read<u32>();
            m_flags_offset  = in.read<u32>();
            m_multi         = in.read<bool>();
        }

        std::expected<void, SerialError> serialize(Serializer &out) {
            out.write(m_type);
            out.write(m_flags_count);
            out.write(m_flags_offset);
            out.write(m_multi);
        }
    };

    struct StructCacheInfoLow : public ISerializable {
        u32 m_member_count;
        u32 m_member_offset;

        std::expected<void, SerialError> deserialize(Deserializer &in) {
            m_member_count = in.read<u32>();
            m_member_offset = in.read<u32>();
        }

        std::expected<void, SerialError> serialize(Serializer &out) {
            out.write(m_member_count);
            out.write(m_member_offset);
        }
    };

    struct TemplateCacheInfoLow : public ISerializable {
        u32 m_name_offset;
        u32 m_wizard_count;
        u32 m_wizard_offset;
        u32 m_struct_count;
        u32 m_struct_offset;
        u32 m_enum_count;
        u32 m_enum_offset;

        std::expected<void, SerialError> deserialize(Deserializer &in) {
            m_name_offset   = in.read<u32>();
            m_wizard_count  = in.read<u32>();
            m_wizard_offset = in.read<u32>();
            m_struct_count  = in.read<u32>();
            m_struct_offset = in.read<u32>();
            m_enum_count    = in.read<u32>();
            m_enum_offset   = in.read<u32>();
        }

        std::expected<void, SerialError> serialize(Serializer &out) {
            out.write(m_name_offset);
            out.write(m_wizard_count);
            out.write(m_wizard_offset);
            out.write(m_struct_count);
            out.write(m_struct_offset);
            out.write(m_enum_count);
            out.write(m_enum_offset);
        }
    };

    struct WizardCacheInfoLow : public ISerializable {
        u32 m_name_offset;
        u32 m_member_count;
        u32 m_member_offset;
        u32 m_model_name_offset;
        u32 m_material_path_offset;
        u32 m_animation_path_count;
        u32 m_animation_path_offset;

        std::expected<void, SerialError> deserialize(Deserializer &in) {
            m_name_offset           = in.read<u32>();
            m_member_count          = in.read<u32>();
            m_member_offset         = in.read<u32>();
            m_model_name_offset     = in.read<u32>();
            m_material_path_offset  = in.read<u32>();
            m_animation_path_count  = in.read<u32>();
            m_animation_path_offset = in.read<u32>();
        }

        std::expected<void, SerialError> serialize(Serializer &out) {
            out.write(m_name_offset);
            out.write(m_member_count);
            out.write(m_member_offset);
            out.write(m_model_name_offset);
            out.write(m_material_path_offset);
            out.write(m_animation_path_count);
            out.write(m_animation_path_offset);
        }
    };

    std::expected<void, SerialError> TemplateFactory::loadFromCacheBlob(Deserializer &in) {
        return std::expected<void, SerialError>();
    }

    std::expected<void, SerialError> TemplateFactory::saveToCacheBlob(Serializer &out) {
        out.pushBreakpoint();
        {
            // Metadata
            out.write<u32>('TMPL');
            out.write<u32>(0);     // TMP SIZE
            out.write<u32>(0x20);  // Enum section offset
            out.write<u32>(0);     // Struct section offset
            out.write<u32>(0);     // Template section offset
            out.write<u32>(0);     // Names section offset
            out.padTo(32, 0);
            
            // Enum info
            out.write<u32>('ENUM');
            out.write<u32>(0);  // Enum count
        }
        out.popBreakpoint();

        return std::expected<void, SerialError>();
    }

}