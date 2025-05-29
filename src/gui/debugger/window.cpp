#include "gui/debugger/window.hpp"
#include "gui/application.hpp"
#include "gui/dragdrop/dragdropmanager.hpp"
#include "gui/new_item/window.hpp"
#include "model/fsmodel.hpp"

#include <cctype>
#include <cmath>
#include <imgui/imgui.h>

#include "gui/imgui_ext.hpp"

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
        ImGui::SameLine(0, 0);

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
    }

    static u32 calculateBytesForRow(u32 byte_limit, u8 column_count, u8 byte_width) {
        // Calculate the number of bytes that can be rendered in a row
        // ---
        // If byte_limit is less than column_count * byte_width, we can only render up to byte_limit
        // bytes.
        u32 bytes_per_row = std::min<u32>(byte_limit, column_count * byte_width);
        return bytes_per_row;
    }

    static u32 renderMemoryRow(void *handle, u32 base_address, u32 byte_limit, u8 column_count,
                               u8 byte_width) {
        // Render the address
        // ---
        ImGui::Text("0x%08X", base_address);
        ImGui::SameLine();

        byte_width              = std::min<u8>(byte_width, 4);
        const char formatter[8] = {'%', '0', '0' + byte_width * 2, 'X', '\0'};

        u64 value = 0;

        void *mem_ptr = handle;
        u32 b_limit   = byte_limit;

        // u32 char_width = std::min<u32>(column_count, byte_limit);

        // Render each byte group in the row
        for (u8 column = 0; column < column_count; ++column) {
            ImGui::SameLine();
            value = std::byteswap(*((u64 *)mem_ptr));

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
            ImGui::Text(formatter, (value & mask) >> (64 - bit_width));

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
        if (ImGui::BeginChild("##MemoryWatchList", {m_list_width, 0}, true,
                              ImGuiWindowFlags_ChildWindow | ImGuiWindowFlags_NoDecoration)) {
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {2, 2});
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {2, 2});
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 3.0f);

            ImGui::PopStyleVar(5);
        }
        ImGui::EndChild();
    }

    void DebuggerWindow::onAttach() {
        ImWindow::onAttach();

        m_initialized_splitter_widths = false;
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

}  // namespace Toolbox::UI
