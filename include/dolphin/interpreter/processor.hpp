#pragma once

#include "core/core.hpp"
#include "core/memory.hpp"

#include "registers.hpp"
#include "serial.hpp"

namespace Toolbox::Interpreter {

    using func_ret_cb = std::function<void(const Register::RegisterSnapshot &snapshot)>;
    using func_exception_cb =
        std::function<void(u32 bad_instr_ptr, const Register::RegisterSnapshot &snapshot)>;

    enum class BOHint : u8 {
        NONE,
        RESERVED,
        UNLIKELY,
        LIKELY,
    };

    enum class SyncType : u8 {
        HEAVY,
        LIGHT,
        HEAVY_ORDERED,
        RESERVED,
    };

    enum class DataCacheHintType : u8 {
        THIS_BLOCK,
        THIS_STREAM_ALL,
        THIS_BLOCK_ALL,
        STREAM_DESCRIPT = 8,
    };

    class SystemProcessor {
    public:
        friend class SystemDolphin;

        SystemProcessor() = default;

        void onException(func_exception_cb cb) { m_exception_cb = cb; }

    private:
        void mftb(u8 rt, u16 tbr);

        // Storage control

        void icbi(u8 ra, u8 rb, Buffer &storage) {}
        void dcbt(u8 ra, u8 rb, DataCacheHintType th, Buffer &storage) {}
        void dcbf(u8 ra, u8 rb, bool l, Buffer &storage) {}
        void dcbtst(u8 ra, u8 rb, Buffer &storage) {}
        void dcbz(u8 ra, u8 rb, Buffer &storage) {}
        void dcbst(u8 ra, u8 rb, Buffer &storage) {}

        // Sync - order

        void isync() {}
        void sync(SyncType l) {}
        void eieio() {}

        // External control

        void eciwx(u8 rt, u8 ra, u8 rb, Buffer &storage) {}
        void ecowx(u8 rs, u8 ra, u8 rb, Buffer &storage) {}

        // Interrupt flow

        void rfid() {}
        void hrfid() {}

        Register::PC m_pc;
        Register::TB m_tb;
        Register::MSR m_msr;
        Register::DAR m_dar;
        Register::DSISR m_dsisr;

        func_exception_cb m_exception_cb;
    };

    class BranchProcessor {
    public:
        friend class SystemDolphin;

        BranchProcessor() = default;

        void onReturn(func_ret_cb cb) { m_return_cb = cb; }
        void onException(func_exception_cb cb) { m_exception_cb = cb; }

    protected:
        void b(s32 target_addr, bool aa, bool lk, Register::PC &pc);
        void bc(s32 target_addr, u8 bo, u8 bi, bool aa, bool lk, Register::PC &pc);
        void bclr(u8 bo, u8 bi, u8 bh, bool lk, Register::PC &pc);
        void bcctr(u8 bo, u8 bi, u8 bh, bool lk, Register::PC &pc);

        void sc(u8 lev, Register::PC &pc);

        void crand(u8 bt, u8 ba, u8 bb, Register::PC &pc);
        void crandc(u8 bt, u8 ba, u8 bb, Register::PC &pc);
        void creqv(u8 bt, u8 ba, u8 bb, Register::PC &pc);
        void cror(u8 bt, u8 ba, u8 bb, Register::PC &pc);
        void crorc(u8 bt, u8 ba, u8 bb, Register::PC &pc);
        void crnand(u8 bt, u8 ba, u8 bb, Register::PC &pc);
        void crnor(u8 bt, u8 ba, u8 bb, Register::PC &pc);
        void crxor(u8 bt, u8 ba, u8 bb, Register::PC &pc);

    private:
        Register::CR m_cr;
        Register::LR m_lr;
        Register::CTR m_ctr;

        func_ret_cb m_return_cb;
        func_exception_cb m_exception_cb;
    };

    class FixedPointProcessor {
    public:
        friend class SystemDolphin;

        FixedPointProcessor() = default;

        void onException(func_exception_cb cb) { m_exception_cb = cb; }

    protected:
        // Memory
        void lbz(u8 rt, s16 d, u8 ra, Buffer &storage);
        void lbzu(u8 rt, s16 d, u8 ra, Register::PC &pc, Buffer &storage);
        void lbzx(u8 rt, u8 ra, u8 rb, Buffer &storage);
        void lbzux(u8 rt, u8 ra, u8 rb, Register::PC &pc, Buffer &storage);

        void lhz(u8 rt, s16 d, u8 ra, Buffer &storage);
        void lhzu(u8 rt, s16 d, u8 ra, Register::PC &pc, Buffer &storage);
        void lhzx(u8 rt, u8 ra, u8 rb, Buffer &storage);
        void lhzux(u8 rt, u8 ra, u8 rb, Register::PC &pc, Buffer &storage);
        void lha(u8 rt, s16 d, u8 ra, Buffer &storage);
        void lhau(u8 rt, s16 d, u8 ra, Register::PC &pc, Buffer &storage);
        void lhax(u8 rt, u8 ra, u8 rb, Buffer &storage);
        void lhaux(u8 rt, u8 ra, u8 rb, Register::PC &pc, Buffer &storage);

        void lwz(u8 rt, s16 d, u8 ra, Buffer &storage);
        void lwzu(u8 rt, s16 d, u8 ra, Register::PC &pc, Buffer &storage);
        void lwzx(u8 rt, u8 ra, u8 rb, Buffer &storage);
        void lwzux(u8 rt, u8 ra, u8 rb, Register::PC &pc, Buffer &storage);
        void lwa(u8 rt, s16 d, u8 ra, Buffer &storage);
        void lwax(u8 rt, u8 ra, u8 rb, Buffer &storage);
        void lwaux(u8 rt, u8 ra, u8 rb, Register::PC &pc, Buffer &storage);

        void ld(u8 rt, s16 d, u8 ra, Buffer &storage);
        void ldu(u8 rt, s16 d, u8 ra, Register::PC &pc, Buffer &storage);
        void ldx(u8 rt, u8 ra, u8 rb, Buffer &storage);
        void ldux(u8 rt, u8 ra, u8 rb, Register::PC &pc, Buffer &storage);

        void stb(u8 rs, s16 d, u8 ra, Buffer &storage);
        void stbu(u8 rs, s16 d, u8 ra, Register::PC &pc, Buffer &storage);
        void stbx(u8 rs, u8 ra, u8 rb, Buffer &storage);
        void stbux(u8 rs, u8 ra, u8 rb, Register::PC &pc, Buffer &storage);

        void sth(u8 rs, s16 d, u8 ra, Buffer &storage);
        void sthu(u8 rs, s16 d, u8 ra, Register::PC &pc, Buffer &storage);
        void sthx(u8 rs, u8 ra, u8 rb, Buffer &storage);
        void sthux(u8 rs, u8 ra, u8 rb, Register::PC &pc, Buffer &storage);

        void stw(u8 rs, s16 d, u8 ra, Buffer &storage);
        void stwu(u8 rs, s16 d, u8 ra, Register::PC &pc, Buffer &storage);
        void stwx(u8 rs, u8 ra, u8 rb, Buffer &storage);
        void stwux(u8 rs, u8 ra, u8 rb, Register::PC &pc, Buffer &storage);

        void std(u8 rs, s16 d, u8 ra, Buffer &storage);
        void stdu(u8 rs, s16 d, u8 ra, Register::PC &pc, Buffer &storage);
        void stdx(u8 rs, u8 ra, u8 rb, Buffer &storage);
        void stdux(u8 rs, u8 ra, u8 rb, Register::PC &pc, Buffer &storage);

        void lhbrx(u8 rt, u8 ra, u8 rb, Buffer &storage);
        void lwbrx(u8 rt, u8 ra, u8 rb, Buffer &storage);

        void sthbrx(u8 rs, u8 ra, u8 rb, Buffer &storage);
        void stwbrx(u8 rs, u8 ra, u8 rb, Buffer &storage);

        void lmw(u8 rt, s16 d, u8 ra, Buffer &storage);
        void stmw(u8 rs, s16 d, u8 ra, Buffer &storage);

        void lswi(u8 rt, u8 ra, u8 nb, Buffer &storage);
        void lswx(u8 rt, u8 ra, u8 rb, Buffer &storage);

        void stswi(u8 rt, u8 ra, u8 nb, Buffer &storage);
        void stswx(u8 rt, u8 ra, u8 rb, Buffer &storage);

        // Math

        void addi(u8 rt, u8 ra, s16 si);
        void addis(u8 rt, u8 ra, s16 si);
        void add(u8 rt, u8 ra, u8 rb, bool oe, bool rc, Register::CR &cr);
        void addic(u8 rt, u8 ra, s16 si, bool rc, Register::CR &cr);
        void subf(u8 rt, u8 ra, u8 rb, bool oe, bool rc, Register::CR &cr);
        void subfic(u8 rt, u8 ra, s16 si);
        void addc(u8 rt, u8 ra, u8 rb, bool oe, bool rc, Register::CR &cr);
        void subfc(u8 rt, u8 ra, u8 rb, bool oe, bool rc, Register::CR &cr);
        void adde(u8 rt, u8 ra, u8 rb, bool oe, bool rc, Register::CR &cr);
        void subfe(u8 rt, u8 ra, u8 rb, bool oe, bool rc, Register::CR &cr);
        void addme(u8 rt, u8 ra, bool oe, bool rc, Register::CR &cr);
        void subfme(u8 rt, u8 ra, bool oe, bool rc, Register::CR &cr);
        void addze(u8 rt, u8 ra, bool oe, bool rc, Register::CR &cr);
        void subfze(u8 rt, u8 ra, bool oe, bool rc, Register::CR &cr);

        void mulli(u8 rt, u8 ra, s16 si);
        void mulld(u8 rt, u8 ra, u8 rb, bool oe, bool rc, Register::CR &cr);
        void mullw(u8 rt, u8 ra, u8 rb, bool oe, bool rc, Register::CR &cr);
        void mullhd(u8 rt, u8 ra, u8 rb, bool rc, Register::CR &cr);
        void mullhdu(u8 rt, u8 ra, u8 rb, bool rc, Register::CR &cr);
        void mullhw(u8 rt, u8 ra, u8 rb, bool rc, Register::CR &cr);
        void mullhwu(u8 rt, u8 ra, u8 rb, bool rc, Register::CR &cr);

        void divd(u8 rt, u8 ra, u8 rb, bool oe, bool rc, Register::CR &cr);
        void divw(u8 rt, u8 ra, u8 rb, bool oe, bool rc, Register::CR &cr);
        void divdu(u8 rt, u8 ra, u8 rb, bool oe, bool rc, Register::CR &cr);
        void divwu(u8 rt, u8 ra, u8 rb, bool oe, bool rc, Register::CR &cr);

        // Compare

        void cmpi(u8 bf, bool l, u8 ra, s16 si, Register::CR &cr);
        void cmp(u8 bf, bool l, u8 ra, u8 rb, Register::CR &cr);
        void cmpli(u8 bf, bool l, u8 ra, u16 ui, Register::CR &cr);
        void cmpl(u8 bf, bool l, u8 ra, u8 rb, Register::CR &cr);

        // Trap

        void tdi(u8 to, u8 ra, s16 si);
        void twi(u8 to, u8 ra, s16 si);
        void td(u8 to, u8 ra, u8 rb);
        void tw(u8 to, u8 ra, u8 rb);

        // Logic

        void andi(u8 ra, u8 rs, u16 ui, Register::CR &cr);
        void andis(u8 ra, u8 rs, u16 ui, Register::CR &cr);
        void ori(u8 ra, u8 rs, u16 ui);
        void oris(u8 ra, u8 rs, u16 ui);
        void xori(u8 ra, u8 rs, u16 ui);
        void and_(u8 ra, u8 rs, u8 rb, bool rc, Register::CR &cr);
        void or_(u8 ra, u8 rs, u8 rb, bool rc, Register::CR &cr);
        void xor_(u8 ra, u8 rs, u8 rb, bool rc, Register::CR &cr);
        void nand_(u8 ra, u8 rs, u8 rb, bool rc, Register::CR &cr);
        void nor_(u8 ra, u8 rs, u8 rb, bool rc, Register::CR &cr);
        void eqv_(u8 ra, u8 rs, u8 rb, bool rc, Register::CR &cr);
        void andc_(u8 ra, u8 rs, u8 rb, bool rc, Register::CR &cr);
        void orc_(u8 ra, u8 rs, u8 rb, bool rc, Register::CR &cr);

        void extsb(u8 ra, u8 rs, bool rc, Register::CR &cr);
        void extsh(u8 ra, u8 rs, bool rc, Register::CR &cr);
        void extsw(u8 ra, u8 rs, bool rc, Register::CR &cr);

        void cntlzd(u8 ra, u8 rs, bool rc, Register::CR &cr);
        void cntlzw(u8 ra, u8 rs, bool rc, Register::CR &cr);

        void popcntb(u8 ra, u8 rs);

        // Rotate | shift

        void rldicl(u8 ra, u8 rs, u8 sh, u8 mb, bool rc, Register::CR &cr);
        void rldicr(u8 ra, u8 rs, u8 sh, u8 me, bool rc, Register::CR &cr);
        void rldic(u8 ra, u8 rs, u8 sh, u8 mb, bool rc, Register::CR &cr);
        void rlwinm(u8 ra, u8 rs, u8 sh, u8 mb, u8 me, bool rc, Register::CR &cr);
        void rldcl(u8 ra, u8 rs, u8 rb, u8 mb, bool rc, Register::CR &cr);
        void rldcr(u8 ra, u8 rs, u8 rb, u8 me, bool rc, Register::CR &cr);
        void rlwnm(u8 ra, u8 rs, u8 rb, u8 mb, u8 me, bool rc, Register::CR &cr);
        void rldimi(u8 ra, u8 rs, u8 sh, u8 mb, bool rc, Register::CR &cr);
        void rlwimi(u8 ra, u8 rs, u8 sh, u8 mb, u8 me, bool rc, Register::CR &cr);

        void sld(u8 ra, u8 rs, u8 rb, bool rc, Register::CR &cr);
        void slw(u8 ra, u8 rs, u8 rb, bool rc, Register::CR &cr);
        void srd(u8 ra, u8 rs, u8 rb, bool rc, Register::CR &cr);
        void srw(u8 ra, u8 rs, u8 rb, bool rc, Register::CR &cr);
        void sradi(u8 ra, u8 rs, u8 sh, bool rc, Register::CR &cr);
        void srawi(u8 ra, u8 rs, u8 sh, bool rc, Register::CR &cr);
        void srad(u8 ra, u8 rs, u8 rb, bool rc, Register::CR &cr);
        void sraw(u8 ra, u8 rs, u8 rb, bool rc, Register::CR &cr);

        // SPRs

        void mtspr(SPRType spr, u8 rs, Register::LR &lr, Register::CTR &ctr);
        void mfspr(SPRType spr, u8 rt, const Register::LR &lr, const Register::CTR &ctr);
        void mtcrf(u16 fxm, u8 rs, Register::CR &cr);
        void mfcr(u8 rt, const Register::CR &cr);
        void mtmsrd(u8 rs, bool l, Register::MSR &msr);
        void mfmsr(u8 rs, const Register::MSR &msr);

        // Extended

        void mtocrf(u8 fxm, u8 rs, Register::CR &cr);
        void mfocrf(u8 fxm, u8 rt, const Register::CR &cr);

    private:
        Register::XER m_xer;
        Register::GPR m_gpr[32];

        func_exception_cb m_exception_cb;
    };

    class FloatingPointProcessor {
    public:
        friend class SystemDolphin;

        FloatingPointProcessor() = default;

        void onException(func_exception_cb cb) { m_exception_cb = cb; }

    protected:
        // Memory

        void lfs(u8 frt, s16 d, u8 ra);
        void lfsu(u8 frt, s16 d, u8 ra);
        void lfsx(u8 frt, u8 ra, u8 rb);
        void lfsux(u8 frt, u8 ra, u8 rb);

        void lfd(u8 frt, s16 d, u8 ra);
        void lfdu(u8 frt, s16 d, u8 ra);
        void lfdx(u8 frt, u8 ra, u8 rb);
        void lfdux(u8 frt, u8 ra, u8 rb);

        void stfs(u8 frs, s16 d, u8 ra);
        void stfsu(u8 frs, s16 d, u8 ra);
        void stfsx(u8 frs, u8 ra, u8 rb);
        void stfsux(u8 frs, u8 ra, u8 rb);

        void stfd(u8 frs, s16 d, u8 ra);
        void stfdu(u8 frs, s16 d, u8 ra);
        void stfdx(u8 frs, u8 ra, u8 rb);
        void stfdux(u8 frs, u8 ra, u8 rb);

        void stfiwx(u8 frs, u8 ra, u8 rb);

        // Move

        void fmr(u8 frt, u8 frb, bool rc, Register::CR &cr);
        void fabs(u8 frt, u8 frb, bool rc, Register::CR &cr);
        void fneg(u8 frt, u8 frb, bool rc, Register::CR &cr);
        void fnabs(u8 frt, u8 frb, bool rc, Register::CR &cr);

        // Math

        void fadd(u8 frt, u8 fra, u8 frb, bool rc, Register::CR &cr);
        void fadds(u8 frt, u8 fra, u8 frb, bool rc, Register::CR &cr);
        void fsub(u8 frt, u8 fra, u8 frb, bool rc, Register::CR &cr);
        void fsubs(u8 frt, u8 fra, u8 frb, bool rc, Register::CR &cr);
        void fmul(u8 frt, u8 fra, u8 frb, bool rc, Register::CR &cr);
        void fmuls(u8 frt, u8 fra, u8 frb, bool rc, Register::CR &cr);
        void fdiv(u8 frt, u8 fra, u8 frb, bool rc, Register::CR &cr);
        void fdivs(u8 frt, u8 fra, u8 frb, bool rc, Register::CR &cr);

        void fmadd(u8 frt, u8 fra, u8 frc, u8 frb, bool rc, Register::CR &cr);
        void fmadds(u8 frt, u8 fra, u8 frc, u8 frb, bool rc, Register::CR &cr);
        void fmsub(u8 frt, u8 fra, u8 frc, u8 frb, bool rc, Register::CR &cr);
        void fmsubs(u8 frt, u8 fra, u8 frc, u8 frb, bool rc, Register::CR &cr);
        void fnmadd(u8 frt, u8 fra, u8 frc, u8 frb, bool rc, Register::CR &cr);
        void fnmadds(u8 frt, u8 fra, u8 frc, u8 frb, bool rc, Register::CR &cr);
        void fnmsub(u8 frt, u8 fra, u8 frc, u8 frb, bool rc, Register::CR &cr);
        void fnmsubs(u8 frt, u8 fra, u8 frc, u8 frb, bool rc, Register::CR &cr);

        // Rounding and conversion

        void fsrp(u8 frt, u8 frb, bool rc, Register::CR &cr);
        void fctid(u8 frt, u8 frb, bool rc, Register::CR &cr);
        void fdtidz(u8 frt, u8 frb, bool rc, Register::CR &cr);
        void fcfid(u8 frt, u8 frb, bool rc, Register::CR &cr);

        // Compare

        void fcmpu(u8 bf, u8 fra, u8 frb, Register::CR &cr);
        void fcmpo(u8 bf, u8 fra, u8 frb, Register::CR &cr);

        // FPSCR

        void mffs(u8 frt, bool rc, Register::CR &cr);
        void mcrfs(u8 bf, u8 bfa, Register::CR &cr);
        void mtfsfi(u8 bf, u8 u, bool rc, Register::CR &cr);
        void mtfsf(u8 flm, u8 frb, bool rc, Register::CR &cr);
        void mtfsb0(u8 bt, bool rc, Register::CR &cr);
        void mtfsb1(u8 bt, bool rc, Register::CR &cr);

        // Extended

        void fsqrt(u8 frt, u8 frb, bool rc, Register::CR &cr);
        void fsqrts(u8 frt, u8 frb, bool rc, Register::CR &cr);
        void fre(u8 frt, u8 frb, bool rc, Register::CR &cr);
        void fres(u8 frt, u8 frb, bool rc, Register::CR &cr);
        void frsqrte(u8 frt, u8 frb, bool rc, Register::CR &cr);
        void frsqrtes(u8 frt, u8 frb, bool rc, Register::CR &cr);
        void fsel(u8 frt, u8 fra, u8 frc, u8 frb, bool rc, Register::CR &cr);

    private:
        Register::FPSCR m_fpscr;
        Register::FPR m_fpr[32];

        func_exception_cb m_exception_cb;
    };

}  // namespace Toolbox::Interpreter