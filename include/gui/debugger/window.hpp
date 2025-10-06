#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/memory.hpp"
#include "fsystem.hpp"
#include "objlib/object.hpp"
#include "objlib/template.hpp"
#include "scene/scene.hpp"
#include "smart_resource.hpp"

#include "core/clipboard.hpp"
#include "dolphin/watch.hpp"
#include "game/task.hpp"
#include "gui/context_menu.hpp"
#include "gui/debugger/dialog.hpp"
#include "gui/event/event.hpp"
#include "gui/image/imagepainter.hpp"
#include "gui/project/asset.hpp"
#include "gui/window.hpp"
#include "model/selection.hpp"
#include "model/watchmodel.hpp"

#include <imgui.h>

namespace Toolbox::UI {

    struct WatchGroup {
        std::string m_name;
        std::vector<MetaWatch> m_meta_watches;
        std::vector<MemoryWatch> m_byte_watches;
        bool m_locked = false;
    };

    class DebuggerWindow final : public ImWindow {
    public:
        DebuggerWindow(const std::string &name);
        ~DebuggerWindow() = default;

    protected:
        void onRenderMenuBar() override;
        void onRenderBody(TimeStep delta_time) override;

        void renderMemoryAddressBar();
        void renderMemoryView();
        u32 renderMemoryRow(void *handle, u32 base_address, u32 byte_limit, u8 column_count,
                            u8 byte_width);
        void renderMemoryWatchList();
        void renderMemoryWatch(const ModelIndex &index, int depth, float table_start_x,
                               float table_width);
        void renderWatchGroup(const ModelIndex &index, int depth, float table_start_x,
                              float table_width);

    public:
        ImGuiWindowFlags flags() const override { return ImWindow::flags(); }

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
                {800, 400}
            };
        }

        [[nodiscard]] std::string context() const override;

        [[nodiscard]] bool unsaved() const override { return false; }

        // Returns the supported file types, empty string is designed for a folder.
        [[nodiscard]] std::vector<std::string> extensions() const override { return {}; }

        [[nodiscard]] bool onLoadData(const std::filesystem::path &path) override { return true; }

        [[nodiscard]] bool onSaveData(std::optional<std::filesystem::path> path) override {
            return true;
        }

        void onAttach() override;
        void onDetach() override;
        void onImGuiUpdate(TimeStep delta_time) override;
        void onContextMenuEvent(RefPtr<ContextMenuEvent> ev) override;
        void onDragEvent(RefPtr<DragEvent> ev) override;
        void onDropEvent(RefPtr<DropEvent> ev) override;

        // Selection actions
        void actionDeleteIndexes(std::vector<ModelIndex> &indices);
        void actionOpenIndexes(const std::vector<ModelIndex> &indices);
        void actionRenameIndex(const ModelIndex &index);
        void actionPasteIntoIndex(const ModelIndex &index, const std::vector<fs_path> &data);
        void actionCopyIndexes(const std::vector<ModelIndex> &indices);

        void actionSelectIndex(const ModelIndex &child_index);
        void actionClearRequestExcIndex(const ModelIndex &child_index, bool is_left_button);

    private:
        struct HistoryPair {
            u32 m_address;
            std::string m_label;
        };

        void buildContextMenus();

        void recursiveLock(ModelIndex src_idx, bool lock);

        ModelIndex insertGroup(ModelIndex group_index, size_t row,
                               AddGroupDialog::InsertPolicy policy, std::string_view group_name);
        ModelIndex insertWatch(ModelIndex group_index, size_t row,
                               AddWatchDialog::InsertPolicy policy, std::string_view watch_name,
                               MetaType watch_type, u32 watch_address, u32 watch_size);

        std::string buildQualifiedId(ModelIndex index) const;

        // ----
        void renderPreview(f32 column_width, const MetaValue &value);
        void renderPreviewSingle(f32 column_width, const MetaValue &value);
        void renderPreviewRGBA(f32 column_width, const MetaValue &value);
        void renderPreviewRGB(f32 column_width, const MetaValue &value);
        void renderPreviewVec3(f32 column_width, const MetaValue &value);
        void renderPreviewTransform(f32 column_width, const MetaValue &value);
        void renderPreviewMatrix34(f32 column_width, const MetaValue &value);
        void calcPreview(char *preview_out, size_t preview_size, const MetaValue &value) const;
        Color::RGBShader calcColorRGB(const MetaValue &value);
        Color::RGBAShader calcColorRGBA(const MetaValue &value);
        // ----

        void overwriteNibbleAtCursor(u8 nibble_value);
        void overwriteCharAtCursor(char char_value);
        void processKeyInputsAtAddress(int32_t column_count);

        using transformer_t = std::function<u8(u8)>;
        static void CopyBytesFromAddressSpan(const AddressSpan &span);
        static void CopyASCIIFromAddressSpan(const AddressSpan &span);
        static void FillAddressSpan(const AddressSpan &span, u8 initial_val,
                                    transformer_t transformer);

        UUID64 m_attached_scene_uuid = 0;

        u32 m_base_address = 0x80000000;
        u8 m_byte_width    = 1;

        bool m_initialized_splitter_widths = false;
        float m_list_width                 = 0.0f;
        float m_view_width                 = 0.0f;

        std::array<char, 32> m_address_input;
        std::vector<HistoryPair> m_address_search_history = {};

        size_t m_column_count_idx = 0;
        size_t m_byte_width_idx   = 0;

        std::unordered_map<std::string, ImageHandle> m_icon_map;
        ImagePainter m_icon_painter;

        ContextMenu<AddressSpan> m_ascii_view_context_menu;
        ContextMenu<AddressSpan> m_byte_view_context_menu;
        ContextMenu<ModelIndex> m_watch_view_context_menu;

        AddGroupDialog m_add_group_dialog;
        AddWatchDialog m_add_watch_dialog;
        FillBytesDialog m_fill_bytes_dialog;

        RefPtr<WatchDataModel> m_watch_model;
        RefPtr<WatchDataModelSortFilterProxy> m_proxy_model;

        ModelIndex m_last_selected_index;
        ModelSelectionState m_selection;
        ModelSelectionState m_selection_ctx;

        bool m_did_drag_drop;
        bool m_any_row_clicked;

        ImVec2 m_last_reg_mouse_pos;

        std::unordered_map<ImGuiID, bool> m_node_open_state;

        bool m_selection_was_ascii             = false;
        bool m_address_selection_new           = false;
        ImVec2 m_address_selection_mouse_start = {};
        u32 m_address_selection_begin          = 0;
        u32 m_address_selection_begin_nibble   = 0;
        u32 m_address_selection_end            = 0;
        u32 m_address_selection_end_nibble     = 0;
        u32 m_address_cursor                   = 0;
        u8 m_address_cursor_nibble             = 0;

        float m_cursor_step_timer = -0.3f;
        float m_cursor_anim_timer = 0.0f;
        float m_delta_time        = 0.0f;
    };

}  // namespace Toolbox::UI