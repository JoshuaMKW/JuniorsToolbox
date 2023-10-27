#pragma once

#include "objlib/meta/member.hpp"
#include "objlib/object.hpp"

#include <array>
#include <vector>
#include <imgui.h>

namespace Toolbox::UI {

    class IProperty {
    public:
        IProperty(std::shared_ptr<Object::MetaMember> prop) : m_member(prop) {}
        virtual ~IProperty() = default;

        virtual std::shared_ptr<Object::MetaMember> member() { return m_member; }
        virtual void init() = 0;
        virtual void render(float label_width) = 0;

        ImVec2 labelSize();

    protected:
        std::shared_ptr<Object::MetaMember> m_member;
    };

    class BoolProperty : public IProperty {
    public:
        BoolProperty(std::shared_ptr<Object::MetaMember> prop) : IProperty(prop) {}
        ~BoolProperty() override = default;

        void init() override;
        void render(float label_width) override;
    
    private:
        std::vector<char> m_bools;
    };

    class NumberProperty : public IProperty {
    public:
        NumberProperty(std::shared_ptr<Object::MetaMember> prop) : IProperty(prop) {}
        ~NumberProperty() override = default;

        void init() override;
        void render(float label_width) override;

        std::vector<s64> m_numbers;

    private:
        s64 m_min;
        s64 m_max;
    };

    class FloatProperty : public IProperty {
    public:
        FloatProperty(std::shared_ptr<Object::MetaMember> prop) : IProperty(prop) {}
        ~FloatProperty() override = default;

        void init() override;
        void render(float label_width) override;

    private:
        std::vector<f64> m_numbers;
        f64 m_min;
        f64 m_max;
    };

    class StringProperty : public IProperty {
    public:
        StringProperty(std::shared_ptr<Object::MetaMember> prop) : IProperty(prop) {}
        ~StringProperty() override = default;

        void init() override;
        void render(float label_width) override;

    private:
        std::vector<std::array<char, 512>> m_strings;
    };

    class ColorProperty : public IProperty {
    public:
        ColorProperty(std::shared_ptr<Object::MetaMember> prop) : IProperty(prop) {}
        ~ColorProperty() override = default;

        void init() override;
        void render(float label_width) override;

    private:
        std::vector<Color::RGBA32> m_colors;
    };

    class VectorProperty : public IProperty {
    public:
        VectorProperty(std::shared_ptr<Object::MetaMember> prop) : IProperty(prop) {}
        ~VectorProperty() override = default;

        void init() override;
        void render(float label_width) override;

    private:
        std::vector<glm::vec3> m_vectors;
    };

    class TransformProperty : public IProperty {
    public:
        TransformProperty(std::shared_ptr<Object::MetaMember> prop) : IProperty(prop) {}
        ~TransformProperty() override = default;

        void init() override;
        void render(float label_width) override;

    private:
        std::vector<Object::Transform> m_transforms;
    };

    class EnumProperty : public NumberProperty {
    public:
        EnumProperty(std::shared_ptr<Object::MetaMember> prop) : NumberProperty(prop) {}
        ~EnumProperty() override = default;

        void render(float label_width) override;

    private:
        std::vector<std::vector<char>> m_checked_state;
    };

    class StructProperty : public IProperty {
    public:
        StructProperty(std::shared_ptr<Object::MetaMember> prop);
        ~StructProperty() override = default;

        void init() override;
        void render(float label_width) override;

    private:
        std::vector<std::vector<std::unique_ptr<IProperty>>> m_children_ary;
        bool m_open = false;
    };

    std::unique_ptr<IProperty> createProperty(std::shared_ptr<Object::MetaMember> prop);

}  // namespace Toolbox::UI