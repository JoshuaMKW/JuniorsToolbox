#include "gui/property/property.hpp"
#include "gui/imgui_ext.hpp"
#include "gui/util.hpp"
#include "objlib/meta/member.hpp"
#include "objlib/object.hpp"

#include <imgui.h>

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

namespace Toolbox::UI {
    void BoolProperty::init() {
        m_member->syncArray();
        m_bools.resize(m_member->arraysize());
        for (size_t i = 0; i < m_bools.size(); ++i) {
            m_bools.at(i) = Object::getMetaValue<bool>(m_member, i).value();
        }
    }

    void BoolProperty::render(float label_width) {
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

        const bool is_array = m_member->isArray();

        ImVec2 window_size        = ImGui::GetWindowSize();
        const bool collapse_lines = window_size.x < 400;

        ImGui::Text(m_member->name().c_str());
        if (is_array || m_member->isEmpty()) {

            ImGuiID array_id = ImGui::GetID(m_member->name().c_str());
            if (ImGui::BeginChild(array_id, {0, 100}, true)) {
                for (size_t i = 0; i < m_bools.size(); ++i) {
                    std::string label = std::format("##{}-{}", m_member->name().c_str(), i);
                    ImGui::Checkbox(label.c_str(), reinterpret_cast<bool *>(m_bools.data() + i));
                }
            }
            ImGui::EndChild();

            return;
        }

        if (!collapse_lines) {
            ImGui::SameLine();
            ImGui::Dummy({label_width - ImGui::CalcTextSize(m_member->name().c_str()).x, 0});
            ImGui::SameLine();
        }

        std::string label = std::format("##{}", m_member->name().c_str());
        ImGui::Checkbox(label.c_str(), reinterpret_cast<bool *>(m_bools.data()));
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

    void NumberProperty::render(float label_width) {
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

        const bool is_array = m_member->isArray();

        ImVec2 window_size        = ImGui::GetWindowSize();
        const bool collapse_lines = window_size.x < 400;

        ImGui::Text(m_member->name().c_str());
        if (is_array || m_member->isEmpty()) {

            ImGuiID array_id = ImGui::GetID(m_member->name().c_str());
            if (ImGui::BeginChild(array_id, {0, 100}, true)) {
                for (size_t i = 0; i < m_numbers.size(); ++i) {
                    std::string label = std::format("##{}-{}", m_member->name().c_str(), i);
                    if (ImGui::InputScalar(label.c_str(), ImGuiDataType_S64, m_numbers.data() + i,
                                           nullptr, nullptr, nullptr,
                                           ImGuiInputTextFlags_CharsDecimal |
                                               ImGuiInputTextFlags_CharsNoBlank)) {
                        s64 &number = m_numbers.at(i);
                        if (number > m_max) {
                            number = m_min + (number - m_max);
                        } else if (number < m_min) {
                            number = m_max + (number - m_min);
                        }
                        Object::setMetaValue(m_member, i, number);
                    }
                }
            }
            ImGui::EndChild();

            return;
        }

        if (!collapse_lines) {
            ImGui::SameLine();
            ImGui::Dummy({label_width - ImGui::CalcTextSize(m_member->name().c_str()).x, 0});
            ImGui::SameLine();
        }

        std::string label = std::format("##{}", m_member->name().c_str());
        if (ImGui::InputScalar(
                label.c_str(), ImGuiDataType_S64, m_numbers.data(), nullptr, nullptr, nullptr,
                ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_CharsNoBlank)) {
            s64 &number = m_numbers.at(0);
            number      = std::clamp(number, m_min, m_max);
            Object::setMetaValue(m_member, 0, number);
        }
    }

    void FloatProperty::init() {
        m_member->syncArray();
        m_numbers.resize(m_member->arraysize());

        switch (Object::getMetaType(m_member).value()) {
        case Object::MetaType::F32:
            for (size_t i = 0; i < m_numbers.size(); ++i) {
                m_numbers.at(i) = Object::getMetaValue<f32>(m_member, i).value();
            }
            m_min = -FLT_MAX;
            m_max = FLT_MAX;
            break;
        case Object::MetaType::F64:
            for (size_t i = 0; i < m_numbers.size(); ++i) {
                m_numbers.at(i) = Object::getMetaValue<f64>(m_member, i).value();
            }
            m_min = -DBL_MAX;
            m_max = DBL_MAX;
            break;
        default:
            for (size_t i = 0; i < m_numbers.size(); ++i) {
                m_numbers.at(i) = Object::getMetaValue<f64>(m_member, i).value();
            }
            m_min = -DBL_MAX;
            m_max = DBL_MAX;
            break;
        }
    }

    void FloatProperty::render(float label_width) {
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

        const bool is_array = m_member->isArray();

        ImVec2 window_size        = ImGui::GetWindowSize();
        const bool collapse_lines = window_size.x < 400;

        ImGui::Text(m_member->name().c_str());
        if (is_array || m_member->isEmpty()) {

            ImGuiID array_id = ImGui::GetID(m_member->name().c_str());
            if (ImGui::BeginChild(array_id, {0, 100}, true)) {
                for (size_t i = 0; i < m_numbers.size(); ++i) {
                    std::string label = std::format("##{}-{}", m_member->name().c_str(), i);
                    if (ImGui::InputScalar(label.c_str(), ImGuiDataType_Double,
                                           m_numbers.data() + i, nullptr, nullptr, nullptr,
                                           ImGuiInputTextFlags_CharsDecimal |
                                               ImGuiInputTextFlags_CharsNoBlank)) {
                        f64 &number = m_numbers.at(i);
                        if (number > m_max) {
                            number = m_min + (number - m_max);
                        } else if (number < m_min) {
                            number = m_max + (number - m_min);
                        }
                        Object::setMetaValue(m_member, i, number);
                    }
                }
            }
            ImGui::EndChild();

            return;
        }

        if (!collapse_lines) {
            ImGui::SameLine();
            ImGui::Dummy({label_width - ImGui::CalcTextSize(m_member->name().c_str()).x, 0});
            ImGui::SameLine();
        }

        std::string label = std::format("##{}", m_member->name().c_str());
        if (ImGui::InputScalar(
                label.c_str(), ImGuiDataType_Double, m_numbers.data(), nullptr, nullptr, nullptr,
                ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_CharsNoBlank)) {
            f64 &number = m_numbers.at(0);
            number      = std::clamp(number, m_min, m_max);
            Object::setMetaValue(m_member, 0, number);
        }
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

    void StringProperty::render(float label_width) {
        ImGuiStyle &style = ImGui::GetStyle();

        if (m_member->isTypeStruct()) {
            std::cout << "Trying to render struct as number" << std::endl;
        }

        if (m_member->isTypeEnum()) {
            std::cout << "Trying to render enum as number" << std::endl;
        }

        const bool is_array = m_member->isArray();

        ImVec2 window_size        = ImGui::GetWindowSize();
        const bool collapse_lines = window_size.x < 400;

        if (m_strings.size() != m_member->arraysize()) {
            init();
        }

        ImGui::Text(m_member->name().c_str());
        if (is_array || m_member->isEmpty()) {

            ImGuiID array_id = ImGui::GetID(m_member->name().c_str());
            if (ImGui::BeginChild(array_id, {0, 100}, true)) {
                for (size_t i = 0; i < m_strings.size(); ++i) {
                    auto &str_data    = m_strings.at(i);
                    std::string label = std::format("##{}-{}", m_member->name().c_str(), i);
                    if (ImGui::InputText(label.c_str(), str_data.data(), str_data.size())) {
                        Object::setMetaValue(m_member, i, convertArrayToStringView(str_data));
                    }
                }
            }
            ImGui::EndChild();

            return;
        }

        if (!collapse_lines) {
            ImGui::SameLine();
            ImGui::Dummy({label_width - ImGui::CalcTextSize(m_member->name().c_str()).x, 0});
            ImGui::SameLine();
        }

        std::string label = std::format("##{}", m_member->name().c_str());
        auto &str_data    = m_strings.at(0);
        if (ImGui::InputText(label.c_str(), str_data.data(), str_data.size())) {
            Object::setMetaValue(m_member, 0, convertArrayToStringView(str_data));
        }
    }

    void ColorProperty::init() {
        m_member->syncArray();
        m_colors.resize(m_member->arraysize());
    }

    void ColorProperty::render(float label_width) {
        ImGuiStyle &style = ImGui::GetStyle();

        if (m_member->isTypeStruct()) {
            std::cout << "Trying to render struct as number" << std::endl;
        }

        if (m_member->isTypeEnum()) {
            std::cout << "Trying to render enum as number" << std::endl;
        }

        const bool use_alpha = Object::getMetaType(m_member).value() == Object::MetaType::RGBA;

        const bool is_array = m_member->isArray();

        ImVec2 window_size        = ImGui::GetWindowSize();
        const bool collapse_lines = window_size.x < 400;

        if (m_colors.size() != m_member->arraysize()) {
            init();
        }

        ImGui::Text(m_member->name().c_str());
        if (is_array || m_member->isEmpty()) {

            ImGuiID array_id = ImGui::GetID(m_member->name().c_str());
            if (ImGui::BeginChild(array_id, {0, 100}, true)) {
                for (size_t i = 0; i < m_colors.size(); ++i) {
                    Color::RGBA32 &color = m_colors.at(i);

                    ImGui::InputScalar("r", ImGuiDataType_U8, &color.m_r, nullptr, nullptr, nullptr,
                                       ImGuiInputTextFlags_CharsDecimal |
                                           ImGuiInputTextFlags_CharsNoBlank);
                    ImGui::InputScalar("g", ImGuiDataType_U8, &color.m_g, nullptr, nullptr, nullptr,
                                       ImGuiInputTextFlags_CharsDecimal |
                                           ImGuiInputTextFlags_CharsNoBlank);
                    ImGui::InputScalar("b", ImGuiDataType_U8, &color.m_b, nullptr, nullptr, nullptr,
                                       ImGuiInputTextFlags_CharsDecimal |
                                           ImGuiInputTextFlags_CharsNoBlank);

                    if (use_alpha) {
                        ImGui::InputScalar(
                            "a", ImGuiDataType_U8, &color.m_a, nullptr, nullptr, nullptr,
                            ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_CharsNoBlank);
                        Object::setMetaValue<Color::RGBA32>(m_member, i, color);
                    } else {
                        Object::setMetaValue<Color::RGB24>(
                            m_member, i, Color::RGB24(color.m_r, color.m_g, color.m_b));
                    }
                }
            }
            ImGui::EndChild();

            return;
        }

        if (!collapse_lines) {
            ImGui::SameLine();
            ImGui::Dummy({label_width - ImGui::CalcTextSize(m_member->name().c_str()).x, 0});
            ImGui::SameLine();
        }

        Color::RGBA32 &color = m_colors.at(0);

        ImGui::InputScalar("r", ImGuiDataType_U8, &color.m_r, nullptr, nullptr, nullptr,
                           ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_CharsNoBlank);
        ImGui::InputScalar("g", ImGuiDataType_U8, &color.m_g, nullptr, nullptr, nullptr,
                           ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_CharsNoBlank);
        ImGui::InputScalar("b", ImGuiDataType_U8, &color.m_b, nullptr, nullptr, nullptr,
                           ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_CharsNoBlank);

        if (use_alpha) {
            ImGui::InputScalar("a", ImGuiDataType_U8, &color.m_a, nullptr, nullptr, nullptr,
                               ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_CharsNoBlank);
            Object::setMetaValue<Color::RGBA32>(m_member, 0, color);
        } else {
            Object::setMetaValue<Color::RGB24>(m_member, 0,
                                               Color::RGB24(color.m_r, color.m_g, color.m_b));
        }
    }

    // TODO: Implement these!
    void VectorProperty::init() { m_member->syncArray(); }

    void VectorProperty::render(float label_width) {}

    void TransformProperty::init() { m_member->syncArray(); }

    void TransformProperty::render(float label_width) {}

    void EnumProperty::render(float label_width) {
        ImGuiStyle &style = ImGui::GetStyle();

        if (m_member->isTypeStruct()) {
            std::cout << "Trying to render struct as enum" << std::endl;
        }

        if (!m_member->isTypeEnum()) {
            std::cout << "Trying to render number as enum" << std::endl;
        }

        auto enum_values = Object::getMetaEnumValues(m_member).value();

        if (m_numbers.size() != m_member->arraysize()) {
            init();
            m_checked_state.resize(m_numbers.size());
            for (size_t i = 0; i < m_checked_state.size(); ++i) {
                s64 value   = m_numbers.at(i);
                auto &state = m_checked_state.at(i);
                m_checked_state.at(i).resize(enum_values.size());
                for (size_t j = 0; j < m_checked_state.size(); ++j) {
                    if ((value & (1 << j)) != 0) {
                        state.at(i) = true;
                    }
                }
            }
        }

        const bool is_array = m_member->isArray();

        ImVec2 window_size        = ImGui::GetWindowSize();
        const bool collapse_lines = window_size.x < 400;

        ImGui::Text(m_member->name().c_str());
        if (is_array || m_member->isEmpty()) {

            ImGuiID array_id = ImGui::GetID(m_member->name().c_str());
            if (ImGui::BeginChild(array_id, {0, 100}, true)) {
                for (size_t i = 0; i < m_checked_state.size(); ++i) {
                    ImGuiID array_enum_id =
                        ImGui::GetID(std::format("##{}-{}", m_member->name(), i).c_str());
                    if (ImGui::BeginChild(array_enum_id, {0, 100}, true)) {
                        s64 &number = m_numbers.at(i);
                        for (size_t j = 0; j < m_checked_state.at(0).size(); ++j) {
                            if (ImGui::Checkbox(
                                    enum_values.at(j).first.c_str(),
                                    reinterpret_cast<bool *>(m_checked_state.at(i).data() + j))) {
                                bool checked = m_checked_state.at(i).at(j);
                                if (checked) {
                                    number |= (1 << j);
                                } else {
                                    number &= ~(1 << j);
                                }
                            }
                        }
                        Object::setMetaValue(m_member, i, number);
                    }
                    ImGui::EndChild();
                }
            }
            ImGui::EndChild();

            return;
        }
        ImVec2 rect_pos_min = ImGui::GetCursorPos();
        float rect_width    = ImGui::GetWindowSize().x - (style.WindowPadding.x * 2);

        std::string label = std::format("##{}", m_member->name().c_str());
        s64 &number       = m_numbers.at(0);
        for (size_t j = 0; j < m_checked_state.at(0).size(); ++j) {
            if (ImGui::Checkbox(enum_values.at(j).first.c_str(),
                                reinterpret_cast<bool *>(m_checked_state.at(0).data() + j))) {
                bool checked = m_checked_state.at(0).at(j);
                if (checked) {
                    number |= (1 << j);
                } else {
                    number &= ~(1 << j);
                }
            }
        }
        Object::setMetaValue(m_member, 0, number);

        ImVec2 rect_pos_max = ImGui::GetCursorPos();
        rect_pos_max.x += rect_width;

        ImDrawList *draw_list = ImGui::GetForegroundDrawList();
        draw_list->AddRect(rect_pos_min, rect_pos_max,
                           ImGui::ColorConvertFloat4ToU32(style.Colors[ImGuiCol_Border]),
                           ImDrawListFlags_AntiAliasedLines);
    }

    StructProperty::StructProperty(std::shared_ptr<Object::MetaMember> prop)
        : IProperty(prop), m_open(true), m_array_open() {
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

    void StructProperty::render(float label_width) {
        ImGuiStyle &style = ImGui::GetStyle();

        if (m_children_ary.size() != m_member->arraysize()) {
            init();
        }

        ImGuiID struct_id = ImGui::GetID(m_member->name().c_str());
        // ImGui::PushID(struct_id);
        if (ImGui::CollapsingHeader(m_member->name().c_str())) {
            if (m_children_ary.size() > 1) {
                float label_width = 0;
                for (size_t i = 0; i < m_children_ary.at(0).size(); ++i) {
                    label_width =
                        std::max(label_width, m_children_ary.at(0).at(i)->labelSize().x);
                }
                for (size_t i = 0; i < m_children_ary.size(); ++i) {
                    auto struct_ = m_member->value<Object::MetaStruct>(i).value();
                    auto members = struct_->members();

                    std::string array_name =
                        std::format("[{}]##{}", i, m_member->name().c_str());
                    if (ImGui::BeginGroupPanel(
                            array_name.c_str(),
                            reinterpret_cast<bool *>(m_array_open.data() + i), {})) {
                        for (size_t j = 0; j < members.size(); ++j) {
                            m_children_ary.at(i).at(j)->render(label_width);
                            ImGui::ItemSize({0, 2});
                        }
                    }
                    ImGui::EndGroupPanel();
                }
            } else if (m_children_ary.size() == 1) {
                float label_width = 0;
                for (size_t i = 0; i < m_children_ary.at(0).size(); ++i) {
                    label_width =
                        std::max(label_width, m_children_ary.at(0).at(i)->labelSize().x);
                }
                auto struct_ = m_member->value<Object::MetaStruct>(0).value();
                auto members = struct_->members();
                for (size_t j = 0; j < members.size(); ++j) {
                    m_children_ary.at(0).at(j)->render(label_width);
                    ImGui::ItemSize({0, 2});
                }
            }
        }
        ImGui::ItemSize({0, 4});

        // ImGui::PopID();

        // ImVec2 item_rect_min = ImGui::GetItemRectMin();
        // ImVec2 item_rect_max = ImGui::GetItemRectMax();

        // ImDrawList *draw_list = ImGui::GetWindowDrawList();
        // draw_list->AddRect(item_rect_min, item_rect_max,
        //                    ImGui::ColorConvertFloat4ToU32(style.Colors[ImGuiCol_Border])); //
        //                    Red color for the border
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
        } else if (meta_type == Object::MetaType::F32 || meta_type == Object::MetaType::F64) {
            return std::make_unique<FloatProperty>(m_member);
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
