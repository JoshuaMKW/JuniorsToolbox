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
    ImVec2(0.126939728447689, -0.9979365868418832),
    ImVec2(0.24654011951415217, -0.9834500658396175),
    ImVec2(0.3991203624960013, -0.9642552499613684),
    ImVec2(0.4731294497400282, -0.9422992692885306),
    ImVec2(0.555472391832531, -0.9243304500576878),
    ImVec2(0.6377442204293304, -0.9022302426980523),
    ImVec2(0.6993571959488083, -0.8804906369602239),
    ImVec2(0.7568398006395951, -0.8588231271133879),
    ImVec2(0.8143935188260857, -0.8412870053953447),
    ImVec2(0.8676716396493923, -0.8155602556679278),
    ImVec2(0.9209497430202924, -0.7898325060928159),
    ImVec2(0.9701686065106118, -0.7683092403851841),
    ImVec2(1.0435276389115409, -0.7091689413818865),
    ImVec2(1.1047800128092116, -0.6667704825667368),
    ImVec2(1.1615404493135018, -0.603787283713055),
    ImVec2(1.18959663126559, -0.55370300774473),
    ImVec2(1.2258434412270518, -0.4993431344132213),
    ImVec2(1.2410718137894867, -0.42468599202148705),
    ImVec2(1.2604315570283078, -0.34995673628635393),
    ImVec2(1.274722403769338, -0.22158877555851345),
    ImVec2(1.2917022670567455, -0.010516288721074168),
    ImVec2(1.308321546174752, 0.2212140513459913),
    ImVec2(1.2969784397531323, 0.634302002138202),
    ImVec2(1.2825650319419606, 0.7497710399806854),
    ImVec2(1.2605359555304354, 0.8279104806009969),
    ImVec2(1.230677835126632, 0.8811164880809047),
    ImVec2(1.1966152308553488, 0.9383817354413934),
    ImVec2(1.125368273348793, 0.9949989276541746),
    ImVec2(1.050783261752864, 1.0060949296925281),
    ImVec2(0.9809786583763175, 0.9800797091391094),
    ImVec2(0.928061139174818, 0.9336941063343712),
    ImVec2(0.9000049397703234, 0.8836108302137414),
    ImVec2(0.8762253724815205, 0.8253349438407543),
    ImVec2(0.8567213851034955, 0.7588694293060888),
    ImVec2(0.8412045591674652, 0.7007377696198994),
    ImVec2(0.8235035100888499, 0.5309819890894076),
    ImVec2(0.8094290208304226, 0.3902189164847134),
    ImVec2(0.7865859620629226, 0.2782331941709088),
    ImVec2(0.7707806653008905, 0.2366280170379594),
    ImVec2(0.750771867066667, 0.1990830970856925),
    ImVec2(0.7267759248428548, 0.15320332243725432),
    ImVec2(0.6208687556440506, 0.06456448735185925),
    ImVec2(0.5799857466975977, 0.039054095107045404),
    ImVec2(0.53497038467947, 0.013470572218731113),
    ImVec2(0.49408737573301714, -0.012039820026082731),
    ImVec2(0.4531312535954703, -0.0334188590469168),
    ImVec2(0.40818900476843656, -0.06313373515924768),
    ImVec2(0.3668049440877048, -0.08364690579452823),
    ImVec2(0.32546364595397516, -0.10704697421285306),
    ImVec2(0.22621878502070732, -0.1226670720812432),
    ImVec2(0.12282118431437794, -0.14296770752534527),
    ImVec2(-0.1341262527410403, -0.1449181844869373),
    ImVec2(-0.21694248525807607, -0.1239674386736311),
    ImVec2(-0.299596901514939, -0.10743815317818765),
    ImVec2(-0.3533061194768429, -0.08677708787917242),
    ImVec2(-0.3987603042385443, -0.07024821100337546),
    ImVec2(-0.44421509987946595, -0.0495866418631448),
    ImVec2(-0.4855370793342304, -0.03305834364810655),
    ImVec2(-0.5268596232513064, -0.012396642684578782),
    ImVec2(-0.5849825298519895, 0.010453239209275725),
    ImVec2(-0.6857991194839402, 0.10374895650102961),
    ImVec2(-0.7366045637910664, 0.17312084491237023),
    ImVec2(-0.7621149734882868, 0.21400485370651823),
    ImVec2(-0.7834949949044095, 0.25495995854396347),
    ImVec2(-0.8058836730900469, 0.35375826969400276),
    ImVec2(-0.8246457914554964, 0.48154928876983943),
    ImVec2(-0.84768350718413, 0.6175309358550498),
    ImVec2(-0.8655802305239805, 0.6957435071188617),
    ImVec2(-0.8875361937444118, 0.7697515945151934),
    ImVec2(-0.9094200436214442, 0.8396283112351388),
    ImVec2(-0.931015422672475, 0.8929785454018443),
    ImVec2(-0.998203105393637, 0.9537982217867155),
    ImVec2(-1.118524682251308, 0.9806284070913974),
    ImVec2(-1.1799213002881828, 0.9464936894767152),
    ImVec2(-1.2407419765207492, 0.8793059893031466),
    ImVec2(-1.2683644611123428, 0.8044325068812159),
    ImVec2(-1.292145045701247, 0.7461576029035175),
    ImVec2(-1.3067243632682795, 0.634316124728917),
    ImVec2(-1.3194276293448075, 0.4150530273345106),
    ImVec2(-1.2902599129267363, -0.07211612404512581),
    ImVec2(-1.2659240697548912, -0.2824674425437636),
    ImVec2(-1.2478831371806494, -0.3689417553126528),
    ImVec2(-1.2256386856818098, -0.4594773251099196),
    ImVec2(-1.1997677092675951, -0.5210181872859987),
    ImVec2(-1.165560860857108, -0.5865471758469549),
    ImVec2(-1.0728627858911317, -0.688249861178279),
    ImVec2(-0.9975565885039951, -0.74066256952358),
    ImVec2(-0.9474723125356702, -0.7687187514756681),
    ImVec2(-0.8931124392041613, -0.8049655614371299),
    ImVec2(-0.8430281632358365, -0.8330217433892182),
    ImVec2(-0.7848253900539435, -0.8526699574540411),
    ImVec2(-0.7348853582248227, -0.8724623982056618),
    ImVec2(-0.689148810415487, -0.8881955816242949),
    ImVec2(-0.6185519252044354, -0.9076274556589214),
    ImVec2(-0.5562888948418596, -0.9230721682515528),
    ImVec2(-0.4980861216599666, -0.9427203823163757),
    ImVec2(-0.4027701261909109, -0.9575881532570034),
    ImVec2(-0.30738301722615163, -0.9765873123264237),
    ImVec2(-0.21206802160479116, -0.9914551007194579),
    ImVec2(-0.005271132130535799, -1.0002443359974917)};

static const std::vector<ImVec2> s_controller_form_l_points = {
    ImVec2(0.208122, 0.949239),   ImVec2(0.28934, 0.86802),     ImVec2(0.451777, 0.695431),
    ImVec2(0.532995, 0.502538),   ImVec2(0.553299, 0.269036),   ImVec2(0.553299, 0.045685),
    ImVec2(0.695431, -0.116751),  ImVec2(0.786802, -0.238579),  ImVec2(0.847716, -0.431472),
    ImVec2(0.817259, -0.664975),  ImVec2(0.725888, -0.847716),  ImVec2(0.553299, -0.969543),
    ImVec2(0.329949, -1.0),       ImVec2(0.126904, -0.959391),  ImVec2(-0.035533, -0.817259),
    ImVec2(-0.137056, -0.624365), ImVec2(-0.248731, -0.431472), ImVec2(-0.340102, -0.380711),
    ImVec2(-0.522843, -0.279188), ImVec2(-0.695431, -0.137056), ImVec2(-0.817259, 0.055838),
    ImVec2(-0.847716, 0.258883),  ImVec2(-0.837563, 0.482234),  ImVec2(-0.746193, 0.685279),
    ImVec2(-0.604061, 0.857868),  ImVec2(-0.42132, 0.969543),   ImVec2(-0.19797, 1.0),
    ImVec2(0.015228, 0.979695)};

static const std::vector<ImVec2> s_controller_form_r_points = {
    ImVec2(-0.015228, 0.979695),  ImVec2(0.19797, 1.0),         ImVec2(0.42132, 0.969543),
    ImVec2(0.604061, 0.857868),   ImVec2(0.746193, 0.685279),   ImVec2(0.837563, 0.482234),
    ImVec2(0.847716, 0.258883),   ImVec2(0.817259, 0.055838),   ImVec2(0.695431, -0.137056),
    ImVec2(0.522843, -0.279188),  ImVec2(0.340102, -0.380711),  ImVec2(0.248731, -0.431472),
    ImVec2(0.137056, -0.624365),  ImVec2(0.035533, -0.817259),  ImVec2(-0.126904, -0.959391),
    ImVec2(-0.329949, -1.0),      ImVec2(-0.553299, -0.969543), ImVec2(-0.725888, -0.847716),
    ImVec2(-0.817259, -0.664975), ImVec2(-0.847716, -0.431472), ImVec2(-0.786802, -0.238579),
    ImVec2(-0.695431, -0.116751), ImVec2(-0.553299, 0.045685),  ImVec2(-0.553299, 0.269036),
    ImVec2(-0.532995, 0.502538),  ImVec2(-0.451777, 0.695431),  ImVec2(-0.28934, 0.86802),
    ImVec2(-0.208122, 0.949239)};

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
    ImVec2(-0.92, -0.49), ImVec2(-0.75, -0.5),  ImVec2(-0.65, -0.495), ImVec2(-0.45, -0.46),
    ImVec2(-0.35, -0.43), ImVec2(-0.15, -0.38), ImVec2(0.08, -0.3),    ImVec2(0.28, -0.2),
    ImVec2(0.45, -0.1),   ImVec2(0.58, 0.0),    ImVec2(0.70, 0.1),     ImVec2(0.82, 0.2),
    ImVec2(0.84, 0.25),   ImVec2(0.85, 0.30),   ImVec2(0.82, 0.4),     ImVec2(0.79, 0.45),
    ImVec2(0.76, 0.5),    ImVec2(-1.0, -0.4),   ImVec2(-0.97, -0.48),
};

static const std::vector<ImVec2> s_lr_button_points = {
    ImVec2(-0.95, 0.70),  ImVec2(-0.90, 0.45),  ImVec2(-0.80, 0.20),  ImVec2(-0.67, -0.05),
    ImVec2(-0.49, -0.23), ImVec2(-0.25, -0.40), ImVec2(-0.05, -0.46), ImVec2(0.13, -0.48),
    ImVec2(0.35, -0.46),  ImVec2(0.55, -0.40),  ImVec2(0.68, -0.32),  ImVec2(0.83, -0.22),
    ImVec2(0.92, -0.08),  ImVec2(0.97, 0.02),   ImVec2(0.99, 0.14),   ImVec2(1.00, 0.30),
};

namespace Toolbox::UI {

    PadInputWindow::PadInputWindow() : ImWindow("Pad Recorder"), m_pad_rail("mariomodoki") {
        m_pad_recorder.tStart(false, nullptr);
        m_pad_recorder.onCreateLink(
            [this](const ReplayLinkNode &node) { tryReuseOrCreateRailNode(node); });
    }

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
                if (ImGui::Checkbox("Inverse Camera Transform", &inverse_state)) {
                    m_pad_recorder.setCameraInversed(inverse_state);
                }

                if (is_recording) {
                    ImGui::EndDisabled();
                }

                ImGui::Separator();

                ImGui::Checkbox("View Rumble", &m_is_viewing_rumble);

                ImGui::EndMenu();
            }

            if (ImGui::MenuItem("Help")) {
                // TODO: Show help dialog
            }

            ImGui::EndMenuBar();
        }
    }

    void PadInputWindow::onRenderBody(TimeStep delta_time) {
        renderRecordPanel();
        renderControllerView();
        renderRecordedInputData();
        renderFileDialogs();
    }

    void PadInputWindow::renderRecordPanel() {
        float window_bar_height =
            ImGui::GetStyle().FramePadding.y * 2.0f + ImGui::GetTextLineHeight();
        window_bar_height *= 2.0f;
        ImGui::SetCursorPos({0, window_bar_height + 5});

        const ImVec2 frame_padding  = ImGui::GetStyle().FramePadding;
        const ImVec2 window_padding = ImGui::GetStyle().WindowPadding;

        const ImVec2 window_size = ImGui::GetWindowSize();
        ImVec2 cmd_button_size   = ImGui::CalcTextSize(ICON_FK_UNDO) + frame_padding;
        cmd_button_size.x        = std::max(cmd_button_size.x, cmd_button_size.y) * 1.5f;
        cmd_button_size.y        = std::max(cmd_button_size.x, cmd_button_size.y) * 1.f;

        bool is_recording = m_pad_recorder.isRecording();

        if (is_recording) {
            ImGui::BeginDisabled();
        }

        ImGui::PushStyleColor(ImGuiCol_Button, {0.1f, 0.35f, 0.1f, 0.9f});
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, {0.2f, 0.7f, 0.2f, 0.9f});
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.2f, 0.7f, 0.2f, 1.0f});

        ImGui::SetCursorPosX(window_size.x / 2 - cmd_button_size.x / 2);
        if (ImGui::AlignedButton(ICON_FK_PLAY, cmd_button_size)) {
            m_pad_recorder.startRecording();
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
        if (ImGui::AlignedButton(ICON_FK_STOP, cmd_button_size)) {
            m_pad_recorder.stopRecording();
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
        if (ImGui::AlignedButton(ICON_FK_UNDO, cmd_button_size)) {
            m_pad_recorder.resetRecording();
        }

        ImGui::PopStyleColor(3);
    }

    void PadInputWindow::renderControllerView() {
        ImGui::PushStyleColor(ImGuiCol_WindowBg, {0.8, 0.2, 0.7, 1.0});

        if (ImGui::Checkbox("View Shadow Mario", &m_is_viewing_shadow_mario)) {
            m_is_viewing_piantissimo = false;
        }

        if (ImGui::Checkbox("View Piantissimo", &m_is_viewing_piantissimo)) {
            m_is_viewing_shadow_mario = false;
        }

        ImGui::BeginChild("Controller View", {0, 300}, true);

        DolphinCommunicator &communicator = GUIApplication::instance().getDolphinCommunicator();
        if (!communicator.manager().isHooked()) {
            ImGui::Text("Dolphin not connected.");
            ImGui::EndChild();
            ImGui::PopStyleColor();
            return;
        }

        PadButtons held_buttons    = PadButtons::BUTTON_NONE;
        PadButtons pressed_buttons = PadButtons::BUTTON_NONE;

        u8 trigger_l = 0;
        u8 trigger_r = 0;

        f32 stick_x = 0.0f;
        f32 stick_y = 0.0f;

        f32 c_stick_x = 0.0f;
        f32 c_stick_y = 0.0f;

        f32 stick_mag   = 0.0f;
        s16 stick_angle = 0;

        if (m_is_viewing_shadow_mario) {
            Game::TaskCommunicator &task_communicator =
                GUIApplication::instance().getTaskCommunicator();
            if (!task_communicator.isSceneLoaded(m_scene_id, m_episode_id) ||
                m_shadow_mario_ptr == 0) {
                if (!task_communicator.getLoadedScene(m_scene_id, m_episode_id)) {
                    TOOLBOX_ERROR("[PAD RECORD] Scene not loaded.");
                    m_shadow_mario_ptr        = 0;
                    m_is_viewing_shadow_mario = false;
                } else {
                    std::string shadow_mario_name =
                        "\x83\x7D\x83\x8A\x83\x49\x83\x82\x83\x68\x83\x4C\x5F\x30";
                    m_shadow_mario_ptr = task_communicator.getActorPtr(shadow_mario_name);
                }
            }
            if (m_shadow_mario_ptr == 0) {
                m_is_viewing_shadow_mario = false;
                TOOLBOX_ERROR("[PAD RECORD] Shadow Mario not found in scene.");
            } else {
                u32 enemy_mario_ptr = communicator.read<u32>(m_shadow_mario_ptr + 0x150).value();
                stick_mag   = communicator.read<f32>(enemy_mario_ptr + 0x8C).value() / 32.0f;
                stick_angle = communicator.read<s16>(enemy_mario_ptr + 0x90).value();
                stick_x     = stick_mag *
                          std::cos(PadData::convertAngleS16ToFloat(stick_angle) * (IM_PI / 180.0f) -
                                   IM_PI / 2);
                stick_y = stick_mag *
                          std::sin(PadData::convertAngleS16ToFloat(stick_angle) * (IM_PI / 180.0f) -
                                   IM_PI / 2);

                u32 controller_meaning_ptr =
                    communicator.read<u32>(enemy_mario_ptr + 0x108).value();
                pressed_buttons = static_cast<PadButtons>(
                    communicator.read<u32>(controller_meaning_ptr + 0x8).value());
                held_buttons = static_cast<PadButtons>(
                    communicator.read<u32>(controller_meaning_ptr + 0x4).value());
            }
        } else if (m_is_viewing_piantissimo) {
            Game::TaskCommunicator &task_communicator =
                GUIApplication::instance().getTaskCommunicator();
            if (!task_communicator.isSceneLoaded(m_scene_id, m_episode_id) ||
                m_piantissimo_ptr == 0) {
                if (!task_communicator.getLoadedScene(m_scene_id, m_episode_id)) {
                    TOOLBOX_ERROR("[PAD RECORD] Scene not loaded.");
                    m_shadow_mario_ptr        = 0;
                    m_is_viewing_shadow_mario = false;
                } else {
                    std::string piantissimo_name = "\x83\x82\x83\x93\x83\x65\x83\x7D\x83\x93";
                    m_piantissimo_ptr            = task_communicator.getActorPtr(piantissimo_name);
                }
            }
            if (m_piantissimo_ptr == 0) {
                m_is_viewing_piantissimo = false;
                TOOLBOX_ERROR("[PAD RECORD] Shadow Mario not found in scene.");
            } else {
                u32 enemy_mario_ptr = communicator.read<u32>(m_piantissimo_ptr + 0x150).value();
                stick_mag   = communicator.read<f32>(enemy_mario_ptr + 0x8C).value() / 32.0f;
                stick_angle = communicator.read<s16>(enemy_mario_ptr + 0x90).value();
                stick_x     = stick_mag *
                          std::cos(PadData::convertAngleS16ToFloat(stick_angle) * (IM_PI / 180.0f) -
                                   IM_PI / 2);
                stick_y = stick_mag *
                          std::sin(PadData::convertAngleS16ToFloat(stick_angle) * (IM_PI / 180.0f) -
                                   IM_PI / 2);

                u32 controller_meaning_ptr =
                    communicator.read<u32>(enemy_mario_ptr + 0x108).value();
                pressed_buttons = static_cast<PadButtons>(
                    communicator.read<u32>(controller_meaning_ptr + 0x8).value());
                held_buttons = static_cast<PadButtons>(
                    communicator.read<u32>(controller_meaning_ptr + 0x4).value());
            }
        } else {
            u32 application_ptr = 0x803E9700;
            u32 gamepad_ptr =
                communicator.read<u32>(application_ptr + 0x20 + (m_pad_recorder.getPort() << 2))
                    .value();

            bool is_connected = communicator.read<u8>(gamepad_ptr + 0x7A).value() != 0xFF;
            if (!is_connected) {
                TOOLBOX_ERROR("[PAD RECORD] Controller is not connected. Please ensure that "
                              "the controller is "
                              "connected and the input is being read by Dolphin.");
                return;
            }

            held_buttons =
                static_cast<PadButtons>(communicator.read<u32>(gamepad_ptr + 0x18).value());
            pressed_buttons =
                static_cast<PadButtons>(communicator.read<u32>(gamepad_ptr + 0x1C).value());

            trigger_l = communicator.read<u8>(gamepad_ptr + 0x26).value();
            trigger_r = communicator.read<u8>(gamepad_ptr + 0x27).value();

            stick_x = communicator.read<f32>(gamepad_ptr + 0x48).value();
            stick_y = communicator.read<f32>(gamepad_ptr + 0x4C).value();

            c_stick_x = communicator.read<f32>(gamepad_ptr + 0x58).value();
            c_stick_y = communicator.read<f32>(gamepad_ptr + 0x5C).value();

            stick_mag   = communicator.read<f32>(gamepad_ptr + 0x50).value();
            stick_angle = communicator.read<s16>(gamepad_ptr + 0x54).value();
        }

        f32 rumble_x   = 0.0f;
        f32 rumble_y   = 0.0f;
        u32 rumble_ptr = communicator.read<u32>(0x804141C0 - 0x60F0).value();
        if (rumble_ptr && !m_is_viewing_shadow_mario && !m_is_viewing_piantissimo &&
            m_is_viewing_rumble) {
            u32 data_ptr =
                communicator.read<u32>(rumble_ptr + 0xC + (m_pad_recorder.getPort() << 2)).value();
            rumble_x = communicator.read<f32>(data_ptr + 0x0).value();
            rumble_y = communicator.read<f32>(data_ptr + 0x4).value();
        }

        ImVec2 rumble_position = {rumble_x * 7.0f, rumble_y * 7.0f};

        // L button
        {
            bool is_l_pressed = (held_buttons & PadButtons::BUTTON_L) != PadButtons::BUTTON_NONE;
            ImVec2 l_button_position = {85, 50};
            float l_button_radius    = 27.0f;
            ImU32 l_button_color     = is_l_pressed ? IM_COL32(240, 240, 240, 255)
                                                    : IM_COL32(160, 160, 160, 255);

            std::vector<ImVec2> points = s_lr_button_points;

            for (ImVec2 &point : points) {
                point *= l_button_radius;
                point += l_button_position + rumble_position;
                point.y += 7.0f * (trigger_l / 150.0f);
            }

            ImGui::DrawConvexPolygon(points.data(), points.size(), IM_COL32_BLACK, l_button_color,
                                     2.0f);
        }

        // R button
        {
            bool is_r_pressed = (held_buttons & PadButtons::BUTTON_R) != PadButtons::BUTTON_NONE;
            ImVec2 r_button_position = {255, 50};
            float r_button_radius    = 28.0f;
            ImU32 r_button_color     = is_r_pressed ? IM_COL32(240, 240, 240, 255)
                                                    : IM_COL32(160, 160, 160, 255);

            std::vector<ImVec2> points = s_lr_button_points;

            for (ImVec2 &point : points) {
                point.x *= -1;
                point *= r_button_radius;
                point += r_button_position + rumble_position;
                point.y += 7.0f * (trigger_r / 150.0f);
            }

            ImGui::DrawConvexPolygon(points.data(), points.size(), IM_COL32_BLACK, r_button_color,
                                     2.0f);
        }

        // Z button
        {
            bool is_z_pressed = (held_buttons & PadButtons::BUTTON_Z) != PadButtons::BUTTON_NONE;
            ImVec2 z_button_position = {257, 60};
            float z_button_radius    = 28.0f;
            ImU32 z_button_color     = is_z_pressed ? IM_COL32(140, 80, 240, 255)
                                                    : IM_COL32(80, 20, 200, 255);

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
                point += z_button_position + rumble_position;
            }

            ImGui::DrawConvexPolygon(points.data(), points.size(), IM_COL32_BLACK, z_button_color,
                                     2.0f);
        }

        bool is_rumble_active = rumble_x != 0.0f && rumble_y != 0.0f;
        // Controller base
        {
            ImVec2 controller_position = {171, 140};
            float controller_radius    = 100.0f;
            ImU32 controller_color     = is_rumble_active ? IM_COL32(140, 60, 220, 255)
                                                          : IM_COL32(100, 30, 180, 255);

            std::vector<ImVec2> points = s_controller_base_points;
            for (ImVec2 &point : points) {
                point *= controller_radius;
                point += controller_position + rumble_position;
            }

            ImGui::DrawConcavePolygon(points.data(), points.size(), IM_COL32_BLACK,
                                      controller_color, 2.0f);
        }

        // Controller form l
        {
            ImVec2 controller_position = {100, 130};
            float controller_radius    = 69.0f;
            ImU32 controller_color     = is_rumble_active ? IM_COL32(140, 60, 220, 255)
                                                          : IM_COL32(100, 30, 180, 255);

            std::vector<ImVec2> points = s_controller_form_l_points;
            for (ImVec2 &point : points) {
                point *= controller_radius;
                point.y *= -1.0f;
                point += controller_position + rumble_position;
            }

            ImGui::DrawConcavePolygon(points.data(), points.size(), IM_COL32_BLACK,
                                      controller_color, 2.0f);
        }

        // Controller form r
        {
            ImVec2 controller_position = {240, 130};
            float controller_radius    = 69.0f;
            ImU32 controller_color     = is_rumble_active ? IM_COL32(140, 60, 220, 255)
                                                          : IM_COL32(100, 30, 180, 255);

            std::vector<ImVec2> points = s_controller_form_r_points;
            for (ImVec2 &point : points) {
                point *= controller_radius;
                point.y *= -1.0f;
                point += controller_position + rumble_position;
            }

            ImGui::DrawConcavePolygon(points.data(), points.size(), IM_COL32_BLACK,
                                      controller_color, 2.0f);
        }

        // Control stick
        {
            float stick_radius    = 15.0f;
            ImVec2 stick_position = ImVec2(90, 106) + rumble_position;
            ImVec2 stick_tilted_position =
                stick_position + ImVec2(stick_x * stick_radius, -stick_y * stick_radius);
            ImU32 stick_color = IM_COL32(160, 160, 160, 255);
            ImVec2 stick_angle_position =
                stick_position +
                ImVec2(std::cos(PadData::convertAngleS16ToFloat(stick_angle) * (IM_PI / 180.0f) -
                                IM_PI / 2) *
                           stick_radius,
                       -std::sin(PadData::convertAngleS16ToFloat(stick_angle) * (IM_PI / 180.0f) -
                                 IM_PI / 2) *
                           stick_radius);

            ImGui::DrawNgon(8, stick_position, stick_radius + 7.5f, IM_COL32_BLACK, stick_color,
                            2.0f, 0.0f);
            ImGui::DrawCircle(stick_tilted_position, stick_radius, IM_COL32_BLACK, stick_color,
                              2.0f);
            ImGui::DrawCircle(stick_angle_position, 1.5f, IM_COL32_WHITE, IM_COL32(255, 0, 0, 255),
                              2.0f);
        }

        // C stick
        {
            float c_stick_radius    = 9.00f;
            ImVec2 c_stick_position = ImVec2(216, 164) + rumble_position;
            ImVec2 c_stick_tilted_position =
                c_stick_position + ImVec2(c_stick_x * c_stick_radius, -c_stick_y * c_stick_radius);
            ImU32 c_stick_color = IM_COL32(200, 160, 30, 255);

            ImGui::DrawNgon(8, c_stick_position, c_stick_radius + 11.00f, IM_COL32_BLACK,
                            c_stick_color, 2.0f, 0.0f);
            ImGui::DrawCircle(c_stick_tilted_position, c_stick_radius, IM_COL32_BLACK,
                              c_stick_color, 2.0f);
        }

        // A button
        {
            bool is_a_pressed = (held_buttons & PadButtons::BUTTON_A) != PadButtons::BUTTON_NONE;
            ImVec2 a_button_position = ImVec2(251, 107) + rumble_position;
            ImU32 a_button_color     = is_a_pressed ? IM_COL32(90, 210, 150, 255)
                                                    : IM_COL32(20, 170, 90, 255);

            ImGui::DrawCircle(a_button_position, 15.0f, IM_COL32_BLACK, a_button_color, 2.0f);
        }

        // B button
        {
            bool is_b_pressed = (held_buttons & PadButtons::BUTTON_B) != PadButtons::BUTTON_NONE;
            ImVec2 b_button_position = ImVec2(221, 123) + rumble_position;
            ImU32 b_button_color     = is_b_pressed ? IM_COL32(240, 130, 150, 255)
                                                    : IM_COL32(210, 30, 50, 255);

            ImGui::DrawCircle(b_button_position, 8.0f, IM_COL32_BLACK, b_button_color, 2.0f);
        }

        // X button
        {
            bool is_x_pressed = (held_buttons & PadButtons::BUTTON_X) != PadButtons::BUTTON_NONE;
            ImVec2 x_button_position = ImVec2(280, 97) + rumble_position;
            float x_button_radius    = 31.0f;
            ImU32 x_button_color     = is_x_pressed ? IM_COL32(240, 240, 240, 255)
                                                    : IM_COL32(160, 160, 160, 255);

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

            ImGui::DrawConcavePolygon(points.data(), points.size(), IM_COL32_BLACK, x_button_color,
                                      2.0f);
        }

        // Y button
        {
            bool is_y_pressed = (held_buttons & PadButtons::BUTTON_Y) != PadButtons::BUTTON_NONE;
            ImVec2 y_button_position = ImVec2(241, 78) + rumble_position;
            float y_button_radius    = 31.0f;
            ImU32 y_button_color     = is_y_pressed ? IM_COL32(240, 240, 240, 255)
                                                    : IM_COL32(160, 160, 160, 255);

            std::vector<ImVec2> points = s_xy_button_points;
            for (ImVec2 &point : points) {
                point *= y_button_radius;
                point += y_button_position;
            }

            ImGui::DrawConcavePolygon(points.data(), points.size(), IM_COL32_BLACK, y_button_color,
                                      2.0f);
        }

        // Start button
        {
            bool is_start_pressed =
                (held_buttons & PadButtons::BUTTON_START) != PadButtons::BUTTON_NONE;
            ImVec2 start_button_position = ImVec2(170, 110) + rumble_position;
            ImU32 start_button_color     = is_start_pressed ? IM_COL32(240, 240, 240, 255)
                                                            : IM_COL32(160, 160, 160, 255);

            ImGui::DrawCircle(start_button_position, 6.0f, IM_COL32_BLACK, start_button_color,
                              2.0f);
        }

        // D-pad
        {
            bool is_up_pressed = (held_buttons & PadButtons::BUTTON_UP) != PadButtons::BUTTON_NONE;
            bool is_down_pressed =
                (held_buttons & PadButtons::BUTTON_DOWN) != PadButtons::BUTTON_NONE;
            bool is_left_pressed =
                (held_buttons & PadButtons::BUTTON_LEFT) != PadButtons::BUTTON_NONE;
            bool is_right_pressed =
                (held_buttons & PadButtons::BUTTON_RIGHT) != PadButtons::BUTTON_NONE;

            ImVec2 dpad_center = ImVec2(124, 164) + rumble_position;
            float dpad_radius  = 6.0f;
            float dpad_size    = 3.5f;

            ImU32 dpad_color         = IM_COL32(160, 160, 160, 255);
            ImU32 dpad_color_pressed = IM_COL32(240, 240, 240, 255);

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

        ImGui::EndChild();

        ImGui::PopStyleColor();
    }

    void PadInputWindow::renderRecordedInputData() {
        ImGui::BeginChild("Recorded Input Data", {0, 0}, true);

        std::string window_preview = "Scene not selected.";

        const std::vector<RefPtr<ImWindow>> &windows = GUIApplication::instance().getWindows();
        auto window_it =
            std::find_if(windows.begin(), windows.end(), [this](RefPtr<ImWindow> window) {
                return window->getUUID() == m_attached_scene_uuid;
            });
        if (window_it != windows.end()) {
            window_preview = (*window_it)->context();
        }

        ImGui::Text("Scene Context");
        ImGui::PushID("Scene Context Combo");
        if (ImGui::BeginCombo("", window_preview.c_str(), ImGuiComboFlags_PopupAlignLeft)) {
            for (RefPtr<const ImWindow> window : windows) {
                if (window->name() != "Scene Editor") {
                    continue;
                }
                bool selected = window->getUUID() == m_attached_scene_uuid;
                if (ImGui::Selectable(window->context().c_str(), &selected)) {
                    m_attached_scene_uuid = window->getUUID();
                }
            }
            ImGui::EndCombo();
        }
        ImGui::PopID();

        ImGui::SameLine();

        if (m_attached_scene_uuid == 0) {
            ImGui::BeginDisabled();
        }

        if (ImGui::Button("Make Scene Rail")) {
            GUIApplication::instance().dispatchEvent<SceneCreateRailEvent, true>(
                m_attached_scene_uuid, m_pad_rail);
        }

        if (m_attached_scene_uuid == 0) {
            ImGui::EndDisabled();
        }

        ImGui::Separator();

        renderLinkDataState();

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

                auto file_result = Toolbox::is_directory(path);
                if (!file_result) {
                    ImGuiFileDialog::Instance()->Close();
                    return;
                }

                if (!file_result.value()) {
                    ImGuiFileDialog::Instance()->Close();
                    return;
                }

                m_load_path = path;

                m_pad_recorder.resetRecording();
                m_pad_recorder.loadFromFolder(*m_load_path);

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
        if (ImGui::BeginChildPanel(id, {0, 0})) {
            const std::vector<ReplayLinkNode> &link_nodes = m_pad_recorder.linkData().linkNodes();
            for (size_t i = 0; i < link_nodes.size(); ++i) {
                for (size_t j = 0; j < 3; ++j) {
                    renderLinkPanel(link_nodes[i].m_infos[j], 'A' + i);
                }
            }
            ImGui::EndChildPanel();
        }
    }

    void PadInputWindow::renderLinkPanel(const ReplayNodeInfo &link_node, char link_chr) {
        char from_link = link_chr;
        char to_link   = link_node.m_next_link;
        if (to_link == '*') {
            return;
        }

        Game::TaskCommunicator &task_communicator =
            GUIApplication::instance().getTaskCommunicator();

        std::string link_name = std::format(ICON_FK_LINK " {} -> {}", from_link, to_link);
        if (ImGui::BeginGroupPanel(link_name.c_str(), nullptr, {0, 0})) {
            ImVec2 anchor_pos = ImGui::GetCursorPos();
            ImVec2 group_size = ImGui::GetContentRegionAvail();

            ImGui::Text("Pad Record - ");
            ImGui::SameLine(0.0f, 0.0f);

            bool is_recording = m_pad_recorder.isRecording(from_link, to_link);

            std::string button_label;
            if (m_pad_recorder.hasRecordData(from_link, to_link)) {
                button_label = ICON_FK_UNDO;
            } else {
                button_label = ICON_FK_CIRCLE;
            }

            if (is_recording) {
                button_label = ICON_FK_SQUARE;
            }

            if (ImGui::Button(button_label.c_str())) {
                if (is_recording) {
                    m_pad_recorder.stopRecording();
                } else {

                    Transform player_transform;
                    size_t from_index              = from_link - 'A';
                    size_t to_index                = to_link - 'A';
                    player_transform.m_translation = m_pad_rail.nodes()[from_index]->getPosition();

                    {
                        glm::vec3 from_to = m_pad_rail.nodes()[to_index]->getPosition() -
                                            player_transform.m_translation;

                        player_transform.m_rotation =
                            glm::vec3(0.0f, std::atan2(from_to.x, from_to.z) * (180.0f / IM_PI), 0.0f);
                    }

                    player_transform.m_scale = glm::vec3(1.0f);

                    task_communicator.setMarioTransform(player_transform);

                    m_pad_recorder.startRecording(from_link, to_link);
                }
            }

            ImGui::SameLine(0.0f, 0.0f);

            if (ImGui::Button(ICON_FK_PLAY)) {
                m_pad_recorder.playPadRecording(from_link, to_link);
            }

            ImGui::SameLine(0.0f, 0.0f);

            if (ImGui::Button(ICON_FK_TRASH)) {
                m_pad_recorder.clearLink(from_link, to_link);
            }

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
            ImGui::EndGroupPanel();
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
        glm::vec3 player_position = {communicator.read<f32>(player_ptr + 0x10).value(),
                                     communicator.read<f32>(player_ptr + 0x14).value(),
                                     communicator.read<f32>(player_ptr + 0x18).value()};

        std::vector<Rail::Rail::node_ptr_t> rail_nodes = m_pad_rail.nodes();
        auto node_it = std::find_if(rail_nodes.begin(), rail_nodes.end(),
                                    [&player_position](const Rail::Rail::node_ptr_t &node) {
                                        glm::vec3 node_pos = node->getPosition();
                                        glm::vec3 pos_diff = node_pos - player_position;
                                        return std::sqrtf(glm::dot(pos_diff, pos_diff)) < 100.0f;
                                    });
        if (node_it != rail_nodes.end()) {
            TOOLBOX_INFO("[PAD RECORD] Reusing existing rail node and snapping player to node.");
            glm::vec3 node_position = (*node_it)->getPosition();
            communicator.write<f32>(player_ptr + 0x10, node_position.x);
            communicator.write<f32>(player_ptr + 0x14, node_position.y);
            communicator.write<f32>(player_ptr + 0x18, node_position.z);
            return;
        }

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

        communicator.write<f32>(player_ptr + 0x10, static_cast<f32>(node_position.x));
        communicator.write<f32>(player_ptr + 0x14, static_cast<f32>(node_position.y));
        communicator.write<f32>(player_ptr + 0x18, static_cast<f32>(node_position.z));
    }

    bool PadInputWindow::onLoadData(const std::filesystem::path &path) {
        auto file_result = Toolbox::is_directory(path);
        if (!file_result) {
            return false;
        }

        if (!file_result.value()) {
            return false;
        }

        if (Toolbox::is_directory(path)) {
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

    void PadInputWindow::onImGuiUpdate(TimeStep delta_time) {}

    void PadInputWindow::onImGuiPostUpdate(TimeStep delta_time) {}

    void PadInputWindow::onContextMenuEvent(RefPtr<ContextMenuEvent> ev) {}

    void PadInputWindow::onDragEvent(RefPtr<DragEvent> ev) {}

    void PadInputWindow::onDropEvent(RefPtr<DropEvent> ev) {}

}  // namespace Toolbox::UI