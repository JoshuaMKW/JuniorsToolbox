#include "dolphin/process.hpp"
#include "dolphin/watch.hpp"
#include "gui/application.hpp"
#include "gui/debugger/dialog.hpp"

using namespace Toolbox::Dolphin;

namespace Toolbox::UI {

    void AddWatchDialog::setup() {
        memset(m_watch_name.data(), 0, m_watch_name.size());
        for (int i = 0; i < 8; ++i) {
            m_watch_p_chain[i].fill('\0');
        }
        m_watch_p_chain_size = 2;
        m_watch_type         = MetaType::U8;
        m_watch_size         = 1;
    }

    void AddWatchDialog::openToAddress(u32 address) {
        open();
        snprintf(m_watch_p_chain[0].data(), m_watch_p_chain[0].size(), "%08X", address);
    }

    void AddWatchDialog::openToAddressAsType(u32 address, MetaType type, size_t address_size) {
        open();
        snprintf(m_watch_p_chain[0].data(), m_watch_p_chain[0].size(), "%08X", address);
        m_watch_type = type;
        m_watch_size = address_size;
    }

    void AddWatchDialog::openToAddressAsBytes(u32 address, size_t address_size) {
        open();
        snprintf(m_watch_p_chain[0].data(), m_watch_p_chain[0].size(), "%08X", address);
        m_watch_type = MetaType::UNKNOWN;
        m_watch_size = address_size;
    }

    void AddWatchDialog::render(ModelIndex group_idx, size_t row) {
        const bool state_valid =
            m_filter_predicate
                ? m_filter_predicate(
                      std::string_view(m_watch_name.data(), strlen(m_watch_name.data())), group_idx)
                : true;

        const float label_width = 4.0f * ImGui::GetFontSize();

        ImGuiStyle &style = ImGui::GetStyle();

        if (m_opening) {
            ImGui::OpenPopup("Add Watch");
            m_open = true;
        }

        if (ImGui::BeginPopupModal("Add Watch", &m_open, ImGuiWindowFlags_AlwaysAutoResize)) {
            m_opening = false;

            // Render the watch name input box
            // ---
            {
                ImGui::TextAndWidth(label_width, "Name: ");
                ImGui::SameLine();

                // ImGui::SetNextItemWidth(150.0f);
                ImGui::InputTextWithHint("##watch_name", "Enter unique name here...",
                                         m_watch_name.data(), m_watch_name.size(),
                                         ImGuiInputTextFlags_AutoSelectAll);
            }

            // Render the watch type selection combo box
            // ---
            {
                ImGui::TextAndWidth(label_width, "Type: ");
                ImGui::SameLine();

                const char *watch_type_items[] = {
                    "BOOL", "S8",     "U8",   "S16",       "U16",   "S32", "U32",  "F32",
                    "F64",  "STRING", "VEC3", "TRANSFORM", "MTX34", "RGB", "RGBA", "BYTES"};
                const char *current_watch_type = watch_type_items[static_cast<int>(m_watch_type)];

                // ImGui::SetNextItemWidth(150.0f);
                if (ImGui::BeginCombo("##watch_type_combo", current_watch_type)) {
                    for (int i = 0; i < IM_ARRAYSIZE(watch_type_items); ++i) {
                        if (ImGui::Selectable(watch_type_items[i],
                                              m_watch_type == static_cast<MetaType>(i))) {
                            m_watch_type = static_cast<MetaType>(i);
                        }
                    }
                    ImGui::EndCombo();
                }
            }

            if (m_watch_type == MetaType::STRING || m_watch_type == MetaType::UNKNOWN) {
                ImGui::TextAndWidth(label_width, "Length: ");
                ImGui::SameLine();

                int value = (int)m_watch_size;
                ImGui::InputInt("##watch_length", &value, 1, 10, ImGuiInputTextFlags_CharsNoBlank);
                m_watch_size = std::clamp<int>(value, 1, WATCH_MAX_BUFFER_SIZE - 1);
            } else {
                m_watch_size = 0;
            }

            // Render the address input box
            // ---
            {
                ImGui::TextAndWidth(label_width, "Address: ");
                ImGui::SameLine();

                // ImGui::SetNextItemWidth(150.0f);
                ImGui::InputTextWithHint("##watch_address", "Enter address in hex...",
                                         m_watch_p_chain[0].data(), m_watch_p_chain[0].size(),
                                         ImGuiInputTextFlags_AutoSelectAll |
                                             ImGuiInputTextFlags_CharsHexadecimal);
            }

            // Render the address pointer checkbox
            // ---
            {
                ImGui::TextAndWidth(label_width, "Is Pointer: ");
                ImGui::SameLine();

                ImGui::Checkbox("##watch_is_pointer", &m_watch_is_pointer);
            }

            // Render the preview based on the watch type
            // ---
            std::vector<u32> pointer_chain;
            pointer_chain.resize(m_watch_p_chain_size);
            for (int i = 0; i < m_watch_p_chain_size; ++i) {
                pointer_chain[i] = strtoul(m_watch_p_chain[i].data(), nullptr, 16);
            }

            std::vector<u32> address_chain =
                MemoryWatch::ResolvePointerChainAsAddress(pointer_chain);

            float panel_height =
                ImGui::GetTextLineHeightWithSpacing() + style.FramePadding.y * 2.0f;
            panel_height *= (address_chain.size() + 1);
            float panel_width = ImGui::GetContentRegionAvail().x;

            if (m_watch_is_pointer) {
                ImGuiID addr_panel_id = ImGui::GetID("##addr_panel");
                if (ImGui::BeginChildPanel(addr_panel_id, {panel_width, panel_height})) {
                    if (ImGui::Button("Add Offset")) {
                        m_watch_p_chain_size = std::clamp<u32>(m_watch_p_chain_size + 1, 2, 8);
                    }

                    ImGui::SameLine();

                    if (ImGui::Button("Remove Offset")) {
                        m_watch_p_chain_size = std::clamp<u32>(m_watch_p_chain_size - 1, 2, 8);
                    }

                    ImGui::Separator();

                    // Render the offset input boxes
                    // ---
                    {
                        // ImGui::SetNextItemWidth(150.0f);
                        for (int i = 1; i < address_chain.size(); ++i) {
                            ImGui::TextAndWidth(label_width - style.WindowPadding.x,
                                                "Level %d: ", i);
                            ImGui::SameLine();

                            ImGui::SetNextItemWidth(100.0f);
                            std::string tag = std::format("##watch_address-{}", i);
                            ImGui::InputTextWithHint(tag.data(), "Enter offset in hex...",
                                                     m_watch_p_chain[i].data(),
                                                     m_watch_p_chain[i].size(),
                                                     ImGuiInputTextFlags_AutoSelectAll |
                                                         ImGuiInputTextFlags_CharsHexadecimal);

                            ImGui::SameLine();
                            ImGui::Text("-> %08X", address_chain[i]);
                        }
                    }
                }
                ImGui::EndChildPanel();
            }

            size_t watch_name_length = strlen(m_watch_name.data());
            std::string_view watch_name_view(m_watch_name.data(), watch_name_length);

            u32 address = m_watch_is_pointer
                              ? MemoryWatch::TracePointerChainToAddress(pointer_chain)
                              : pointer_chain[0];
            size_t address_size;
            if (m_watch_type == MetaType::STRING || m_watch_type == MetaType::UNKNOWN) {
                address_size = m_watch_size;
            } else {
                address_size = meta_type_size(m_watch_type);
            }

            {
                ImGui::Separator();

                renderPreview(label_width, address, address_size);
            }

            if (!state_valid) {
                ImVec4 disabled_color = ImGui::GetStyle().Colors[ImGuiCol_Button];
                disabled_color.x -= 0.1f;
                disabled_color.y -= 0.1f;
                disabled_color.z -= 0.1f;
                // Change button color to make it look "disabled"
                ImGui::PushStyleColor(ImGuiCol_Button, disabled_color);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, disabled_color);
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, disabled_color);
                // Begin disabled block
                ImGui::BeginDisabled();
            }

            if (ImGui::Button("Create")) {
                m_on_accept(group_idx, row, m_insert_policy, watch_name_view, m_watch_type,
                            pointer_chain, address_size, m_watch_is_pointer);
                m_open = false;
            }

            if (!state_valid) {
                // End disabled block
                ImGui::EndDisabled();
                // Restore the modified style colors
                ImGui::PopStyleColor(3);
            }

            ImGui::SameLine();

            if (ImGui::Button("Cancel")) {
                if (m_on_reject) {
                    m_on_reject(group_idx);
                }
                m_open = false;
            }

            ImGui::EndPopup();
        }
    }

    void AddWatchDialog::renderPreview(f32 label_width, u32 address, size_t address_size) {
        ImGuiStyle &style = ImGui::GetStyle();

        switch (m_watch_type) {
        default:
            renderPreviewSingle(label_width, address, address_size);
            break;
        case MetaType::RGB:
            renderPreviewRGB(label_width, address);
            break;
        case MetaType::RGBA:
            renderPreviewRGBA(label_width, address);
            break;
        case MetaType::VEC3:
            renderPreviewVec3(label_width, address);
            break;
        case MetaType::TRANSFORM:
            renderPreviewTransform(label_width, address);
            break;
        case MetaType::MTX34:
            renderPreviewMatrix34(label_width, address);
            break;
        case MetaType::STRING:
            renderPreviewSingle(label_width, address, address_size);
            break;
        case MetaType::UNKNOWN:
            renderPreviewSingle(label_width, address, address_size);
            break;
        }
    }

    void AddWatchDialog::renderPreviewSingle(f32 label_width, u32 address, size_t address_size) {
        ImVec2 cursor_pos  = ImGui::GetCursorPos();
        ImVec2 preview_pos = cursor_pos;
        preview_pos.x += label_width;

        ImGui::TextAndWidth(label_width, "Preview: ");

        ImGui::SetCursorPos(preview_pos);
        ImGui::SameLine();

        size_t preview_size = 256;

        char *value_buf = new char[preview_size];
        calcPreview(value_buf, preview_size, address, address_size, m_watch_type);

        if (address_size < 8) {
            ImGui::SetNextItemWidth(200.0f);
        } else {
            ImGui::SetNextItemWidth(350.0f);
        }

        ImGui::InputText("##single_preview", value_buf, preview_size,
                         ImGuiInputTextFlags_ReadOnly | ImGuiInputTextFlags_AutoSelectAll);

        delete[] value_buf;
    }

    void AddWatchDialog::renderPreviewRGBA(f32 label_width, u32 address) {
        ImGuiStyle &style  = ImGui::GetStyle();
        ImVec2 cursor_pos  = ImGui::GetCursorPos();
        ImVec2 preview_pos = cursor_pos;
        preview_pos.x += label_width;

        ImGui::TextAndWidth(label_width, "Preview: ");

        ImGui::SetCursorPos(preview_pos);
        ImGui::SameLine();

        char value_buf[32] = {};
        calcPreview(value_buf, sizeof(value_buf), address, 4, m_watch_type);

        ImGui::SetNextItemWidth(400.0f - ImGui::GetFrameHeight() - style.ItemSpacing.x);
        ImGui::InputText("##single_preview", value_buf, sizeof(value_buf),
                         ImGuiInputTextFlags_ReadOnly | ImGuiInputTextFlags_AutoSelectAll);

        ImGui::SameLine();

        Color::RGBAShader calc_color = calcColorRGBA(address);
        ImVec4 color_rgba = {calc_color.m_r, calc_color.m_g, calc_color.m_b, calc_color.m_a};

        if (ImGui::ColorButton("##color_rgb_preview", color_rgba,
                               ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop |
                                   ImGuiColorEditFlags_NoPicker | ImGuiColorEditFlags_NoInputs)) {
            // Color button clicked, do nothing
        }
    }

    void AddWatchDialog::renderPreviewRGB(f32 label_width, u32 address) {
        ImGuiStyle &style = ImGui::GetStyle();

        ImGui::TextAndWidth(label_width, "Preview: ");

        ImGui::SameLine();

        char value_buf[32] = {};
        calcPreview(value_buf, sizeof(value_buf), address, 3, m_watch_type);

        ImGui::SetNextItemWidth(400.0f - ImGui::GetFrameHeight() - style.ItemSpacing.x);
        ImGui::InputText("##single_preview", value_buf, sizeof(value_buf),
                         ImGuiInputTextFlags_ReadOnly | ImGuiInputTextFlags_AutoSelectAll);

        ImGui::SameLine();

        Color::RGBShader calc_color = calcColorRGB(address);
        ImVec4 color_rgba           = {calc_color.m_r, calc_color.m_g, calc_color.m_b, 1.0f};

        if (ImGui::ColorButton("##color_rgb_preview", color_rgba,
                               ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop |
                                   ImGuiColorEditFlags_NoPicker | ImGuiColorEditFlags_NoInputs)) {
            // Color button clicked, do nothing
        }
    }

    void AddWatchDialog::renderPreviewVec3(f32 label_width, u32 address) {
        ImGuiStyle &style    = ImGui::GetStyle();
        ImVec2 cursor_origin = ImGui::GetCursorPos();

        ImVec2 preview_size = {0, 0};
        preview_size.x      = 100.0f;
        preview_size.y      = ImGui::GetFrameHeight();

        ImGui::TextAndWidth(label_width, "Preview: ");
        ImGui::SameLine(0, style.ItemSpacing.x * 2.0f);

        ImVec2 preview_pos = cursor_origin;
        preview_pos.x      = ImGui::GetCursorPosX();

        ImGui::SetCursorPos(preview_pos);

        ImGuiWindow *window            = ImGui::GetCurrentWindow();
        window->DC.CursorPosPrevLine.y = window->Pos.y + preview_pos.y;

        ImGui::BeginGroup();
        {

            for (int i = 0; i < 3; ++i) {
                char value_buf[32] = {};
                calcPreview(value_buf, sizeof(value_buf), address + i * 4, 4, MetaType::F32);

                ImGui::PushID(i);

                ImGui::SetNextItemWidth(100.0f);
                ImGui::InputText("##vec3_f32_preview", value_buf, sizeof(value_buf),
                                 ImGuiInputTextFlags_ReadOnly | ImGuiInputTextFlags_AutoSelectAll);

                if (i < 2) {
                    ImGui::SameLine();
                }

                ImGui::PopID();
            }
        }
        ImGui::EndGroup();

        ImRect bb = {
            {preview_pos.x - style.ItemSpacing.x,     preview_pos.y                 },
            {preview_pos.x - style.ItemSpacing.x + 2, preview_pos.y + preview_size.y}
        };
        bb.Translate(window->Pos);
        window->DrawList->AddRectFilled(bb.Min, bb.Max, ImGui::GetColorU32(ImGuiCol_Separator));
    }

    void AddWatchDialog::renderPreviewTransform(f32 label_width, u32 address) {
        ImGuiStyle &style = ImGui::GetStyle();

        ImVec2 preview_size   = {0, 0};
        float label_width_vec = ImGui::CalcTextSize("R:").x;

        preview_size.x = 100.0f * 3.0f;
        preview_size.x += label_width_vec;
        preview_size.x += style.ItemSpacing.x * 3.0f;  // Add spacing for padding

        preview_size.y = ImGui::GetFrameHeight() * 3.0f;
        preview_size.y += style.ItemSpacing.y * 2.0f;  // Add spacing for padding

        ImVec2 cursor_origin = ImGui::GetCursorPos();
        ImVec2 label_pos     = cursor_origin;
        label_pos.y += preview_size.y * 0.5f - ImGui::GetTextLineHeight() * 0.5f;

        ImGui::SetCursorPos(label_pos);
        ImGui::TextAndWidth(label_width, "Preview: ");

        ImGui::SameLine(0, style.ItemSpacing.x * 2.0f);

        ImVec2 preview_pos = cursor_origin;
        preview_pos.x      = ImGui::GetCursorPosX();

        ImGui::SetCursorPos(preview_pos);

        ImGuiWindow *window            = ImGui::GetCurrentWindow();
        window->DC.CursorPosPrevLine.y = window->Pos.y + preview_pos.y;

        ImGui::BeginGroup();
        {
            // In Sunshine, transforms have the following memory layout...
            // Translation: (ofs: 0x0)  3 x f32 (12 bytes)
            // Rotation:    (ofs: 0x14) 3 x f32 (12 bytes)
            // Scale:       (ofs: 0x20) 3 x f32 (12 bytes)
            // ---

            ImGui::TextAndWidth(label_width_vec, "T:");
            ImGui::SameLine();

            for (int i = 0; i < 3; ++i) {
                char value_buf[32] = {};
                calcPreview(value_buf, sizeof(value_buf), address + i * 4, 4, MetaType::F32);

                ImGui::PushID(i);

                ImGui::SetNextItemWidth(100.0f);
                ImGui::InputText("##vec3_f32_preview_trans", value_buf, sizeof(value_buf),
                                 ImGuiInputTextFlags_ReadOnly | ImGuiInputTextFlags_AutoSelectAll);

                if (i < 2) {
                    ImGui::SameLine();
                }

                ImGui::PopID();
            }

            ImGui::TextAndWidth(label_width_vec, "R:");
            ImGui::SameLine();

            for (int i = 0; i < 3; ++i) {
                char value_buf[32] = {};
                calcPreview(value_buf, sizeof(value_buf), address + i * 4 + 0x14, 4, MetaType::F32);

                ImGui::PushID(i);

                ImGui::SetNextItemWidth(100.0f);
                ImGui::InputText("##vec3_f32_preview_rot", value_buf, sizeof(value_buf),
                                 ImGuiInputTextFlags_ReadOnly | ImGuiInputTextFlags_AutoSelectAll);

                if (i < 2) {
                    ImGui::SameLine();
                }

                ImGui::PopID();
            }

            ImGui::TextAndWidth(label_width_vec, "S:");
            ImGui::SameLine();

            for (int i = 0; i < 3; ++i) {
                char value_buf[32] = {};
                calcPreview(value_buf, sizeof(value_buf), address + i * 4 + 0x20, 4, MetaType::F32);

                ImGui::PushID(i);

                ImGui::SetNextItemWidth(100.0f);
                ImGui::InputText("##transform_f32_preview_scale", value_buf, sizeof(value_buf),
                                 ImGuiInputTextFlags_ReadOnly | ImGuiInputTextFlags_AutoSelectAll);

                if (i < 2) {
                    ImGui::SameLine();
                }

                ImGui::PopID();
            }
        }
        ImGui::EndGroup();

        ImRect bb = {
            {preview_pos.x - style.ItemSpacing.x,     preview_pos.y                 },
            {preview_pos.x - style.ItemSpacing.x + 2, preview_pos.y + preview_size.y}
        };
        bb.Translate(window->Pos);
        window->DrawList->AddRectFilled(bb.Min, bb.Max, ImGui::GetColorU32(ImGuiCol_Separator));
    }

    void AddWatchDialog::renderPreviewMatrix34(f32 label_width, u32 address) {
        // view_mtx[0][0] = communicator.read<f32>(view_mtx_ptr + 0x00).value();
        // view_mtx[1][0] = communicator.read<f32>(view_mtx_ptr + 0x04).value();
        // view_mtx[2][0] = communicator.read<f32>(view_mtx_ptr + 0x08).value();
        // view_mtx[0][1] = communicator.read<f32>(view_mtx_ptr + 0x10).value();
        // view_mtx[1][1] = communicator.read<f32>(view_mtx_ptr + 0x14).value();
        // view_mtx[2][1] = communicator.read<f32>(view_mtx_ptr + 0x18).value();
        // view_mtx[0][2] = -communicator.read<f32>(view_mtx_ptr + 0x20).value();
        // view_mtx[1][2] = -communicator.read<f32>(view_mtx_ptr + 0x24).value();
        // view_mtx[2][2] = -communicator.read<f32>(view_mtx_ptr + 0x28).value();
        // view_mtx[3][0] = communicator.read<f32>(view_mtx_ptr + 0x0C).value();
        // view_mtx[3][1] = communicator.read<f32>(view_mtx_ptr + 0x1C).value();
        // view_mtx[3][2] = -communicator.read<f32>(view_mtx_ptr + 0x2C).value();

        ImGuiStyle &style = ImGui::GetStyle();

        ImVec2 preview_size = {0, 0};

        preview_size.x = 100.0f * 4.0f;
        preview_size.x += style.ItemSpacing.x * 3.0f;  // Add spacing for padding

        preview_size.y = ImGui::GetFrameHeight() * 3.0f;
        preview_size.y += style.ItemSpacing.y * 2.0f;  // Add spacing for padding

        ImVec2 cursor_origin = ImGui::GetCursorPos();
        ImVec2 label_pos     = cursor_origin;
        label_pos.y += preview_size.y * 0.5f - ImGui::GetTextLineHeight() * 0.5f;

        ImGui::SetCursorPos(label_pos);
        ImGui::TextAndWidth(label_width, "Preview: ");

        ImGui::SameLine(0, style.ItemSpacing.x * 2.0f);

        ImVec2 preview_pos = cursor_origin;
        preview_pos.x      = ImGui::GetCursorPosX();

        ImGui::SetCursorPos(preview_pos);

        ImGuiWindow *window            = ImGui::GetCurrentWindow();
        window->DC.CursorPosPrevLine.y = window->Pos.y + preview_pos.y;

        ImGui::BeginGroup();
        {
            for (int j = 0; j < 3; ++j) {
                for (int i = 0; i < 4; ++i) {
                    char value_buf[32] = {};
                    calcPreview(value_buf, sizeof(value_buf), address + i * 4 + j * 16, 4,
                                MetaType::F32);

                    ImGui::PushID(i);

                    ImGui::SetNextItemWidth(100.0f);
                    ImGui::InputText("##mtx34_f32_preview", value_buf, sizeof(value_buf),
                                     ImGuiInputTextFlags_ReadOnly |
                                         ImGuiInputTextFlags_AutoSelectAll);

                    if (i < 3) {
                        ImGui::SameLine();
                    }

                    ImGui::PopID();
                }
            }
        }
        ImGui::EndGroup();

        ImRect bb = {
            {preview_pos.x - style.ItemSpacing.x,     preview_pos.y                 },
            {preview_pos.x - style.ItemSpacing.x + 2, preview_pos.y + preview_size.y}
        };
        bb.Translate(window->Pos);
        window->DrawList->AddRectFilled(bb.Min, bb.Max, ImGui::GetColorU32(ImGuiCol_Separator));
    }

    void AddWatchDialog::calcPreview(char *preview_out, size_t preview_size, u32 address,
                                     size_t address_size, MetaType address_type) const {
        if (preview_out == nullptr || preview_size == 0) {
            return;
        }

        DolphinCommunicator &communicator = GUIApplication::instance().getDolphinCommunicator();
        if (!communicator.manager().isHooked()) {
            snprintf(preview_out, preview_size, "???");
            return;
        }

        const u32 start_addr = 0x80000000;
        const u32 end_addr   = start_addr | communicator.manager().getMemorySize();
        if (address < start_addr || address >= end_addr) {
            snprintf(preview_out, preview_size, "???");
            return;
        }

        if (address_size == 0) {
            snprintf(preview_out, preview_size, "???");
            return;
        }

        if (preview_size < meta_type_size(address_type)) {
            snprintf(preview_out, preview_size, "???");
            return;
        }

        void *mem_view  = communicator.manager().getMemoryView();
        size_t mem_size = communicator.manager().getMemorySize();

        u32 true_address = address & 0x1FFFFFF;
        if (true_address + address_size > mem_size) {
            snprintf(preview_out, preview_size, "???");
            return;
        }

        switch (address_type) {
        case MetaType::BOOL: {
            bool value = communicator.read<bool>(true_address).value_or(false);
            snprintf(preview_out, preview_size, "%s", value ? "true" : "false");
            break;
        }
        case MetaType::S8: {
            s8 value = communicator.read<s8>(true_address).value_or(0);
            snprintf(preview_out, preview_size, "%d", (u8)value);
            break;
        }
        case MetaType::U8: {
            u8 value = communicator.read<u8>(true_address).value_or(0);
            snprintf(preview_out, preview_size, "%u", value);
            break;
        }
        case MetaType::S16: {
            s16 value = communicator.read<s16>(true_address).value_or(0);
            snprintf(preview_out, preview_size, "%d", (u16)value);
            break;
        }
        case MetaType::U16: {
            u16 value = communicator.read<u16>(true_address).value_or(0);
            snprintf(preview_out, preview_size, "%u", value);
            break;
        }
        case MetaType::S32: {
            s32 value = communicator.read<s32>(true_address).value_or(0);
            snprintf(preview_out, preview_size, "%ld", (u32)value);
            break;
        }
        case MetaType::U32: {
            u32 value = communicator.read<u32>(true_address).value_or(0);
            snprintf(preview_out, preview_size, "%lu", value);
            break;
        }
        case MetaType::F32: {
            f32 value = communicator.read<f32>(true_address).value_or(0.0f);
            snprintf(preview_out, preview_size, "%.6f", value);
            break;
        }
        case MetaType::F64: {
            f64 value = communicator.read<f64>(true_address).value_or(0.0);
            snprintf(preview_out, preview_size, "%.6f", value);
            break;
        }
        case MetaType::STRING: {
            communicator
                .readCString(preview_out, std::min<size_t>(preview_size, address_size),
                             true_address)
                .or_else([&](const BaseError &err) {
                    snprintf(preview_out, preview_size, "Error: %s", err.m_message[0].c_str());
                    return Result<void>{};
                });
            break;
        }
        case MetaType::RGB: {
            u32 value = communicator.read<u32>(true_address).value_or(0);
            value &= 0xFFFFFF00;  // Mask to RGB only
            snprintf(preview_out, preview_size, "#%06X", value);
            break;
        }
        case MetaType::RGBA: {
            u32 value = communicator.read<u32>(true_address).value_or(0);
            value &= 0xFFFFFFFF;  // Mask to RGB only
            snprintf(preview_out, preview_size, "#%08X", value);
            break;
        }
        case MetaType::UNKNOWN: {
            size_t tmp_size   = std::min<size_t>(preview_size / 3, address_size);
            size_t final_size = std::min<size_t>(preview_size, address_size);

            char *tmp_buf = new char[tmp_size];

            communicator.readBytes(tmp_buf, true_address, tmp_size)
                .or_else([&](const BaseError &err) {
                    snprintf(preview_out, preview_size, "Error: %s", err.m_message[0].c_str());
                    return Result<void>{};
                });

            size_t i, j = 0;
            for (i = 0; i < tmp_size; ++i) {
                uint8_t ch = ((uint8_t *)tmp_buf)[i];
                snprintf(preview_out + j, preview_size - j, "%02X ", ch);
                j += 3;
            }

            if (j > 0) {
                preview_out[j - 1] = '\0';
            } else {
                preview_out[j] = '\0';
            }

            delete[] tmp_buf;
            break;
        }
        default:
            snprintf(preview_out, preview_size, "Unsupported type");
            break;
        }
    }

    Color::RGBShader AddWatchDialog::calcColorRGB(u32 address) {

        DolphinCommunicator &communicator = GUIApplication::instance().getDolphinCommunicator();
        if (!communicator.manager().isHooked()) {
            return Color::RGBShader(0.0f, 0.0f, 0.0f);
        }

        void *mem_view  = communicator.manager().getMemoryView();
        size_t mem_size = communicator.manager().getMemorySize();

        u32 true_address = address & 0x1FFFFFF;
        if (true_address + 3 > mem_size) {
            return Color::RGBShader(0.0f, 0.0f, 0.0f);
        }

        u32 value = communicator.read<u32>(true_address).value_or(0);
        Color::RGB24 rgba_color((u8)((value >> 24) & 0xFF), (u8)((value >> 16) & 0xFF),
                                (u8)((value >> 8) & 0xFF));
        f32 r, g, b, a;
        rgba_color.getColor(r, g, b, a);

        return Color::RGBShader(r, g, b);
    }

    Color::RGBAShader AddWatchDialog::calcColorRGBA(u32 address) {

        DolphinCommunicator &communicator = GUIApplication::instance().getDolphinCommunicator();
        if (!communicator.manager().isHooked()) {
            return Color::RGBAShader(0.0f, 0.0f, 0.0f, 0.0f);
        }

        void *mem_view  = communicator.manager().getMemoryView();
        size_t mem_size = communicator.manager().getMemorySize();

        u32 true_address = address & 0x1FFFFFF;
        if (true_address + 4 > mem_size) {
            return Color::RGBAShader(0.0f, 0.0f, 0.0f, 0.0f);
        }

        u32 value = communicator.read<u32>(true_address).value_or(0);
        Color::RGBA32 rgba_color((u8)((value >> 24) & 0xFF), (u8)((value >> 16) & 0xFF),
                                 (u8)((value >> 8) & 0xFF), (u8)(value & 0xFF));
        f32 r, g, b, a;
        rgba_color.getColor(r, g, b, a);

        return Color::RGBAShader(r, g, b, a);
    }

}  // namespace Toolbox::UI