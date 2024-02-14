#include "dolphin/interpreter/system.hpp"
#include "dolphin/interpreter/instructions/forms.hpp"
#include "dolphin/process.hpp"

#include "core/application.hpp"

using namespace Toolbox::Dolphin;

namespace Toolbox::Interpreter {

    void SystemDolphin::evaluateFunction() {
        while (m_evaluating.load()) {
            evaluateInstruction();
        }
    }

    void SystemDolphin::evaluateInstruction() {
        DolphinCommunicator &communicator = MainApplication::instance().getDolphinCommunicator();
        if (!communicator.manager().isHooked()) {
            m_evaluating.store(false);
            return;
        }

        u32 inst = communicator.read<u32>(static_cast<u32>(m_system_proc.m_pc)).value();
        Opcode opcode   = FORM_OPCD(inst);
        switch (opcode) {
        case Opcode::OP_TDI: {
            m_fixed_proc.tdi(FORM_TO(inst), FORM_RA(inst), FORM_SI(inst));
        }
        }
    }


}  // namespace Toolbox::Interpreter