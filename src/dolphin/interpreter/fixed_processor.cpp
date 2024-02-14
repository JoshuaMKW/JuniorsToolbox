#pragma once

#include "core/core.hpp"
#include "core/memory.hpp"

#include "serial.hpp"

#include "dolphin/interpreter/processor.hpp"

// Used in implementations of rlwimi, rlwinm, and rlwnm
inline u32 MakeRotationMask(u32 mb, u32 me) {
    // first make 001111111111111 part
    const u32 begin = 0xFFFFFFFF >> mb;
    // then make 000000000001111 part, which is used to flip the bits of the first one
    const u32 end = 0x7FFFFFFF >> me;
    // do the bitflip
    const u32 mask = begin ^ end;

    // and invert if backwards
    if (me < mb)
        return ~mask;

    return mask;
}

namespace Toolbox::Interpreter {

    // Memory
    void FixedPointProcessor::lbz(u8 rt, s16 d, u8 ra, Buffer &storage) {}
    void FixedPointProcessor::lbzu(u8 rt, s16 d, u8 ra, Register::PC &pc, Buffer &storage) {}
    void FixedPointProcessor::lbzx(u8 rt, u8 ra, u8 rb, Buffer &storage) {}
    void FixedPointProcessor::lbzux(u8 rt, u8 ra, u8 rb, Register::PC &pc, Buffer &storage) {}

    void FixedPointProcessor::lhz(u8 rt, s16 d, u8 ra, Buffer &storage) {}
    void FixedPointProcessor::lhzu(u8 rt, s16 d, u8 ra, Register::PC &pc, Buffer &storage) {}
    void FixedPointProcessor::lhzx(u8 rt, u8 ra, u8 rb, Buffer &storage) {}
    void FixedPointProcessor::lhzux(u8 rt, u8 ra, u8 rb, Register::PC &pc, Buffer &storage) {}
    void FixedPointProcessor::lha(u8 rt, s16 d, u8 ra, Buffer &storage) {}
    void FixedPointProcessor::lhau(u8 rt, s16 d, u8 ra, Register::PC &pc, Buffer &storage) {}
    void FixedPointProcessor::lhax(u8 rt, u8 ra, u8 rb, Buffer &storage) {}
    void FixedPointProcessor::lhaux(u8 rt, u8 ra, u8 rb, Register::PC &pc, Buffer &storage) {}

    void FixedPointProcessor::lwz(u8 rt, s16 d, u8 ra, Buffer &storage) {}
    void FixedPointProcessor::lwzu(u8 rt, s16 d, u8 ra, Register::PC &pc, Buffer &storage) {}
    void FixedPointProcessor::lwzx(u8 rt, u8 ra, u8 rb, Buffer &storage) {}
    void FixedPointProcessor::lwzux(u8 rt, u8 ra, u8 rb, Register::PC &pc, Buffer &storage) {}
    void FixedPointProcessor::lwa(u8 rt, s16 d, u8 ra, Buffer &storage) {}
    void FixedPointProcessor::lwax(u8 rt, u8 ra, u8 rb, Buffer &storage) {}
    void FixedPointProcessor::lwaux(u8 rt, u8 ra, u8 rb, Register::PC &pc, Buffer &storage) {}

    void FixedPointProcessor::stb(u8 rs, s16 d, u8 ra, Buffer &storage) {}
    void FixedPointProcessor::stbu(u8 rs, s16 d, u8 ra, Register::PC &pc, Buffer &storage) {}
    void FixedPointProcessor::stbx(u8 rs, u8 ra, u8 rb, Buffer &storage) {}
    void FixedPointProcessor::stbux(u8 rs, u8 ra, u8 rb, Register::PC &pc, Buffer &storage) {}

    void FixedPointProcessor::sth(u8 rs, s16 d, u8 ra, Buffer &storage) {}
    void FixedPointProcessor::sthu(u8 rs, s16 d, u8 ra, Register::PC &pc, Buffer &storage) {}
    void FixedPointProcessor::sthx(u8 rs, u8 ra, u8 rb, Buffer &storage) {}
    void FixedPointProcessor::sthux(u8 rs, u8 ra, u8 rb, Register::PC &pc, Buffer &storage) {}

    void FixedPointProcessor::stw(u8 rs, s16 d, u8 ra, Buffer &storage) {}
    void FixedPointProcessor::stwu(u8 rs, s16 d, u8 ra, Register::PC &pc, Buffer &storage) {}
    void FixedPointProcessor::stwx(u8 rs, u8 ra, u8 rb, Buffer &storage) {}
    void FixedPointProcessor::stwux(u8 rs, u8 ra, u8 rb, Register::PC &pc, Buffer &storage) {}

    void FixedPointProcessor::lhbrx(u8 rt, u8 ra, u8 rb, Buffer &storage) {}
    void FixedPointProcessor::lwbrx(u8 rt, u8 ra, u8 rb, Buffer &storage) {}

    void FixedPointProcessor::sthbrx(u8 rs, u8 ra, u8 rb, Buffer &storage) {}
    void FixedPointProcessor::stwbrx(u8 rs, u8 ra, u8 rb, Buffer &storage) {}

    void FixedPointProcessor::lmw(u8 rt, s16 d, u8 ra, Buffer &storage) {}
    void FixedPointProcessor::stmw(u8 rs, s16 d, u8 ra, Buffer &storage) {}

    void FixedPointProcessor::lswi(u8 rt, u8 ra, u8 nb, Buffer &storage) {}
    void FixedPointProcessor::lswx(u8 rt, u8 ra, u8 rb, Buffer &storage) {}

    void FixedPointProcessor::stswi(u8 rt, u8 ra, u8 nb, Buffer &storage) {}
    void FixedPointProcessor::stswx(u8 rt, u8 ra, u8 rb, Buffer &storage) {}

    // Math

    void FixedPointProcessor::addi(u8 rt, u8 ra, s16 si) {
        u64 result = ra == 0 ? si : m_gpr[ra] + si;
        m_gpr[rt]  = result & 0xFFFFFFFF;
    }

    void FixedPointProcessor::addis(u8 rt, u8 ra, s16 si) {
        u64 result = ra == 0 ? si : m_gpr[ra] + si;
        m_gpr[rt]  = (result << 16) & 0xFFFFFFFF;
    }

    void FixedPointProcessor::add(u8 rt, u8 ra, u8 rb, bool oe, bool rc, Register::CR &cr) {
        u64 result = m_gpr[ra] + m_gpr[rb];
        m_gpr[rt]  = result & 0xFFFFFFFF;
        if (oe && result > 0xFFFFFFFF) {
            XER_SET_OV(m_xer, true);
            XER_SET_SO(m_xer, true);
        }
        if (rc) {
            cr.cmp(0, (s32)m_gpr[rt], 0, m_xer);
        }
    }

    void FixedPointProcessor::addic(u8 rt, u8 ra, s16 si, bool rc, Register::CR &cr) {
        u64 result = ra == 0 ? si : m_gpr[ra] + si;
        m_gpr[rt]  = result & 0xFFFFFFFF;
        XER_SET_CA(m_xer, result > 0xFFFFFFFF);
    }

    void FixedPointProcessor::subf(u8 rt, u8 ra, u8 rb, bool oe, bool rc, Register::CR &cr) {}
    void FixedPointProcessor::subfic(u8 rt, u8 ra, s16 si) {}

    void FixedPointProcessor::addc(u8 rt, u8 ra, u8 rb, bool oe, bool rc, Register::CR &cr) {
        u64 result = m_gpr[ra] + m_gpr[rb];
        m_gpr[rt]  = result & 0xFFFFFFFF;
        XER_SET_CA(m_xer, result > 0xFFFFFFFF);
        if (XER_CA(m_xer) && oe) {
            XER_SET_OV(m_xer, true);
            XER_SET_SO(m_xer, true);
        }
        if (rc) {
            cr.cmp(0, (s32)m_gpr[rt], 0, m_xer);
        }
    }

    void FixedPointProcessor::subfc(u8 rt, u8 ra, u8 rb, bool oe, bool rc, Register::CR &cr) {}

    void FixedPointProcessor::adde(u8 rt, u8 ra, u8 rb, bool oe, bool rc, Register::CR &cr) {
        u64 result = m_gpr[ra] + m_gpr[rb] + (u64)XER_CA(m_xer);
        m_gpr[rt]  = result & 0xFFFFFFFF;
        XER_SET_CA(m_xer, result > 0xFFFFFFFF);
        if (XER_CA(m_xer) && oe) {
            XER_SET_OV(m_xer, true);
            XER_SET_SO(m_xer, true);
        }
        if (rc) {
            cr.cmp(0, (s32)m_gpr[rt], 0, m_xer);
        }
    }
    void FixedPointProcessor::subfe(u8 rt, u8 ra, u8 rb, bool oe, bool rc, Register::CR &cr) {}

    void FixedPointProcessor::addme(u8 rt, u8 ra, bool oe, bool rc, Register::CR &cr) {
        u64 result = m_gpr[ra] + (u64)XER_CA(m_xer) + 0xFFFFFFFF;
        m_gpr[rt]  = result & 0xFFFFFFFF;
        XER_SET_CA(m_xer, result > 0xFFFFFFFF);
        if (XER_CA(m_xer) && oe) {
            XER_SET_OV(m_xer, true);
            XER_SET_SO(m_xer, true);
        }
        if (rc) {
            cr.cmp(0, (s32)m_gpr[rt], 0, m_xer);
        }
    }

    void FixedPointProcessor::subfme(u8 rt, u8 ra, bool oe, bool rc, Register::CR &cr) {}
    void FixedPointProcessor::addze(u8 rt, u8 ra, bool oe, bool rc, Register::CR &cr) {}
    void FixedPointProcessor::subfze(u8 rt, u8 ra, bool oe, bool rc, Register::CR &cr) {}

    void FixedPointProcessor::mulli(u8 rt, u8 ra, s16 si) {}
    void FixedPointProcessor::mullw(u8 rt, u8 ra, u8 rb, bool oe, bool rc, Register::CR &cr) {}
    void FixedPointProcessor::mullhd(u8 rt, u8 ra, u8 rb, bool rc, Register::CR &cr) {}
    void FixedPointProcessor::mullhdu(u8 rt, u8 ra, u8 rb, bool rc, Register::CR &cr) {}
    void FixedPointProcessor::mullhw(u8 rt, u8 ra, u8 rb, bool rc, Register::CR &cr) {}
    void FixedPointProcessor::mullhwu(u8 rt, u8 ra, u8 rb, bool rc, Register::CR &cr) {}

    void FixedPointProcessor::divw(u8 rt, u8 ra, u8 rb, bool oe, bool rc, Register::CR &cr) {
        u64 result = (s32)m_gpr[ra] / (s32)m_gpr[rb];
        if (m_gpr[ra] == 0x80000000 && m_gpr[rb] == 0xFFFFFFFF) {
            result = 0x7FFFFFFF;
        } else if (m_gpr[rb] == 0) {
            result = 0x7FFFFFFF;
        }
        if (oe && result > 0xFFFFFFFF) {
            XER_SET_OV(m_xer, true);
            XER_SET_SO(m_xer, true);
        }
        if (rc) {
            cr.cmp(0, (s32)m_gpr[rt], 0, m_xer);
        }
    }

    void FixedPointProcessor::divwu(u8 rt, u8 ra, u8 rb, bool oe, bool rc, Register::CR &cr) {
        u64 result = (u32)m_gpr[ra] / (u32)m_gpr[rb];
        if (m_gpr[ra] == 0x80000000 && m_gpr[rb] == 0xFFFFFFFF) {
            result = 0x7FFFFFFF;
        } else if (m_gpr[rb] == 0) {
            result = 0x7FFFFFFF;
        }
        if (oe && result > 0xFFFFFFFF) {
            XER_SET_OV(m_xer, true);
            XER_SET_SO(m_xer, true);
        }
        if (rc) {
            cr.cmp(0, (s32)m_gpr[rt], 0, m_xer);
        }
    }

    // Compare

    void FixedPointProcessor::cmpi(u8 bf, bool l, u8 ra, s16 si, Register::CR &cr) {
        // Double comparison unsupported (32-bit)
        if (l == 1) {
            m_invalid_cb(PROC_INVALID_MSG(FixedPointProcessor, cmpi,
                                          "Double comparison (l == 1) is invalid!"));
            return;
        }
        cr.cmp(bf, (s32)m_gpr[ra], si, m_xer);
    }

    void FixedPointProcessor::cmp(u8 bf, bool l, u8 ra, u8 rb, Register::CR &cr) {
        // Double comparison unsupported (32-bit)
        if (l == 1) {
            m_invalid_cb(PROC_INVALID_MSG(FixedPointProcessor, cmp,
                                          "Double comparison (l == 1) is invalid!"));
            return;
        }
        cr.cmp(bf, (s32)m_gpr[ra], (s32)m_gpr[rb], m_xer);
    }

    void FixedPointProcessor::cmpli(u8 bf, bool l, u8 ra, u16 ui, Register::CR &cr) {
        // Double comparison unsupported (32-bit)
        if (l == 1) {
            m_invalid_cb(PROC_INVALID_MSG(FixedPointProcessor, cmpli,
                                          "Double comparison (l == 1) is invalid!"));
            return;
        }
        cr.cmp(bf, (u32)m_gpr[ra], (u32)ui, m_xer);
    }

    void FixedPointProcessor::cmpl(u8 bf, bool l, u8 ra, u8 rb, Register::CR &cr) {
        // Double comparison unsupported (32-bit)
        if (l == 1) {
            m_invalid_cb(PROC_INVALID_MSG(FixedPointProcessor, cmpl,
                                          "Double comparison (l == 1) is invalid!"));
            return;
        }
        cr.cmp(bf, (u32)m_gpr[ra], (u32)m_gpr[rb], m_xer);
    }

    // Trap

    void FixedPointProcessor::tdi(u8 to, u8 ra, s16 si) {}
    void FixedPointProcessor::twi(u8 to, u8 ra, s16 si) {}
    void FixedPointProcessor::td(u8 to, u8 ra, u8 rb) {}
    void FixedPointProcessor::tw(u8 to, u8 ra, u8 rb) {}

    // Logic

    void FixedPointProcessor::andi(u8 ra, u8 rs, u16 ui, Register::CR &cr) {
        u64 result = m_gpr[rs] & ui;
        m_gpr[ra]  = result & 0xFFFFFFFF;
        cr.cmp(0, (s32)m_gpr[ra], 0, m_xer);
    }

    void FixedPointProcessor::andis(u8 ra, u8 rs, u16 ui, Register::CR &cr) {
        u64 result = m_gpr[rs] & ((u64)ui << 16);
        m_gpr[ra]  = result & 0xFFFFFFFF;
        cr.cmp(0, (s32)m_gpr[ra], 0, m_xer);
    }

    void FixedPointProcessor::ori(u8 ra, u8 rs, u16 ui) {
        u64 result = m_gpr[rs] | ui;
        m_gpr[ra]  = result & 0xFFFFFFFF;
    }

    void FixedPointProcessor::oris(u8 ra, u8 rs, u16 ui) {
        u64 result = m_gpr[rs] | ((u64)ui << 16);
        m_gpr[ra]  = result & 0xFFFFFFFF;
    }

    void FixedPointProcessor::xori(u8 ra, u8 rs, u16 ui) {
        u64 result = m_gpr[rs] ^ ui;
        m_gpr[ra]  = result & 0xFFFFFFFF;
    }

    void FixedPointProcessor::and_(u8 ra, u8 rs, u8 rb, bool rc, Register::CR &cr) {
        u64 result = m_gpr[rs] & m_gpr[rb];
        m_gpr[ra]  = result & 0xFFFFFFFF;
        if (rc) {
            cr.cmp(0, (s32)m_gpr[ra], 0, m_xer);
        }
    }

    void FixedPointProcessor::or_(u8 ra, u8 rs, u8 rb, bool rc, Register::CR &cr) {
        u64 result = m_gpr[rs] | m_gpr[rb];
        m_gpr[ra]  = result & 0xFFFFFFFF;
        if (rc) {
            cr.cmp(0, (s32)m_gpr[ra], 0, m_xer);
        }
    }

    void FixedPointProcessor::xor_(u8 ra, u8 rs, u8 rb, bool rc, Register::CR &cr) {
        u64 result = m_gpr[rs] ^ m_gpr[rb];
        m_gpr[ra]  = result & 0xFFFFFFFF;
        if (rc) {
            cr.cmp(0, (s32)m_gpr[ra], 0, m_xer);
        }
    }

    void FixedPointProcessor::nand_(u8 ra, u8 rs, u8 rb, bool rc, Register::CR &cr) {
        u64 result = ~(m_gpr[rs] & m_gpr[rb]);
        m_gpr[ra]  = result & 0xFFFFFFFF;
        if (rc) {
            cr.cmp(0, (s32)m_gpr[ra], 0, m_xer);
        }
    }

    void FixedPointProcessor::nor_(u8 ra, u8 rs, u8 rb, bool rc, Register::CR &cr) {
        u64 result = ~(m_gpr[rs] | m_gpr[rb]);
        m_gpr[ra]  = result & 0xFFFFFFFF;
        if (rc) {
            cr.cmp(0, (s32)m_gpr[ra], 0, m_xer);
        }
    }

    void FixedPointProcessor::eqv_(u8 ra, u8 rs, u8 rb, bool rc, Register::CR &cr) {
        u64 result = m_gpr[rs] ^ m_gpr[rb];
        m_gpr[ra]  = ~(result)&0xFFFFFFFF;
        if (rc) {
            cr.cmp(0, (s32)m_gpr[ra], 0, m_xer);
        }
    }

    void FixedPointProcessor::andc(u8 ra, u8 rs, u8 rb, bool rc, Register::CR &cr) {
        u64 result = m_gpr[rs] & ~m_gpr[rb];
        m_gpr[ra]  = result & 0xFFFFFFFF;
        if (rc) {
            cr.cmp(0, (s32)m_gpr[ra], 0, m_xer);
        }
    }
    void FixedPointProcessor::orc(u8 ra, u8 rs, u8 rb, bool rc, Register::CR &cr) {
        u64 result = m_gpr[rs] | ~m_gpr[rb];
        m_gpr[ra]  = result & 0xFFFFFFFF;
        if (rc) {
            cr.cmp(0, (s32)m_gpr[ra], 0, m_xer);
        }
    }

    void FixedPointProcessor::extsb(u8 ra, u8 rs, bool rc, Register::CR &cr) {
        m_gpr[ra] = std::bit_cast<u64, s64>((s64)m_gpr[rs]) & 0xFFFFFFFF;
        if (rc) {
            cr.cmp(0, (s32)m_gpr[ra], 0, m_xer);
        }
    }

    void FixedPointProcessor::extsh(u8 ra, u8 rs, bool rc, Register::CR &cr) {
        m_gpr[ra] = std::bit_cast<u64, s64>((s64)m_gpr[rs]) & 0xFFFFFFFF;
        if (rc) {
            cr.cmp(0, (s32)m_gpr[ra], 0, m_xer);
        }
    }

    void FixedPointProcessor::cntlzw(u8 ra, u8 rs, bool rc, Register::CR &cr) {
        u64 result = 0;
        while (result < 32) {
            if ((m_gpr[rs] & SIG_BIT((u8)result, 32)) != 0) {
                break;
            }
            result += 1;
        }
        m_gpr[ra] = result & 0xFFFFFFFF;
        if (rc) {
            cr.cmp(0, (s32)m_gpr[ra], 0, m_xer);
        }
    }

    void FixedPointProcessor::popcntb(u8 ra, u8 rs) {}

    // Rotate | shift

    void FixedPointProcessor::rldicl(u8 ra, u8 rs, u8 sh, u8 mb, bool rc, Register::CR &cr) {}
    void FixedPointProcessor::rldicr(u8 ra, u8 rs, u8 sh, u8 me, bool rc, Register::CR &cr) {}
    void FixedPointProcessor::rldic(u8 ra, u8 rs, u8 sh, u8 mb, bool rc, Register::CR &cr) {}
    void FixedPointProcessor::rlwinm(u8 ra, u8 rs, u8 sh, u8 mb, u8 me, bool rc, Register::CR &cr) {
    }
    void FixedPointProcessor::rldcl(u8 ra, u8 rs, u8 rb, u8 mb, bool rc, Register::CR &cr) {}
    void FixedPointProcessor::rldcr(u8 ra, u8 rs, u8 rb, u8 me, bool rc, Register::CR &cr) {}
    void FixedPointProcessor::rlwnm(u8 ra, u8 rs, u8 rb, u8 mb, u8 me, bool rc, Register::CR &cr) {}
    void FixedPointProcessor::rlwimi(u8 ra, u8 rs, u8 sh, u8 mb, u8 me, bool rc, Register::CR &cr) {
    }

    void FixedPointProcessor::slw(u8 ra, u8 rs, u8 rb, bool rc, Register::CR &cr) {}
    void FixedPointProcessor::srw(u8 ra, u8 rs, u8 rb, bool rc, Register::CR &cr) {}
    void FixedPointProcessor::srawi(u8 ra, u8 rs, u8 sh, bool rc, Register::CR &cr) {}
    void FixedPointProcessor::sraw(u8 ra, u8 rs, u8 rb, bool rc, Register::CR &cr) {}

    // SPRs

    void FixedPointProcessor::mtspr(Register::SPRType spr, u8 rs, Register::LR &lr,
                                    Register::CTR &ctr) {}
    void FixedPointProcessor::mfspr(Register::SPRType spr, u8 rt, const Register::LR &lr,
                                    const Register::CTR &ctr) {}
    void FixedPointProcessor::mtcrf(u16 fxm, u8 rs, Register::CR &cr) {}
    void FixedPointProcessor::mfcr(u8 rt, const Register::CR &cr) {}
    void FixedPointProcessor::mtmsrd(u8 rs, bool l, Register::MSR &msr) {}
    void FixedPointProcessor::mfmsr(u8 rs, const Register::MSR &msr) {}

    // Extended

    void FixedPointProcessor::mtocrf(u8 fxm, u8 rs, Register::CR &cr) {}
    void FixedPointProcessor::mfocrf(u8 fxm, u8 rt, const Register::CR &cr) {}

    // External control

    void FixedPointProcessor::eciwx(u8 rt, u8 ra, u8 rb, Buffer &storage) { m_gpr[rt] = 0; }
    void FixedPointProcessor::ecowx(u8 rs, u8 ra, u8 rb, Buffer &storage) {}

}  // namespace Toolbox::Interpreter