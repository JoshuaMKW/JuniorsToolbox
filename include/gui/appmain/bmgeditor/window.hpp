#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <queue>
#include <string>
#include <string_view>
#include <vector>

#include "core/memory.hpp"
#include "fsystem.hpp"
#include "objlib/object.hpp"
#include "objlib/template.hpp"
#include "scene/scene.hpp"
#include "smart_resource.hpp"

#include "bmg/bmg.hpp"
#include "core/clipboard.hpp"
#include "game/task.hpp"
#include "gui/appmain/project/asset.hpp"
#include "gui/appmain/project/rarc_processor.hpp"
#include "gui/context_menu.hpp"
#include "gui/event/event.hpp"
#include "gui/image/imagepainter.hpp"
#include "gui/selection.hpp"
#include "gui/window.hpp"

#include "model/bmgmodel.hpp"

#include <imgui.h>

namespace Toolbox::UI {

    class IBMGEditorView {
    public:
        virtual ~IBMGEditorView() = default;

        virtual size_t getPageCount(const BMG::MessageData::Entry &message) const = 0;

        virtual bool render(const ImRect &render_rect, const BMG::MessageData::Entry &message,
                            size_t current_page) = 0;
    };

    enum class EditorViewStyle {
        STYLE_NPC,
        STYLE_BOARD,
        STYLE_DEBS,
    };

    class BMGEditorViewFactory {
    public:
        static ScopePtr<IBMGEditorView> create(BMG::MessageFlagSize flag_size,
                                               EditorViewStyle style);
    };

    class BMGEditorWindow final : public ImWindow {
    public:
        BMGEditorWindow(const std::string &name);
        ~BMGEditorWindow() = default;

    public:
        static int GetExTextPaddingFromChar(char character);
        static int GetExTextPaddingFromChar(uint8_t character);
        static ImVec4 GetTextColorFromIndex(size_t index);

        static std::string_view GetFruitIDFromIndex(size_t index);
        static std::string_view GetSampleRecordFromIndex(size_t index);

    protected:
        void onRenderMenuBar() override;
        void onRenderBody(TimeStep delta_time) override;

        void renderMessagesList();

        void renderMessageEditorGroup();
        void renderMessageEditorMetadata();
        void renderMessageEditorTextbox();
        void renderMessageEditorPreview();

        float getMessageListMinWidth() const;
        float getEditorGroupMinWidth() const;
        float getEditorTextMinWidth() const;
        float getEditorPreviewMinWidth() const;

        float getMessageListMaxWidth() const;
        float getEditorGroupMaxWidth() const;
        float getEditorTextMaxWidth() const;
        float getEditorPreviewMaxWidth() const;

        void loadCurrentBackdropSelection();

        void onViewChanged(const ModelIndex &index);

    public:
        ImGuiWindowFlags flags() const override {
            return ImWindow::flags() | ImGuiWindowFlags_MenuBar;
        }

        const ImGuiWindowClass *windowClass() const override {
            if (parent() && parent()->windowClass()) {
                return parent()->windowClass();
            }

            ImGuiWindow *currentWindow           = ImGui::GetCurrentWindow();
            m_window_class.ClassId               = (ImGuiID)getUUID();
            m_window_class.ParentViewportId      = currentWindow->ViewportId;
            m_window_class.DockingAllowUnclassed = true;
            m_window_class.DockingAlwaysTabBar   = false;
            return nullptr;
        }

        std::optional<ImVec2> minSize() const override {
            return {
                {800, 600}
            };
        }

        [[nodiscard]] std::string context() const override {
            if (m_io_context_path.empty())
                return "(unknown)";
            return m_io_context_path.string();
        }

        [[nodiscard]] bool unsaved() const override { return false; }

        // Returns the supported file types, empty string is designed for a folder.
        [[nodiscard]] std::vector<std::string> extensions() const override { return {}; }

        [[nodiscard]] bool onLoadData(const std::filesystem::path &path) override;
        [[nodiscard]] bool onSaveData(std::optional<std::filesystem::path> path) override;

        void onAttach() override;
        void onDetach() override;
        void onImGuiUpdate(TimeStep delta_time) override;
        void onContextMenuEvent(RefPtr<ContextMenuEvent> ev) override;
        void onDragEvent(RefPtr<DragEvent> ev) override;
        void onDropEvent(RefPtr<DropEvent> ev) override;
        void onEvent(RefPtr<BaseEvent> ev) override;

        void buildContextMenu();

    private:
        RefPtr<BMGModel> m_bmg_model;

        ModelSelectionManager m_selection_mgr;
        ScopePtr<ModelHistoryHandler> m_history_handler;

        ImagePainter m_backdrop_painter;
        RefPtr<const ImageHandle> m_current_backdrop;
        UUID64 m_backdrop_path_uuid;

        ScopePtr<IBMGEditorView> m_editor_view;
        size_t m_current_page = 0;

        ContextMenu<ModelIndex> m_list_context_menu;
        ContextMenu<ModelIndex> m_text_context_menu;

        ModelIndex m_view_index;
        std::vector<ModelIndex> m_cut_indices;

        bool m_is_renaming = false;
        char m_rename_buffer[128];
        bool m_is_valid_name = true;

        bool m_did_drag_drop = false;

        ImVec2 m_last_reg_mouse_pos;

        std::array<char, 128> m_search_buf;
        std::string m_search_str;

        std::array<char, 4096> m_textbox_buf;

        size_t m_selected_background     = 0;
        EditorViewStyle m_selected_style = EditorViewStyle::STYLE_NPC;

        float m_list_width    = 0.0f;
        float m_editor_width  = 0.0f;
        float m_text_width    = 0.0f;
        float m_preview_width = 0.0f;

        fs_path m_io_context_path;
        bool m_is_save_default_ready = false;
        bool m_is_save_as_dialog_open = false;
    };

}  // namespace Toolbox::UI