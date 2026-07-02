#pragma once

#include <string_view>

#include "bmg/bmg.hpp"

#include "core/mathutil.hpp"
#include "gui/appmain/application.hpp"
#include "gui/appmain/bmgeditor/window.hpp"
#include "gui/logging/errors.hpp"
#include "spline.hpp"

namespace Toolbox::UI {

    class BMGEditorViewNPC final : public IBMGEditorView {
    public:
        BMGEditorViewNPC(bool is_mirrored);
        ~BMGEditorViewNPC() override = default;

        bool canPlayback() const override;
        bool isPlayback() const override;
        bool startPlayback() override;
        bool stopPlayback() override;

        bool render(TimeStep delta_time, const ImRect &render_rect,
                    const BMG::MessageData::Entry &message);

    protected:
        void renderForPaused(TimeStep delta_time, const ImRect &render_rect,
                             const BMG::MessageData::Entry &message);
        void renderForPlayback(TimeStep delta_time, const ImRect &render_rect,
                               const BMG::MessageData::Entry &message);

        float renderLine(std::string_view line);

        float renderCommand(float spline_lerp, std::string_view command);

        void resetPlaybackStateForPage();
        void _enableFadeShaderCallback(const ImDrawList *parent_list, const ImDrawCmd *cmd);
        void _disableFadeShaderCallback(const ImDrawList *parent_list, const ImDrawCmd *cmd);

    private:
        ScopePtr<CatmullSpline2D> m_talk_spline;
        std::array<std::string, 2> m_talk_options;

        RefPtr<const ImageHandle> m_talk_line_image;
        RefPtr<const ImageHandle> m_talk_button_image;
        RefPtr<const ImageHandle> m_talk_arrow_image;
        RefPtr<const ImageHandle> m_talk_exit_image;
        RefPtr<const ImageHandle> m_talk_option_image;

        ScopePtr<CatmullSpline2D> m_button_fader_spline;
        float m_button_fader_lerp = 0.0f;

        GLuint m_back_shader_id    = 0;
        GLuint m_default_shader_id = 0;

        bool m_is_playback                     = false;
        float m_fader_playback_lerp            = 0.0f;
        float m_character_fade_lerp            = 0.0f;
        float m_character_playback_lerp        = 0.0f;
        float m_character_playback_speed       = 0.0f;
        uint32_t m_character_playback_index    = 0;
        uint32_t m_message_back_playback_index = 0;
        float m_button_playback_fade_lerp      = 0.0f;

        size_t m_current_page = 0;
    };

    class BMGEditorViewBillboard final : public IBMGEditorView {
    public:
        ~BMGEditorViewBillboard() override = default;

        bool canPlayback() const override;
        bool isPlayback() const override;
        bool startPlayback() override;
        bool stopPlayback() override;

        bool render(TimeStep delta_time, const ImRect &render_rect,
                    const BMG::MessageData::Entry &message) override;

    private:
        bool m_is_playback = false;
    };

    class BMGEditorViewDEBS final : public IBMGEditorView {
    public:
        ~BMGEditorViewDEBS() override = default;

        bool canPlayback() const override;
        bool isPlayback() const override;
        bool startPlayback() override;
        bool stopPlayback() override;

        bool render(TimeStep delta_time, const ImRect &render_rect,
                    const BMG::MessageData::Entry &message);

    private:
        bool m_is_playback = false;
    };

    class BMGEditorViewShineSelect final : public IBMGEditorView {
    public:
        ~BMGEditorViewShineSelect() override = default;

        bool canPlayback() const override;
        bool isPlayback() const override;
        bool startPlayback() override;
        bool stopPlayback() override;

        bool render(TimeStep delta_time, const ImRect &render_rect,
                    const BMG::MessageData::Entry &message);

    private:
        bool m_is_playback = false;
    };

}  // namespace Toolbox::UI