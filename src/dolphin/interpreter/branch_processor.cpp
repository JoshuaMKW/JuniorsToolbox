#pragma once

#include "core/core.hpp"
#include "core/memory.hpp"

#include "serial.hpp"

#include "dolphin/interpreter/processor.hpp"

namespace Toolbox::Interpreter {

    void BranchProcessor::b(s32 target_addr, bool aa, bool lk, Register::PC &pc) {
        if (lk) {
            m_lr = (pc + 4) & 0xFFFFFFFF;
        }

        if (aa) {
            pc = target_addr & 0xFFFFFFFF;
        } else {
            pc = (pc + target_addr) & 0xFFFFFFFF;
        }
    }

    void BranchProcessor::bc(s32 target_addr, u8 bo, u8 bi, bool aa, bool lk, Register::PC &pc) {
        bool cond_true = m_cr.is(bi / 4, Register::CRCmp(bi % 4));

        // bdnz && !cri
        if ((bo & 0b11110) == 0b00000) {
            m_ctr -= 1;
            if (m_ctr != 0 && !cond_true) {
                b(target_addr, aa, lk, pc);
            }
        }

        // bdz && !cri
        if ((bo & 0b11110) == 0b00010) {
            m_ctr -= 1;
            if (m_ctr == 0 && !cond_true) {
                b(target_addr, aa, lk, pc);
            }
        }

        // bc !cri
        if ((bo & 0b11100) == 0b00100 && !cond_true) {
            b(target_addr, aa, lk, pc);
        }

        // bdnz && cri
        if ((bo & 0b11110) == 0b01000) {
            m_ctr -= 1;
            if (m_ctr != 0 && cond_true) {
                b(target_addr, aa, lk, pc);
            }
        }

        // bdz && cri
        if ((bo & 0b11110) == 0b01010) {
            m_ctr -= 1;
            if (m_ctr == 0 && cond_true) {
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

        m_invalid_cb(PROC_INVALID_MSG(BranchProcessor, b, "Invalid form!"));
    }

    void BranchProcessor::bclr(u8 bo, u8 bi, bool lk, Register::PC &pc) {
        bool cond_true = m_cr.is(bi / 4, Register::CRCmp(bi % 4));

        // bdnz && !cri
        if ((bo & 0b11110) == 0b00000) {
            m_ctr -= 1;
            if (m_ctr != 0 && !cond_true) {
                b((s32)m_lr, true, lk, pc);
            }
        }

        // bdz && !cri
        if ((bo & 0b11110) == 0b00010) {
            m_ctr -= 1;
            if (m_ctr == 0 && !cond_true) {
                b((s32)m_lr, true, lk, pc);
            }
        }

        // bc !cri
        if ((bo & 0b11100) == 0b00100 && !cond_true) {
            b((s32)m_lr, true, lk, pc);
        }

        // bdnz && cri
        if ((bo & 0b11110) == 0b01000) {
            m_ctr -= 1;
            if (m_ctr != 0 && cond_true) {
                b((s32)m_lr, true, lk, pc);
            }
        }

        // bdz && cri
        if ((bo & 0b11110) == 0b01010) {
            m_ctr -= 1;
            if (m_ctr == 0 && cond_true) {
                b((s32)m_lr, true, lk, pc);
            }
        }

        // bc cri
        if ((bo & 0b11100) == 0b01100 && cond_true) {
            b((s32)m_lr, true, lk, pc);
        }

        // blr
        if ((bo & 0b10100) == 0b10100) [[likely]] {
            b((s32)m_lr, true, lk, pc);
        }

        m_invalid_cb(PROC_INVALID_MSG(BranchProcessor, bclr, "Invalid form!"));
    }

    void BranchProcessor::bcctr(u8 bo, u8 bi, bool lk, Register::PC &pc) {
        bool cond_true = m_cr.is(bi / 4, Register::CRCmp(bi % 4));

        // CTR manip is invalid
        if ((bo & 0b00100) == 0) {
            m_invalid_cb(PROC_INVALID_MSG(BranchProcessor, bcctr,
                                          "CTR manipulation (bo & 0b00100) is invalid!"));
            return;
        }

        // bc !cri
        if ((bo & 0b11100) == 0b00100 && !cond_true) {
            b(m_ctr, true, lk, pc);
        }

        // bc cri
        if ((bo & 0b11100) == 0b01100 && cond_true) {
            b(m_ctr, true, lk, pc);
        }

        // b (call branch directly to save performance)
        if ((bo & 0b10100) == 0b10100) [[likely]] {
            b(m_ctr, true, lk, pc);
        }

        m_invalid_cb(PROC_INVALID_MSG(BranchProcessor, bcctr, "Invalid form!"));
    }

    void BranchProcessor::crand(u8 bt, u8 ba, u8 bb) {
        bool result = GET_SIG_BIT(m_cr.m_crf, ba, 32) & GET_SIG_BIT(m_cr.m_crf, bb, 32);
        SET_SIG_BIT(m_cr.m_crf, bt, result, 32);
    }

    void BranchProcessor::crandc(u8 bt, u8 ba, u8 bb) {
        bool result = GET_SIG_BIT(m_cr.m_crf, ba, 32) & ~GET_SIG_BIT(m_cr.m_crf, bb, 32);
        SET_SIG_BIT(m_cr.m_crf, bt, result, 32);
    }

    void BranchProcessor::creqv(u8 bt, u8 ba, u8 bb) {
        bool result = GET_SIG_BIT(m_cr.m_crf, ba, 32) ^ GET_SIG_BIT(m_cr.m_crf, bb, 32);
        SET_SIG_BIT(m_cr.m_crf, bt, ~result, 32);
    }

    void BranchProcessor::cror(u8 bt, u8 ba, u8 bb) {
        bool result = GET_SIG_BIT(m_cr.m_crf, ba, 32) | GET_SIG_BIT(m_cr.m_crf, bb, 32);
        SET_SIG_BIT(m_cr.m_crf, bt, result, 32);
    }

    void BranchProcessor::crorc(u8 bt, u8 ba, u8 bb) {
        bool result = GET_SIG_BIT(m_cr.m_crf, ba, 32) | ~GET_SIG_BIT(m_cr.m_crf, bb, 32);
        SET_SIG_BIT(m_cr.m_crf, bt, result, 32);
    }

    void BranchProcessor::crnand(u8 bt, u8 ba, u8 bb) {
        bool result = GET_SIG_BIT(m_cr.m_crf, ba, 32) & GET_SIG_BIT(m_cr.m_crf, bb, 32);
        SET_SIG_BIT(m_cr.m_crf, bt, ~result, 32);
    }

    void BranchProcessor::crnor(u8 bt, u8 ba, u8 bb) {
        bool result = GET_SIG_BIT(m_cr.m_crf, ba, 32) | GET_SIG_BIT(m_cr.m_crf, bb, 32);
        SET_SIG_BIT(m_cr.m_crf, bt, ~result, 32);
    }

    void BranchProcessor::crxor(u8 bt, u8 ba, u8 bb) {
        bool result = GET_SIG_BIT(m_cr.m_crf, ba, 32) ^ GET_SIG_BIT(m_cr.m_crf, bb, 32);
        SET_SIG_BIT(m_cr.m_crf, bt, result, 32);
    }

    void BranchProcessor::mcrf(u8 bt, u8 ba) {
        u8 shift_src = ((7 - ba) * 4);
        u8 shift_dst = ((7 - bt) * 4);
        u8 result    = (m_cr.m_crf >> shift_src) & 0b1111;
        m_cr.m_crf = (m_cr.m_crf & ~(0b1111 << shift_dst)) | (result << shift_dst);
    }

    void BranchProcessor::mcrfs(u8 bt, u8 ba) {}

}  // namespace Toolbox::Interpreter