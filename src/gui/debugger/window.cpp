#include "core/input/keycode.hpp"

#include "gui/application.hpp"
#include "gui/debugger/window.hpp"
#include "gui/dragdrop/dragdropmanager.hpp"
#include "gui/logging/errors.hpp"
#include "gui/new_item/window.hpp"
#include "model/fsmodel.hpp"

#include <cctype>
#include <cmath>
#include <imgui/imgui.h>

#include "gui/imgui_ext.hpp"

template <typename _T> constexpr static _T OverwriteNibble(_T value, u8 nibble_idx, u8 nibble_val) {
    static_assert(std::is_integral_v<_T>, "_T must be a basic integral type");

    constexpr size_t byte_width   = sizeof(_T);
    constexpr size_t nibble_width = byte_width * 2;

    if (nibble_idx >= nibble_width) {
        return value;
    }

    nibble_val &= 0b1111;

    _T nibble_idx_big = ((nibble_width - 1) - nibble_idx);
    _T nibble_mask    = static_cast<_T>(0b1111) << (nibble_idx_big * 4);
    _T new_value = (value & ~nibble_mask) | (static_cast<_T>(nibble_val) << (nibble_idx_big * 4));

    return new_value;
}

namespace Toolbox::UI {

    DebuggerWindow::DebuggerWindow(const std::string &name) : ImWindow(name), m_address_input() {}

    std::string DebuggerWindow::context() const {
        if (m_attached_scene_uuid == 0) {
            return "MRAM";
        }

        RefPtr<ImWindow> scene_window =
            GUIApplication::instance().findWindow(m_attached_scene_uuid);
        if (!scene_window) {
            return "MRAM";
        }

        return std::format("MRAM ({})", scene_window->context());
    }

    void DebuggerWindow::onRenderMenuBar() {}

    void DebuggerWindow::onRenderBody(TimeStep delta_time) {
        const float splitter_width = 6.0f;

        m_delta_time = delta_time;

        ImVec2 avail_region = ImGui::GetContentRegionAvail();
        ImVec2 min_size     = minSize().value_or(ImVec2{200, 200});

        const ImGuiStyle &style = ImGui::GetStyle();

        if (!m_initialized_splitter_widths) {
            m_initialized_splitter_widths = true;

            m_list_width = std::max(min_size.x * 0.25f, avail_region.x * 0.25f);
            m_view_width = std::max(min_size.x * 0.75f, avail_region.x * 0.75f);
        }

        float last_total_width = m_list_width + m_view_width;

        ImGuiID splitter_id = ImGui::GetID("##MemorySplitter");
        ImGui::SplitterBehavior(splitter_id, ImGuiAxis_X, splitter_width, &m_list_width,
                                &m_view_width, min_size.x * 0.1f, min_size.x * 0.7f);

        m_view_width = (m_view_width / last_total_width) * (avail_region.x - splitter_width);
        m_view_width = std::max(m_view_width, min_size.x * 0.7f);

        m_list_width = (m_list_width / last_total_width) * (avail_region.x - splitter_width);
        m_list_width = std::max(m_list_width, min_size.x * 0.1f);

        renderMemoryWatchList();

        ImGui::SameLine(0);
        // ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::InvisibleButton("##MemorySplitter", ImVec2(splitter_width, avail_region.y));

        ImGuiWindow *window    = ImGui::GetCurrentWindow();
        window->DC.CursorPos.x = window->DC.CursorPosPrevLine.x - splitter_width;
        window->DC.CursorPos.y = window->DC.CursorPosPrevLine.y;

        // Disable group padding

        // ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, style.WindowPadding.y});

        ImGuiChildFlags flags = ImGuiChildFlags_AlwaysAutoResize | ImGuiChildFlags_AutoResizeX |
                                ImGuiChildFlags_AutoResizeY;
        if (ImGui::BeginChild("##MemoryViewGroup", {0, 0}, ImGuiChildFlags_Borders,
                              ImGuiWindowFlags_ChildWindow | ImGuiWindowFlags_NoDecoration)) {
            renderMemoryAddressBar();
            ImGui::Separator();
            renderMemoryView();
        }
        ImGui::EndChild();

        // ImGui::PopStyleVar();

        ModelIndex parent_index = m_proxy_model->getParent(m_last_selected_index);
        int64_t the_row         = m_proxy_model->getRow(m_last_selected_index);

        m_add_group_dialog.render(m_last_selected_index, the_row);
        m_add_watch_dialog.render(m_last_selected_index, the_row);

        m_did_drag_drop = DragDropManager::instance().getCurrentDragAction() != nullptr;
    }

    static u32 calculateBytesForRow(u32 byte_limit, u8 column_count, u8 byte_width) {
        // Calculate the number of bytes that can be rendered in a row
        // ---
        // If byte_limit is less than column_count * byte_width, we can only render up to byte_limit
        // bytes.
        u32 bytes_per_row = std::min<u32>(byte_limit, column_count * byte_width);
        return bytes_per_row;
    }

    u32 DebuggerWindow::renderMemoryRow(void *handle, u32 base_address, u32 byte_limit,
                                        u8 column_count, u8 byte_width) {
        ImGuiStyle &style   = ImGui::GetStyle();
        ImGuiWindow *window = ImGui::GetCurrentWindow();

        const bool window_focused = ImGui::IsWindowFocused();
        const float font_width    = ImGui::GetFontSize();
        float ch_width            = ImGui::CalcTextSize("0").x;

        // Render the address
        // ---
        ImGui::Text("0x%08X", base_address);
        ImGui::SameLine();

        byte_width              = std::min<u8>(byte_width, 4);
        const char formatter[8] = {'%', '0', '0' + byte_width * 2, 'X', '\0'};

        u64 value = 0;

        void *mem_ptr = handle;
        u32 b_limit   = byte_limit;

        // Calculate the selection quad for this row
        u32 base_address_end = base_address + column_count * byte_width;

        u32 selection_start = std::min<u32>(m_address_selection_begin, m_address_selection_end);
        u32 selection_end   = std::max<u32>(m_address_selection_begin, m_address_selection_end);

        if (selection_start < base_address_end && selection_end > base_address &&
            selection_start != 0 && selection_end != 0) {
            u32 addr_beg = std::max<u32>(selection_start, base_address);
            u32 addr_end = std::min<u32>(selection_end, base_address_end);

            u8 col_start = (addr_beg - base_address) / byte_width;
            u8 col_end   = (addr_end - base_address) / byte_width;

            if (col_start < col_end) {
                char spoof_buf[16];
                snprintf(spoof_buf, sizeof(spoof_buf), formatter, 0);

                ImVec2 text_size  = ImGui::CalcTextSize(spoof_buf);

                ImRect text_rect = {};
                text_rect.Min =
                    ImGui::GetWindowPos() + ImGui::GetCursorPos() - style.ItemSpacing / 2;
                text_rect.Min.x += (style.ItemSpacing.x + text_size.x) * col_start;
                text_rect.Max = text_rect.Min;
                text_rect.Max.x += (style.ItemSpacing.x + text_size.x) * (col_end - col_start);
                text_rect.Max.y += style.ItemSpacing.y + text_size.y;

                window->DrawList->AddQuadFilled(text_rect.GetTL(), text_rect.GetTR(),
                                                text_rect.GetBR(), text_rect.GetBL(),
                                                ImGui::GetColorU32(ImGuiCol_TabSelected));
            }
        }

        // Render each byte group in the row
        for (u8 column = 0; column < column_count; ++column) {
            ImGui::SameLine();
            value = std::byteswap(*((u64 *)mem_ptr));

            u32 cur_address = base_address + column * byte_width;

            // Calculate a big-endian mask
            // ---
            // Examples
            // ---
            // byte_width = 1 -> 0x00FFFFFF'FFFFFFFF -> 0xFF000000'00000000
            // byte_width = 2 -> 0x0000FFFF'FFFFFFFF -> 0xFFFF0000'00000000
            // ...
            // ---
            u32 bit_width = std::min<u32>(byte_width, byte_limit) * 8;
            u64 mask      = ~(0xFFFFFFFF'FFFFFFFF >> bit_width);

            // Calulate the rect of the text for interaction purposes.
            char text_buf[64];
            snprintf(text_buf, sizeof(text_buf), formatter, (value & mask) >> (64 - bit_width));

            ImRect text_rect = {};
            text_rect.Min = ImGui::GetWindowPos() + ImGui::GetCursorPos() - style.ItemSpacing / 2;
            text_rect.Max = text_rect.Min + ImGui::CalcTextSize(text_buf) + style.ItemSpacing;

            double m_x, m_y;
            Input::GetMousePosition(m_x, m_y);

            ImVec2 mouse_pos = ImVec2{(float)m_x, (float)m_y};

            bool column_hovered = text_rect.ContainsWithPad(mouse_pos, style.TouchExtraPadding);
            if (column_hovered) {
                window->DrawList->AddRectFilled(text_rect.Min, text_rect.Max,
                                                ImGui::GetColorU32(ImGuiCol_TabHovered));
            }

            bool column_clicked =
                column_hovered && Input::GetMouseButtonDown(MouseButton::BUTTON_LEFT);
            if (column_clicked) {
                m_address_selection_new         = true;
                m_address_selection_mouse_start = mouse_pos;

                m_address_selection_begin = cur_address;
                m_address_selection_end   = cur_address;

                m_address_cursor = cur_address;
                m_address_cursor_nibble =
                    ImClamp<u8>((mouse_pos.x - text_rect.Min.x) / ch_width, 0, byte_width * 2 - 1);

                m_cursor_anim_timer = -0.3f;
            } else if (!Input::GetMouseButton(MouseButton::BUTTON_LEFT)) {
                m_address_selection_new = false;
            }

            // In this case we check for selection dragging
            if (m_address_selection_new && Input::GetMouseButton(MouseButton::BUTTON_LEFT) &&
                column_hovered) {
                ImVec2 difference = mouse_pos - m_address_selection_mouse_start;
                if (ImLengthSqr(difference) > 100.0f) {
                    m_address_selection_end = cur_address + byte_width;
                }
            }

            ImGui::Text(text_buf);

            if (m_address_cursor == cur_address) {
                m_cursor_anim_timer += m_delta_time;
                bool cursor_visible = (m_cursor_anim_timer <= 0.0f) ||
                                      ImFmod(m_cursor_anim_timer, 1.20f) <= 0.80f;
                if (cursor_visible) {
                    ImVec2 tl = text_rect.GetTL();
                    ImVec2 bl = text_rect.GetBL();
                    tl.x += ch_width * m_address_cursor_nibble + style.ItemSpacing.x / 2 - 1;
                    bl.x += ch_width * m_address_cursor_nibble + style.ItemSpacing.x / 2 - 1;
                    window->DrawList->AddLine(tl, bl, 0xA03030FF, 2.0f);
                }
            }

            mem_ptr = (u8 *)mem_ptr + byte_width;
            b_limit -= byte_width;
        }

        mem_ptr = handle;
        b_limit = byte_limit;

        ImGui::SameLine();

        u32 char_width  = calculateBytesForRow(b_limit, column_count, byte_width);
        char *ascii_buf = new char[char_width];
        for (int i = 0; i < char_width; ++i) {
            int ch = ((char *)mem_ptr)[i];
            ch &= 0xFF;
            if (std::isprint(ch)) {
                ascii_buf[i] = ch;
            } else {
                ascii_buf[i] = '.';
            }
        }

        ImGui::TextEx(ascii_buf, ascii_buf + char_width);
        delete[] ascii_buf;

        return char_width;
    }

    static float predictColumnWidth(u8 byte_width) {
        byte_width              = std::min<u8>(byte_width, 4);
        const char formatter[8] = {'%', '0', '0' + byte_width * 2, 'X', '\0'};
        char buffer[64];

        // Calculate the size of the text that would be rendered
        // ---
        snprintf(buffer, sizeof(buffer), formatter, 0);
        return ImGui::CalcTextSize(buffer).x;
    }

    void DebuggerWindow::renderMemoryAddressBar() {
        if (ImGui::BeginChild("##MemoryAddressBar", {0, 0},
                              ImGuiChildFlags_AlwaysAutoResize | ImGuiChildFlags_AutoResizeY,
                              ImGuiWindowFlags_ChildWindow | ImGuiWindowFlags_NoDecoration)) {
            ImVec2 window_size = ImGui::GetWindowSize();
            ImGuiStyle &style  = ImGui::GetStyle();

            ImGui::Text("Address: ");
            ImGui::SameLine();

            const char **history_strs = new const char *[m_address_search_history.size()];
            for (size_t i = 0; i < m_address_search_history.size(); ++i) {
                history_strs[i] = m_address_search_history[i].m_label.c_str();
            }

            float address_field_width = window_size.x;
            address_field_width -= ImGui::CalcTextSize("Address: ").x + style.ItemSpacing.x;
            address_field_width -= ImGui::CalcTextSize("Bytes Per Row: ").x + style.ItemSpacing.x;
            address_field_width -= 100 + style.ItemSpacing.x;
            address_field_width -= ImGui::CalcTextSize("Bytes Width: ").x + style.ItemSpacing.x;
            address_field_width -= 100 + style.ItemSpacing.x;

            ImGui::SetNextItemWidth(address_field_width);

            // Render the address input combo box
            if (ImGui::InputComboTextBox(
                    "##AddressComboBox", m_address_input.data(), m_address_input.size(),
                    history_strs, m_address_search_history.size(), nullptr, ImGuiComboFlags_None,
                    ImGuiInputTextFlags_EnterReturnsTrue)) {
                // Parse the input address
                std::string input_str(m_address_input.data());
                u32 input_address = 0;
                if (input_str.starts_with("0x")) {
                    input_str = input_str.substr(2);
                }

                if (input_str.length() == 8 &&
                    std::all_of(input_str.begin(), input_str.end(),
                                [](char c) { return std::isxdigit(c); })) {
                    input_address = std::stoul(input_str, nullptr, 16);
                } else {
                    ImGui::SetTooltip("Invalid address format. Use 0xXXXXXXXX format.");
                    ImGui::EndChild();
                    return;
                }

                m_address_search_history.push_back({input_address, input_str});
                m_base_address = input_address;
            }

            ImGui::SameLine();
            ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
            ImGui::SameLine();

            // This is where the column width selector will be rendered
            // ---
            {
                ImGui::Text("Bytes Per Row: ");
                ImGui::SameLine();

                static const char *column_width_presets[] = {"Auto", "1",  "2",  "4",
                                                             "8",    "16", "32", "64"};
                const size_t column_presets               = IM_ARRAYSIZE(column_width_presets);

                if (m_column_count_idx >= column_presets) {
                    m_column_count_idx = 0;  // Reset to auto if out of bounds
                }

                const char *selected_column_width = column_width_presets[m_column_count_idx];

                ImGui::SetNextItemWidth(100);
                if (ImGui::BeginCombo("##ColumnWidthCombo", selected_column_width)) {
                    for (size_t i = 0; i < column_presets; ++i) {
                        if (ImGui::Selectable(column_width_presets[i], m_column_count_idx == i)) {
                            m_column_count_idx = i;
                        }
                    }
                    ImGui::EndCombo();
                }
            }

            ImGui::SameLine();

            // This is where the byte width selector will be rendered
            // ---
            {
                ImGui::Text("Byte Width: ");
                ImGui::SameLine();

                static const char *byte_width_presets[] = {"1", "2", "4"};
                const size_t byte_presets               = IM_ARRAYSIZE(byte_width_presets);

                if (m_byte_width_idx >= byte_presets) {
                    m_byte_width_idx = 0;  // Reset to 1 byte if out of bounds
                }

                const char *selected_byte_width = byte_width_presets[m_byte_width_idx];

                ImGui::SetNextItemWidth(100);
                if (ImGui::BeginCombo("##ByteWidthCombo", selected_byte_width)) {
                    for (size_t i = 0; i < byte_presets; ++i) {
                        if (ImGui::Selectable(byte_width_presets[i], m_byte_width_idx == i)) {
                            m_byte_width_idx = i;
                            m_byte_width     = 1 << i;
                        }
                    }
                    ImGui::EndCombo();
                }
            }
        }
        ImGui::EndChild();
    }

    void DebuggerWindow::renderMemoryView() {
        if (ImGui::BeginChild("##MemoryView", {m_view_width, 0}, ImGuiChildFlags_None,
                              ImGuiWindowFlags_ChildWindow | ImGuiWindowFlags_NoDecoration)) {
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {2, 2});
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {2, 2});
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 3.0f);

            ImFont *mono_font = GUIApplication::instance().getFontManager().getFont(
                "NanumGothicCoding-Bold", 12.0f);
            if (mono_font) {
                ImGui::PushFont(mono_font);
            }

            // Calculate the rows and columns based on
            // the set configuration and the width/height
            // ---
            const ImGuiStyle &style = ImGui::GetStyle();
            ImVec2 size             = ImGui::GetContentRegionAvail();

            u32 row_count = (u32)(size.y / ImGui::GetTextLineHeightWithSpacing() + 0.5f);
            if (row_count == 0) {
                row_count = 1;
            }

            u32 column_count = 0;
            if (m_column_count_idx == 0) {
                // Auto column count
                float address_text_width = ImGui::CalcTextSize("0x00000000").x;

                float full_col_width    = (style.ItemSpacing.x + predictColumnWidth(m_byte_width));
                float address_col_width = (style.ItemSpacing.x + address_text_width);
                float predicted_ascii_width = 0.0f;

                float factor = 0.759f;
                float adjust = -0.5f;

                int k = m_byte_width;
                while (k >> 1 != 0) {
                    factor -= 0.036f;
                    adjust += 0.3f;
                    k >>= 1;
                }

                float column_count_pred =
                    std::max<float>(size.x - address_col_width, 0.0f) / full_col_width + adjust;

                column_count = (column_count_pred * factor);  // Give space for ascii column
                if (column_count == 0) {
                    column_count = 1;
                }
            } else {
                column_count = 1 << (m_column_count_idx - 1);  // |Auto|, 1, 2, 4, 8, 16, 32, 64
            }

            // Adjust columns based on suggested byte width
            // column_count /= m_byte_width;

            DolphinHookManager &manager = DolphinHookManager::instance();
            void *memory_buffer         = manager.getMemoryView();
            if (!memory_buffer) {
                ImGui::Text("Failed to get memory view.");
                ImGui::PopStyleVar(5);

                if (mono_font) {
                    ImGui::PopFont();
                }

                ImGui::EndChild();
                return;
            }

            u32 memory_size    = manager.getMemorySize();
            u32 memory_address = m_base_address;

            auto advanceCursor = [this]() {
                if (m_address_cursor_nibble + 1 >= m_byte_width * 2) {
                    if (m_address_cursor + m_byte_width <= 0x81800000 - m_byte_width) {
                        m_address_cursor += m_byte_width;
                        m_address_cursor_nibble = 0;
                    }
                } else {
                    m_address_cursor_nibble += 1;
                }
            };

            auto reverseCursor = [this]() {
                if (m_address_cursor_nibble == 0) {
                    if (m_address_cursor - m_byte_width >= 0x80000000) {
                        m_address_cursor -= m_byte_width;
                        m_address_cursor_nibble = m_byte_width * 2 - 1;
                    }
                } else {
                    m_address_cursor_nibble -= 1;
                }
            };

            auto advanceCursorLine = [this, column_count]() {
                u32 desired_address_cursor = m_address_cursor + m_byte_width * column_count;
                if (desired_address_cursor < 0x81800000 - m_byte_width) {
                    m_address_cursor = desired_address_cursor;
                }
            };

            auto reverseCursorLine = [this, column_count]() {
                u32 desired_address_cursor = m_address_cursor - m_byte_width * column_count;
                if (desired_address_cursor >= 0x80000000) {
                    m_address_cursor = desired_address_cursor;
                }
            };

            // Process controls
            if (m_address_cursor != 0) {
                bool cursor_moves = m_cursor_step_timer > 0.10f || m_cursor_step_timer == -0.3f;

                bool key_held = false;

                KeyCodes pressed_keys = Input::GetPressedKeys();
                for (const KeyCode &key : pressed_keys) {
                    if (key >= KeyCode::KEY_D0 && key <= KeyCode::KEY_D9) {
                        key_held = true;
                        if (!cursor_moves) {
                            continue;
                        }
                        u8 nibble_value = (u8)key - (u8)KeyCode::KEY_D0;
                        overwriteNibbleAtCursor(nibble_value);
                        advanceCursor();
                    } else if (key >= KeyCode::KEY_A && key <= KeyCode::KEY_F) {
                        key_held = true;
                        if (!cursor_moves) {
                            continue;
                        }
                        u8 nibble_value = ((u8)key - (u8)KeyCode::KEY_A) + 10;
                        overwriteNibbleAtCursor(nibble_value);
                        advanceCursor();
                    } else if (key == KeyCode::KEY_RIGHT) {
                        key_held = true;
                        if (!cursor_moves) {
                            continue;
                        }
                        advanceCursor();
                    } else if (key == KeyCode::KEY_LEFT) {
                        key_held = true;
                        if (!cursor_moves) {
                            continue;
                        }
                        reverseCursor();
                    } else if (key == KeyCode::KEY_DOWN) {
                        key_held = true;
                        if (!cursor_moves) {
                            continue;
                        }
                        advanceCursorLine();
                    } else if (key == KeyCode::KEY_UP) {
                        key_held = true;
                        if (!cursor_moves) {
                            continue;
                        }
                        reverseCursorLine();
                    }
                }

                if (key_held) {
                    if (cursor_moves && m_cursor_step_timer > 0.0f) {
                        m_cursor_step_timer = m_delta_time;
                    } else {
                        m_cursor_step_timer += m_delta_time;
                    }
                } else {
                    m_cursor_step_timer = -0.3f;
                }
            }

            for (u32 row = 0; row < row_count; ++row) {
                ImGui::PushID(row);

                u32 true_address = (memory_address & 0x1FFFFFF);
                u32 byte_limit   = memory_size > true_address ? memory_size - true_address : 0;
                if (byte_limit == 0) {
                    ImGui::PopID();
                    break;
                }

                void *mem_ptr           = (u8 *)memory_buffer + true_address;
                u32 suggested_increment = renderMemoryRow(mem_ptr, memory_address, byte_limit,
                                                          column_count, m_byte_width);
                memory_address += suggested_increment;

                ImGui::PopID();
            }

            if (mono_font) {
                ImGui::PopFont();
            }

            ImGui::PopStyleVar(5);

            // Check for memory view hover
            if (ImGui::IsWindowHovered()) {
                double x, y;
                Input::GetMouseScrollDelta(x, y);

                u32 true_address = (memory_address & 0x1FFFFFF);
                u32 byte_limit   = memory_size > true_address ? memory_size - true_address : 0;

                // When the scroll is greater than our epsilon
                // we want to update the base address
                if (y > 0.1f) {
                    m_base_address -= calculateBytesForRow(byte_limit, column_count, m_byte_width);
                    m_base_address = std::max<u32>(0x80000000, m_base_address);
                } else if (y < -0.1f) {
                    m_base_address += calculateBytesForRow(byte_limit, column_count, m_byte_width);
                    m_base_address = std::min<u32>(0x80000000 + memory_size, m_base_address);
                }
            }
        }
        ImGui::EndChild();
    }

    void DebuggerWindow::renderMemoryWatchList() {
        m_any_row_clicked            = false;
        bool any_interactive_clicked = false;

        if (ImGui::BeginChild("##MemoryWatchList", {m_list_width, 0}, true,
                              ImGuiWindowFlags_ChildWindow | ImGuiWindowFlags_NoDecoration)) {
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {2, 2});
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {2, 2});
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 3.0f);

            ImGuiStyle &style           = ImGui::GetStyle();
            ImVec2 avail_content_region = ImGui::GetContentRegionAvail();

            float group_button_width =
                (avail_content_region.x - style.FramePadding.x - style.ItemSpacing.x) * 0.5f;
            float watch_button_width = group_button_width;

            if (ImGui::Button("Add Group", {group_button_width, 0.0f}, 5.0f,
                              ImDrawFlags_RoundCornersAll)) {
                m_add_group_dialog.open();
                any_interactive_clicked = true;
            }

            ImGui::SameLine();

            if (ImGui::Button("Add Watch", {watch_button_width, 0.0f}, 5.0f,
                              ImDrawFlags_RoundCornersAll)) {
                m_add_watch_dialog.open();
                any_interactive_clicked = true;
            }

            ImGui::Separator();

            // Name | Type | Address | Lock | Value
            // ------------------------------------
            // ---
            const int columns = 5;
            const ImGuiTableFlags flags =
                ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable | ImGuiTableFlags_ScrollY;
            const ImVec2 desired_size = ImGui::GetContentRegionAvail();

            ImVec2 table_pos  = ImGui::GetWindowPos() + ImGui::GetCursorPos(),
                   table_size = desired_size;
            if (ImGui::BeginTable("##MemoryWatchTable", 5, flags, desired_size)) {

                // ImGui::TableSetupScrollFreeze(3, 1);
                ImGui::TableSetupColumn("Name");
                ImGui::TableSetupColumn("Type");
                ImGui::TableSetupColumn("Address", ImGuiTableColumnFlags_WidthFixed, 80.0f);
                ImGui::TableSetupColumn("Lock", ImGuiTableColumnFlags_WidthFixed, 30.0f);
                ImGui::TableSetupColumn("Value");

                float table_start_x = ImGui::GetCursorPosX() + ImGui::GetWindowPos().x;
                float table_width   = ImGui::GetContentRegionAvail().x;

                ImGui::TableHeadersRow();

                int64_t row_count = m_proxy_model->getRowCount(ModelIndex());
                for (int64_t row = 0; row < row_count; ++row) {
                    ModelIndex index = m_proxy_model->getIndex(row, 0);
                    if (m_proxy_model->isIndexGroup(index)) {
                        renderWatchGroup(index, 0, table_start_x, table_width);
                    } else {
                        renderMemoryWatch(index, 0, table_start_x, table_width);
                    }
                }

                ImGui::EndTable();
            }

            ImGui::PopStyleVar(5);

            bool left_click  = Input::GetMouseButtonDown(Input::MouseButton::BUTTON_LEFT);
            bool right_click = Input::GetMouseButtonDown(Input::MouseButton::BUTTON_RIGHT);

            ImRect window_rect  = ImRect{table_pos, table_size};
            bool mouse_captured = ImGui::IsMouseHoveringRect(window_rect.Min, window_rect.Max);

            if (!m_add_group_dialog.is_open() && !m_add_watch_dialog.is_open()) {
                if (!m_any_row_clicked && mouse_captured && (left_click || right_click) &&
                    Input::GetPressedKeyModifiers() == KeyModifier::KEY_NONE) {
                    m_selection.clearSelection();
                    m_selection_ctx.clearSelection();
                    m_last_selected_index = ModelIndex();
                }
            }
        }
        ImGui::EndChild();
    }

    void DebuggerWindow::renderMemoryWatch(const ModelIndex &watch_idx, int depth,
                                           float table_start_x, float table_width) {
        if (!m_proxy_model->validateIndex(watch_idx)) {
            return;
        }

        std::string name = m_proxy_model->getDisplayText(watch_idx);
        if (name.empty()) {
            return;  // Skip empty groups
        }

        DolphinHookManager &manager = DolphinHookManager::instance();
        void *mem_view              = manager.getMemoryView();
        size_t mem_size             = manager.getMemorySize();

        MetaValue meta_value = m_proxy_model->getWatchValueMeta(watch_idx);
        u32 address          = m_proxy_model->getWatchAddress(watch_idx);
        u32 size             = m_proxy_model->getWatchSize(watch_idx);
        bool locked          = m_proxy_model->getWatchLock(watch_idx);

        const ImGuiStyle &style = ImGui::GetStyle();
        ImGuiWindow *window     = ImGui::GetCurrentWindow();

        ImVec2 text_size = ImGui::CalcTextSize(name.c_str(), nullptr, true);

        ImVec2 mouse_pos;
        {
            double mouse_x, mouse_y;
            Input::GetMousePosition(mouse_x, mouse_y);
            mouse_pos.x = mouse_x;
            mouse_pos.y = mouse_y;
        }

        bool is_left_click         = Input::GetMouseButtonDown(MouseButton::BUTTON_LEFT);
        bool is_left_click_release = Input::GetMouseButtonUp(MouseButton::BUTTON_LEFT);
        bool is_left_drag          = Input::GetMouseButton(MouseButton::BUTTON_LEFT);

        if (!is_left_drag) {
            m_last_reg_mouse_pos = mouse_pos;
        }

        bool is_double_left_click   = ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
        bool is_right_click         = Input::GetMouseButtonDown(MouseButton::BUTTON_RIGHT);
        bool is_right_click_release = Input::GetMouseButtonUp(MouseButton::BUTTON_RIGHT);

        bool is_selected = m_selection.is_selected(watch_idx);

        ImGui::TableNextRow();

        ImVec2 row_button_pos;
        ImVec2 row_button_size = {text_size.y, text_size.y};
        ImRect row_button_bb;

        // Establish row metrics for rendering selection box
        float row_width  = table_width;
        float row_height = text_size.y + style.ItemSpacing.y * 2;

        ImRect row_rect = {};
        row_rect.Min    = ImVec2{table_start_x, ImGui::GetCursorPosY() + ImGui::GetWindowPos().y};
        row_rect.Max    = row_rect.Min + ImVec2{row_width, row_height};

        bool is_rect_hovered = false;
        {
            ImGuiContext &g = *GImGui;

            double m_x, m_y;
            Input::GetMousePosition(m_x, m_y);

            ImVec2 mouse_pos = ImVec2{(float)m_x, (float)m_y};

            // Clip
            ImRect rect_clipped(row_rect.Min, row_rect.Max);

            // Hit testing, expanded for touch input
            if (rect_clipped.ContainsWithPad(mouse_pos, g.Style.TouchExtraPadding)) {
                if (g.MouseViewport->GetMainRect().Overlaps(rect_clipped))
                    is_rect_hovered = true;
            }
        }

        bool is_rect_clicked =
            is_rect_hovered && (Input::GetMouseButtonDown(Input::MouseButton::BUTTON_LEFT) ||
                                Input::GetMouseButtonDown(Input::MouseButton::BUTTON_RIGHT));

        bool is_collapse_hovered = false;
        bool is_collapse_held    = false;
        bool is_collapse_pressed = false;
        bool is_lock_hovered     = false;
        bool is_lock_pressed     = false;

        ImDrawFlags backdrop_flags = ImDrawFlags_None;

        ImVec4 selected_col = style.Colors[ImGuiCol_TabActive];
        ImVec4 hovered_col  = style.Colors[ImGuiCol_TabHovered];

        selected_col.w = 0.5;
        hovered_col.w  = 0.5;

        if (is_rect_hovered) {
            ImGui::RenderFrame(row_rect.Min, row_rect.Max,
                               ImGui::ColorConvertFloat4ToU32(hovered_col), false, 0.0f,
                               backdrop_flags);
        } else if (is_selected) {
            ImGui::RenderFrame(row_rect.Min, row_rect.Max,
                               ImGui::ColorConvertFloat4ToU32(selected_col), false, 0.0f,
                               backdrop_flags);
        }

        bool any_items_hovered = false;

        ModelIndex src_idx  = m_proxy_model->toSourceIndex(watch_idx);
        std::string qual_id = buildQualifiedId(src_idx);

        ImGuiID watch_id = ImGui::GetID(qual_id.c_str());
        ImGui::PushID(watch_id);

        if (ImGui::TableNextColumn()) {
            ImGui::Dummy({style.ItemSpacing.x * depth * 2, style.ItemSpacing.y * 2});
            ImGui::SameLine();
            ImGui::Text(name.c_str());
        }

        // ImGui::PopStyleColor(3);

        if (ImGui::TableNextColumn()) {
            if (meta_value.type() == MetaType::UNKNOWN) {
                ImGui::Text("bytes [%lu]", size);
            } else if (meta_value.type() == MetaType::STRING) {
                std::string meta_name = std::string(meta_type_name(meta_value.type()));
                ImGui::Text("%s [%lu]", meta_name.c_str(), size);
            } else {
                std::string meta_name = std::string(meta_type_name(meta_value.type()));
                ImGui::Text(meta_name.c_str());
            }
        }

        if (ImGui::TableNextColumn()) {
            ImGui::Text("0x%X", address);
        }

        ModelIndex source_group_idx = m_proxy_model->toSourceIndex(watch_idx);

        if (ImGui::TableNextColumn()) {
            is_lock_pressed = ImGui::Checkbox("##lockbox", &locked);
            if (is_lock_pressed) {
                recursiveLock(source_group_idx, locked);
            }

            is_lock_hovered = ImGui::IsItemHovered();
        }

        if (ImGui::TableNextColumn()) {
            u32 true_address = address & 0x1FFFFFF;
            u32 watch_size   = size;

            if (true_address + watch_size > mem_size) {
                ImGui::Text("Invalid Watch");
            } else {
                if (meta_value.type() == MetaType::UNKNOWN) {
                    ImVec2 avail_region = ImGui::GetContentRegionAvail();
                    float local_pos_x   = 0.0f;

                    float byte_render_width = ImGui::CalcTextSize("00").x;
                    float byte_trail_width  = ImGui::CalcTextSize("...").x;

                    u8 *watch_view = (u8 *)mem_view + true_address;
                    while (local_pos_x + byte_render_width + byte_trail_width < avail_region.x) {
                        for (int i = 0; i < watch_size; ++i) {
                            ImGui::Text("%02X", watch_view[i]);
                            if (i < watch_size - 1) {
                                ImGui::SameLine();
                            }
                        }
                        local_pos_x += byte_render_width + byte_trail_width;
                    }
                } else {
                    f32 column_width = ImGui::GetContentRegionAvail().x;
                    renderPreview(column_width, meta_value);
                }
            }
        }

        // Handle click responses
        {
            if (is_rect_clicked && !is_collapse_hovered && !is_lock_hovered) {
                m_any_row_clicked = true;
                if ((is_left_click && !is_left_click_release) ||
                    (is_right_click && !is_right_click_release)) {
                    actionSelectIndex(watch_idx);
                } else if (is_left_click_release || is_right_click_release) {
                    actionClearRequestExcIndex(watch_idx, is_left_click_release);
                }
            }
        }

        ImGui::PopID();
    }

    void DebuggerWindow::renderWatchGroup(const ModelIndex &group_idx, int depth,
                                          float table_start_x, float table_width) {
        if (!m_proxy_model->validateIndex(group_idx)) {
            return;
        }

        bool open = false;

        std::string name = m_proxy_model->getDisplayText(group_idx);
        if (name.empty()) {
            return;  // Skip empty groups
        }

        bool locked = m_proxy_model->getWatchLock(group_idx);

        const ImGuiStyle &style = ImGui::GetStyle();
        ImGuiWindow *window     = ImGui::GetCurrentWindow();

        ImVec2 text_size = ImGui::CalcTextSize(name.c_str(), nullptr, true);

        ImVec2 mouse_pos;
        {
            double mouse_x, mouse_y;
            Input::GetMousePosition(mouse_x, mouse_y);
            mouse_pos.x = mouse_x;
            mouse_pos.y = mouse_y;
        }

        bool is_left_click         = Input::GetMouseButtonDown(MouseButton::BUTTON_LEFT);
        bool is_left_click_release = Input::GetMouseButtonUp(MouseButton::BUTTON_LEFT);
        bool is_left_drag          = Input::GetMouseButton(MouseButton::BUTTON_LEFT);

        if (!is_left_drag) {
            m_last_reg_mouse_pos = mouse_pos;
        }

        bool is_double_left_click   = ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
        bool is_right_click         = Input::GetMouseButtonDown(MouseButton::BUTTON_RIGHT);
        bool is_right_click_release = Input::GetMouseButtonUp(MouseButton::BUTTON_RIGHT);

        bool is_selected = m_selection.is_selected(group_idx);

        ImGui::TableNextRow();

        ImVec2 row_button_pos;
        ImVec2 row_button_size = {text_size.y, text_size.y};
        ImRect row_button_bb;

        // Establish row metrics for rendering selection box
        float row_width  = table_width;
        float row_height = text_size.y + style.ItemSpacing.y * 2;

        ImRect row_rect = {};
        row_rect.Min    = ImVec2{table_start_x, ImGui::GetCursorPosY() + ImGui::GetWindowPos().y};
        row_rect.Max    = row_rect.Min + ImVec2{row_width, row_height};

        bool is_rect_hovered = false;
        {
            ImGuiContext &g = *GImGui;

            double m_x, m_y;
            Input::GetMousePosition(m_x, m_y);

            ImVec2 mouse_pos = ImVec2{(float)m_x, (float)m_y};

            // Clip
            ImRect rect_clipped(row_rect.Min, row_rect.Max);

            // Hit testing, expanded for touch input
            if (rect_clipped.ContainsWithPad(mouse_pos, style.TouchExtraPadding)) {
                if (g.MouseViewport->GetMainRect().Overlaps(rect_clipped))
                    is_rect_hovered = true;
            }
        }

        bool is_rect_clicked =
            is_rect_hovered && (Input::GetMouseButtonDown(Input::MouseButton::BUTTON_LEFT) ||
                                Input::GetMouseButtonDown(Input::MouseButton::BUTTON_RIGHT));

        bool is_collapse_hovered = false;
        bool is_collapse_held    = false;
        bool is_collapse_pressed = false;
        bool is_lock_hovered     = false;
        bool is_lock_pressed     = false;

        ImDrawFlags backdrop_flags = ImDrawFlags_None;

        ImVec4 selected_col = style.Colors[ImGuiCol_TabActive];
        ImVec4 hovered_col  = style.Colors[ImGuiCol_TabHovered];

        selected_col.w = 0.5;
        hovered_col.w  = 0.5;

        if (is_rect_hovered) {
            ImGui::RenderFrame(row_rect.Min, row_rect.Max,
                               ImGui::ColorConvertFloat4ToU32(hovered_col), false, 0.0f,
                               backdrop_flags);
        } else if (is_selected) {
            ImGui::RenderFrame(row_rect.Min, row_rect.Max,
                               ImGui::ColorConvertFloat4ToU32(selected_col), false, 0.0f,
                               backdrop_flags);
        }

        bool any_items_hovered = false;

        ModelIndex src_idx  = m_proxy_model->toSourceIndex(group_idx);
        std::string qual_id = buildQualifiedId(src_idx);

        ImGuiID group_id = ImGui::GetID(qual_id.c_str());
        ImGui::PushID(group_id);

        if (ImGui::TableNextColumn()) {
#if 0
            ImGuiTreeNodeFlags flags = is_selected ? ImGuiTreeNodeFlags_Selected
                                                   : ImGuiTreeNodeFlags_None;
            flags |= ImGuiTreeNodeFlags_AllowOverlap;
            flags |= ImGuiTreeNodeFlags_SpanAllColumns;

            open = ImGui::TreeNodeEx(name.c_str(), flags);
#else
            std::string button_id_str = "##node_behavior";
            ImGuiID button_id         = ImGui::GetID(button_id_str.c_str());

            row_button_pos = ImGui::GetCursorPos() + ImGui::GetWindowPos() + style.ItemSpacing;
            row_button_pos.x += style.ItemSpacing.x * depth * 2;
            row_button_bb = {
                row_button_pos,
                row_button_pos + row_button_size,
            };

            ImVec2 spoof_item_size = row_button_size;
            spoof_item_size.x += style.ItemSpacing.x * depth * 2;

            ImGui::ItemSize(spoof_item_size, style.FramePadding.y);
            if (!ImGui::ItemAdd(row_button_bb, button_id)) {
                ImGui::PopID();
                return;
            }

            is_collapse_hovered = false;
            is_collapse_held    = false;
            is_collapse_pressed =
                ImGui::ButtonBehavior(row_button_bb, button_id, &is_collapse_hovered,
                                      &is_collapse_held, ImGuiButtonFlags_MouseButtonLeft);

            open = (m_node_open_state[button_id] ^= is_collapse_pressed);

            ImGuiDir direction = open == true ? ImGuiDir_Down : ImGuiDir_Right;
            ImGui::RenderArrow(ImGui::GetForegroundDrawList(), row_button_pos,
                               ImGui::ColorConvertFloat4ToU32({1.0, 1.0, 1.0, 1.0}), direction);

            ImGui::SameLine();

            ImGui::Text(name.c_str());
#endif
        }

        // ImGui::PopStyleColor(3);

        ImGui::TableNextColumn();
        ImGui::TableNextColumn();

        ModelIndex source_group_idx = m_proxy_model->toSourceIndex(group_idx);

        if (ImGui::TableNextColumn()) {
            is_lock_pressed = ImGui::Checkbox("##lockbox", &locked);
            if (is_lock_pressed) {
                recursiveLock(source_group_idx, locked);
            }

            is_lock_hovered = ImGui::IsItemHovered();
        }

        ImGui::TableNextColumn();

        // Handle click responses
        {
            if (is_rect_clicked && !is_collapse_hovered && !is_lock_hovered) {
                m_any_row_clicked = true;
                if ((is_left_click && !is_left_click_release) ||
                    (is_right_click && !is_right_click_release)) {
                    actionSelectIndex(group_idx);
                } else if (is_left_click_release || is_right_click_release) {
                    actionClearRequestExcIndex(group_idx, is_left_click_release);
                }
            }
        }

        if (open) {
            for (size_t i = 0; i < m_proxy_model->getRowCount(group_idx); ++i) {
                ModelIndex index = m_proxy_model->getIndex(i, 0, group_idx);
                if (m_proxy_model->isIndexGroup(index)) {
                    renderWatchGroup(index, depth + 1, table_start_x, table_width);
                } else {
                    renderMemoryWatch(index, depth + 1, table_start_x, table_width);
                }
            }

#if 0
            ImGui::TreePop();
#endif
        }

        ImGui::PopID();
    }

    void DebuggerWindow::onAttach() {
        ImWindow::onAttach();

        m_initialized_splitter_widths = false;

        m_watch_model = make_referable<WatchDataModel>();
        m_watch_model->initialize();

        m_proxy_model = make_referable<WatchDataModelSortFilterProxy>();
        m_proxy_model->setSourceModel(m_watch_model);
        m_proxy_model->setSortOrder(ModelSortOrder::SORT_ASCENDING);
        m_proxy_model->setSortRole(WatchModelSortRole::SORT_ROLE_NAME);

        m_selection.setModel(m_proxy_model);
        m_selection_ctx.setModel(m_proxy_model);

        m_add_group_dialog.setInsertPolicy(
            AddGroupDialog::InsertPolicy::INSERT_AFTER);  // Insert after the current group
        m_add_group_dialog.setFilterPredicate(
            [this](std::string_view group_name, ModelIndex group_idx) -> bool {
                // Check if the group name is unique
                ModelIndex src_idx = m_proxy_model->toSourceIndex(group_idx);
                for (size_t i = 0; i < m_watch_model->getRowCount(src_idx); ++i) {
                    ModelIndex child_idx   = m_watch_model->getIndex(i, 0, src_idx);
                    std::string child_name = m_watch_model->getDisplayText(child_idx);
                    if (child_name == group_name) {
                        return false;
                    }
                }
                return true;  // Group name is unique
            });
        m_add_group_dialog.setActionOnAccept(
            [&](ModelIndex group_idx, size_t row, AddGroupDialog::InsertPolicy policy,
                std::string_view group_name) { insertGroup(group_idx, row, policy, group_name); });

        m_add_watch_dialog.setInsertPolicy(
            AddWatchDialog::InsertPolicy::INSERT_AFTER);  // Insert after the current group
        m_add_watch_dialog.setFilterPredicate(
            [this](std::string_view watch_name, ModelIndex group_idx) -> bool {
                // Check if the watch name is unique
                ModelIndex src_idx = m_proxy_model->toSourceIndex(group_idx);
                for (size_t i = 0; i < m_watch_model->getRowCount(src_idx); ++i) {
                    ModelIndex child_idx   = m_watch_model->getIndex(i, 0, src_idx);
                    std::string child_name = m_watch_model->getDisplayText(child_idx);
                    if (child_name == watch_name) {
                        return false;
                    }
                }
                return true;  // Watch name is unique
            });
        m_add_watch_dialog.setActionOnAccept(
            [&](ModelIndex group_idx, size_t row, AddWatchDialog::InsertPolicy policy,
                std::string_view watch_name, MetaType type, u32 address, u32 size) {
                insertWatch(group_idx, row, policy, watch_name, type, address, size);
            });
        // buildContextMenu();
    }

    void DebuggerWindow::onDetach() { ImWindow::onDetach(); }

    void DebuggerWindow::onImGuiUpdate(TimeStep delta_time) {}

    void DebuggerWindow::onContextMenuEvent(RefPtr<ContextMenuEvent> ev) {}

    void DebuggerWindow::onDragEvent(RefPtr<DragEvent> ev) {
        float x, y;
        ev->getGlobalPoint(x, y);

        if (ev->getType() == EVENT_DRAG_ENTER) {
            ev->accept();
        } else if (ev->getType() == EVENT_DRAG_LEAVE) {
            ev->accept();
        } else if (ev->getType() == EVENT_DRAG_MOVE) {
            ev->accept();
        }
    }

    void DebuggerWindow::onDropEvent(RefPtr<DropEvent> ev) { ev->accept(); }

    void DebuggerWindow::actionDeleteIndexes(std::vector<ModelIndex> &indices) {}

    void DebuggerWindow::actionOpenIndexes(const std::vector<ModelIndex> &indices) {}

    void DebuggerWindow::actionRenameIndex(const ModelIndex &index) {}

    void DebuggerWindow::actionPasteIntoIndex(const ModelIndex &index,
                                              const std::vector<fs_path> &data) {}

    void DebuggerWindow::actionCopyIndexes(const std::vector<ModelIndex> &indices) {

        // Build a json from the index data
        // ---
        MimeData data;
        data.set_text("UNIMPLEMENTED");

        auto result = SystemClipboard::instance().setContent(data);
        if (!result) {
            TOOLBOX_ERROR("[PROJECT] Failed to set contents of clipboard");
        }
    }

    void DebuggerWindow::actionSelectIndex(const ModelIndex &index) {
        if (m_did_drag_drop) {
            return;
        }

        if (Input::GetKey(KeyCode::KEY_LEFTCONTROL) || Input::GetKey(KeyCode::KEY_RIGHTCONTROL)) {
            if (m_selection.is_selected(index)) {
                m_selection.deselect(index);
            } else {
                m_selection.selectSingle(index, true);
            }
        } else {
            if (Input::GetKey(KeyCode::KEY_LEFTSHIFT) || Input::GetKey(KeyCode::KEY_RIGHTSHIFT)) {
                if (m_watch_model->validateIndex(m_last_selected_index)) {
                    m_selection.selectSpan(index, m_last_selected_index, false, true);
                } else {
                    m_selection.selectSingle(index);
                }
            } else {
                m_selection.selectSingle(index);
            }
        }

        m_last_selected_index = index;
    }

    void DebuggerWindow::actionClearRequestExcIndex(const ModelIndex &index, bool is_left_button) {
        if (m_did_drag_drop) {
            return;
        }

        if (Input::GetKey(KeyCode::KEY_LEFTCONTROL) || Input::GetKey(KeyCode::KEY_RIGHTCONTROL)) {
            return;
        }

        if (Input::GetKey(KeyCode::KEY_LEFTSHIFT) || Input::GetKey(KeyCode::KEY_RIGHTSHIFT)) {
            return;
        }

        if (is_left_button) {
            if (m_selection.count() > 0) {
                m_last_selected_index = ModelIndex();
                if (m_selection.selectSingle(index)) {
                    m_last_selected_index = index;
                }
            }
        } else {
            if (!m_proxy_model->validateIndex(index)) {
                m_selection.clearSelection();
            }
            m_selection_ctx       = m_selection;
            m_last_selected_index = ModelIndex();
        }
    }

    void DebuggerWindow::recursiveLock(ModelIndex src_idx, bool lock) {
        m_watch_model->setWatchLock(src_idx, lock);
        for (size_t i = 0; i < m_watch_model->getRowCount(src_idx); ++i) {
            ModelIndex src_child_idx = m_watch_model->getIndex(i, 0, src_idx);
            recursiveLock(src_child_idx, lock);
        }
    }

    ModelIndex DebuggerWindow::insertGroup(ModelIndex group_index, size_t row,
                                           AddGroupDialog::InsertPolicy policy,
                                           std::string_view group_name) {
        ModelIndex src_group_idx  = m_proxy_model->toSourceIndex(group_index);
        ModelIndex target_idx     = m_proxy_model->getIndex(row, 0, group_index);
        ModelIndex src_target_idx = m_proxy_model->toSourceIndex(target_idx);

        int64_t src_row = m_watch_model->getRow(src_target_idx);
        if (src_row == -1) {
            src_row = 0;
        }

        int64_t sibling_count = m_watch_model->getRowCount(src_group_idx);

        switch (policy) {
        case AddGroupDialog::InsertPolicy::INSERT_BEFORE: {
            return m_watch_model->makeGroupIndex(std::string(group_name),
                                                 std::min(sibling_count, src_row), src_group_idx);
        }
        case AddGroupDialog::InsertPolicy::INSERT_AFTER: {
            return m_watch_model->makeGroupIndex(
                std::string(group_name), std::min(sibling_count, src_row + 1), src_group_idx);
        }
        case AddGroupDialog::InsertPolicy::INSERT_CHILD: {
            int64_t row_count = m_watch_model->getRowCount(src_target_idx);
            return m_watch_model->makeGroupIndex(std::string(group_name), row_count,
                                                 src_target_idx);
        }
        }

        return ModelIndex();
    }

    ModelIndex DebuggerWindow::insertWatch(ModelIndex group_index, size_t row,
                                           AddWatchDialog::InsertPolicy policy,
                                           std::string_view watch_name, MetaType watch_type,
                                           u32 watch_address, u32 watch_size) {
        ModelIndex src_group_idx  = m_proxy_model->toSourceIndex(group_index);
        ModelIndex target_idx     = m_proxy_model->getIndex(row, 0, group_index);
        ModelIndex src_target_idx = m_proxy_model->toSourceIndex(target_idx);

        int64_t src_row = m_proxy_model->getRow(src_target_idx);
        if (src_row == -1) {
            src_row = 0;
        }

        int64_t sibling_count = m_watch_model->getRowCount(src_group_idx);

        switch (policy) {
        case AddWatchDialog::InsertPolicy::INSERT_BEFORE: {
            return m_watch_model->makeWatchIndex(std::string(watch_name), watch_type, watch_address,
                                                 watch_size, std::min(sibling_count, src_row),
                                                 src_group_idx);
        }
        case AddWatchDialog::InsertPolicy::INSERT_AFTER: {
            return m_watch_model->makeWatchIndex(std::string(watch_name), watch_type, watch_address,
                                                 watch_size, std::min(sibling_count, src_row + 1),
                                                 src_group_idx);
        }
        case AddWatchDialog::InsertPolicy::INSERT_CHILD: {
            int64_t row_count = m_watch_model->getRowCount(src_target_idx);
            return m_watch_model->makeWatchIndex(std::string(watch_name), watch_type, watch_address,
                                                 watch_size, row_count, src_target_idx);
        }
        }

        return ModelIndex();
    }

    std::string DebuggerWindow::buildQualifiedId(ModelIndex src_index) const {
        if (!m_watch_model->validateIndex(src_index)) {
            return "";
        }

        std::string result;
        result.reserve(256);
        result.append("##");
        result.append(m_watch_model->getDisplayText(src_index));

        ModelIndex parent_idx = m_watch_model->getParent(src_index);
        while (m_watch_model->validateIndex(parent_idx)) {
            result.append(",");
            result.append(m_watch_model->getDisplayText(parent_idx));
            parent_idx = m_watch_model->getParent(parent_idx);
        }

        return result;
    }

    void DebuggerWindow::renderPreview(f32 label_width, const MetaValue &value) {
        ImGuiStyle &style = ImGui::GetStyle();

        switch (value.type()) {
        default:
            renderPreviewSingle(label_width, value);
            break;
        case MetaType::RGB:
            renderPreviewRGB(label_width, value);
            break;
        case MetaType::RGBA:
            renderPreviewRGBA(label_width, value);
            break;
        case MetaType::VEC3:
            renderPreviewVec3(label_width, value);
            break;
        case MetaType::TRANSFORM:
            renderPreviewTransform(label_width, value);
            break;
        case MetaType::MTX34:
            renderPreviewMatrix34(label_width, value);
            break;
        case MetaType::STRING:
            renderPreviewSingle(label_width, value);
            break;
        }
    }

    void DebuggerWindow::renderPreviewSingle(f32 column_width, const MetaValue &value) {
        ImVec2 cursor_pos = ImGui::GetCursorPos();

        size_t preview_size = 4096;

        char *value_buf = new char[preview_size];
        calcPreview(value_buf, preview_size, value);

        if (value.computeSize() < 8) {
            ImGui::SetNextItemWidth(200.0f);
        } else {
            ImGui::SetNextItemWidth(350.0f);
        }

        ImGui::InputText("##single_preview", value_buf, preview_size,
                         ImGuiInputTextFlags_ReadOnly | ImGuiInputTextFlags_AutoSelectAll);

        delete[] value_buf;
    }

    void DebuggerWindow::renderPreviewRGBA(f32 column_width, const MetaValue &value) {
        ImGuiStyle &style = ImGui::GetStyle();
        ImVec2 cursor_pos = ImGui::GetCursorPos();

        char value_buf[32] = {};
        calcPreview(value_buf, sizeof(value_buf), value);

        ImGui::SetNextItemWidth(400.0f - ImGui::GetFrameHeight() - style.ItemSpacing.x);
        ImGui::InputText("##single_preview", value_buf, sizeof(value_buf),
                         ImGuiInputTextFlags_ReadOnly | ImGuiInputTextFlags_AutoSelectAll);

        ImGui::SameLine();

        Color::RGBAShader calc_color = calcColorRGBA(value);
        ImVec4 color_rgba = {calc_color.m_r, calc_color.m_g, calc_color.m_b, calc_color.m_a};

        if (ImGui::ColorButton("##color_rgb_preview", color_rgba,
                               ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop |
                                   ImGuiColorEditFlags_NoPicker | ImGuiColorEditFlags_NoInputs)) {
            // Color button clicked, do nothing
        }
    }

    void DebuggerWindow::renderPreviewRGB(f32 column_width, const MetaValue &value) {
        ImGuiStyle &style = ImGui::GetStyle();

        char value_buf[32] = {};
        calcPreview(value_buf, sizeof(value_buf), value);

        ImGui::SetNextItemWidth(400.0f - ImGui::GetFrameHeight() - style.ItemSpacing.x);
        ImGui::InputText("##single_preview", value_buf, sizeof(value_buf),
                         ImGuiInputTextFlags_ReadOnly | ImGuiInputTextFlags_AutoSelectAll);

        ImGui::SameLine();

        Color::RGBShader calc_color = calcColorRGB(value);
        ImVec4 color_rgba           = {calc_color.m_r, calc_color.m_g, calc_color.m_b, 1.0f};

        if (ImGui::ColorButton("##color_rgb_preview", color_rgba,
                               ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop |
                                   ImGuiColorEditFlags_NoPicker | ImGuiColorEditFlags_NoInputs)) {
            // Color button clicked, do nothing
        }
    }

    void DebuggerWindow::renderPreviewVec3(f32 column_width, const MetaValue &value) {
        ImGuiStyle &style    = ImGui::GetStyle();
        ImVec2 cursor_origin = ImGui::GetCursorPos();

        ImVec2 preview_size = {0, 0};
        preview_size.x      = 100.0f;
        preview_size.y      = ImGui::GetFrameHeight();

        ImVec2 preview_pos = cursor_origin;
        preview_pos.x      = ImGui::GetCursorPosX();

        ImGui::SetCursorPos(preview_pos);

        ImGuiWindow *window            = ImGui::GetCurrentWindow();
        window->DC.CursorPosPrevLine.y = window->Pos.y + preview_pos.y;

        ImGui::BeginGroup();
        {

            for (int i = 0; i < 3; ++i) {
                char value_buf[32] = {};
                calcPreview(value_buf, sizeof(value_buf), value);

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

    void DebuggerWindow::renderPreviewTransform(f32 column_width, const MetaValue &value) {
        ImGuiStyle &style = ImGui::GetStyle();

        ImVec2 preview_size    = {0, 0};
        float column_width_vec = ImGui::CalcTextSize("R:").x;

        preview_size.x = 100.0f * 3.0f;
        preview_size.x += column_width_vec;
        preview_size.x += style.ItemSpacing.x * 3.0f;  // Add spacing for padding

        preview_size.y = ImGui::GetFrameHeight() * 3.0f;
        preview_size.y += style.ItemSpacing.y * 2.0f;  // Add spacing for padding

        ImVec2 cursor_origin = ImGui::GetCursorPos();

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

            ImGui::TextAndWidth(column_width_vec, "T:");
            ImGui::SameLine();

            for (int i = 0; i < 3; ++i) {
                char value_buf[32] = {};
                calcPreview(value_buf, sizeof(value_buf), value);

                ImGui::PushID(i);

                ImGui::SetNextItemWidth(100.0f);
                ImGui::InputText("##vec3_f32_preview_trans", value_buf, sizeof(value_buf),
                                 ImGuiInputTextFlags_ReadOnly | ImGuiInputTextFlags_AutoSelectAll);

                if (i < 2) {
                    ImGui::SameLine();
                }

                ImGui::PopID();
            }

            ImGui::TextAndWidth(column_width_vec, "R:");
            ImGui::SameLine();

            for (int i = 0; i < 3; ++i) {
                char value_buf[32] = {};
                calcPreview(value_buf, sizeof(value_buf), value);

                ImGui::PushID(i);

                ImGui::SetNextItemWidth(100.0f);
                ImGui::InputText("##vec3_f32_preview_rot", value_buf, sizeof(value_buf),
                                 ImGuiInputTextFlags_ReadOnly | ImGuiInputTextFlags_AutoSelectAll);

                if (i < 2) {
                    ImGui::SameLine();
                }

                ImGui::PopID();
            }

            ImGui::TextAndWidth(column_width_vec, "S:");
            ImGui::SameLine();

            for (int i = 0; i < 3; ++i) {
                char value_buf[32] = {};
                calcPreview(value_buf, sizeof(value_buf), value);

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

    void DebuggerWindow::renderPreviewMatrix34(f32 column_width, const MetaValue &value) {
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
                    calcPreview(value_buf, sizeof(value_buf), value);

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

    void DebuggerWindow::calcPreview(char *preview_out, size_t preview_size,
                                     const MetaValue &value) const {
        if (preview_out == nullptr || preview_size == 0) {
            return;
        }

        size_t address_size = value.computeSize();

        if (address_size == 0) {
            snprintf(preview_out, preview_size, "???");
            return;
        }

        if (preview_size < address_size) {
            snprintf(preview_out, preview_size, "???");
            return;
        }

#if 0
        DolphinCommunicator &communicator = GUIApplication::instance().getDolphinCommunicator();
        if (!communicator.manager().isHooked()) {
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
            communicator.readCString(preview_out, preview_size, true_address)
                .or_else([&](const BaseError &err) {
                    snprintf(preview_out, preview_size, "Error: %s", err.m_message[0]);
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
        default:
            snprintf(preview_out, preview_size, "Unsupported type");
            break;
        }
#else
        switch (value.type()) {
        case MetaType::BOOL: {
            value.get<bool>()
                .and_then([&](bool result) {
                    snprintf(preview_out, preview_size, "%s", result ? "true" : "false");
                    return Result<bool, std::string>();
                })
                .or_else([&](std::string &&error) {
                    snprintf(preview_out, preview_size, "%s", error.c_str());
                    return Result<bool, std::string>();
                });
            break;
        }
        case MetaType::S8: {
            value.get<s8>()
                .and_then([&](s8 result) {
                    snprintf(preview_out, preview_size, "%d", result);
                    return Result<s8, std::string>();
                })
                .or_else([&](std::string &&error) {
                    snprintf(preview_out, preview_size, "%s", error.c_str());
                    return Result<s8, std::string>();
                });
            break;
        }
        case MetaType::U8: {
            value.get<u8>()
                .and_then([&](u8 result) {
                    snprintf(preview_out, preview_size, "%u", result);
                    return Result<u8, std::string>();
                })
                .or_else([&](std::string &&error) {
                    snprintf(preview_out, preview_size, "%s", error.c_str());
                    return Result<u8, std::string>();
                });
            break;
        }
        case MetaType::S16: {
            value.get<s16>()
                .and_then([&](s16 result) {
                    snprintf(preview_out, preview_size, "%d", result);
                    return Result<s16, std::string>();
                })
                .or_else([&](std::string &&error) {
                    snprintf(preview_out, preview_size, "%s", error.c_str());
                    return Result<s16, std::string>();
                });
            break;
        }
        case MetaType::U16: {
            value.get<u16>()
                .and_then([&](u16 result) {
                    snprintf(preview_out, preview_size, "%u", result);
                    return Result<u16, std::string>();
                })
                .or_else([&](std::string &&error) {
                    snprintf(preview_out, preview_size, "%s", error.c_str());
                    return Result<u16, std::string>();
                });
            break;
        }
        case MetaType::S32: {
            value.get<s32>()
                .and_then([&](s32 result) {
                    snprintf(preview_out, preview_size, "%ld", result);
                    return Result<s32, std::string>();
                })
                .or_else([&](std::string &&error) {
                    snprintf(preview_out, preview_size, "%s", error.c_str());
                    return Result<s32, std::string>();
                });
            break;
        }
        case MetaType::U32: {
            value.get<u32>()
                .and_then([&](u32 result) {
                    snprintf(preview_out, preview_size, "%lu", result);
                    return Result<u32, std::string>();
                })
                .or_else([&](std::string &&error) {
                    snprintf(preview_out, preview_size, "%s", error.c_str());
                    return Result<u32, std::string>();
                });
            break;
        }
        case MetaType::F32: {
            value.get<f32>()
                .and_then([&](f32 result) {
                    snprintf(preview_out, preview_size, "%.6f", result);
                    return Result<f32, std::string>();
                })
                .or_else([&](std::string &&error) {
                    snprintf(preview_out, preview_size, "%s", error.c_str());
                    return Result<f32, std::string>();
                });
            break;
        }
        case MetaType::F64: {
            value.get<f64>()
                .and_then([&](f64 result) {
                    snprintf(preview_out, preview_size, "%.6f", result);
                    return Result<f64, std::string>();
                })
                .or_else([&](std::string &&error) {
                    snprintf(preview_out, preview_size, "%s", error.c_str());
                    return Result<f64, std::string>();
                });
            break;
        }
        case MetaType::STRING: {
            value.get<std::string>()
                .and_then([&](const std::string &result) {
                    snprintf(preview_out, preview_size, "%s", result.c_str());
                    return Result<std::string, std::string>();
                })
                .or_else([&](std::string &&error) {
                    snprintf(preview_out, preview_size, "%s", error.c_str());
                    return Result<std::string, std::string>();
                });
            break;
        }
        case MetaType::RGB: {
            value.get<Color::RGB24>()
                .and_then([&](Color::RGB24 result) {
                    u32 value = (result.m_r << 16) | (result.m_g << 8) | result.m_b;
                    snprintf(preview_out, preview_size, "#%06X", value);
                    return Result<Color::RGB24, std::string>();
                })
                .or_else([&](std::string &&error) {
                    snprintf(preview_out, preview_size, "%s", error.c_str());
                    return Result<Color::RGB24, std::string>();
                });
            break;
        }
        case MetaType::RGBA: {
            value.get<Color::RGBA32>()
                .and_then([&](Color::RGBA32 result) {
                    u32 value =
                        (result.m_r << 24) | (result.m_g << 16) | (result.m_b << 8) | result.m_a;
                    snprintf(preview_out, preview_size, "#%08X", value);
                    return Result<Color::RGBA32, std::string>();
                })
                .or_else([&](std::string &&error) {
                    snprintf(preview_out, preview_size, "%s", error.c_str());
                    return Result<Color::RGBA32, std::string>();
                });
            break;
        }
        default:
            snprintf(preview_out, preview_size, "Unsupported type");
            break;
        }
#endif
    }

    Color::RGBShader DebuggerWindow::calcColorRGB(const MetaValue &value) {
#if 0
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

        u32 val = communicator.read<u32>(true_address).value_or(0);
#else
        Color::RGB24 rgba_color = value.get<Color::RGB24>().value_or(Color::RGB24());
#endif
        // Color::RGB24 rgba_color((u8)((val >> 24) & 0xFF), (u8)((val >> 16) & 0xFF),
        //                         (u8)((val >> 8) & 0xFF));
        f32 r, g, b, a;
        rgba_color.getColor(r, g, b, a);

        return Color::RGBShader(r, g, b);
    }

    Color::RGBAShader DebuggerWindow::calcColorRGBA(const MetaValue &value) {
#if 0
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
#else
        Color::RGBA32 rgba_color = value.get<Color::RGBA32>().value_or(Color::RGBA32());
#endif
        // Color::RGBA32 rgba_color((u8)((val >> 24) & 0xFF), (u8)((val >> 16) & 0xFF),
        //                          (u8)((val >> 8) & 0xFF), (u8)(val & 0xFF));
        f32 r, g, b, a;
        rgba_color.getColor(r, g, b, a);

        return Color::RGBAShader(r, g, b, a);
    }

    void DebuggerWindow::overwriteNibbleAtCursor(u8 nibble_value) {
        DolphinCommunicator &communicator = GUIApplication::instance().getDolphinCommunicator();

        if ((m_address_cursor & 0x1FFFFFF) + (m_address_cursor_nibble / 2) >=
            communicator.manager().getMemorySize()) {
            TOOLBOX_ERROR_V("[MEMORY VIEW] Tried to overwrite nibble at invalid address");
            return;
        }

        switch (m_byte_width) {
        case 1: {
            communicator.read<u8>(m_address_cursor)
                .and_then([&](u8 value) {
                    u8 new_value = OverwriteNibble(value, m_address_cursor_nibble, nibble_value);
                    return communicator.write<u8>(m_address_cursor, new_value);
                })
                .or_else([](const BaseError &error) {
                    LogError(error);
                    return Result<void>();
                });
            break;
        }
        case 2: {
            communicator.read<u16>(m_address_cursor)
                .and_then([&](u16 value) {
                    u16 new_value = OverwriteNibble(value, m_address_cursor_nibble, nibble_value);
                    return communicator.write<u16>(m_address_cursor, new_value);
                })
                .or_else([](const BaseError &error) {
                    LogError(error);
                    return Result<void>();
                });
            break;
        }
        case 4: {
            communicator.read<u32>(m_address_cursor)
                .and_then([&](u32 value) {
                    u32 new_value = OverwriteNibble(value, m_address_cursor_nibble, nibble_value);
                    return communicator.write<u32>(m_address_cursor, new_value);
                })
                .or_else([](const BaseError &error) {
                    LogError(error);
                    return Result<void>();
                });
            break;
        }
        case 8: {
            communicator.read<u64>(m_address_cursor)
                .and_then([&](u64 value) {
                    u64 new_value = OverwriteNibble(value, m_address_cursor_nibble, nibble_value);
                    return communicator.write<u64>(m_address_cursor, new_value);
                })
                .or_else([](const BaseError &error) {
                    LogError(error);
                    return Result<void>();
                });
            break;
        }
        }
    }

}  // namespace Toolbox::UI
