#include "gcm.hpp"

#include <algorithm>
#include <string.h>
#include <fstream>

namespace GCM_NAMESPACE {

    Apploader::Apploader() {
        m_data.resize(0x20);
    }

    GCM_NODISCARD std::unique_ptr<Apploader> Apploader::FromData(ByteView _data) {
        GCM_RUNTIME_ASSERT(_data.size() >= 0x20, "Provided Apploader data is smaller than the Apploader metadata header!");

        const u32 loader_size = GCM_BIG_TO_SYSTEM_ENDIAN(*(u32*)(_data.data() + 0x14));
        const u32 trailer_size = GCM_BIG_TO_SYSTEM_ENDIAN(*(u32*)(_data.data() + 0x18));

        GCM_RUNTIME_ASSERT(_data.size() == loader_size + trailer_size + 0x20, "Provided Apploader data mismatches the metadata size markers!");

        std::unique_ptr<Apploader> new_apploader = std::make_unique<Apploader>();
        new_apploader->m_data.resize(_data.size());
        memcpy(new_apploader->m_data.data(), _data.data(), _data.size());

        return new_apploader;
    }

    GCM_NODISCARD std::unique_ptr<Apploader> Apploader::FromFile(const std::string& _data) {
        std::ifstream in_file(_data, std::ios::in | std::ios::binary);
        if (!in_file) {
            return nullptr;
        }

        in_file.seekg(0x14, std::ios::beg);

        u32 loader_size, trailer_size;
        in_file.read((char*)&loader_size, sizeof(u32));
        in_file.read((char*)&trailer_size, sizeof(u32));
        loader_size = GCM_BIG_TO_SYSTEM_ENDIAN(loader_size);
        trailer_size = GCM_BIG_TO_SYSTEM_ENDIAN(trailer_size);

        in_file.seekg(0, std::ios::end);
        GCM_RUNTIME_ASSERT(in_file.tellg() == loader_size + trailer_size + 0x20, "Provided Apploader data mismatches the metadata size markers!");

        in_file.seekg(0, std::ios::beg);
        std::unique_ptr<Apploader> new_apploader = std::make_unique<Apploader>();
        new_apploader->m_data.resize(loader_size + trailer_size + 0x20);
        in_file.read((char*)new_apploader->m_data.data(), new_apploader->m_data.size());

        return new_apploader;
    }

    GCM_NODISCARD bool Apploader::ToData(std::vector<u8>& _out) const {
        if GCM_UNLIKELY (!IsValid()) {
            return false;
        }

        _out = m_data;
        return true;
    }

    GCM_NODISCARD bool Apploader::ToFile(const std::string& _path) const {
        if GCM_UNLIKELY (!IsValid()) {
            return false;
        }

        std::ofstream out_file(_path, std::ios::out | std::ios::binary);
        if (!out_file) {
            return false;
        }

        out_file.write((const char*)m_data.data(), m_data.size());
        return true;
    }

    GCM_NODISCARD bool Apploader::IsValid() const GCM_NOEXCEPT {
        if GCM_UNLIKELY (m_data.size() < 0x20) {
            return false;
        }

        const u32 loader_size = GCM_BIG_TO_SYSTEM_ENDIAN(*(u32*)(m_data.data() + 0x14));
        const u32 trailer_size = GCM_BIG_TO_SYSTEM_ENDIAN(*(u32*)(m_data.data() + 0x18));

        return m_data.size() == loader_size + trailer_size + 0x20;
    }

    GCM_NODISCARD std::string Apploader::GetBuildDate() const GCM_NOEXCEPT {
        if GCM_UNLIKELY (!IsValid()) {
            return "";
        }

        return std::string((const char*)m_data.data(), 10);  // YYYY/MM/DD
    }

    GCM_NODISCARD u32 Apploader::GetEntryPoint() const GCM_NOEXCEPT {
        if GCM_UNLIKELY (!IsValid()) {
            return 0xFFFFFFFF;
        }

        return GCM_BIG_TO_SYSTEM_ENDIAN(*(u32*)(m_data.data() + 0x10));
    }

    GCM_NODISCARD u32 Apploader::GetLoaderSize() const GCM_NOEXCEPT {
        if GCM_UNLIKELY (!IsValid()) {
            return 0xFFFFFFFF;
        }

        return GCM_BIG_TO_SYSTEM_ENDIAN(*(u32*)(m_data.data() + 0x14));
    }

    GCM_NODISCARD u32 Apploader::GetTrailerSize() const GCM_NOEXCEPT {
        if GCM_UNLIKELY (!IsValid()) {
            return 0xFFFFFFFF;
        }

        return GCM_BIG_TO_SYSTEM_ENDIAN(*(u32*)(m_data.data() + 0x18));
    }

    GCM_NODISCARD ByteView Apploader::GetLoaderView() const GCM_NOEXCEPT {
        if GCM_UNLIKELY (!IsValid()) {
            return {};
        }

        const u32 loader_size = GetLoaderSize();
        return make_view(m_data.data() + 0x20, loader_size);
    }

    GCM_NODISCARD ByteView Apploader::GetTrailerView() const GCM_NOEXCEPT {
        if GCM_UNLIKELY (!IsValid()) {
            return {};
        }

        const u32 loader_size = GetLoaderSize();
        const u32 trailer_size = GetTrailerSize();
        return make_view(m_data.data() + loader_size + 0x20, trailer_size);
    }

    void Apploader::SetBuildDate(const std::string& _date) GCM_NOEXCEPT {
        GCM_RUNTIME_ASSERT(_date.length() == 10, "Apploader expects a date in the YYYY/MM/DD format!");
        memcpy(m_data.data(), _date.data(), 10);
    }

    void Apploader::SetEntryPoint(u32 _entrypoint) GCM_NOEXCEPT {
        GCM_RUNTIME_ASSERT(0x80003000 <= _entrypoint && _entrypoint < 0x81800000, "Apploader expects an entrypoint within the valid range [0x80003000, 0x81800000)!");
        *(u32*)(m_data.data() + 0x10) = GCM_SYSTEM_TO_BIG_ENDIAN(_entrypoint);
    }

    void Apploader::SetLoaderData(ByteView _newdata) GCM_NOEXCEPT {
        GCM_RUNTIME_ASSERT(_newdata.size() > 0 && _newdata.size() < 0x1800000, "Apploader data must fall within a valid size!");
        u32 new_size = RoundUp(static_cast<u32>(_newdata.size()), (u32)4);

        std::vector<u8> new_data;
        new_data.resize(new_size + GetTrailerSize() + 0x20);

        memcpy(new_data.data(), m_data.data(), 0x20);
        *(u32*)(new_data.data() + 0x14) = GCM_SYSTEM_TO_BIG_ENDIAN(new_size);

        memcpy(new_data.data() + 0x20, _newdata.data(), _newdata.size());
        memset(new_data.data() + _newdata.size() + 0x20, '\0', new_size - _newdata.size());

        ByteView trailer_view = GetTrailerView();
        memcpy(new_data.data() + new_size + 0x20, trailer_view.data(), trailer_view.size());

        m_data = std::move(new_data);
        *(u32*)(m_data.data() + 0x14) = GCM_SYSTEM_TO_BIG_ENDIAN(new_size);
    }

    void Apploader::SetTrailerData(ByteView _newdata) GCM_NOEXCEPT {
        GCM_RUNTIME_ASSERT(_newdata.size() > 0 && _newdata.size() < 0x1800000, "Apploader data must fall within a valid size!");
        u32 new_size = RoundUp(static_cast<u32>(_newdata.size()), (u32)4);

        std::vector<u8> new_data;
        new_data.resize(GetLoaderSize() + new_size + 0x20);

        memcpy(new_data.data(), m_data.data(), 0x20);
        *(u32*)(new_data.data() + 0x18) = GCM_SYSTEM_TO_BIG_ENDIAN(new_size);

        ByteView loader_view = GetLoaderView();
        memcpy(new_data.data() + 0x20, loader_view.data(), loader_view.size());

        memcpy(new_data.data() + loader_view.size() + 0x20, _newdata.data(), _newdata.size());
        memset(new_data.data() + _newdata.size() + 0x20, '\0', new_size - _newdata.size());

        m_data = std::move(new_data);
        *(u32*)(m_data.data() + 0x18) = GCM_SYSTEM_TO_BIG_ENDIAN(new_size);
    }

    DOLExecutable::DOLExecutable() {
        m_data.resize(0x100);
    }

    GCM_NODISCARD std::unique_ptr<DOLExecutable> DOLExecutable::FromData(ByteView _in) {
        GCM_RUNTIME_ASSERT(_in.size() >= 0x100, "DOL header data is incomplete!");

        for (u32 i = 0; i < MAX_TEXT_SECTIONS; ++i) {
            const u32 section_ofs = GCM_BIG_TO_SYSTEM_ENDIAN(*(u32*)(_in.data() + i * sizeof(u32)));
            const u32 section_addr = GCM_BIG_TO_SYSTEM_ENDIAN(*(u32*)(_in.data() + i * sizeof(u32) + 0x48));
            const u32 section_size = GCM_BIG_TO_SYSTEM_ENDIAN(*(u32*)(_in.data() + i * sizeof(u32) + 0x90));

            GCM_RUNTIME_ASSERT(section_ofs + section_size <= _in.size(), "Provided DOL text section markers exceed the size of the file!");
        }

        for (u32 i = MAX_TEXT_SECTIONS; i < MAX_TEXT_SECTIONS + MAX_DATA_SECTIONS; ++i) {
            const u32 section_ofs = GCM_BIG_TO_SYSTEM_ENDIAN(*(u32*)(_in.data() + i * sizeof(u32)));
            const u32 section_addr = GCM_BIG_TO_SYSTEM_ENDIAN(*(u32*)(_in.data() + i * sizeof(u32) + 0x48));
            const u32 section_size = GCM_BIG_TO_SYSTEM_ENDIAN(*(u32*)(_in.data() + i * sizeof(u32) + 0x90));

            GCM_RUNTIME_ASSERT(section_ofs + section_size <= _in.size(), "Provided DOL data section markers exceed the size of the file!");
        }

        std::unique_ptr<DOLExecutable> new_dol = std::make_unique<DOLExecutable>();
        new_dol->m_data.resize(_in.size());
        memcpy(new_dol->m_data.data(), _in.data(), _in.size());
        return new_dol;
    }

    GCM_NODISCARD std::unique_ptr<DOLExecutable> DOLExecutable::FromFile(const std::string& _path) {
        std::ifstream in_file(_path, std::ios::in | std::ios::binary);
        if (!in_file) {
            return nullptr;
        }

        in_file.seekg(0, std::ios::end);
        const u32 file_size = (u32)in_file.tellg();

        GCM_RUNTIME_ASSERT(file_size >= 0x100, "DOL header data is incomplete!");

        for (u32 i = 0; i < MAX_TEXT_SECTIONS; ++i) {
            u32 section_ofs, section_addr, section_size;

            in_file.seekg(i * sizeof(u32), std::ios::beg);
            in_file.read((char*)&section_ofs, sizeof(u32));
            in_file.seekg(i * sizeof(u32) + 0x48, std::ios::beg);
            in_file.read((char*)&section_addr, sizeof(u32));
            in_file.seekg(i * sizeof(u32) + 0x90, std::ios::beg);
            in_file.read((char*)&section_size, sizeof(u32));

            section_ofs = GCM_BIG_TO_SYSTEM_ENDIAN(section_ofs);
            section_addr = GCM_BIG_TO_SYSTEM_ENDIAN(section_addr);
            section_size = GCM_BIG_TO_SYSTEM_ENDIAN(section_size);

            GCM_RUNTIME_ASSERT(section_ofs + section_size <= file_size, "Provided DOL text section markers exceed the size of the file!");
        }

        for (u32 i = MAX_TEXT_SECTIONS; i < MAX_TEXT_SECTIONS + MAX_DATA_SECTIONS; ++i) {
            u32 section_ofs, section_addr, section_size;

            in_file.seekg(i * sizeof(u32), std::ios::beg);
            in_file.read((char*)&section_ofs, sizeof(u32));
            in_file.seekg(i * sizeof(u32) + 0x48, std::ios::beg);
            in_file.read((char*)&section_addr, sizeof(u32));
            in_file.seekg(i * sizeof(u32) + 0x90, std::ios::beg);
            in_file.read((char*)&section_size, sizeof(u32));

            section_ofs = GCM_BIG_TO_SYSTEM_ENDIAN(section_ofs);
            section_addr = GCM_BIG_TO_SYSTEM_ENDIAN(section_addr);
            section_size = GCM_BIG_TO_SYSTEM_ENDIAN(section_size);

            GCM_RUNTIME_ASSERT(section_ofs + section_size <= file_size, "Provided DOL data section markers exceed the size of the file!");
        }

        std::unique_ptr<DOLExecutable> new_dol = std::make_unique<DOLExecutable>();
        new_dol->m_data.resize(file_size);
        in_file.seekg(0, std::ios::beg);
        in_file.read((char*)new_dol->m_data.data(), file_size);
        return new_dol;
    }

    GCM_NODISCARD bool DOLExecutable::ToData(std::vector<u8>& _out) const {
        if GCM_UNLIKELY (!IsValid()) {
            return false;
        }

        _out = m_data;
        return true;
    }

    GCM_NODISCARD bool DOLExecutable::ToFile(const std::string& _path) const {
        if GCM_UNLIKELY (!IsValid()) {
            return false;
        }

        std::ofstream out_file(_path, std::ios::out | std::ios::binary);
        if (!out_file) {
            return false;
        }

        out_file.write((const char*)m_data.data(), m_data.size());
        return true;
    }

    GCM_NODISCARD bool DOLExecutable::IsValid() const GCM_NOEXCEPT {
        for (u32 i = 0; i < MAX_TEXT_SECTIONS + MAX_DATA_SECTIONS; ++i) {
            const u32 section_ofs = GCM_BIG_TO_SYSTEM_ENDIAN(*(u32*)(m_data.data() + i * sizeof(u32)));
            const u32 section_addr = GCM_BIG_TO_SYSTEM_ENDIAN(*(u32*)(m_data.data() + i * sizeof(u32) + 0x48));
            const u32 section_size = GCM_BIG_TO_SYSTEM_ENDIAN(*(u32*)(m_data.data() + i * sizeof(u32) + 0x90));

            if (section_ofs == 0 && section_addr == 0 && section_size == 0) {
                return true;
            }

            if (section_ofs < 0x100 || section_ofs + section_size > m_data.size()) {
                return false;
            }
        }
        return true;
    }

    GCM_NODISCARD u32 DOLExecutable::GetBSSAddress() const GCM_NOEXCEPT {
        if GCM_UNLIKELY (!IsValid()) {
            return false;
        }

        return GCM_BIG_TO_SYSTEM_ENDIAN(*(const u32*)(m_data.data() + 0xD8));
    }

    GCM_NODISCARD u32 DOLExecutable::GetBSSSize() const GCM_NOEXCEPT {
        if GCM_UNLIKELY (!IsValid()) {
            return false;
        }

        return GCM_BIG_TO_SYSTEM_ENDIAN(*(const u32*)(m_data.data() + 0xDC));
    }

    GCM_NODISCARD u32 DOLExecutable::GetEntryAddress() const GCM_NOEXCEPT {
        if GCM_UNLIKELY (!IsValid()) {
            return false;
        }

        return GCM_BIG_TO_SYSTEM_ENDIAN(*(const u32*)(m_data.data() + 0xE0));
    }

    void DOLExecutable::SetBSSAddress(u32 _val) GCM_NOEXCEPT {
        if GCM_UNLIKELY (!IsValid()) {
            return;
        }

        *(u32*)(m_data.data() + 0xD8) = GCM_SYSTEM_TO_BIG_ENDIAN(_val);
    }

    void DOLExecutable::SetBSSSize(u32 _val) GCM_NOEXCEPT {
        if GCM_UNLIKELY (!IsValid()) {
            return;
        }

        *(u32*)(m_data.data() + 0xDC) = GCM_SYSTEM_TO_BIG_ENDIAN(_val);
    }

    void DOLExecutable::SetEntryAddress(u32 _val) GCM_NOEXCEPT {
        if GCM_UNLIKELY (!IsValid()) {
            return;
        }

        *(u32*)(m_data.data() + 0xE0) = GCM_SYSTEM_TO_BIG_ENDIAN(_val);
    }

    GCM_NODISCARD u8 DOLExecutable::GetTextSectionCount() const GCM_NOEXCEPT {
        if GCM_UNLIKELY (!IsValid()) {
            return 0;
        }

        u8 section_count = 0;
        for (u32 i = 0; i < MAX_TEXT_SECTIONS; ++i) {
            const u32 section_ofs = GCM_BIG_TO_SYSTEM_ENDIAN(*(u32*)(m_data.data() + i * sizeof(u32)));
            const u32 section_addr = GCM_BIG_TO_SYSTEM_ENDIAN(*(u32*)(m_data.data() + i * sizeof(u32) + 0x48));
            const u32 section_size = GCM_BIG_TO_SYSTEM_ENDIAN(*(u32*)(m_data.data() + i * sizeof(u32) + 0x90));

            if (section_ofs >= 0x100) {
                section_count++;
            }
        }

        return section_count;
    }

    GCM_NODISCARD u8 DOLExecutable::GetDataSectionCount() const GCM_NOEXCEPT {
        if GCM_UNLIKELY (!IsValid()) {
            return 0;
        }

        u8 section_count = 0;
        for (u32 i = MAX_TEXT_SECTIONS; i < MAX_TEXT_SECTIONS + MAX_DATA_SECTIONS; ++i) {
            const u32 section_ofs = GCM_BIG_TO_SYSTEM_ENDIAN(*(u32*)(m_data.data() + i * sizeof(u32)));
            const u32 section_addr = GCM_BIG_TO_SYSTEM_ENDIAN(*(u32*)(m_data.data() + i * sizeof(u32) + 0x48));
            const u32 section_size = GCM_BIG_TO_SYSTEM_ENDIAN(*(u32*)(m_data.data() + i * sizeof(u32) + 0x90));

            if (section_ofs >= 0x100) {
                section_count++;
            }
        }

        return section_count;
    }

    GCM_NODISCARD u8 DOLExecutable::GetTextSectionForAddress(u32 virtual_addr, u32 desired_len) const GCM_NOEXCEPT {
        if GCM_UNLIKELY (!IsValid()) {
            return INVALID_SECTION;
        }

        desired_len = desired_len == 0 ? 1 : desired_len;
        
        for (u8 i = 0; i < MAX_TEXT_SECTIONS; ++i) {
            const u32 section_offset = GCM_BIG_TO_SYSTEM_ENDIAN(*(u32*)(m_data.data() + i * sizeof(u32)));
            if (section_offset == 0) {
                return INVALID_SECTION;
            }
            const u32 section_addr = GCM_BIG_TO_SYSTEM_ENDIAN(*(u32*)(m_data.data() + i * sizeof(u32) + 0x48));
            const u32 section_size = GCM_BIG_TO_SYSTEM_ENDIAN(*(u32*)(m_data.data() + i * sizeof(u32) + 0x90));

            if (section_addr <= virtual_addr && (virtual_addr + desired_len) <= section_addr + section_size) {
                return i;
            }
        }

        return INVALID_SECTION;
    }

    GCM_NODISCARD u8 DOLExecutable::GetDataSectionForAddress(u32 virtual_addr, u32 desired_len) const GCM_NOEXCEPT {
        if GCM_UNLIKELY (!IsValid()) {
            return INVALID_SECTION;
        }

        desired_len = desired_len == 0 ? 1 : desired_len;
        
        for (u8 i = MAX_TEXT_SECTIONS; i < MAX_TEXT_SECTIONS + MAX_DATA_SECTIONS; ++i) {
            const u32 section_offset = GCM_BIG_TO_SYSTEM_ENDIAN(*(u32*)(m_data.data() + i * sizeof(u32)));
            if (section_offset == 0) {
                return INVALID_SECTION;
            }
            const u32 section_addr = GCM_BIG_TO_SYSTEM_ENDIAN(*(u32*)(m_data.data() + i * sizeof(u32) + 0x48));
            const u32 section_size = GCM_BIG_TO_SYSTEM_ENDIAN(*(u32*)(m_data.data() + i * sizeof(u32) + 0x90));

            if (section_addr <= virtual_addr && (virtual_addr + desired_len) <= section_addr + section_size) {
                return i;
            }
        }

        return INVALID_SECTION;
    }

    GCM_NODISCARD u32 DOLExecutable::GetTextSectionAddress(u8 section_idx) const GCM_NOEXCEPT {
        if GCM_UNLIKELY (!IsValid()) {
            return 0xFFFFFFFF;
        }
        return GCM_BIG_TO_SYSTEM_ENDIAN(*(u32*)(m_data.data() + section_idx * sizeof(u32) + 0x48));
    }

    GCM_NODISCARD u32 DOLExecutable::GetDataSectionAddress(u8 section_idx) const GCM_NOEXCEPT {
        if GCM_UNLIKELY (!IsValid()) {
            return 0xFFFFFFFF;
        }
        return GCM_BIG_TO_SYSTEM_ENDIAN(*(u32*)(m_data.data() + (section_idx + MAX_TEXT_SECTIONS) * sizeof(u32) + 0x48));
    }

    GCM_NODISCARD ByteView DOLExecutable::GetTextSectionView(u8 section_idx) const GCM_NOEXCEPT {
        if GCM_UNLIKELY (!IsValid()) {
            return {};
        }

        if (section_idx < MAX_TEXT_SECTIONS) {
            return {};
        }

        const u32 section_ofs = GCM_BIG_TO_SYSTEM_ENDIAN(*(const u32*)(m_data.data() + section_idx * sizeof(u32)));
        const u32 section_size = GCM_BIG_TO_SYSTEM_ENDIAN(*(const u32*)(m_data.data() + section_idx * sizeof(u32) + 0x90));

        return make_view(m_data.data() + section_ofs, (size_t)section_size);
    }

    GCM_NODISCARD ByteView DOLExecutable::GetDataSectionView(u8 section_idx) const GCM_NOEXCEPT {
        if GCM_UNLIKELY (!IsValid()) {
            return {};
        }

        if (section_idx < MAX_DATA_SECTIONS) {
            return {};
        }

        const u32 section_ofs = GCM_BIG_TO_SYSTEM_ENDIAN(*(const u32*)(m_data.data() + (section_idx + MAX_TEXT_SECTIONS) * sizeof(u32)));
        const u32 section_size = GCM_BIG_TO_SYSTEM_ENDIAN(*(const u32*)(m_data.data() + (section_idx + MAX_TEXT_SECTIONS) * sizeof(u32) + 0x90));

        return make_view(m_data.data() + section_ofs, (size_t)section_size);
    }

    void DOLExecutable::SetTextSectionData(u8 section_idx, ByteView data) GCM_NOEXCEPT {
        if GCM_UNLIKELY (!IsValid()) {
            return;
        }

        if (section_idx >= MAX_TEXT_SECTIONS) {
            return;
        }

        const u32 this_ofs = GCM_BIG_TO_SYSTEM_ENDIAN(*(const u32*)(m_data.data() + section_idx * sizeof(u32)));
        const u32 new_size = RoundUp(static_cast<u32>(data.size()), (u32)4);
        const u32 old_size = GCM_BIG_TO_SYSTEM_ENDIAN(*(const u32*)(m_data.data() + section_idx * sizeof(u32) + 0x90));
        const s32 size_diff = (s32)((s64)new_size - (s64)old_size);

        std::vector<u8> new_data;
        new_data.resize(m_data.size() + size_diff);

        memcpy(new_data.data(), m_data.data(), 0x100);
        *(u32*)(new_data.data() + section_idx * sizeof(u32) + 0x90) = GCM_SYSTEM_TO_BIG_ENDIAN(new_size);

        if (new_size == 0) {
            for (u32 j = section_idx; j < MAX_TEXT_SECTIONS - 1; ++j) {
                *(u32*)(new_data.data() + j * sizeof(u32)) = *(u32*)(new_data.data() + (u64)(j + 1) * sizeof(u32));
                *(u32*)(new_data.data() + j * sizeof(u32) + 0x48) = *(u32*)(new_data.data() + (u64)(j + 1) * sizeof(u32) + 0x48);
                *(u32*)(new_data.data() + j * sizeof(u32) + 0x90) = *(u32*)(new_data.data() + (u64)(j + 1) * sizeof(u32) + 0x90);
            }
            *(u32*)(new_data.data() + (u64)(MAX_TEXT_SECTIONS - 1) * sizeof(u32)) = 0;
            *(u32*)(new_data.data() + (u64)(MAX_TEXT_SECTIONS - 1) * sizeof(u32) + 0x48) = 0;
            *(u32*)(new_data.data() + (u64)(MAX_TEXT_SECTIONS - 1) * sizeof(u32) + 0x90) = 0;
        } else {
            for (u32 i = 0; i < MAX_TEXT_SECTIONS + MAX_DATA_SECTIONS; ++i) {
                const u32 section_ofs = GCM_BIG_TO_SYSTEM_ENDIAN(*(const u32*)(m_data.data() + i * sizeof(u32)));
                if (section_ofs > this_ofs) {
                    *(u32*)(new_data.data() + i * sizeof(u32)) = GCM_SYSTEM_TO_BIG_ENDIAN(section_ofs + size_diff);
                }
            }
        }

        memcpy(new_data.data() + 0x100, m_data.data() + 0x100, this_ofs - 0x100);
        memcpy(new_data.data() + this_ofs, data.data(), data.size());
        if (size_diff > 0) {
            memset(new_data.data() + this_ofs + data.size(), '\0', size_diff);
        }
        memcpy(new_data.data() + this_ofs + new_size, m_data.data() + this_ofs + old_size, m_data.size() - (this_ofs + old_size));
        m_data = std::move(new_data);
    }

    void DOLExecutable::SetDataSectionData(u8 section_idx, ByteView data) GCM_NOEXCEPT {
        if GCM_UNLIKELY (!IsValid()) {
            return;
        }

        if (section_idx >= MAX_DATA_SECTIONS) {
            return;
        }

        const u32 this_ofs = GCM_BIG_TO_SYSTEM_ENDIAN(*(const u32*)(m_data.data() + (section_idx + MAX_TEXT_SECTIONS) * sizeof(u32)));
        const u32 new_size = RoundUp(static_cast<u32>(data.size()), (u32)4);
        const u32 old_size = GCM_BIG_TO_SYSTEM_ENDIAN(*(const u32*)(m_data.data() + (section_idx + MAX_TEXT_SECTIONS) * sizeof(u32) + 0x90));
        const s32 size_diff = (s32)((s64)new_size - (s64)old_size);

        std::vector<u8> new_data;
        new_data.resize(m_data.size() + size_diff);

        memcpy(new_data.data(), m_data.data(), 0x100);
        *(u32*)(new_data.data() + (section_idx + MAX_TEXT_SECTIONS) * sizeof(u32) + 0x90) = GCM_SYSTEM_TO_BIG_ENDIAN(new_size);

        if (new_size == 0) {
            for (u32 j = section_idx + MAX_TEXT_SECTIONS; j < MAX_TEXT_SECTIONS + MAX_DATA_SECTIONS - 1; ++j) {
                *(u32*)(new_data.data() + j * sizeof(u32)) = *(u32*)(new_data.data() + (u64)(j + 1) * sizeof(u32));
                *(u32*)(new_data.data() + j * sizeof(u32) + 0x48) = *(u32*)(new_data.data() + (u64)(j + 1) * sizeof(u32) + 0x48);
                *(u32*)(new_data.data() + j * sizeof(u32) + 0x90) = *(u32*)(new_data.data() + (u64)(j + 1) * sizeof(u32) + 0x90);
            }
            *(u32*)(new_data.data() + (u64)(MAX_TEXT_SECTIONS + MAX_DATA_SECTIONS - 1) * sizeof(u32)) = 0;
            *(u32*)(new_data.data() + (u64)(MAX_TEXT_SECTIONS + MAX_DATA_SECTIONS - 1) * sizeof(u32) + 0x48) = 0;
            *(u32*)(new_data.data() + (u64)(MAX_TEXT_SECTIONS + MAX_DATA_SECTIONS - 1) * sizeof(u32) + 0x90) = 0;
        } else {
            for (u32 i = 0; i < MAX_TEXT_SECTIONS + MAX_DATA_SECTIONS; ++i) {
                const u32 section_ofs = GCM_BIG_TO_SYSTEM_ENDIAN(*(const u32*)(m_data.data() + i * sizeof(u32)));
                if (section_ofs > this_ofs) {
                    *(u32*)(new_data.data() + i * sizeof(u32)) = GCM_SYSTEM_TO_BIG_ENDIAN(section_ofs + size_diff);
                }
            }
        }

        memcpy(new_data.data() + 0x100, m_data.data() + 0x100, this_ofs - 0x100);
        memcpy(new_data.data() + this_ofs, data.data(), data.size());
        if (size_diff > 0) {
            memset(new_data.data() + this_ofs + data.size(), '\0', size_diff);
        }
        memcpy(new_data.data() + this_ofs + new_size, m_data.data() + this_ofs + old_size, m_data.size() - (this_ofs + old_size));
        m_data = std::move(new_data);
    }

    GCM_NODISCARD bool DOLExecutable::ReadAddressBool(u32 virtual_addr) const GCM_NOEXCEPT {
        GCM_RUNTIME_ASSERT(IsValid(), "Tried to read from an invalid DOL!");

        const u8 text_section = GetTextSectionForAddress(virtual_addr, 1);
        if (text_section != INVALID_SECTION) {
            const u32 section_ofs = GCM_BIG_TO_SYSTEM_ENDIAN(*(const u32*)(m_data.data() + text_section * sizeof(u32)));
            const u32 section_addr = GCM_BIG_TO_SYSTEM_ENDIAN(*(const u32*)(m_data.data() + text_section * sizeof(u32) + 0x48));
            const u8* data_ptr = m_data.data() + section_ofs + (virtual_addr - section_addr);
            return *(bool*)data_ptr;
        }

        const u8 data_section = GetDataSectionForAddress(virtual_addr, 1);
        if (data_section != INVALID_SECTION) {
            const u32 section_ofs = GCM_BIG_TO_SYSTEM_ENDIAN(*(const u32*)(m_data.data() + (text_section + MAX_TEXT_SECTIONS) * sizeof(u32)));
            const u32 section_addr = GCM_BIG_TO_SYSTEM_ENDIAN(*(const u32*)(m_data.data() + (text_section + MAX_TEXT_SECTIONS) * sizeof(u32) + 0x48));
            const u8* data_ptr = m_data.data() + section_ofs + (virtual_addr - section_addr);
            return *(bool*)data_ptr;
        }

        GCM_RUNTIME_ASSERT(false, "Virtual address did not map to a valid section offset!");
        return {};
    }

    GCM_NODISCARD s8 DOLExecutable::ReadAddressS8(u32 virtual_addr) const GCM_NOEXCEPT {
        GCM_RUNTIME_ASSERT(IsValid(), "Tried to read from an invalid DOL!");

        const u8 text_section = GetTextSectionForAddress(virtual_addr, 1);
        if (text_section != INVALID_SECTION) {
            const u32 section_ofs = GCM_BIG_TO_SYSTEM_ENDIAN(*(const u32*)(m_data.data() + text_section * sizeof(u32)));
            const u32 section_addr = GCM_BIG_TO_SYSTEM_ENDIAN(*(const u32*)(m_data.data() + text_section * sizeof(u32) + 0x48));
            const u8* data_ptr = m_data.data() + section_ofs + (virtual_addr - section_addr);
            return *(s8*)data_ptr;
        }

        const u8 data_section = GetDataSectionForAddress(virtual_addr, 1);
        if (data_section != INVALID_SECTION) {
            const u32 section_ofs = GCM_BIG_TO_SYSTEM_ENDIAN(*(const u32*)(m_data.data() + (text_section + MAX_TEXT_SECTIONS) * sizeof(u32)));
            const u32 section_addr = GCM_BIG_TO_SYSTEM_ENDIAN(*(const u32*)(m_data.data() + (text_section + MAX_TEXT_SECTIONS) * sizeof(u32) + 0x48));
            const u8* data_ptr = m_data.data() + section_ofs + (virtual_addr - section_addr);
            return *(s8*)data_ptr;
        }

        GCM_RUNTIME_ASSERT(false, "Virtual address did not map to a valid section offset!");
        return {};
    }

    GCM_NODISCARD u8 DOLExecutable::ReadAddressU8(u32 virtual_addr) const GCM_NOEXCEPT {
        GCM_RUNTIME_ASSERT(IsValid(), "Tried to read from an invalid DOL!");

        const u8 text_section = GetTextSectionForAddress(virtual_addr, 1);
        if (text_section != INVALID_SECTION) {
            const u32 section_ofs = GCM_BIG_TO_SYSTEM_ENDIAN(*(const u32*)(m_data.data() + text_section * sizeof(u32)));
            const u32 section_addr = GCM_BIG_TO_SYSTEM_ENDIAN(*(const u32*)(m_data.data() + text_section * sizeof(u32) + 0x48));
            const u8* data_ptr = m_data.data() + section_ofs + (virtual_addr - section_addr);
            return *(u8*)data_ptr;
        }

        const u8 data_section = GetDataSectionForAddress(virtual_addr, 1);
        if (data_section != INVALID_SECTION) {
            const u32 section_ofs = GCM_BIG_TO_SYSTEM_ENDIAN(*(const u32*)(m_data.data() + (text_section + MAX_TEXT_SECTIONS) * sizeof(u32)));
            const u32 section_addr = GCM_BIG_TO_SYSTEM_ENDIAN(*(const u32*)(m_data.data() + (text_section + MAX_TEXT_SECTIONS) * sizeof(u32) + 0x48));
            const u8* data_ptr = m_data.data() + section_ofs + (virtual_addr - section_addr);
            return *(u8*)data_ptr;
        }

        GCM_RUNTIME_ASSERT(false, "Virtual address did not map to a valid section offset!");
        return {};
    }

    GCM_NODISCARD s16 DOLExecutable::ReadAddressS16(u32 virtual_addr) const GCM_NOEXCEPT {
        GCM_RUNTIME_ASSERT(IsValid(), "Tried to read from an invalid DOL!");

        const u8 text_section = GetTextSectionForAddress(virtual_addr, 2);
        if (text_section != INVALID_SECTION) {
            const u32 section_ofs = GCM_BIG_TO_SYSTEM_ENDIAN(*(const u32*)(m_data.data() + text_section * sizeof(u32)));
            const u32 section_addr = GCM_BIG_TO_SYSTEM_ENDIAN(*(const u32*)(m_data.data() + text_section * sizeof(u32) + 0x48));
            const u8* data_ptr = m_data.data() + section_ofs + (virtual_addr - section_addr);
            return GCM_BIG_TO_SYSTEM_ENDIAN(*(s16*)data_ptr);
        }

        const u8 data_section = GetDataSectionForAddress(virtual_addr, 2);
        if (data_section != INVALID_SECTION) {
            const u32 section_ofs = GCM_BIG_TO_SYSTEM_ENDIAN(*(const u32*)(m_data.data() + (text_section + MAX_TEXT_SECTIONS) * sizeof(u32)));
            const u32 section_addr = GCM_BIG_TO_SYSTEM_ENDIAN(*(const u32*)(m_data.data() + (text_section + MAX_TEXT_SECTIONS) * sizeof(u32) + 0x48));
            const u8* data_ptr = m_data.data() + section_ofs + (virtual_addr - section_addr);
            return GCM_BIG_TO_SYSTEM_ENDIAN(*(s16*)data_ptr);
        }

        GCM_RUNTIME_ASSERT(false, "Virtual address did not map to a valid section offset!");
        return {};
    }

    GCM_NODISCARD u16 DOLExecutable::ReadAddressU16(u32 virtual_addr) const GCM_NOEXCEPT {
        GCM_RUNTIME_ASSERT(IsValid(), "Tried to read from an invalid DOL!");

        const u8 text_section = GetTextSectionForAddress(virtual_addr, 2);
        if (text_section != INVALID_SECTION) {
            const u32 section_ofs = GCM_BIG_TO_SYSTEM_ENDIAN(*(const u32*)(m_data.data() + text_section * sizeof(u32)));
            const u32 section_addr = GCM_BIG_TO_SYSTEM_ENDIAN(*(const u32*)(m_data.data() + text_section * sizeof(u32) + 0x48));
            const u8* data_ptr = m_data.data() + section_ofs + (virtual_addr - section_addr);
            return GCM_BIG_TO_SYSTEM_ENDIAN(*(u16*)data_ptr);
        }

        const u8 data_section = GetDataSectionForAddress(virtual_addr, 2);
        if (data_section != INVALID_SECTION) {
            const u32 section_ofs = GCM_BIG_TO_SYSTEM_ENDIAN(*(const u32*)(m_data.data() + (text_section + MAX_TEXT_SECTIONS) * sizeof(u32)));
            const u32 section_addr = GCM_BIG_TO_SYSTEM_ENDIAN(*(const u32*)(m_data.data() + (text_section + MAX_TEXT_SECTIONS) * sizeof(u32) + 0x48));
            const u8* data_ptr = m_data.data() + section_ofs + (virtual_addr - section_addr);
            return GCM_BIG_TO_SYSTEM_ENDIAN(*(u16*)data_ptr);
        }

        GCM_RUNTIME_ASSERT(false, "Virtual address did not map to a valid section offset!");
        return {};
    }

    GCM_NODISCARD s32 DOLExecutable::ReadAddressS32(u32 virtual_addr) const GCM_NOEXCEPT {
        GCM_RUNTIME_ASSERT(IsValid(), "Tried to read from an invalid DOL!");

        const u8 text_section = GetTextSectionForAddress(virtual_addr, 4);
        if (text_section != INVALID_SECTION) {
            const u32 section_ofs = GCM_BIG_TO_SYSTEM_ENDIAN(*(const u32*)(m_data.data() + text_section * sizeof(u32)));
            const u32 section_addr = GCM_BIG_TO_SYSTEM_ENDIAN(*(const u32*)(m_data.data() + text_section * sizeof(u32) + 0x48));
            const u8* data_ptr = m_data.data() + section_ofs + (virtual_addr - section_addr);
            return GCM_BIG_TO_SYSTEM_ENDIAN(*(s32*)data_ptr);
        }

        const u8 data_section = GetDataSectionForAddress(virtual_addr, 4);
        if (data_section != INVALID_SECTION) {
            const u32 section_ofs = GCM_BIG_TO_SYSTEM_ENDIAN(*(const u32*)(m_data.data() + (text_section + MAX_TEXT_SECTIONS) * sizeof(u32)));
            const u32 section_addr = GCM_BIG_TO_SYSTEM_ENDIAN(*(const u32*)(m_data.data() + (text_section + MAX_TEXT_SECTIONS) * sizeof(u32) + 0x48));
            const u8* data_ptr = m_data.data() + section_ofs + (virtual_addr - section_addr);
            return GCM_BIG_TO_SYSTEM_ENDIAN(*(s32*)data_ptr);
        }

        GCM_RUNTIME_ASSERT(false, "Virtual address did not map to a valid section offset!");
        return {};
    }

    GCM_NODISCARD u32 DOLExecutable::ReadAddressU32(u32 virtual_addr) const GCM_NOEXCEPT {
        GCM_RUNTIME_ASSERT(IsValid(), "Tried to read from an invalid DOL!");

        const u8 text_section = GetTextSectionForAddress(virtual_addr, 4);
        if (text_section != INVALID_SECTION) {
            const u32 section_ofs = GCM_BIG_TO_SYSTEM_ENDIAN(*(const u32*)(m_data.data() + text_section * sizeof(u32)));
            const u32 section_addr = GCM_BIG_TO_SYSTEM_ENDIAN(*(const u32*)(m_data.data() + text_section * sizeof(u32) + 0x48));
            const u8* data_ptr = m_data.data() + section_ofs + (virtual_addr - section_addr);
            return GCM_BIG_TO_SYSTEM_ENDIAN(*(u32*)data_ptr);
        }

        const u8 data_section = GetDataSectionForAddress(virtual_addr, 4);
        if (data_section != INVALID_SECTION) {
            const u32 section_ofs = GCM_BIG_TO_SYSTEM_ENDIAN(*(const u32*)(m_data.data() + data_section * sizeof(u32)));
            const u32 section_addr = GCM_BIG_TO_SYSTEM_ENDIAN(*(const u32*)(m_data.data() + data_section * sizeof(u32) + 0x48));
            const u8* data_ptr = m_data.data() + section_ofs + (virtual_addr - section_addr);
            return GCM_BIG_TO_SYSTEM_ENDIAN(*(u32*)data_ptr);
        }

        GCM_RUNTIME_ASSERT(false, "Virtual address did not map to a valid section offset!");
        return {};
    }

    GCM_NODISCARD f32 DOLExecutable::ReadAddressF32(u32 virtual_addr) const GCM_NOEXCEPT {
        GCM_RUNTIME_ASSERT(IsValid(), "Tried to read from an invalid DOL!");

        const u8 text_section = GetTextSectionForAddress(virtual_addr, 4);
        if (text_section != INVALID_SECTION) {
            const u32 section_ofs = GCM_BIG_TO_SYSTEM_ENDIAN(*(const u32*)(m_data.data() + text_section * sizeof(u32)));
            const u32 section_addr = GCM_BIG_TO_SYSTEM_ENDIAN(*(const u32*)(m_data.data() + text_section * sizeof(u32) + 0x48));
            const u8* data_ptr = m_data.data() + section_ofs + (virtual_addr - section_addr);
            return GCM_BIG_TO_SYSTEM_ENDIAN(*(f32*)data_ptr);
        }

        const u8 data_section = GetDataSectionForAddress(virtual_addr, 4);
        if (data_section != INVALID_SECTION) {
            const u32 section_ofs = GCM_BIG_TO_SYSTEM_ENDIAN(*(const u32*)(m_data.data() + data_section * sizeof(u32)));
            const u32 section_addr = GCM_BIG_TO_SYSTEM_ENDIAN(*(const u32*)(m_data.data() + data_section * sizeof(u32) + 0x48));
            const u8* data_ptr = m_data.data() + section_ofs + (virtual_addr - section_addr);
            return GCM_BIG_TO_SYSTEM_ENDIAN(*(f32*)data_ptr);
        }

        GCM_RUNTIME_ASSERT(false, "Virtual address did not map to a valid section offset!");
        return {};
    }

    GCM_NODISCARD f64 DOLExecutable::ReadAddressF64(u32 virtual_addr) const GCM_NOEXCEPT {
        GCM_RUNTIME_ASSERT(IsValid(), "Tried to read from an invalid DOL!");

        const u8 text_section = GetTextSectionForAddress(virtual_addr, 8);
        if (text_section != INVALID_SECTION) {
            const u32 section_ofs = GCM_BIG_TO_SYSTEM_ENDIAN(*(const u32*)(m_data.data() + text_section * sizeof(u32)));
            const u32 section_addr = GCM_BIG_TO_SYSTEM_ENDIAN(*(const u32*)(m_data.data() + text_section * sizeof(u32) + 0x48));
            const u8* data_ptr = m_data.data() + section_ofs + (virtual_addr - section_addr);
            return GCM_BIG_TO_SYSTEM_ENDIAN(*(f64*)data_ptr);
        }

        const u8 data_section = GetDataSectionForAddress(virtual_addr, 8);
        if (data_section != INVALID_SECTION) {
            const u32 section_ofs = GCM_BIG_TO_SYSTEM_ENDIAN(*(const u32*)(m_data.data() + data_section * sizeof(u32)));
            const u32 section_addr = GCM_BIG_TO_SYSTEM_ENDIAN(*(const u32*)(m_data.data() + data_section * sizeof(u32) + 0x48));
            const u8* data_ptr = m_data.data() + section_ofs + (virtual_addr - section_addr);
            return GCM_BIG_TO_SYSTEM_ENDIAN(*(f64*)data_ptr);
        }

        GCM_RUNTIME_ASSERT(false, "Virtual address did not map to a valid section offset!");
        return {};
    }

    GCM_NODISCARD std::string DOLExecutable::ReadAddressCString(u32 virtual_addr) const GCM_NOEXCEPT {
        GCM_RUNTIME_ASSERT(IsValid(), "Tried to read from an invalid DOL!");

        const u8 text_section = GetTextSectionForAddress(virtual_addr);
        if (text_section != INVALID_SECTION) {
            const u32 section_ofs = GCM_BIG_TO_SYSTEM_ENDIAN(*(const u32*)(m_data.data() + text_section * sizeof(u32)));
            const u32 section_addr = GCM_BIG_TO_SYSTEM_ENDIAN(*(const u32*)(m_data.data() + text_section * sizeof(u32) + 0x48));
            const u8* data_ptr = m_data.data() + section_ofs + (virtual_addr - section_addr);
            return std::string((char*)data_ptr);
        }

        const u8 data_section = GetDataSectionForAddress(virtual_addr);
        if (data_section != INVALID_SECTION) {
            const u32 section_ofs = GCM_BIG_TO_SYSTEM_ENDIAN(*(const u32*)(m_data.data() + data_section * sizeof(u32)));
            const u32 section_addr = GCM_BIG_TO_SYSTEM_ENDIAN(*(const u32*)(m_data.data() + data_section * sizeof(u32) + 0x48));
            const u8* data_ptr = m_data.data() + section_ofs + (virtual_addr - section_addr);
            return std::string((char*)data_ptr);
        }

        GCM_RUNTIME_ASSERT(false, "Virtual address did not map to a valid section offset!");
        return {};
    }

    void DOLExecutable::WriteAddressBool(u32 virtual_addr, bool data) GCM_NOEXCEPT {
        GCM_RUNTIME_ASSERT(IsValid(), "Tried to read from an invalid DOL!");

        const u8 text_section = GetTextSectionForAddress(virtual_addr, 1);
        if (text_section != INVALID_SECTION) {
            const u32 section_ofs = GCM_BIG_TO_SYSTEM_ENDIAN(*(const u32*)(m_data.data() + text_section * sizeof(u32)));
            const u32 section_addr = GCM_BIG_TO_SYSTEM_ENDIAN(*(const u32*)(m_data.data() + text_section * sizeof(u32) + 0x48));
            u8* data_ptr = m_data.data() + section_ofs + (virtual_addr - section_addr);
            *(bool*)data_ptr = data;
        }

        const u8 data_section = GetDataSectionForAddress(virtual_addr, 1);
        if (data_section != INVALID_SECTION) {
            const u32 section_ofs = GCM_BIG_TO_SYSTEM_ENDIAN(*(const u32*)(m_data.data() + data_section * sizeof(u32)));
            const u32 section_addr = GCM_BIG_TO_SYSTEM_ENDIAN(*(const u32*)(m_data.data() + data_section * sizeof(u32) + 0x48));
            u8* data_ptr = m_data.data() + section_ofs + (virtual_addr - section_addr);
            *(bool*)data_ptr = data;
        }

        GCM_RUNTIME_ASSERT(false, "Virtual address did not map to a valid section offset!");
    }

    void DOLExecutable::WriteAddressS8(u32 virtual_addr, s8 data) GCM_NOEXCEPT {
        GCM_RUNTIME_ASSERT(IsValid(), "Tried to read from an invalid DOL!");

        const u8 text_section = GetTextSectionForAddress(virtual_addr, 1);
        if (text_section != INVALID_SECTION) {
            const u32 section_ofs = GCM_BIG_TO_SYSTEM_ENDIAN(*(const u32*)(m_data.data() + text_section * sizeof(u32)));
            const u32 section_addr = GCM_BIG_TO_SYSTEM_ENDIAN(*(const u32*)(m_data.data() + text_section * sizeof(u32) + 0x48));
            u8* data_ptr = m_data.data() + section_ofs + (virtual_addr - section_addr);
            *(s8*)data_ptr = data;
        }

        const u8 data_section = GetDataSectionForAddress(virtual_addr, 1);
        if (data_section != INVALID_SECTION) {
            const u32 section_ofs = GCM_BIG_TO_SYSTEM_ENDIAN(*(const u32*)(m_data.data() + data_section * sizeof(u32)));
            const u32 section_addr = GCM_BIG_TO_SYSTEM_ENDIAN(*(const u32*)(m_data.data() + data_section * sizeof(u32) + 0x48));
            u8* data_ptr = m_data.data() + section_ofs + (virtual_addr - section_addr);
            *(s8*)data_ptr = data;
        }

        GCM_RUNTIME_ASSERT(false, "Virtual address did not map to a valid section offset!");
    }

    void DOLExecutable::WriteAddressU8(u32 virtual_addr, u8 data) GCM_NOEXCEPT {
        GCM_RUNTIME_ASSERT(IsValid(), "Tried to read from an invalid DOL!");

        const u8 text_section = GetTextSectionForAddress(virtual_addr, 1);
        if (text_section != INVALID_SECTION) {
            const u32 section_ofs = GCM_BIG_TO_SYSTEM_ENDIAN(*(const u32*)(m_data.data() + text_section * sizeof(u32)));
            const u32 section_addr = GCM_BIG_TO_SYSTEM_ENDIAN(*(const u32*)(m_data.data() + text_section * sizeof(u32) + 0x48));
            u8* data_ptr = m_data.data() + section_ofs + (virtual_addr - section_addr);
            *(u8*)data_ptr = data;
        }

        const u8 data_section = GetDataSectionForAddress(virtual_addr, 1);
        if (data_section != INVALID_SECTION) {
            const u32 section_ofs = GCM_BIG_TO_SYSTEM_ENDIAN(*(const u32*)(m_data.data() + data_section * sizeof(u32)));
            const u32 section_addr = GCM_BIG_TO_SYSTEM_ENDIAN(*(const u32*)(m_data.data() + data_section * sizeof(u32) + 0x48));
            u8* data_ptr = m_data.data() + section_ofs + (virtual_addr - section_addr);
            *(u8*)data_ptr = data;
        }

        GCM_RUNTIME_ASSERT(false, "Virtual address did not map to a valid section offset!");
    }

    void DOLExecutable::WriteAddressS16(u32 virtual_addr, s16 data) GCM_NOEXCEPT {
        GCM_RUNTIME_ASSERT(IsValid(), "Tried to read from an invalid DOL!");

        const u8 text_section = GetTextSectionForAddress(virtual_addr, 2);
        if (text_section != INVALID_SECTION) {
            const u32 section_ofs = GCM_BIG_TO_SYSTEM_ENDIAN(*(const u32*)(m_data.data() + text_section * sizeof(u32)));
            const u32 section_addr = GCM_BIG_TO_SYSTEM_ENDIAN(*(const u32*)(m_data.data() + text_section * sizeof(u32) + 0x48));
            u8* data_ptr = m_data.data() + section_ofs + (virtual_addr - section_addr);
            *(s16*)data_ptr = GCM_SYSTEM_TO_BIG_ENDIAN(data);
        }

        const u8 data_section = GetDataSectionForAddress(virtual_addr, 2);
        if (data_section != INVALID_SECTION) {
            const u32 section_ofs = GCM_BIG_TO_SYSTEM_ENDIAN(*(const u32*)(m_data.data() + data_section * sizeof(u32)));
            const u32 section_addr = GCM_BIG_TO_SYSTEM_ENDIAN(*(const u32*)(m_data.data() + data_section * sizeof(u32) + 0x48));
            u8* data_ptr = m_data.data() + section_ofs + (virtual_addr - section_addr);
            *(s16*)data_ptr = GCM_SYSTEM_TO_BIG_ENDIAN(data);
        }

        GCM_RUNTIME_ASSERT(false, "Virtual address did not map to a valid section offset!");
    }

    void DOLExecutable::WriteAddressU16(u32 virtual_addr, u16 data) GCM_NOEXCEPT {
        GCM_RUNTIME_ASSERT(IsValid(), "Tried to read from an invalid DOL!");

        const u8 text_section = GetTextSectionForAddress(virtual_addr, 2);
        if (text_section != INVALID_SECTION) {
            const u32 section_ofs = GCM_BIG_TO_SYSTEM_ENDIAN(*(const u32*)(m_data.data() + text_section * sizeof(u32)));
            const u32 section_addr = GCM_BIG_TO_SYSTEM_ENDIAN(*(const u32*)(m_data.data() + text_section * sizeof(u32) + 0x48));
            u8* data_ptr = m_data.data() + section_ofs + (virtual_addr - section_addr);
            *(u16*)data_ptr = GCM_SYSTEM_TO_BIG_ENDIAN(data);
        }

        const u8 data_section = GetDataSectionForAddress(virtual_addr, 2);
        if (data_section != INVALID_SECTION) {
            const u32 section_ofs = GCM_BIG_TO_SYSTEM_ENDIAN(*(const u32*)(m_data.data() + data_section * sizeof(u32)));
            const u32 section_addr = GCM_BIG_TO_SYSTEM_ENDIAN(*(const u32*)(m_data.data() + data_section * sizeof(u32) + 0x48));
            u8* data_ptr = m_data.data() + section_ofs + (virtual_addr - section_addr);
            *(u16*)data_ptr = GCM_SYSTEM_TO_BIG_ENDIAN(data);
        }

        GCM_RUNTIME_ASSERT(false, "Virtual address did not map to a valid section offset!");
    }

    void DOLExecutable::WriteAddressS32(u32 virtual_addr, s32 data) GCM_NOEXCEPT {
        GCM_RUNTIME_ASSERT(IsValid(), "Tried to read from an invalid DOL!");

        const u8 text_section = GetTextSectionForAddress(virtual_addr, 4);
        if (text_section != INVALID_SECTION) {
            const u32 section_ofs = GCM_BIG_TO_SYSTEM_ENDIAN(*(const u32*)(m_data.data() + text_section * sizeof(u32)));
            const u32 section_addr = GCM_BIG_TO_SYSTEM_ENDIAN(*(const u32*)(m_data.data() + text_section * sizeof(u32) + 0x48));
            u8* data_ptr = m_data.data() + section_ofs + (virtual_addr - section_addr);
            *(s32*)data_ptr = GCM_SYSTEM_TO_BIG_ENDIAN(data);
        }

        const u8 data_section = GetDataSectionForAddress(virtual_addr, 4);
        if (data_section != INVALID_SECTION) {
            const u32 section_ofs = GCM_BIG_TO_SYSTEM_ENDIAN(*(const u32*)(m_data.data() + data_section * sizeof(u32)));
            const u32 section_addr = GCM_BIG_TO_SYSTEM_ENDIAN(*(const u32*)(m_data.data() + data_section * sizeof(u32) + 0x48));
            u8* data_ptr = m_data.data() + section_ofs + (virtual_addr - section_addr);
            *(s32*)data_ptr = GCM_SYSTEM_TO_BIG_ENDIAN(data);
        }

        GCM_RUNTIME_ASSERT(false, "Virtual address did not map to a valid section offset!");
    }

    void DOLExecutable::WriteAddressU32(u32 virtual_addr, u32 data) GCM_NOEXCEPT {
        GCM_RUNTIME_ASSERT(IsValid(), "Tried to read from an invalid DOL!");

        const u8 text_section = GetTextSectionForAddress(virtual_addr, 4);
        if (text_section != INVALID_SECTION) {
            const u32 section_ofs = GCM_BIG_TO_SYSTEM_ENDIAN(*(const u32*)(m_data.data() + text_section * sizeof(u32)));
            const u32 section_addr = GCM_BIG_TO_SYSTEM_ENDIAN(*(const u32*)(m_data.data() + text_section * sizeof(u32) + 0x48));
            u8* data_ptr = m_data.data() + section_ofs + (virtual_addr - section_addr);
            *(u32*)data_ptr = GCM_SYSTEM_TO_BIG_ENDIAN(data);
        }

        const u8 data_section = GetDataSectionForAddress(virtual_addr, 4);
        if (data_section != INVALID_SECTION) {
            const u32 section_ofs = GCM_BIG_TO_SYSTEM_ENDIAN(*(const u32*)(m_data.data() + data_section * sizeof(u32)));
            const u32 section_addr = GCM_BIG_TO_SYSTEM_ENDIAN(*(const u32*)(m_data.data() + data_section * sizeof(u32) + 0x48));
            u8* data_ptr = m_data.data() + section_ofs + (virtual_addr - section_addr);
            *(u32*)data_ptr = GCM_SYSTEM_TO_BIG_ENDIAN(data);
        }

        GCM_RUNTIME_ASSERT(false, "Virtual address did not map to a valid section offset!");
    }

    void DOLExecutable::WriteAddressF32(u32 virtual_addr, f32 data) GCM_NOEXCEPT {
        GCM_RUNTIME_ASSERT(IsValid(), "Tried to read from an invalid DOL!");

        const u8 text_section = GetTextSectionForAddress(virtual_addr, 4);
        if (text_section != INVALID_SECTION) {
            const u32 section_ofs = GCM_BIG_TO_SYSTEM_ENDIAN(*(const u32*)(m_data.data() + text_section * sizeof(u32)));
            const u32 section_addr = GCM_BIG_TO_SYSTEM_ENDIAN(*(const u32*)(m_data.data() + text_section * sizeof(u32) + 0x48));
            u8* data_ptr = m_data.data() + section_ofs + (virtual_addr - section_addr);
            *(f32*)data_ptr = GCM_SYSTEM_TO_BIG_ENDIAN(data);
        }

        const u8 data_section = GetDataSectionForAddress(virtual_addr, 4);
        if (data_section != INVALID_SECTION) {
            const u32 section_ofs = GCM_BIG_TO_SYSTEM_ENDIAN(*(const u32*)(m_data.data() + data_section * sizeof(u32)));
            const u32 section_addr = GCM_BIG_TO_SYSTEM_ENDIAN(*(const u32*)(m_data.data() + data_section * sizeof(u32) + 0x48));
            u8* data_ptr = m_data.data() + section_ofs + (virtual_addr - section_addr);
            *(f32*)data_ptr = GCM_SYSTEM_TO_BIG_ENDIAN(data);
        }

        GCM_RUNTIME_ASSERT(false, "Virtual address did not map to a valid section offset!");
    }

    void DOLExecutable::WriteAddressF64(u32 virtual_addr, f64 data) GCM_NOEXCEPT {
        GCM_RUNTIME_ASSERT(IsValid(), "Tried to read from an invalid DOL!");

        const u8 text_section = GetTextSectionForAddress(virtual_addr, 8);
        if (text_section != INVALID_SECTION) {
            const u32 section_ofs = GCM_BIG_TO_SYSTEM_ENDIAN(*(const u32*)(m_data.data() + text_section * sizeof(u32)));
            const u32 section_addr = GCM_BIG_TO_SYSTEM_ENDIAN(*(const u32*)(m_data.data() + text_section * sizeof(u32) + 0x48));
            u8* data_ptr = m_data.data() + section_ofs + (virtual_addr - section_addr);
            *(f64*)data_ptr = GCM_SYSTEM_TO_BIG_ENDIAN(data);
        }

        const u8 data_section = GetDataSectionForAddress(virtual_addr, 8);
        if (data_section != INVALID_SECTION) {
            const u32 section_ofs = GCM_BIG_TO_SYSTEM_ENDIAN(*(const u32*)(m_data.data() + data_section * sizeof(u32)));
            const u32 section_addr = GCM_BIG_TO_SYSTEM_ENDIAN(*(const u32*)(m_data.data() + data_section * sizeof(u32) + 0x48));
            u8* data_ptr = m_data.data() + section_ofs + (virtual_addr - section_addr);
            *(f64*)data_ptr = GCM_SYSTEM_TO_BIG_ENDIAN(data);
        }

        GCM_RUNTIME_ASSERT(false, "Virtual address did not map to a valid section offset!");
        
    }

    void DOLExecutable::WriteAddressCString(u32 virtual_addr, const std::string& data) GCM_NOEXCEPT {
        GCM_RUNTIME_ASSERT(IsValid(), "Tried to read from an invalid DOL!");

        if (data.empty()) {
            return;
        }

        const u8 text_section = GetTextSectionForAddress(virtual_addr, (u32)(data.size() + 1));
        if (text_section != INVALID_SECTION) {
            const u32 section_ofs = GCM_BIG_TO_SYSTEM_ENDIAN(*(const u32*)(m_data.data() + text_section * sizeof(u32)));
            const u32 section_addr = GCM_BIG_TO_SYSTEM_ENDIAN(*(const u32*)(m_data.data() + text_section * sizeof(u32) + 0x48));
            u8* data_ptr = m_data.data() + section_ofs + (virtual_addr - section_addr);
            memcpy(data_ptr, data.c_str(), data.size() + 1);
        }

        const u8 data_section = GetDataSectionForAddress(virtual_addr, (u32)(data.size() + 1));
        if (data_section != INVALID_SECTION) {
            const u32 section_ofs = GCM_BIG_TO_SYSTEM_ENDIAN(*(const u32*)(m_data.data() + data_section * sizeof(u32)));
            const u32 section_addr = GCM_BIG_TO_SYSTEM_ENDIAN(*(const u32*)(m_data.data() + data_section * sizeof(u32) + 0x48));
            u8* data_ptr = m_data.data() + section_ofs + (virtual_addr - section_addr);
            memcpy(data_ptr, data.c_str(), data.size() + 1);
        }

        GCM_RUNTIME_ASSERT(false, "Virtual address did not map to a valid section offset!");
    }

    BI2Sector::BI2Sector() {
        m_data.fill('\0');
    }

    GCM_NODISCARD std::unique_ptr<BI2Sector> BI2Sector::FromData(ByteView _data) {
        GCM_RUNTIME_ASSERT((u32)_data.size() == BI2Sector::FORMAT_SIZE, "Provided BI2 data does not match the BI2 format size!");

        std::unique_ptr<BI2Sector> new_bi2 = std::make_unique<BI2Sector>();
        memcpy(new_bi2->m_data.data(), _data.data(), _data.size());

        return new_bi2;
    }

    GCM_NODISCARD std::unique_ptr<BI2Sector> BI2Sector::FromFile(const std::string& _data) {
        std::ifstream in_file(_data, std::ios::in | std::ios::binary);
        if (!in_file) {
            return nullptr;
        }

        in_file.seekg(0, std::ios::end);
        GCM_RUNTIME_ASSERT((u32)in_file.tellg() == BI2Sector::FORMAT_SIZE, "Provided BI2 data does not match the BI2 format size!");

        in_file.seekg(0, std::ios::beg);
        std::unique_ptr<BI2Sector> new_bi2 = std::make_unique<BI2Sector>();
        in_file.read((char*)new_bi2->m_data.data(), new_bi2->m_data.size());

        return new_bi2;
    }

    GCM_NODISCARD bool BI2Sector::ToData(std::vector<u8>& _out) const {
        if GCM_UNLIKELY (!IsValid()) {
            return false;
        }

        _out.resize(m_data.size());
        memcpy(_out.data(), m_data.data(), m_data.size());
        return true;
    }

    GCM_NODISCARD bool BI2Sector::ToFile(const std::string& _path) const {
        if GCM_UNLIKELY (!IsValid()) {
            return false;
        }

        std::ofstream out_file(_path, std::ios::out | std::ios::binary);
        if (!out_file) {
            return false;
        }

        out_file.write((const char*)m_data.data(), m_data.size());
        return true;
    }

    GCM_NODISCARD bool BI2Sector::IsValid() const GCM_NOEXCEPT {
        return (u32)m_data.size() == BI2Sector::FORMAT_SIZE;
    }

    GCM_NODISCARD u32 BI2Sector::GetArgumentOffset() const GCM_NOEXCEPT {
        if GCM_UNLIKELY (!IsValid()) {
            return 0xFFFFFFFF;
        }

        return GCM_BIG_TO_SYSTEM_ENDIAN(*(u32*)(m_data.data() + 0xC)); // YYYY/MM/DD
    }

    GCM_NODISCARD u32 BI2Sector::GetDebugMonitorSize() const GCM_NOEXCEPT {
        if GCM_UNLIKELY (!IsValid()) {
            return 0xFFFFFFFF;
        }

        return GCM_BIG_TO_SYSTEM_ENDIAN(*(u32*)(m_data.data()));
    }

    GCM_NODISCARD u32 BI2Sector::GetDebugFlag() const GCM_NOEXCEPT {
        if GCM_UNLIKELY (!IsValid()) {
            return 0xFFFFFFFF;
        }

        return GCM_BIG_TO_SYSTEM_ENDIAN(*(u32*)(m_data.data() + 0x8));
    }

    GCM_NODISCARD ERegion BI2Sector::GetRegion() const GCM_NOEXCEPT {
        if GCM_UNLIKELY (!IsValid()) {
            return ERegion::UNKNOWN;
        }

        u32 ret = GCM_BIG_TO_SYSTEM_ENDIAN(*(u32*)(m_data.data() + 0x18));
        if GCM_UNLIKELY (ret > 2) {
            return ERegion::UNKNOWN;
        }

        return static_cast<ERegion>(ret);
    }

    GCM_NODISCARD u32 BI2Sector::GetSimulatedMemSize() const GCM_NOEXCEPT {
        if GCM_UNLIKELY (!IsValid()) {
            return 0xFFFFFFFF;
        }

        return GCM_BIG_TO_SYSTEM_ENDIAN(*(u32*)(m_data.data() + 0x4));
    }

    GCM_NODISCARD u32 BI2Sector::GetTrackLocation() const GCM_NOEXCEPT {
        if GCM_UNLIKELY (!IsValid()) {
            return 0xFFFFFFFF;
        }

        return GCM_BIG_TO_SYSTEM_ENDIAN(*(u32*)(m_data.data() + 0x10));
    }

    GCM_NODISCARD u32 BI2Sector::GetTrackSize() const GCM_NOEXCEPT {
        if GCM_UNLIKELY (!IsValid()) {
            return 0xFFFFFFFF;
        }

        return GCM_BIG_TO_SYSTEM_ENDIAN(*(u32*)(m_data.data() + 0x14));
    }

    void BI2Sector::SetArgumentOffset(u32 _val) GCM_NOEXCEPT {
        if GCM_UNLIKELY (!IsValid()) {
            return;
        }

        *(u32*)(m_data.data() + 0xC) = GCM_SYSTEM_TO_BIG_ENDIAN(_val);
    }

    void BI2Sector::SetDebugMonitorSize(u32 _val) GCM_NOEXCEPT {
        if GCM_UNLIKELY (!IsValid()) {
            return;
        }

        *(u32*)(m_data.data()) = GCM_SYSTEM_TO_BIG_ENDIAN(_val);
    }

    void BI2Sector::SetDebugFlag(u32 _val) GCM_NOEXCEPT {
        if GCM_UNLIKELY (!IsValid()) {
            return;
        }

        *(u32*)(m_data.data() + 0x8) = GCM_SYSTEM_TO_BIG_ENDIAN(_val);
    }

    void BI2Sector::SetRegion(ERegion _region) GCM_NOEXCEPT {
        if GCM_UNLIKELY (!IsValid()) {
            return;
        }

        *(u32*)(m_data.data() + 0x18) = GCM_SYSTEM_TO_BIG_ENDIAN((u32)_region);
    }

    void BI2Sector::SetSimulatedMemSize(u32 _val) GCM_NOEXCEPT {
        if GCM_UNLIKELY (!IsValid()) {
            return;
        }

        *(u32*)(m_data.data() + 0x4) = GCM_SYSTEM_TO_BIG_ENDIAN(_val);
    }

    void BI2Sector::SetTrackLocation(u32 _val) GCM_NOEXCEPT {
        if GCM_UNLIKELY (!IsValid()) {
            return;
        }

        *(u32*)(m_data.data() + 0x10) = GCM_SYSTEM_TO_BIG_ENDIAN(_val);
    }

    void BI2Sector::SetTrackSize(u32 _val) GCM_NOEXCEPT {
        if GCM_UNLIKELY (!IsValid()) {
            return;
        }

        *(u32*)(m_data.data() + 0x14) = GCM_SYSTEM_TO_BIG_ENDIAN(_val);
    }

    BootSector::BootSector() {
        m_data.fill('\0');
    }

    GCM_NODISCARD std::unique_ptr<BootSector> BootSector::FromData(ByteView _data) {
        GCM_RUNTIME_ASSERT((u32)_data.size() == BootSector::FORMAT_SIZE, "Provided Boot data does not match the Boot format size!");

        std::unique_ptr<BootSector> new_bi2 = std::make_unique<BootSector>();
        memcpy(new_bi2->m_data.data(), _data.data(), _data.size());

        return new_bi2;
    }

    GCM_NODISCARD std::unique_ptr<BootSector> BootSector::FromFile(const std::string& _data) {
        std::ifstream in_file(_data, std::ios::in | std::ios::binary);
        if (!in_file) {
            return nullptr;
        }

        in_file.seekg(0, std::ios::end);
        GCM_RUNTIME_ASSERT((u32)in_file.tellg() == BootSector::FORMAT_SIZE, "Provided Boot data does not match the Boot format size!");

        in_file.seekg(0, std::ios::beg);
        std::unique_ptr<BootSector> new_bi2 = std::make_unique<BootSector>();
        in_file.read((char*)new_bi2->m_data.data(), new_bi2->m_data.size());

        return new_bi2;
    }

    GCM_NODISCARD bool BootSector::ToData(std::vector<u8>& _out) const {
        if GCM_UNLIKELY (!IsValid()) {
            return false;
        }

        _out.resize(m_data.size());
        memcpy(_out.data(), m_data.data(), m_data.size());
        return true;
    }

    GCM_NODISCARD bool BootSector::ToFile(const std::string& _path) const {
        if GCM_UNLIKELY (!IsValid()) {
            return false;
        }

        std::ofstream out_file(_path, std::ios::out | std::ios::binary);
        if (!out_file) {
            return false;
        }

        out_file.write((const char*)m_data.data(), m_data.size());
        return true;
    }

    GCM_NODISCARD bool BootSector::IsValid() const GCM_NOEXCEPT {
        return (u32)m_data.size() == BootSector::FORMAT_SIZE;
    }

    GCM_NODISCARD u8 BootSector::GetAudioStreamBufferSize() const GCM_NOEXCEPT {
        if GCM_UNLIKELY (!IsValid()) {
            return 0xFF;
        }

        return *(u8*)(m_data.data() + 0x9);
    }

    GCM_NODISCARD bool BootSector::GetAudioStreamToggle() const GCM_NOEXCEPT {
        if GCM_UNLIKELY (!IsValid()) {
            return false;
        }

        return *(bool*)(m_data.data() + 0x8);
    }

    GCM_NODISCARD u32 BootSector::GetDebugMonitorOffset() const GCM_NOEXCEPT {
        if GCM_UNLIKELY (!IsValid()) {
            return 0xFFFFFFFF;
        }

        return GCM_BIG_TO_SYSTEM_ENDIAN(*(u32*)(m_data.data() + 0x400));
    }

    GCM_NODISCARD u32 BootSector::GetDebugMonitorVirtualAddress() const GCM_NOEXCEPT {
        if GCM_UNLIKELY (!IsValid()) {
            return 0xFFFFFFFF;
        }

        return GCM_BIG_TO_SYSTEM_ENDIAN(*(u32*)(m_data.data() + 0x404));
    }

    GCM_NODISCARD u8 BootSector::GetDiskID() const GCM_NOEXCEPT {
        if GCM_UNLIKELY (!IsValid()) {
            return 0xFF;
        }

        return *(u8*)(m_data.data() + 0x6);
    }

    GCM_NODISCARD u8 BootSector::GetDiskVersion() const GCM_NOEXCEPT {
        if GCM_UNLIKELY (!IsValid()) {
            return 0xFF;
        }

        return *(u8*)(m_data.data() + 0x7);
    }

    GCM_NODISCARD u32 BootSector::GetFirstFileOffset() const GCM_NOEXCEPT {
        if GCM_UNLIKELY (!IsValid()) {
            return 0xFFFFFFFF;
        }

        return GCM_BIG_TO_SYSTEM_ENDIAN(*(u32*)(m_data.data() + 0x434));
    }

    GCM_NODISCARD u32 BootSector::GetFSTCapacity() const GCM_NOEXCEPT {
        if GCM_UNLIKELY (!IsValid()) {
            return 0xFFFFFFFF;
        }

        return GCM_BIG_TO_SYSTEM_ENDIAN(*(u32*)(m_data.data() + 0x42C));
    }

    GCM_NODISCARD u32 BootSector::GetFSTOffset() const GCM_NOEXCEPT {
        if GCM_UNLIKELY (!IsValid()) {
            return 0xFFFFFFFF;
        }

        return GCM_BIG_TO_SYSTEM_ENDIAN(*(u32*)(m_data.data() + 0x424));
    }

    GCM_NODISCARD u32 BootSector::GetFSTSize() const GCM_NOEXCEPT {
        if GCM_UNLIKELY (!IsValid()) {
            return 0xFFFFFFFF;
        }

        return GCM_BIG_TO_SYSTEM_ENDIAN(*(u32*)(m_data.data() + 0x428));
    }

    GCM_NODISCARD u32 BootSector::GetGameCode() const GCM_NOEXCEPT {
        if GCM_UNLIKELY (!IsValid()) {
            return 0xFFFFFFFF;
        }

        return GCM_BIG_TO_SYSTEM_ENDIAN(*(u32*)(m_data.data()));
    }

    GCM_NODISCARD std::string BootSector::GetGameName() const GCM_NOEXCEPT {
        if GCM_UNLIKELY (!IsValid()) {
            return "";
        }
        const char* str_ptr = (const char*)(m_data.data() + 0x20);
        size_t str_len = strnlen(str_ptr, 0x3E0);
        return std::string(str_ptr, str_len);
    }

    GCM_NODISCARD u32 BootSector::GetMainDOLOffset() const GCM_NOEXCEPT {
        if GCM_UNLIKELY (!IsValid()) {
            return 0xFFFFFFFF;
        }

        return GCM_BIG_TO_SYSTEM_ENDIAN(*(u32*)(m_data.data() + 0x420));
    }

    GCM_NODISCARD u16 BootSector::GetMakerCode() const GCM_NOEXCEPT {
        if GCM_UNLIKELY (!IsValid()) {
            return 0xFFFF;
        }

        return GCM_BIG_TO_SYSTEM_ENDIAN(*(u16*)(m_data.data() + 0x4));
    }

    GCM_NODISCARD EConsole BootSector::GetTargetConsole() const GCM_NOEXCEPT {
        if GCM_UNLIKELY (!IsValid()) {
            return EConsole::UNKNOWN;
        }

        u32 _val = GCM_BIG_TO_SYSTEM_ENDIAN(*(u32*)(m_data.data() + 0x1C));
        switch (_val) {
        case MAGIC_GAMECUBE:
            return EConsole::GCN;
        case MAGIC_WII:
            return EConsole::WII;
        default:
            return EConsole::UNKNOWN;
        }
    }

    GCM_NODISCARD u32 BootSector::GetVirtualAddress() const GCM_NOEXCEPT {
        if GCM_UNLIKELY (!IsValid()) {
            return 0xFFFFFFFF;
        }

        return GCM_BIG_TO_SYSTEM_ENDIAN(*(u32*)(m_data.data() + 0x430));
    }

    void BootSector::SetAudioStreamBufferSize(u8 _val) GCM_NOEXCEPT {
        if GCM_UNLIKELY (!IsValid()) {
            return;
        }

        *(u8*)(m_data.data() + 0x9) = _val;
    }

    void BootSector::SetAudioStreamToggle(bool _val) GCM_NOEXCEPT {
        if GCM_UNLIKELY (!IsValid()) {
            return;
        }

        *(bool*)(m_data.data() + 0x8) = _val;
    }

    void BootSector::SetDebugMonitorOffset(u32 _val) GCM_NOEXCEPT {
        if GCM_UNLIKELY (!IsValid()) {
            return;
        }

        *(u32*)(m_data.data() + 0x400) = GCM_SYSTEM_TO_BIG_ENDIAN(_val);
    }

    void BootSector::SetDebugMonitorVirtualAddress(u32 _val) GCM_NOEXCEPT {
        if GCM_UNLIKELY (!IsValid()) {
            return;
        }

        *(u32*)(m_data.data() + 0x404) = GCM_SYSTEM_TO_BIG_ENDIAN(_val);
    }

    void BootSector::SetDiskID(u8 _val) GCM_NOEXCEPT {
        if GCM_UNLIKELY (!IsValid()) {
            return;
        }

        *(u8*)(m_data.data() + 0x6) = _val;
    }

    void BootSector::SetDiskVersion(u8 _val) GCM_NOEXCEPT {
        if GCM_UNLIKELY (!IsValid()) {
            return;
        }

        *(u8*)(m_data.data() + 0x7) = _val;
    }

    void BootSector::SetFirstFileOffset(u32 _val) GCM_NOEXCEPT {
        if GCM_UNLIKELY (!IsValid()) {
            return;
        }

        *(u32*)(m_data.data() + 0x434) = GCM_SYSTEM_TO_BIG_ENDIAN(_val);
    }

    void BootSector::SetFSTCapacity(u32 _val) GCM_NOEXCEPT {
        if GCM_UNLIKELY (!IsValid()) {
            return;
        }

        *(u32*)(m_data.data() + 0x42C) = GCM_SYSTEM_TO_BIG_ENDIAN(_val);
    }

    void BootSector::SetFSTOffset(u32 _val) GCM_NOEXCEPT {
        if GCM_UNLIKELY (!IsValid()) {
            return;
        }

        *(u32*)(m_data.data() + 0x424) = GCM_SYSTEM_TO_BIG_ENDIAN(_val);
    }

    void BootSector::SetFSTSize(u32 _val) GCM_NOEXCEPT {
        if GCM_UNLIKELY (!IsValid()) {
            return;
        }

        *(u32*)(m_data.data() + 0x428) = GCM_SYSTEM_TO_BIG_ENDIAN(_val);
    }

    void BootSector::SetGameCode(u32 _val) GCM_NOEXCEPT {
        if GCM_UNLIKELY (!IsValid()) {
            return;
        }

        *(u32*)(m_data.data()) = GCM_SYSTEM_TO_BIG_ENDIAN(_val);
    }

    void BootSector::SetGameName(const std::string& _name) GCM_NOEXCEPT {
        if GCM_UNLIKELY (!IsValid()) {
            return;
        }

        char* str_ptr = (char*)(m_data.data() + 0x20);
        memset(str_ptr, '\0', 0x3E0);
        strncpy(str_ptr, _name.data(), std::min<size_t>(_name.size(), 0x3E0));
    }

    void BootSector::SetMainDOLOffset(u32 _val) GCM_NOEXCEPT {
        if GCM_UNLIKELY (!IsValid()) {
            return;
        }

        *(u32*)(m_data.data() + 0x420) = GCM_SYSTEM_TO_BIG_ENDIAN(_val);
    }

    void BootSector::SetMakerCode(u16 _val) GCM_NOEXCEPT {
        if GCM_UNLIKELY (!IsValid()) {
            return;
        }

        *(u16*)(m_data.data() + 0x4) = GCM_SYSTEM_TO_BIG_ENDIAN(_val);
    }

    void BootSector::SetTargetConsole(EConsole _val) GCM_NOEXCEPT {
        if GCM_UNLIKELY (!IsValid()) {
            return;
        }

        switch (_val) {
        case EConsole::GCN:
            *(u32*)(m_data.data() + 0x1C) = GCM_SYSTEM_TO_BIG_ENDIAN((u32)MAGIC_GAMECUBE);
            return;
        case EConsole::WII:
            *(u32*)(m_data.data() + 0x1C) = GCM_SYSTEM_TO_BIG_ENDIAN((u32)MAGIC_WII);
            return;
        default:
            *(u32*)(m_data.data() + 0x1C) = 0xFFFFFFFF;
            return;
        }
    }

    void BootSector::SetVirtualAddress(u32 _val) GCM_NOEXCEPT {
        if GCM_UNLIKELY (!IsValid()) {
            return;
        }

        *(u32*)(m_data.data() + 0x430) = GCM_SYSTEM_TO_BIG_ENDIAN(_val);
    }

#define _GCM_OFS_FROM_STROFS(str_ofs) (u32)(((str_ofs)[0] << 16) | ((str_ofs)[1] << 8) | ((str_ofs)[2]))
#define _GCM_OFS_TO_STROFS(ofs, str_ofs)   \
        ((str_ofs)[0] = (((ofs) >> 16) & 0xFF)); \
        ((str_ofs)[1] = (((ofs) >> 8) & 0xFF)); \
        ((str_ofs)[2] = (((ofs)) & 0xFF))

    GCM_NODISCARD static std::string GetParentPath(const std::string& path) {
        size_t sep_result = path.find_last_of("/");
        if (sep_result == std::string::npos) {
            sep_result = path.find_last_of("\\");
        }

        if (sep_result == std::string::npos) {
            return ".";
        }

        if (sep_result == 0) {
            return ".";
        }

        return path.substr(0, sep_result);
    }

    GCM_NODISCARD static std::string GetStemPath(const std::string& path) {
        size_t sep_result = path.find_last_of("/");
        if (sep_result == std::string::npos) {
            sep_result = path.find_last_of("\\");
        }

        if (sep_result == std::string::npos) {
            return path;
        }

        return path.substr(sep_result + 1);
    }

    FSTSector::FSTSector() {
        LowFileNode root_node = {};
        root_node.m_type = (u8)EntryType::DIRECTORY;
        root_node.m_str_ofs[0] = 0;
        root_node.m_str_ofs[1] = 0;
        root_node.m_str_ofs[2] = 0;
        root_node.m_parent = 0;
        root_node.m_next = GCM_SYSTEM_TO_BIG_ENDIAN(1);
        m_file_nodes.push_back(root_node);
        m_str_table.push_back('\0');
    }

    GCM_NODISCARD std::unique_ptr<FSTSector> FSTSector::FromData(ByteView _data) {
        GCM_RUNTIME_ASSERT(_data.size() >= 0xC, "Provided FST data does not contain enough data!");
        
        LowFileNode* low_nodes = (LowFileNode*)_data.data();
        u32 low_nodes_count = GCM_BIG_TO_SYSTEM_ENDIAN(low_nodes[0].m_next);

        GCM_RUNTIME_ASSERT(_data.size() >= low_nodes_count * sizeof(LowFileNode), "Provided FST data has an incomplete file table!");

        const char* str_table = (const char*)_data.data() + low_nodes_count * sizeof(LowFileNode);

        std::vector<LowFileNode> file_nodes;
        file_nodes.resize(low_nodes_count);
        memcpy(file_nodes.data(), low_nodes, low_nodes_count * sizeof(LowFileNode));

        std::vector<char> str_vec;
        str_vec.resize(_data.size() - low_nodes_count * sizeof(LowFileNode));
        memcpy(str_vec.data(), str_table, str_vec.size());

        std::unique_ptr<FSTSector> new_fst = std::make_unique<FSTSector>();
        new_fst->m_file_nodes = std::move(file_nodes);
        new_fst->m_str_table = std::move(str_vec);
        return new_fst;
    }

    GCM_NODISCARD std::unique_ptr<FSTSector> FSTSector::FromFile(const std::string& _data) {
        std::ifstream in_file(_data, std::ios::in | std::ios::binary);
        if (!in_file) {
            return nullptr;
        }

        in_file.seekg(0, std::ios::end);
        size_t length = in_file.tellg();
        in_file.seekg(0, std::ios::beg);
        std::vector<u8> fst_data;
        fst_data.resize(length);
        in_file.read((char*)fst_data.data(), fst_data.size());

        return FromData(fst_data);
    }

    GCM_NODISCARD bool FSTSector::ToData(std::vector<u8>& _out) const {
        if GCM_UNLIKELY (!IsValid()) {
            return false;
        }

        _out.resize(m_file_nodes.size() * sizeof(LowFileNode) + m_str_table.size());
        memcpy(_out.data(), m_file_nodes.data(), m_file_nodes.size() * sizeof(LowFileNode));
        memcpy(_out.data() + m_file_nodes.size() * sizeof(LowFileNode), m_str_table.data(), m_str_table.size());
        return true;
    }

    GCM_NODISCARD bool FSTSector::ToFile(const std::string& _path) const {
        if GCM_UNLIKELY (!IsValid()) {
            return false;
        }

        std::ofstream out_file(_path, std::ios::out | std::ios::binary);
        if (!out_file) {
            return false;
        }

        std::vector<u8> out_data;
        if (!ToData(out_data)) {
            return false;
        }

        out_file.write((const char*)out_data.data(), out_data.size());
        return true;
    }

    GCM_NODISCARD bool FSTSector::IsValid() const GCM_NOEXCEPT {
        if (m_file_nodes.empty()) {
            return false;
        }

        if (m_file_nodes[0].m_type != (u32)EntryType::DIRECTORY) {
            return false;
        }

        if (_GCM_OFS_FROM_STROFS(m_file_nodes[0].m_str_ofs) != 0) {
            return false;
        }

        if (m_file_nodes[0].m_parent != 0) {
            return false;
        }

        if (GCM_BIG_TO_SYSTEM_ENDIAN(m_file_nodes[0].m_next) != m_file_nodes.size()) {
            return false;
        }

        std::vector<u32> dir_stack;
        dir_stack.reserve(m_file_nodes.size());
        dir_stack.push_back(0);

        for (u32 i = 1; i < (u32)m_file_nodes.size(); ++i) {
            if (dir_stack.empty()) {
                return false;
            }

            while (!dir_stack.empty() && i >= GCM_BIG_TO_SYSTEM_ENDIAN(m_file_nodes[dir_stack.back()].m_next)) {
                dir_stack.pop_back();
            }

            const LowFileNode& low_node = m_file_nodes[i];
            if (low_node.m_type > 1) {
                return false;
            }

            const u32 str_ofs = _GCM_OFS_FROM_STROFS(low_node.m_str_ofs);
            if (str_ofs >= m_str_table.size() - 2) {
                return false;
            }

            if (m_str_table.at(str_ofs) == 0) {
                return false;
            }

            switch (EntryType(low_node.m_type)) {
            case EntryType::FILE: {
                const u32 low_pos = GCM_BIG_TO_SYSTEM_ENDIAN(low_node.m_position);
                const u32 low_size = GCM_BIG_TO_SYSTEM_ENDIAN(low_node.m_size);
                if (low_pos < m_entry_pos_min || m_entry_pos_max <= low_pos + low_size) {
                    return false;
                }
                break;
            }
            case EntryType::DIRECTORY: {
                const u32 low_parent = GCM_BIG_TO_SYSTEM_ENDIAN(low_node.m_parent);
                const u32 low_next = GCM_BIG_TO_SYSTEM_ENDIAN(low_node.m_next);
                if (low_parent != dir_stack.back()) {
                    return false;
                }

                if (low_next > m_file_nodes.size()) {
                    return false;
                }

                dir_stack.emplace_back(i);
                break;
            }
            case EntryType::UNKNOWN: {
                return false;
            }
            }
        }

        return true;
    }

    GCM_NODISCARD u32 FSTSector::GetEntryPositionMin() const GCM_NOEXCEPT { return m_entry_pos_min; }
    GCM_NODISCARD u32 FSTSector::GetEntryPositionMax() const GCM_NOEXCEPT { return m_entry_pos_max; }
    void FSTSector::SetEntryPositionMin(u32 min) GCM_NOEXCEPT { m_entry_pos_min = min; }
    void FSTSector::SetEntryPositionMax(u32 max) GCM_NOEXCEPT { m_entry_pos_max = max; }

    GCM_NODISCARD u32 FSTSector::GetEntryNum(u32 cwd_entrynum, const std::string& path) const GCM_NOEXCEPT {
        if GCM_UNLIKELY (!IsValid()) {
            return FSTSector::INVALID_ENTRYNUM;
        }

        if (cwd_entrynum >= m_file_nodes.size()) {
            return FSTSector::INVALID_ENTRYNUM;
        }

        u32 the_entrynum = cwd_entrynum;

        std::string proc_path = path;
        if (path[0] == '/' || path[0] == '\\') {
            the_entrynum = GetRootEntryNum();
            proc_path = proc_path.substr(1);
        } else if (path[0] == '.') {
            if (path[1] == '/' || path[1] == '\\') {
                proc_path = proc_path.substr(2);
            } else {
                proc_path = proc_path.substr(1);
            }
        }

        if (proc_path.empty()) {
            return cwd_entrynum;
        }

        size_t sep_pos = 0;
        while (true) {
            size_t sep_result = proc_path.find("/", sep_pos);
            if (sep_result == std::string::npos) {
                sep_result = proc_path.find("\\", sep_pos);
            }

            std::string this_name = proc_path.substr(sep_pos, sep_result - sep_pos);
            if (this_name.empty()) {
                return the_entrynum != cwd_entrynum ? the_entrynum : FSTSector::INVALID_ENTRYNUM;
            }

            bool any_matched = false;
            for (u32 i = GetFirst(the_entrynum); i <= GetLast(the_entrynum); i = GetNext(i)) {
                std::string file_name = std::string(m_str_table.data() + _GCM_OFS_FROM_STROFS(m_file_nodes[i].m_str_ofs));
                if (file_name == this_name) {
                    the_entrynum = i;
                    if (sep_result == std::string::npos) {
                        return the_entrynum;
                    }
                    sep_pos = sep_result + 1;
                    any_matched = true;
                    break;
                }
            }

            if (!any_matched) {
                return FSTSector::INVALID_ENTRYNUM;
            }
        }

        // Unreachable
        return FSTSector::INVALID_ENTRYNUM;
    }

    GCM_NODISCARD std::string FSTSector::GetEntryPath(u32 cwd_entrynum, u32 entrynum) const GCM_NOEXCEPT {
        if GCM_UNLIKELY (!IsValid()) {
            return "";
        }

        if GCM_UNLIKELY (entrynum < cwd_entrynum) {
            return "";
        }

        if GCM_UNLIKELY (entrynum >= m_file_nodes.size()) {
            return "";
        }

        if GCM_UNLIKELY (cwd_entrynum >= m_file_nodes.size()) {
            return "";
        }

        if (entrynum >= m_file_nodes[cwd_entrynum].m_next) {
            return "";
        }

        if (entrynum == cwd_entrynum) {
            return ".";
        }

        std::string path = m_str_table.data() + _GCM_OFS_FROM_STROFS(m_file_nodes[entrynum].m_str_ofs);

        while (m_file_nodes[entrynum].m_type == (u32)EntryType::FILE) {
            --entrynum;
        }

        const u32 root_entry = GetRootEntryNum();
        while (entrynum > cwd_entrynum) {
            path = std::string(m_str_table.data() + _GCM_OFS_FROM_STROFS(m_file_nodes[entrynum].m_str_ofs)) + "/" + path;
            entrynum = m_file_nodes[entrynum].m_parent;
        }

        return (cwd_entrynum != root_entry ? "./" : "/") + path;
    }

    GCM_NODISCARD u32 FSTSector::GetRootEntryNum() const GCM_NOEXCEPT {
        if GCM_UNLIKELY (!IsValid()) {
            return FSTSector::INVALID_ENTRYNUM;
        }
        return 0;
    }

    GCM_NODISCARD u32 FSTSector::GetEntryParent(u32 entrynum) const GCM_NOEXCEPT {
        if GCM_UNLIKELY (!IsValid()) {
            return FSTSector::INVALID_ENTRYNUM;
        }

        if GCM_UNLIKELY (entrynum >= m_file_nodes.size()) {
            return FSTSector::INVALID_ENTRYNUM;
        }

        if (m_file_nodes[entrynum].m_type == (u32)EntryType::DIRECTORY) {
            return m_file_nodes[entrynum].m_parent;
        }

        u32 search_entry = entrynum;
        while (true) {
            while (m_file_nodes[search_entry].m_type == (u32)EntryType::FILE) {
                --search_entry;
            }
            if (m_file_nodes[search_entry].m_next > entrynum) {
                break;
            }
        }

        return search_entry;
    }

    GCM_NODISCARD FSTSector::EntryType FSTSector::GetEntryType(u32 entrynum) const GCM_NOEXCEPT {
        if GCM_UNLIKELY (!IsValid()) {
            return EntryType::UNKNOWN;
        }

        if GCM_UNLIKELY (entrynum >= m_file_nodes.size()) {
            return EntryType::UNKNOWN;
        }

        return static_cast<EntryType>(m_file_nodes[entrynum].m_type);
    }

    GCM_NODISCARD u32 FSTSector::GetEntryPosition(u32 entrynum) const GCM_NOEXCEPT {
        if GCM_UNLIKELY (!IsValid()) {
            return FSTSector::INVALID_ENTRYNUM;
        }

        if GCM_UNLIKELY (entrynum >= m_file_nodes.size()) {
            return FSTSector::INVALID_ENTRYNUM;
        }

        switch (static_cast<EntryType>(m_file_nodes[entrynum].m_type)) {
        case EntryType::FILE:
            return m_file_nodes[entrynum].m_position;
        case EntryType::DIRECTORY:
            return entrynum - m_file_nodes[entrynum].m_parent;
        case EntryType::UNKNOWN:
            return FSTSector::INVALID_ENTRYNUM;
        }

        return FSTSector::INVALID_ENTRYNUM;
    }

    GCM_NODISCARD u32 FSTSector::GetEntrySize(u32 entrynum) const GCM_NOEXCEPT {
        if GCM_UNLIKELY (!IsValid()) {
            return FSTSector::INVALID_ENTRYNUM;
        }

        if GCM_UNLIKELY (entrynum >= m_file_nodes.size()) {
            return FSTSector::INVALID_ENTRYNUM;
        }

        switch (static_cast<EntryType>(m_file_nodes[entrynum].m_type)) {
        case EntryType::FILE:
            return GCM_BIG_TO_SYSTEM_ENDIAN(m_file_nodes[entrynum].m_size);
        case EntryType::DIRECTORY:
            return GCM_BIG_TO_SYSTEM_ENDIAN(m_file_nodes[entrynum].m_next) - entrynum;
        case EntryType::UNKNOWN:
            return FSTSector::INVALID_ENTRYNUM;
        }

        return FSTSector::INVALID_ENTRYNUM;
    }

    GCM_NODISCARD u32 FSTSector::GetFirst(u32 entrynum) const GCM_NOEXCEPT {
        if GCM_UNLIKELY (!IsValid()) {
            return FSTSector::INVALID_ENTRYNUM;
        }

        if GCM_UNLIKELY (entrynum >= m_file_nodes.size()) { 
            return FSTSector::INVALID_ENTRYNUM;
        }

        if GCM_UNLIKELY(m_file_nodes[entrynum].m_type != (u32)EntryType::DIRECTORY) {
            return FSTSector::INVALID_ENTRYNUM;
        }

        return entrynum + 1;
    }

    GCM_NODISCARD u32 FSTSector::GetLast(u32 entrynum) const GCM_NOEXCEPT {
        if GCM_UNLIKELY (!IsValid()) {
            return FSTSector::INVALID_ENTRYNUM;
        }

        if GCM_UNLIKELY (entrynum >= m_file_nodes.size()) {
            return FSTSector::INVALID_ENTRYNUM;
        }

        if GCM_UNLIKELY (m_file_nodes[entrynum].m_type != (u32)EntryType::DIRECTORY) {
            return FSTSector::INVALID_ENTRYNUM;
        }

        return GCM_BIG_TO_SYSTEM_ENDIAN(m_file_nodes[entrynum].m_next) - 1;
    }

    GCM_NODISCARD u32 FSTSector::GetNext(u32 entrynum, bool recursive) const GCM_NOEXCEPT {
        if GCM_UNLIKELY(!IsValid()) {
            return FSTSector::INVALID_ENTRYNUM;
        }

        if GCM_UNLIKELY (entrynum >= m_file_nodes.size() - 1) {
            return FSTSector::INVALID_ENTRYNUM;
        }

        if (m_file_nodes[entrynum].m_type == (u32)EntryType::DIRECTORY) {
            if (recursive) {
                return entrynum + 1;
            }
            return GCM_BIG_TO_SYSTEM_ENDIAN(m_file_nodes[entrynum].m_next);
        }

        return entrynum + 1;
    }

    GCM_NODISCARD u32 FSTSector::GetPrev(u32 entrynum, bool recursive) const GCM_NOEXCEPT {
        if GCM_UNLIKELY (!IsValid()) {
            return FSTSector::INVALID_ENTRYNUM;
        }

        if GCM_UNLIKELY (entrynum >= m_file_nodes.size()) {
            return FSTSector::INVALID_ENTRYNUM;
        }

        if (recursive) {
            return entrynum - 1;
        }

        const u32 parent_entry = GetEntryParent(entrynum);
        u32 the_entry = GetFirst(parent_entry);
        if (the_entry >= entrynum) {
            return the_entry;
        }

        while (true) {
            u32 next_entry = GetNext(the_entry);
            if (next_entry >= entrynum) {
                return the_entry;
            }
            the_entry = next_entry;
        }

        return the_entry;
    }

    void FSTSector::SetEntryPosition(u32 entrynum, u32 position) GCM_NOEXCEPT {
        if GCM_UNLIKELY (!IsValid()) {
            return;
        }

        if GCM_UNLIKELY (entrynum >= m_file_nodes.size()) {
            return;
        }

        if ((EntryType)m_file_nodes[entrynum].m_type != EntryType::FILE) {
            return;
        }

        m_file_nodes[entrynum].m_position = GCM_SYSTEM_TO_BIG_ENDIAN(position);
    }

    void FSTSector::SetEntrySize(u32 entrynum, u32 size) GCM_NOEXCEPT {
        if GCM_UNLIKELY (!IsValid()) {
            return;
        }

        if GCM_UNLIKELY (entrynum >= m_file_nodes.size()) {
            return;
        }

        if ((EntryType)m_file_nodes[entrynum].m_type != EntryType::FILE) {
            return;
        }

        m_file_nodes[entrynum].m_size = GCM_SYSTEM_TO_BIG_ENDIAN(size);
    }

    GCM_NODISCARD u32 FSTSector::CreateEntry(u32 cwd_entrynum, EntryType type, const std::string& name) GCM_NOEXCEPT {
        if GCM_UNLIKELY (!IsValid()) {
            return FSTSector::INVALID_ENTRYNUM;
        }

        if GCM_UNLIKELY (cwd_entrynum >= m_file_nodes.size()) {
            return FSTSector::INVALID_ENTRYNUM;
        }

        std::string proc_name = GetStemPath(name);
        std::string parent_path = GetParentPath(name);  // Just in case nested paths are specified.
        if (!parent_path.empty() && parent_path != ".") {
            cwd_entrynum = GetEntryNum(cwd_entrynum, parent_path);
            if (cwd_entrynum == FSTSector::INVALID_ENTRYNUM) {
                return FSTSector::INVALID_ENTRYNUM;
            }
        }

        const u32 original_count = GCM_BIG_TO_SYSTEM_ENDIAN(m_file_nodes[0].m_next);
        u32 insert_entry = FSTSector::INVALID_ENTRYNUM;

        {
            const LowFileNode& dir_node = m_file_nodes[cwd_entrynum];
            for (u32 i = GetFirst(cwd_entrynum); i <= GetLast(cwd_entrynum); i = GetNext(i)) {
                const char* node_name = m_str_table.data() + _GCM_OFS_FROM_STROFS(m_file_nodes[i].m_str_ofs);
                int name_diff = strncmp(proc_name.data(), node_name, proc_name.size());
                if (name_diff > 0) {
                    continue;
                }
                if GCM_UNLIKELY (name_diff == 0) {
                    if (type != (EntryType)m_file_nodes[i].m_type) {
                        return FSTSector::INVALID_ENTRYNUM;
                    }
                    return i;
                }
                insert_entry = i;
                break;
            }
        }

        if (insert_entry == FSTSector::INVALID_ENTRYNUM) {
            return FSTSector::INVALID_ENTRYNUM;
        }

        {
            LowFileNode new_node = {};
            new_node.m_type = (u32)type;
            switch (type) {
            default:
            case EntryType::FILE:
                new_node.m_position = 0;
                new_node.m_size = 0;
                break;
            case EntryType::DIRECTORY:
                new_node.m_parent = GCM_SYSTEM_TO_BIG_ENDIAN(cwd_entrynum);
                new_node.m_next = GCM_SYSTEM_TO_BIG_ENDIAN(insert_entry); // Gets +1 later
                break;
            }
            new_node.m_str_ofs[0] = m_file_nodes[insert_entry].m_str_ofs[0];
            new_node.m_str_ofs[1] = m_file_nodes[insert_entry].m_str_ofs[1];
            new_node.m_str_ofs[2] = m_file_nodes[insert_entry].m_str_ofs[2];
            m_file_nodes.insert(m_file_nodes.begin() + insert_entry, std::move(new_node));

            u32 node_name_ofs = _GCM_OFS_FROM_STROFS(new_node.m_str_ofs);
            m_str_table.insert(m_str_table.begin() + node_name_ofs, proc_name.size() + 1, '\0');
            memcpy(m_str_table.data() + node_name_ofs, proc_name.data(), proc_name.size());
        }

        for (s64 i = (s64)m_file_nodes.size() - 1; i >= 0; --i) {
            LowFileNode& the_node = m_file_nodes[i];
            switch (static_cast<EntryType>(the_node.m_type)) {
            case EntryType::FILE: {
                if (i != insert_entry && i > 0) {
                    const u32 node_name_ofs = _GCM_OFS_FROM_STROFS(the_node.m_str_ofs);
                    if (node_name_ofs >= _GCM_OFS_FROM_STROFS(m_file_nodes[insert_entry].m_str_ofs)) {
                        _GCM_OFS_TO_STROFS(node_name_ofs + proc_name.size() + 1, the_node.m_str_ofs);
                    }
                }
                break;
            }
            case EntryType::DIRECTORY: {
                if (GCM_BIG_TO_SYSTEM_ENDIAN(the_node.m_next) >= insert_entry) {
                    the_node.m_next += GCM_SYSTEM_TO_BIG_ENDIAN(1);
                }
                if (GCM_BIG_TO_SYSTEM_ENDIAN(the_node.m_parent) >= insert_entry) {
                    the_node.m_parent += GCM_SYSTEM_TO_BIG_ENDIAN(1);
                }
                if (i != insert_entry && i > 0) {
                    const u32 node_name_ofs = _GCM_OFS_FROM_STROFS(the_node.m_str_ofs);
                    if (node_name_ofs >= _GCM_OFS_FROM_STROFS(m_file_nodes[insert_entry].m_str_ofs)) {
                        _GCM_OFS_TO_STROFS(node_name_ofs + proc_name.size() + 1, the_node.m_str_ofs);
                    }
                }
                break;
            }
            case EntryType::UNKNOWN: {
                break;
            }
            }
        }

        return insert_entry;
    }

    GCM_NODISCARD bool FSTSector::RemoveEntry(u32 entrynum, bool recursive) GCM_NOEXCEPT {
        if GCM_UNLIKELY (!IsValid()) {
            return false;
        }

        if GCM_UNLIKELY (entrynum >= m_file_nodes.size()) {
            return false;
        }

        auto entry_it = m_file_nodes.begin() + entrynum;
        const u32 name_ofs_start = _GCM_OFS_FROM_STROFS(entry_it->m_str_ofs);

        u32 erase_size = 1;
        u32 erase_str_size = 0;
        if (entry_it->m_type == (u32)EntryType::DIRECTORY) {
            if (recursive) {
                auto start_it = m_file_nodes.begin() + entrynum;
                auto end_it = m_file_nodes.begin() + GCM_BIG_TO_SYSTEM_ENDIAN(start_it->m_next);
                erase_size = (u32)std::distance(start_it, end_it);
                erase_str_size = std::accumulate(start_it, end_it, (u32)0, [&](u32 accum, const LowFileNode& node) {
                    return accum + (u32)strnlen(m_str_table.data() + _GCM_OFS_FROM_STROFS(node.m_str_ofs), 128) + 1;
                    });
                m_file_nodes.erase(start_it, end_it);
            } else if (GCM_BIG_TO_SYSTEM_ENDIAN(entry_it->m_next) != entrynum + 1) {
                return false;
            } else {
                erase_str_size = (u32)strnlen(m_str_table.data() + _GCM_OFS_FROM_STROFS(entry_it->m_str_ofs), 128) + 1;
                m_file_nodes.erase(entry_it);
            }
        } else {
            erase_str_size = (u32)strnlen(m_str_table.data() + _GCM_OFS_FROM_STROFS(entry_it->m_str_ofs), 128) + 1;
            m_file_nodes.erase(entry_it);
        }

        m_str_table.erase(m_str_table.begin() + name_ofs_start, m_str_table.begin() + name_ofs_start + erase_str_size);

        for (s64 i = (s64)m_file_nodes.size() - 1; i >= 0; --i) {
            LowFileNode& the_node = m_file_nodes[i];
            switch (static_cast<EntryType>(the_node.m_type)) {
            case EntryType::FILE: {
                if (i > 0) {
                    const u32 node_name_ofs = _GCM_OFS_FROM_STROFS(the_node.m_str_ofs);
                    if (node_name_ofs >= name_ofs_start) {
                        _GCM_OFS_TO_STROFS(node_name_ofs - erase_str_size, the_node.m_str_ofs);
                    }
                }
                break;
            }
            case EntryType::DIRECTORY: {
                if (GCM_BIG_TO_SYSTEM_ENDIAN(the_node.m_next) > entrynum) {
                    the_node.m_next -= GCM_SYSTEM_TO_BIG_ENDIAN(erase_size);
                }
                if (GCM_BIG_TO_SYSTEM_ENDIAN(the_node.m_parent) > entrynum) {
                    the_node.m_parent -= GCM_SYSTEM_TO_BIG_ENDIAN(erase_size);
                }
                if (i > 0) {
                    const u32 node_name_ofs = _GCM_OFS_FROM_STROFS(the_node.m_str_ofs);
                    if (node_name_ofs >= name_ofs_start) {
                        _GCM_OFS_TO_STROFS(node_name_ofs - erase_str_size, the_node.m_str_ofs);
                    }
                }
                break;
            }
            case EntryType::UNKNOWN: {
                break;
            }
            }
        }

        return true;
    }

    GCM_NODISCARD bool FSTSector::RecalculatePositions(const std::vector<FileRuleset>& rulesets) GCM_NOEXCEPT {
        u32 end_boundary = m_entry_pos_max;
        for (s64 i = (s64)m_file_nodes.size() - 1; i >= 0; --i) {
            LowFileNode& low_node = m_file_nodes[i];
            if (low_node.m_type != (u32)EntryType::FILE) {
                continue;
            }

            u32 file_alignment = 4;
            const u32 file_size = GCM_BIG_TO_SYSTEM_ENDIAN(low_node.m_size);

            const std::string file_name = m_str_table.data() + _GCM_OFS_FROM_STROFS(low_node.m_str_ofs);
            size_t ext_pos = file_name.find_last_of(".");
            if (ext_pos != std::string::npos) {
                const std::string extension = file_name.substr(ext_pos);
                auto rule_it = std::find_if(rulesets.begin(), rulesets.end(), [&](const FileRuleset& ruleset) {
                    return strncmp(extension.c_str(), ruleset.m_extension, sizeof(ruleset.m_extension)) == 0;
                });
                if (rule_it != rulesets.end()) {
                    file_alignment = rule_it->m_alignment;
                }
            }

            const u32 file_pos = RoundDown(end_boundary - file_size, file_alignment);
            if (file_pos < m_entry_pos_min) {
                return false;
            }

            low_node.m_position = GCM_SYSTEM_TO_BIG_ENDIAN(file_pos);
            end_boundary = file_pos;
        }
        return true;
    }

#undef _GCM_OFS_FROM_STROFS
#undef _GCM_OFS_TO_STROFS

}