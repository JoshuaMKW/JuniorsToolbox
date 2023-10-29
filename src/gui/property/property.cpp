#include "gui/property/property.hpp"
#include "gui/imgui_ext.hpp"
#include "gui/util.hpp"
#include "objlib/meta/enum.hpp"
#include "objlib/meta/member.hpp"
#include "objlib/object.hpp"

#include <imgui.h>

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
        for (size_t i = 0; i < m_bools.size(); ++i) {
            m_bools.at(i) = Object::getMetaValue<bool>(m_member, i).value();
        }
    }

    bool BoolProperty::render(float label_width) {
        ImGuiStyle &style = ImGui::GetStyle();

        if (m_member->isTypeStruct()) {
            std::cout << "Trying to render struct as number" << std::endl;
        }

        if (m_member->isTypeEnum()) {
            std::cout << "Trying to render enum as number" << std::endl;
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
                    any_changed |= ImGui::Checkbox(id_str.c_str(),
                                                   reinterpret_cast<bool *>(m_bools.data() + i));
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
        any_changed |= ImGui::Checkbox(id_str.c_str(), reinterpret_cast<bool *>(m_bools.data()));

        return any_changed;
    }

    void NumberProperty::init() {
        m_member->syncArray();
        m_numbers.resize(m_member->arraysize());

        switch (Object::getMetaType(m_member).value()) {
        case Object::MetaType::S8:
            for (size_t i = 0; i < m_numbers.size(); ++i) {
                m_numbers.at(i) = Object::getMetaValue<s8>(m_member, i).value();
            }
            m_min = -128;
            m_max = 127;
            break;
        case Object::MetaType::U8:
            for (size_t i = 0; i < m_numbers.size(); ++i) {
                m_numbers.at(i) = Object::getMetaValue<u8>(m_member, i).value();
            }
            m_min = 0;
            m_max = 255;
            break;
        case Object::MetaType::S16:
            for (size_t i = 0; i < m_numbers.size(); ++i) {
                m_numbers.at(i) = Object::getMetaValue<s16>(m_member, i).value();
            }
            m_min = -32768;
            m_max = 32767;
            break;
        case Object::MetaType::U16:
            for (size_t i = 0; i < m_numbers.size(); ++i) {
                m_numbers.at(i) = Object::getMetaValue<u16>(m_member, i).value();
            }
            m_min = 0;
            m_max = 65535;
            break;
        case Object::MetaType::S32:
            for (size_t i = 0; i < m_numbers.size(); ++i) {
                m_numbers.at(i) = Object::getMetaValue<s32>(m_member, i).value();
            }
            m_min = -2147483648;
            m_max = 2147483647;
            break;
        case Object::MetaType::U32:
            for (size_t i = 0; i < m_numbers.size(); ++i) {
                m_numbers.at(i) = Object::getMetaValue<u32>(m_member, i).value();
            }
            m_min = 0;
            m_max = 4294967295;
            break;
        default:
            for (size_t i = 0; i < m_numbers.size(); ++i) {
                m_numbers.at(i) = Object::getMetaValue<s32>(m_member, i).value();
            }
            m_min = -2147483648;
            m_max = 2147483647;
            break;
        }
    }

    bool NumberProperty::render(float label_width) {
        ImGuiStyle &style = ImGui::GetStyle();

        if (m_member->isTypeStruct()) {
            std::cout << "Trying to render struct as number" << std::endl;
        }

        if (m_member->isTypeEnum()) {
            std::cout << "Trying to render enum as number" << std::endl;
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
                            id_str.c_str(), ImGuiDataType_S64, m_numbers.data() + i, &m_step,
                            &m_step_fast, nullptr,
                            ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_CharsNoBlank)) {
                        s64 &number = m_numbers.at(i);
                        if (number > m_max) {
                            number = m_min + (number - m_max);
                        } else if (number < m_min) {
                            number = m_max + (number - m_min);
                        }
                        Object::setMetaValue(m_member, i, number);
                        any_changed = true;
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
                label.c_str(), ImGuiDataType_S64, m_numbers.data(), &m_step, &m_step_fast, nullptr,
                ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_CharsNoBlank)) {
            s64 &number = m_numbers.at(0);
            number      = std::clamp(number, m_min, m_max);
            Object::setMetaValue(m_member, 0, number);
            any_changed = true;
        }

        return any_changed;
    }

    void FloatProperty::init() {
        m_member->syncArray();
        m_numbers.resize(m_member->arraysize());

        for (size_t i = 0; i < m_numbers.size(); ++i) {
            m_numbers.at(i) = Object::getMetaValue<f32>(m_member, i).value();
        }
        m_min = -FLT_MAX;
        m_max = FLT_MAX;
    }

    bool FloatProperty::render(float label_width) {
        ImGuiStyle &style = ImGui::GetStyle();

        if (m_member->isTypeStruct()) {
            std::cout << "Trying to render struct as number" << std::endl;
        }

        if (m_member->isTypeEnum()) {
            std::cout << "Trying to render enum as number" << std::endl;
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
                        Object::setMetaValue(m_member, i, number);
                        any_changed = true;
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
            Object::setMetaValue(m_member, 0, number);
            any_changed = true;
        }

        return any_changed;
    }

    void DoubleProperty::init() {
        m_member->syncArray();
        m_numbers.resize(m_member->arraysize());

        for (size_t i = 0; i < m_numbers.size(); ++i) {
            m_numbers.at(i) = Object::getMetaValue<f64>(m_member, i).value();
        }
        m_min = -DBL_MAX;
        m_max = DBL_MAX;
    }

    bool DoubleProperty::render(float label_width) {
        ImGuiStyle &style = ImGui::GetStyle();

        if (m_member->isTypeStruct()) {
            std::cout << "Trying to render struct as number" << std::endl;
        }

        if (m_member->isTypeEnum()) {
            std::cout << "Trying to render enum as number" << std::endl;
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
                        Object::setMetaValue(m_member, i, number);
                        any_changed = true;
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
            Object::setMetaValue(m_member, 0, number);
            any_changed = true;
        }

        return any_changed;
    }

    void StringProperty::init() {
        m_member->syncArray();
        m_strings.resize(m_member->arraysize());

        for (size_t i = 0; i < m_strings.size(); ++i) {
            std::string str = Object::getMetaValue<std::string>(m_member, i).value();
            size_t size     = std::min(str.size(), size_t(128));
            std::copy(str.begin(), str.begin() + size, m_strings.at(i).begin());
        }
    }

    bool StringProperty::render(float label_width) {
        ImGuiStyle &style = ImGui::GetStyle();

        if (m_member->isTypeStruct()) {
            std::cout << "Trying to render struct as number" << std::endl;
        }

        if (m_member->isTypeEnum()) {
            std::cout << "Trying to render enum as number" << std::endl;
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
                        Object::setMetaValue(m_member, i, convertArrayToStringView(str_data));
                        any_changed = true;
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
            Object::setMetaValue(m_member, 0, convertArrayToStringView(str_data));
            any_changed = true;
        }

        return any_changed;
    }

    void ColorProperty::init() {
        m_member->syncArray();
        m_colors.resize(m_member->arraysize());
        for (size_t i = 0; i < m_colors.size(); ++i) {
            auto color = Object::getMetaValue<Color::RGBA32>(m_member, i).value();
            f32 r, g, b, a;
            color.getColor(r, g, b, a);
            m_colors.at(i) = Color::RGBAShader(r, g, b, a);
        }
    }

    bool ColorProperty::render(float label_width) {
        ImGuiStyle &style = ImGui::GetStyle();

        if (m_member->isTypeStruct()) {
            std::cout << "Trying to render struct as number" << std::endl;
        }

        if (m_member->isTypeEnum()) {
            std::cout << "Trying to render enum as number" << std::endl;
        }

        bool any_changed = false;

        const bool use_alpha = Object::getMetaType(m_member).value() == Object::MetaType::RGBA;

        const bool is_array = m_member->isArray();

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
                            Object::setMetaValue<Color::RGBA32>(
                                m_member, i,
                                Color::RGBA32(color.m_r, color.m_g, color.m_b, color.m_a));
                            any_changed = true;
                        }
                    } else {
                        if (ImGui::ColorEdit3(name.c_str(), &color.m_r)) {
                            Object::setMetaValue<Color::RGB24>(
                                m_member, i, Color::RGB24(color.m_r, color.m_g, color.m_b));
                            any_changed = true;
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
                Object::setMetaValue<Color::RGBA32>(
                    m_member, 0, Color::RGBA32(color.m_r, color.m_g, color.m_b, color.m_a));
                any_changed = true;
            }
        } else {
            if (ImGui::ColorEdit3(m_member->name().c_str(), &color.m_r)) {
                Object::setMetaValue<Color::RGB24>(m_member, 0,
                                                   Color::RGB24(color.m_r, color.m_g, color.m_b));
                any_changed = true;
            }
        }

        return any_changed;
    }

    // TODO: Implement these!
    void VectorProperty::init() {
        m_member->syncArray();
        m_vectors.resize(m_member->arraysize());

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
            std::cout << "Trying to render struct as enum" << std::endl;
        }

        if (!m_member->isTypeEnum()) {
            std::cout << "Trying to render number as enum" << std::endl;
        }

        if (m_vectors.size() != m_member->arraysize()) {
            init();
        }

        bool any_changed = false;

        ImVec2 window_size        = ImGui::GetWindowSize();
        const bool collapse_lines = window_size.x < 350;

        if (ImGui::CollapsingHeader(m_member->name().c_str())) {
            if (m_vectors.size() > 1) {
                float label_width = 0;
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
                            Object::setMetaValue(m_member, 0, m_vectors.at(i),
                                                 Object::MetaType::VEC3);
                            any_changed = true;
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
                    Object::setMetaValue(m_member, 0, m_vectors.at(0), Object::MetaType::VEC3);
                    any_changed = true;
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

        switch (Object::getMetaType(m_member).value()) {
        default:
        case Object::MetaType::TRANSFORM:
            for (size_t i = 0; i < m_transforms.size(); ++i) {
                m_transforms.at(i) = Object::getMetaValue<Object::Transform>(m_member, i).value();
            }
            m_min = -FLT_MAX;
            m_max = FLT_MAX;
            break;
        }
    }

    bool TransformProperty::render(float label_width) {
        ImGuiStyle &style = ImGui::GetStyle();

        if (m_member->isTypeStruct()) {
            std::cout << "Trying to render struct as enum" << std::endl;
        }

        if (!m_member->isTypeEnum()) {
            std::cout << "Trying to render number as enum" << std::endl;
        }

        if (m_transforms.size() != m_member->arraysize()) {
            init();
        }

        bool any_changed = false;

        ImVec2 window_size        = ImGui::GetWindowSize();
        const bool collapse_lines = window_size.x < 350;

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
                            Object::setMetaValue(m_member, 0, m_transforms.at(i),
                                                 Object::MetaType::TRANSFORM);
                            any_changed = true;
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
                            Object::setMetaValue(m_member, 0, m_transforms.at(i),
                                                 Object::MetaType::TRANSFORM);
                            any_changed = true;
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
                            Object::setMetaValue(m_member, 0, m_transforms.at(i),
                                                 Object::MetaType::TRANSFORM);
                            any_changed = true;
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
                    Object::setMetaValue(m_member, 0, m_transforms.at(0),
                                         Object::MetaType::TRANSFORM);
                    any_changed = true;
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
                    Object::setMetaValue(m_member, 0, m_transforms.at(0),
                                         Object::MetaType::TRANSFORM);
                    any_changed = true;
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
                    Object::setMetaValue(m_member, 0, m_transforms.at(0),
                                         Object::MetaType::TRANSFORM);
                    any_changed = true;
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
            std::cout << "Trying to render struct as enum" << std::endl;
        }

        if (!m_member->isTypeEnum()) {
            std::cout << "Trying to render number as enum" << std::endl;
        }

        bool any_changed = false;

        auto enum_values = Object::getMetaEnumValues(m_member).value();
        auto enum_type = Object::getMetaType(m_member).value();

        if (m_numbers.size() != m_member->arraysize()) {
            init();
            m_checked_state.resize(m_numbers.size());
            for (size_t i = 0; i < m_checked_state.size(); ++i) {
                s64 value   = m_numbers.at(i);
                auto &state = m_checked_state.at(i);
                m_checked_state.at(i).resize(enum_values.size());
                for (size_t j = 0; j < enum_values.size(); ++j) {
                    if ((value & getEnumFlagValue(enum_values.at(j), enum_type)) != 0) {
                        state.at(j) = true;
                    }
                }
            }
        }

        const bool is_array = m_member->isArray();

        ImVec2 window_size        = ImGui::GetWindowSize();
        const bool collapse_lines = window_size.x < 350;
        if (is_array || m_member->isEmpty()) {
            if (ImGui::BeginGroupPanel(m_member->name().c_str(), &m_open, {})) {
                for (size_t i = 0; i < m_checked_state.size(); ++i) {
                    std::string array_str = std::format("[{}]", i).c_str();
                    if (ImGui::BeginGroupPanel(array_str.c_str(),
                                               reinterpret_cast<bool *>(&m_array_open.at(i)), {})) {
                        s64 &number = m_numbers.at(i);
                        for (size_t j = 0; j < m_checked_state.at(0).size(); ++j) {
                            if (ImGui::Checkbox(
                                    enum_values.at(j).first.c_str(),
                                    reinterpret_cast<bool *>(m_checked_state.at(i).data() + j))) {
                                bool checked = m_checked_state.at(i).at(j);
                                if (checked) {
                                    number |= getEnumFlagValue(enum_values.at(j), enum_type);
                                } else {
                                    number &= ~getEnumFlagValue(enum_values.at(j), enum_type);
                                }
                                any_changed = true;
                            }
                        }
                        Object::setMetaValue(m_member, i, number);
                    }
                    ImGui::EndGroupPanel();
                }
            }
            ImGui::EndGroupPanel();

            return any_changed;
        }

        if (ImGui::BeginGroupPanel(m_member->name().c_str(), &m_open, {})) {
            s64 &number = m_numbers.at(0);
            for (size_t j = 0; j < m_checked_state.at(0).size(); ++j) {
                if (ImGui::Checkbox(enum_values.at(j).first.c_str(),
                                    reinterpret_cast<bool *>(m_checked_state.at(0).data() + j))) {
                    bool checked = m_checked_state.at(0).at(j);
                    if (checked) {
                        number |= getEnumFlagValue(enum_values.at(j), enum_type);
                    } else {
                        number &= ~getEnumFlagValue(enum_values.at(j), enum_type);
                    }
                    any_changed = true;
                }
            }
            if (any_changed)
                Object::setMetaValue(m_member, 0, number);
        }
        ImGui::EndGroupPanel();

        return any_changed;
    }

    StructProperty::StructProperty(std::shared_ptr<Object::MetaMember> prop)
        : IProperty(prop), m_open(true) {
        prop->syncArray();
        m_children_ary.resize(prop->arraysize());
        m_array_open.resize(m_children_ary.size());
        for (size_t i = 0; i < m_children_ary.size(); ++i) {
            auto struct_ = prop->value<Object::MetaStruct>(i).value();
            auto members = struct_->members();
            for (size_t j = 0; j < members.size(); ++j) {
                m_children_ary.at(i).push_back(createProperty(members.at(j)));
            }
        }
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
                auto new_prop = createProperty(members.at(j));
                new_prop->init();
                m_children_ary.at(i).push_back(std::move(new_prop));
            }
        }
    }

    bool StructProperty::render(float label_width) {
        ImGuiStyle &style = ImGui::GetStyle();

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

    std::unique_ptr<IProperty> createProperty(std::shared_ptr<Object::MetaMember> m_member) {
        auto meta_type = Object::getMetaType(m_member);
        if (m_member->isTypeStruct()) {
            return std::make_unique<StructProperty>(m_member);
        } else if (m_member->isTypeEnum()) {
            return std::make_unique<EnumProperty>(m_member);
        } else if (meta_type == Object::MetaType::STRING) {
            return std::make_unique<StringProperty>(m_member);
        } else if (meta_type == Object::MetaType::RGB || meta_type == Object::MetaType::RGBA) {
            return std::make_unique<ColorProperty>(m_member);
        } else if (meta_type == Object::MetaType::VEC3) {
            return std::make_unique<VectorProperty>(m_member);
        } else if (meta_type == Object::MetaType::TRANSFORM) {
            return std::make_unique<TransformProperty>(m_member);
        } else if (meta_type == Object::MetaType::F32) {
            return std::make_unique<FloatProperty>(m_member);
        } else if (meta_type == Object::MetaType::F64) {
            return std::make_unique<DoubleProperty>(m_member);
        } else if (meta_type == Object::MetaType::BOOL) {
            return std::make_unique<FloatProperty>(m_member);
        } else if (meta_type == Object::MetaType::COMMENT) {
            return {};
        } else {
            return std::make_unique<NumberProperty>(m_member);
        }
    }

    ImVec2 IProperty::labelSize() {
        ImFont *font      = ImGui::GetFont();
        ImGuiStyle &style = ImGui::GetStyle();

        ImVec2 textSize =
            font->CalcTextSizeA(font->FontSize, FLT_MAX, 0.0f, m_member->name().c_str());
        return textSize;
    }

}  // namespace Toolbox::UI
