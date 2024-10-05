#pragma once

#include "gui/dragdrop/dragdropmanager.hpp"
#include "core/clipboard.hpp"
#include "core/memory.hpp"
#include "core/mimedata/mimedata.hpp"
#include "gui/dragdrop/dragaction.hpp"
#include "gui/dragdrop/dropaction.hpp"

#ifdef TOOLBOX_PLATFORM_WINDOWS
#include <Windows.h>
#include <objidl.h>
#include <shlobj.h>
#include <shobjidl.h>
#elif defined(TOOLBOX_PLATFORM_LINUX)
#endif

static std::vector<std::string_view> splitLines(std::string_view s) {
    std::vector<std::string_view> result;
    size_t last_pos         = 0;
    size_t next_newline_pos = s.find('\n', 0);
    while (next_newline_pos != std::string::npos) {
        if (s[next_newline_pos - 1] == '\r') {
            result.push_back(s.substr(last_pos, next_newline_pos - last_pos - 1));
        } else {
            result.push_back(s.substr(last_pos, next_newline_pos - last_pos));
        }
        last_pos         = next_newline_pos + 1;
        next_newline_pos = s.find('\n', last_pos);
    }
    if (last_pos < s.size()) {
        if (s[s.size() - 1] == '\r') {
            result.push_back(s.substr(last_pos, s.size() - last_pos - 1));
        } else {
            result.push_back(s.substr(last_pos));
        }
    }
    return result;
}

namespace Toolbox::UI {

    RefPtr<DragAction> DragDropManager::createDragAction(UUID64 source_uuid, MimeData &&data) {
        m_current_drag_action = make_referable<DragAction>(source_uuid);
        m_current_drag_action->setPayload(data);
        return m_current_drag_action;
    }

    void DragDropManager::destroyDragAction(RefPtr<DragAction> action) {
        m_current_drag_action.reset();
    }

#ifdef TOOLBOX_PLATFORM_WINDOWS
    bool DragDropManager::initialize() { return OleInitialize(nullptr) >= 0; }

    void DragDropManager::shutdown() { OleUninitialize(); }

    Result<void, BaseError> DragDropManager::createSystemDragDropSource(MimeData &&data) {
        std::vector<std::string> formats =
            SystemClipboard::instance().getAvailableContentFormats().value_or(
                std::vector<std::string>());

        if (std::find(formats.begin(), formats.end(), "text/uri-list") == formats.end()) {
            return make_error<void>("DRAG_DROP", "No paths available in mimedata");
        }

        std::string paths = data.get_urls().value_or("");

        std::vector<PIDLIST_ABSOLUTE> pidls;
        std::vector<std::string_view> lines = splitLines(paths);

        size_t next_newline_pos = 0;
        for (const std::string_view &line : lines) {
            if (line.empty()) {
                continue;
            }

            std::wstring wpath = std::wstring(line.begin(), line.end());

            PIDLIST_ABSOLUTE pidl = ILCreateFromPathW(wpath.c_str());
            if (pidl == NULL) {
                for (PIDLIST_ABSOLUTE pidl : pidls) {
                    ILFree(pidl);
                }
                return make_error<void>("DRAG_DROP", "Failed to create PIDL from path");
            }

            pidls.push_back(pidl);
        }

        IDataObject *data_object = nullptr;
        {
            HRESULT hr = SHCreateDataObject(NULL, static_cast<UINT>(pidls.size()), (PCIDLIST_ABSOLUTE *)pidls.data(),
                                            nullptr, IID_IDataObject, (void **)&data_object);

            for (PIDLIST_ABSOLUTE pidl : pidls) {
                ILFree(pidl);
            }

            if (FAILED(hr)) {
                return make_error<void>("DRAG_DROP", "Failed to create data object");
            }
        }

        IDropSource *drop_source = nullptr;
        {
            HRESULT hr = CoCreateInstance(CLSID_DragDropHelper, nullptr, CLSCTX_INPROC_SERVER,
                                          IID_IDropSource, (void **)&drop_source);
            if (FAILED(hr)) {
                data_object->Release();
                return make_error<void>("DRAG_DROP", "Failed to create drop source");
            }
        }

        DWORD effect;
        {
            HRESULT hr = DoDragDrop(data_object, drop_source, DROPEFFECT_COPY, &effect);
            if (FAILED(hr)) {
                drop_source->Release();
                data_object->Release();
                return make_error<void>("DRAG_DROP", "Failed to perform drag drop operation");
            }
        }

        m_data_object = data_object;
        m_drop_source = drop_source;
        return {};
    }

    Result<void, BaseError> DragDropManager::destroySystemDragDropSource() {
        if (m_data_object) {
            m_data_object->Release();
        }
        if (m_drop_source) {
            m_drop_source->Release();
        }
        return {};
    }

#elif defined(TOOLBOX_PLATFORM_LINUX)

    bool DragDropManager::initialize() { return false; }

    void DragDropManager::shutdown() {}

    Result<void, BaseError> DragDropManager::createSystemDragDropSource(MimeData &&data) {
        return {};
    }

    Result<void, BaseError> DragDropManager::destroySystemDragDropSource() {
        return {};
    }

#endif

}  // namespace Toolbox::UI