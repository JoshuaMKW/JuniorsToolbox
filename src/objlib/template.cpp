#include "objlib/template.hpp"
#include "objlib/transform.hpp"

namespace Toolbox::Object {

    template <> struct TemplateTypeMap<bool, false> {
        static constexpr TemplateType value = TemplateType::BOOL;
    };

    template <> struct TemplateTypeMap<s8, false> {
        static constexpr TemplateType value = TemplateType::S8;
    };

    template <> struct TemplateTypeMap<u8, false> {
        static constexpr TemplateType value = TemplateType::U8;
    };

    template <> struct TemplateTypeMap<s16, false> {
        static constexpr TemplateType value = TemplateType::S16;
    };

    template <> struct TemplateTypeMap<u16, false> {
        static constexpr TemplateType value = TemplateType::U16;
    };

    template <> struct TemplateTypeMap<s32, false> {
        static constexpr TemplateType value = TemplateType::S32;
    };

    template <> struct TemplateTypeMap<int, false> {
        static constexpr TemplateType value = TemplateType::S32;
    };

    template <> struct TemplateTypeMap<u32, false> {
        static constexpr TemplateType value = TemplateType::U32;
    };

    template <> struct TemplateTypeMap<f32, false> {
        static constexpr TemplateType value = TemplateType::F32;
    };

    template <> struct TemplateTypeMap<f64, false> {
        static constexpr TemplateType value = TemplateType::F64;
    };

    template <> struct TemplateTypeMap<std::string, false> {
        static constexpr TemplateType value = TemplateType::STRING;
    };

    template <> struct TemplateTypeMap<TemplateEnum, false> {
        static constexpr TemplateType value = TemplateType::TRANSFORM;
    };

    template <> struct TemplateTypeMap<TemplateStruct, false> {
        static constexpr TemplateType value = TemplateType::STRUCT;
    };

    template <> struct TemplateTypeMap<std::string, true> {
        static constexpr TemplateType value = TemplateType::COMMENT;
    };

}  // namespace Toolbox::Object