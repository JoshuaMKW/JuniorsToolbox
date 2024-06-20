#pragma once

#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/clipboard.hpp"
#include "core/memory.hpp"
#include "fsystem.hpp"
#include "objlib/object.hpp"
#include "objlib/template.hpp"
#include "scene/scene.hpp"
#include "smart_resource.hpp"

#include "game/task.hpp"
#include "gui/application.hpp"
#include "gui/context_menu.hpp"
#include "gui/image/imagepainter.hpp"
#include "gui/pad/window.hpp"
#include "gui/property/property.hpp"
#include "gui/scene/billboard.hpp"
#include "gui/scene/camera.hpp"
#include "gui/scene/nodeinfo.hpp"
#include "gui/scene/objdialog.hpp"
#include "gui/scene/path.hpp"
#include "gui/scene/renderer.hpp"
#include "gui/window.hpp"

#include "gui/imgui_ext.hpp"
#include <ImGuiFileDialog.h>
#include <imgui.h>

static const std::vector<ImVec2> s_controller_base_points = {
    ImVec2(0.126939728447689f, -0.9979365868418832f),
    ImVec2(0.24654011951415217f, -0.9834500658396175f),
    ImVec2(0.3991203624960013f, -0.9642552499613684f),
    ImVec2(0.4731294497400282f, -0.9422992692885306f),
    ImVec2(0.555472391832531f, -0.9243304500576878f),
    ImVec2(0.6377442204293304f, -0.9022302426980523f),
    ImVec2(0.6993571959488083f, -0.8804906369602239f),
    ImVec2(0.7568398006395951f, -0.8588231271133879f),
    ImVec2(0.8143935188260857f, -0.8412870053953447f),
    ImVec2(0.8676716396493923f, -0.8155602556679278f),
    ImVec2(0.9209497430202924f, -0.7898325060928159f),
    ImVec2(0.9701686065106118f, -0.7683092403851841f),
    ImVec2(1.0435276389115409f, -0.7091689413818865f),
    ImVec2(1.1047800128092116f, -0.6667704825667368f),
    ImVec2(1.1615404493135018f, -0.603787283713055f),
    ImVec2(1.18959663126559f, -0.55370300774473f),
    ImVec2(1.2258434412270518f, -0.4993431344132213f),
    ImVec2(1.2410718137894867f, -0.42468599202148705f),
    ImVec2(1.2604315570283078f, -0.34995673628635393f),
    ImVec2(1.274722403769338f, -0.22158877555851345f),
    ImVec2(1.2917022670567455f, -0.010516288721074168f),
    ImVec2(1.308321546174752f, 0.2212140513459913f),
    ImVec2(1.2969784397531323f, 0.634302002138202f),
    ImVec2(1.2825650319419606f, 0.7497710399806854f),
    ImVec2(1.2605359555304354f, 0.8279104806009969f),
    ImVec2(1.230677835126632f, 0.8811164880809047f),
    ImVec2(1.1966152308553488f, 0.9383817354413934f),
    ImVec2(1.125368273348793f, 0.9949989276541746f),
    ImVec2(1.050783261752864f, 1.0060949296925281f),
    ImVec2(0.9809786583763175f, 0.9800797091391094f),
    ImVec2(0.928061139174818f, 0.9336941063343712f),
    ImVec2(0.9000049397703234f, 0.8836108302137414f),
    ImVec2(0.8762253724815205f, 0.8253349438407543f),
    ImVec2(0.8567213851034955f, 0.7588694293060888f),
    ImVec2(0.8412045591674652f, 0.7007377696198994f),
    ImVec2(0.8235035100888499f, 0.5309819890894076f),
    ImVec2(0.8094290208304226f, 0.3902189164847134f),
    ImVec2(0.7865859620629226f, 0.2782331941709088f),
    ImVec2(0.7707806653008905f, 0.2366280170379594f),
    ImVec2(0.750771867066667f, 0.1990830970856925f),
    ImVec2(0.7267759248428548f, 0.15320332243725432f),
    ImVec2(0.6208687556440506f, 0.06456448735185925f),
    ImVec2(0.5799857466975977f, 0.039054095107045404f),
    ImVec2(0.53497038467947f, 0.013470572218731113f),
    ImVec2(0.49408737573301714f, -0.012039820026082731f),
    ImVec2(0.4531312535954703f, -0.0334188590469168f),
    ImVec2(0.40818900476843656f, -0.06313373515924768f),
    ImVec2(0.3668049440877048f, -0.08364690579452823f),
    ImVec2(0.32546364595397516f, -0.10704697421285306f),
    ImVec2(0.22621878502070732f, -0.1226670720812432f),
    ImVec2(0.12282118431437794f, -0.14296770752534527f),
    ImVec2(-0.1341262527410403f, -0.1449181844869373f),
    ImVec2(-0.21694248525807607f, -0.1239674386736311f),
    ImVec2(-0.299596901514939f, -0.10743815317818765f),
    ImVec2(-0.3533061194768429f, -0.08677708787917242f),
    ImVec2(-0.3987603042385443f, -0.07024821100337546f),
    ImVec2(-0.44421509987946595f, -0.0495866418631448f),
    ImVec2(-0.4855370793342304f, -0.03305834364810655f),
    ImVec2(-0.5268596232513064f, -0.012396642684578782f),
    ImVec2(-0.5849825298519895f, 0.010453239209275725f),
    ImVec2(-0.6857991194839402f, 0.10374895650102961f),
    ImVec2(-0.7366045637910664f, 0.17312084491237023f),
    ImVec2(-0.7621149734882868f, 0.21400485370651823f),
    ImVec2(-0.7834949949044095f, 0.25495995854396347f),
    ImVec2(-0.8058836730900469f, 0.35375826969400276f),
    ImVec2(-0.8246457914554964f, 0.48154928876983943f),
    ImVec2(-0.84768350718413f, 0.6175309358550498f),
    ImVec2(-0.8655802305239805f, 0.6957435071188617f),
    ImVec2(-0.8875361937444118f, 0.7697515945151934f),
    ImVec2(-0.9094200436214442f, 0.8396283112351388f),
    ImVec2(-0.931015422672475f, 0.8929785454018443f),
    ImVec2(-0.998203105393637f, 0.9537982217867155f),
    ImVec2(-1.118524682251308f, 0.9806284070913974f),
    ImVec2(-1.1799213002881828f, 0.9464936894767152f),
    ImVec2(-1.2407419765207492f, 0.8793059893031466f),
    ImVec2(-1.2683644611123428f, 0.8044325068812159f),
    ImVec2(-1.292145045701247f, 0.7461576029035175f),
    ImVec2(-1.3067243632682795f, 0.634316124728917f),
    ImVec2(-1.3194276293448075f, 0.4150530273345106f),
    ImVec2(-1.2902599129267363f, -0.07211612404512581f),
    ImVec2(-1.2659240697548912f, -0.2824674425437636f),
    ImVec2(-1.2478831371806494f, -0.3689417553126528f),
    ImVec2(-1.2256386856818098f, -0.4594773251099196f),
    ImVec2(-1.1997677092675951f, -0.5210181872859987f),
    ImVec2(-1.165560860857108f, -0.5865471758469549f),
    ImVec2(-1.0728627858911317f, -0.688249861178279f),
    ImVec2(-0.9975565885039951f, -0.74066256952358f),
    ImVec2(-0.9474723125356702f, -0.7687187514756681f),
    ImVec2(-0.8931124392041613f, -0.8049655614371299f),
    ImVec2(-0.8430281632358365f, -0.8330217433892182f),
    ImVec2(-0.7848253900539435f, -0.8526699574540411f),
    ImVec2(-0.7348853582248227f, -0.8724623982056618f),
    ImVec2(-0.689148810415487f, -0.8881955816242949f),
    ImVec2(-0.6185519252044354f, -0.9076274556589214f),
    ImVec2(-0.5562888948418596f, -0.9230721682515528f),
    ImVec2(-0.4980861216599666f, -0.9427203823163757f),
    ImVec2(-0.4027701261909109f, -0.9575881532570034f),
    ImVec2(-0.30738301722615163f, -0.9765873123264237f),
    ImVec2(-0.21206802160479116f, -0.9914551007194579f),
    ImVec2(-0.005271132130535799f, -1.0002443359974917f)};

static const std::vector<ImVec2> s_controller_form_l_points = {
    ImVec2(0.208122f, 0.949239f),   ImVec2(0.28934f, 0.86802f),     ImVec2(0.451777f, 0.695431f),
    ImVec2(0.532995f, 0.502538f),   ImVec2(0.553299f, 0.269036f),   ImVec2(0.553299f, 0.045685f),
    ImVec2(0.695431f, -0.116751f),  ImVec2(0.786802f, -0.238579f),  ImVec2(0.847716f, -0.431472f),
    ImVec2(0.817259f, -0.664975f),  ImVec2(0.725888f, -0.847716f),  ImVec2(0.553299f, -0.969543f),
    ImVec2(0.329949f, -1.0f),       ImVec2(0.126904f, -0.959391f),  ImVec2(-0.035533f, -0.817259f),
    ImVec2(-0.137056f, -0.624365f), ImVec2(-0.248731f, -0.431472f), ImVec2(-0.340102f, -0.380711f),
    ImVec2(-0.522843f, -0.279188f), ImVec2(-0.695431f, -0.137056f), ImVec2(-0.817259f, 0.055838f),
    ImVec2(-0.847716f, 0.258883f),  ImVec2(-0.837563f, 0.482234f),  ImVec2(-0.746193f, 0.685279f),
    ImVec2(-0.604061f, 0.857868f),  ImVec2(-0.42132f, 0.969543f),   ImVec2(-0.19797f, 1.0f),
    ImVec2(0.015228f, 0.979695f)};

static const std::vector<ImVec2> s_controller_form_r_points = {
    ImVec2(-0.015228f, 0.979695f),  ImVec2(0.19797f, 1.0f),         ImVec2(0.42132f, 0.969543f),
    ImVec2(0.604061f, 0.857868f),   ImVec2(0.746193f, 0.685279f),   ImVec2(0.837563f, 0.482234f),
    ImVec2(0.847716f, 0.258883f),   ImVec2(0.817259f, 0.055838f),   ImVec2(0.695431f, -0.137056f),
    ImVec2(0.522843f, -0.279188f),  ImVec2(0.340102f, -0.380711f),  ImVec2(0.248731f, -0.431472f),
    ImVec2(0.137056f, -0.624365f),  ImVec2(0.035533f, -0.817259f),  ImVec2(-0.126904f, -0.959391f),
    ImVec2(-0.329949f, -1.0f),      ImVec2(-0.553299f, -0.969543f), ImVec2(-0.725888f, -0.847716f),
    ImVec2(-0.817259f, -0.664975f), ImVec2(-0.847716f, -0.431472f), ImVec2(-0.786802f, -0.238579f),
    ImVec2(-0.695431f, -0.116751f), ImVec2(-0.553299f, 0.045685f),  ImVec2(-0.553299f, 0.269036f),
    ImVec2(-0.532995f, 0.502538f),  ImVec2(-0.451777f, 0.695431f),  ImVec2(-0.28934f, 0.86802f),
    ImVec2(-0.208122f, 0.949239f)};

static const std::vector<ImVec2> s_xy_button_points = {
    {-0.4577f, 0.1615f },
    {-0.4477f, 0.0415f },
    {-0.4077f, -0.0305f},
    {-0.2977f, -0.1505f},
    {-0.0877f, -0.2705f},
    {0.0423f,  -0.3065f},
    {0.1923f,  -0.3185f},
    {0.3023f,  -0.3065f},
    {0.4023f,  -0.2825f},
    {0.4723f,  -0.1985f},
    {0.4923f,  -0.0905f},
    {0.4623f,  0.0175f },
    {0.3923f,  0.0895f },
    {0.3123f,  0.1255f },
    {0.1623f,  0.1495f },
    {0.0423f,  0.1975f },
    {-0.0277f, 0.2215f },
    {-0.1277f, 0.2935f },
    {-0.2277f, 0.3295f },
    {-0.3277f, 0.3295f },
    {-0.3777f, 0.2935f },
    {-0.4177f, 0.2455f },
};

static const std::vector<ImVec2> s_z_button_points = {
    ImVec2(-0.92f, -0.49f), ImVec2(-0.75f, -0.5f),  ImVec2(-0.65f, -0.495f), ImVec2(-0.45f, -0.46f),
    ImVec2(-0.35f, -0.43f), ImVec2(-0.15f, -0.38f), ImVec2(0.08f, -0.3f),    ImVec2(0.28f, -0.2f),
    ImVec2(0.45f, -0.1f),   ImVec2(0.58f, 0.0f),    ImVec2(0.70f, 0.1f),     ImVec2(0.82f, 0.2f),
    ImVec2(0.84f, 0.25f),   ImVec2(0.85f, 0.30f),   ImVec2(0.82f, 0.4f),     ImVec2(0.79f, 0.45f),
    ImVec2(0.76f, 0.5f),    ImVec2(-1.0f, -0.4f),   ImVec2(-0.97f, -0.48f),
};

static const std::vector<ImVec2> s_lr_button_points = {
    ImVec2(-0.95f, 0.70f),  ImVec2(-0.90f, 0.45f),  ImVec2(-0.80f, 0.20f),  ImVec2(-0.67f, -0.05f),
    ImVec2(-0.49f, -0.23f), ImVec2(-0.25f, -0.40f), ImVec2(-0.05f, -0.46f), ImVec2(0.13f, -0.48f),
    ImVec2(0.35f, -0.46f),  ImVec2(0.55f, -0.40f),  ImVec2(0.68f, -0.32f),  ImVec2(0.83f, -0.22f),
    ImVec2(0.92f, -0.08f),  ImVec2(0.97f, 0.02f),   ImVec2(0.99f, 0.14f),   ImVec2(1.00f, 0.30f),
};

namespace Toolbox::UI {

    PadInputWindow::PadInputWindow(const std::string &name)
        : ImWindow(name), m_pad_rail("mariomodoki") {}

    PadInputWindow::~PadInputWindow() {}

    void PadInputWindow::onRenderMenuBar() {
        if (ImGui::BeginMenuBar()) {
            bool is_recording = m_pad_recorder.isRecording();

            if (ImGui::BeginMenu("File")) {
                bool is_record_complete = !is_recording ? m_pad_recorder.isRecordComplete() : false;

                if (is_recording) {
                    ImGui::BeginDisabled();
                }

                if (ImGui::MenuItem("Open")) {
                    m_is_open_dialog_open = true;
                }

                if (!m_load_path) {
                    ImGui::BeginDisabled();
                }

                if (ImGui::MenuItem("Save")) {
                    if (!is_record_complete) {
                        TOOLBOX_ERROR("[PAD RECORD] Record is not complete. Please check remaining "
                                      "links for missing pad data.");
                    } else {
                        m_is_save_default_ready = true;
                        m_is_save_dialog_open   = true;
                    }
                }

                if (!m_load_path) {
                    ImGui::EndDisabled();
                }

                if (ImGui::MenuItem("Save As")) {
                    if (!is_record_complete) {
                        TOOLBOX_ERROR("[PAD RECORD] Record is not complete. Please check remaining "
                                      "links for missing pad data.");
                    } else {
                        m_is_save_dialog_open = true;
                    }
                }

                if (is_recording) {
                    ImGui::EndDisabled();
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Settings")) {
                if (is_recording) {
                    ImGui::BeginDisabled();
                }

                bool inverse_state = m_pad_recorder.isCameraInversed();
                if (ImGui::Checkbox("Make World Space", &inverse_state)) {
                    m_pad_recorder.setCameraInversed(inverse_state);
                }

                if (is_recording) {
                    ImGui::EndDisabled();
                }

                ImGui::Separator();

                ImGui::Checkbox("View Rumble", &m_is_viewing_rumble);
                ImGui::Checkbox("Controller Overlay", &m_render_controller_overlay);

                ImGui::EndMenu();
            }

            if (ImGui::MenuItem("Help")) {
                // TODO: Show help dialog
            }

            ImGui::EndMenuBar();
        }
    }

    void PadInputWindow::onRenderBody(TimeStep delta_time) {
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 5.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.0f);
        {
            ImVec2 window_pos            = ImGui::GetWindowPos();
            ImVec2 controller_cursor_pos = ImGui::GetCursorPos();

            ImGui::SetNextItemAllowOverlap();

            // Controller panel
            {
                ImGui::PushStyleColor(ImGuiCol_ChildBg,
                                      ImGui::GetStyleColorVec4(ImGuiCol_TableRowBgAlt));
                {
                    if (ImGui::BeginChild("Controller View", {0, 300}, true)) {
                        ImVec2 content_region = ImGui::GetContentRegionAvail();
                        ImVec2 center         = {content_region.x / 2, content_region.y / 2 - 20};
                        renderControllerOverlay(center, 1.0f, 255);
                    }
                    ImGui::EndChild();
                }
                ImGui::PopStyleColor();
            }

            ImVec2 after_cursor_pos = ImGui::GetCursorPos();
            ImVec2 item_rect_min    = ImGui::GetItemRectMin();
            ImVec2 item_rect_max    = ImGui::GetItemRectMax();

            // Record panel
            {
                ImGui::SetCursorPos(controller_cursor_pos);
                if (ImGui::BeginChild("Record Panel", {0, 0}, ImGuiChildFlags_None,
                                      ImGuiWindowFlags_AlwaysUseWindowPadding)) {
                    renderControlButtons();
                }
                ImGui::EndChild();
                ImGui::SetCursorPos(after_cursor_pos);
            }

            renderRecordedInputData();
            renderSceneContext();
            renderFileDialogs();

            m_create_link_dialog.render();
        }
        ImGui::PopStyleVar(3);
    }

    void PadInputWindow::renderControlButtons() {
        const ImVec2 frame_padding  = ImGui::GetStyle().FramePadding;
        const ImVec2 window_padding = ImGui::GetStyle().WindowPadding;

        std::string combo_preview = ICON_FK_EYE " Player";
        if (m_is_viewing_shadow_mario) {
            combo_preview = ICON_FK_EYE " Shadow Mario";
        } else if (m_is_viewing_piantissimo) {
            combo_preview = ICON_FK_EYE " Piantissimo";
        }

        ImGui::SetNextItemWidth(130.0f);

        bool is_selection_invalid = (m_is_viewing_shadow_mario && m_shadow_mario_ptr == 0) ||
                                    (m_is_viewing_piantissimo && m_piantissimo_ptr == 0);
        if (is_selection_invalid) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.0f, 0.0f, 0.4f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.7f, 0.2f, 0.2f, 0.6f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.7f, 0.0f, 0.0f, 0.6f));
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.7f, 0.0f, 0.0f, 0.3f));
            ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.7f, 0.2f, 0.2f, 0.5f));
        }

        if (ImGui::BeginCombo("##Viewing", combo_preview.c_str())) {
            if (ImGui::Selectable("Player",
                                  !m_is_viewing_shadow_mario && !m_is_viewing_piantissimo)) {
                m_is_viewing_shadow_mario = false;
                m_is_viewing_piantissimo  = false;
            }

            if (ImGui::Selectable("Shadow Mario", m_is_viewing_shadow_mario)) {
                m_is_viewing_shadow_mario = true;
                m_is_viewing_piantissimo  = false;
            }

            if (ImGui::Selectable("Piantissimo", m_is_viewing_piantissimo)) {
                m_is_viewing_shadow_mario = false;
                m_is_viewing_piantissimo  = true;
            }

            ImGui::EndCombo();
        }

        if (is_selection_invalid) {
            ImGui::PopStyleColor(5);
        }

        float combo_height = ImGui::GetItemRectSize().y;

        ImGui::SameLine();

        float window_bar_height =
            ImGui::GetStyle().FramePadding.y * 2.0f + ImGui::GetTextLineHeight();
        window_bar_height *= 2.0f;

        const ImVec2 window_size = ImGui::GetWindowSize();
        ImVec2 cmd_button_size   = ImGui::CalcTextSize(ICON_FK_UNDO) + frame_padding;
        cmd_button_size.x        = std::max(cmd_button_size.x, cmd_button_size.y) * 1.5f;
        cmd_button_size.y        = combo_height;

        bool is_recording = m_pad_recorder.isRecording();

        if (is_recording) {
            ImGui::BeginDisabled();
        }

        ImGui::PushStyleColor(ImGuiCol_Button, {0.1f, 0.35f, 0.1f, 0.9f});
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, {0.2f, 0.7f, 0.2f, 0.9f});
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.2f, 0.7f, 0.2f, 1.0f});

        ImGui::SetCursorPosX(window_size.x / 2 - cmd_button_size.x / 2);
        if (ImGui::AlignedButton(ICON_FK_CIRCLE, cmd_button_size)) {
            m_pad_recorder.startRecording();
            m_is_recording_pad_data = true;
            if (m_attached_scene_uuid) {
                GUIApplication::instance().dispatchEvent<BaseEvent, true>(
                    m_attached_scene_uuid, SCENE_DISABLE_CONTROL_EVENT);
            }
        }

        ImGui::PopStyleColor(3);

        if (is_recording) {
            ImGui::EndDisabled();
        }

        ImGui::SameLine();

        ImGui::PushStyleColor(ImGuiCol_Button, {0.35f, 0.1f, 0.1f, 0.9f});
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, {0.7f, 0.2f, 0.2f, 0.9f});
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.7f, 0.2f, 0.2f, 1.0f});

        if (!is_recording) {
            ImGui::BeginDisabled();
        }

        ImGui::SetCursorPosX(window_size.x / 2 - cmd_button_size.x / 2 + cmd_button_size.x);
        if (ImGui::AlignedButton(ICON_FK_STOP, cmd_button_size, ImGuiButtonFlags_None, 5.0f,
                                 ImDrawFlags_RoundCornersRight)) {
            m_pad_recorder.stopRecording();
            m_cur_from_link = '*';
            m_cur_to_link   = '*';
        }

        if (!is_recording) {
            ImGui::EndDisabled();
        }

        ImGui::PopStyleColor(3);

        ImGui::SameLine();

        ImGui::PushStyleColor(ImGuiCol_Button, {0.1f, 0.2f, 0.4f, 0.9f});
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, {0.2f, 0.4f, 0.8f, 0.9f});
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.2f, 0.4f, 0.8f, 1.0f});

        ImGui::SetCursorPosX(window_size.x / 2 - cmd_button_size.x / 2 - cmd_button_size.x);
        if (ImGui::AlignedButton(ICON_FK_UNDO, cmd_button_size, ImGuiButtonFlags_None, 5.0f,
                                 ImDrawFlags_RoundCornersLeft)) {
            m_pad_recorder.resetRecording();
            m_pad_rail.clearNodes();
        }

        ImGui::PopStyleColor(3);
    }

    void PadInputWindow::renderControllerOverlay(const ImVec2 &center, f32 scale, u8 alpha) {
        DolphinCommunicator &communicator = GUIApplication::instance().getDolphinCommunicator();
        if (!communicator.manager().isHooked()) {
            ImGui::Text("Dolphin not connected.");
            return;
        }

        PadRecorder::PadFrameData frame_data{};

        if (m_pad_recorder.isPlaying()) {
            frame_data = m_playback_data;
        } else if (m_is_viewing_shadow_mario) {
            auto result =
                m_pad_recorder.readPadFrameData(PadRecorder::PadSourceType::SOURCE_EMARIO);
            if (!result) {
                frame_data = {};
            } else {
                frame_data = result.value();
            }
        } else if (m_is_viewing_piantissimo) {
            auto result =
                m_pad_recorder.readPadFrameData(PadRecorder::PadSourceType::SOURCE_PIANTISSIMO);
            if (!result) {
                frame_data = {};
            } else {
                frame_data = result.value();
            }
        } else {
            auto result =
                m_pad_recorder.readPadFrameData(PadRecorder::PadSourceType::SOURCE_PLAYER);
            if (!result) {
                frame_data = {};
            } else {
                frame_data = result.value();
            }
        }

        ImVec2 rumble_position =
            m_is_viewing_rumble ? ImVec2(frame_data.m_rumble_x * 7.0f, frame_data.m_rumble_y * 7.0f)
                                : ImVec2(0.0f, 0.0f);

        // L button
        {
            bool is_l_pressed =
                (frame_data.m_held_buttons & PadButtons::BUTTON_L) != PadButtons::BUTTON_NONE;
            ImVec2 l_button_position = (ImVec2(-85, -57) + rumble_position) * scale + center;
            float l_button_radius    = 27.0f * scale;
            ImU32 l_button_color     = is_l_pressed ? IM_COL32(240, 240, 240, alpha)
                                                    : IM_COL32(160, 160, 160, alpha);

            std::vector<ImVec2> points = s_lr_button_points;

            for (ImVec2 &point : points) {
                point *= l_button_radius;
                point += l_button_position;
                point.y += 7.0f * (frame_data.m_trigger_l / 150.0f) * scale;
            }

            ImGui::DrawConvexPolygon(points.data(), static_cast<int>(points.size()), IM_COL32_BLACK,
                                     l_button_color, 2.0f);
        }

        // R button
        {
            bool is_r_pressed =
                (frame_data.m_held_buttons & PadButtons::BUTTON_R) != PadButtons::BUTTON_NONE;
            ImVec2 r_button_position = (ImVec2(85, -57) + rumble_position) * scale + center;
            float r_button_radius    = 28.0f * scale;
            ImU32 r_button_color     = is_r_pressed ? IM_COL32(240, 240, 240, alpha)
                                                    : IM_COL32(160, 160, 160, alpha);

            std::vector<ImVec2> points = s_lr_button_points;

            for (ImVec2 &point : points) {
                point.x *= -1;
                point *= r_button_radius;
                point += r_button_position;
                point.y += 7.0f * (frame_data.m_trigger_r / 150.0f) * scale;
            }

            ImGui::DrawConvexPolygon(points.data(), static_cast<int>(points.size()), IM_COL32_BLACK,
                                     r_button_color, 2.0f);
        }

        // Z button
        {
            bool is_z_pressed =
                (frame_data.m_held_buttons & PadButtons::BUTTON_Z) != PadButtons::BUTTON_NONE;
            ImVec2 z_button_position = (ImVec2(87, -47) + rumble_position) * scale + center;
            float z_button_radius    = 28.0f * scale;
            ImU32 z_button_color     = is_z_pressed ? IM_COL32(140, 80, 240, alpha)
                                                    : IM_COL32(80, 20, 200, alpha);

            std::vector<ImVec2> points = s_z_button_points;

            if (!is_z_pressed) {
                for (ImVec2 &point : points) {
                    f32 x   = point.x + 1;
                    f32 y   = point.y + 1;
                    point.x = x * std::cos(-IM_PI / 64.0f) - y * std::sin(-IM_PI / 64.0f) - 1;
                    point.y = x * std::sin(-IM_PI / 64.0f) + y * std::cos(-IM_PI / 64.0f) - 1;
                }
            }

            for (ImVec2 &point : points) {
                point *= z_button_radius;
                point += z_button_position;
            }

            ImGui::DrawConvexPolygon(points.data(), static_cast<int>(points.size()), IM_COL32_BLACK,
                                     z_button_color, 2.0f);
        }

        bool is_rumble_active = rumble_position.x != 0.0f && rumble_position.y != 0.0f;
        // Controller base
        {
            ImVec2 controller_position = (ImVec2(1, 33) + rumble_position) * scale + center;
            float controller_radius    = 100.0f * scale;
            ImU32 controller_color     = is_rumble_active ? IM_COL32(140, 60, 220, alpha)
                                                          : IM_COL32(100, 30, 180, alpha);

            std::vector<ImVec2> points = s_controller_base_points;
            for (ImVec2 &point : points) {
                point *= controller_radius;
                point += controller_position;
            }

            ImGui::DrawConcavePolygon(points.data(), static_cast<int>(points.size()),
                                      IM_COL32_BLACK, controller_color, 2.0f);
        }

        // Controller form l
        {
            ImVec2 controller_position = (ImVec2(-70, 23) + rumble_position) * scale + center;
            float controller_radius    = 69.0f * scale;
            ImU32 controller_color     = is_rumble_active ? IM_COL32(140, 60, 220, alpha)
                                                          : IM_COL32(100, 30, 180, alpha);

            std::vector<ImVec2> points = s_controller_form_l_points;
            for (ImVec2 &point : points) {
                point *= controller_radius;
                point.y *= -1.0f;
                point += controller_position;
            }

            ImGui::DrawConcavePolygon(points.data(), static_cast<int>(points.size()),
                                      IM_COL32_BLACK, controller_color, 2.0f);
        }

        // Controller form r
        {
            ImVec2 controller_position = (ImVec2(70, 23) + rumble_position) * scale + center;
            float controller_radius    = 69.0f * scale;
            ImU32 controller_color     = is_rumble_active ? IM_COL32(140, 60, 220, alpha)
                                                          : IM_COL32(100, 30, 180, alpha);

            std::vector<ImVec2> points = s_controller_form_r_points;
            for (ImVec2 &point : points) {
                point *= controller_radius;
                point.y *= -1.0f;
                point += controller_position;
            }

            ImGui::DrawConcavePolygon(points.data(), static_cast<int>(points.size()),
                                      IM_COL32_BLACK, controller_color, 2.0f);
        }

        // Control stick
        {
            float stick_radius    = 15.0f;
            ImVec2 stick_position = (ImVec2(-80, -1) + rumble_position) * scale + center;
            ImVec2 stick_tilted_position =
                stick_position +
                ImVec2(frame_data.m_stick_x, -frame_data.m_stick_y) * stick_radius * scale;
            ImU32 stick_color = IM_COL32(160, 160, 160, alpha);

            ImGui::DrawNgon(8, stick_position, (stick_radius + 6.0f) * scale, IM_COL32_BLACK,
                            stick_color, 2.0f, 0.0f);
            ImGui::DrawCircle(stick_tilted_position, stick_radius * scale, IM_COL32_BLACK,
                              stick_color, 2.0f);
        }

        // C stick
        {
            float c_stick_radius    = 11.00f;
            ImVec2 c_stick_position = (ImVec2(46, 57) + rumble_position) * scale + center;
            ImVec2 c_stick_tilted_position =
                c_stick_position +
                ImVec2(frame_data.m_c_stick_x, -frame_data.m_c_stick_y) * c_stick_radius * scale;
            ImU32 c_stick_color = IM_COL32(200, 160, 30, alpha);

            ImGui::DrawNgon(8, c_stick_position, (c_stick_radius + 10.00f) * scale, IM_COL32_BLACK,
                            c_stick_color, 2.0f, 0.0f);
            ImGui::DrawCircle(c_stick_tilted_position, c_stick_radius * scale, IM_COL32_BLACK,
                              c_stick_color, 2.0f);
        }

        // A button
        {
            bool is_a_pressed =
                (frame_data.m_held_buttons & PadButtons::BUTTON_A) != PadButtons::BUTTON_NONE;
            ImVec2 a_button_position = (ImVec2(81, 0) + rumble_position) * scale + center;
            ImU32 a_button_color     = is_a_pressed ? IM_COL32(90, 210, 150, alpha)
                                                    : IM_COL32(20, 170, 90, alpha);

            ImGui::DrawCircle(a_button_position, 17.0f * scale, IM_COL32_BLACK, a_button_color,
                              2.0f);
        }

        // B button
        {
            bool is_b_pressed =
                (frame_data.m_held_buttons & PadButtons::BUTTON_B) != PadButtons::BUTTON_NONE;
            ImVec2 b_button_position = (ImVec2(51, 16) + rumble_position) * scale + center;
            ImU32 b_button_color     = is_b_pressed ? IM_COL32(240, 130, 150, alpha)
                                                    : IM_COL32(210, 30, 50, alpha);

            ImGui::DrawCircle(b_button_position, 9.0f * scale, IM_COL32_BLACK, b_button_color,
                              2.0f);
        }

        // X button
        {
            bool is_x_pressed =
                (frame_data.m_held_buttons & PadButtons::BUTTON_X) != PadButtons::BUTTON_NONE;
            ImVec2 x_button_position = (ImVec2(110, -10) + rumble_position) * scale + center;
            float x_button_radius    = 31.0f * scale;
            ImU32 x_button_color     = is_x_pressed ? IM_COL32(240, 240, 240, alpha)
                                                    : IM_COL32(160, 160, 160, alpha);

            std::vector<ImVec2> points = s_xy_button_points;

            // Rotate clockwise by 45 degrees
            for (auto &point : points) {
                f32 x   = point.x;
                f32 y   = point.y;
                point.x = x * std::cos(IM_PI / 2.0f) - y * std::sin(IM_PI / 2.0f);
                point.y = x * std::sin(IM_PI / 2.0f) + y * std::cos(IM_PI / 2.0f);
            }

            for (auto &point : points) {
                point *= x_button_radius;
                point += x_button_position;
            }

            ImGui::DrawConcavePolygon(points.data(), static_cast<int>(points.size()),
                                      IM_COL32_BLACK, x_button_color, 2.0f);
        }

        // Y button
        {
            bool is_y_pressed =
                (frame_data.m_held_buttons & PadButtons::BUTTON_Y) != PadButtons::BUTTON_NONE;
            ImVec2 y_button_position = (ImVec2(71, -29) + rumble_position) * scale + center;
            float y_button_radius    = 31.0f * scale;
            ImU32 y_button_color     = is_y_pressed ? IM_COL32(240, 240, 240, alpha)
                                                    : IM_COL32(160, 160, 160, alpha);

            std::vector<ImVec2> points = s_xy_button_points;
            for (ImVec2 &point : points) {
                point *= y_button_radius;
                point += y_button_position;
            }

            ImGui::DrawConcavePolygon(points.data(), static_cast<int>(points.size()),
                                      IM_COL32_BLACK, y_button_color, 2.0f);
        }

        // Start button
        {
            bool is_start_pressed =
                (frame_data.m_held_buttons & PadButtons::BUTTON_START) != PadButtons::BUTTON_NONE;
            ImVec2 start_button_position = (ImVec2(0, 3) + rumble_position) * scale + center;
            ImU32 start_button_color     = is_start_pressed ? IM_COL32(240, 240, 240, alpha)
                                                            : IM_COL32(160, 160, 160, alpha);

            ImGui::DrawCircle(start_button_position, 7.0f * scale, IM_COL32_BLACK,
                              start_button_color, 2.0f);
        }

        // D-pad
        {
            bool is_up_pressed =
                (frame_data.m_held_buttons & PadButtons::BUTTON_UP) != PadButtons::BUTTON_NONE;
            bool is_down_pressed =
                (frame_data.m_held_buttons & PadButtons::BUTTON_DOWN) != PadButtons::BUTTON_NONE;
            bool is_left_pressed =
                (frame_data.m_held_buttons & PadButtons::BUTTON_LEFT) != PadButtons::BUTTON_NONE;
            bool is_right_pressed =
                (frame_data.m_held_buttons & PadButtons::BUTTON_RIGHT) != PadButtons::BUTTON_NONE;

            ImVec2 dpad_center = (ImVec2(-46, 57) + rumble_position) * scale + center;
            float dpad_radius  = 6.0f * scale;
            float dpad_size    = 3.5f;

            ImU32 dpad_color         = IM_COL32(160, 160, 160, alpha);
            ImU32 dpad_color_pressed = IM_COL32(240, 240, 240, alpha);

            float angle_offset = IM_PI / 4.0f;

            ImVec2 up_position    = dpad_center + ImVec2(0, -dpad_radius * 2);
            ImVec2 down_position  = dpad_center + ImVec2(0, dpad_radius * 2);
            ImVec2 left_position  = dpad_center + ImVec2(-dpad_radius * 2, 0);
            ImVec2 right_position = dpad_center + ImVec2(dpad_radius * 2, 0);

            // Draw outline
            {
                // Draw each direction
                ImGui::DrawSquare(up_position, dpad_radius * 2, IM_COL32_BLACK, 0, 2.0f);
                ImGui::DrawSquare(down_position, dpad_radius * 2, IM_COL32_BLACK, 0, 2.0f);
                ImGui::DrawSquare(left_position, dpad_radius * 2, IM_COL32_BLACK, 0, 2.0f);
                ImGui::DrawSquare(right_position, dpad_radius * 2, IM_COL32_BLACK, 0, 2.0f);
            }

            // Draw center
            ImGui::DrawSquare(dpad_center, dpad_radius * 2, IM_COL32_BLACK, dpad_color, 0.0f);

            // Draw up
            {
                ImU32 up_color = is_up_pressed ? dpad_color_pressed : dpad_color;
                ImGui::DrawSquare(up_position, dpad_radius * 2, IM_COL32_BLACK, up_color, 2.0f);
                ImGui::DrawNgon(3, up_position - ImVec2(0.5f, 0.5f), dpad_size, IM_COL32_BLACK,
                                dpad_color, 0.0f, IM_PI * 1.5f);
            }

            // Draw down
            {
                ImU32 down_color = is_down_pressed ? dpad_color_pressed : dpad_color;
                ImGui::DrawSquare(down_position, dpad_radius * 2, IM_COL32_BLACK, down_color, 2.0f);
                ImGui::DrawNgon(3, down_position - ImVec2(0.5f, 0.5f), dpad_size, IM_COL32_BLACK,
                                dpad_color, 0.0f, IM_PI * 0.5f);
            }

            // Draw left
            {
                ImU32 left_color = is_left_pressed ? dpad_color_pressed : dpad_color;
                ImGui::DrawSquare(left_position, dpad_radius * 2, IM_COL32_BLACK, left_color, 2.0f);
                ImGui::DrawNgon(3, left_position - ImVec2(0.5f, 0.5f), dpad_size, IM_COL32_BLACK,
                                dpad_color, 0.0f, IM_PI);
            }

            // Draw right
            {
                ImU32 right_color = is_right_pressed ? dpad_color_pressed : dpad_color;
                ImGui::DrawSquare(right_position, dpad_radius * 2, IM_COL32_BLACK, right_color,
                                  2.0f);
                ImGui::DrawNgon(3, right_position - ImVec2(0.5f, 0.5f), dpad_size, IM_COL32_BLACK,
                                dpad_color, 0.0f, 0.0f);
            }
        }
    }

    void PadInputWindow::renderRecordedInputData() {
        ImGui::SeparatorText("Record Links");
        {
            if (ImGui::BeginChild("##Record Link Controls", ImVec2(0, 0),
                                  ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysAutoResize)) {
                if (ImGui::Button("Add Link")) {
                    m_create_link_dialog.setActionOnAccept([this](char from_link, char to_link) {
                        onCreateLinkNode(from_link, to_link);
                    });
                    m_create_link_dialog.setActionOnReject([](char from_link, char to_link) {});
                    m_create_link_dialog.setLinkData(m_pad_recorder.linkData());
                    m_create_link_dialog.open();
                }
            }
            ImGui::EndChild();

            renderLinkDataState();
        }
    }

    void PadInputWindow::renderSceneContext() {
        ImGui::SeparatorText("Scene Context");
        if (ImGui::BeginChild("##Scene Context Elem")) {
            std::string window_preview = "Scene not selected.";

            const std::vector<RefPtr<ImWindow>> &windows = GUIApplication::instance().getWindows();
            auto window_it =
                std::find_if(windows.begin(), windows.end(), [this](RefPtr<ImWindow> window) {
                    return window->getUUID() == m_attached_scene_uuid;
                });
            if (window_it != windows.end()) {
                window_preview = (*window_it)->context();
            }

            if (m_pad_recorder.isRecording()) {
                ImGui::BeginDisabled();
            }

            if (ImGui::BeginCombo("##Scene Context Combo", window_preview.c_str(),
                                  ImGuiComboFlags_PopupAlignLeft)) {
                for (RefPtr<const ImWindow> window : windows) {
                    if (window->name() != "Scene Editor") {
                        continue;
                    }
                    bool selected = window->getUUID() == m_attached_scene_uuid;
                    if (ImGui::Selectable(window->context().c_str(), &selected)) {
                        GUIApplication::instance().deregisterDolphinOverlay(m_attached_scene_uuid,
                                                                            "Pad Record Overlay");
                        m_attached_scene_uuid = window->getUUID();
                        GUIApplication::instance().registerDolphinOverlay(
                            m_attached_scene_uuid, "Pad Record Overlay",
                            [this](TimeStep delta_time, std::string_view layer_name, int width,
                                   int height, const glm::mat4x4 &vp_mtx, UUID64 window_uuid) {
                                onRenderPadOverlay(delta_time, layer_name, width, height, vp_mtx,
                                                   window_uuid);
                            });
                    }
                }
                ImGui::EndCombo();
            }

            ImGui::SameLine();

            if (m_attached_scene_uuid == 0) {
                ImGui::BeginDisabled();
            }

            f32 cursor_pos_x = ImGui::GetWindowSize().x - ImGui::CalcTextSize("Apply Rail").x -
                               ImGui::GetStyle().FramePadding.x * 2.0f -
                               ImGui::GetStyle().WindowPadding.x;
            ImGui::SetCursorPosX(cursor_pos_x);

            if (ImGui::Button("Apply Rail")) {
                GUIApplication::instance().dispatchEvent<SceneCreateRailEvent, true>(
                    m_attached_scene_uuid, m_pad_rail);
            }

            if (m_attached_scene_uuid == 0) {
                ImGui::EndDisabled();
            }

            if (m_pad_recorder.isRecording()) {
                ImGui::EndDisabled();
            }
        }
        ImGui::EndChild();
    }

    void PadInputWindow::renderFileDialogs() {
        if (m_is_open_dialog_open) {
            ImGuiFileDialog::Instance()->OpenDialog(
                "OpenPadDialog", "Choose Folder", nullptr,
                m_load_path ? m_load_path->string().c_str() : ".", "");
        }

        if (ImGuiFileDialog::Instance()->Display("OpenPadDialog")) {
            m_is_open_dialog_open = false;
            if (ImGuiFileDialog::Instance()->IsOk()) {
                std::filesystem::path path = ImGuiFileDialog::Instance()->GetFilePathName();

                Toolbox::Filesystem::is_directory(path)
                    .and_then([&path, this](bool is_dir) {
                        if (!is_dir) {
                            return make_fs_error<bool>(std::error_code(),
                                                       {"Path is not a directory."});
                        }

                        m_load_path = path;

                        m_pad_recorder.resetRecording();
                        if (m_attached_scene_uuid) {
                            GUIApplication::instance().dispatchEvent<BaseEvent, true>(
                                m_attached_scene_uuid, SCENE_ENABLE_CONTROL_EVENT);
                        }

                        m_pad_recorder.loadFromFolder(*m_load_path);
                        return Result<bool, FSError>(true);
                    })
                    .or_else([](const FSError &error) {
                        LogError(error);
                        return Result<bool, FSError>(false);
                    });

                ImGuiFileDialog::Instance()->Close();
                return;
            }
        }

        if (m_is_save_dialog_open) {
            if (!m_is_save_default_ready) {
                ImGuiFileDialog::Instance()->OpenDialog(
                    "SavePadDialog", "Choose Folder", nullptr,
                    m_load_path ? m_load_path->string().c_str() : ".", "");
            } else {
                m_pad_recorder.saveToFolder(*m_load_path);
                return;
            }
        }

        if (ImGuiFileDialog::Instance()->Display("SavePadDialog")) {
            m_is_save_dialog_open = false;
            if (ImGuiFileDialog::Instance()->IsOk()) {
                m_load_path = ImGuiFileDialog::Instance()->GetFilePathName();

                m_pad_recorder.saveToFolder(*m_load_path);

                ImGuiFileDialog::Instance()->Close();
                return;
            }
        }

        if (m_is_import_dialog_open) {
            ImGuiFileDialog::Instance()->OpenDialog("ImportPadRecordDialog", "Choose File",
                                                    ".pad,.txt",
                                                    m_import_path ? m_import_path->string().c_str()
                                                    : m_load_path ? m_load_path->string().c_str()
                                                                  : ".",
                                                    "");
        }

        if (ImGuiFileDialog::Instance()->Display("ImportPadRecordDialog")) {
            m_is_import_dialog_open = false;
            if (ImGuiFileDialog::Instance()->IsOk()) {
                m_import_path = ImGuiFileDialog::Instance()->GetFilePathName();

                m_pad_recorder.loadPadRecording(m_cur_from_link, m_cur_to_link, *m_import_path);

                ImGuiFileDialog::Instance()->Close();
                return;
            }
        }

        if (m_is_export_dialog_open) {
            ImGuiFileDialog::Instance()->OpenDialog("ExportPadRecordDialog", "Choose File",
                                                    ".pad,.txt",
                                                    m_export_path ? m_export_path->string().c_str()
                                                    : m_load_path ? m_load_path->string().c_str()
                                                                  : ".",
                                                    "");
        }

        if (ImGuiFileDialog::Instance()->Display("ExportPadRecordDialog")) {
            m_is_export_dialog_open = false;
            if (ImGuiFileDialog::Instance()->IsOk()) {
                m_export_path = ImGuiFileDialog::Instance()->GetFilePathName();

                m_pad_recorder.savePadRecording(m_cur_from_link, m_cur_to_link, *m_export_path);

                ImGuiFileDialog::Instance()->Close();
                return;
            }
        }
    }

    void PadInputWindow::renderLinkDataState() {
        ImGuiID id = ImGui::GetID("Link Data State");

        float height = ImGui::GetTextLineHeight() * 2.0f + ImGui::GetFrameHeight();

        if (ImGui::BeginChildPanel(id, {0, ImGui::GetContentRegionAvail().y - height})) {
            bool is_recording                             = m_pad_recorder.isRecording();
            bool is_playing                               = m_pad_recorder.isPlaying();
            const std::vector<ReplayLinkNode> &link_nodes = m_pad_recorder.linkData()->linkNodes();
            for (size_t i = 0; i < link_nodes.size(); ++i) {
                for (size_t j = 0; j < 3; ++j) {
                    char from_link = 'A' + i;
                    char to_link   = link_nodes[i].m_infos[j].m_next_link;

                    bool should_disable =
                        is_recording ? !m_pad_recorder.isRecording(from_link, to_link) : false;
                    should_disable |= is_playing ? !m_pad_recorder.isPlaying(from_link, to_link)
                                                 : false;

                    if (should_disable) {
                        ImGui::BeginDisabled();
                    }

                    renderLinkPanel(link_nodes[i].m_infos[j], 'A' + i);

                    if (should_disable) {
                        ImGui::EndDisabled();
                    }
                }
            }
        }
        ImGui::EndChildPanel();
    }

    void PadInputWindow::renderLinkPanel(const ReplayNodeInfo &link_node, char link_chr) {
        char from_link = link_chr;
        char to_link   = link_node.m_next_link;
        if (to_link == '*') {
            return;
        }

        bool is_recording = m_pad_recorder.isRecording(from_link, to_link);

        std::string link_name = std::format(ICON_FK_LINK " {} -> {}", from_link, to_link);
        ImGuiID id            = ImGui::GetID(link_name.c_str());

        ImGuiStyle &style = ImGui::GetStyle();

        ImVec4 panel_color;
        if (m_cur_from_link == from_link && m_cur_to_link == to_link) {
            panel_color   = style.Colors[ImGuiCol_FrameBgActive];
            panel_color.w = 0.2f;
        } else {
            panel_color = m_pad_recorder.hasRecordData(from_link, to_link)
                              ? style.Colors[ImGuiCol_TableRowBgAlt]
                              : ImVec4(0.7f, 0.0f, 0.0f, 0.4f);
        }
        ImGui::PushStyleColor(ImGuiCol_TableRowBg, panel_color);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5);

        float panel_height = ImGui::GetTextLineHeightWithSpacing() + style.FramePadding.y * 2.0f;
        panel_height *= 2.0f;
        panel_height += style.ItemSpacing.y + style.FramePadding.y * 4.0f;

        ImVec2 panel_pos = ImGui::GetCursorPos();

        if (ImGui::BeginChildPanel(id, {0, panel_height})) {
            ImVec2 window_pos = ImGui::GetWindowPos();
            ImVec2 anchor_pos = ImGui::GetCursorPos();
            ImVec2 group_size = ImGui::GetContentRegionAvail();

            // Link name
            {
                ImVec2 text_size = ImGui::CalcTextSize(link_name.c_str());

                ImU32 tag_color =
                    ImGui::ColorConvertFloat4ToU32(style.Colors[ImGuiCol_TableHeaderBg]);

                ImGui::GetWindowDrawList()->AddRectFilled(
                    window_pos,
                    window_pos + ImVec2(text_size.x + style.FramePadding.x * 8, panel_height),
                    tag_color, 5.0f, ImDrawFlags_RoundCornersLeft);

                ImGui::SetCursorPosX(anchor_pos.x + style.FramePadding.x * 2);
                ImGui::SetCursorPosY(anchor_pos.y + (panel_height / 2.0f) -
                                     ImGui::GetTextLineHeight());
                ImGui::Text(link_name.c_str());
            }

            // Import/Export buttons
            {
                if (is_recording) {
                    ImGui::BeginDisabled();
                }

                ImGuiStyle &style         = ImGui::GetStyle();
                ImVec2 import_button_size = ImGui::CalcTextSize("Import") + style.FramePadding * 2;
                ImVec2 export_button_size = ImGui::CalcTextSize("Export") + style.FramePadding * 2;

                ImGui::SetCursorPos(
                    {anchor_pos.x + group_size.x -
                         (import_button_size.x + export_button_size.x + style.ItemSpacing.x),
                     anchor_pos.y});

                if (ImGui::Button("Import")) {
                    m_is_import_dialog_open = true;
                    m_cur_from_link         = from_link;
                    m_cur_to_link           = to_link;
                }

                ImGui::SameLine();

                if (ImGui::Button("Export")) {
                    m_is_export_dialog_open = true;
                    m_cur_from_link         = from_link;
                    m_cur_to_link           = to_link;
                }

                if (is_recording) {
                    ImGui::EndDisabled();
                }
            }

            // Pad record data controls
            {
                f32 record_button_width =
                    ImGui::CalcTextSize(ICON_FK_CIRCLE).x * 2.0f + style.FramePadding.x * 2;
                f32 play_button_width =
                    ImGui::CalcTextSize(ICON_FK_PLAY).x * 2.0f + style.FramePadding.x * 2;
                f32 trash_button_width =
                    ImGui::CalcTextSize(ICON_FK_TRASH).x * 2.0f + style.FramePadding.x * 2;

                ImVec2 button_panel_pos = {
                    anchor_pos.x + group_size.x -
                        (record_button_width + play_button_width + trash_button_width),
                    anchor_pos.y + ImGui::GetTextLineHeightWithSpacing() +
                        style.FramePadding.y * 2};

                ImGui::SetCursorPos(button_panel_pos);

                std::string button_label;
                if (m_pad_recorder.hasRecordData(from_link, to_link)) {
                    button_label = ICON_FK_UNDO;
                } else {
                    button_label = ICON_FK_CIRCLE;
                }

                if (is_recording) {
                    button_label = ICON_FK_SQUARE;
                }

                if (ImGui::AlignedButton(button_label.c_str(), {record_button_width, 0.0f},
                                         ImGuiButtonFlags_None, 5.0f,
                                         ImDrawFlags_RoundCornersLeft)) {
                    if (is_recording) {
                        // Submit task to stop recording pad data
                        m_update_tasks.push_back([this]() {
                            m_pad_recorder.stopRecording();
                            m_cur_from_link = '*';
                            m_cur_to_link   = '*';
                        });
                    } else {
                        // Submit task to record pad data
                        m_update_tasks.push_back([this, from_link, to_link]() {
                            Game::TaskCommunicator &task_communicator =
                                GUIApplication::instance().getTaskCommunicator();

                            Transform player_transform;
                            size_t from_index = from_link - 'A';
                            size_t to_index   = to_link - 'A';
                            player_transform.m_translation =
                                m_pad_rail.nodes()[from_index]->getPosition();

                            {
                                glm::vec3 from_to = m_pad_rail.nodes()[to_index]->getPosition() -
                                                    player_transform.m_translation;

                                player_transform.m_rotation = glm::vec3(
                                    0.0f, std::atan2f(from_to.x, from_to.z) * (180.0f / IM_PI),
                                    0.0f);
                            }

                            player_transform.m_scale = glm::vec3(1.0f);

                            task_communicator.setMarioTransform(player_transform, true);

                            m_cur_from_link = from_link;
                            m_cur_to_link   = to_link;
                            m_pad_recorder.startRecording(from_link, to_link);
                            m_is_recording_pad_data = true;
                            if (m_attached_scene_uuid) {
                                GUIApplication::instance().dispatchEvent<BaseEvent, true>(
                                    m_attached_scene_uuid, SCENE_DISABLE_CONTROL_EVENT);
                            }
                        });
                    }
                }

                ImGui::SetCursorPos({button_panel_pos.x + record_button_width, button_panel_pos.y});

                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0);

                // Submit task to play back pad data
                if (!m_pad_recorder.isPlaying(from_link, to_link)) {
                    if (ImGui::AlignedButton(ICON_FK_PLAY, {play_button_width, 0.0f})) {
                        m_update_tasks.push_back([this, from_link, to_link]() {
                            signalPadPlayback(from_link, to_link);
                        });
                    }
                } else {
                    if (ImGui::AlignedButton(ICON_FK_SQUARE, {play_button_width, 0.0f})) {
                        m_update_tasks.push_back(
                            [this, from_link, to_link]() { m_pad_recorder.stopPadPlayback(); });
                    }
                }

                ImGui::PopStyleVar();

                ImGui::SetCursorPos({button_panel_pos.x + record_button_width + play_button_width,
                                     button_panel_pos.y});

                // Submit task to clear pad data
                if (ImGui::AlignedButton(ICON_FK_TRASH, {trash_button_width, 0.0f},
                                         ImGuiButtonFlags_None, 5.0f,
                                         ImDrawFlags_RoundCornersRight)) {
                    // TODO: Defer to later so caller loop doesn't die
                    // (Generally processing should be done in the update loop)
                    m_update_tasks.push_back([this, from_link, to_link]() {
                        m_pad_recorder.clearLink(from_link, to_link);
                    });
                }
            }
        }
        ImGui::EndChildPanel();

        ImGui::PopStyleColor(1);
        ImGui::PopStyleVar(1);
    }

    void PadInputWindow::onRenderPadOverlay(TimeStep delta_time, std::string_view layer_name,
                                            int width, int height, const glm::mat4x4 &vp_mtx,
                                            UUID64 window_uuid) {
        tryRenderNodes(delta_time, layer_name, width, height, vp_mtx, window_uuid);

        ImGui::Text(layer_name.data());

        if (m_render_controller_overlay) {
            f32 controller_scale = static_cast<float>(std::min(width, height)) / 1500.0f;
            renderControllerOverlay({width * 0.12f, height * 0.9f}, controller_scale, 255);
        }
    }

    void PadInputWindow::loadMimePadData(Buffer &buffer) {}

    void PadInputWindow::tryReuseOrCreateRailNode(const ReplayLinkNode &link_node) {
        DolphinCommunicator &communicator = GUIApplication::instance().getDolphinCommunicator();
        u32 player_ptr                    = communicator.read<u32>(0x8040E108).value();
        if (!player_ptr) {
            if (m_pad_recorder.isCameraInversed()) {
                TOOLBOX_ERROR("[PAD RECORD] Player pointer is null. Please ensure that the game is "
                              "running and the player is loaded.");
                RefPtr<RailNode> node = make_referable<RailNode>(0, 0, 0);
                m_pad_rail.addNode(node);
                if (m_pad_rail.nodes().size() > 1) {
                    size_t prev_node_index = m_pad_rail.nodes().size() - 2;
                    if (prev_node_index == 0) {
                        m_pad_rail.connectNodeToNext(prev_node_index);
                    } else {
                        m_pad_rail.connectNodeToNeighbors(prev_node_index, true);
                    }
                    m_pad_rail.connectNodeToPrev(node);
                }
                return;
            }
            return;
        }

        Game::TaskCommunicator &task_communicator =
            GUIApplication::instance().getTaskCommunicator();
        {
            glm::vec3 player_position;
            task_communicator.getMarioTranslation(player_position);

            TOOLBOX_INFO("[PAD RECORD] Creating new rail node and snapping player to node.");

            glm::vec<3, s16> node_position = {static_cast<s16>(player_position.x),
                                              static_cast<s16>(player_position.y),
                                              static_cast<s16>(player_position.z)};
            RefPtr<RailNode> node =
                make_referable<RailNode>(node_position.x, node_position.y, node_position.z);
            m_pad_rail.addNode(node);
            if (m_pad_rail.nodes().size() > 1) {
                size_t prev_node_index = m_pad_rail.nodes().size() - 2;
                if (prev_node_index == 0) {
                    m_pad_rail.connectNodeToNext(prev_node_index);
                } else {
                    m_pad_rail.connectNodeToNeighbors(prev_node_index, true);
                }
                m_pad_rail.connectNodeToPrev(node);
            }

            task_communicator.setMarioTranslation(glm::vec3(node_position), true);
        }
    }

    void PadInputWindow::tryRenderNodes(TimeStep delta_time, std::string_view layer_name, int width,
                                        int height, const glm::mat4x4 &vp_mtx, UUID64 window_uuid) {
        DolphinCommunicator &communicator = GUIApplication::instance().getDolphinCommunicator();

        u32 application_address = 0x803E9700;
        u32 display_address     = communicator.read<u32>(application_address + 0x1C).value();
        u32 xfb_address         = communicator.read<u32>(display_address + 0x8).value();
        u16 xfb_width           = communicator.read<u16>(display_address + 0x14).value();
        u16 xfb_height          = communicator.read<u16>(display_address + 0x18).value();

        f32 client_to_game_ratio = ((f32)width / (f32)height) / ((f32)xfb_width / (f32)xfb_height);
        f32 scaled_width         = width;
        f32 scaled_height        = height * 1.025f;
        /*if (client_to_game_ratio > 1.0f) {
            scaled_width = width / (0.67f + client_to_game_ratio * 0.33f);
        } else {
            scaled_height = height * (0.5f + client_to_game_ratio * 0.5f);
        }*/

        u32 mario_address         = communicator.read<u32>(0x8040E108).value();
        glm::vec3 player_position = {
            communicator.read<f32>(mario_address + 0x10).value(),
            communicator.read<f32>(mario_address + 0x14).value(),
            communicator.read<f32>(mario_address + 0x18).value(),
        };

        struct NodeData {
            ImVec2 m_screen_position;
            f32 m_screen_depth;
            f32 m_player_distance;
        };

        bool is_node_recording = m_pad_recorder.isRecording(m_cur_from_link, m_cur_to_link);

        // Transform each rail node into screen space
        std::vector<NodeData> node_datas;

        std::vector<Rail::Rail::node_ptr_t> rail_nodes = m_pad_rail.nodes();
        for (const RefPtr<RailNode> &node : rail_nodes) {
            NodeData node_data;

            glm::vec3 node_position   = node->getPosition();
            glm::vec3 screen_position = vp_mtx * glm::vec4(node_position, 1.0f);

            node_data.m_screen_depth = -screen_position.z;
            if (node_data.m_screen_depth < FLT_EPSILON) {
                node_datas.push_back({});
                continue;
            }

            node_data.m_screen_position.x = screen_position.x / node_data.m_screen_depth;
            node_data.m_screen_position.x =
                ((node_data.m_screen_position.x + 1.0f) * 0.5f) * scaled_width;

            node_data.m_screen_position.y = screen_position.y / node_data.m_screen_depth;
            node_data.m_screen_position.y =
                ((1.0f - node_data.m_screen_position.y) * 0.5f) * scaled_height;

            glm::vec3 player_to_node    = node_position - player_position;
            node_data.m_player_distance = std::sqrtf(glm::dot(player_to_node, player_to_node));

            node_datas.emplace_back(node_data);
        }

        ImVec2 cursor_pos = ImGui::GetCursorPos();

        // Draw rail nodes
        for (size_t i = 0; i < rail_nodes.size(); ++i) {
            if (node_datas[i].m_screen_depth < FLT_EPSILON) {
                continue;
            }

            bool should_highlight = m_cur_to_link == ('A' + i) && is_node_recording;

            ImVec2 node_position = node_datas[i].m_screen_position;
            f32 node_size        = 20000.0f / node_datas[i].m_screen_depth;
            node_size            = std::clamp(node_size, 2.0f, 10000.0f);
            ImU32 node_color     = should_highlight ? IM_COL32(0, 255, 0, 200)
                                                    : IM_COL32(255, 0, 0, 200);
            ImGui::DrawCircle(node_position, node_size, IM_COL32_BLACK, node_color, 2.0f);

            std::string distance_text =
                std::format("{:.2f}m", node_datas[i].m_player_distance * 0.01);

            ImVec2 text_size = ImGui::CalcTextSize(distance_text.c_str());

            ImVec2 item_padding = ImGui::GetStyle().FramePadding;
            ImVec2 back_pos =
                node_position - ImVec2(text_size.x * 0.5f, text_size.y + node_size + 5.0f);
            ImGui::GetWindowDrawList()->AddRectFilled(
                ImGui::GetWindowPos() + back_pos - item_padding,
                ImGui::GetWindowPos() + back_pos + text_size + item_padding, IM_COL32(0, 0, 0, 200),
                5.0f);

            ImGui::SetCursorPos(back_pos);
            ImGui::Text(distance_text.c_str());
        }

        ImGui::SetCursorPos(cursor_pos);
    }

    void PadInputWindow::onCreateLinkNode(char from_link, char to_link) {
        RefPtr<ReplayLinkData> link_data        = m_pad_recorder.linkData();
        std::vector<ReplayLinkNode> &link_nodes = link_data->linkNodes();

        if (from_link - 'A' < link_nodes.size()) {
            ReplayLinkNode &link_node = link_nodes[from_link - 'A'];
            for (size_t i = 0; i < 3; ++i) {
                if (link_node.m_infos[i].m_next_link == to_link) {
                    TOOLBOX_ERROR("[PAD RECORD] Link {} -> {} already exists.", from_link, to_link);
                    return;
                }

                if (link_node.m_infos[i].m_next_link == '*') {
                    link_node.m_infos[i].m_next_link = to_link;
                    return;
                }
            }

            TOOLBOX_ERROR("[PAD RECORD] Link {} is full.", from_link);
            return;
        }

        ReplayLinkNode node{};
        {
            node.m_node_name = std::format("Link{}", from_link);

            ReplayNodeInfo node_info{};
            node_info.m_next_link = to_link;
            node_info.m_unk_0     = 1;

            node.m_infos[0] = std::move(node_info);
        }

        link_nodes.push_back(std::move(node));
    }

    void PadInputWindow::signalPadPlayback(char from_link, char to_link) {
        if (m_pad_recorder.getPadFrameCount(from_link, to_link) == 0) {
            TOOLBOX_ERROR("[PAD RECORD] No pad data found for link {} -> {}", from_link, to_link);
            return;
        }

        Game::TaskCommunicator &task_communicator =
            GUIApplication::instance().getTaskCommunicator();

        RefPtr<RailNode> from_node = m_pad_rail.nodes()[from_link - 'A'];

        PadRecorder::PadFrameData data = m_pad_recorder.getPadFrameData(from_link, to_link, 0);

        Transform from_transform;
        from_transform.m_translation = from_node->getPosition();
        from_transform.m_rotation    = {0.0f, PadData::convertAngleS16ToFloat(data.m_stick_angle),
                                        0.0f};
        from_transform.m_scale       = {1.0f, 1.0f, 1.0f};

        task_communicator.setMarioTransform(from_transform, true);

        m_pad_recorder.playPadRecording(
            from_link, to_link,
            [this](const PadRecorder::PadFrameData &data) { m_playback_data = data; });
    }

    f32 PadInputWindow::getDistanceFromPlayer(const glm::vec3 &pos) const {
        DolphinCommunicator &communicator = GUIApplication::instance().getDolphinCommunicator();
        u32 mario_address                 = communicator.read<u32>(0x8040E108).value();
        glm::vec3 player_position         = {
            communicator.read<f32>(mario_address + 0x10).value(),
            communicator.read<f32>(mario_address + 0x14).value(),
            communicator.read<f32>(mario_address + 0x18).value(),
        };
        glm::vec3 player_to_node = pos - player_position;
        return std::sqrtf(glm::dot(player_to_node, player_to_node));
    }

    bool PadInputWindow::onLoadData(const std::filesystem::path &path) {
        auto file_result = Toolbox::Filesystem::is_directory(path);
        if (!file_result) {
            return false;
        }

        if (!file_result.value()) {
            return false;
        }

        if (Toolbox::Filesystem::is_directory(path)) {
            if (!path.filename().string().starts_with("pad")) {
                return false;
            }

            return m_pad_recorder.loadFromFolder(path);
        }

        // TODO: Implement opening from archives.
        return false;
    }

    bool PadInputWindow::onSaveData(std::optional<std::filesystem::path> path) {
        if (!path && !m_load_path) {
            return false;
        }
        return m_pad_recorder.saveToFolder(path ? *path : *m_load_path);
    }

    void PadInputWindow::onAttach() {
        m_pad_recorder.onCreateLink(
            [this](const ReplayLinkNode &node) { tryReuseOrCreateRailNode(node); });
        m_pad_recorder.tStart(false, nullptr);
    }

    void PadInputWindow::onDetach() { m_pad_recorder.tKill(true); }

    void PadInputWindow::onImGuiUpdate(TimeStep delta_time) {
        for (auto &task : m_update_tasks) {
            task();
        }
        m_update_tasks.clear();

        if (m_pad_recorder.isRecording(m_cur_from_link, m_cur_to_link)) {
            Rail::Rail::node_ptr_t to_node = m_pad_rail.nodes()[m_cur_to_link - 'A'];
            if (getDistanceFromPlayer(to_node->getPosition()) < 20.0f) {
                m_pad_recorder.stopRecording();
                m_cur_from_link = '*';
                m_cur_to_link   = '*';
            }
        }

        if (m_pad_recorder.isPlaying(m_cur_from_link, m_cur_to_link)) {
            DolphinCommunicator &communicator = GUIApplication::instance().getDolphinCommunicator();
            if (communicator.manager().isHooked()) {
            }
        }

        if (!m_pad_recorder.isRecording() && m_is_recording_pad_data) {
            m_is_recording_pad_data = false;
            if (m_attached_scene_uuid) {
                GUIApplication::instance().dispatchEvent<BaseEvent, true>(
                    m_attached_scene_uuid, SCENE_ENABLE_CONTROL_EVENT);
            }
        }
    }

    void PadInputWindow::onContextMenuEvent(RefPtr<ContextMenuEvent> ev) {}

    void PadInputWindow::onDragEvent(RefPtr<DragEvent> ev) {}

    void PadInputWindow::onDropEvent(RefPtr<DropEvent> ev) {}

}  // namespace Toolbox::UI