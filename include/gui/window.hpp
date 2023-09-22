#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace Toolbox::UI {

    class IWindow {
    public:
        virtual ~IWindow() = default;

        virtual void render()                     = 0;
        [[nodiscard]] virtual bool isOpen() const = 0;
    };

    class SimpleWindow : public IWindow {
    public:
        SimpleWindow()                                = delete;
        SimpleWindow(const SimpleWindow &)            = delete;
        SimpleWindow &operator=(const SimpleWindow &) = delete;

        SimpleWindow(const std::string &name, const std::string &title)
            : SimpleWindow(name, title, true) {}
        SimpleWindow(const std::string &name, const std::string &title, bool open)
            : SimpleWindow(name, title, nullptr, open) {}
        SimpleWindow(const std::string &name, const std::string &title, SimpleWindow *parent)
            : SimpleWindow(name, title, parent, true) {}
        SimpleWindow(const std::string &name, const std::string &title, SimpleWindow *parent,
                     bool open)
            : m_name(name), m_title(title), m_open(open), m_parent(parent) {}

        ~SimpleWindow() override = default;

        void render() override;
        [[nodiscard]] bool isOpen() const override;

        [[nodiscard]] std::string_view getName() const;
        [[nodiscard]] std::string_view getTitle() const;

        void setName(std::string_view name);
        void setTitle(std::string_view title);

        void open() { m_open = true; }
        void close() { m_open = false; }

    protected:
        std::string m_name;
        std::string m_title;
        bool m_open;

        SimpleWindow *m_parent;
    };

    class DockableWindow : public SimpleWindow {
    public:
        DockableWindow()                                  = delete;
        DockableWindow(const DockableWindow &)            = delete;
        DockableWindow &operator=(const DockableWindow &) = delete;

        DockableWindow(const std::string &name, const std::string &title)
            : DockableWindow(name, title, true) {}
        DockableWindow(const std::string &name, const std::string &title, bool open)
            : SimpleWindow(name, title, open) {}

        ~DockableWindow() override = default;

        void render() override;

        [[nodiscard]] bool isDocked() const { return m_docked; }
        void setDocked(bool docked) { m_docked = docked; }

    private:
        bool m_docked = false;
    };

}  // namespace Toolbox::UI