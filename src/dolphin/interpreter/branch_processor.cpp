#pragma once

#include "core/core.hpp"
#include "core/memory.hpp"

#include "serial.hpp"

#include "dolphin/interpreter/processor.hpp"

namespace Toolbox::Interpreter {

    void BranchProcessor::b(s32 target_addr, bool aa, bool lk, Register::PC &pc) {
        if (lk) {
            m_lr.m_target_address = (pc.m_address + 4) & 0xFFFFFFFF;
        }

        if (aa) {
            pc.m_address = target_addr & 0xFFFFFFFF;
        } else {
            pc.m_address = (pc.m_address + target_addr) & 0xFFFFFFFF;
        }
    }
    void BranchProcessor::bc(s32 target_addr, u8 bo, u8 bi, bool aa, bool lk, Register::PC &pc) {
        bool cond_true = m_cr.is(bi / 4, Register::CRCmp(bi % 4));

        // bdnz && !cri
        if ((bo & 0b11110) == 0b00000) {
            m_ctr.m_0.m_count -= 1;
            if (m_ctr.m_0.m_count != 0 && !cond_true) {
                b(target_addr, aa, lk, pc);
            }
        }

        // bdz && !cri
        if ((bo & 0b11110) == 0b00010) {
            m_ctr.m_0.m_count -= 1;
            if (m_ctr.m_0.m_count == 0 && !cond_true) {
                b(target_addr, aa, lk, pc);
            }
        }

        // bc !cri
        if ((bo & 0b11100) == 0b00100 && !cond_true) {
            b(target_addr, aa, lk, pc);
        }

        // bdnz && cri
        if ((bo & 0b11110) == 0b01000) {
            m_ctr.m_0.m_count -= 1;
            if (m_ctr.m_0.m_count != 0 && cond_true) {
                b(target_addr, aa, lk, pc);
            }
        }

        // bdz && cri
        if ((bo & 0b11110) == 0b01010) {
            m_ctr.m_0.m_count -= 1;
            if (m_ctr.m_0.m_count == 0 && cond_true) {
                b(target_addr, aa, lk, pc);
            }
        }

        // bc cri
        if ((bo & 0b11100) == 0b01100 && cond_true) {
            b(target_addr, aa, lk, pc);
        }

        // b (call branch directly to save performance)
        if ((bo & 0b10100) == 0b10100) [[likely]] {
            b(target_addr, aa, lk, pc);
        }
    }
    void BranchProcessor::bclr(u8 bo, u8 bi, u8 bh, bool lk, Register::PC &pc) {}
    void BranchProcessor::bcctr(u8 bo, u8 bi, u8 bh, bool lk, Register::PC &pc) {}

    void BranchProcessor::sc(u8 lev, Register::PC &pc) {}

    void BranchProcessor::crand(u8 bt, u8 ba, u8 bb, Register::PC &pc) {}
    void BranchProcessor::crandc(u8 bt, u8 ba, u8 bb, Register::PC &pc) {}
    void BranchProcessor::creqv(u8 bt, u8 ba, u8 bb, Register::PC &pc) {}
    void BranchProcessor::cror(u8 bt, u8 ba, u8 bb, Register::PC &pc) {}
    void BranchProcessor::crorc(u8 bt, u8 ba, u8 bb, Register::PC &pc) {}
    void BranchProcessor::crnand(u8 bt, u8 ba, u8 bb, Register::PC &pc) {}
    void BranchProcessor::crnor(u8 bt, u8 ba, u8 bb, Register::PC &pc) {}
    void BranchProcessor::crxor(u8 bt, u8 ba, u8 bb, Register::PC &pc) {}

}  // namespace Toolbox::Interpreter