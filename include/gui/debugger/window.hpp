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
#include "model/memscanmodel.hpp"
#include "model/selection.hpp"
#include "model/watchmodel.hpp"

#include <imgui.h>

#define SCAN_INPUT_MAX_LEN 1024

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

        void renderMemoryScanner();

        void renderMemoryWatchList();
        void renderMemoryWatch(const ModelIndex &index, int depth, float table_start_x,
                               float table_width, bool table_focused, bool table_hovered);
        void renderWatchGroup(const ModelIndex &index, int depth, float table_start_x,
                              float table_width, bool table_focused, bool table_hovered);

        void countMemoryWatch(const ModelIndex &index, int *row);
        void countWatchGroup(const ModelIndex &index, int *row);
        std::vector<ModelIndex>
        computeModelWatchFlatTree(const std::unordered_map<UUID64, bool> &open_state) const;

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
                {1400, 600}
            };
        }

        [[nodiscard]] std::string context() const override;

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

    private:
        enum class ScanRadix {
            RADIX_BINARY,
            RADIX_OCTAL,
            RADIX_DECIMAL,
            RADIX_HEXADECIMAL,
        };

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
                               MetaType watch_type, const std::vector<u32> &pointer_chain,
                               u32 watch_size, bool is_pointer);

        ModelIndex createWatchGroupFromScanSelection();
        ModelIndex createWatchGroupFromScanAll();
        void removeScanSelection();

        std::string buildQualifiedId(ModelIndex index) const;

        // ----
        void renderPreview(f32 column_width, const MetaValue &value, WatchValueBase value_base);
        void renderPreviewSingle(f32 column_width, const MetaValue &value,
                                 WatchValueBase value_base);
        void renderPreviewRGBA(f32 column_width, const MetaValue &value);
        void renderPreviewRGB(f32 column_width, const MetaValue &value);
        void renderPreviewVec3(f32 column_width, const MetaValue &value);
        void renderPreviewTransform(f32 column_width, const MetaValue &value);
        void renderPreviewMatrix34(f32 column_width, const MetaValue &value);
        std::string calcPreview(const MetaValue &value,
                                WatchValueBase value_base = WatchValueBase::BASE_DECIMAL) const;
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

        bool m_initialized_splitters = false;
        float m_scan_height          = 0.0f;
        float m_list_height          = 0.0f;
        float m_list_width           = 0.0f;
        float m_view_width           = 0.0f;

        std::array<char, 32> m_scan_begin_input;
        std::array<char, 32> m_scan_end_input;
        std::array<char, SCAN_INPUT_MAX_LEN> m_scan_value_input_a;
        std::array<char, SCAN_INPUT_MAX_LEN> m_scan_value_input_b;
        bool m_scan_enforce_alignment = true;

        MetaType m_scan_type                       = MetaType::BOOL;
        MemScanModel::ScanOperator m_scan_operator = MemScanModel::ScanOperator::OP_EXACT;
        ScanRadix m_scan_radix                     = ScanRadix::RADIX_DECIMAL;

        std::array<char, 32> m_address_input;
        std::vector<HistoryPair> m_address_search_history = {};

        size_t m_column_count_idx = 0;
        size_t m_byte_width_idx   = 0;

        std::unordered_map<std::string, ImageHandle> m_icon_map;
        ImagePainter m_icon_painter;

        ContextMenu<AddressSpan> m_ascii_view_context_menu;
        ContextMenu<AddressSpan> m_byte_view_context_menu;

        ContextMenu<ModelIndex> m_watch_view_context_menu;
        ContextMenu<ModelIndex> m_group_view_context_menu;

        ContextMenu<ModelIndex> m_scan_view_context_menu;

        AddGroupDialog m_add_group_dialog;
        AddWatchDialog m_add_watch_dialog;
        FillBytesDialog m_fill_bytes_dialog;

        RefPtr<WatchDataModel> m_watch_model;
        RefPtr<WatchDataModelSortFilterProxy> m_watch_proxy_model;
        ModelSelectionManager m_watch_selection_mgr;

        RefPtr<MemScanModel> m_scan_model;
        ModelSelectionManager m_scan_selection_mgr;

        bool m_scan_active = false;

        bool m_did_drag_drop   = false;
        bool m_any_row_clicked = false;

        ImVec2 m_last_reg_mouse_pos = {};

        std::unordered_map<UUID64, bool> m_watch_node_open_state;

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

        bool m_keybind_wait_for_keyup = false;

        std::optional<fs_path> m_resource_path;
        bool m_is_open_dialog     = false;
        bool m_is_save_dialog     = false;
        bool m_is_load_dme_dialog = false;

        bool m_error_modal_open       = false;
        std::string m_error_modal_msg = "";
    };

}  // namespace Toolbox::UI