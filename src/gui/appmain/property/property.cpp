#include "gui/appmain/property/property.hpp"
#include "core/log.hpp"
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
    void BoolProperty::init() {
        m_member->syncArray();
        m_bools.resize(m_member->arraysize());
        m_array_open.resize(m_member->arraysize(), true);
        for (size_t i = 0; i < m_bools.size(); ++i) {
            m_bools.at(i) = Object::getMetaValue<bool>(m_member, i).value();
        }
    }

    bool BoolProperty::render(float label_width) {
        ImGuiStyle &style = ImGui::GetStyle();

        if (m_member->isTypeStruct()) {
            TOOLBOX_ERROR_V("[SCENE_PROPERTY] Trying to render struct \"{}\" as bool",
                            m_member->qualifiedName().toString());
        }

        if (m_member->isTypeEnum()) {
            TOOLBOX_ERROR_V("[SCENE_PROPERTY] Trying to render enum \"{}\" as bool",
                            m_member->qualifiedName().toString());
        }

        if (m_bools.size() != m_member->arraysize()) {
            init();
        }

        bool any_changed = false;

        const bool is_array = m_member->isArray();

        ImVec2 window_size        = ImGui::GetWindowSize();
        const bool collapse_lines = window_size.x < 350;

        if (is_array || m_member->isEmpty()) {
            if (ImGui::BeginGroupPanel(m_member->name().c_str(), &m_open, {})) {
                for (size_t i = 0; i < m_bools.size(); ++i) {
                    std::string id_str = std::format("##{}-{}", m_member->name().c_str(), i);
                    std::string name   = std::format("[{}]", i);
                    ImGui::Text(name.c_str());
                    ImGui::SameLine();
                    if (ImGui::Checkbox(id_str.c_str(),
                                        reinterpret_cast<bool *>(m_bools.data() + i))) {
                        // if (Object::setMetaValue<bool>(m_member, i, m_bools[i])) {
                        //     any_changed = true;
                        // }
                        if (m_value_setter) {
                            MetaValue new_value = MetaValue(static_cast<bool>(m_bools[i]));
                            any_changed |= m_value_setter(m_member, i, new_value);
                        }
                    }
                }
            }
            ImGui::EndGroupPanel();

            return any_changed;
        }

        ImGui::Text(m_member->name().c_str());
        if (!collapse_lines) {
            ImGui::SameLine();
            ImGui::Dummy({label_width - ImGui::CalcTextSize(m_member->name().c_str()).x, 0});
            ImGui::SameLine();
        }

        std::string id_str = std::format("##{}", m_member->name().c_str());
        if (ImGui::Checkbox(id_str.c_str(), reinterpret_cast<bool *>(m_bools.data()))) {
            if (m_value_setter) {
                MetaValue new_value = MetaValue(static_cast<bool>(m_bools[0]));
                any_changed |= m_value_setter(m_member, 0, new_value);
            }
        }

        return any_changed;
    }

    void NumberProperty::init() {
        m_member->syncArray();

        const u32 member_array_size = m_member->arraysize();
        m_array_open.resize(member_array_size, true);

        if (member_array_size == 0) {
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
                "[SCENE_PROPERTY] NumberProperty initialized with unsupported type \"{}\" for "
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

    bool NumberProperty::render(float label_width) {
        ImGuiStyle &style = ImGui::GetStyle();

        if (m_member->isTypeStruct()) {
            TOOLBOX_ERROR_V("[SCENE_PROPERTY] Trying to render struct \"{}\" as number",
                            m_member->qualifiedName().toString());
        }

        if (m_member->isTypeEnum()) {
            TOOLBOX_ERROR_V("[SCENE_PROPERTY] Trying to render enum \"{}\" as number",
                            m_member->qualifiedName().toString());
        }

        if (!m_initialized) {
            init();
            m_initialized = true;
        }

        bool any_changed = false;

        const bool is_array = m_member->isArray();

        ImVec2 window_size        = ImGui::GetWindowSize();
        const bool collapse_lines = window_size.x < 350;

        if (is_array || m_member->isEmpty()) {
            if (ImGui::BeginGroupPanel(m_member->name().c_str(), &m_open, {})) {
                const u32 array_size = m_member->arraysize();
                for (size_t i = 0; i < array_size; ++i) {
                    const std::string id_str = std::format("##{}-{}", m_member->name().c_str(), i);
                    const std::string name   = std::format("[{}]", i);

                    ImGui::Text(name.c_str());

                    ImGui::SameLine();

                    MetaValue number = m_value_getter(m_member, i);
                    s64 number_val   = getS64FromMetaValue(number);

                    if (ImGui::InputScalarCompact(
                            id_str.c_str(), ImGuiDataType_S64, &number_val, &m_step,
                            &m_step_fast, nullptr,
                            ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_CharsNoBlank)) {
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
        case MetaType::U8:
            value.set<u8>(static_cast<u8>(number));
        case MetaType::S16:
            value.set<s16>(static_cast<s16>(number));
        case MetaType::U16:
            value.set<u16>(static_cast<u16>(number));
        case MetaType::S32:
            value.set<s32>(static_cast<s32>(number));
        case MetaType::U32:
            value.set<u32>(static_cast<u32>(number));
        default:
            return;
        }
    }

    void FloatProperty::init() {
        m_member->syncArray();

        u32 member_array_size = m_member->arraysize();

        m_numbers.resize(member_array_size);
        m_array_open.resize(member_array_size, true);

        if (member_array_size == 0) {
            return;
        }

        for (size_t i = 0; i < m_numbers.size(); ++i) {
            m_numbers.at(i) = Object::getMetaValue<f32>(m_member, i).value();
        }

        m_min = Object::getMetaValueMin<f32>(m_member, 0).value_or(std::numeric_limits<f32>::min());
        m_max = Object::getMetaValueMax<f32>(m_member, 0).value_or(std::numeric_limits<f32>::max());
    }

    bool FloatProperty::render(float label_width) {
        ImGuiStyle &style = ImGui::GetStyle();

        if (m_member->isTypeStruct()) {
            TOOLBOX_ERROR_V("[SCENE_PROPERTY] Trying to render struct \"{}\" as float",
                            m_member->qualifiedName().toString());
        }

        if (m_member->isTypeEnum()) {
            TOOLBOX_ERROR_V("[SCENE_PROPERTY] Trying to render enum \"{}\" as float",
                            m_member->qualifiedName().toString());
        }

        if (m_numbers.size() != m_member->arraysize()) {
            init();
        }

        bool any_changed = false;

        const bool is_array = m_member->isArray();

        ImVec2 window_size        = ImGui::GetWindowSize();
        const bool collapse_lines = window_size.x < 350;

        if (is_array || m_member->isEmpty()) {
            if (ImGui::BeginGroupPanel(m_member->name().c_str(), &m_open, {})) {
                for (size_t i = 0; i < m_numbers.size(); ++i) {
                    std::string id_str = std::format("##{}-{}", m_member->name().c_str(), i);
                    std::string name   = std::format("[{}]", i);
                    ImGui::Text(name.c_str());
                    ImGui::SameLine();
                    if (ImGui::InputScalarCompact(
                            id_str.c_str(), ImGuiDataType_Float, m_numbers.data() + i, &m_step,
                            &m_step_fast, nullptr,
                            ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_CharsNoBlank)) {
                        f32 &number = m_numbers.at(i);
                        if (number > m_max) {
                            number = m_min + (number - m_max);
                        } else if (number < m_min) {
                            number = m_max + (number - m_min);
                        }
                        if (m_value_setter) {
                            MetaValue new_value = MetaValue(getMetaType(m_member).value());
                            new_value.setVariant(number);
                            any_changed |= m_value_setter(m_member, i, new_value);
                        }
                    }
                }
            }
            ImGui::EndGroupPanel();

            return any_changed;
        }

        ImGui::Text(m_member->name().c_str());

        if (!collapse_lines) {
            ImGui::SameLine();
            ImGui::Dummy({label_width - ImGui::CalcTextSize(m_member->name().c_str()).x, 0});
            ImGui::SameLine();
        }

        std::string label = std::format("##{}", m_member->name().c_str());
        if (ImGui::InputScalarCompact(
                label.c_str(), ImGuiDataType_Float, m_numbers.data(), &m_step, &m_step_fast,
                nullptr, ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_CharsNoBlank)) {
            f32 &number = m_numbers.at(0);
            number      = std::clamp(number, m_min, m_max);
            if (m_value_setter) {
                MetaValue new_value = MetaValue(getMetaType(m_member).value());
                new_value.setVariant(number);
                any_changed |= m_value_setter(m_member, 0, new_value);
            }
        }

        return any_changed;
    }

    void DoubleProperty::init() {
        m_member->syncArray();

        u32 member_array_size = m_member->arraysize();

        m_numbers.resize(member_array_size);
        m_array_open.resize(member_array_size, true);

        if (member_array_size == 0) {
            return;
        }

        RefPtr<MetaValue> value = m_member->value<MetaValue>(0).value();

        for (size_t i = 0; i < m_numbers.size(); ++i) {
            m_numbers.at(i) = Object::getMetaValue<f64>(m_member, i).value();
        }

        m_min = Object::getMetaValueMin<f64>(m_member, 0).value_or(std::numeric_limits<f64>::min());
        m_max = Object::getMetaValueMax<f64>(m_member, 0).value_or(std::numeric_limits<f64>::max());
    }

    bool DoubleProperty::render(float label_width) {
        ImGuiStyle &style = ImGui::GetStyle();

        if (m_member->isTypeStruct()) {
            TOOLBOX_ERROR_V("[SCENE_PROPERTY] Trying to render struct \"{}\" as double",
                            m_member->qualifiedName().toString());
        }

        if (m_member->isTypeEnum()) {
            TOOLBOX_ERROR_V("[SCENE_PROPERTY] Trying to render enum \"{}\" as double",
                            m_member->qualifiedName().toString());
        }

        if (m_numbers.size() != m_member->arraysize()) {
            init();
        }

        bool any_changed = false;

        const bool is_array = m_member->isArray();

        ImVec2 window_size        = ImGui::GetWindowSize();
        const bool collapse_lines = window_size.x < 350;

        if (is_array || m_member->isEmpty()) {
            if (ImGui::BeginGroupPanel(m_member->name().c_str(), &m_open, {})) {
                for (size_t i = 0; i < m_numbers.size(); ++i) {
                    std::string id_str = std::format("##{}-{}", m_member->name().c_str(), i);
                    std::string name   = std::format("[{}]", i);
                    ImGui::Text(name.c_str());
                    ImGui::SameLine();
                    if (ImGui::InputScalarCompact(
                            id_str.c_str(), ImGuiDataType_Double, m_numbers.data() + i, &m_step,
                            &m_step_fast, nullptr,
                            ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_CharsNoBlank)) {
                        f64 &number = m_numbers.at(i);
                        if (number > m_max) {
                            number = m_min + (number - m_max);
                        } else if (number < m_min) {
                            number = m_max + (number - m_min);
                        }
                        if (m_value_setter) {
                            MetaValue new_value = MetaValue(getMetaType(m_member).value());
                            new_value.setVariant(number);
                            any_changed |= m_value_setter(m_member, i, new_value);
                        }
                    }
                }
            }
            ImGui::EndGroupPanel();

            return any_changed;
        }

        ImGui::Text(m_member->name().c_str());

        if (!collapse_lines) {
            ImGui::SameLine();
            ImGui::Dummy({label_width - ImGui::CalcTextSize(m_member->name().c_str()).x, 0});
            ImGui::SameLine();
        }

        std::string label = std::format("##{}", m_member->name().c_str());
        if (ImGui::InputScalarCompact(
                label.c_str(), ImGuiDataType_Double, m_numbers.data(), &m_step, &m_step_fast,
                nullptr, ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_CharsNoBlank)) {
            f64 &number = m_numbers.at(0);
            number      = std::clamp(number, m_min, m_max);
            if (m_value_setter) {
                MetaValue new_value = MetaValue(getMetaType(m_member).value());
                new_value.setVariant(number);
                any_changed |= m_value_setter(m_member, 0, new_value);
            }
        }

        return any_changed;
    }

    void StringProperty::init() {
        m_member->syncArray();
        m_strings.resize(m_member->arraysize());
        m_array_open.resize(m_member->arraysize(), true);

        for (size_t i = 0; i < m_strings.size(); ++i) {
            std::string str = Object::getMetaValue<std::string>(m_member, i).value();
            size_t size     = std::min(str.size(), size_t(128));
            std::copy(str.begin(), str.begin() + size, m_strings.at(i).begin());
        }
    }

    bool StringProperty::render(float label_width) {
        ImGuiStyle &style = ImGui::GetStyle();

        if (m_member->isTypeStruct()) {
            TOOLBOX_ERROR_V("[SCENE_PROPERTY] Trying to render struct \"{}\" as string",
                            m_member->qualifiedName().toString());
        }

        if (m_member->isTypeEnum()) {
            TOOLBOX_ERROR_V("[SCENE_PROPERTY] Trying to render enum \"{}\" as string",
                            m_member->qualifiedName().toString());
        }

        bool any_changed = false;

        const bool is_array = m_member->isArray();

        ImVec2 window_size        = ImGui::GetWindowSize();
        const bool collapse_lines = window_size.x < 350;

        if (m_strings.size() != m_member->arraysize()) {
            init();
        }

        if (is_array || m_member->isEmpty()) {
            if (ImGui::BeginGroupPanel(m_member->name().c_str(), &m_open, {})) {
                for (size_t i = 0; i < m_strings.size(); ++i) {
                    auto &str_data     = m_strings.at(i);
                    std::string id_str = std::format("##{}-{}", m_member->name().c_str(), i);
                    std::string name   = std::format("[{}]", i);
                    ImGui::Text(name.c_str());
                    ImGui::SameLine();
                    if (ImGui::InputText(id_str.c_str(), str_data.data(), str_data.size())) {
                        if (Object::setMetaValue(m_member, i, convertArrayToStringView(str_data))) {
                            any_changed = true;
                        }
                        if (m_value_setter) {
                            MetaValue new_value = MetaValue(getMetaType(m_member).value());
                            std::string str_opa = std::string(convertArrayToStringView(str_data));
                            new_value.setVariant(str_opa);
                            any_changed |= m_value_setter(m_member, i, new_value);
                        }
                    }
                }
            }
            ImGui::EndGroupPanel();

            return any_changed;
        }

        ImGui::Text(m_member->name().c_str());

        if (!collapse_lines) {
            ImGui::SameLine();
            ImGui::Dummy({label_width - ImGui::CalcTextSize(m_member->name().c_str()).x, 0});
            ImGui::SameLine();
        }

        std::string label = std::format("##{}", m_member->name().c_str());
        auto &str_data    = m_strings.at(0);
        if (ImGui::InputText(label.c_str(), str_data.data(), str_data.size())) {
            if (m_value_setter) {
                MetaValue new_value = MetaValue(getMetaType(m_member).value());
                std::string str_opa = std::string(convertArrayToStringView(str_data));
                new_value.setVariant(str_opa);
                any_changed |= m_value_setter(m_member, 0, new_value);
            }
        }

        return any_changed;
    }

    void ColorProperty::init() {
        m_member->syncArray();
        m_colors.resize(m_member->arraysize());
        m_array_open.resize(m_member->arraysize(), true);

        const bool isRGB    = m_member->isTypeRGB();
        const bool isRGBA   = m_member->isTypeRGBA();
        const bool isRGB32  = m_member->isTypeRGB32();
        const bool isRGBA32 = m_member->isTypeRGBA32();

        for (size_t i = 0; i < m_colors.size(); ++i) {
            m_colors.at(i) = getColor(i);
        }
    }

    bool ColorProperty::render(float label_width) {
        ImGuiStyle &style = ImGui::GetStyle();

        if (m_member->isTypeStruct()) {
            TOOLBOX_ERROR_V("[SCENE_PROPERTY] Trying to render struct \"{}\" as color",
                            m_member->qualifiedName().toString());
        }

        if (m_member->isTypeEnum()) {
            TOOLBOX_ERROR_V("[SCENE_PROPERTY] Trying to render enum \"{}\" as color",
                            m_member->qualifiedName().toString());
        }

        const bool is_array = m_member->isArray();

        const bool isRGB    = m_member->isTypeRGB();
        const bool isRGBA   = m_member->isTypeRGBA();
        const bool isRGB32  = m_member->isTypeRGB32();
        const bool isRGBA32 = m_member->isTypeRGBA32();

        const bool use_alpha = isRGBA || isRGBA32;

        bool any_changed = false;

        ImVec2 window_size        = ImGui::GetWindowSize();
        const bool collapse_lines = window_size.x < 350;

        if (m_colors.size() != m_member->arraysize()) {
            init();
        }

        if (is_array || m_member->isEmpty()) {
            if (ImGui::BeginGroupPanel(m_member->name().c_str(), &m_open, {})) {
                for (size_t i = 0; i < m_colors.size(); ++i) {
                    Color::RGBAShader &color = m_colors.at(i);

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

            return any_changed;
        }

        ImGui::Text(m_member->name().c_str());

        if (!collapse_lines) {
            ImGui::SameLine();
            ImGui::Dummy({label_width - ImGui::CalcTextSize(m_member->name().c_str()).x, 0});
            ImGui::SameLine();
        }

        Color::RGBAShader &color = m_colors.at(0);

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
                Object::getMetaValue<Color::RGB8>(m_member, array_index).value_or(Color::RGB8());
            f32 r, g, b, a;
            tmp_color.getColor(r, g, b, a);
            out_color.setColor(r, g, b, a);
        }

        if (isRGBA) {
            Color::RGBA8 tmp_color =
                Object::getMetaValue<Color::RGBA8>(m_member, array_index).value_or(Color::RGBA8());
            f32 r, g, b, a;
            tmp_color.getColor(r, g, b, a);
            out_color.setColor(r, g, b, a);
        }

        if (isRGB32) {
            Color::RGB32 tmp_color =
                Object::getMetaValue<Color::RGB32>(m_member, array_index).value_or(Color::RGB32());
            f32 r, g, b, a;
            tmp_color.getColor(r, g, b, a);
            out_color.setColor(r, g, b, a);
        }

        if (isRGBA32) {
            Color::RGBA32 tmp_color = Object::getMetaValue<Color::RGBA32>(m_member, array_index)
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

    void VectorProperty::init() {
        m_member->syncArray();
        m_vectors.resize(m_member->arraysize());
        m_array_open.resize(m_member->arraysize(), true);

        switch (Object::getMetaType(m_member).value()) {
        default:
        case Object::MetaType::VEC3:
            for (size_t i = 0; i < m_vectors.size(); ++i) {
                m_vectors.at(i) = Object::getMetaValue<glm::vec3>(m_member, i).value();
            }
            m_min = -FLT_MAX;
            m_max = FLT_MAX;
            break;
        }
    }

    bool VectorProperty::render(float label_width) {
        ImGuiStyle &style = ImGui::GetStyle();

        if (m_member->isTypeStruct()) {
            TOOLBOX_ERROR_V("[SCENE_PROPERTY] Trying to render struct \"{}\" as vector",
                            m_member->qualifiedName().toString());
        }

        if (m_member->isTypeEnum()) {
            TOOLBOX_ERROR_V("[SCENE_PROPERTY] Trying to render enum \"{}\" as vector",
                            m_member->qualifiedName().toString());
        }

        if (m_vectors.size() != m_member->arraysize()) {
            init();
        }

        bool any_changed = false;

        ImVec2 window_size        = ImGui::GetWindowSize();
        const bool collapse_lines = window_size.x < 350;

        if (ImGui::CollapsingHeader(m_member->name().c_str())) {
            if (m_vectors.size() > 1) {
                for (size_t i = 0; i < m_vectors.size(); ++i) {
                    auto struct_ = m_member->value<Object::MetaStruct>(i).value();
                    auto members = struct_->members();

                    std::string array_name = std::format("[{}]##{}", i, m_member->name().c_str());
                    if (ImGui::BeginGroupPanel(array_name.c_str(),
                                               reinterpret_cast<bool *>(m_array_open.data() + i),
                                               {})) {
                        ImGui::Text(m_member->name().c_str());
                        if (!collapse_lines) {
                            ImGui::SameLine();
                            ImGui::Dummy(
                                {label_width - ImGui::CalcTextSize(m_member->name().c_str()).x, 0});
                            ImGui::SameLine();
                        }
                        if (ImGui::InputScalarCompactN("##vector", ImGuiDataType_Float,
                                                       reinterpret_cast<f32 *>(&m_vectors.at(i)), 3,
                                                       &m_step, &m_step_fast, "%.3f")) {
                            if (m_value_setter) {
                                MetaValue new_value = MetaValue(getMetaType(m_member).value());
                                new_value.setVariant(m_vectors.at(i));
                                any_changed |= m_value_setter(m_member, i, new_value);
                            }
                        }
                        ImGui::Spacing();
                    }
                    ImGui::EndGroupPanel();
                }
            } else if (m_vectors.size() == 1) {
                ImGui::Text(m_member->name().c_str());
                if (!collapse_lines) {
                    ImGui::SameLine();
                    ImGui::Dummy(
                        {label_width - ImGui::CalcTextSize(m_member->name().c_str()).x, 0});
                    ImGui::SameLine();
                }
                if (ImGui::InputScalarCompactN("##vector", ImGuiDataType_Float,
                                               reinterpret_cast<f32 *>(&m_vectors.at(0)), 3,
                                               &m_step, &m_step_fast, "%.3f")) {
                    if (m_value_setter) {
                        MetaValue new_value = MetaValue(getMetaType(m_member).value());
                        new_value.setVariant(m_vectors.at(0));
                        any_changed |= m_value_setter(m_member, 0, new_value);
                    }
                }
                ImGui::Spacing();
            }
        }
        ImGui::ItemSize({0, 4});

        return any_changed;
    }

    void TransformProperty::init() {
        m_member->syncArray();
        m_transforms.resize(m_member->arraysize());
        m_array_open.resize(m_member->arraysize());

        switch (Object::getMetaType(m_member).value()) {
        default:
        case Object::MetaType::TRANSFORM:
            for (size_t i = 0; i < m_transforms.size(); ++i) {
                m_transforms.at(i) = Object::getMetaValue<Object::Transform>(m_member, i).value();
                m_array_open.at(i) = i == 0;
            }
            m_min = -FLT_MAX;
            m_max = FLT_MAX;
            break;
        }
    }

    bool TransformProperty::render(float label_width) {
        ImGuiStyle &style = ImGui::GetStyle();

        if (m_member->isTypeStruct()) {
            TOOLBOX_ERROR_V("[SCENE_PROPERTY] Trying to render struct \"{}\" as transform",
                            m_member->qualifiedName().toString());
        }

        if (m_member->isTypeEnum()) {
            TOOLBOX_ERROR_V("[SCENE_PROPERTY] Trying to render enum \"{}\" as transform",
                            m_member->qualifiedName().toString());
        }

        if (m_transforms.size() != m_member->arraysize()) {
            init();
        }

        bool any_changed = false;

        ImVec2 window_size        = ImGui::GetWindowSize();
        const bool collapse_lines = window_size.x < 350;

        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        if (ImGui::CollapsingHeader(m_member->name().c_str())) {
            if (m_transforms.size() > 1) {
                float label_width = 0;
                for (size_t i = 0; i < m_transforms.size(); ++i) {
                    auto struct_ = m_member->value<Object::MetaStruct>(i).value();
                    auto members = struct_->members();

                    std::string array_name = std::format("[{}]##{}", i, m_member->name().c_str());
                    if (ImGui::BeginGroupPanel(array_name.c_str(),
                                               reinterpret_cast<bool *>(m_array_open.data() + i),
                                               {})) {
                        ImGui::PushID("Translation");
                        ImGui::Text("Translation");
                        if (!collapse_lines) {
                            ImGui::SameLine();
                            ImGui::Dummy({label_width - ImGui::CalcTextSize("Translation").x, 0});
                            ImGui::SameLine();
                        }
                        if (ImGui::InputScalarCompactN(
                                "##Translation", ImGuiDataType_Float,
                                reinterpret_cast<f32 *>(&m_transforms.at(i).m_translation), 3,
                                &m_step, &m_step_fast, "%.3f")) {
                            if (m_value_setter) {
                                MetaValue new_value = MetaValue(getMetaType(m_member).value());
                                new_value.setVariant(m_transforms.at(i));
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
                                reinterpret_cast<f32 *>(&m_transforms.at(i).m_rotation), 3, &m_step,
                                &m_step_fast, "%.3f")) {
                            if (m_value_setter) {
                                MetaValue new_value = MetaValue(getMetaType(m_member).value());
                                new_value.setVariant(m_transforms.at(i));
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
                                reinterpret_cast<f32 *>(&m_transforms.at(i).m_scale), 3, &m_step,
                                &m_step_fast, "%.3f")) {
                            if (m_value_setter) {
                                MetaValue new_value = MetaValue(getMetaType(m_member).value());
                                new_value.setVariant(m_transforms.at(i));
                                any_changed |= m_value_setter(m_member, i, new_value);
                            }
                        }
                        ImGui::Spacing();
                        ImGui::PopID();
                    }
                    ImGui::EndGroupPanel();
                }
            } else if (m_transforms.size() == 1) {
                ImGui::PushID("Translation");
                ImGui::Text("Translation");
                if (!collapse_lines) {
                    ImGui::SameLine();
                    ImGui::Dummy({label_width - ImGui::CalcTextSize("Translation").x, 0});
                    ImGui::SameLine();
                }
                if (ImGui::InputScalarCompactN(
                        "##Translation", ImGuiDataType_Float,
                        reinterpret_cast<f32 *>(&m_transforms.at(0).m_translation), 3, &m_step,
                        &m_step_fast, "%.3f")) {
                    if (m_value_setter) {
                        MetaValue new_value = MetaValue(getMetaType(m_member).value());
                        new_value.setVariant(m_transforms.at(0));
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
                if (ImGui::InputScalarCompactN(
                        "##Rotation", ImGuiDataType_Float,
                        reinterpret_cast<f32 *>(&m_transforms.at(0).m_rotation), 3, &m_step,
                        &m_step_fast, "%.3f")) {
                    if (m_value_setter) {
                        MetaValue new_value = MetaValue(getMetaType(m_member).value());
                        new_value.setVariant(m_transforms.at(0));
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
                                               reinterpret_cast<f32 *>(&m_transforms.at(0).m_scale),
                                               3, &m_step, &m_step_fast, "%.3f")) {
                    if (m_value_setter) {
                        MetaValue new_value = MetaValue(getMetaType(m_member).value());
                        new_value.setVariant(m_transforms.at(0));
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

    bool EnumProperty::render(float label_width) {
        ImGuiStyle &style = ImGui::GetStyle();

        if (m_member->isTypeStruct()) {
            TOOLBOX_ERROR_V("[SCENE_PROPERTY] Trying to render struct \"{}\" as enum",
                            m_member->qualifiedName().toString());
        }

        if (!m_member->isTypeEnum()) {
            TOOLBOX_ERROR_V("[SCENE_PROPERTY] Trying to render value \"{}\" as enum",
                            m_member->qualifiedName().toString());
        }

        bool any_changed = false;

        auto enum_values    = Object::getMetaEnumValues(m_member).value();
        auto enum_type      = Object::getMetaType(m_member).value();
        bool enum_bitmasked = Object::getMetaEnumBitmasked(m_member).value();

        if (!m_initialized) {
            init();

            const u32 array_size = m_member->arraysize();
            m_checked_state.resize(array_size);

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
            m_initialized = true;
        }

        const bool is_array = m_member->isArray();

        ImVec2 window_size        = ImGui::GetWindowSize();
        const bool collapse_lines = window_size.x < 350;

        if (enum_bitmasked) {
            if (is_array || m_member->isEmpty()) {
                if (ImGui::BeginGroupPanel(m_member->name().c_str(), &m_open, {})) {
                    for (size_t i = 0; i < m_checked_state.size(); ++i) {
                        std::string array_str = std::format("[{}]", i).c_str();
                        if (ImGui::BeginGroupPanel(array_str.c_str(),
                                                   reinterpret_cast<bool *>(&m_array_open.at(i)),
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
                    }
                }
                ImGui::EndGroupPanel();

                return any_changed;
            }

            if (ImGui::BeginGroupPanel(m_member->name().c_str(), &m_open, {})) {
                MetaValue number = m_value_getter(m_member, 0);
                s64 number_val   = getS64FromMetaValue(number);

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
                if (m_value_setter) {
                    MetaValue new_value = MetaValue(getMetaType(m_member).value());
                    setS64ToMetaValue(new_value, number_val);
                    any_changed |= m_value_setter(m_member, 0, new_value);
                }
            }
            ImGui::EndGroupPanel();

            return any_changed;
        } else {
            if (is_array || m_member->isEmpty()) {
                if (ImGui::BeginGroupPanel(m_member->name().c_str(), &m_open, {})) {
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

            MetaValue number          = m_value_getter(m_member, 0);
            s64 number_val            = getS64FromMetaValue(number);

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

    StructProperty::StructProperty(RefPtr<Object::MetaMember> prop, getter_cb getter, setter_cb setter)
        : IProperty(prop, getter, setter), m_open(true) {
        //prop->syncArray();
        //m_children_ary.resize(prop->arraysize());
        //m_array_open.resize(m_children_ary.size());
        //for (size_t i = 0; i < m_children_ary.size(); ++i) {
        //    auto struct_ = prop->value<Object::MetaStruct>(i).value();
        //    auto members = struct_->members();
        //    for (size_t j = 0; j < members.size(); ++j) {
        //        m_children_ary.at(i).push_back(createProperty(members.at(j), getter, setter));
        //    }
        //}
    }

    void StructProperty::init() {
        m_member->syncArray();

        size_t min_end = std::min(m_children_ary.size(), size_t(m_member->arraysize()));
        m_children_ary.resize(m_member->arraysize());
        m_array_open.resize(m_children_ary.size());

        for (size_t i = 0; i < min_end; ++i) {
            auto struct_ = m_member->value<Object::MetaStruct>(i).value();
            auto members = struct_->members();
            for (size_t j = 0; j < m_children_ary.at(i).size(); ++j) {
                m_children_ary.at(i).at(j)->init();
            }
        }

        if (min_end >= m_member->arraysize()) {
            return;
        }

        for (size_t i = min_end; i < m_children_ary.size(); ++i) {
            auto struct_ = m_member->value<Object::MetaStruct>(i).value();
            auto members = struct_->members();
            for (size_t j = 0; j < members.size(); ++j) {
                auto new_prop = createProperty(members.at(j), m_value_getter, m_value_setter);
                new_prop->init();
                m_children_ary.at(i).push_back(std::move(new_prop));
            }
        }
    }

    bool StructProperty::render(float label_width) {
        ImGuiStyle &style = ImGui::GetStyle();

        if (m_member->isTypeEnum()) {
            TOOLBOX_ERROR_V("[SCENE_PROPERTY] Trying to render struct \"{}\" as enum",
                            m_member->qualifiedName().toString());
        }

        if (!m_member->isTypeStruct()) {
            TOOLBOX_ERROR_V("[SCENE_PROPERTY] Trying to render value \"{}\" as struct",
                            m_member->qualifiedName().toString());
        }

        if (m_children_ary.size() != m_member->arraysize()) {
            init();
        }

        bool any_changed = false;

        ImGuiID struct_id = ImGui::GetID(m_member->name().c_str());
        // ImGui::PushID(struct_id);
        if (ImGui::CollapsingHeader(m_member->name().c_str())) {
            if (m_children_ary.size() > 1) {
                float label_width = 0;
                for (size_t i = 0; i < m_children_ary.at(0).size(); ++i) {
                    label_width = std::max(label_width, m_children_ary.at(0).at(i)->labelSize().x);
                }
                for (size_t i = 0; i < m_children_ary.size(); ++i) {
                    auto struct_ = m_member->value<Object::MetaStruct>(i).value();
                    auto members = struct_->members();

                    std::string array_name = std::format("[{}]##{}", i, m_member->name().c_str());
                    if (ImGui::BeginGroupPanel(array_name.c_str(),
                                               reinterpret_cast<bool *>(m_array_open.data() + i),
                                               {})) {
                        for (size_t j = 0; j < members.size(); ++j) {
                            any_changed |= m_children_ary.at(i).at(j)->render(label_width);
                            ImGui::ItemSize({0, 2});
                        }
                    }
                    ImGui::EndGroupPanel();
                }
            } else if (m_children_ary.size() == 1) {
                float label_width = 0;
                for (size_t i = 0; i < m_children_ary.at(0).size(); ++i) {
                    label_width = std::max(label_width, m_children_ary.at(0).at(i)->labelSize().x);
                }
                auto struct_ = m_member->value<Object::MetaStruct>(0).value();
                auto members = struct_->members();
                for (size_t j = 0; j < members.size(); ++j) {
                    any_changed |= m_children_ary.at(0).at(j)->render(label_width);
                    ImGui::ItemSize({0, 2});
                }
            }
        }
        ImGui::ItemSize({0, 4});

        return any_changed;
    }

    ScopePtr<IProperty> createProperty(RefPtr<Object::MetaMember> m_member, IProperty::getter_cb getter, IProperty::setter_cb setter) {
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

    ImVec2 IProperty::labelSize() {
        ImFont *font      = ImGui::GetFont();
        ImGuiStyle &style = ImGui::GetStyle();

        ImVec2 textSize =
            font->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, 0.0f, m_member->name().c_str());
        return textSize;
    }

}  // namespace Toolbox::UI
