#pragma once

#include <algorithm>
#include <any>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <variant>
#include <vector>
#include <unordered_set>

#include "core/mimedata/mimedata.hpp"
#include "core/types.hpp"
#include "fsystem.hpp"
#include "image/imagehandle.hpp"
#include "unique.hpp"

namespace Toolbox {

    enum ModelSortOrder {
        SORT_ASCENDING,
        SORT_DESCENDING,
    };

    enum ModelDataRole {
        DATA_ROLE_NONE,
        DATA_ROLE_DISPLAY,
        DATA_ROLE_TOOLTIP,
        DATA_ROLE_DECORATION,
        DATA_ROLE_USER,
    };

    class ModelIndex final : public IUnique {
    public:
        friend class IDataModel;

        ModelIndex() = default;
        ModelIndex(UUID64 model_uuid) : m_model_uuid(model_uuid) {}
        ModelIndex(UUID64 model_uuid, UUID64 self_uuid)
            : m_model_uuid(model_uuid), m_uuid(self_uuid) {}
        ModelIndex(const ModelIndex &other)
            : m_uuid(other.m_uuid), m_model_uuid(other.m_model_uuid) {
            m_data.m_inline = other.m_data.m_inline;
        }

        template <typename T = void> [[nodiscard]] T *data() const {
            return reinterpret_cast<T *>(m_data.m_ptr);
        }
        void setData(void *data) { m_data.m_ptr = data; }

        // If using inline data, it is 8 bytes and overwrites the data ptr.
        [[nodiscard]] u64 inlineData() const { return m_data.m_inline; }
        // If using inline data, it is 8 bytes and overwrites the data ptr.
        void setInlineData(u64 data) const { m_data.m_inline = data; }

        [[nodiscard]] UUID64 getUUID() const override { return m_uuid; }
        [[nodiscard]] UUID64 getModelUUID() const { return m_model_uuid; }

        ModelIndex &operator=(const ModelIndex &other) {
            m_uuid          = other.m_uuid;
            m_model_uuid    = other.m_model_uuid;
            m_data.m_inline = other.m_data.m_inline;
            return *this;
        }

        [[nodiscard]] bool operator==(const ModelIndex &other) const {
            return m_uuid == other.m_uuid && m_model_uuid == other.m_model_uuid;
        }

    private:
        UUID64 m_uuid;
        UUID64 m_model_uuid = 0;

        mutable union {
            void *m_ptr;
            u64 m_inline;
        } m_data = {};
    };

    class IDataModel : public IUnique {
    public:
        [[nodiscard]] virtual bool validateIndex(const ModelIndex &index) const {
            return index.getModelUUID() == getUUID();
        }

        [[nodiscard]] virtual bool isReadOnly() const = 0;

        [[nodiscard]] virtual std::string getDisplayText(const ModelIndex &index) const {
            return std::any_cast<std::string>(getData(index, ModelDataRole::DATA_ROLE_DISPLAY));
        }

        virtual void setDisplayText(const ModelIndex &index, const std::string &text) {
            setData(index, text, ModelDataRole::DATA_ROLE_DISPLAY);
        }

        [[nodiscard]] virtual std::string getToolTip(const ModelIndex &index) const {
            return std::any_cast<std::string>(getData(index, ModelDataRole::DATA_ROLE_TOOLTIP));
        }

        virtual void setToolTip(const ModelIndex &index, const std::string &text) {
            setData(index, text, ModelDataRole::DATA_ROLE_TOOLTIP);
        }

        [[nodiscard]] virtual RefPtr<const ImageHandle>
        getDecoration(const ModelIndex &index) const {
            return std::any_cast<RefPtr<const ImageHandle>>(
                getData(index, ModelDataRole::DATA_ROLE_DECORATION));
        }

        virtual void setDecoration(const ModelIndex &index, RefPtr<const ImageHandle> decoration) {
            setData(index, decoration, ModelDataRole::DATA_ROLE_DECORATION);
        }

        [[nodiscard]] virtual std::any getData(const ModelIndex &index, int role) const      = 0;
        [[nodiscard]] virtual void setData(const ModelIndex &index, std::any data, int role) = 0;

        [[nodiscard]] virtual ModelIndex getIndex(const UUID64 &uuid) const = 0;
        [[nodiscard]] virtual ModelIndex
        getIndex(int64_t row, int64_t column, const ModelIndex &parent = ModelIndex()) const = 0;

        [[nodiscard]] virtual bool removeIndex(const ModelIndex &index) = 0;

        [[nodiscard]] virtual ModelIndex getParent(const ModelIndex &index) const  = 0;
        [[nodiscard]] virtual ModelIndex getSibling(int64_t row, int64_t column,
                                                    const ModelIndex &index) const = 0;

        [[nodiscard]] virtual size_t getColumnCount(const ModelIndex &index) const = 0;
        [[nodiscard]] virtual size_t getRowCount(const ModelIndex &index) const    = 0;

        [[nodiscard]] virtual int64_t getColumn(const ModelIndex &index) const = 0;
        [[nodiscard]] virtual int64_t getRow(const ModelIndex &index) const    = 0;

        [[nodiscard]] virtual bool hasChildren(const ModelIndex &parent = ModelIndex()) const = 0;

        [[nodiscard]] virtual ScopePtr<MimeData>
        createMimeData(const std::unordered_set<ModelIndex> &indexes) const          = 0;
        [[nodiscard]] virtual bool insertMimeData(const ModelIndex &index,
                                                  const MimeData &data)              = 0;
        [[nodiscard]] virtual std::vector<std::string> getSupportedMimeTypes() const = 0;

        [[nodiscard]] virtual bool canFetchMore(const ModelIndex &index) = 0;
        virtual void fetchMore(const ModelIndex &index)                  = 0;

        virtual void reset() = 0;

    protected:
        static void setIndexUUID(ModelIndex &index, UUID64 uuid) { index.m_uuid = uuid; }
    };

}  // namespace Toolbox

namespace std {

    template <> struct hash<Toolbox::ModelIndex> {
        Toolbox::u64 operator()(const Toolbox::ModelIndex &index) const {
            return hash<Toolbox::UUID64>()(index.getUUID());
        }
    };

}  // namespace std