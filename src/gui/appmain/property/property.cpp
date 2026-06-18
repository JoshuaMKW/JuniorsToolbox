#include "gui/appmain/property/property.hpp"
#include "core/log.hpp"
#include "core/mathutil.hpp"
#include "gui/imgui_ext.hpp"
#include "gui/util.hpp"
#include "objlib/meta/enum.hpp"
#include "objlib/meta/member.hpp"
#include "objlib/meta/value.hpp"
#include "objlib/object.hpp"

#include <imgui.h>

using namespace Toolbox;
using namespace Toolbox::Object;

template <size_t _Size>
static std::array<char, _Size> convertStringToArray(const std::string &str_data) {
    std::array<char, _Size> ret;
    std::copy(str_data.begin(), str_data.begin() + std::min(_Size, str_data.size()), ret.begin());
    return ret;
}

template <size_t _Size>
static std::string_view convertArrayToStringView(const std::array<char, _Size> &str_data) {
    return std::string_view(str_data.data(), str_data.size());
}

template <size_t _Size>
static std::string_view convertArrayToStringView(const std::array<const char, _Size> &str_data) {
    return std::string_view(str_data.data(), str_data.size());
}

static u64 getEnumFlagValue(const MetaEnum::enum_type &enum_flag, MetaType type) {
    switch (type) {
    case MetaType::S8:
        return enum_flag.second.get<s8>().value();
    case MetaType::U8:
        return enum_flag.second.get<u8>().value();
    case MetaType::S16:
        return enum_flag.second.get<s16>().value();
    case MetaType::U16:
        return enum_flag.second.get<u16>().value();
    case MetaType::S32:
        return enum_flag.second.get<s32>().value();
    case MetaType::U32:
        return enum_flag.second.get<u32>().value();
    default:
        return 0;
    }
}

namespace Toolbox::UI {

    void BoolProperty::update_(const u32 array_size) {}

    bool BoolProperty::render_(float label_width) {
        ImGuiStyle &style = ImGui::GetStyle();

        if (m_member->isTypeStruct()) {
            TOOLBOX_ERROR_V("[SCENE_PROPERTY] Trying to render struct \"{}\" as bool",
                            m_member->qualifiedName().toString());
        }

        if (m_member->isTypeEnum()) {
            TOOLBOX_ERROR_V("[SCENE_PROPERTY] Trying to render enum \"{}\" as bool",
                            m_member->qualifiedName().toString());
        }

        // Don't bother rendering when empty
        if (m_member->isEmpty()) {
            return false;
        }

        bool is_open     = getSelfOpen();
        bool any_changed = false;

        const ImVec2 window_size  = ImGui::GetWindowSize();
        const bool collapse_lines = window_size.x < 350;

        if (m_member->isArray()) {
            if (ImGui::BeginGroupPanel(m_member->name().c_str(), &is_open, {})) {
                const u32 array_size = m_member->arraysize();
                for (size_t i = 0; i < array_size; ++i) {
                    std::string id_str = std::format("##{}-{}", m_member->name().c_str(), i);
                    std::string name   = std::format("[{}]", i);

                    ImGui::Text(name.c_str());
                    ImGui::SameLine();

                    MetaValue value = m_value_getter(m_member, i);
                    bool value_bool = value.get<bool>().value();

                    if (ImGui::Checkbox(id_str.c_str(), &value_bool)) {
                        if (m_value_setter) {
                            MetaValue new_value = MetaValue(value_bool);
                            any_changed |= m_value_setter(m_member, i, new_value);
                        }
                    }
                }
            }
            ImGui::EndGroupPanel();

            setSelfOpen(is_open);
            return any_changed;
        }

        ImGui::Text(m_member->name().c_str());
        if (!collapse_lines) {
            ImGui::SameLine();
            ImGui::Dummy({label_width - ImGui::CalcTextSize(m_member->name().c_str()).x, 0});
            ImGui::SameLine();
        }

        MetaValue value = m_value_getter(m_member, 0);
        bool value_bool = value.get<bool>().value();

        std::string id_str = std::format("##{}", m_member->name().c_str());
        if (ImGui::Checkbox(id_str.c_str(), &value_bool)) {
            if (m_value_setter) {
                MetaValue new_value = MetaValue(value_bool);
                any_changed |= m_value_setter(m_member, 0, new_value);
            }
        }

        return any_changed;
    }

    void NumberProperty::update_(const u32 array_size) {
        if (array_size == 0) {
            return;
        }

        switch (Object::getMetaType(m_member).value()) {
        case Object::MetaType::S8:
            m_min =
                Object::getMetaValueMin<s8>(m_member, 0).value_or(std::numeric_limits<s8>::min());
            m_max =
                Object::getMetaValueMax<s8>(m_member, 0).value_or(std::numeric_limits<s8>::max());
            break;
        case Object::MetaType::U8:
            m_min =
                Object::getMetaValueMin<u8>(m_member, 0).value_or(std::numeric_limits<u8>::min());
            m_max =
                Object::getMetaValueMax<u8>(m_member, 0).value_or(std::numeric_limits<u8>::max());
            break;
        case Object::MetaType::S16:
            m_min =
                Object::getMetaValueMin<s16>(m_member, 0).value_or(std::numeric_limits<s16>::min());
            m_max =
                Object::getMetaValueMax<s16>(m_member, 0).value_or(std::numeric_limits<s16>::max());
            break;
        case Object::MetaType::U16:
            m_min =
                Object::getMetaValueMin<u16>(m_member, 0).value_or(std::numeric_limits<u16>::min());
            m_max =
                Object::getMetaValueMax<u16>(m_member, 0).value_or(std::numeric_limits<u16>::max());
            break;
        case Object::MetaType::S32:
            m_min =
                Object::getMetaValueMin<s32>(m_member, 0).value_or(std::numeric_limits<s32>::min());
            m_max =
                Object::getMetaValueMax<s32>(m_member, 0).value_or(std::numeric_limits<s32>::max());
            break;
        case Object::MetaType::U32:
            m_min =
                Object::getMetaValueMin<u32>(m_member, 0).value_or(std::numeric_limits<u32>::min());
            m_max =
                Object::getMetaValueMax<u32>(m_member, 0).value_or(std::numeric_limits<u32>::max());
            break;
        default:
            TOOLBOX_WARN_V(
                "[SCENE_PROPERTY] NumberProperty updateialized with unsupported type \"{}\" for "
                "member \"{}\"",
                magic_enum::enum_name(Object::getMetaType(m_member).value()),
                m_member->qualifiedName().toString());
            m_min =
                Object::getMetaValueMin<u32>(m_member, 0).value_or(std::numeric_limits<s32>::min());
            m_max =
                Object::getMetaValueMax<u32>(m_member, 0).value_or(std::numeric_limits<s32>::max());
            break;
        }
    }

    bool NumberProperty::render_(float label_width) {
        ImGuiStyle &style = ImGui::GetStyle();

        if (m_member->isTypeStruct()) {
            TOOLBOX_ERROR_V("[SCENE_PROPERTY] Trying to render struct \"{}\" as number",
                            m_member->qualifiedName().toString());
        }

        if (m_member->isTypeEnum()) {
            TOOLBOX_ERROR_V("[SCENE_PROPERTY] Trying to render enum \"{}\" as number",
                            m_member->qualifiedName().toString());
        }

        // Don't bother rendering when empty
        if (m_member->isEmpty()) {
            return false;
        }

        bool is_open     = getSelfOpen();
        bool any_changed = false;

        const ImVec2 window_size  = ImGui::GetWindowSize();
        const bool collapse_lines = window_size.x < 350;

        if (m_member->isArray()) {
            if (ImGui::BeginGroupPanel(m_member->name().c_str(), &is_open, {})) {
                const u32 array_size = m_member->arraysize();
                for (size_t i = 0; i < array_size; ++i) {
                    const std::string id_str = std::format("##{}-{}", m_member->name().c_str(), i);
                    const std::string name   = std::format("[{}]", i);

                    ImGui::Text(name.c_str());

                    ImGui::SameLine();

                    MetaValue number = m_value_getter(m_member, i);
                    s64 number_val   = getS64FromMetaValue(number);

                    if (ImGui::InputScalarCompact(id_str.c_str(), ImGuiDataType_S64, &number_val,
                                                  &m_step, &m_step_fast, nullptr,
                                                  ImGuiInputTextFlags_CharsDecimal |
                                                      ImGuiInputTextFlags_CharsNoBlank)) {
                        if (number_val > m_max) {
                            number_val = m_min + (number_val - m_max);
                        } else if (number_val < m_min) {
                            number_val = m_max + (number_val - m_min);
                        }
                        if (m_value_setter) {
                            MetaValue new_value = MetaValue(getMetaType(m_member).value());
                            setS64ToMetaValue(new_value, number_val);
                            any_changed |= m_value_setter(m_member, i, new_value);
                        }
                    }
                }
            }
            ImGui::EndGroupPanel();

            setSelfOpen(is_open);
            return any_changed;
        }

        ImGui::Text(m_member->name().c_str());

        if (!collapse_lines) {
            ImGui::SameLine();
            ImGui::Dummy({label_width - ImGui::CalcTextSize(m_member->name().c_str()).x, 0});
            ImGui::SameLine();
        }

        std::string label = std::format("##{}", m_member->name().c_str());

        MetaValue number = m_value_getter(m_member, 0);
        s64 number_val   = getS64FromMetaValue(number);

        if (ImGui::InputScalarCompact(
                label.c_str(), ImGuiDataType_S64, &number_val, &m_step, &m_step_fast, nullptr,
                ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_CharsNoBlank)) {
            if (number_val > m_max) {
                number_val = m_min + (number_val - m_max);
            } else if (number_val < m_min) {
                number_val = m_max + (number_val - m_min);
            }
            if (m_value_setter) {
                MetaValue new_value = MetaValue(getMetaType(m_member).value());
                setS64ToMetaValue(new_value, number_val);
                any_changed |= m_value_setter(m_member, 0, new_value);
            }
        }

        return any_changed;
    }

    s64 NumberProperty::getS64FromMetaValue(const MetaValue &value) const {
        switch (value.type()) {
        case MetaType::S8:
            return static_cast<s64>(value.get<s8>().value());
        case MetaType::U8:
            return static_cast<s64>(value.get<u8>().value());
        case MetaType::S16:
            return static_cast<s64>(value.get<s16>().value());
        case MetaType::U16:
            return static_cast<s64>(value.get<u16>().value());
        case MetaType::S32:
            return static_cast<s64>(value.get<s32>().value());
        case MetaType::U32:
            return static_cast<s64>(value.get<u32>().value());
        default:
            return -1;
        }
    }

    void NumberProperty::setS64ToMetaValue(Object::MetaValue &value, s64 number) {
        switch (value.type()) {
        case MetaType::S8:
            value.set<s8>(static_cast<s8>(number));
            return;
        case MetaType::U8:
            value.set<u8>(static_cast<u8>(number));
            return;
        case MetaType::S16:
            value.set<s16>(static_cast<s16>(number));
            return;
        case MetaType::U16:
            value.set<u16>(static_cast<u16>(number));
            return;
        case MetaType::S32:
            value.set<s32>(static_cast<s32>(number));
            return;
        case MetaType::U32:
            value.set<u32>(static_cast<u32>(number));
            return;
        default:
            return;
        }
    }

    void FloatProperty::update_(const u32 array_size) {
        if (array_size == 0) {
            return;
        }

        m_min = Object::getMetaValueMin<f32>(m_member, 0).value_or(std::numeric_limits<f32>::min());
        m_max = Object::getMetaValueMax<f32>(m_member, 0).value_or(std::numeric_limits<f32>::max());
    }

    bool FloatProperty::render_(float label_width) {
        ImGuiStyle &style = ImGui::GetStyle();

        if (m_member->isTypeStruct()) {
            TOOLBOX_ERROR_V("[SCENE_PROPERTY] Trying to render struct \"{}\" as float",
                            m_member->qualifiedName().toString());
        }

        if (m_member->isTypeEnum()) {
            TOOLBOX_ERROR_V("[SCENE_PROPERTY] Trying to render enum \"{}\" as float",
                            m_member->qualifiedName().toString());
        }

        // Don't bother rendering when empty
        if (m_member->isEmpty()) {
            return false;
        }

        bool is_open     = getSelfOpen();
        bool any_changed = false;

        const ImVec2 window_size  = ImGui::GetWindowSize();
        const bool collapse_lines = window_size.x < 350;

        if (m_member->isArray()) {
            if (ImGui::BeginGroupPanel(m_member->name().c_str(), &is_open, {})) {
                const u32 array_size = m_member->arraysize();
                for (size_t i = 0; i < array_size; ++i) {
                    std::string id_str = std::format("##{}-{}", m_member->name().c_str(), i);
                    std::string name   = std::format("[{}]", i);

                    ImGui::Text(name.c_str());
                    ImGui::SameLine();

                    MetaValue value = m_value_getter(m_member, i);
                    f32 value_flt   = value.get<f32>().value();

                    if (ImGui::InputScalarCompact(id_str.c_str(), ImGuiDataType_Float, &value_flt,
                                                  &m_step, &m_step_fast, nullptr,
                                                  ImGuiInputTextFlags_CharsDecimal |
                                                      ImGuiInputTextFlags_CharsNoBlank)) {
                        value_flt = Toolbox::Wrap(value_flt, m_min, m_max);
                        if (m_value_setter) {
                            MetaValue new_value = MetaValue(getMetaType(m_member).value());
                            new_value.setVariant(value_flt);
                            any_changed |= m_value_setter(m_member, i, new_value);
                        }
                    }
                }
            }
            ImGui::EndGroupPanel();

            setSelfOpen(is_open);
            return any_changed;
        }

        ImGui::Text(m_member->name().c_str());

        if (!collapse_lines) {
            ImGui::SameLine();
            ImGui::Dummy({label_width - ImGui::CalcTextSize(m_member->name().c_str()).x, 0});
            ImGui::SameLine();
        }

        MetaValue value = m_value_getter(m_member, 0);
        f32 value_flt   = value.get<f32>().value();

        std::string label = std::format("##{}", m_member->name().c_str());
        if (ImGui::InputScalarCompact(
                label.c_str(), ImGuiDataType_Float, &value_flt, &m_step, &m_step_fast, nullptr,
                ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_CharsNoBlank)) {
            value_flt = Toolbox::Wrap(value_flt, m_min, m_max);
            if (m_value_setter) {
                MetaValue new_value = MetaValue(getMetaType(m_member).value());
                new_value.setVariant(value_flt);
                any_changed |= m_value_setter(m_member, 0, new_value);
            }
        }

        return any_changed;
    }

    void DoubleProperty::update_(const u32 array_size) {
        if (array_size == 0) {
            return;
        }

        m_min = Object::getMetaValueMin<f64>(m_member, 0).value_or(std::numeric_limits<f64>::min());
        m_max = Object::getMetaValueMax<f64>(m_member, 0).value_or(std::numeric_limits<f64>::max());
    }

    bool DoubleProperty::render_(float label_width) {
        ImGuiStyle &style = ImGui::GetStyle();

        if (m_member->isTypeStruct()) {
            TOOLBOX_ERROR_V("[SCENE_PROPERTY] Trying to render struct \"{}\" as double",
                            m_member->qualifiedName().toString());
        }

        if (m_member->isTypeEnum()) {
            TOOLBOX_ERROR_V("[SCENE_PROPERTY] Trying to render enum \"{}\" as double",
                            m_member->qualifiedName().toString());
        }

        // Don't bother rendering when empty
        if (m_member->isEmpty()) {
            return false;
        }

        bool is_open     = getSelfOpen();
        bool any_changed = false;

        const ImVec2 window_size  = ImGui::GetWindowSize();
        const bool collapse_lines = window_size.x < 350;

        if (m_member->isArray()) {
            if (ImGui::BeginGroupPanel(m_member->name().c_str(), &is_open, {})) {
                const u32 array_size = m_member->arraysize();
                for (size_t i = 0; i < array_size; ++i) {
                    std::string id_str = std::format("##{}-{}", m_member->name().c_str(), i);
                    std::string name   = std::format("[{}]", i);

                    ImGui::Text(name.c_str());
                    ImGui::SameLine();

                    MetaValue value = m_value_getter(m_member, i);
                    f64 value_dbl   = value.get<f64>().value();

                    if (ImGui::InputScalarCompact(id_str.c_str(), ImGuiDataType_Double, &value_dbl,
                                                  &m_step, &m_step_fast, nullptr,
                                                  ImGuiInputTextFlags_CharsDecimal |
                                                      ImGuiInputTextFlags_CharsNoBlank)) {
                        value_dbl = Toolbox::Wrap(value_dbl, m_min, m_max);
                        if (m_value_setter) {
                            MetaValue new_value = MetaValue(getMetaType(m_member).value());
                            new_value.setVariant(value_dbl);
                            any_changed |= m_value_setter(m_member, i, new_value);
                        }
                    }
                }
            }
            ImGui::EndGroupPanel();

            setSelfOpen(is_open);
            return any_changed;
        }

        ImGui::Text(m_member->name().c_str());

        if (!collapse_lines) {
            ImGui::SameLine();
            ImGui::Dummy({label_width - ImGui::CalcTextSize(m_member->name().c_str()).x, 0});
            ImGui::SameLine();
        }

        MetaValue value = m_value_getter(m_member, 0);
        f64 value_dbl   = value.get<f64>().value();

        std::string label = std::format("##{}", m_member->name().c_str());
        if (ImGui::InputScalarCompact(
                label.c_str(), ImGuiDataType_Double, &value_dbl, &m_step, &m_step_fast, nullptr,
                ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_CharsNoBlank)) {
            value_dbl = Toolbox::Wrap(value_dbl, m_min, m_max);
            if (m_value_setter) {
                MetaValue new_value = MetaValue(getMetaType(m_member).value());
                new_value.setVariant(value_dbl);
                any_changed |= m_value_setter(m_member, 0, new_value);
            }
        }

        return any_changed;
    }

    void StringProperty::update_(const u32 array_size) {}

    bool StringProperty::render_(float label_width) {
        ImGuiStyle &style = ImGui::GetStyle();

        if (m_member->isTypeStruct()) {
            TOOLBOX_ERROR_V("[SCENE_PROPERTY] Trying to render struct \"{}\" as string",
                            m_member->qualifiedName().toString());
        }

        if (m_member->isTypeEnum()) {
            TOOLBOX_ERROR_V("[SCENE_PROPERTY] Trying to render enum \"{}\" as string",
                            m_member->qualifiedName().toString());
        }

        // Don't bother rendering when empty
        if (m_member->isEmpty()) {
            return false;
        }

        bool is_open     = getSelfOpen();
        bool any_changed = false;

        const ImVec2 window_size  = ImGui::GetWindowSize();
        const bool collapse_lines = window_size.x < 350;

        if (m_member->isArray()) {
            if (ImGui::BeginGroupPanel(m_member->name().c_str(), &is_open, {})) {
                const u32 array_size = m_member->arraysize();
                for (size_t i = 0; i < array_size; ++i) {
                    std::string id_str = std::format("##{}-{}", m_member->name().c_str(), i);
                    std::string name   = std::format("[{}]", i);

                    ImGui::Text(name.c_str());
                    ImGui::SameLine();

                    MetaValue value      = m_value_getter(m_member, i);
                    std::string str_data = value.get<std::string>().value();

                    std::array<char, 256> str_ary{};
                    str_ary.fill('\0');
                    std::strncpy(str_ary.data(), str_data.c_str(), str_data.size());

                    if (ImGui::InputText(id_str.c_str(), str_ary.data(), str_ary.size())) {
                        if (m_value_setter) {
                            MetaValue new_value       = MetaValue(getMetaType(m_member).value());
                            std::string_view str_view = convertArrayToStringView(str_ary);
                            new_value.set(str_view);
                            any_changed |= m_value_setter(m_member, i, new_value);
                        }
                    }
                }
            }
            ImGui::EndGroupPanel();

            setSelfOpen(is_open);
            return any_changed;
        }

        ImGui::Text(m_member->name().c_str());

        if (!collapse_lines) {
            ImGui::SameLine();
            ImGui::Dummy({label_width - ImGui::CalcTextSize(m_member->name().c_str()).x, 0});
            ImGui::SameLine();
        }

        std::string label = std::format("##{}", m_member->name().c_str());

        MetaValue value      = m_value_getter(m_member, 0);
        std::string str_data = value.get<std::string>().value();

        std::array<char, 256> str_ary{};
        str_ary.fill('\0');
        std::strncpy(str_ary.data(), str_data.c_str(), str_data.size());

        if (ImGui::InputText(label.c_str(), str_ary.data(), str_ary.size())) {
            if (m_value_setter) {
                MetaValue new_value       = MetaValue(getMetaType(m_member).value());
                std::string_view str_view = convertArrayToStringView(str_ary);
                new_value.set(str_view);
                any_changed |= m_value_setter(m_member, 0, new_value);
            }
        }

        return any_changed;
    }

    void ColorProperty::update_(const u32 array_size) {}

    bool ColorProperty::render_(float label_width) {
        ImGuiStyle &style = ImGui::GetStyle();

        if (m_member->isTypeStruct()) {
            TOOLBOX_ERROR_V("[SCENE_PROPERTY] Trying to render struct \"{}\" as color",
                            m_member->qualifiedName().toString());
        }

        if (m_member->isTypeEnum()) {
            TOOLBOX_ERROR_V("[SCENE_PROPERTY] Trying to render enum \"{}\" as color",
                            m_member->qualifiedName().toString());
        }

        const bool isRGB    = m_member->isTypeRGB();
        const bool isRGBA   = m_member->isTypeRGBA();
        const bool isRGB32  = m_member->isTypeRGB32();
        const bool isRGBA32 = m_member->isTypeRGBA32();

        const bool use_alpha = isRGBA || isRGBA32;

        // Don't bother rendering when empty
        if (m_member->isEmpty()) {
            return false;
        }

        bool is_open     = getSelfOpen();
        bool any_changed = false;

        const ImVec2 window_size  = ImGui::GetWindowSize();
        const bool collapse_lines = window_size.x < 350;

        if (m_member->isArray()) {
            if (ImGui::BeginGroupPanel(m_member->name().c_str(), &is_open, {})) {
                const u32 array_size = m_member->arraysize();
                for (size_t i = 0; i < array_size; ++i) {
                    Color::RGBAShader color = getColor(i);

                    std::string id_str = std::format("##{}-{}", m_member->name().c_str(), i);
                    std::string name   = std::format("[{}]", i);

                    if (use_alpha) {
                        if (ImGui::ColorEdit4(name.c_str(), &color.m_r)) {
                            if (setColor(i, color)) {
                                any_changed = true;
                            }
                        }
                    } else {
                        if (ImGui::ColorEdit3(name.c_str(), &color.m_r)) {
                            if (setColor(i, color)) {
                                any_changed = true;
                            }
                        }
                    }
                }
            }
            ImGui::EndGroupPanel();

            setSelfOpen(is_open);
            return any_changed;
        }

        ImGui::Text(m_member->name().c_str());

        if (!collapse_lines) {
            ImGui::SameLine();
            ImGui::Dummy({label_width - ImGui::CalcTextSize(m_member->name().c_str()).x, 0});
            ImGui::SameLine();
        }

        Color::RGBAShader color = getColor(0);

        if (use_alpha) {
            if (ImGui::ColorEdit4(m_member->name().c_str(), &color.m_r)) {
                if (setColor(0, color)) {
                    any_changed = true;
                }
            }
        } else {
            if (ImGui::ColorEdit3(m_member->name().c_str(), &color.m_r)) {
                if (setColor(0, color)) {
                    any_changed = true;
                }
            }
        }

        return any_changed;
    }

    Color::RGBAShader ColorProperty::getColor(int array_index) {
        Color::RGBAShader out_color;

        const bool isRGB    = m_member->isTypeRGB();
        const bool isRGBA   = m_member->isTypeRGBA();
        const bool isRGB32  = m_member->isTypeRGB32();
        const bool isRGBA32 = m_member->isTypeRGBA32();

        if (isRGB) {
            Color::RGB8 tmp_color =
                m_value_getter(m_member, array_index).get<Color::RGB8>().value_or(Color::RGB8());
            f32 r, g, b, a;
            tmp_color.getColor(r, g, b, a);
            out_color.setColor(r, g, b, a);
        }

        if (isRGBA) {
            Color::RGBA8 tmp_color =
                m_value_getter(m_member, array_index).get<Color::RGBA8>().value_or(Color::RGBA8());
            f32 r, g, b, a;
            tmp_color.getColor(r, g, b, a);
            out_color.setColor(r, g, b, a);
        }

        if (isRGB32) {
            Color::RGB32 tmp_color =
                m_value_getter(m_member, array_index).get<Color::RGB32>().value_or(Color::RGB32());
            f32 r, g, b, a;
            tmp_color.getColor(r, g, b, a);
            out_color.setColor(r, g, b, a);
        }

        if (isRGBA32) {
            Color::RGBA32 tmp_color = m_value_getter(m_member, array_index)
                                          .get<Color::RGBA32>()
                                          .value_or(Color::RGBA32());
            f32 r, g, b, a;
            tmp_color.getColor(r, g, b, a);
            out_color.setColor(r, g, b, a);
        }

        return out_color;
    }

    bool ColorProperty::setColor(int array_index, const Color::RGBAShader &color) {
        const bool isRGB    = m_member->isTypeRGB();
        const bool isRGBA   = m_member->isTypeRGBA();
        const bool isRGB32  = m_member->isTypeRGB32();
        const bool isRGBA32 = m_member->isTypeRGBA32();

        if (isRGB) {
            if (m_value_setter) {
                MetaValue new_value = MetaValue(getMetaType(m_member).value());
                new_value.setVariant(Color::RGB8(color.m_r, color.m_g, color.m_b));
                return m_value_setter(m_member, array_index, new_value);
            }
        }

        if (isRGBA) {
            if (m_value_setter) {
                MetaValue new_value = MetaValue(getMetaType(m_member).value());
                new_value.setVariant(Color::RGBA8(color.m_r, color.m_g, color.m_b, color.m_a));
                return m_value_setter(m_member, array_index, new_value);
            }
        }

        if (isRGB32) {
            if (m_value_setter) {
                MetaValue new_value = MetaValue(getMetaType(m_member).value());
                new_value.setVariant(Color::RGB32(color.m_r, color.m_g, color.m_b));
                return m_value_setter(m_member, array_index, new_value);
            }
        }

        if (isRGBA32) {
            if (m_value_setter) {
                MetaValue new_value = MetaValue(getMetaType(m_member).value());
                new_value.setVariant(Color::RGBA32(color.m_r, color.m_g, color.m_b, color.m_a));
                return m_value_setter(m_member, array_index, new_value);
            }
        }

        return false;
    }

    void VectorProperty::update_(const u32 array_size) {
        switch (Object::getMetaType(m_member).value()) {
        default:
        case Object::MetaType::VEC3:
            m_min = -FLT_MAX;
            m_max = FLT_MAX;
            break;
        }
    }

    bool VectorProperty::render_(float label_width) {
        ImGuiStyle &style = ImGui::GetStyle();

        if (m_member->isTypeStruct()) {
            TOOLBOX_ERROR_V("[SCENE_PROPERTY] Trying to render struct \"{}\" as vector",
                            m_member->qualifiedName().toString());
        }

        if (m_member->isTypeEnum()) {
            TOOLBOX_ERROR_V("[SCENE_PROPERTY] Trying to render enum \"{}\" as vector",
                            m_member->qualifiedName().toString());
        }

        // Don't bother rendering when empty
        if (m_member->isEmpty()) {
            return false;
        }

        bool is_open     = getSelfOpen();
        bool any_changed = false;

        const ImVec2 window_size  = ImGui::GetWindowSize();
        const bool collapse_lines = window_size.x < 350;

        ImGui::SetNextItemOpen(is_open, ImGuiCond_Always);

        const bool header_open = ImGui::CollapsingHeader(m_member->name().c_str());

        if (ImGui::IsItemToggledOpen()) {
            setSelfOpen(!is_open);
        }

        if (header_open) {
            const u32 array_size = m_member->arraysize();
            if (m_member->isArray()) {
                for (size_t i = 0; i < array_size; ++i) {
                    const std::string array_name = std::format("[{}]##{}", i, m_member->name().c_str());
                    
                    bool array_open  = getArrayIdxOpen(i);
                    if (ImGui::BeginGroupPanel(array_name.c_str(),
                                               &array_open,
                                               {})) {
                        MetaValue value     = m_value_getter(m_member, i);
                        glm::vec3 value_vec = value.get<glm::vec3>().value();

                        ImGui::Text(m_member->name().c_str());
                        if (!collapse_lines) {
                            ImGui::SameLine();
                            ImGui::Dummy(
                                {label_width - ImGui::CalcTextSize(m_member->name().c_str()).x, 0});
                            ImGui::SameLine();
                        }
                        if (ImGui::InputScalarCompactN("##vector", ImGuiDataType_Float,
                                                       reinterpret_cast<f32 *>(&value_vec.x), 3,
                                                       &m_step, &m_step_fast, "%.3f")) {
                            if (m_value_setter) {
                                MetaValue new_value = MetaValue(getMetaType(m_member).value());
                                new_value.setVariant(value_vec);
                                any_changed |= m_value_setter(m_member, i, new_value);
                            }
                        }
                        ImGui::Spacing();
                    }
                    ImGui::EndGroupPanel();

                    setArrayIdxOpen(i, array_open);
                }
            } else {
                MetaValue value     = m_value_getter(m_member, 0);
                glm::vec3 value_vec = value.get<glm::vec3>().value();

                ImGui::Text(m_member->name().c_str());
                if (!collapse_lines) {
                    ImGui::SameLine();
                    ImGui::Dummy(
                        {label_width - ImGui::CalcTextSize(m_member->name().c_str()).x, 0});
                    ImGui::SameLine();
                }
                if (ImGui::InputScalarCompactN("##vector", ImGuiDataType_Float,
                                               reinterpret_cast<f32 *>(&value_vec.x), 3, &m_step,
                                               &m_step_fast, "%.3f")) {
                    if (m_value_setter) {
                        MetaValue new_value = MetaValue(getMetaType(m_member).value());
                        new_value.setVariant(value_vec);
                        any_changed |= m_value_setter(m_member, 0, new_value);
                    }
                }
                ImGui::Spacing();
            }
        }

        ImGui::ItemSize({0, 4});

        return any_changed;
    }

    void TransformProperty::update_(const u32 array_size) {
        switch (Object::getMetaType(m_member).value()) {
        default:
        case Object::MetaType::TRANSFORM:
            m_min = -FLT_MAX;
            m_max = FLT_MAX;
            break;
        }
    }

    bool TransformProperty::render_(float label_width) {
        ImGuiStyle &style = ImGui::GetStyle();

        if (m_member->isTypeStruct()) {
            TOOLBOX_ERROR_V("[SCENE_PROPERTY] Trying to render struct \"{}\" as transform",
                            m_member->qualifiedName().toString());
        }

        if (m_member->isTypeEnum()) {
            TOOLBOX_ERROR_V("[SCENE_PROPERTY] Trying to render enum \"{}\" as transform",
                            m_member->qualifiedName().toString());
        }

        // Don't bother rendering when empty
        if (m_member->isEmpty()) {
            return false;
        }

        bool is_open     = getSelfOpen();
        bool any_changed = false;

        const ImVec2 window_size  = ImGui::GetWindowSize();
        const bool collapse_lines = window_size.x < 350;

        ImGui::SetNextItemOpen(is_open, ImGuiCond_Once);

        const bool header_open = ImGui::CollapsingHeader(m_member->name().c_str());

        if (ImGui::IsItemToggledOpen()) {
            setSelfOpen(!is_open);
        }

        if (header_open) {
            if (m_member->isArray() || m_member->isEmpty()) {
                //float label_width    = 0;
                const u32 array_size = m_member->arraysize();
                for (size_t i = 0; i < array_size; ++i) {
                    const std::string array_name = std::format("[{}]##{}", i, m_member->name().c_str());

                    bool array_open = getArrayIdxOpen(i);
                    if (ImGui::BeginGroupPanel(array_name.c_str(), &array_open,
                                               {})) {
                        ImGui::PushID("Translation");
                        ImGui::Text("Translation");

                        MetaValue value           = m_value_getter(m_member, i);
                        Transform value_transform = value.get<Transform>().value();

                        if (!collapse_lines) {
                            ImGui::SameLine();
                            ImGui::Dummy({label_width - ImGui::CalcTextSize("Translation").x, 0});
                            ImGui::SameLine();
                        }
                        if (ImGui::InputScalarCompactN(
                                "##Translation", ImGuiDataType_Float,
                                reinterpret_cast<f32 *>(&value_transform.m_translation), 3, &m_step,
                                &m_step_fast, "%.3f")) {
                            if (m_value_setter) {
                                MetaValue new_value = MetaValue(getMetaType(m_member).value());
                                new_value.setVariant(value_transform);
                                any_changed |= m_value_setter(m_member, i, new_value);
                            }
                        }
                        ImGui::Spacing();
                        ImGui::PopID();

                        ImGui::PushID("Rotation");
                        ImGui::Text("Rotation");
                        if (!collapse_lines) {
                            ImGui::SameLine();
                            ImGui::Dummy({label_width - ImGui::CalcTextSize("Rotation").x, 0});
                            ImGui::SameLine();
                        }
                        if (ImGui::InputScalarCompactN(
                                "##Rotation", ImGuiDataType_Float,
                                reinterpret_cast<f32 *>(&value_transform.m_rotation), 3, &m_step,
                                &m_step_fast, "%.3f")) {
                            if (m_value_setter) {
                                MetaValue new_value = MetaValue(getMetaType(m_member).value());
                                new_value.setVariant(value_transform);
                                any_changed |= m_value_setter(m_member, i, new_value);
                            }
                        }
                        ImGui::Spacing();
                        ImGui::PopID();

                        ImGui::PushID("Scale");
                        ImGui::Text("Scale");
                        if (!collapse_lines) {
                            ImGui::SameLine();
                            ImGui::Dummy({label_width - ImGui::CalcTextSize("Scale").x, 0});
                            ImGui::SameLine();
                        }
                        if (ImGui::InputScalarCompactN(
                                "##Scale", ImGuiDataType_Float,
                                reinterpret_cast<f32 *>(&value_transform.m_scale), 3, &m_step,
                                &m_step_fast, "%.3f")) {
                            if (m_value_setter) {
                                MetaValue new_value = MetaValue(getMetaType(m_member).value());
                                new_value.setVariant(value_transform);
                                any_changed |= m_value_setter(m_member, i, new_value);
                            }
                        }
                        ImGui::Spacing();
                        ImGui::PopID();
                    }
                    ImGui::EndGroupPanel();

                    setArrayIdxOpen(i, array_open);
                }
            } else {
                ImGui::PushID("Translation");
                ImGui::Text("Translation");

                MetaValue value           = m_value_getter(m_member, 0);
                Transform value_transform = value.get<Transform>().value();

                if (!collapse_lines) {
                    ImGui::SameLine();
                    ImGui::Dummy({label_width - ImGui::CalcTextSize("Translation").x, 0});
                    ImGui::SameLine();
                }
                if (ImGui::InputScalarCompactN(
                        "##Translation", ImGuiDataType_Float,
                        reinterpret_cast<f32 *>(&value_transform.m_translation), 3, &m_step,
                        &m_step_fast, "%.3f")) {
                    if (m_value_setter) {
                        MetaValue new_value = MetaValue(getMetaType(m_member).value());
                        new_value.setVariant(value_transform);
                        any_changed |= m_value_setter(m_member, 0, new_value);
                    }
                }
                ImGui::Spacing();
                ImGui::PopID();

                ImGui::PushID("Rotation");
                ImGui::Text("Rotation");
                if (!collapse_lines) {
                    ImGui::SameLine();
                    ImGui::Dummy({label_width - ImGui::CalcTextSize("Rotation").x, 0});
                    ImGui::SameLine();
                }
                if (ImGui::InputScalarCompactN("##Rotation", ImGuiDataType_Float,
                                               reinterpret_cast<f32 *>(&value_transform.m_rotation),
                                               3, &m_step, &m_step_fast, "%.3f")) {
                    if (m_value_setter) {
                        MetaValue new_value = MetaValue(getMetaType(m_member).value());
                        new_value.setVariant(value_transform);
                        any_changed |= m_value_setter(m_member, 0, new_value);
                    }
                }
                ImGui::Spacing();
                ImGui::PopID();

                ImGui::PushID("Scale");
                ImGui::Text("Scale");
                if (!collapse_lines) {
                    ImGui::SameLine();
                    ImGui::Dummy({label_width - ImGui::CalcTextSize("Scale").x, 0});
                    ImGui::SameLine();
                }
                if (ImGui::InputScalarCompactN("##Scale", ImGuiDataType_Float,
                                               reinterpret_cast<f32 *>(&value_transform.m_scale), 3,
                                               &m_step, &m_step_fast, "%.3f")) {
                    if (m_value_setter) {
                        MetaValue new_value = MetaValue(getMetaType(m_member).value());
                        new_value.setVariant(value_transform);
                        any_changed |= m_value_setter(m_member, 0, new_value);
                    }
                }
                ImGui::Spacing();
                ImGui::PopID();
            }
        }
        ImGui::ItemSize({0, 4});

        return any_changed;
    }

    void EnumProperty::update_(const u32 array_size) {
        m_checked_state.resize(array_size);

        auto enum_values    = Object::getMetaEnumValues(m_member).value();
        auto enum_type      = Object::getMetaType(m_member).value();
        bool enum_bitmasked = Object::getMetaEnumBitmasked(m_member).value();

        for (size_t i = 0; i < m_checked_state.size(); ++i) {
            MetaValue number = m_value_getter(m_member, i);
            s64 number_val   = getS64FromMetaValue(number);

            auto &state = m_checked_state.at(i);
            m_checked_state.at(i).resize(enum_values.size());
            for (size_t j = 0; j < enum_values.size(); ++j) {
                if (enum_bitmasked) {
                    if ((number_val & getEnumFlagValue(enum_values.at(j), enum_type)) != 0) {
                        state.at(j) = true;
                    }
                } else {
                    if (number_val == getEnumFlagValue(enum_values.at(j), enum_type)) {
                        state.at(j) = true;
                    }
                }
            }
        }
    }

    bool EnumProperty::render_(float label_width) {
        ImGuiStyle &style = ImGui::GetStyle();

        if (m_member->isTypeStruct()) {
            TOOLBOX_ERROR_V("[SCENE_PROPERTY] Trying to render struct \"{}\" as enum",
                            m_member->qualifiedName().toString());
        }

        if (!m_member->isTypeEnum()) {
            TOOLBOX_ERROR_V("[SCENE_PROPERTY] Trying to render value \"{}\" as enum",
                            m_member->qualifiedName().toString());
        }

        auto enum_values    = Object::getMetaEnumValues(m_member).value();
        auto enum_type      = Object::getMetaType(m_member).value();
        bool enum_bitmasked = Object::getMetaEnumBitmasked(m_member).value();

        // Don't bother rendering when empty
        if (m_member->isEmpty()) {
            return false;
        }

        bool is_open     = getSelfOpen();
        bool any_changed = false;

        const ImVec2 window_size  = ImGui::GetWindowSize();
        const bool collapse_lines = window_size.x < 350;

        if (enum_bitmasked) {
            if (m_member->isArray()) {
                if (ImGui::BeginGroupPanel(m_member->name().c_str(), &is_open, {})) {
                    for (size_t i = 0; i < m_checked_state.size(); ++i) {
                        const std::string array_str = std::format("[{}]", i).c_str();

                        bool array_open = getArrayIdxOpen(i);
                        if (ImGui::BeginGroupPanel(array_str.c_str(),
                                                   &array_open,
                                                   {})) {
                            MetaValue number = m_value_getter(m_member, i);
                            s64 number_val   = getS64FromMetaValue(number);

                            for (size_t j = 0; j < m_checked_state.at(0).size(); ++j) {
                                if (ImGui::Checkbox(enum_values.at(j).first.c_str(),
                                                    reinterpret_cast<bool *>(
                                                        m_checked_state.at(i).data() + j))) {
                                    bool checked = m_checked_state.at(i).at(j);
                                    if (checked) {
                                        number_val |=
                                            getEnumFlagValue(enum_values.at(j), enum_type);
                                    } else {
                                        number_val &=
                                            ~getEnumFlagValue(enum_values.at(j), enum_type);
                                    }
                                }
                            }
                            if (m_value_setter) {
                                MetaValue new_value = MetaValue(getMetaType(m_member).value());
                                setS64ToMetaValue(new_value, number_val);
                                any_changed |= m_value_setter(m_member, i, new_value);
                            }
                        }
                        ImGui::EndGroupPanel();

                        setArrayIdxOpen(i, array_open);
                    }
                }
                ImGui::EndGroupPanel();

                setSelfOpen(is_open);
                return any_changed;
            }

            if (ImGui::BeginGroupPanel(m_member->name().c_str(), &is_open, {})) {
                MetaValue number = m_value_getter(m_member, 0);
                s64 number_val   = getS64FromMetaValue(number);
                s64 old_val      = number_val;

                for (size_t j = 0; j < m_checked_state.at(0).size(); ++j) {
                    if (ImGui::Checkbox(
                            enum_values.at(j).first.c_str(),
                            reinterpret_cast<bool *>(m_checked_state.at(0).data() + j))) {
                        bool checked = m_checked_state.at(0).at(j);
                        if (checked) {
                            number_val |= getEnumFlagValue(enum_values.at(j), enum_type);
                        } else {
                            number_val &= ~getEnumFlagValue(enum_values.at(j), enum_type);
                        }
                    }
                }
                if (m_value_setter && number_val != old_val) {
                    MetaValue new_value = MetaValue(getMetaType(m_member).value());
                    setS64ToMetaValue(new_value, number_val);
                    any_changed |= m_value_setter(m_member, 0, new_value);
                }
            }
            ImGui::EndGroupPanel();

            setSelfOpen(is_open);
            return any_changed;
        } else {
            if (m_member->isArray()) {
                if (ImGui::BeginGroupPanel(m_member->name().c_str(), &is_open, {})) {
                    for (size_t i = 0; i < m_checked_state.size(); ++i) {
                        std::vector<char> &checked_state = m_checked_state.at(i);

                        const int selected_index = [&]() {
                            for (int i = 0; i < checked_state.size(); ++i) {
                                if (checked_state.at(i)) {
                                    return i;
                                }
                            }
                            return -1;
                        }();

                        MetaValue number = m_value_getter(m_member, i);
                        s64 number_val   = getS64FromMetaValue(number);

                        std::string preview_value = selected_index >= 0
                                                        ? enum_values.at(selected_index).first
                                                        : "[[Invalid State]]";
                        if (ImGui::BeginCombo("##enum_combobox", preview_value.c_str())) {
                            for (int j = 0; j < enum_values.size(); ++j) {
                                std::string enum_str = enum_values.at(j).first;
                                bool selected        = j == selected_index;
                                if (ImGui::Selectable(enum_str.c_str(), &selected)) {
                                    std::fill(checked_state.begin(), checked_state.end(), false);
                                    number_val = getEnumFlagValue(enum_values.at(j), enum_type);
                                    checked_state[j] = true;
                                    any_changed      = true;

                                    if (m_value_setter) {
                                        MetaValue new_value =
                                            MetaValue(getMetaType(m_member).value());
                                        setS64ToMetaValue(new_value, number_val);
                                        any_changed |= m_value_setter(m_member, i, new_value);
                                    }
                                }
                            }
                            ImGui::EndCombo();
                        }
                    }
                }
                ImGui::EndGroupPanel();

                setSelfOpen(is_open);
                return any_changed;
            }

            ImGui::Text(m_member->name().c_str());

            if (!collapse_lines) {
                ImGui::SameLine();
                ImGui::Dummy({label_width - ImGui::CalcTextSize(m_member->name().c_str()).x, 0});
                ImGui::SameLine();
            }

            std::vector<char> &checked_state = m_checked_state.at(0);

            const int selected_index = [&]() {
                for (int i = 0; i < checked_state.size(); ++i) {
                    if (checked_state.at(i)) {
                        return i;
                    }
                }
                return -1;
            }();

            MetaValue number = m_value_getter(m_member, 0);
            s64 number_val   = getS64FromMetaValue(number);

            std::string preview_value = selected_index >= 0 ? enum_values.at(selected_index).first
                                                            : "[[Invalid State]]";
            if (ImGui::BeginCombo("##enum_combobox", preview_value.c_str())) {
                for (int i = 0; i < enum_values.size(); ++i) {
                    std::string enum_str = enum_values.at(i).first;
                    bool selected        = i == selected_index;
                    if (ImGui::Selectable(enum_str.c_str(), &selected)) {
                        std::fill(checked_state.begin(), checked_state.end(), false);
                        number_val       = getEnumFlagValue(enum_values.at(i), enum_type);
                        checked_state[i] = true;
                        any_changed      = true;

                        if (m_value_setter) {
                            MetaValue new_value = MetaValue(getMetaType(m_member).value());
                            setS64ToMetaValue(new_value, number_val);
                            any_changed |= m_value_setter(m_member, 0, new_value);
                        }
                    }
                }
                ImGui::EndCombo();
            }

            return any_changed;
        }
    }

    StructProperty::StructProperty(RefPtr<Object::MetaMember> prop, getter_cb getter,
                                   setter_cb setter)
        : AbstractBaseProperty(prop, getter, setter), m_open(true) {
        // prop->syncArray();
        // m_children_ary.resize(prop->arraysize());
        // m_array_open.resize(m_children_ary.size());
        // for (size_t i = 0; i < m_children_ary.size(); ++i) {
        //     auto struct_ = prop->value<Object::MetaStruct>(i).value();
        //     auto members = struct_->members();
        //     for (size_t j = 0; j < members.size(); ++j) {
        //         m_children_ary.at(i).push_back(createProperty(members.at(j), getter, setter));
        //     }
        // }
    }

    void StructProperty::update_(const u32 array_size) {
        m_children_ary.resize(m_member->arraysize());

        for (size_t i = 0; i < m_children_ary.size(); ++i) {
            RefPtr<MetaStruct> struct_ = m_member->value<Object::MetaStruct>(i).value();
            const std::vector<RefPtr<MetaMember>> &members = struct_->members();
            for (size_t j = 0; j < members.size(); ++j) {
                ScopePtr<IProperty> property =
                    createProperty(members.at(j), m_value_getter, m_value_setter);
                property->update();
                m_children_ary.at(i).push_back(std::move(property));
            }
        }
    }

    bool StructProperty::render_(float label_width) {
        ImGuiStyle &style = ImGui::GetStyle();

        if (m_member->isTypeEnum()) {
            TOOLBOX_ERROR_V("[SCENE_PROPERTY] Trying to render struct \"{}\" as enum",
                            m_member->qualifiedName().toString());
        }

        if (!m_member->isTypeStruct()) {
            TOOLBOX_ERROR_V("[SCENE_PROPERTY] Trying to render value \"{}\" as struct",
                            m_member->qualifiedName().toString());
        }

        // Don't bother rendering when empty
        if (m_member->isEmpty()) {
            return false;
        }

        bool is_open     = getSelfOpen();
        bool any_changed = false;

        const ImVec2 window_size  = ImGui::GetWindowSize();
        const bool collapse_lines = window_size.x < 350;

        ImGui::SetNextItemOpen(is_open, ImGuiCond_Always);

        const bool header_open = ImGui::CollapsingHeader(m_member->name().c_str());

        if (ImGui::IsItemToggledOpen()) {
            setSelfOpen(!is_open);
        }

        if (header_open) {
            if (m_member->isArray()) {
                float label_width    = 0;
                const u32 array_size = m_children_ary.at(0).size();
                for (size_t i = 0; i < array_size; ++i) {
                    label_width = std::max(label_width, m_children_ary.at(0).at(i)->labelSize().x);
                }
                for (size_t i = 0; i < m_children_ary.size(); ++i) {
                    RefPtr<MetaStruct> struct_ = m_member->value<Object::MetaStruct>(i).value();
                    const std::vector<RefPtr<MetaMember>> &members = struct_->members();

                    const std::string array_name = std::format("[{}]##{}", i, m_member->name().c_str());
                    
                    bool array_open        = getArrayIdxOpen(i);
                    if (ImGui::BeginGroupPanel(array_name.c_str(),
                                               &array_open,
                                               {})) {
                        for (size_t j = 0; j < members.size(); ++j) {
                            ImGui::PushID(static_cast<int>(i << 8 | j));
                            any_changed |= m_children_ary.at(i).at(j)->render(label_width);
                            ImGui::PopID();
                            ImGui::ItemSize({0, 2});
                        }
                    }
                    ImGui::EndGroupPanel();

                    setArrayIdxOpen(i, array_open);
                }
            } else {
                float label_width    = 0;
                const u32 array_size = m_children_ary.at(0).size();
                for (size_t i = 0; i < array_size; ++i) {
                    label_width = std::max(label_width, m_children_ary.at(0).at(i)->labelSize().x);
                }

                RefPtr<MetaStruct> struct_ = m_member->value<Object::MetaStruct>(0).value();
                const std::vector<RefPtr<MetaMember>> &members = struct_->members();
                for (size_t j = 0; j < members.size(); ++j) {
                    any_changed |= m_children_ary.at(0).at(j)->render(label_width);
                    ImGui::ItemSize({0, 2});
                }
            }
        }
        ImGui::ItemSize({0, 4});

        return any_changed;
    }

    ScopePtr<IProperty> createProperty(RefPtr<Object::MetaMember> m_member,
                                       IProperty::getter_cb getter, IProperty::setter_cb setter) {
        auto meta_type = Object::getMetaType(m_member);
        if (m_member->isTypeStruct()) {
            return make_scoped<StructProperty>(m_member, getter, setter);
        } else if (m_member->isTypeEnum()) {
            return make_scoped<EnumProperty>(m_member, getter, setter);
        } else if (meta_type == Object::MetaType::STRING) {
            return make_scoped<StringProperty>(m_member, getter, setter);
        } else if (meta_type == Object::MetaType::RGB || meta_type == Object::MetaType::RGBA ||
                   meta_type == Object::MetaType::RGB32 || meta_type == Object::MetaType::RGBA32) {
            return make_scoped<ColorProperty>(m_member, getter, setter);
        } else if (meta_type == Object::MetaType::VEC3) {
            return make_scoped<VectorProperty>(m_member, getter, setter);
        } else if (meta_type == Object::MetaType::TRANSFORM) {
            return make_scoped<TransformProperty>(m_member, getter, setter);
        } else if (meta_type == Object::MetaType::F32) {
            return make_scoped<FloatProperty>(m_member, getter, setter);
        } else if (meta_type == Object::MetaType::F64) {
            return make_scoped<DoubleProperty>(m_member, getter, setter);
        } else if (meta_type == Object::MetaType::BOOL) {
            return make_scoped<BoolProperty>(m_member, getter, setter);
        } else {
            return make_scoped<NumberProperty>(m_member, getter, setter);
        }
    }

    void AbstractBaseProperty::update() {
        m_member->syncArray();

        const u32 array_size = m_member->arraysize();
        if (m_last_member_size == array_size) {
            return;
        }
        m_last_member_size = array_size;

        if (m_array_open.size() < array_size) {
            m_array_open.resize(array_size, true);
        }

        update_(array_size);
    }

    bool AbstractBaseProperty::render(float label_width) {
        update();
        return render_(label_width);
    }

    static std::unordered_map<QualifiedName, bool> s_qualname_to_open_map = {};

    bool AbstractBaseProperty::getSelfOpen() const {
        auto it = s_qualname_to_open_map.find(m_member->qualifiedName());
        if (it == s_qualname_to_open_map.end()) {
            return true;  // Open by default
        }
        return it->second;
    }

    void AbstractBaseProperty::setSelfOpen(bool open) {
        s_qualname_to_open_map[m_member->qualifiedName()] = open;
    }

    bool AbstractBaseProperty::getArrayIdxOpen(size_t idx) const {
        if (idx >= m_array_open.size()) {
            return false;
        }
        return m_array_open[idx];
    }

    void AbstractBaseProperty::setArrayIdxOpen(size_t idx, bool open) {
        if (idx >= m_array_open.size()) {
            return;
        }
        m_array_open[idx] = open;
    }

    ImVec2 AbstractBaseProperty::labelSize() const {
        std::string_view label = m_member->name();
        if (label.empty()) {
            return m_label_size;
        }

        if (m_label_size.x == 0.0f && m_label_size.y == 0.0f) {
            ImFont *font      = ImGui::GetFont();
            ImGuiStyle &style = ImGui::GetStyle();

            m_label_size =
                font->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, 0.0f, m_member->name().c_str());
        }

        return m_label_size;
    }

}  // namespace Toolbox::UI
