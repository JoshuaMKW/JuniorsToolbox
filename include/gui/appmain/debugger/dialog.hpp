#pragma once

#include <array>
#include <functional>
#include <imgui.h>
#include <vector>

#include "core/error.hpp"
#include "core/memory.hpp"
#include "gui/appmain/scene/nodeinfo.hpp"
#include "model/model.hpp"
#include "objlib/template.hpp"

namespace Toolbox::UI {

    struct AddressSpan {
        u32 m_begin;
        u32 m_end;
    };

    class AddGroupDialog {
    public:
        enum class InsertPolicy {
            INSERT_BEFORE,
            INSERT_AFTER,
            INSERT_CHILD,
        };

        using action_t = std::function<void(ModelIndex, size_t, InsertPolicy, std::string_view)>;
        using cancel_t = std::function<void(ModelIndex)>;
        using filter_t = std::function<bool(std::string_view, ModelIndex group_idx)>;

        AddGroupDialog()  = default;
        ~AddGroupDialog() = default;

        void setInsertPolicy(InsertPolicy policy) { m_insert_policy = policy; }
        void setActionOnAccept(action_t on_accept) { m_on_accept = on_accept; }
        void setActionOnReject(cancel_t on_reject) { m_on_reject = on_reject; }

        void setFilterPredicate(filter_t filter) { m_filter_predicate = std::move(filter); }

        void setup();

        void open() { m_opening = true; }
        bool is_open() const { return m_open == true || m_opening == true; }

        void render(ModelIndex group_idx, size_t row);

    private:
        bool m_open    = false;
        bool m_opening = false;

        std::array<char, 128> m_group_name = {};

        InsertPolicy m_insert_policy = InsertPolicy::INSERT_BEFORE;

        action_t m_on_accept;
        cancel_t m_on_reject;
        filter_t m_filter_predicate;
    };

    class AddWatchDialog {
    public:
        enum class InsertPolicy {
            INSERT_BEFORE,
            INSERT_AFTER,
            INSERT_CHILD,
        };

        using action_t = std::function<void(ModelIndex, size_t, InsertPolicy, std::string_view,
                                            MetaType, const std::vector<u32> &, u32, bool)>;
        using cancel_t = std::function<void(ModelIndex)>;
        using filter_t = std::function<bool(std::string_view, ModelIndex group_idx)>;

        AddWatchDialog()  = default;
        ~AddWatchDialog() = default;

        void setInsertPolicy(InsertPolicy policy) { m_insert_policy = policy; }
        void setActionOnAccept(action_t on_accept) { m_on_accept = on_accept; }
        void setActionOnReject(cancel_t on_reject) { m_on_reject = on_reject; }

        void setFilterPredicate(filter_t filter) { m_filter_predicate = std::move(filter); }

        void setup();

        void open() {
            setup();
            m_opening = true;
        }
        bool is_open() const { return m_open == true || m_opening == true; }

        void render(ModelIndex group_idx, size_t row);

        void openToAddress(u32 address);
        void openToAddressAsType(u32 address, MetaType type, size_t address_size);
        void openToAddressAsBytes(u32 address, size_t address_size);

    protected:
        void renderPreview(f32 label_width, u32 address, size_t address_size);
        void renderPreviewSingle(f32 label_width, u32 address, size_t address_size);
        void renderPreviewRGBA(f32 label_width, u32 address);
        void renderPreviewRGB(f32 label_width, u32 address);
        void renderPreviewVec3(f32 label_width, u32 address);
        void renderPreviewTransform(f32 label_width, u32 address);
        void renderPreviewMatrix34(f32 label_width, u32 address);
        void calcPreview(char *preview_out, size_t preview_size, u32 address, size_t address_size,
                         MetaType address_type) const;
        Color::RGBShader calcColorRGB(u32 address);
        Color::RGBAShader calcColorRGBA(u32 address);

    private:
        bool m_open    = false;
        bool m_opening = false;

        std::array<char, 128> m_watch_name = {};

        std::array<char, 32> m_watch_p_chain[8] = {};
        size_t m_watch_p_chain_size             = 2;
        bool m_watch_is_pointer                 = false;

        MetaType m_watch_type = MetaType::U8;
        // std::array<char, 16> m_watch_size = {};
        size_t m_watch_size = 0;

        InsertPolicy m_insert_policy = InsertPolicy::INSERT_BEFORE;

        action_t m_on_accept;
        cancel_t m_on_reject;
        filter_t m_filter_predicate;
    };

    class FillBytesDialog {
    public:
        enum class InsertPolicy {
            INSERT_CONSTANT,
            INSERT_INCREMENT,
            INSERT_DECREMENT,
        };

        using action_t = std::function<void(const AddressSpan &, InsertPolicy, u8)>;
        using cancel_t = std::function<void(const AddressSpan &)>;

        FillBytesDialog()  = default;
        ~FillBytesDialog() = default;

        void setInsertPolicy(InsertPolicy policy) { m_insert_policy = policy; }
        void setActionOnAccept(action_t on_accept) { m_on_accept = on_accept; }
        void setActionOnReject(cancel_t on_reject) { m_on_reject = on_reject; }

        void setup();

        void open() { m_opening = true; }
        bool is_open() const { return m_open == true || m_opening == true; }

        void render(const AddressSpan &span);

    private:
        bool m_open    = false;
        bool m_opening = false;

        int m_byte_value;

        InsertPolicy m_insert_policy = InsertPolicy::INSERT_CONSTANT;

        action_t m_on_accept;
        cancel_t m_on_reject;
    };

}  // namespace Toolbox::UI