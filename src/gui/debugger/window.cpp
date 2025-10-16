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

template <typename _T> constexpr static _T OverwriteByte(_T value, u8 byte_idx, u8 byte_val) {
    static_assert(std::is_integral_v<_T>, "_T must be a basic integral type");

    constexpr size_t byte_width = sizeof(_T);

    if (byte_idx >= byte_width) {
        return value;
    }

    byte_val &= 0b11111111;

    _T byte_idx_big = ((byte_width - 1) - byte_idx);
    _T byte_mask    = static_cast<_T>(0b11111111) << (byte_idx_big * 8);
    _T new_value    = (value & ~byte_mask) | (static_cast<_T>(byte_val) << (byte_idx_big * 8));

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

        if (!m_initialized_splitters) {
            m_initialized_splitters = true;

            m_scan_height = std::max(min_size.y * 0.7f, avail_region.x * 0.7f);
            m_list_height = std::max(min_size.y * 0.5f, avail_region.x * 0.5f);
            m_list_width  = std::max(min_size.x * 0.5f, avail_region.x * 0.5f);
            m_view_width  = std::max(min_size.x * 0.7f, avail_region.x * 0.7f);
        }

        float last_total_height = m_scan_height + m_list_height;
        float last_total_width  = m_list_width + m_view_width;

        {
            ImGuiID splitter_id = ImGui::GetID("##WatchMemorySplitter");
            ImGui::SplitterBehavior(splitter_id, ImGuiAxis_X, splitter_width, &m_list_width,
                                    &m_view_width, min_size.x * 0.5f, min_size.x * 0.5f);
        }

        m_view_width = (m_view_width / last_total_width) * (avail_region.x - splitter_width);
        m_view_width = std::max(m_view_width, min_size.x * 0.5f);

        m_list_width = (m_list_width / last_total_width) * (avail_region.x - splitter_width);
        m_list_width = std::max(m_list_width, min_size.x * 0.5f);

        // ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {2, 2});
        // ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
        // ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.0f);

        if (ImGui::BeginChild("MemoryWatchScanner", {m_list_width, 0}, true,
                              ImGuiWindowFlags_ChildWindow | ImGuiWindowFlags_NoDecoration)) {
            m_scan_height = (m_scan_height / last_total_height) * (avail_region.y - splitter_width);
            m_scan_height = std::max(m_scan_height, min_size.y * 0.7f);

            m_list_height = (m_list_height / last_total_height) * (avail_region.y - splitter_width);
            m_list_height = std::max(m_list_height, min_size.y * 0.3f);

            {
                ImGuiID splitter_id = ImGui::GetID("##ScannerWatchSplitter");
                ImGui::SplitterBehavior(splitter_id, ImGuiAxis_Y, splitter_width, &m_scan_height,
                                        &m_list_height, min_size.y * 0.7f, min_size.y * 0.3f);
            }

            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {2, 2});
            ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.0f);
            {

                renderMemoryScanner();

                ImGui::InvisibleButton("##ScannerWatchSplitter",
                                       ImVec2(avail_region.x, splitter_width));

                renderMemoryWatchList();
            }
            ImGui::PopStyleVar(2);
        }
        ImGui::EndChild();

        // ImGui::PopStyleVar(3);

        ImGui::SameLine(0);
        // ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::InvisibleButton("##WatchMemorySplitter", ImVec2(splitter_width, avail_region.y));

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

            AddressSpan span = {m_address_selection_begin + m_address_selection_begin_nibble / 2,
                                m_address_selection_end + m_address_selection_end_nibble / 2};

            m_ascii_view_context_menu.render("ASCII View", span);
            m_byte_view_context_menu.render("Byte View", span);
            m_fill_bytes_dialog.render(span);
        }
        ImGui::EndChild();

        // ImGui::PopStyleVar();

        ModelIndex last_selected_watch = m_watch_selection.getLastSelected();

        ModelIndex parent_index = m_watch_proxy_model->getParent(last_selected_watch);
        int64_t the_row         = m_watch_proxy_model->getRow(last_selected_watch);

        m_add_group_dialog.render(last_selected_watch, the_row);
        m_add_watch_dialog.render(last_selected_watch, the_row);

        m_watch_view_context_menu.render("Memory Watch", last_selected_watch);

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
        const bool window_hovered = ImGui::IsWindowHovered();

        const float font_width = ImGui::GetFontSize();
        const float ch_width   = ImGui::CalcTextSize("0").x;
        const float ch_height  = ImGui::CalcTextSize("0").y;

        // Render the address
        // ---
        ImGui::Text("0x%08X", base_address);
        ImGui::SameLine();

        const int16_t nibble_width = byte_width * 2;

        byte_width              = std::min<u8>(byte_width, 4);
        const char formatter[8] = {'%', '0', '0' + nibble_width, 'X', '\0'};

        u64 value = 0;

        void *mem_ptr = handle;
        u32 b_limit   = byte_limit;

        // This is the end of the row as an address.
        const u32 base_address_end = base_address + column_count * byte_width;

        u32 selection_start;
        u32 selection_end;
        int16_t selection_start_nibble;
        int16_t selection_end_nibble;

        if (m_address_selection_begin <= m_address_selection_end) {
            selection_start        = m_address_selection_begin;
            selection_start_nibble = m_address_selection_begin_nibble;
            selection_end          = m_address_selection_end;
            selection_end_nibble   = m_address_selection_end_nibble;
        } else {
            selection_end          = m_address_selection_begin;
            selection_end_nibble   = m_address_selection_begin_nibble;
            selection_start        = m_address_selection_end;
            selection_start_nibble = m_address_selection_end_nibble;
        }

        if (selection_start < base_address) {
            selection_start_nibble = 0;
        }

        if (selection_end > base_address_end) {
            selection_end_nibble = 0;
        }

        const bool selection_at_column_start = selection_start_nibble % nibble_width == 0;
        const bool selection_at_column_end   = selection_end_nibble % nibble_width == 0;

        ImRect context_menu_rect;
        if (m_byte_view_context_menu.is_open()) {
            context_menu_rect = m_byte_view_context_menu.rect();
        } else {
            context_menu_rect = m_ascii_view_context_menu.rect();
        }

        // HEXADECIMAL DISPLAY
        {

            // Here we actually render the selection span slice as a quad.
            if (selection_start < base_address_end && selection_end >= base_address &&
                selection_start != 0 && selection_end != 0) {
                u32 addr_beg = std::max<u32>(selection_start, base_address);
                u32 addr_end = std::min<u32>(selection_end, base_address_end);

                int16_t col_start = (addr_beg - base_address) / byte_width;
                int16_t col_end   = (addr_end - base_address) / byte_width;
                int16_t col_span  = col_end - col_start;

                bool valid_span = col_start < col_end;
                valid_span |=
                    col_start == col_end && (selection_start_nibble < selection_end_nibble);

                if (valid_span) {
                    char spoof_buf[16];
                    snprintf(spoof_buf, sizeof(spoof_buf), formatter, 0);

                    ImVec2 text_size = ImGui::CalcTextSize(spoof_buf);

                    ImRect text_rect = {};
                    text_rect.Min    = ImGui::GetWindowPos() + ImGui::GetCursorPos();
                    text_rect.Min.x += (style.ItemSpacing.x + text_size.x) * col_start;
                    text_rect.Min.y -= style.ItemSpacing.y / 2;

                    text_rect.Max = text_rect.Min;
                    text_rect.Max.x += (style.ItemSpacing.x + text_size.x) * col_span;
                    text_rect.Max.y += style.ItemSpacing.y + text_size.y;

                    if (selection_at_column_start) {
                        text_rect.Min.x -= style.ItemSpacing.x / 2;
                    } else if (col_start != column_count) {
                        text_rect.Min.x += ch_width * selection_start_nibble;
                    }

                    if (selection_at_column_end) {
                        text_rect.Max.x -= style.ItemSpacing.x / 2;
                    } else if (col_span != column_count) {
                        text_rect.Max.x += ch_width * selection_end_nibble;
                    }

                    ImVec4 color = style.Colors[ImGuiCol_TabSelected];
                    if (m_selection_was_ascii) {
                        color.w *= 0.5f;
                    }

                    window->DrawList->AddQuadFilled(text_rect.GetTL(), text_rect.GetTR(),
                                                    text_rect.GetBR(), text_rect.GetBL(),
                                                    ImGui::ColorConvertFloat4ToU32(color));
                } else {
                    // TOOLBOX_DEBUG_LOG_V("snib {}, enib {}, saddr {}, eaddr {}",
                    //                     selection_start_nibble, selection_end_nibble,
                    //                     selection_start, selection_end);
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
                text_rect.Min =
                    ImGui::GetWindowPos() + ImGui::GetCursorPos() - style.ItemSpacing / 2;
                text_rect.Max = text_rect.Min + ImGui::CalcTextSize(text_buf) + style.ItemSpacing;

                double m_x, m_y;
                Input::GetMousePosition(m_x, m_y);

                ImVec2 mouse_pos = ImVec2{(float)m_x, (float)m_y};

                bool column_hovered =
                    window_hovered && text_rect.ContainsWithPad(mouse_pos, style.TouchExtraPadding);
                bool column_clicked =
                    column_hovered && Input::GetMouseButtonDown(MouseButton::BUTTON_LEFT);
                if (column_clicked) {
                    m_selection_was_ascii = false;

                    m_address_selection_new         = true;
                    m_address_selection_mouse_start = mouse_pos;

                    m_address_selection_begin = cur_address;
                    m_address_selection_end   = cur_address;

                    m_address_cursor        = cur_address;
                    m_address_cursor_nibble = ImClamp<u8>(
                        (mouse_pos.x - text_rect.Min.x) / ch_width, 0, nibble_width - 1);

                    // Set the nibble to an even index so it traverses whole bytes
                    m_address_selection_begin_nibble = m_address_cursor_nibble & ~1;
                    m_address_selection_end_nibble   = m_address_selection_begin_nibble;

                    m_cursor_anim_timer = -0.3f;
                } else if (!Input::GetMouseButton(MouseButton::BUTTON_LEFT)) {
                    m_address_selection_new = false;
                }

                // In this case we check for selection dragging
                if (!m_selection_was_ascii && m_address_selection_new &&
                    Input::GetMouseButton(MouseButton::BUTTON_LEFT) && column_hovered) {
                    ImVec2 difference = mouse_pos - m_address_selection_mouse_start;
                    if (ImLengthSqr(difference) > 10.0f) {
                        if (cur_address < m_address_selection_begin) {
                            m_address_selection_end_nibble =
                                (ImClamp<u8>((mouse_pos.x - text_rect.Min.x) / ch_width, 0,
                                             nibble_width - 1) &
                                 ~1);
                            m_address_selection_end = cur_address;
                            /*if (m_address_selection_end_nibble == 0) {
                                m_address_selection_end_nibble = nibble_width - 2;
                                m_address_selection_end -= byte_width;
                            } else {
                                m_address_selection_end_nibble -= 2;
                            }*/
                        } else if (cur_address > m_address_selection_begin) {
                            m_address_selection_end_nibble =
                                (ImClamp<u8>((mouse_pos.x - text_rect.Min.x) / ch_width, 0,
                                             nibble_width - 1) &
                                 ~1) +
                                2;
                            m_address_selection_end = cur_address;
                            if (m_address_selection_end_nibble == nibble_width) {
                                m_address_selection_end_nibble = 0;
                                m_address_selection_end += byte_width;
                            }
                        } else {
                            m_address_selection_end_nibble =
                                (ImClamp<u8>((mouse_pos.x - text_rect.Min.x) / ch_width, 0,
                                             nibble_width - 1) &
                                 ~1);
                            m_address_selection_end = cur_address;
                            if (m_address_selection_end_nibble < m_address_selection_begin_nibble) {
                                // if (m_address_selection_end_nibble == 0) {
                                //     m_address_selection_end_nibble = nibble_width - 2;
                                //     m_address_selection_end -= byte_width;
                                // } else {
                                //     m_address_selection_end_nibble -= 2;
                                // }
                            } else {
                                m_address_selection_end_nibble += 2;
                                if (m_address_selection_end_nibble == nibble_width) {
                                    m_address_selection_end_nibble = 0;
                                    m_address_selection_end += byte_width;
                                }
                            }
                        }

                        m_byte_view_context_menu.setCanOpen(true);
                        m_ascii_view_context_menu.setCanOpen(false);
                        m_watch_view_context_menu.setCanOpen(false);
                    }
                } else if (column_hovered) {
                    window->DrawList->AddRectFilled(text_rect.Min, text_rect.Max,
                                                    ImGui::GetColorU32(ImGuiCol_TabHovered));
                }

                ImGui::Text(text_buf);

                if (!m_selection_was_ascii && m_address_cursor == cur_address) {
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
        }
        // --------------

        mem_ptr = handle;
        b_limit = byte_limit;

        ImGui::SameLine();

        u32 char_width = calculateBytesForRow(b_limit, column_count, byte_width);

        // ASCII DISPLAY
        {

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

            // Calculate the selection quad for this row
            u32 base_address_end = base_address + char_width;

            // Here we actually render the selection span slice as a quad.
            if (selection_start < base_address_end && selection_end > base_address &&
                selection_start != 0 && selection_end != 0) {
                u32 addr_beg = std::max<u32>(selection_start, base_address);
                u32 addr_end = std::min<u32>(selection_end, base_address_end);

                u8 col_start = (addr_beg - base_address) + (selection_start_nibble / 2);
                u8 col_end   = (addr_end - base_address) + (selection_end_nibble / 2);

                if (col_start < col_end) {
                    char spoof_buf[16];
                    snprintf(spoof_buf, sizeof(spoof_buf), formatter, 0);

                    ImVec2 text_size = ImGui::CalcTextSize(spoof_buf);

                    ImRect text_rect = {};
                    text_rect.Min    = ImGui::GetWindowPos() + ImGui::GetCursorPos();
                    text_rect.Min.x += ch_width * col_start;
                    text_rect.Min.y -= style.ItemSpacing.y / 2;
                    text_rect.Max = text_rect.Min;
                    text_rect.Max.x += ch_width * (col_end - col_start);
                    text_rect.Max.y += style.ItemSpacing.y + text_size.y;

                    ImVec4 color = style.Colors[ImGuiCol_TabSelected];
                    if (!m_selection_was_ascii) {
                        color.w *= 0.5f;
                    }

                    window->DrawList->AddQuadFilled(text_rect.GetTL(), text_rect.GetTR(),
                                                    text_rect.GetBR(), text_rect.GetBL(),
                                                    ImGui::ColorConvertFloat4ToU32(color));
                }
            }

#if 1
            ImRect char_rect = {};
            char_rect.Min    = ImGui::GetWindowPos() + ImGui::GetCursorPos();
            char_rect.Min.y -= style.ItemSpacing.y / 2;
            char_rect.Max = char_rect.Min;
            char_rect.Max.x += ch_width;
            char_rect.Max.y += ch_height + style.ItemSpacing.y;

            ImRect text_rect = {};
            text_rect.Min    = char_rect.Min;
            text_rect.Max.x  = text_rect.Min.x + ch_width * char_width;
            text_rect.Max.y  = char_rect.Max.y;

            double m_x, m_y;
            Input::GetMousePosition(m_x, m_y);

            ImVec2 mouse_pos = ImVec2{(float)m_x, (float)m_y};

            int64_t column_hovered = -1;
            for (int64_t i = 0; i < char_width; ++i) {
                bool ch_hovered =
                    window_hovered && char_rect.ContainsWithPad(mouse_pos, style.TouchExtraPadding);
                if (ch_hovered) {
                    window->DrawList->AddRectFilled(char_rect.Min, char_rect.Max,
                                                    ImGui::GetColorU32(ImGuiCol_TabHovered));
                    column_hovered = i;
                    break;
                }
                char_rect.TranslateX(ch_width);
            }

            bool column_clicked_down =
                column_hovered >= 0 && Input::GetMouseButtonDown(MouseButton::BUTTON_LEFT);

            bool column_clicked_up =
                column_hovered >= 0 && Input::GetMouseButtonUp(MouseButton::BUTTON_LEFT);

            if (column_clicked_down) {
                m_selection_was_ascii = true;

                m_address_selection_new         = true;
                m_address_selection_mouse_start = mouse_pos;

                m_address_selection_begin = base_address;
                m_address_selection_end   = base_address;

                m_address_cursor        = (base_address + column_hovered) & ~(byte_width - 1);
                m_address_cursor_nibble = (column_hovered * 2) % nibble_width;

                // Set the nibble to an even index so it traverses whole bytes
                m_address_selection_begin_nibble = m_address_cursor_nibble & ~1;
                m_address_selection_end_nibble   = m_address_selection_begin_nibble;

                m_cursor_anim_timer = -0.3f;
            } else if (!Input::GetMouseButton(MouseButton::BUTTON_LEFT)) {
                m_address_selection_new = false;
            }

            // In this case we check for selection dragging
            if (m_selection_was_ascii && m_address_selection_new &&
                Input::GetMouseButton(MouseButton::BUTTON_LEFT) && column_hovered >= 0) {
                ImVec2 difference = mouse_pos - m_address_selection_mouse_start;
                if (ImLengthSqr(difference) > 10.0f) {
                    int64_t remainder = column_hovered % byte_width;
                    m_address_selection_end =
                        base_address + ((int64_t)(column_hovered / byte_width) * byte_width) + 1;
                    m_address_selection_end_nibble = remainder * 2;

                    m_byte_view_context_menu.setCanOpen(false);
                    m_ascii_view_context_menu.setCanOpen(true);
                    m_watch_view_context_menu.setCanOpen(false);
                }
            } else if (column_hovered >= 0) {
                window->DrawList->AddRectFilled(char_rect.Min, char_rect.Max,
                                                ImGui::GetColorU32(ImGuiCol_TabHovered));
            }

            int16_t cursor_idx = (m_address_cursor + (m_address_cursor_nibble / 2)) - base_address;
            if (m_selection_was_ascii && cursor_idx >= 0 && cursor_idx < char_width) {
                m_cursor_anim_timer += m_delta_time;
                bool cursor_visible = (m_cursor_anim_timer <= 0.0f) ||
                                      ImFmod(m_cursor_anim_timer, 1.20f) <= 0.80f;
                if (cursor_visible) {
                    ImVec2 tl = text_rect.GetTL();
                    ImVec2 bl = text_rect.GetBL();
                    tl.x += ch_width * cursor_idx - 1;
                    bl.x += ch_width * cursor_idx - 1;
                    window->DrawList->AddLine(tl, bl, 0xA03030FF, 2.0f);
                }
            }
#endif

            ImGui::TextEx(ascii_buf, ascii_buf + char_width);
            delete[] ascii_buf;
        }

        return char_width;
    }

    void DebuggerWindow::renderMemoryScanner() {
        bool is_left_click          = Input::GetMouseButtonDown(Input::MouseButton::BUTTON_LEFT);
        bool is_right_click         = Input::GetMouseButtonDown(Input::MouseButton::BUTTON_RIGHT);
        bool is_left_click_release  = Input::GetMouseButtonUp(Input::MouseButton::BUTTON_LEFT);
        bool is_right_click_release = Input::GetMouseButtonUp(Input::MouseButton::BUTTON_RIGHT);
        bool outer_window_focused   = ImGui::IsWindowFocused(ImGuiHoveredFlags_ChildWindows);

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {5.0f, 5.0f});
        if (ImGui::BeginChild("##MemoryScannerView", {0, m_scan_height}, true,
                              ImGuiWindowFlags_ChildWindow | ImGuiWindowFlags_NoDecoration)) {
            ImVec2 avail_size       = ImGui::GetContentRegionAvail();
            const ImGuiStyle &style = ImGui::GetStyle();

            if (ImGui::BeginChild("##ScanResultView", {avail_size.x / 2, 0}, true,
                                  ImGuiWindowFlags_ChildWindow | ImGuiWindowFlags_NoDecoration)) {
                ImVec2 scan_view_avail = ImGui::GetContentRegionAvail();

                const double progress = m_scan_model->getScanProgress();

                const ImVec2 results_pos = ImGui::GetCursorPos();

                ImRect progress_bar_rect;
                progress_bar_rect.Min = ImVec2(
                    results_pos.x + (scan_view_avail.x / 2) + style.ItemSpacing.x, results_pos.y);
                progress_bar_rect.Max =
                    progress_bar_rect.Min + ImVec2((scan_view_avail.x / 2) - style.ItemSpacing.x,
                                                   ImGui::GetTextLineHeight());

                ImGui::SetCursorPos(progress_bar_rect.Min);
                if (!m_scan_model->isScanBusy()) {
                    ImGui::BeginDisabled();
                }
                ImGui::ProgressBar((float)progress, progress_bar_rect.GetSize());
                if (!m_scan_model->isScanBusy()) {
                    ImGui::EndDisabled();
                }
                ImGui::SetCursorPos(results_pos);

                size_t results = m_scan_model->getRowCount(ModelIndex());
                if (m_scan_active) {
                    if (results == 1) {
                        ImGui::Text("1 result found");
                    } else {
                        ImGui::Text("%d results found", results);
                    }
                } else {
                    ImGui::NewLine();
                }

                ImVec2 scan_table_size = scan_view_avail;
                scan_table_size.y -= ImGui::GetTextLineHeight() * 3 + style.ItemSpacing.y * 2;

                if (ImGui::BeginTable("##ResultsTable", 3,
                                      ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersInnerV |
                                          ImGuiTableFlags_Resizable |
                                          ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollY,
                                      scan_table_size)) {

                    ImGui::TableSetupScrollFreeze(0, 1);
                    ImGui::TableSetupColumn("Address",
                                            ImGuiTableColumnFlags_PreferSortAscending |
                                                ImGuiTableColumnFlags_WidthFixed,
                                            100.0f);
                    ImGui::TableSetupColumn("Scanned", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableSetupColumn("Current", ImGuiTableColumnFlags_WidthStretch);

                    float table_start_x = ImGui::GetCursorPosX() + ImGui::GetWindowPos().x;

                    ImGui::TableHeadersRow();

                    if (results <= 100000 && !m_scan_model->isScanBusy()) {
                        ImGuiListClipper clipper;
                        clipper.Begin(results);

                        while (clipper.Step()) {
                            for (int n = clipper.DisplayStart; n < clipper.DisplayEnd; n++) {
                                ModelIndex idx = m_scan_model->getIndex(n, 0);

                                std::string addr_str = m_scan_model->getDisplayText(idx);
                                std::string scanned_str;
                                std::string current_str;

                                switch (m_scan_radix) {
                                case ScanRadix::RADIX_BINARY:
                                    scanned_str = m_scan_model->getScanValue(idx).toString(2);
                                    current_str = m_scan_model->getCurrentValue(idx).toString(2);
                                    break;
                                case ScanRadix::RADIX_OCTAL:
                                    scanned_str = m_scan_model->getScanValue(idx).toString(8);
                                    current_str = m_scan_model->getCurrentValue(idx).toString(8);
                                    break;
                                case ScanRadix::RADIX_DECIMAL:
                                    scanned_str = m_scan_model->getScanValue(idx).toString(10);
                                    current_str = m_scan_model->getCurrentValue(idx).toString(10);
                                    break;
                                case ScanRadix::RADIX_HEXADECIMAL:
                                    scanned_str = m_scan_model->getScanValue(idx).toString(16);
                                    current_str = m_scan_model->getCurrentValue(idx).toString(16);
                                    break;
                                }

                                ImGui::TableNextRow();

                                // Establish row metrics for rendering selection box
                                float row_width  = scan_table_size.x;
                                float row_height = ImGui::GetTextLineHeight() +
                                                   style.CellPadding.y * 2;

                                ImRect row_rect = {};
                                row_rect.Min    = ImVec2{table_start_x, ImGui::GetCursorPosY() +
                                                                         ImGui::GetWindowPos().y -
                                                                         ImGui::GetScrollY()};
                                row_rect.Max    = row_rect.Min + ImVec2{row_width, row_height};

                                bool is_rect_hovered = false;
                                if (outer_window_focused) {
                                    ImGuiContext &g = *GImGui;

                                    double m_x, m_y;
                                    Input::GetMousePosition(m_x, m_y);

                                    ImVec2 mouse_pos = ImVec2{(float)m_x, (float)m_y};

                                    // Clip
                                    ImRect rect_clipped(row_rect.Min, row_rect.Max);

                                    // Hit testing, expanded for touch input
                                    if (rect_clipped.ContainsWithPad(mouse_pos,
                                                                     g.Style.TouchExtraPadding)) {
                                        if (g.MouseViewport->GetMainRect().Overlaps(rect_clipped))
                                            is_rect_hovered = true;
                                    }
                                }

                                bool is_rect_clicked =
                                    is_rect_hovered &&
                                    (Input::GetMouseButtonDown(Input::MouseButton::BUTTON_LEFT) ||
                                     Input::GetMouseButtonDown(Input::MouseButton::BUTTON_RIGHT));

                                ImDrawFlags backdrop_flags = ImDrawFlags_None;

                                ImVec4 selected_col = style.Colors[ImGuiCol_TabActive];
                                ImVec4 hovered_col  = style.Colors[ImGuiCol_TabHovered];

                                selected_col.w = 0.5;
                                hovered_col.w  = 0.5;

                                if (is_rect_hovered) {
                                    ImGui::RenderFrame(row_rect.Min, row_rect.Max,
                                                       ImGui::ColorConvertFloat4ToU32(hovered_col),
                                                       false, 0.0f, backdrop_flags);
                                } else if (m_scan_selection.is_selected(idx)) {
                                    ImGui::RenderFrame(row_rect.Min, row_rect.Max,
                                                       ImGui::ColorConvertFloat4ToU32(selected_col),
                                                       false, 0.0f, backdrop_flags);
                                }

                                ImGui::TableNextColumn();

                                ImGui::Text("%s", addr_str.c_str());

                                ImGui::TableNextColumn();

                                ImGui::Text("%s", scanned_str.c_str());

                                ImGui::TableNextColumn();

                                ImGui::Text("%s", current_str.c_str());

                                // Handle click responses
                                {
                                    if (is_rect_clicked) {
                                        m_any_row_clicked = true;
                                        if ((is_left_click && !is_left_click_release) ||
                                            (is_right_click && !is_right_click_release)) {
                                            m_scan_selection_mgr.actionSelectIndex(
                                                m_scan_selection, idx);
                                        } else if (is_left_click_release ||
                                                   is_right_click_release) {
                                            m_scan_selection_mgr.actionClearRequestExcIndex(
                                                m_scan_selection, idx, is_left_click_release);
                                        }
                                    }
                                }
                            }
                        }
                    }

                    ImGui::EndTable();
                }

                ImVec2 avail       = ImGui::GetContentRegionAvail();
                float button_width = (avail.x - style.ItemSpacing.x * 2) / 3.0f;

                if (ImGui::Button("Add Selection", {button_width, 0.0f}, 5.0f,
                                  ImDrawFlags_RoundCornersAll)) {
                }

                ImGui::SameLine();

                if (ImGui::Button("Add All", {button_width, 0.0f}, 5.0f,
                                  ImDrawFlags_RoundCornersAll)) {
                }

                ImGui::SameLine();

                if (ImGui::Button("Remove Selection", {button_width, 0.0f}, 5.0f,
                                  ImDrawFlags_RoundCornersAll)) {
                }
            }
            ImGui::EndChild();

            ImGui::SameLine();

            if (ImGui::BeginChild("##ScanControlView", {0, 0}, true,
                                  ImGuiWindowFlags_ChildWindow | ImGuiWindowFlags_NoDecoration)) {
                ImVec2 avail = ImGui::GetContentRegionAvail();

                if (m_scan_active) {
                    float button_width = (avail.x - style.ItemSpacing.x * 2) / 3.0f;

                    if (ImGui::Button("Next Scan", {button_width, 0.0f}, 5.0f,
                                      ImDrawFlags_RoundCornersAll)) {
                        u32 address_begin =
                            strlen(m_scan_begin_input.data()) > 0
                                ? std::strtol(m_scan_begin_input.data(), nullptr, 16)
                                : 0x80000000;
                        u32 address_end = strlen(m_scan_end_input.data()) > 0
                                              ? std::strtol(m_scan_end_input.data(), nullptr, 16)
                                              : 0x81800000;

                        std::string str_a = std::string(m_scan_value_input_a.data());
                        std::string str_b = std::string(m_scan_value_input_b.data());

                        int radix = 0;
                        switch (m_scan_radix) {
                        case ScanRadix::RADIX_BINARY:
                            radix = 2;
                            break;
                        case ScanRadix::RADIX_OCTAL:
                            radix = 8;
                            break;
                        case ScanRadix::RADIX_DECIMAL:
                            radix = 10;
                            break;
                        case ScanRadix::RADIX_HEXADECIMAL:
                            radix = 16;
                            break;
                        }

                        m_scan_active = m_scan_model->requestScan(
                            address_begin, address_end - address_begin, m_scan_type,
                            m_scan_operator, str_a, str_b, radix, m_scan_enforce_alignment, false,
                            500000, 16);
                        if (!m_scan_active) {
                            TOOLBOX_ERROR_V(
                                "[MEM_SCANNER] Failed to initialize the requested memory scan!");
                        }
                    }

                    ImGui::SameLine();

                    if (ImGui::Button("Undo Scan", {button_width, 0.0f}, 5.0f,
                                      ImDrawFlags_RoundCornersAll)) {
                        m_scan_model->undoScan();
                        if (!m_scan_model->canUndoScan()) {
                            m_scan_active = false;
                            m_scan_model->reset();
                        }
                    }

                    ImGui::SameLine();

                    if (ImGui::Button("Reset Scan", {button_width, 0.0f}, 5.0f,
                                      ImDrawFlags_RoundCornersAll)) {
                        m_scan_active = false;
                        m_scan_model->reset();
                    }
                } else {
                    float addr_width   = (avail.x - style.ItemSpacing.x) / 2.0f;
                    float button_width = avail.x;

                    ImGui::SetNextItemWidth(addr_width);
                    ImGui::InputTextWithHint("##SearchBeginInput", "Search Begin (Optional)",
                                             m_scan_begin_input.data(), m_scan_begin_input.size(),
                                             ImGuiInputTextFlags_AutoSelectAll |
                                                 ImGuiInputTextFlags_CharsHexadecimal);

                    ImGui::SameLine();

                    ImGui::SetNextItemWidth(addr_width);
                    ImGui::InputTextWithHint("##SearchEndInput", "Search End (Optional)",
                                             m_scan_end_input.data(), m_scan_end_input.size(),
                                             ImGuiInputTextFlags_AutoSelectAll |
                                                 ImGuiInputTextFlags_CharsHexadecimal);

                    if (ImGui::Button("First Scan", {button_width, 0.0f}, 5.0f,
                                      ImDrawFlags_RoundCornersAll)) {
                        u32 address_begin =
                            strlen(m_scan_begin_input.data()) > 0
                                ? std::strtol(m_scan_begin_input.data(), nullptr, 16)
                                : 0x80000000;
                        u32 address_end = strlen(m_scan_end_input.data()) > 0
                                              ? std::strtol(m_scan_end_input.data(), nullptr, 16)
                                              : 0x81800000;

                        std::string str_a = std::string(m_scan_value_input_a.data());
                        std::string str_b = std::string(m_scan_value_input_b.data());

                        int radix = 0;
                        switch (m_scan_radix) {
                        case ScanRadix::RADIX_BINARY:
                            radix = 2;
                            break;
                        case ScanRadix::RADIX_OCTAL:
                            radix = 8;
                            break;
                        case ScanRadix::RADIX_DECIMAL:
                            radix = 10;
                            break;
                        case ScanRadix::RADIX_HEXADECIMAL:
                            radix = 16;
                            break;
                        }
                        m_scan_active = m_scan_model->requestScan(
                            address_begin, address_end - address_begin, m_scan_type,
                            m_scan_operator, str_a, str_b, radix, m_scan_enforce_alignment, true,
                            500000, 16);
                        if (!m_scan_active) {
                            TOOLBOX_ERROR_V(
                                "[MEM_SCANNER] Failed to initialize the requested memory scan!");
                        }
                    }
                }

                if (m_scan_active) {
                    ImGui::BeginDisabled();
                }

                static const std::vector<MetaType> type_list = {
                    MetaType::BOOL,   MetaType::S8,      MetaType::U8,  MetaType::S16,
                    MetaType::U16,    MetaType::U32,     MetaType::F32, MetaType::F64,
                    MetaType::STRING, MetaType::UNKNOWN,
                };

                ImGui::SetNextItemWidth(avail.x);

                std::string_view meta_label = meta_type_name(m_scan_type);
                if (ImGui::BeginCombo("##ScanTypeCombo", meta_label.data(), ImGuiComboFlags_None)) {
                    for (size_t i = 0; i < type_list.size(); ++i) {
                        bool selected          = type_list[i] == m_scan_type;
                        std::string_view mname = meta_type_name(type_list[i]);
                        if (ImGui::Selectable(mname.data(), &selected)) {
                            m_scan_type = type_list[i];
                        }
                    }
                    ImGui::EndCombo();
                }

                if (m_scan_active) {
                    ImGui::EndDisabled();
                }

                if (m_scan_type == MetaType::STRING || m_scan_type == MetaType::UNKNOWN) {
                    m_scan_operator = MemScanModel::ScanOperator::OP_EXACT;

                    ImGui::BeginDisabled();
                    ImGui::SetNextItemWidth(avail.x);
                    if (ImGui::BeginCombo("##ScanOpCombo", "Exact Value", ImGuiComboFlags_None)) {
                        bool selected = true;
                        ImGui::Selectable("Exact Value", &selected);
                        ImGui::EndCombo();
                    }
                    ImGui::EndDisabled();
                } else {
                    static const std::vector<MemScanModel::ScanOperator> scan_list = {
                        MemScanModel::ScanOperator::OP_EXACT,
                        MemScanModel::ScanOperator::OP_INCREASED_BY,
                        MemScanModel::ScanOperator::OP_DECREASED_BY,
                        MemScanModel::ScanOperator::OP_BETWEEN,
                        MemScanModel::ScanOperator::OP_BIGGER_THAN,
                        MemScanModel::ScanOperator::OP_SMALLER_THAN,
                        MemScanModel::ScanOperator::OP_INCREASED,
                        MemScanModel::ScanOperator::OP_DECREASED,
                        MemScanModel::ScanOperator::OP_CHANGED,
                        MemScanModel::ScanOperator::OP_UNCHANGED,
                        MemScanModel::ScanOperator::OP_UNKNOWN_INITIAL,
                    };

                    static const std::vector<std::string> scan_name_list = {
                        "Exact Value",
                        "Increased By",
                        "Decreased By",
                        "Between",
                        "Bigger Than",
                        "Smaller Than",
                        "Increased",
                        "Decreased",
                        "Changed",
                        "Unchanged",
                        "Unknown Initial Value",
                    };

                    if (m_scan_active) {
                        static const std::vector<size_t> scan_list_idxs = {0, 1, 2, 3, 4,
                                                                           5, 6, 7, 8, 9};

                        std::string op_label;
                        for (size_t i = 0; i < scan_list_idxs.size(); ++i) {
                            size_t j = scan_list_idxs[i];
                            if (scan_list[j] == m_scan_operator) {
                                op_label = scan_name_list[j];
                                break;
                            }
                        }

                        if (op_label.empty()) {
                            m_scan_operator = scan_list[scan_list_idxs[0]];
                            op_label        = scan_name_list[scan_list_idxs[0]];
                        }

                        ImGui::SetNextItemWidth(avail.x);
                        if (ImGui::BeginCombo("##ScanOpCombo", op_label.c_str(),
                                              ImGuiComboFlags_None)) {
                            for (size_t i = 0; i < scan_list_idxs.size(); ++i) {
                                size_t j                 = scan_list_idxs[i];
                                bool selected            = scan_list[j] == m_scan_operator;
                                const std::string &mname = scan_name_list[j];
                                if (ImGui::Selectable(mname.c_str(), &selected)) {
                                    m_scan_operator = scan_list[j];
                                }
                            }
                            ImGui::EndCombo();
                        }
                    } else {
                        static const std::vector<size_t> scan_list_idxs = {0, 3, 4, 5, 10};

                        std::string op_label;
                        for (size_t i = 0; i < scan_list_idxs.size(); ++i) {
                            size_t j = scan_list_idxs[i];
                            if (scan_list[j] == m_scan_operator) {
                                op_label = scan_name_list[j];
                                break;
                            }
                        }

                        if (op_label.empty()) {
                            m_scan_operator = scan_list[scan_list_idxs[0]];
                            op_label        = scan_name_list[scan_list_idxs[0]];
                        }

                        ImGui::SetNextItemWidth(avail.x);
                        if (ImGui::BeginCombo("##ScanOpCombo", op_label.c_str(),
                                              ImGuiComboFlags_None)) {
                            for (size_t i = 0; i < scan_list_idxs.size(); ++i) {
                                size_t j                 = scan_list_idxs[i];
                                bool selected            = scan_list[j] == m_scan_operator;
                                const std::string &mname = scan_name_list[j];
                                if (ImGui::Selectable(mname.c_str(), &selected)) {
                                    m_scan_operator = scan_list[j];
                                }
                            }
                            ImGui::EndCombo();
                        }
                    }
                }

                switch (m_scan_operator) {
                case MemScanModel::ScanOperator::OP_INCREASED:
                case MemScanModel::ScanOperator::OP_DECREASED:
                case MemScanModel::ScanOperator::OP_CHANGED:
                case MemScanModel::ScanOperator::OP_UNCHANGED:
                case MemScanModel::ScanOperator::OP_UNKNOWN_INITIAL:
                    break;
                case MemScanModel::ScanOperator::OP_EXACT:
                case MemScanModel::ScanOperator::OP_INCREASED_BY:
                case MemScanModel::ScanOperator::OP_DECREASED_BY:
                case MemScanModel::ScanOperator::OP_BIGGER_THAN:
                case MemScanModel::ScanOperator::OP_SMALLER_THAN: {
                    float input_width = avail.x;
                    ImGui::SetNextItemWidth(input_width);
                    ImGui::InputText("##ScanValueInputA", m_scan_value_input_a.data(),
                                     m_scan_value_input_a.size(),
                                     ImGuiInputTextFlags_AutoSelectAll |
                                         ImGuiInputTextFlags_CharsHexadecimal |
                                         ImGuiInputTextFlags_CharsScientific);
                    break;
                }
                case MemScanModel::ScanOperator::OP_BETWEEN: {
                    float input_width = (avail.x - style.ItemSpacing.x) / 2.0f;
                    ImGui::SetNextItemWidth(input_width);
                    ImGui::InputText("##ScanValueInputA", m_scan_value_input_a.data(),
                                     m_scan_value_input_a.size(),
                                     ImGuiInputTextFlags_AutoSelectAll |
                                         ImGuiInputTextFlags_CharsHexadecimal |
                                         ImGuiInputTextFlags_CharsScientific);
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(input_width);
                    ImGui::InputText("##ScanValueInputB", m_scan_value_input_b.data(),
                                     m_scan_value_input_b.size(),
                                     ImGuiInputTextFlags_AutoSelectAll |
                                         ImGuiInputTextFlags_CharsHexadecimal |
                                         ImGuiInputTextFlags_CharsScientific);
                    break;
                }
                }

                if (m_scan_active) {
                    ImGui::BeginDisabled();
                }

                ImGui::Text("Base To Use:");

                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {10.0f, 10.0f});
                ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 2.0f);
                ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 5.0f);
                if (ImGui::BeginChild(
                        "Base to Use##xyz",
                        {avail.x, ImGui::GetTextLineHeight() + style.FramePadding.y * 2 +
                                      style.WindowPadding.y * 2},
                        true, ImGuiWindowFlags_ChildWindow | ImGuiWindowFlags_NoDecoration)) {
                    if (ImGui::RadioButton("Decimal", m_scan_radix == ScanRadix::RADIX_DECIMAL)) {
                        m_scan_radix = ScanRadix::RADIX_DECIMAL;
                    }

                    ImGui::SameLine();

                    if (ImGui::RadioButton("Hexadecimal",
                                           m_scan_radix == ScanRadix::RADIX_HEXADECIMAL)) {
                        m_scan_radix = ScanRadix::RADIX_HEXADECIMAL;
                    }

                    ImGui::SameLine();

                    if (ImGui::RadioButton("Octal", m_scan_radix == ScanRadix::RADIX_OCTAL)) {
                        m_scan_radix = ScanRadix::RADIX_OCTAL;
                    }

                    ImGui::SameLine();

                    if (ImGui::RadioButton("Binary", m_scan_radix == ScanRadix::RADIX_BINARY)) {
                        m_scan_radix = ScanRadix::RADIX_BINARY;
                    }
                }
                ImGui::EndChild();
                ImGui::PopStyleVar(3);

                if (m_scan_active) {
                    ImGui::EndDisabled();
                }

                ImGui::Checkbox("Enforce Alignment##xyz", &m_scan_enforce_alignment);
            }
            ImGui::EndChild();
        }
        ImGui::EndChild();
        ImGui::PopStyleVar();
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

            const bool window_focused = ImGui::IsWindowFocused();
            const bool window_hovered = ImGui::IsWindowHovered();

            if (!window_focused) {
                m_address_cursor        = 0;
                m_address_cursor_nibble = 0;
            }

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

            // Process controls
            if (m_address_cursor != 0 && ImGui::IsWindowFocused()) {
                // TODO: Enhance held input logic so it doesn't
                // stutter and drop inputs during many keys pressing across frames.
                KeyModifiers mods    = GetPressedKeyModifiers();
                KeyModifier ctrl_mod = mods & KeyModifier::KEY_CTRL;
                if (ctrl_mod == KeyModifier::KEY_NONE) {
                    processKeyInputsAtAddress(column_count);
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
            if (window_hovered) {
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

        const AppSettings &settings =
            GUIApplication::instance().getSettingsManager().getCurrentProfile();

        m_watch_model->setRefreshRate(settings.m_dolphin_refresh_rate);

        if (ImGui::BeginChild("##MemoryWatchList", {0, 0}, true,
                              ImGuiWindowFlags_ChildWindow | ImGuiWindowFlags_NoDecoration)) {
            // ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {2, 2});
            // ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
            // ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.0f);
            // ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {2, 2});
            // ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 3.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, {4.0f, 6.0f});

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

            // ImGui::Separator();

            // Name | Type | Address | Lock | Value
            // ------------------------------------
            // ---
            const int columns           = 5;
            const ImGuiTableFlags flags = ImGuiTableFlags_BordersOuter |
                                          ImGuiTableFlags_BordersInnerV |
                                          ImGuiTableFlags_Resizable |
                                          ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollY;
            const ImVec2 desired_size = ImGui::GetContentRegionAvail();

            ImVec2 table_pos  = ImGui::GetWindowPos() + ImGui::GetCursorPos(),
                   table_size = desired_size;
            if (ImGui::BeginTable("##MemoryWatchTable", 5, flags, desired_size)) {
                ImGui::TableSetupScrollFreeze(5, 1);

                ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 120.0f);
                ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 80.0f);
                ImGui::TableSetupColumn("Address", ImGuiTableColumnFlags_WidthFixed, 80.0f);
                ImGui::TableSetupColumn("Lock", ImGuiTableColumnFlags_WidthFixed, 50.0f);
                ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

                float table_start_x = ImGui::GetCursorPosX() + ImGui::GetWindowPos().x;
                float table_width   = ImGui::GetContentRegionAvail().x;

                ImGui::TableHeadersRow();

                int64_t row_count = m_watch_proxy_model->getRowCount(ModelIndex());
                for (int64_t row = 0; row < row_count; ++row) {
                    ModelIndex index = m_watch_proxy_model->getIndex(row, 0);
                    if (m_watch_proxy_model->isIndexGroup(index)) {
                        renderWatchGroup(index, 0, table_start_x, table_width);
                    } else {
                        renderMemoryWatch(index, 0, table_start_x, table_width);
                    }
                }

                ImGui::EndTable();
            }

            ImGui::PopStyleVar(1);

            bool left_click  = Input::GetMouseButtonDown(Input::MouseButton::BUTTON_LEFT);
            bool right_click = Input::GetMouseButtonDown(Input::MouseButton::BUTTON_RIGHT);

            ImRect window_rect  = ImRect{table_pos, table_pos + table_size};
            bool mouse_captured = ImGui::IsMouseHoveringRect(window_rect.Min, window_rect.Max);

            if (!m_add_group_dialog.is_open() && !m_add_watch_dialog.is_open()) {
                if (!m_any_row_clicked && mouse_captured && (left_click || right_click) &&
                    Input::GetPressedKeyModifiers() == KeyModifier::KEY_NONE) {
                    m_watch_selection.clearSelection();
                }
            }
        }
        ImGui::EndChild();
    }

    void DebuggerWindow::renderMemoryWatch(const ModelIndex &watch_idx, int depth,
                                           float table_start_x, float table_width) {
        if (!m_watch_proxy_model->validateIndex(watch_idx)) {
            return;
        }

        std::string name = m_watch_proxy_model->getDisplayText(watch_idx);
        if (name.empty()) {
            return;  // Skip empty groups
        }

        DolphinHookManager &manager = DolphinHookManager::instance();
        void *mem_view              = manager.getMemoryView();
        size_t mem_size             = manager.getMemorySize();

        MetaValue meta_value = m_watch_proxy_model->getWatchValueMeta(watch_idx);
        u32 address          = m_watch_proxy_model->getWatchAddress(watch_idx);
        u32 size             = m_watch_proxy_model->getWatchSize(watch_idx);
        bool locked          = m_watch_proxy_model->getWatchLock(watch_idx);

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

        bool is_selected = m_watch_selection.is_selected(watch_idx);

        ImGui::TableNextRow();

        ImVec2 row_button_pos;
        ImVec2 row_button_size = {text_size.y, text_size.y};
        ImRect row_button_bb;

        // Establish row metrics for rendering selection box
        float row_width  = table_width;
        float row_height = text_size.y + style.CellPadding.y * 2 + style.ItemSpacing.y;

        ImRect row_rect = {};
        row_rect.Min    = ImVec2{table_start_x, ImGui::GetCursorPosY() + ImGui::GetWindowPos().y -
                                                 ImGui::GetScrollY()};
        row_rect.Max    = row_rect.Min + ImVec2{row_width, row_height};

        bool is_rect_hovered = false;
        if (ImGui::IsWindowFocused()) {
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

        ModelIndex src_idx  = m_watch_proxy_model->toSourceIndex(watch_idx);
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

        ModelIndex source_group_idx = m_watch_proxy_model->toSourceIndex(watch_idx);

        if (ImGui::TableNextColumn()) {
            is_lock_pressed = ImGui::Checkbox("##lockbox", &locked);
            if (is_lock_pressed) {
                recursiveLock(source_group_idx, locked);
            }

            is_lock_hovered = ImGui::IsItemHovered();
        }

        if (ImGui::TableNextColumn()) {
            const u32 start_addr = 0x80000000;
            const u32 end_addr   = start_addr | mem_size;

            if (mem_size == 0) {
                ImGui::Text("Dolphin Not Found");
            } else if (address < start_addr || address + size >= end_addr) {
                ImGui::Text("Invalid Watch");
            } else {
                f32 column_width = ImGui::GetContentRegionAvail().x;
                renderPreview(column_width, meta_value);
            }
        }

        // Handle click responses
        {
            if (is_rect_clicked && !is_collapse_hovered && !is_lock_hovered) {
                m_any_row_clicked = true;
                if ((is_left_click && !is_left_click_release) ||
                    (is_right_click && !is_right_click_release)) {
                    m_watch_selection_mgr.actionSelectIndex(m_watch_selection, watch_idx);
                } else if (is_left_click_release || is_right_click_release) {
                    m_watch_selection_mgr.actionClearRequestExcIndex(m_watch_selection, watch_idx,
                                                                     is_left_click_release);
                }
            }
        }

        ImGui::PopID();
    }

    void DebuggerWindow::renderWatchGroup(const ModelIndex &group_idx, int depth,
                                          float table_start_x, float table_width) {
        if (!m_watch_proxy_model->validateIndex(group_idx)) {
            return;
        }

        bool open = false;

        std::string name = m_watch_proxy_model->getDisplayText(group_idx);
        if (name.empty()) {
            return;  // Skip empty groups
        }

        bool locked = m_watch_proxy_model->getWatchLock(group_idx);

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

        bool is_selected = m_watch_selection.is_selected(group_idx);

        ImGui::TableNextRow();

        ImVec2 row_button_pos;
        ImVec2 row_button_size = {text_size.y, text_size.y};
        ImRect row_button_bb;

        // Establish row metrics for rendering selection box
        float row_width  = table_width;
        float row_height = text_size.y + style.CellPadding.y * 2 + style.ItemSpacing.y;

        ImRect row_rect = {};
        row_rect.Min    = ImVec2{table_start_x, ImGui::GetCursorPosY() + ImGui::GetWindowPos().y -
                                                 ImGui::GetScrollY()};
        row_rect.Max    = row_rect.Min + ImVec2{row_width, row_height};

        bool is_rect_hovered = false;
        if (ImGui::IsWindowFocused()) {
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

        ModelIndex src_idx  = m_watch_proxy_model->toSourceIndex(group_idx);
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
            row_button_pos.y -= ImGui::GetScrollY();
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
            ImGui::RenderArrow(window->DrawList, row_button_pos,
                               ImGui::ColorConvertFloat4ToU32({1.0, 1.0, 1.0, 1.0}), direction);

            ImGui::SameLine();

            ImGui::Text(name.c_str());
#endif
        }

        // ImGui::PopStyleColor(3);

        ImGui::TableNextColumn();
        ImGui::TableNextColumn();

        ModelIndex source_group_idx = m_watch_proxy_model->toSourceIndex(group_idx);

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
                    m_watch_selection_mgr.actionSelectIndex(m_watch_selection, group_idx);
                } else if (is_left_click_release || is_right_click_release) {
                    m_watch_selection_mgr.actionClearRequestExcIndex(m_watch_selection, group_idx,
                                                                     is_left_click_release);
                }
            }
        }

        if (open) {
            for (size_t i = 0; i < m_watch_proxy_model->getRowCount(group_idx); ++i) {
                ModelIndex index = m_watch_proxy_model->getIndex(i, 0, group_idx);
                if (m_watch_proxy_model->isIndexGroup(index)) {
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

        m_initialized_splitters = false;

        m_watch_model = make_referable<WatchDataModel>();
        m_watch_model->initialize();

        const AppSettings &settings =
            GUIApplication::instance().getSettingsManager().getCurrentProfile();

        m_watch_model->setRefreshRate(settings.m_dolphin_refresh_rate);

        m_watch_proxy_model = make_referable<WatchDataModelSortFilterProxy>();
        m_watch_proxy_model->setSourceModel(m_watch_model);
        m_watch_proxy_model->setSortOrder(ModelSortOrder::SORT_ASCENDING);
        m_watch_proxy_model->setSortRole(WatchModelSortRole::SORT_ROLE_NAME);

        m_watch_selection.setModel(m_watch_proxy_model);

        m_scan_model = make_referable<MemScanModel>();
        m_scan_model->initialize();

        m_scan_selection.setModel(m_scan_model);

        m_scan_active = false;
        m_scan_begin_input.fill('\0');
        m_scan_end_input.fill('\0');
        m_scan_operator = MemScanModel::ScanOperator::OP_EXACT;
        m_scan_value_input_a.fill('\0');
        m_scan_value_input_b.fill('\0');
        m_scan_radix             = ScanRadix::RADIX_DECIMAL;
        m_scan_enforce_alignment = true;

        m_add_group_dialog.setInsertPolicy(
            AddGroupDialog::InsertPolicy::INSERT_AFTER);  // Insert after the current group
        m_add_group_dialog.setFilterPredicate(
            [this](std::string_view group_name, ModelIndex group_idx) -> bool {
                // Check if the group name is unique
                ModelIndex src_idx = m_watch_proxy_model->toSourceIndex(group_idx);
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
                ModelIndex src_idx = m_watch_proxy_model->toSourceIndex(group_idx);
                for (size_t i = 0; i < m_watch_model->getRowCount(src_idx); ++i) {
                    ModelIndex child_idx   = m_watch_model->getIndex(i, 0, src_idx);
                    std::string child_name = m_watch_model->getDisplayText(child_idx);
                    if (child_name == watch_name) {
                        return false;
                    }
                }
                return true;  // Watch name is unique
            });
        m_add_watch_dialog.setActionOnAccept([&](ModelIndex group_idx, size_t row,
                                                 AddWatchDialog::InsertPolicy policy,
                                                 std::string_view watch_name, MetaType type,
                                                 const std::vector<u32> &pointer_chain, u32 size,
                                                 bool is_pointer) {
            insertWatch(group_idx, row, policy, watch_name, type, pointer_chain, size, is_pointer);
        });

        m_fill_bytes_dialog.setInsertPolicy(FillBytesDialog::InsertPolicy::INSERT_CONSTANT);
        m_fill_bytes_dialog.setActionOnAccept(
            [&](const AddressSpan &span, FillBytesDialog::InsertPolicy policy, u8 byte_value) {
                switch (policy) {
                case FillBytesDialog::InsertPolicy::INSERT_CONSTANT: {
                    FillAddressSpan(span, byte_value, [](u8 val) { return val; });
                    break;
                }
                case FillBytesDialog::InsertPolicy::INSERT_INCREMENT: {
                    FillAddressSpan(span, byte_value,
                                    [](u8 val) { return val == 255 ? 255 : val + 1; });
                    break;
                }
                case FillBytesDialog::InsertPolicy::INSERT_DECREMENT: {
                    FillAddressSpan(span, byte_value,
                                    [](u8 val) { return val == 0 ? 0 : val - 1; });
                    break;
                }
                }
            });

        buildContextMenus();
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

    void DebuggerWindow::buildContextMenus() {
        m_byte_view_context_menu.addOption(
            "Copy Selection as Bytes", {KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_C},
            [](AddressSpan span) { CopyBytesFromAddressSpan(span); });

        m_byte_view_context_menu.addOption(
            "Copy Selection as ASCII",
            {KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_LEFTSHIFT, KeyCode::KEY_C},
            [](AddressSpan span) { CopyASCIIFromAddressSpan(span); });

        m_byte_view_context_menu.addDivider();

        m_byte_view_context_menu.addOption(
            "Fill Selection...", {KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_LEFTSHIFT, KeyCode::KEY_F},
            [&](AddressSpan span) { m_fill_bytes_dialog.open(); });

        m_byte_view_context_menu.addDivider();

        m_byte_view_context_menu.addOption(
            "Add Selection as Bytes...", {KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_B},
            [&](AddressSpan span) {
                u32 begin = std::min<u32>(span.m_begin, span.m_end);
                u32 end   = std::max<u32>(span.m_begin, span.m_end);
                begin     = std::clamp<u32>(begin, 0x80000000, 0x817FFFFF);
                end       = std::clamp<u32>(end, 0x80000000, 0x817FFFFF);
                m_add_watch_dialog.openToAddressAsBytes(begin, (size_t)end - begin);
            });

        m_byte_view_context_menu.addOption(
            "Add Watch at Cursor Address...", {KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_W},
            [&](AddressSpan span) {
                u32 begin = std::min<u32>(span.m_begin, span.m_end);
                begin     = std::clamp<u32>(begin, 0x80000000, 0x817FFFFF);
                m_add_watch_dialog.openToAddress(begin);
            });

        // -----------------------------------------

        m_ascii_view_context_menu.addOption(
            "Copy Selection as Bytes",
            {KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_LEFTSHIFT, KeyCode::KEY_C},
            [](AddressSpan span) { CopyBytesFromAddressSpan(span); });

        m_ascii_view_context_menu.addOption(
            "Copy Selection as ASCII", {KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_C},
            [](AddressSpan span) { CopyASCIIFromAddressSpan(span); });

        m_ascii_view_context_menu.addDivider();

        m_ascii_view_context_menu.addOption(
            "Fill Selection...", {KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_LEFTSHIFT, KeyCode::KEY_F},
            [&](AddressSpan span) { m_fill_bytes_dialog.open(); });

        m_ascii_view_context_menu.addDivider();

        m_ascii_view_context_menu.addOption(
            "Add Selection as Bytes...", {KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_B},
            [&](AddressSpan span) {
                u32 begin = std::min<u32>(span.m_begin, span.m_end);
                u32 end   = std::max<u32>(span.m_begin, span.m_end);
                begin     = std::clamp<u32>(begin, 0x80000000, 0x817FFFFF);
                end       = std::clamp<u32>(end, 0x80000000, 0x817FFFFF);
                m_add_watch_dialog.openToAddressAsBytes(begin, (size_t)end - begin);
            });

        m_ascii_view_context_menu.addOption(
            "Add Watch at Cursor Address...", {KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_W},
            [&](AddressSpan span) {
                u32 begin = std::min<u32>(span.m_begin, span.m_end);
                begin     = std::clamp<u32>(begin, 0x80000000, 0x817FFFFF);
                m_add_watch_dialog.openToAddress(begin);
            });
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
        ModelIndex src_group_idx  = m_watch_proxy_model->toSourceIndex(group_index);
        ModelIndex target_idx     = m_watch_proxy_model->getIndex(row, 0, group_index);
        ModelIndex src_target_idx = m_watch_proxy_model->toSourceIndex(target_idx);

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
                                           const std::vector<u32> &pointer_chain, u32 watch_size,
                                           bool is_pointer) {
        ModelIndex src_group_idx  = m_watch_proxy_model->toSourceIndex(group_index);
        ModelIndex target_idx     = m_watch_proxy_model->getIndex(row, 0, group_index);
        ModelIndex src_target_idx = m_watch_proxy_model->toSourceIndex(target_idx);

        int64_t src_row = m_watch_proxy_model->getRow(src_target_idx);
        if (src_row == -1) {
            src_row = 0;
        }

        int64_t sibling_count = m_watch_model->getRowCount(src_group_idx);

        switch (policy) {
        case AddWatchDialog::InsertPolicy::INSERT_BEFORE: {
            return m_watch_model->makeWatchIndex(std::string(watch_name), watch_type, pointer_chain,
                                                 watch_size, is_pointer,
                                                 std::min(sibling_count, src_row), src_group_idx);
        }
        case AddWatchDialog::InsertPolicy::INSERT_AFTER: {
            return m_watch_model->makeWatchIndex(
                std::string(watch_name), watch_type, pointer_chain, watch_size, is_pointer,
                std::min(sibling_count, src_row + 1), src_group_idx);
        }
        case AddWatchDialog::InsertPolicy::INSERT_CHILD: {
            int64_t row_count = m_watch_model->getRowCount(src_target_idx);
            return m_watch_model->makeWatchIndex(std::string(watch_name), watch_type, pointer_chain,
                                                 watch_size, is_pointer, row_count, src_target_idx);
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
        case MetaType::UNKNOWN:
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
                                   /*ImGuiColorEditFlags_NoPicker*/ ImGuiColorEditFlags_NoInputs)) {
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
                                   /*ImGuiColorEditFlags_NoPicker*/ ImGuiColorEditFlags_NoInputs)) {
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
            glm::vec3 vec = value.get<glm::vec3>().value_or(glm::vec3());

            for (int i = 0; i < 3; ++i) {
                MetaValue flt_val = MetaValue(vec[i]);

                char value_buf[32] = {};
                calcPreview(value_buf, sizeof(value_buf), flt_val);

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

            Transform t = value.get<Transform>().value_or(Transform());

            for (int i = 0; i < 3; ++i) {
                MetaValue flt_val = MetaValue(t.m_translation[i]);

                char value_buf[32] = {};
                calcPreview(value_buf, sizeof(value_buf), flt_val);

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
                MetaValue flt_val = MetaValue(t.m_rotation[i]);

                char value_buf[32] = {};
                calcPreview(value_buf, sizeof(value_buf), flt_val);

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
                MetaValue flt_val = MetaValue(t.m_scale[i]);

                char value_buf[32] = {};
                calcPreview(value_buf, sizeof(value_buf), flt_val);

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

        glm::mat3x4 value_mtx = value.get<glm::mat3x4>().value_or(glm::mat3x4());

        ImGui::BeginGroup();
        {
            for (int j = 0; j < 3; ++j) {
                for (int i = 0; i < 4; ++i) {
                    MetaValue flt_val = MetaValue(value_mtx[j][i]);

                    char value_buf[32] = {};
                    calcPreview(value_buf, sizeof(value_buf), flt_val);

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
    }

    void DebuggerWindow::calcPreview(char *preview_out, size_t preview_size,
                                     const MetaValue &value) const {
        if (preview_out == nullptr || preview_size == 0) {
            return;
        }

        size_t address_size = value.computeSize();
        // if (value.type() == MetaType::UNKNOWN) {
        //
        // }

        if (address_size == 0) {
            snprintf(preview_out, preview_size, "???");
            return;
        }

        if (preview_size < address_size) {
            snprintf(preview_out, preview_size, "???");
            return;
        }

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
        case MetaType::UNKNOWN: {
            size_t tmp_size   = std::min<size_t>(preview_size / 3, address_size);
            size_t final_size = std::min<size_t>(preview_size, address_size);

#if 0
            communicator.readBytes(tmp_buf, true_address, tmp_size)
                .or_else([&](const BaseError &err) {
                    snprintf(preview_out, preview_size, "Error: %s", err.m_message[0].c_str());
                    return Result<void>{};
                });
#else
            const char *byte_buf = value.buf().buf<char>();
#endif

            size_t i, j = 0;
            for (i = 0; i < tmp_size; ++i) {
                uint8_t ch = ((uint8_t *)byte_buf)[i];
                snprintf(preview_out + j, preview_size - j, "%02X ", ch);
                j += 3;
            }

            if (j > 0) {
                preview_out[j - 1] = '\0';
            } else {
                preview_out[j] = '\0';
            }

            break;
        }
        default:
            snprintf(preview_out, preview_size, "Unsupported type");
            break;
        }
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

    void DebuggerWindow::overwriteCharAtCursor(char char_value) {
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
                    u8 new_value = OverwriteByte(value, m_address_cursor_nibble / 2, char_value);
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
                    u16 new_value = OverwriteByte(value, m_address_cursor_nibble / 2, char_value);
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
                    u32 new_value = OverwriteByte(value, m_address_cursor_nibble / 2, char_value);
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
                    u64 new_value = OverwriteByte(value, m_address_cursor_nibble / 2, char_value);
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

    void DebuggerWindow::processKeyInputsAtAddress(int32_t column_count) {
        KeyCodes pressed_keys = Input::GetPressedKeys();
        KeyModifiers mods     = GetPressedKeyModifiers();
        KeyModifier shift_mod = mods & KeyModifier::KEY_SHIFT;
        KeyModifier ctrl_mod  = mods & KeyModifier::KEY_CTRL;

        bool caps_lock = Input::IsCapsLockOn();
        if (caps_lock) {
            shift_mod ^= KeyModifier::KEY_SHIFT;
        }

        bool cursor_moves = m_cursor_step_timer > 0.10f || m_cursor_step_timer == -0.3f;
        bool key_held     = false;

        auto advanceCursorNibble = [this]() {
            if (m_address_cursor_nibble + 1 >= m_byte_width * 2) {
                if (m_address_cursor + m_byte_width <= 0x81800000 - m_byte_width) {
                    m_address_cursor += m_byte_width;
                    m_address_cursor_nibble = 0;
                }
            } else {
                m_address_cursor_nibble += 1;
            }
        };

        auto advanceCursorChar = [this]() {
            if (m_address_cursor_nibble + 2 >= m_byte_width * 2) {
                if (m_address_cursor + m_byte_width <= 0x81800000 - m_byte_width) {
                    m_address_cursor += m_byte_width;
                    m_address_cursor_nibble = (m_address_cursor_nibble + 2) - m_byte_width * 2;
                }
            } else {
                m_address_cursor_nibble += 2;
            }
        };

        auto reverseCursorNibble = [this]() {
            if (m_address_cursor_nibble == 0) {
                if (m_address_cursor - m_byte_width >= 0x80000000) {
                    m_address_cursor -= m_byte_width;
                    m_address_cursor_nibble = m_byte_width * 2 - 1;
                }
            } else {
                m_address_cursor_nibble -= 1;
            }
        };

        auto reverseCursorChar = [this]() {
            if (m_address_cursor_nibble < 2) {
                if (m_address_cursor - m_byte_width >= 0x80000000) {
                    m_address_cursor -= m_byte_width;
                    m_address_cursor_nibble = m_byte_width * 2 - (2 - m_address_cursor_nibble);
                }
            } else {
                m_address_cursor_nibble -= 2;
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

        for (const KeyCode &key : pressed_keys) {
            if (m_selection_was_ascii) {
                char ch = GetCharForKeyCode(key, shift_mod);
                if (ch != -1) {
                    key_held = true;
                    if (!cursor_moves) {
                        continue;
                    }
                    overwriteCharAtCursor(ch);
                    advanceCursorChar();
                }

                if (key == KeyCode::KEY_RIGHT) {
                    key_held = true;
                    if (!cursor_moves) {
                        continue;
                    }
                    advanceCursorChar();
                } else if (key == KeyCode::KEY_LEFT) {
                    key_held = true;
                    if (!cursor_moves) {
                        continue;
                    }
                    reverseCursorChar();
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
            } else {
                if (key >= KeyCode::KEY_D0 && key <= KeyCode::KEY_D9) {
                    key_held = true;
                    if (!cursor_moves) {
                        continue;
                    }
                    u8 nibble_value = (u8)key - (u8)KeyCode::KEY_D0;
                    overwriteNibbleAtCursor(nibble_value);
                    advanceCursorNibble();
                } else if (key >= KeyCode::KEY_A && key <= KeyCode::KEY_F) {
                    key_held = true;
                    if (!cursor_moves) {
                        continue;
                    }
                    u8 nibble_value = ((u8)key - (u8)KeyCode::KEY_A) + 10;
                    overwriteNibbleAtCursor(nibble_value);
                    advanceCursorNibble();
                }

                if (key == KeyCode::KEY_RIGHT) {
                    key_held = true;
                    if (!cursor_moves) {
                        continue;
                    }
                    advanceCursorNibble();
                } else if (key == KeyCode::KEY_LEFT) {
                    key_held = true;
                    if (!cursor_moves) {
                        continue;
                    }
                    reverseCursorNibble();
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

    void DebuggerWindow::CopyBytesFromAddressSpan(const AddressSpan &span) {
        DolphinCommunicator &communicator = GUIApplication::instance().getDolphinCommunicator();

        u32 begin = std::min<u32>(span.m_begin, span.m_end);
        u32 end   = std::max<u32>(span.m_begin, span.m_end);
        begin     = std::clamp<u32>(begin, 0x80000000, 0x817FFFFF);
        end       = std::clamp<u32>(end, 0x80000000, 0x817FFFFF);
        u32 width = std::min<u32>(end - begin, 0x10000);  // Investigate a sensible limit?

        std::string text;
        text.resize(width);

        std::string bytes;
        bytes.reserve(width * 4);

        communicator.readBytes(text.data(), begin, width);

        size_t i = 0;
        for (char ch : text) {
            std::string byte = std::format("{:02X}", ch);
            bytes.append(byte);
            if ((++i % 16) == 0) {
                bytes.push_back('\n');
            } else {
                bytes.push_back(' ');
            }
        }

        // Remove trailing space.
        if (bytes.length() > 0) {
            bytes.pop_back();
        }

        SystemClipboard::instance().setText(bytes).or_else([](const ClipboardError &error) {
            LogError(error);
            return Result<void, ClipboardError>();
        });
    }

    void DebuggerWindow::CopyASCIIFromAddressSpan(const AddressSpan &span) {
        DolphinCommunicator &communicator = GUIApplication::instance().getDolphinCommunicator();

        u32 begin = std::min<u32>(span.m_begin, span.m_end);
        u32 end   = std::max<u32>(span.m_begin, span.m_end);
        begin     = std::clamp<u32>(begin, 0x80000000, 0x817FFFFF);
        end       = std::clamp<u32>(end, 0x80000000, 0x817FFFFF);
        u32 width = std::min<u32>(end - begin, 0x10000);  // Investigate a sensible limit?

        std::string text;
        text.resize(width);

        communicator.readBytes(text.data(), begin, width);

        SystemClipboard::instance().setText(text).or_else([](const ClipboardError &error) {
            LogError(error);
            return Result<void, ClipboardError>();
        });
    }

    void DebuggerWindow::FillAddressSpan(const AddressSpan &span, u8 initial_val,
                                         transformer_t transformer) {
        DolphinCommunicator &communicator = GUIApplication::instance().getDolphinCommunicator();

        u32 start_addr = std::min<u32>(span.m_begin, span.m_end);
        u32 end_addr   = std::max<u32>(span.m_begin, span.m_end);

        u8 value = initial_val;
        while (start_addr < end_addr) {
            communicator.write<u8>(start_addr, value);
            value = transformer(value);
            start_addr += 1;
        }
    }

}  // namespace Toolbox::UI
