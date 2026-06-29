#pragma once

#include "bmg/bmg.hpp"

#include "gui/appmain/application.hpp"
#include "gui/appmain/bmgeditor/window.hpp"
#include "gui/logging/errors.hpp"
#include "spline.hpp"

constexpr float SplitterWidth = 6.0f;

namespace Toolbox::UI {

    class BMGEditorViewNPC final : public IBMGEditorView {
    public:
        BMGEditorViewNPC(bool is_mirrored) {
            static const std::vector<CatmullSpline2D::ControlPoint> control_points = {
                {-1.0f, 0.8f},
                {0.0f,  0.0f},
                {1.0f,  0.0f},
                {2.0f,  0.8f},
            };

            ScopePtr<CatmullSpline2D> spline = make_scoped<CatmullSpline2D>(control_points);
            spline->setScale(400.0f);
            // spline->setRotate(IM_PI * 0.15f);

            m_talk_spline = std::move(spline);

            ResourceManager &manager = MainApplication::instance().getResourceManager();

            const UUID64 editor_dir_uuid = manager.getResourcePathUUID("Images/BMGEditor");

            m_talk_line_image =
                manager.getImageHandle("message_back.png", editor_dir_uuid).value_or(nullptr);
            m_talk_button_image = manager.getImageHandle("message_button_back.png", editor_dir_uuid)
                                      .value_or(nullptr);
            m_talk_arrow_image =
                manager.getImageHandle("message_cursor.png", editor_dir_uuid).value_or(nullptr);
            m_talk_exit_image =
                manager.getImageHandle("message_return.png", editor_dir_uuid).value_or(nullptr);
            m_talk_option_image = manager.getImageHandle("message_option_back.png", editor_dir_uuid)
                                      .value_or(nullptr);
        }
        ~BMGEditorViewNPC() override = default;

        size_t getPageCount(const BMG::MessageData::Entry &message) const override { return 0; }

        bool render(const ImRect &render_rect, const BMG::MessageData::Entry &message,
                    size_t current_page) override {
            m_talk_options[0] = "";
            m_talk_options[1] = "";

            const std::string &message_str = message.getMessage().getString();
            if (message_str.empty()) {
                return false;
            }

            const float render_factor = render_rect.GetSize().x / 600.0f;

            const float font_size        = 15.0f;
            const float scaled_font_size = font_size * render_factor;

            ImGui::SetCursorScreenPos(render_rect.Min);

            std::vector<std::string_view> message_lines;
            message_lines.reserve(64);

            size_t newline_pos = 0;
            size_t current_pos = 0;
            while (true) {
                newline_pos = std::min(message_str.find('\n', current_pos), message_str.size());

                message_lines.push_back(
                    std::string_view(message_str.c_str() + current_pos, newline_pos - current_pos));

                if (newline_pos + 1 >= message_str.size()) {
                    break;
                }

                current_pos = newline_pos + 1;
            }

            const size_t render_line_base = (current_page * 3);
            const size_t render_line_count =
                std::clamp<size_t>(message_lines.size() - render_line_base, 0, 3);

            ImFont *font = FontManager::instance().getFont("BMGEditor/sms_message");
            ImGui::PushFont(font, scaled_font_size);

            const ImVec4 text_color_backup = ImGui::GetStyle().Colors[ImGuiCol_Text];

            ImagePainter message_painter;
            message_painter.setRotation(IM_PI * 0.10f);
            message_painter.setTintColor({1.0f, 1.0f, 1.0f, 0.8f});

            for (size_t i = 0; i < render_line_count; ++i) {
                ImGui::SetCursorScreenPos(render_rect.Min);

                const ImVec2 spline_translation = {(285.0f - i * 7.0f) * render_factor,
                                                   (22.0f + i * 23.0f) * render_factor};

                if (m_talk_line_image) {
                    const auto [image_width, image_height] = m_talk_line_image->size();
                    const ImVec2 render_size =
                        ImVec2(220.0f * render_factor,
                               image_height * (220.0f / image_width) * render_factor);
                    message_painter.render(*m_talk_line_image,
                                           render_rect.Min + spline_translation +
                                               ImVec2(-5.0f, -22.0f) * render_factor,
                                           render_size);
                }

                m_talk_spline->setTranslate(spline_translation.x, spline_translation.y);
                m_talk_spline->setRotate(IM_PI * 0.1125f);
                m_talk_spline->setScale(210.0f * render_factor);

                std::string_view line_text = message_lines[i];
                renderLine(line_text);
            }

            ImGui::GetStyle().Colors[ImGuiCol_Text] = text_color_backup;

            ImGui::PopFont();
            return true;
        }

        void renderLine(std::string_view line) {
            size_t current_pos = 0;
            size_t end_pos     = 0;

            float spline_lerp = 0.0f;

            while (true) {
                if (current_pos >= line.size()) {
                    break;
                }

                if (spline_lerp >= 1.0f) {
                    break;
                }

                if (line.at(current_pos) == '{') {
                    size_t r_pos = line.find('}', current_pos);
                    if (r_pos != std::string_view::npos) {
                        std::string_view command =
                            line.substr(current_pos, r_pos - current_pos + 1);
                        spline_lerp = renderCommand(spline_lerp, command);
                        current_pos = r_pos + 1;
                        continue;
                    }
                }

                end_pos = line.find('{', current_pos);
                if (end_pos == std::string_view::npos) {
                    std::string_view trailing = line.substr(current_pos);
                    spline_lerp = ImGui::TextSplineUnformattedEx(spline_lerp, m_talk_spline.get(),
                                                                 trailing.data(),
                                                                 trailing.data() + trailing.size());
                    break;
                }

                std::string_view slice = line.substr(current_pos, end_pos - current_pos);
                spline_lerp            = ImGui::TextSplineUnformattedEx(
                    spline_lerp, m_talk_spline.get(), slice.data(), slice.data() + slice.size());
                current_pos = end_pos;
            }
        }

        float renderCommand(float spline_lerp, std::string_view command) {
            if (command.starts_with("{speed:")) {
                if (command.size() <= 7) {
                    TOOLBOX_WARN("[BMG_PREVIEW] Received incomplete speed command!");
                    return spline_lerp;
                }

                std::string_view speed_val = command.substr(7, command.size() - 7);
                int speed                  = StringToTypedIntegral<int>(speed_val).value_or(0);
                return spline_lerp;  // TODO: At some point we will emulate playing the message
            }

            if (command.starts_with("{option:")) {
                if (command.size() <= 10) {
                    TOOLBOX_WARN("[BMG_PREVIEW] Received incomplete option command!");
                    return spline_lerp;
                }

                size_t command_split = command.rfind(':');
                std::string_view message =
                    command.substr(command_split + 1, command.size() - (command_split + 1) - 1);

                u8 option =
                    StringToTypedIntegral<u8>(command.substr(9, command_split - 9)).value_or(0);
                if (option > 1) {
                    return spline_lerp;
                }

                m_talk_options[option] = std::string(message);
                return spline_lerp;
            }

            if (command.starts_with("{ctx:")) {
                std::string_view ctx_val  = command.substr(5, command.size() - 5 - 1);
                size_t ctx_idx            = StringToTypedIntegral<size_t>(ctx_val).value_or(0);
                std::string_view fruit_id = BMGEditorWindow::GetFruitIDFromIndex(ctx_idx);
                return ImGui::TextSplineUnformattedEx(spline_lerp, m_talk_spline.get(),
                                                      fruit_id.data(),
                                                      fruit_id.data() + fruit_id.size());
            }

            if (command.starts_with("{color:")) {
                constexpr std::array<std::string_view, 6> ArrayColorNameToIdx = {
                    "white", "white_alt", "red", "yellow", "blue", "green"};
                std::string_view color_val = command.substr(7, command.size() - 7 - 1);

                size_t color_idx = 0;
                for (size_t i = 0; i < ArrayColorNameToIdx.size(); ++i) {
                    if (color_val == ArrayColorNameToIdx[i]) {
                        color_idx = i;
                        break;
                    }
                }

                ImGui::GetStyle().Colors[ImGuiCol_Text] =
                    BMGEditorWindow::GetTextColorFromIndex(color_idx);
                return spline_lerp;
            }

            if (command.starts_with("{record:")) {
                std::string_view record_val = command.substr(8, command.size() - 8 - 1);
                size_t record_idx           = StringToTypedIntegral<size_t>(record_val).value_or(0);
                std::string_view record     = BMGEditorWindow::GetSampleRecordFromIndex(record_idx);
                return ImGui::TextSplineUnformattedEx(spline_lerp, m_talk_spline.get(),
                                                      record.data(), record.data() + record.size());
            }
        }

    private:
        ScopePtr<CatmullSpline2D> m_talk_spline;
        std::array<std::string, 2> m_talk_options;

        RefPtr<const ImageHandle> m_talk_line_image;
        RefPtr<const ImageHandle> m_talk_button_image;
        RefPtr<const ImageHandle> m_talk_arrow_image;
        RefPtr<const ImageHandle> m_talk_exit_image;
        RefPtr<const ImageHandle> m_talk_option_image;
    };

    class BMGEditorViewBillboard final : public IBMGEditorView {
    public:
        ~BMGEditorViewBillboard() override = default;

        size_t getPageCount(const BMG::MessageData::Entry &message) const override { return 0; }

        bool render(const ImRect &render_rect, const BMG::MessageData::Entry &message,
                    size_t current_page) override {
            return false;
        }
    };

    class BMGEditorViewDEBS final : public IBMGEditorView {
    public:
        ~BMGEditorViewDEBS() override = default;

        size_t getPageCount(const BMG::MessageData::Entry &message) const override { return 0; }

        bool render(const ImRect &render_rect, const BMG::MessageData::Entry &message,
                    size_t current_page) override {
            return false;
        }
    };

    class BMGEditorViewShineSelect final : public IBMGEditorView {
    public:
        ~BMGEditorViewShineSelect() override = default;

        size_t getPageCount(const BMG::MessageData::Entry &message) const override { return 0; }

        bool render(const ImRect &render_rect, const BMG::MessageData::Entry &message,
                    size_t current_page) override {
            return false;
        }
    };

    ScopePtr<IBMGEditorView> BMGEditorViewFactory::create(BMG::MessageFlagSize flag_size,
                                                          EditorViewStyle style) {
        switch (flag_size) {
        case BMG::MessageFlagSize::FLAG_SIZE_SYSTEM:
            return make_scoped<BMGEditorViewShineSelect>();
        case BMG::MessageFlagSize::FLAG_SIZE_UNK8:
        case BMG::MessageFlagSize::FLAG_SIZE_NPC: {
            switch (style) {
            case EditorViewStyle::STYLE_NPC:
                return make_scoped<BMGEditorViewNPC>(false);
            case EditorViewStyle::STYLE_BOARD:
                return make_scoped<BMGEditorViewBillboard>();
            case EditorViewStyle::STYLE_DEBS:
                return make_scoped<BMGEditorViewDEBS>();
            }
        }
        }
        return nullptr;
    }

    constexpr float PreviewFontSize = 15.0f;

    static const std::unordered_map<uint8_t, int> s_preview_character_padding_map = {
        {' ', 4},
        {'c', 2},
        {'l', 2},
        {'u', 1},
        {'y', 1},
        {',', 6},
    };

    BMGEditorWindow::BMGEditorWindow(const std::string &name)
        : ImWindow(name), m_rename_buffer(), m_search_buf(), m_textbox_buf() {
        m_bmg_model = make_referable<BMGModel>();

        m_selection_mgr   = ModelSelectionManager(m_bmg_model);
        m_history_handler = make_scoped<ModelHistoryHandler>(m_bmg_model);

        ResourceManager &resource_manager = MainApplication::instance().getResourceManager();
        m_backdrop_path_uuid = resource_manager.getResourcePathUUID("Images/BMGEditor");

        m_selected_background = 0;
        loadCurrentBackdropSelection();
        onViewChanged(ModelIndex());
    }

    int BMGEditorWindow::GetExTextPaddingFromChar(char character) { return 0; }

    int BMGEditorWindow::GetExTextPaddingFromChar(uint8_t character) { return 0; }

    constexpr float ColorNrm(int color) { return static_cast<float>(color) / 255.0f; }

    ImVec4 BMGEditorWindow::GetTextColorFromIndex(size_t index) {
        constexpr std::array<ImVec4, 6> TextColorArray = {
            ImVec4(ColorNrm(0xFF), ColorNrm(0xFF), ColorNrm(0xFF), ColorNrm(0xFF)),
            ImVec4(ColorNrm(0xFF), ColorNrm(0xFF), ColorNrm(0xFF), ColorNrm(0xFF)),
            ImVec4(ColorNrm(0xFF), ColorNrm(0xB4), ColorNrm(0x8C), ColorNrm(0xFF)),
            ImVec4(ColorNrm(0x6E), ColorNrm(0xE6), ColorNrm(0xFF), ColorNrm(0xFF)),
            ImVec4(ColorNrm(0xFF), ColorNrm(0xFF), ColorNrm(0x00), ColorNrm(0xFF)),
            ImVec4(ColorNrm(0xAA), ColorNrm(0xFF), ColorNrm(0x50), ColorNrm(0xFF))};

        return index < TextColorArray.size() ? TextColorArray[index] : TextColorArray[0];
    }

    std::string_view BMGEditorWindow::GetFruitIDFromIndex(size_t index) {
        constexpr std::array<std::string_view, 4> FruitIDArray = {"3", "3", "3", "3"};
        return index < FruitIDArray.size() ? FruitIDArray[index] : FruitIDArray[0];
    }

    std::string_view BMGEditorWindow::GetSampleRecordFromIndex(size_t index) {
        constexpr std::array<std::string_view, 7> RecordArray = {
            "00:30:00", "00:35:00", "30", "1", "", "", "00:40:00"};
        return index < RecordArray.size() ? RecordArray[index] : RecordArray[0];
    }

    void BMGEditorWindow::onRenderMenuBar() {
        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem(ICON_FA_FLOPPY_DISK " Save")) {
                    m_is_save_default_ready = true;
                }
                if (ImGui::MenuItem(ICON_FA_FLOPPY_DISK " Save As...")) {
                    m_is_save_as_dialog_open = true;
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Region")) {
                const bool is_ntscu = m_bmg_model->isNTSC();
                if (ImGui::MenuItem("NTSC-U", nullptr, is_ntscu)) {
                    m_bmg_model->setNTSC(true);
                }

                const bool is_pal = !m_bmg_model->isNTSC();
                if (ImGui::MenuItem("PAL", nullptr, is_pal)) {
                    m_bmg_model->setNTSC(false);
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Flag Size")) {
                const bool is_system =
                    m_bmg_model->getFlagSize() == BMG::MessageFlagSize::FLAG_SIZE_SYSTEM;
                if (ImGui::MenuItem("4 (System)", nullptr, is_system)) {
                    m_bmg_model->setFlagSize(BMG::MessageFlagSize::FLAG_SIZE_SYSTEM);
                }

                const bool is_unk8 =
                    m_bmg_model->getFlagSize() == BMG::MessageFlagSize::FLAG_SIZE_UNK8;
                if (ImGui::MenuItem("8 (Unknown)", nullptr, is_unk8)) {
                    m_bmg_model->setFlagSize(BMG::MessageFlagSize::FLAG_SIZE_UNK8);
                }

                const bool is_npc =
                    m_bmg_model->getFlagSize() == BMG::MessageFlagSize::FLAG_SIZE_NPC;
                if (ImGui::MenuItem("12 (NPC)", nullptr, is_npc)) {
                    m_bmg_model->setFlagSize(BMG::MessageFlagSize::FLAG_SIZE_NPC);
                }
                ImGui::EndMenu();
            }

            ImGui::EndMenuBar();
        }

        if (m_is_save_default_ready) {
            m_is_save_default_ready = false;
            if (!m_io_context_path.empty()) {
                if (onSaveData(m_io_context_path)) {
                    MainApplication::instance().showSuccessModal(this, name(),
                                                                 "BMG saved successfully!");
                } else {
                    MainApplication::instance().showErrorModal(this, name(), "BMG failed to save!");
                }
            } else {
                m_is_save_as_dialog_open = true;
            }
        }

        if (m_is_save_as_dialog_open) {
            if (!FileDialog::instance()->isAlreadyOpen()) {
                FileDialog::instance()->saveDialog(*this, m_io_context_path, "message.bmg", false);
            }
            m_is_save_as_dialog_open = false;
        }

        if (FileDialog::instance()->isDone(*this)) {
            FileDialog::instance()->close();
            if (FileDialog::instance()->isOk()) {
                switch (FileDialog::instance()->getFilenameMode()) {
                case FileDialog::FileNameMode::MODE_SAVE: {
                    fs_path selected_path = FileDialog::instance()->getFilenameResult();

                    if (selected_path.empty()) {
                        selected_path = m_io_context_path;
                    }

                    auto dir_result =
                        Toolbox::Filesystem::is_directory(selected_path).value_or(false);
                    if (!dir_result) {
                        return;
                    }

                    m_io_context_path = selected_path;
                    if (onSaveData(m_io_context_path)) {
                        MainApplication::instance().showSuccessModal(this, name(),
                                                                     "Scene saved successfully!");
                    } else {
                        MainApplication::instance().showErrorModal(this, name(),
                                                                   "Scene failed to save!");
                    }
                    break;
                }
                default:
                    break;
                }
            }
        }
    }

    void BMGEditorWindow::onRenderBody(TimeStep delta_time) {
        ImWindow::onRenderBody(delta_time);

        const ImGuiStyle &style = ImGui::GetStyle();
        {
            const ImVec2 avail_region    = ImGui::GetContentRegionAvail();
            const float list_min_width   = getMessageListMinWidth();
            const float editor_min_width = getEditorGroupMinWidth();

            if (m_list_width <= 0.0f || m_editor_width <= 0.0f) {
                m_list_width = avail_region.x * 0.3f;
            }

            m_editor_width = avail_region.x - m_list_width - SplitterWidth;

            if (m_editor_width < editor_min_width) {
                m_editor_width = editor_min_width;
                m_list_width   = avail_region.x - m_editor_width - SplitterWidth;
            }
            if (m_list_width < list_min_width) {
                m_list_width   = list_min_width;
                m_editor_width = avail_region.x - m_list_width - SplitterWidth;
            }

            const ImGuiID splitter_id = ImGui::GetID("##ViewSplitter");
            if (ImGui::SplitterBehavior(splitter_id, ImGuiAxis_X, SplitterWidth, &m_list_width,
                                        &m_editor_width, list_min_width, editor_min_width)) {
                // On Splitter
            }

            ImGui::SetCursorPosX(ImGui::GetCursorPosX() - ImGui::GetStyle().WindowPadding.x);
            renderMessagesList();

            ImGui::SameLine(0.0f, 0.0f);
            ImGui::Dummy(ImVec2(SplitterWidth, avail_region.y));
            ImGui::SameLine(0.0f, 0.0f);

            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetStyle().WindowPadding.x);
            renderMessageEditorGroup();
        }
    }

    void BMGEditorWindow::renderMessagesList() {
        ImGuiStyle &style = ImGui::GetStyle();

        const ImVec2 orig_window_padding = style.WindowPadding;
        const ImVec4 orig_border_color   = style.Colors[ImGuiCol_Border];
        const ImVec4 orig_border_shadow  = style.Colors[ImGuiCol_BorderShadow];

        const ImVec2 this_window_padding = {style.WindowPadding.x, 1.0f};
        const ImVec4 this_border_color   = {0.0f, 0.0f, 0.0f, 0.0f};
        const ImVec4 this_border_shadow  = {0.0f, 0.0f, 0.0f, 0.0f};

        const int64_t row_count = static_cast<int64_t>(m_bmg_model->getRowCount(ModelIndex()));

        style.WindowPadding                 = this_window_padding;
        style.Colors[ImGuiCol_Border]       = this_border_color;
        style.Colors[ImGuiCol_BorderShadow] = this_border_shadow;

        if (ImGui::BeginChild("##MessageListPane",
                              ImVec2(m_list_width + style.WindowPadding.x, 0.0f),
                              ImGuiChildFlags_Borders)) {
            style.WindowPadding                 = orig_window_padding;
            style.Colors[ImGuiCol_Border]       = orig_border_color;
            style.Colors[ImGuiCol_BorderShadow] = orig_border_shadow;

            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            if (ImGui::InputTextWithHint("##MessageSearchBox", "Search for text here...",
                                         m_search_buf.data(), m_search_buf.size(),
                                         ImGuiInputTextFlags_AlwaysOverwrite)) {
                // ...
            }

            // ImGui::TextUnformatted("Messages");
            if (ImGui::BeginChild("##MessagesListBox", ImVec2(-FLT_MIN, -FLT_MIN),
                                  ImGuiChildFlags_Borders)) {
                const bool window_hovered = ImGui::IsWindowHovered();

                for (int64_t i = 0; i < row_count; ++i) {
                    const ModelIndex index = m_bmg_model->getIndex(i, 0);

                    const std::string_view message_name = m_bmg_model->getMessageName(index);

                    bool selected = m_selection_mgr.getState().isSelected(index);
                    bool is_cut   = false;

                    if (ImGui::Selectable(message_name.data(), &selected)) {
                    }

                    const ImRect node_rect =
                        ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());

                    if (ImGui::IsItemHovered() /* || window_hovered*/) {
                        m_selection_mgr.handleActionsByMouseInput(index, true);
                    }

                    const ImGuiStyle &style = ImGui::GetStyle();

                    const float render_alpha = is_cut ? 0.5f : 1.0f;
                    ImVec4 col               = ImGui::GetStyleColorVec4(ImGuiCol_Text);
                    col.w *= render_alpha;

                    if (index == m_view_index) {
                        ImVec4 col = ImGui::GetStyleColorVec4(ImGuiCol_Text);
                        col.w *= render_alpha;
                        ImGui::GetWindowDrawList()->AddRect(
                            node_rect.Min - (style.FramePadding / 2.0f),
                            node_rect.Max + (style.FramePadding / 2.0f),
                            ImGui::ColorConvertFloat4ToU32(col), 0.0f, ImDrawFlags_RoundCornersAll,
                            2.0f);
                    }
                }
            }
            ImGui::EndChild();
        }

        style.WindowPadding                 = this_window_padding;
        style.Colors[ImGuiCol_Border]       = this_border_color;
        style.Colors[ImGuiCol_BorderShadow] = this_border_shadow;

        ImGui::EndChild();

        style.WindowPadding                 = orig_window_padding;
        style.Colors[ImGuiCol_Border]       = orig_border_color;
        style.Colors[ImGuiCol_BorderShadow] = orig_border_shadow;
    }

    void BMGEditorWindow::renderMessageEditorGroup() {
        const ImGuiStyle &style = ImGui::GetStyle();

        if (ImGui::BeginChild("##EditorGroupPane",
                              ImVec2(m_editor_width - style.WindowPadding.x, 0.0f))) {
            renderMessageEditorMetadata();

            // ImGui::Separator();

            const std::string text_editor_name = ImWindowComponentTitle(*this, "Text Editor");
            if (ImGui::BeginChild(text_editor_name.c_str(), {0.0f, 0.0f})) {
                const ImVec2 avail_region = ImGui::GetContentRegionAvail();
                const ImVec2 min_size     = ImVec2(500.0f, 300.0f);

                const float text_min_width    = getEditorTextMinWidth();
                const float preview_min_width = getEditorPreviewMinWidth();

                // 2. Initialize inner widths on the first frame
                if (m_text_width <= 0.0f || m_preview_width <= 0.0f) {
                    m_text_width = avail_region.x * 0.6f;
                }

                m_preview_width = avail_region.x - m_text_width - SplitterWidth;

                if (m_preview_width < preview_min_width) {
                    m_preview_width = preview_min_width;
                    m_text_width    = avail_region.x - m_preview_width - SplitterWidth;
                }
                if (m_text_width < text_min_width) {
                    m_text_width    = text_min_width;
                    m_preview_width = avail_region.x - m_text_width - SplitterWidth;
                }

                const ImGuiID splitter_id = ImGui::GetID("##EditorSplitter");
                if (ImGui::SplitterBehavior(splitter_id, ImGuiAxis_X, SplitterWidth, &m_text_width,
                                            &m_preview_width, text_min_width, preview_min_width)) {
                    // On Splitter
                }

                if (ImGui::BeginChild("##TextBoxPane", ImVec2(m_text_width, 0.0f),
                                      ImGuiChildFlags_Borders)) {
                    renderMessageEditorTextbox();
                }
                ImGui::EndChild();

                ImGuiWindow *window = ImGui::GetCurrentWindow();

                ImGui::SameLine(0.0f, 0.0f);
                ImGui::Dummy(ImVec2(SplitterWidth, avail_region.y));
                ImGui::SameLine(0.0f, 0.0f);

                ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
                if (ImGui::BeginChild("##PreviewPane", ImVec2(m_preview_width, 0.0f),
                                      ImGuiChildFlags_Borders)) {
                    renderMessageEditorPreview();
                }
                ImGui::EndChild();
                ImGui::PopStyleColor();
            }
            ImGui::EndChild();
        }
        ImGui::EndChild();
    }

    static BMG::MessageSound GetMessageSoundForUIIndex(u8 index) {
        static std::array<BMG::MessageSound, 135> s_ordered_sounds = {
            BMG::MessageSound::NOTHING,
            BMG::MessageSound::PEACH_NORMAL,
            BMG::MessageSound::PEACH_SURPRISE,
            BMG::MessageSound::PEACH_WORRY,
            BMG::MessageSound::PEACH_ANGER_L,
            BMG::MessageSound::PEACH_APPEAL,
            BMG::MessageSound::PEACH_DOUBT,
            BMG::MessageSound::TOADSWORTH_NORMAL,
            BMG::MessageSound::TOADSWORTH_EXCITED,
            BMG::MessageSound::TOADSWORTH_ADVICE,
            BMG::MessageSound::TOADSWORTH_CONFUSED,
            BMG::MessageSound::TOADSWORTH_ASK,
            BMG::MessageSound::TOAD_NORMAL,
            BMG::MessageSound::TOAD_CRY_S,
            BMG::MessageSound::TOAD_CRY_L,
            BMG::MessageSound::TOAD_PEACE,
            BMG::MessageSound::TOAD_SAD_S,
            BMG::MessageSound::TOAD_ASK,
            BMG::MessageSound::TOAD_CONFUSED,
            BMG::MessageSound::TOAD_RECOVER,
            BMG::MessageSound::TOAD_SAD_L,
            BMG::MessageSound::MALE_PIANTA_NORMAL,
            BMG::MessageSound::MALE_PIANTA_LAUGH_D,
            BMG::MessageSound::MALE_PIANTA_DISGUSTED,
            BMG::MessageSound::MALE_PIANTA_ANGER_S,
            BMG::MessageSound::MALE_PIANTA_SURPRISE,
            BMG::MessageSound::MALE_PIANTA_DOUBT,
            BMG::MessageSound::MALE_PIANTA_DISPLEASED,
            BMG::MessageSound::MALE_PIANTA_CONFUSED,
            BMG::MessageSound::MALE_PIANTA_REGRET,
            BMG::MessageSound::MALE_PIANTA_PROUD,
            BMG::MessageSound::MALE_PIANTA_RECOVER,
            BMG::MessageSound::MALE_PIANTA_INVITING,
            BMG::MessageSound::MALE_PIANTA_QUESTION,
            BMG::MessageSound::MALE_PIANTA_LAUGH,
            BMG::MessageSound::MALE_PIANTA_THANKS,
            BMG::MessageSound::FEMALE_PIANTA_NORMAL,
            BMG::MessageSound::FEMALE_PIANTA_REGRET,
            BMG::MessageSound::FEMALE_PIANTA_INVITING,
            BMG::MessageSound::FEMALE_PIANTA_LAUGH,
            BMG::MessageSound::FEMALE_PIANTA_LAUGH_D,
            BMG::MessageSound::FEMALE_PIANTA_DISGUSTED,
            BMG::MessageSound::FEMALE_PIANTA_ANGER_S,
            BMG::MessageSound::FEMALE_PIANTA_SURPRISE,
            BMG::MessageSound::FEMALE_PIANTA_DOUBT,
            BMG::MessageSound::FEMALE_PIANTA_DISPLEASED,
            BMG::MessageSound::FEMALE_PIANTA_CONFUSED,
            BMG::MessageSound::FEMALE_PIANTA_PROUD,
            BMG::MessageSound::FEMALE_PIANTA_RECOVER,
            BMG::MessageSound::FEMALE_PIANTA_QUESTION,
            BMG::MessageSound::FEMALE_PIANTA_THANKS,
            BMG::MessageSound::CHILD_MALE_PIANTA_NORMAL,
            BMG::MessageSound::CHILD_MALE_PIANTA_LAUGH_D,
            BMG::MessageSound::CHILD_MALE_PIANTA_DISGUSTED,
            BMG::MessageSound::CHILD_MALE_PIANTA_ANGER_S,
            BMG::MessageSound::CHILD_MALE_PIANTA_SURPRISE,
            BMG::MessageSound::CHILD_MALE_PIANTA_DOUBT,
            BMG::MessageSound::CHILD_MALE_PIANTA_DISPLEASED,
            BMG::MessageSound::CHILD_MALE_PIANTA_CONFUSED,
            BMG::MessageSound::CHILD_MALE_PIANTA_REGRET,
            BMG::MessageSound::CHILD_MALE_PIANTA_PROUD,
            BMG::MessageSound::CHILD_MALE_PIANTA_RECOVER,
            BMG::MessageSound::CHILD_MALE_PIANTA_INVITING,
            BMG::MessageSound::CHILD_MALE_PIANTA_QUESTION,
            BMG::MessageSound::CHILD_MALE_PIANTA_LAUGH,
            BMG::MessageSound::CHILD_MALE_PIANTA_THANKS,
            BMG::MessageSound::CHILD_FEMALE_PIANTA_NORMAL,
            BMG::MessageSound::CHILD_FEMALE_PIANTA_LAUGH_D,
            BMG::MessageSound::CHILD_FEMALE_PIANTA_DISGUSTED,
            BMG::MessageSound::CHILD_FEMALE_PIANTA_ANGER_S,
            BMG::MessageSound::CHILD_FEMALE_PIANTA_SURPRISE,
            BMG::MessageSound::CHILD_FEMALE_PIANTA_DOUBT,
            BMG::MessageSound::CHILD_FEMALE_PIANTA_DISPLEASED,
            BMG::MessageSound::CHILD_FEMALE_PIANTA_CONFUSED,
            BMG::MessageSound::CHILD_FEMALE_PIANTA_REGRET,
            BMG::MessageSound::CHILD_FEMALE_PIANTA_PROUD,
            BMG::MessageSound::CHILD_FEMALE_PIANTA_RECOVER,
            BMG::MessageSound::CHILD_FEMALE_PIANTA_INVITING,
            BMG::MessageSound::CHILD_FEMALE_PIANTA_QUESTION,
            BMG::MessageSound::CHILD_FEMALE_PIANTA_LAUGH,
            BMG::MessageSound::CHILD_FEMALE_PIANTA_THANKS,
            BMG::MessageSound::MALE_NOKI_NORMAL,
            BMG::MessageSound::MALE_NOKI_REGRET,
            BMG::MessageSound::MALE_NOKI_LAUGH,
            BMG::MessageSound::MALE_NOKI_APPEAL,
            BMG::MessageSound::MALE_NOKI_SUPRISE,
            BMG::MessageSound::MALE_NOKI_SAD,
            BMG::MessageSound::MALE_NOKI_ASK,
            BMG::MessageSound::MALE_NOKI_PROMPT,
            BMG::MessageSound::MALE_NOKI_THANKS,
            BMG::MessageSound::FEMALE_NOKI_NORMAL,
            BMG::MessageSound::FEMALE_NOKI_REGRET,
            BMG::MessageSound::FEMALE_NOKI_LAUGH,
            BMG::MessageSound::FEMALE_NOKI_APPEAL,
            BMG::MessageSound::FEMALE_NOKI_SURPRISE,
            BMG::MessageSound::FEMALE_NOKI_SAD,
            BMG::MessageSound::FEMALE_NOKI_ASK,
            BMG::MessageSound::FEMALE_NOKI_PROMPT,
            BMG::MessageSound::FEMALE_NOKI_THANKS,
            BMG::MessageSound::ELDER_NOKI_NORMAL,
            BMG::MessageSound::ELDER_NOKI_REGRET,
            BMG::MessageSound::ELDER_NOKI_LAUGH,
            BMG::MessageSound::ELDER_NOKI_APPEAL,
            BMG::MessageSound::ELDER_NOKI_SURPRISE,
            BMG::MessageSound::ELDER_NOKI_SAD,
            BMG::MessageSound::ELDER_NOKI_ASK,
            BMG::MessageSound::ELDER_NOKI_PROMPT,
            BMG::MessageSound::ELDER_NOKI_THANKS,
            BMG::MessageSound::CHILD_MALE_NOKI_NORMAL,
            BMG::MessageSound::CHILD_MALE_NOKI_REGRET,
            BMG::MessageSound::CHILD_MALE_NOKI_LAUGH,
            BMG::MessageSound::CHILD_MALE_NOKI_APPEAL,
            BMG::MessageSound::CHILD_MALE_NOKI_SURPRISE,
            BMG::MessageSound::CHILD_MALE_NOKI_SAD,
            BMG::MessageSound::CHILD_MALE_NOKI_ASK,
            BMG::MessageSound::CHILD_MALE_NOKI_PROMPT,
            BMG::MessageSound::CHILD_MALE_NOKI_THANKS,
            BMG::MessageSound::CHILD_FEMALE_NOKI_NORMAL,
            BMG::MessageSound::CHILD_FEMALE_NOKI_REGRET,
            BMG::MessageSound::CHILD_FEMALE_NOKI_LAUGH,
            BMG::MessageSound::CHILD_FEMALE_NOKI_APPEAL,
            BMG::MessageSound::CHILD_FEMALE_NOKI_SURPRISE,
            BMG::MessageSound::CHILD_FEMALE_NOKI_SAD,
            BMG::MessageSound::CHILD_FEMALE_NOKI_ASK,
            BMG::MessageSound::CHILD_FEMALE_NOKI_PROMPT,
            BMG::MessageSound::CHILD_FEMALE_NOKI_THANKS,
            BMG::MessageSound::SUNFLOWER_PARENT_JOY,
            BMG::MessageSound::SUNFLOWER_PARENT_SAD,
            BMG::MessageSound::TANUKI_NORMAL,
            BMG::MessageSound::SHADOW_MARIO_PSHOT,
            BMG::MessageSound::IL_PIANTISSIMO_NORMAL,
            BMG::MessageSound::IL_PIANTISSIMO_LOST,
            BMG::MessageSound::ITEM_COLLECT_DELIGHT,
            BMG::MessageSound::ITEM_NOT_COLLECT,
            BMG::MessageSound::BGM_FANFARE,
        };

        return s_ordered_sounds[index];
    }

    static u8 GetMessageSoundCount() { return 135; }

    void BMGEditorWindow::renderMessageEditorMetadata() {
        const bool is_entry_selected = m_bmg_model->validateIndex(m_view_index);

        BMG::MessageSound sound = BMG::MessageSound::NOTHING;
        u16 start_frame = 0, end_frame = 0;

        if (is_entry_selected) {
            sound       = m_bmg_model->getMessageSoundID(m_view_index);
            start_frame = m_bmg_model->getMessageStartFrame(m_view_index);
            end_frame   = m_bmg_model->getMessageEndFrame(m_view_index);
        }

        const float panel_height =
            ImGui::GetTextLineHeightWithSpacing() * 3.0f + ImGui::GetStyle().WindowPadding.y * 2.0f;

        if (ImGui::BeginChild("##SoundFramePanel", ImVec2(m_editor_width * 0.7f, panel_height))) {
            if (!is_entry_selected) {
                ImGui::BeginDisabled();
            }

            std::string_view preview_label = magic_enum::enum_name(sound);
            if (ImGui::BeginCombo("Sound ID", preview_label.data())) {
                for (u8 i = 0; i < GetMessageSoundCount(); ++i) {
                    const BMG::MessageSound isound = GetMessageSoundForUIIndex(i);

                    bool selected = isound == sound;

                    std::string_view enum_name = magic_enum::enum_name(isound);
                    if (ImGui::Selectable(enum_name.data(), &selected)) {
                        m_bmg_model->setMessageSoundID(m_view_index, isound);
                    }
                }
                ImGui::EndCombo();
            }

            {
                if (ImGui::InputScalarCompact(
                        "Start Frame", ImGuiDataType_S16, &start_frame, nullptr, nullptr, nullptr,
                        ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_CharsNoBlank)) {
                    start_frame = std::clamp<u16>(start_frame, std::numeric_limits<u16>::min(),
                                                  std::numeric_limits<u16>::max());
                    m_bmg_model->setMessageStartFrame(m_view_index, static_cast<u16>(start_frame));
                }
            }

            {
                if (ImGui::InputScalarCompact(
                        "End Frame", ImGuiDataType_S16, &end_frame, nullptr, nullptr, nullptr,
                        ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_CharsNoBlank)) {
                    end_frame = std::clamp<u16>(end_frame, std::numeric_limits<u16>::min(),
                                                std::numeric_limits<u16>::max());
                    m_bmg_model->setMessageEndFrame(m_view_index, static_cast<u16>(end_frame));
                }
            }

            if (!is_entry_selected) {
                ImGui::EndDisabled();
            }
        }
        ImGui::EndChild();

        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();

        if (ImGui::BeginChild("##BackgroundControl", ImVec2(0.0f, panel_height))) {
            if (m_bmg_model->getFlagSize() != BMG::MessageFlagSize::FLAG_SIZE_NPC) {
                ImGui::BeginDisabled();
            }

            static std::array<std::string, 3> s_background_items = {"Pianta", "Tanooki", "Noki"};
            std::string_view preview_backdrop_label = s_background_items[m_selected_background];

            ImGui::Text("Preview Settings");
            if (ImGui::BeginCombo("##BackgroundComboBox", preview_backdrop_label.data())) {

                for (size_t i = 0; i < s_background_items.size(); ++i) {
                    bool selected = i == m_selected_background;

                    if (ImGui::Selectable(s_background_items[i].c_str(), &selected)) {
                        m_selected_background = i;
                        loadCurrentBackdropSelection();
                    }
                }
                ImGui::EndCombo();
            }

            static std::array<std::string, 3> s_style_items = {"NPC", "Board", "DEBS"};
            std::string_view preview_style_label =
                s_style_items[static_cast<size_t>(m_selected_style)];

            if (ImGui::BeginCombo("##StyleComboBox", preview_style_label.data())) {

                for (size_t i = 0; i < s_style_items.size(); ++i) {
                    bool selected = i == static_cast<size_t>(m_selected_style);

                    if (ImGui::Selectable(s_style_items[i].c_str(), &selected)) {
                        m_selected_style = static_cast<EditorViewStyle>(i);
                    }
                }
                ImGui::EndCombo();
            }

            if (m_bmg_model->getFlagSize() != BMG::MessageFlagSize::FLAG_SIZE_NPC) {
                ImGui::EndDisabled();
            }
        }
        ImGui::EndChild();
    }

    void BMGEditorWindow::renderMessageEditorTextbox() {
        ImGui::InputTextMultiline("##MessageData", m_textbox_buf.data(), m_textbox_buf.size(),
                                  ImVec2(-FLT_MIN, -FLT_MIN));

        if (ImGui::IsItemDeactivatedAfterEdit()) {
            size_t strlen = strnlen(m_textbox_buf.data(), m_textbox_buf.size());
            m_bmg_model->setMessageText(m_view_index, std::string(m_textbox_buf.data(), strlen));
        }
    }

    void BMGEditorWindow::renderMessageEditorPreview() {
        const ImVec2 avail_size = ImGui::GetContentRegionAvail();
        const ImGuiStyle &style = ImGui::GetStyle();

        ImRect render_rect = {ImGui::GetCursorPos(), ImGui::GetCursorPos() + avail_size};

        if (m_current_backdrop) {
            const auto [image_width, image_height] = m_current_backdrop->size();

            const float avail_ratio = avail_size.x / avail_size.y;
            const float image_ratio =
                static_cast<float>(image_width) / static_cast<float>(image_height);

            const float render_factor = image_ratio > avail_ratio
                                            ? avail_size.x / static_cast<float>(image_width)
                                            : avail_size.y / static_cast<float>(image_height);

            const ImVec2 render_size =
                ImVec2(image_width * render_factor, image_height * render_factor);

            const ImVec2 render_pos = ImGui::GetWindowPos() + style.WindowPadding +
                                      ImVec2(avail_size.x / 2.0f - render_size.x / 2.0f,
                                             avail_size.y / 2.0f - render_size.y / 2.0f);

            render_rect = {render_pos, render_pos + render_size};

            m_backdrop_painter.render(*m_current_backdrop, render_pos, render_size);
        }

        if (!m_editor_view) {
            return;
        }

        if (!m_bmg_model->validateIndex(m_view_index)) {
            BMG::MessageData::Entry empty_entry;
            m_editor_view->render(render_rect, empty_entry, 0);
            return;
        }

        BMG::MessageData::Entry selected_entry = m_bmg_model->getMessageEntry(m_view_index);
        m_editor_view->render(render_rect, selected_entry, m_current_page);
    }

    float BMGEditorWindow::getMessageListMinWidth() const {
        const ImVec2 min_win_size = minSize().value_or(ImVec2(500.0f, 300.0f));
        return min_win_size.x * 0.3f;
    }

    float BMGEditorWindow::getEditorGroupMinWidth() const {
        const ImVec2 min_win_size = minSize().value_or(ImVec2(500.0f, 300.0f));
        return min_win_size.x * 0.7f;
    }

    float BMGEditorWindow::getEditorTextMinWidth() const {
        const float min_editor_width = getEditorGroupMinWidth();
        return min_editor_width * 0.3f;
    }

    float BMGEditorWindow::getEditorPreviewMinWidth() const {
        const float min_editor_width = getEditorGroupMinWidth();
        return min_editor_width * 0.7f;
    }

    float BMGEditorWindow::getMessageListMaxWidth() const { return 0.0f; }

    float BMGEditorWindow::getEditorGroupMaxWidth() const { return 0.0f; }

    float BMGEditorWindow::getEditorTextMaxWidth() const { return 0.0f; }

    float BMGEditorWindow::getEditorPreviewMaxWidth() const { return 0.0f; }

    void BMGEditorWindow::loadCurrentBackdropSelection() {
        ResourceManager &resource_manager = MainApplication::instance().getResourceManager();

        static const std::array<std::string, 3> s_backdrop_filenames = {
            "bmg_preview_shades_pianta.png",
            "bmg_preview_tanooki.png",
            "bmg_preview_old_noki.png",
        };

        auto result = resource_manager.getImageHandle(s_backdrop_filenames[m_selected_background],
                                                      m_backdrop_path_uuid);
        if (!result) {
            LogError(result.error());
            return;
        }

        m_current_backdrop = result.value();
    }

    void BMGEditorWindow::onViewChanged(const ModelIndex &index) {
        m_editor_view  = BMGEditorViewFactory::create(m_bmg_model->getFlagSize(), m_selected_style);
        m_view_index   = index;
        m_current_page = 0;

        if (m_bmg_model->validateIndex(index)) {
            std::string_view message_text = m_bmg_model->getMessageText(index);
            m_textbox_buf.fill('\0');
            strncpy(m_textbox_buf.data(), message_text.data(),
                    std::min(message_text.size(), m_textbox_buf.size()));
        }
    }

    bool BMGEditorWindow::onLoadData(const std::filesystem::path &path) {
        if (!Filesystem::is_regular_file(path).value_or(false)) {
            return false;
        }

        BMG::MessageData bmg_data;

        std::ifstream file(path, std::ios::in | std::ios::binary);
        if (file.is_open()) {  // scene.bmg is optional
            Deserializer in(file.rdbuf());

            Result<void, SerialError> result = bmg_data.deserialize(in);
            if (!result) {
                LogError(result.error());
                return false;
            }
        }

        m_bmg_model = make_referable<BMGModel>();
        m_bmg_model->initialize(bmg_data);

        m_selection_mgr   = ModelSelectionManager(m_bmg_model);
        m_history_handler = make_scoped<ModelHistoryHandler>(m_bmg_model);

        m_io_context_path = path;
        return true;
    }

    bool BMGEditorWindow::onSaveData(std::optional<std::filesystem::path> path) { return false; }

    void BMGEditorWindow::onAttach() { ImWindow::onAttach(); }

    void BMGEditorWindow::onDetach() { ImWindow::onDetach(); }

    void BMGEditorWindow::onImGuiUpdate(TimeStep delta_time) {
        ImWindow::onImGuiUpdate(delta_time);

        if (m_view_index != m_selection_mgr.getState().getLastSelected()) {
            onViewChanged(m_selection_mgr.getState().getLastSelected());
        }

        m_history_handler->handleInputs();
    }

    void BMGEditorWindow::onContextMenuEvent(RefPtr<ContextMenuEvent> ev) {
        ImWindow::onContextMenuEvent(ev);
    }

    void BMGEditorWindow::onDragEvent(RefPtr<DragEvent> ev) { ImWindow::onDragEvent(ev); }

    void BMGEditorWindow::onDropEvent(RefPtr<DropEvent> ev) { ImWindow::onDropEvent(ev); }

    void BMGEditorWindow::onEvent(RefPtr<BaseEvent> ev) { ImWindow::onEvent(ev); }

    void BMGEditorWindow::buildContextMenu() {}

}  // namespace Toolbox::UI