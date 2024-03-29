#pragma once

#include "core/memory.hpp"
#include "objlib/meta/member.hpp"
#include "objlib/object.hpp"

#include <array>
#include <imgui.h>
#include <vector>

namespace Toolbox::UI {

    class IProperty {
    public:
        IProperty(RefPtr<Object::MetaMember> prop) : m_member(prop) {}
        virtual ~IProperty() = default;

        virtual RefPtr<Object::MetaMember> member() { return m_member; }
        virtual void init()                    = 0;
        virtual bool render(float label_width) = 0;

        ImVec2 labelSize();

    protected:
        RefPtr<Object::MetaMember> m_member;
        std::vector<char> m_array_open = {};
    };

    class BoolProperty : public IProperty {
    public:
        BoolProperty(RefPtr<Object::MetaMember> prop) : IProperty(prop) {}
        ~BoolProperty() override = default;

        void init() override;
        bool render(float label_width) override;

    private:
        std::vector<char> m_bools;
        bool m_open = false;
    };

    class NumberProperty : public IProperty {
    public:
        NumberProperty(RefPtr<Object::MetaMember> prop)
            : IProperty(prop), m_min(0), m_max(0) {}
        ~NumberProperty() override = default;

        void init() override;
        bool render(float label_width) override;

        std::vector<s64> m_numbers;

    private:
        s64 m_min;
        s64 m_max;
        s64 m_step      = 1;
        s64 m_step_fast = 10;
        bool m_open     = false;
    };

    class FloatProperty : public IProperty {
    public:
        FloatProperty(RefPtr<Object::MetaMember> prop)
            : IProperty(prop), m_min(0), m_max(0) {}
        ~FloatProperty() override = default;

        void init() override;
        bool render(float label_width) override;

    private:
        std::vector<f32> m_numbers;
        f32 m_min;
        f32 m_max;
        f32 m_step      = 1.0f;
        f32 m_step_fast = 10.0f;
        bool m_open     = false;
    };

    class DoubleProperty : public IProperty {
    public:
        DoubleProperty(RefPtr<Object::MetaMember> prop)
            : IProperty(prop), m_min(0), m_max(0) {}
        ~DoubleProperty() override = default;

        void init() override;
        bool render(float label_width) override;

    private:
        std::vector<f64> m_numbers;
        f64 m_min;
        f64 m_max;
        f32 m_step      = 1.0f;
        f32 m_step_fast = 10.0f;
        bool m_open     = false;
    };

    class StringProperty : public IProperty {
    public:
        StringProperty(RefPtr<Object::MetaMember> prop) : IProperty(prop) {}
        ~StringProperty() override = default;

        void init() override;
        bool render(float label_width) override;

    private:
        std::vector<std::array<char, 512>> m_strings;
        bool m_open = false;
    };

    class ColorProperty : public IProperty {
    public:
        ColorProperty(RefPtr<Object::MetaMember> prop) : IProperty(prop) {}
        ~ColorProperty() override = default;

        void init() override;
        bool render(float label_width) override;

    private:
        std::vector<Color::RGBAShader> m_colors;
        bool m_open     = false;
        s64 m_step      = 1;
        s64 m_step_fast = 10;
    };

    class VectorProperty : public IProperty {
    public:
        VectorProperty(RefPtr<Object::MetaMember> prop) : IProperty(prop) {}
        ~VectorProperty() override = default;

        void init() override;
        bool render(float label_width) override;

    private:
        std::vector<glm::vec3> m_vectors;
        f32 m_min       = -FLT_MAX;
        f32 m_max       = FLT_MAX;
        f32 m_step      = 1.0f;
        f32 m_step_fast = 10.0f;
        bool m_open     = false;
    };

    class TransformProperty : public IProperty {
    public:
        TransformProperty(RefPtr<Object::MetaMember> prop) : IProperty(prop) {}
        ~TransformProperty() override = default;

        void init() override;
        bool render(float label_width) override;

    private:
        std::vector<Object::Transform> m_transforms;
        f32 m_min       = -FLT_MAX;
        f32 m_max       = FLT_MAX;
        f32 m_step      = 1.0f;
        f32 m_step_fast = 10.0f;
        bool m_open     = false;
    };

    class EnumProperty : public NumberProperty {
    public:
        EnumProperty(RefPtr<Object::MetaMember> prop) : NumberProperty(prop) {}
        ~EnumProperty() override = default;

        bool render(float label_width) override;

    private:
        std::vector<std::vector<char>> m_checked_state;
        bool m_open = false;
    };

    class StructProperty : public IProperty {
    public:
        StructProperty(RefPtr<Object::MetaMember> prop);
        ~StructProperty() override = default;

        void init() override;
        bool render(float label_width) override;

    private:
        std::vector<std::vector<ScopePtr<IProperty>>> m_children_ary;
        bool m_open = false;
    };

    ScopePtr<IProperty> createProperty(RefPtr<Object::MetaMember> prop);

}  // namespace Toolbox::UI