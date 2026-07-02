#include "bmg/bmg.hpp"

#include "core/mathutil.hpp"
#include "gui/appmain/application.hpp"
#include "gui/appmain/bmgeditor/p_renderers.hpp"
#include "gui/appmain/bmgeditor/window.hpp"
#include "gui/logging/errors.hpp"
#include "spline.hpp"

constexpr ImVec4 MessageTintColor   = ImVec4(1.0f, 1.0f, 1.0f, 0.8f);
constexpr float MessageFadeSoftness = 0.1f;

constexpr std::string_view NPCMessageBackFragmentShader = R"(
// fade_shader.frag
in vec2 TexCoord; // The UV coordinates from ImGui
out vec4 FragColor;

uniform sampler2D myTexture;
uniform float u_progress; // 0.0 to 1.0 (How far along the fade is)
uniform float u_softness; // e.g., 0.15 (How wide the soft gradient edge is)

void main()
{
    vec4 color = texture(myTexture, TexCoord);
    
    float fade_alpha = 1.0 - smoothstep(u_progress - u_softness, u_progress, TexCoord.x);
    
    // Apply the fade to the image's original alpha
    FragColor = vec4(color.rgb, color.a * fade_alpha);
}
)";

static constexpr float GetDeltaScalarFromSpeedValue(u32 speed_value) {
    // Value 8: 6 frames per 60 per sec -> 8 = 1/10 of a sec
    // Value 15: 10 frames per 60 per sec -> 15 = 1/6 of a sec

    return std::min<float>(60.0f / (static_cast<float>(speed_value) * 0.7f), 30.0f);
}

namespace Toolbox::UI {

    BMGEditorViewNPC::BMGEditorViewNPC(bool is_mirrored) {
        {
            static const std::vector<CatmullSpline2D::ControlPoint> control_points = {
                {-1.0f, 0.7f},
                {0.0f,  0.0f},
                {1.0f,  0.0f},
                {2.0f,  0.7f},
            };

            ScopePtr<CatmullSpline2D> spline = make_scoped<CatmullSpline2D>(control_points);
            spline->setScale(400.0f);
            // spline->setRotate(IM_PI * 0.15f);

            m_talk_spline = std::move(spline);
        }

        {
            static const std::vector<CatmullSpline2D::ControlPoint> control_points = {
                {0.0f, 0.2f},
                {0.0f, 0.2f},
                {0.5f, 1.0f},
                {1.0f, 0.2f},
                {1.0f, 0.2f},
            };

            ScopePtr<CatmullSpline2D> spline = make_scoped<CatmullSpline2D>(control_points);
            m_button_fader_spline            = std::move(spline);
        }

        ResourceManager &manager = MainApplication::instance().getResourceManager();

        const UUID64 editor_dir_uuid = manager.getResourcePathUUID("Images/BMGEditor");

        m_talk_line_image =
            manager.getImageHandle("message_back.png", editor_dir_uuid).value_or(nullptr);
        m_talk_button_image =
            manager.getImageHandle("message_button_back.png", editor_dir_uuid).value_or(nullptr);
        m_talk_arrow_image =
            manager.getImageHandle("message_cursor.png", editor_dir_uuid).value_or(nullptr);
        m_talk_exit_image =
            manager.getImageHandle("message_return.png", editor_dir_uuid).value_or(nullptr);
        m_talk_option_image =
            manager.getImageHandle("message_option_back.png", editor_dir_uuid).value_or(nullptr);

        if (!UI::Render::CompileShader(nullptr, nullptr, NPCMessageBackFragmentShader.data(),
                                       m_back_shader_id)) {
            TOOLBOX_ERROR("Failed to compile NPC message back shader!");
        }
    }

    bool BMGEditorViewNPC::canPlayback() const { return true; }
    bool BMGEditorViewNPC::isPlayback() const { return m_is_playback; }
    bool BMGEditorViewNPC::startPlayback() {
        m_is_playback = true;
        resetPlaybackStateForPage();
        return true;
    }
    bool BMGEditorViewNPC::stopPlayback() {
        m_is_playback = false;
        return true;
    }

    bool BMGEditorViewNPC::render(TimeStep delta_time, const ImRect &render_rect,
                                  const BMG::MessageData::Entry &message) {
        m_talk_options[0] = "";
        m_talk_options[1] = "";

        const std::string &message_str = message.getMessage().getString();
        if (message_str.empty()) {
            return false;
        }

        if (m_is_playback) {
            renderForPlayback(delta_time, render_rect, message);
        } else {
            renderForPaused(delta_time, render_rect, message);
        }

        return true;
    }

    void BMGEditorViewNPC::renderForPaused(TimeStep delta_time, const ImRect &render_rect,
                                           const BMG::MessageData::Entry &message) {
        const std::string &message_str = message.getMessage().getString();

        const float render_factor = render_rect.GetSize().x / 600.0f;

        const float font_size        = 16.0f;
        const float scaled_font_size = font_size * render_factor;

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

        const size_t page_count = ((message_lines.size() - 1) / 3) + 1;

        const size_t render_line_base = (m_current_page * 3);
        const size_t render_line_count =
            std::clamp<size_t>(message_lines.size() - render_line_base, 0, 3);
        const bool is_last_page = message_lines.size() - render_line_base <= 3;

        ImFont *font = FontManager::instance().getFont("BMGEditor/FOTPopHappinessStrpGCN-Bold");
        ImGui::PushFont(font, scaled_font_size);

        const ImVec4 text_color_backup = ImGui::GetStyle().Colors[ImGuiCol_Text];

        ImagePainter message_painter;
        message_painter.setRotation(IM_PI * 0.10f, ImVec2(0.0f, 0.0f));
        message_painter.setTintColor(MessageTintColor);

        for (size_t i = 0; i < render_line_count; ++i) {
            ImGui::SetCursorScreenPos(render_rect.Min);

            const ImVec2 spline_translation = {(285.0f - i * 7.0f) * render_factor,
                                               (24.0f + i * 23.0f) * render_factor};

            if (m_talk_line_image) {
                const auto [image_width, image_height] = m_talk_line_image->size();
                const ImVec2 render_size               = ImVec2(
                    220.0f * render_factor, image_height * (220.0f / image_width) * render_factor);
                message_painter.render(*m_talk_line_image,
                                       render_rect.Min + spline_translation +
                                           ImVec2(-5.0f, -24.0f) * render_factor,
                                       render_size);
            }

            ImGui::SetCursorScreenPos(render_rect.Min);
            m_talk_spline->setTranslate(spline_translation.x, spline_translation.y);
            m_talk_spline->setRotate(IM_PI * 0.10f);
            m_talk_spline->setScale(197.0f * render_factor);

            std::string_view line_text = message_lines[render_line_base + i];
            renderLine(line_text);
        }

        bool next_page_requested = false;

        if (m_talk_button_image) {
            ImagePainter button_painter;
            button_painter.setTintColor(MessageTintColor);

            constexpr ImVec2 ButtonPosBase = ImVec2(476.0f, 100.0f);
            const ImVec2 button_pos =
                render_rect.Min + (ButtonPosBase * render_factor) +
                ImVec2((render_line_count - 1) * -7.0f, (render_line_count - 1) * 23.0f) *
                    render_factor;
            const ImVec2 button_size = ImVec2(20.0f * render_factor, 20.0f * render_factor);
            const ImRect button_bb   = {button_pos, button_pos + button_size};
            const ImGuiID button_id  = ImGui::GetID("##NPCEditorViewButton");
            ImGui::ItemSize(button_bb);
            if (!ImGui::ItemAdd(button_bb, button_id)) {
                TOOLBOX_ERROR("[BMG_WINDOW] Failed to register hitbox for message view button");
            }

            button_painter.render(*m_talk_button_image, button_pos, button_size);

            next_page_requested = ImGui::IsItemClicked();

            const f32 button_icon_alpha =
                m_button_fader_spline->getInterpolatedPoint(m_button_fader_lerp).m_y;
            m_button_fader_lerp =
                Wrap<float>(m_button_fader_lerp + delta_time * 0.8f, 0.0f, 1.0f);  // 50bpm

            button_painter.setTintColor({1.0f, 1.0f, 1.0f, button_icon_alpha});
            if (is_last_page) {
                button_painter.render(*m_talk_exit_image, button_pos + button_size * 0.175f,
                                      button_size * 0.65f);
            } else {
                const ImVec2 arrow_ofs = ImVec2(-1.0f, 8.5f) * render_factor;
                button_painter.setRotation(IM_PI * -0.29f, {0.0f, 0.0f});
                button_painter.render(*m_talk_arrow_image,
                                      button_pos + arrow_ofs + button_size * 0.175f,
                                      button_size * 0.65f);
            }
        }

        ImGui::GetStyle().Colors[ImGuiCol_Text] = text_color_backup;

        ImGui::PopFont();

        if (next_page_requested) {
            m_current_page = (m_current_page + 1) % page_count;
        }
    }

    void BMGEditorViewNPC::renderForPlayback(TimeStep delta_time, const ImRect &render_rect,
                                             const BMG::MessageData::Entry &message) {
        ImGuiStyle &style = ImGui::GetStyle();

        const std::string &message_str = message.getMessage().getString();

        const float render_factor = render_rect.GetSize().x / 600.0f;

        const float font_size        = 16.0f;
        const float scaled_font_size = font_size * render_factor;

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

        const size_t page_count = ((message_lines.size() - 1) / 3) + 1;

        const size_t render_line_base = (m_current_page * 3);
        const size_t render_line_count =
            std::clamp<size_t>(message_lines.size() - render_line_base, 0, 3);
        const bool is_last_page = message_lines.size() - render_line_base <= 3;

        if (render_line_count == 0) {
            return;
        }

        const size_t playback_line_count =
            std::min<size_t>(render_line_count, m_message_back_playback_index + 1);

        ImFont *font = FontManager::instance().getFont("BMGEditor/FOTPopHappinessStrpGCN-Bold");
        ImGui::PushFont(font, scaled_font_size);

        const ImVec4 text_color_backup = style.Colors[ImGuiCol_Text];
        style.Colors[ImGuiCol_Text]    = BMGEditorWindow::GetTextColorFromIndex(0);

        ImagePainter message_painter;
        message_painter.setRotation(IM_PI * 0.10f, ImVec2(0.0f, 0.0f));
        message_painter.setTintColor(MessageTintColor);

        float current_spline_render_lerp = 0.0f;

        for (size_t i = 0; i < playback_line_count; ++i) {
            ImGui::SetCursorScreenPos(render_rect.Min);

            const ImVec2 spline_translation = {(285.0f - i * 7.0f) * render_factor,
                                               (24.0f + i * 23.0f) * render_factor};

            if (m_talk_line_image) {
                const auto [image_width, image_height] = m_talk_line_image->size();
                const ImVec2 render_size               = ImVec2(
                    220.0f * render_factor, image_height * (220.0f / image_width) * render_factor);
                message_painter.render(*m_talk_line_image,
                                       render_rect.Min + spline_translation +
                                           ImVec2(-5.0f, -24.0f) * render_factor,
                                       render_size);
            }

            ImGui::SetCursorScreenPos(render_rect.Min);
            m_talk_spline->setTranslate(spline_translation.x, spline_translation.y);
            m_talk_spline->setRotate(IM_PI * 0.10f);
            m_talk_spline->setScale(197.0f * render_factor);

            std::string_view line_text = message_lines[render_line_base + i];

            const bool render_cropped_line = i == m_message_back_playback_index &&
                                             m_character_playback_index < line_text.size() - 1;
            if (render_cropped_line) {
                line_text = line_text.substr(0, m_character_playback_index);
            }

            current_spline_render_lerp = renderLine(line_text);

            if (render_cropped_line) {
                style.Colors[ImGuiCol_Text].w = 1.0f * m_character_fade_lerp;

                std::string_view char_text =
                    message_lines[render_line_base + i].substr(m_character_playback_index, 1);
                current_spline_render_lerp = ImGui::TextSplineUnformattedEx(
                    current_spline_render_lerp, m_talk_spline.get(), char_text.data(),
                    char_text.data() + char_text.size());

                style.Colors[ImGuiCol_Text].w = 1.0f;
            }
        }

        const bool render_talk_button = m_message_back_playback_index >= render_line_count;

        bool next_page_requested = false;

        if (m_talk_button_image && render_talk_button) {
            ImVec4 button_tint_color = MessageTintColor;
            button_tint_color.w *= m_button_playback_fade_lerp;

            ImagePainter button_painter;
            button_painter.setTintColor(button_tint_color);

            constexpr ImVec2 ButtonPosBase = ImVec2(476.0f, 100.0f);
            const ImVec2 button_pos =
                render_rect.Min + (ButtonPosBase * render_factor) +
                ImVec2((render_line_count - 1) * -7.0f, (render_line_count - 1) * 23.0f) *
                    render_factor;
            const ImVec2 button_size = ImVec2(20.0f * render_factor, 20.0f * render_factor);
            const ImRect button_bb   = {button_pos, button_pos + button_size};
            const ImGuiID button_id  = ImGui::GetID("##NPCEditorViewButton");
            ImGui::ItemSize(button_bb);
            if (!ImGui::ItemAdd(button_bb, button_id)) {
                TOOLBOX_ERROR("[BMG_WINDOW] Failed to register hitbox for message view button");
            }

            button_painter.render(*m_talk_button_image, button_pos, button_size);

            next_page_requested = ImGui::IsItemClicked();

            const f32 button_icon_alpha =
                m_button_fader_spline->getInterpolatedPoint(m_button_fader_lerp).m_y *
                m_button_playback_fade_lerp;
            m_button_fader_lerp =
                Wrap<float>(m_button_fader_lerp + delta_time * 0.8f, 0.0f, 1.0f);  // 50bpm
            m_button_playback_fade_lerp =
                ImClamp<float>(m_button_playback_fade_lerp + delta_time * 5.0f, 0.0f, 1.0f);

            button_painter.setTintColor({1.0f, 1.0f, 1.0f, button_icon_alpha});
            if (is_last_page) {
                button_painter.render(*m_talk_exit_image, button_pos + button_size * 0.175f,
                                      button_size * 0.65f);
            } else {
                const ImVec2 arrow_ofs = ImVec2(-1.0f, 8.5f) * render_factor;
                button_painter.setRotation(IM_PI * -0.29f, {0.0f, 0.0f});
                button_painter.render(*m_talk_arrow_image,
                                      button_pos + arrow_ofs + button_size * 0.175f,
                                      button_size * 0.65f);
            }
        }

        ImGui::GetStyle().Colors[ImGuiCol_Text] = text_color_backup;

        ImGui::PopFont();

        if (m_message_back_playback_index < render_line_count) {
            m_character_playback_lerp += delta_time * m_character_playback_speed;
            m_character_fade_lerp =
                ImClamp<float>(m_character_fade_lerp + delta_time * 30.0f, 0.0f, 1.0f);

            if (m_character_playback_lerp >= 1.0f) {
                m_character_playback_index += 1;  // Move to next character

                // If we have reached the end of the current line, move to the next line
                // and reset the character index
                if (m_character_playback_index >=
                    message_lines[render_line_base + m_message_back_playback_index].size()) {
                    m_message_back_playback_index += 1;
                    m_character_playback_index = 0;
                }

                // Reset character fade-in state
                m_character_playback_lerp -= 1.0f;
                m_character_fade_lerp = 0.0f;
            }
        }

        if (m_message_back_playback_index < render_line_count) {
            std::string_view playback_line =
                message_lines[render_line_base + m_message_back_playback_index];

            const bool is_command_start = m_character_playback_index < playback_line.size() - 1 &&
                                          playback_line.at(m_character_playback_index) == '{' &&
                                          playback_line.at(m_character_playback_index + 1) == '{';
            if (is_command_start) {
                size_t r_pos = playback_line.find("}}", m_character_playback_index);
                if (r_pos != std::string_view::npos) {
                    m_character_playback_index = r_pos + 2;
                }
            }
        }

        if (next_page_requested) {
            resetPlaybackStateForPage();
            m_current_page = (m_current_page + 1) % page_count;
            if (m_current_page == 0) {
                stopPlayback();
            }
        }
    }

    float BMGEditorViewNPC::renderLine(std::string_view line) {
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

            const bool is_command_start = current_pos < line.size() - 1 &&
                                          line.at(current_pos) == '{' &&
                                          line.at(current_pos + 1) == '{';
            if (is_command_start) {
                size_t r_pos = line.find("}}", current_pos);
                if (r_pos != std::string_view::npos) {
                    std::string_view command = line.substr(current_pos, r_pos - current_pos + 2);
                    spline_lerp              = renderCommand(spline_lerp, command);
                    current_pos              = r_pos + 2;
                    continue;
                }

                std::string_view trailing = line.substr(current_pos);
                spline_lerp = ImGui::TextSplineUnformattedEx(spline_lerp, m_talk_spline.get(),
                                                             trailing.data(),
                                                             trailing.data() + trailing.size());
                break;
            }

            end_pos = line.find("{{", current_pos);
            if (end_pos == std::string_view::npos) {
                std::string_view trailing = line.substr(current_pos);
                spline_lerp = ImGui::TextSplineUnformattedEx(spline_lerp, m_talk_spline.get(),
                                                             trailing.data(),
                                                             trailing.data() + trailing.size());
                break;
            }

            std::string_view slice = line.substr(current_pos, end_pos - current_pos);
            spline_lerp = ImGui::TextSplineUnformattedEx(spline_lerp, m_talk_spline.get(),
                                                         slice.data(), slice.data() + slice.size());
            current_pos = end_pos;
        }

        return spline_lerp;
    }

    float BMGEditorViewNPC::renderCommand(float spline_lerp, std::string_view command) {
        std::string_view proc_command = command.substr(1, command.size() - 2);

        if (proc_command.starts_with("{speed:")) {
            if (proc_command.size() <= 7) {
                TOOLBOX_WARN("[BMG_PREVIEW] Received incomplete speed command!");
                return spline_lerp;
            }

            std::string_view speed_val = proc_command.substr(7, proc_command.size() - 7 - 1);
            u32 speed                  = StringToTypedIntegral<u32>(speed_val).value_or(0);

            if (m_is_playback) {
                m_character_playback_speed = GetDeltaScalarFromSpeedValue(speed);
            }

            return spline_lerp;
        }

        if (proc_command.starts_with("{option:")) {
            if (proc_command.size() <= 10) {
                TOOLBOX_WARN("[BMG_PREVIEW] Received incomplete option command!");
                return spline_lerp;
            }

            size_t command_split     = proc_command.rfind(':');
            std::string_view message = proc_command.substr(
                command_split + 1, proc_command.size() - (command_split + 1) - 1);

            u8 option =
                StringToTypedIntegral<u8>(proc_command.substr(9, command_split - 9)).value_or(0);
            if (option > 1) {
                return spline_lerp;
            }

            m_talk_options[option] = std::string(message);

            return spline_lerp;
        }

        if (proc_command.starts_with("{ctx:")) {
            std::string_view ctx_val  = proc_command.substr(5, proc_command.size() - 5 - 1);
            size_t ctx_idx            = StringToTypedIntegral<size_t>(ctx_val).value_or(0);
            std::string_view fruit_id = BMGEditorWindow::GetFruitIDFromIndex(ctx_idx);
            return ImGui::TextSplineUnformattedEx(spline_lerp, m_talk_spline.get(), fruit_id.data(),
                                                  fruit_id.data() + fruit_id.size());
        }

        if (proc_command.starts_with("{color:")) {
            constexpr std::array<std::string_view, 6> ArrayColorNameToIdx = {
                "white", "white_alt", "red", "yellow", "blue", "green"};
            std::string_view color_val = proc_command.substr(7, proc_command.size() - 7 - 1);

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

        if (proc_command.starts_with("{record:")) {
            std::string_view record_val = proc_command.substr(8, proc_command.size() - 8 - 1);
            size_t record_idx           = StringToTypedIntegral<size_t>(record_val).value_or(0);
            std::string_view record     = BMGEditorWindow::GetSampleRecordFromIndex(record_idx);
            return ImGui::TextSplineUnformattedEx(spline_lerp, m_talk_spline.get(), record.data(),
                                                  record.data() + record.size());
        }

        return ImGui::TextSplineUnformattedEx(spline_lerp, m_talk_spline.get(), command.data(),
                                              command.data() + command.size());
    }

    void BMGEditorViewNPC::resetPlaybackStateForPage() {
        m_character_playback_speed = GetDeltaScalarFromSpeedValue(3);

        m_fader_playback_lerp         = 0.0f;
        m_character_fade_lerp         = 0.0f;
        m_character_playback_lerp     = 0.0f;
        m_character_playback_index    = 0;
        m_message_back_playback_index = 0;
        m_button_playback_fade_lerp   = 0.0f;
    }

    void BMGEditorViewNPC::_enableFadeShaderCallback(const ImDrawList *parent_list,
                                                     const ImDrawCmd *cmd) {
        float progress = *(float *)cmd->UserCallbackData;

        glUseProgram(m_back_shader_id);

        GLint prog_loc = glGetUniformLocation(m_back_shader_id, "u_progress");
        glUniform1f(prog_loc, progress);

        GLint soft_loc = glGetUniformLocation(m_back_shader_id, "u_softness");
        glUniform1f(soft_loc, 0.15f);
    }

    void BMGEditorViewNPC::_disableFadeShaderCallback(const ImDrawList *parent_list,
                                                      const ImDrawCmd *cmd) {
        glUseProgram(m_default_shader_id);
    }

}  // namespace Toolbox::UI