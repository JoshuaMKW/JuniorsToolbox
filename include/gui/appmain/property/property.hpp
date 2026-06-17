#pragma once

#include "core/memory.hpp"
#include "objlib/meta/member.hpp"
#include "objlib/meta/value.hpp"
#include "objlib/object.hpp"

#include <array>
#include <imgui.h>
#include <vector>

namespace Toolbox::UI {

    class IProperty {
    public:
        using getter_cb =
            std::function<Object::MetaValue(RefPtr<Object::MetaMember>, size_t array_idx)>;
        using setter_cb = std::function<bool(RefPtr<Object::MetaMember>, size_t array_idx,
                                             const Object::MetaValue &)>;

    public:
        virtual ~IProperty() {};

        virtual ImVec2 labelSize() const = 0;

        virtual RefPtr<Object::MetaMember> member() const = 0;
        virtual void update()                             = 0;
        virtual bool render(float label_width)            = 0;
    };

    class AbstractBaseProperty : public IProperty {
    public:
        AbstractBaseProperty(RefPtr<Object::MetaMember> prop, IProperty::getter_cb getter,
                             IProperty::setter_cb setter)
            : m_member(prop), m_value_getter(getter), m_value_setter(setter) {}
        virtual ~AbstractBaseProperty() = default;

        ImVec2 labelSize() const override;

        RefPtr<Object::MetaMember> member() const override final { return m_member; }
        void update() override final;
        bool render(float label_width) override final;

        bool getSelfOpen() const;
        void setSelfOpen(bool open);

        bool getArrayIdxOpen(size_t idx) const;
        void setArrayIdxOpen(size_t idx, bool open);

    protected:
        virtual void update_(const u32 requested_array_size) = 0;
        virtual bool render_(float label_width)              = 0;

        RefPtr<Object::MetaMember> m_member;
        getter_cb m_value_getter;
        setter_cb m_value_setter;

    private:
        mutable size_t m_last_member_size = 0;
        mutable ImVec2 m_label_size       = {};

        std::vector<char> m_array_open = {};
    };

    class BoolProperty : public AbstractBaseProperty {
    public:
        BoolProperty(RefPtr<Object::MetaMember> prop, getter_cb getter, setter_cb setter)
            : AbstractBaseProperty(prop, getter, setter) {}
        ~BoolProperty() override = default;

    protected:
        void update_(const u32 array_size) override;
        bool render_(float label_width) override;

    private:
        bool m_open = false;
    };

    class NumberProperty : public AbstractBaseProperty {
    public:
        NumberProperty(RefPtr<Object::MetaMember> prop, getter_cb getter, setter_cb setter)
            : AbstractBaseProperty(prop, getter, setter), m_min(0), m_max(0) {}
        ~NumberProperty() override = default;

    protected:
        void update_(const u32 array_size) override;
        bool render_(float label_width) override;

        s64 getS64FromMetaValue(const Object::MetaValue &value) const;
        void setS64ToMetaValue(Object::MetaValue &value, s64 number);

    private:
        s64 m_min;
        s64 m_max;
        s64 m_step      = 1;
        s64 m_step_fast = 10;
        bool m_open     = false;
    };

    class FloatProperty : public AbstractBaseProperty {
    public:
        FloatProperty(RefPtr<Object::MetaMember> prop, getter_cb getter, setter_cb setter)
            : AbstractBaseProperty(prop, getter, setter), m_min(0), m_max(0) {}
        ~FloatProperty() override = default;

    protected:
        void update_(const u32 array_size) override;
        bool render_(float label_width) override;

    private:
        f32 m_min;
        f32 m_max;
        f32 m_step      = 1.0f;
        f32 m_step_fast = 10.0f;
        bool m_open     = false;
    };

    class DoubleProperty : public AbstractBaseProperty {
    public:
        DoubleProperty(RefPtr<Object::MetaMember> prop, getter_cb getter, setter_cb setter)
            : AbstractBaseProperty(prop, getter, setter), m_min(0), m_max(0) {}
        ~DoubleProperty() override = default;

    protected:
        void update_(const u32 array_size) override;
        bool render_(float label_width) override;

    private:
        f64 m_min;
        f64 m_max;
        f32 m_step      = 1.0f;
        f32 m_step_fast = 10.0f;
        bool m_open     = false;
    };

    class StringProperty : public AbstractBaseProperty {
    public:
        StringProperty(RefPtr<Object::MetaMember> prop, getter_cb getter, setter_cb setter)
            : AbstractBaseProperty(prop, getter, setter) {}
        ~StringProperty() override = default;

    protected:
        void update_(const u32 array_size) override;
        bool render_(float label_width) override;

    private:
        std::vector<std::array<char, 512>> m_strings;
        bool m_open = false;
    };

    class ColorProperty : public AbstractBaseProperty {
    public:
        ColorProperty(RefPtr<Object::MetaMember> prop, getter_cb getter, setter_cb setter)
            : AbstractBaseProperty(prop, getter, setter) {}
        ~ColorProperty() override = default;

    protected:
        void update_(const u32 array_size) override;
        bool render_(float label_width) override;

        Color::RGBAShader getColor(int array_index);
        bool setColor(int array_index, const Color::RGBAShader &color);

    private:
        bool m_open     = false;
        s64 m_step      = 1;
        s64 m_step_fast = 10;
    };

    class VectorProperty : public AbstractBaseProperty {
    public:
        VectorProperty(RefPtr<Object::MetaMember> prop, getter_cb getter, setter_cb setter)
            : AbstractBaseProperty(prop, getter, setter) {}
        ~VectorProperty() override = default;

    protected:
        void update_(const u32 array_size) override;
        bool render_(float label_width) override;

    private:
        f32 m_min       = -FLT_MAX;
        f32 m_max       = FLT_MAX;
        f32 m_step      = 1.0f;
        f32 m_step_fast = 10.0f;
        bool m_open     = false;
    };

    class TransformProperty : public AbstractBaseProperty {
    public:
        TransformProperty(RefPtr<Object::MetaMember> prop, getter_cb getter, setter_cb setter)
            : AbstractBaseProperty(prop, getter, setter) {}
        ~TransformProperty() override = default;

    protected:
        void update_(const u32 array_size) override;
        bool render_(float label_width) override;

    private:
        f32 m_min       = -FLT_MAX;
        f32 m_max       = FLT_MAX;
        f32 m_step      = 1.0f;
        f32 m_step_fast = 10.0f;
        bool m_open     = false;
    };

    class EnumProperty : public NumberProperty {
    public:
        EnumProperty(RefPtr<Object::MetaMember> prop, getter_cb getter, setter_cb setter)
            : NumberProperty(prop, getter, setter) {}
        ~EnumProperty() override = default;

    protected:
        void update_(const u32 array_size) override;
        bool render_(float label_width) override;

    private:
        std::vector<std::vector<char>> m_checked_state;
        bool m_open           = false;
        bool m_update_ialized = false;
    };

    class StructProperty : public AbstractBaseProperty {
    public:
        StructProperty(RefPtr<Object::MetaMember> prop, getter_cb getter, setter_cb setter);
        ~StructProperty() override = default;

    protected:
        void update_(const u32 array_size) override;
        bool render_(float label_width) override;

    private:
        std::vector<std::vector<ScopePtr<IProperty>>> m_children_ary;
        bool m_open = false;
    };

    ScopePtr<IProperty> createProperty(RefPtr<Object::MetaMember> prop,
                                                  IProperty::getter_cb, IProperty::setter_cb);

}  // namespace Toolbox::UI