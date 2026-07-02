#include "bmg/bmg.hpp"

#include "core/mathutil.hpp"
#include "gui/appmain/application.hpp"
#include "gui/appmain/bmgeditor/window.hpp"
#include "gui/appmain/bmgeditor/p_renderers.hpp"
#include "gui/logging/errors.hpp"
#include "spline.hpp"

constexpr float SplitterWidth       = 6.0f;

namespace Toolbox::UI {

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

    BMGEditorWindow::BMGEditorWindow(const std::string &name)
        : ImWindow(name), m_rename_buffer(), m_search_buf(), m_textbox_buf() {
        m_bmg_model = make_referable<BMGModel>();

        m_selection_mgr   = ModelSelectionManager(m_bmg_model);
        m_history_handler = make_scoped<ModelHistoryHandler>(m_bmg_model);

        ResourceManager &resource_manager = MainApplication::instance().getResourceManager();
        m_backdrop_path_uuid = resource_manager.getResourcePathUUID("Images/BMGEditor");

        m_selected_background = 0;
        loadCurrentBackdropSelection();

        buildListContextMenu();
        buildTextContextMenu();

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
            renderMessageEditorGroup(delta_time);
        }

        m_list_context_menu.tryRender(m_selection_mgr.getState().getLastSelected());
    }

    void BMGEditorWindow::renderPlaybackButtons() {
        const AppSettings &settings =
            MainApplication::instance().getSettingsManager().getCurrentProfile();

        Game::TaskCommunicator &task_communicator =
            MainApplication::instance().getTaskCommunicator();

        ImGui::SetCursorPos({0, 10});

        const ImVec2 frame_padding  = ImGui::GetStyle().FramePadding;
        const ImVec2 window_padding = ImGui::GetStyle().WindowPadding;

        const ImVec2 window_size = ImGui::GetWindowSize();
        ImVec2 cmd_button_size   = ImGui::CalcTextSize(ICON_FA_BACKWARD) + frame_padding;
        cmd_button_size.x        = std::max(cmd_button_size.x, cmd_button_size.y) * 1.5f;
        cmd_button_size.y        = std::max(cmd_button_size.x, cmd_button_size.y) * 1.f;

        const bool context_controls_disabled = !m_editor_view || !m_editor_view->canPlayback();

        if (context_controls_disabled) {
            ImGui::BeginDisabled();
        }

        const bool is_play_disabled = context_controls_disabled || m_editor_view->isPlayback();

        if (is_play_disabled) {
            ImGui::BeginDisabled();
        }

        ImGui::PushStyleColor(ImGuiCol_Button, {0.1f, 0.35f, 0.1f, 0.9f});
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, {0.2f, 0.7f, 0.2f, 0.9f});
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.2f, 0.7f, 0.2f, 1.0f});

        ImGui::SetCursorPosX(window_size.x / 2 - cmd_button_size.x / 2);
        if (ImGui::AlignedButton(ICON_FA_PLAY, cmd_button_size)) {
            m_editor_view->startPlayback();
        }

        if (is_play_disabled) {
            ImGui::EndDisabled();
        }

        ImGui::PopStyleColor(3);

        ImGui::SameLine();

        const bool is_stop_disabled = context_controls_disabled || !m_editor_view->isPlayback();

        if (is_stop_disabled) {
            ImGui::BeginDisabled();
        }

        ImGui::PushStyleColor(ImGuiCol_Button, {0.35f, 0.1f, 0.1f, 0.9f});
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, {0.7f, 0.2f, 0.2f, 0.9f});
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.7f, 0.2f, 0.2f, 1.0f});

        ImGui::SetCursorPosX(window_size.x / 2 - cmd_button_size.x / 2 + cmd_button_size.x);
        if (ImGui::AlignedButton(ICON_FA_STOP, cmd_button_size, ImGuiButtonFlags_None, 5.0f,
                                 ImDrawFlags_RoundCornersBottomRight)) {
            m_editor_view->stopPlayback();
        }

        if (is_stop_disabled) {
            ImGui::EndDisabled();
        }

        ImGui::PopStyleColor(3);

        ImGui::SameLine();

        const bool is_reset_disabled = context_controls_disabled || !m_editor_view->isPlayback();

        if (is_reset_disabled) {
            ImGui::BeginDisabled();
        }

        ImGui::PushStyleColor(ImGuiCol_Button, {0.1f, 0.2f, 0.4f, 0.9f});
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, {0.2f, 0.4f, 0.8f, 0.9f});
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.2f, 0.4f, 0.8f, 1.0f});

        ImGui::SetCursorPosX(window_size.x / 2 - cmd_button_size.x / 2 - cmd_button_size.x);
        if (ImGui::AlignedButton(ICON_FA_BACKWARD, cmd_button_size, ImGuiButtonFlags_None, 5.0f,
                                 ImDrawFlags_RoundCornersBottomLeft)) {
            m_editor_view->stopPlayback();
            m_editor_view->startPlayback();
        }

        if (is_reset_disabled) {
            ImGui::EndDisabled();
        }

        ImGui::PopStyleColor(3);

        if (context_controls_disabled) {
            ImGui::EndDisabled();
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
                        m_list_context_menu.tryOpen(0);
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

    void BMGEditorWindow::renderMessageEditorGroup(TimeStep delta_time) {
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
                    renderMessageEditorPreview(delta_time);
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
        if (ImGui::InputTextMultiline("##MessageData", m_textbox_buf.data(), m_textbox_buf.size(),
                                      ImVec2(-FLT_MIN, -FLT_MIN))) {
            size_t strlen              = strnlen(m_textbox_buf.data(), m_textbox_buf.size());
            std::string_view text_data = std::string_view(m_textbox_buf.data(), strlen);

            Result<std::string, BaseError> bmg_msg =
                String::asEncoding(text_data, IMGUI_ENCODING, BMG::MessageEncoding);
            if (!bmg_msg) {
                LogError(bmg_msg.error());
            } else {
                m_bmg_model->setMessageText(m_view_index, bmg_msg.value());
            }
        }

        // if (ImGui::IsItemDeactivatedAfterEdit()) {
        //     size_t strlen = strnlen(m_textbox_buf.data(), m_textbox_buf.size());
        //     m_bmg_model->setMessageText(m_view_index, std::string(m_textbox_buf.data(), strlen));
        // }
    }

    void BMGEditorWindow::renderMessageEditorPreview(TimeStep delta_time) {
        const ImVec2 cursor_pos = ImGui::GetCursorPos();

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

        ImGui::SetCursorPos(cursor_pos);
        renderPlaybackButtons();

        if (!m_editor_view) {
            return;
        }

        if (!m_bmg_model->validateIndex(m_view_index)) {
            BMG::MessageData::Entry empty_entry;
            m_editor_view->render(delta_time, render_rect, empty_entry);
            return;
        }

        BMG::MessageData::Entry selected_entry = m_bmg_model->getMessageEntry(m_view_index);
        m_editor_view->render(delta_time, render_rect, selected_entry);
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

            Result<std::string, BaseError> textbox_msg =
                String::asEncoding(message_text, BMG::MessageEncoding, IMGUI_ENCODING);
            if (!textbox_msg) {
                LogError(textbox_msg.error());
            } else {
                strncpy(m_textbox_buf.data(), textbox_msg.value().data(),
                        std::min(textbox_msg.value().size(), m_textbox_buf.size()));
            }
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

    bool BMGEditorWindow::onSaveData(std::optional<std::filesystem::path> path) {
        const fs_path out_path = path ? path.value() : m_io_context_path;

        ScopePtr<BMG::MessageData> message_data = m_bmg_model->bakeToMessageData();
        if (!message_data) {
            return false;
        }

        (void)Filesystem::create_directories(out_path.parent_path());

        std::ofstream file(out_path, std::ios::out | std::ios::binary);
        if (file.is_open()) {
            Serializer out(file.rdbuf());

            Result<void, SerialError> result = message_data->serialize(out);
            if (!result) {
                LogError(result.error());
                return false;
            }
        }

        m_io_context_path = out_path;
        return true;
    }

    void BMGEditorWindow::onAttach() { ImWindow::onAttach(); }

    void BMGEditorWindow::onDetach() { ImWindow::onDetach(); }

    void BMGEditorWindow::onImGuiUpdate(TimeStep delta_time) {
        ImWindow::onImGuiUpdate(delta_time);

        if (m_view_index != m_selection_mgr.getState().getLastSelected()) {
            onViewChanged(m_selection_mgr.getState().getLastSelected());
        }
    }

    void BMGEditorWindow::onImGuiPostUpdate(TimeStep delta_time) {
        ImWindow::onImGuiPostUpdate(delta_time);

        m_history_handler->handleInputs();
        m_list_context_menu.applyDeferredCmds();
    }

    void BMGEditorWindow::onContextMenuEvent(RefPtr<ContextMenuEvent> ev) {
        ImWindow::onContextMenuEvent(ev);
    }

    void BMGEditorWindow::onDragEvent(RefPtr<DragEvent> ev) { ImWindow::onDragEvent(ev); }

    void BMGEditorWindow::onDropEvent(RefPtr<DropEvent> ev) { ImWindow::onDropEvent(ev); }

    void BMGEditorWindow::onEvent(RefPtr<BaseEvent> ev) { ImWindow::onEvent(ev); }

    void BMGEditorWindow::buildListContextMenu() {
        m_list_context_menu = ContextMenu<ModelIndex>();

        ContextMenuBuilder(&m_list_context_menu)
            .addOption("Insert Message Before", {KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_R},
                       [this](ModelIndex index) {
                           BMG::MessageData::Entry new_entry;
                           new_entry.setName("New Message");
                           new_entry.setMessage(BMG::CmdMessage(
                               "{color:green}There are{color:red} plenty\nof{color:yellow} cool "
                               "things\n{speed:50}{color:blue}BMGs can do!{color:white}\nIncluding "
                               "multiple\nparagraphs, colored\ntext, and more!"));

                           const int64_t index_row = m_bmg_model->getRow(index);
                           ModelIndex new_index =
                               m_bmg_model->insertMessageEntry(new_entry, index_row);

                           m_original_name = m_bmg_model->getMessageName(index);
                           m_rename_buffer.fill('\0');
                           strncpy(m_rename_buffer.data(), m_original_name.data(),
                                   std::min(m_rename_buffer.size(), m_original_name.size()));
                           m_renaming_index = index;
                           m_is_renaming    = true;
                       })
            .addOption("Insert Message After", {KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_R},
                       [this](ModelIndex index) {
                           BMG::MessageData::Entry new_entry;
                           new_entry.setName("New Message");
                           new_entry.setMessage(BMG::CmdMessage(
                               "{color:green}There are{color:red} plenty\nof{color:yellow} cool "
                               "things\n{speed:50}{color:blue}BMGs can do!{color:white}\nIncluding "
                               "multiple\nparagraphs, colored\ntext, and more!"));

                           const int64_t index_row = m_bmg_model->getRow(index);
                           ModelIndex new_index =
                               m_bmg_model->insertMessageEntry(new_entry, index_row + 1);

                           m_original_name = m_bmg_model->getMessageName(index);
                           m_rename_buffer.fill('\0');
                           strncpy(m_rename_buffer.data(), m_original_name.data(),
                                   std::min(m_rename_buffer.size(), m_original_name.size()));
                           m_renaming_index = index;
                           m_is_renaming    = true;
                       })
            .addDivider()
            .addOption("Rename...", {KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_R},
                       [this](ModelIndex index) {
                           m_original_name = m_bmg_model->getMessageName(index);
                           m_rename_buffer.fill('\0');
                           strncpy(m_rename_buffer.data(), m_original_name.data(),
                                   std::min(m_rename_buffer.size(), m_original_name.size()));
                           m_renaming_index = index;
                           m_is_renaming    = true;
                       })
            .addDivider()
            .addOption("Select All", {KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_A},
                       [this](ModelIndex index) { m_selection_mgr.actionSelectAll(); })
            .addDivider()
            .addOption("Copy", {KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_C},
                       [this](ModelIndex index) {
                           ScopePtr<MimeData> copy_data = m_selection_mgr.actionCopySelection();
                           if (!copy_data) {
                               return;
                           }
                           SystemClipboard::instance().setContent(*copy_data);
                       })
            .addOption("Paste", {KeyCode::KEY_LEFTCONTROL, KeyCode::KEY_V},
                       [this](ModelIndex index) {
                           auto result = SystemClipboard::instance().getContent();
                           if (!result) {
                               LogError(result.error());
                               return;
                           }
                           m_history_handler->startExplicitFrame();
                           m_selection_mgr.actionPasteIntoSelection(result.value());
                           m_history_handler->endExplicitFrame();
                       })
            .addDivider()
            .addOption("Delete", {KeyCode::KEY_DELETE}, [this](ModelIndex index) {
                m_history_handler->startExplicitFrame();
                m_selection_mgr.actionDeleteSelection();
                m_history_handler->endExplicitFrame();
            });
    }

    void BMGEditorWindow::buildTextContextMenu() {}

}  // namespace Toolbox::UI