#pragma once

#include "core/memory.hpp"
#include "instructions/forms.hpp"

using namespace Toolbox;

namespace Toolbox::Interpreter {

    class MemoryStorage {
    public:
        MemoryStorage() = default;

        bool initialize(size_t storage_size = 0x1800000) { return m_buffer.alloc(storage_size); }
        Buffer &buffer() { return m_buffer; }

        void process(const PPC::XForm &instr);
        void process(const PPC::XLForm &instr);
        void process(const PPC::XForm &instr);
        void process(const PPC::XForm &instr);


    private:


        Buffer m_buffer;
    };

}  // namespace Toolbox::Interpreter