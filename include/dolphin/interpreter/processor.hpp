#pragma once

#include "core/core.hpp"
#include "core/memory.hpp"

#include "registers.hpp"
#include "serial.hpp"

namespace Toolbox::Interpreter {

    // Dolphin Emulator
    enum class ExceptionCause {
        EXCEPTION_DECREMENTER         = (1 << 0),
        EXCEPTION_SYSCALL             = (1 << 1),
        EXCEPTION_EXTERNAL_INT        = (1 << 2),
        EXCEPTION_DSI                 = (1 << 3),
        EXCEPTION_ISI                 = (1 << 4),
        EXCEPTION_ALIGNMENT           = (1 << 5),
        EXCEPTION_FPU_UNAVAILABLE     = (1 << 6),
        EXCEPTION_PROGRAM             = (1 << 7),
        EXCEPTION_PERFORMANCE_MONITOR = (1 << 8),

        EXCEPTION_FAKE_MEMCHECK_HIT = (1 << 9),
    };

#define PROC_INVALID_MSG(proc, instr, reason) "[" #proc "] " #instr ": " reason

    using proc_ret_cb       = std::function<void()>;
    using proc_exception_cb = std::function<void(ExceptionCause reason)>;
    using proc_invalid_cb   = std::function<void(std::string reason)>;

    enum class BOHint : u8 {
        BO_NONE,
        BO_RESERVED,
        BO_UNLIKELY,
        BO_LIKELY,
    };

    enum class SyncType : u8 {
        SYNC_HEAVY,
        SYNC_LIGHT,
        SYNC_HEAVY_ORDERED,
        SYNC_RESERVED,
    };

    enum class DataCacheHintType : u8 {
        HINT_THIS_BLOCK,
        HINT_THIS_STREAM_ALL,
        HINT_THIS_BLOCK_ALL,
        HINT_STREAM_DESCRIPT = 8,
    };

    inline bool MemoryContainsVAddress(const Buffer &buffer, u32 address) {
        return address >= 0x80000000 && address < 0x80000000 + buffer.size();
    }

    inline bool MemoryContainsPAddress(const Buffer &buffer, s32 address) {
        return address >= 0 && address < (s32)buffer.size();
    }

    inline bool IsRegValid(u8 reg) { return reg < 32; }

    class SystemProcessor {
    public:
        friend class SystemDolphin;

        SystemProcessor()                        = default;
        SystemProcessor(const SystemProcessor &) = default;
        SystemProcessor(SystemProcessor &&)      = default;

        SystemProcessor &operator=(SystemProcessor &&) = default;

        void onException(proc_exception_cb cb) { m_exception_cb = cb; }
        void onInvalid(proc_invalid_cb cb) { m_invalid_cb = cb; }

    private:
        // Storage control

        void icbi(u8 ra, u8 rb, Buffer &storage) {}
        void dcbi(u8 ra, u8 rb, Buffer &storage) {}
        void dcbt(u8 ra, u8 rb, DataCacheHintType th, Buffer &storage) {}
        void dcbf(u8 ra, u8 rb, bool l, Buffer &storage) {}
        void dcbtst(u8 ra, u8 rb, Buffer &storage) {}
        void dcbz(u8 ra, u8 rb, Buffer &storage) {}
        void dcbst(u8 ra, u8 rb, Buffer &storage) {}

        // Sync - order

        void isync() {}
        void sync(SyncType l) {}
        void eieio() {}

        // Interrupt flow

        void sc(u8 lev) {
            m_srr0 = m_pc;
            m_srr1 = m_msr & 0b10000111110000001111111101110011;
            // TODO: assign values to msr, execute system call exception, etc
            // https://fail0verflow.com/media/files/ppc_750cl.pdf
        }
        void rfi() {}

        Register::PC m_pc{};
        Register::PC m_last_pc{};
        Register::TB m_tb{};
        Register::MSR m_msr{};
        Register::DAR m_dar{};
        Register::DSISR m_dsisr{};
        Register::SRR0 m_srr0{};
        Register::SRR1 m_srr1{};

        proc_exception_cb m_exception_cb;
        proc_invalid_cb m_invalid_cb;
    };

    class BranchProcessor {
    public:
        friend class SystemDolphin;

        BranchProcessor()                        = default;
        BranchProcessor(const BranchProcessor &) = default;
        BranchProcessor(BranchProcessor &&)      = default;

        BranchProcessor &operator=(BranchProcessor &&) = default;

        void onReturn(proc_ret_cb cb) { m_return_cb = cb; }
        void onException(proc_exception_cb cb) { m_exception_cb = cb; }
        void onInvalid(proc_invalid_cb cb) { m_invalid_cb = cb; }

    protected:
        void b(s32 target_addr, bool aa, bool lk, Register::PC &pc);
        void bc(s32 target_addr, u8 bo, u8 bi, bool aa, bool lk, Register::PC &pc);
        void bclr(u8 bo, u8 bi, bool lk, Register::PC &pc);
        void bcctr(u8 bo, u8 bi, bool lk, Register::PC &pc);

        void crand(u8 bt, u8 ba, u8 bb);
        void crandc(u8 bt, u8 ba, u8 bb);
        void creqv(u8 bt, u8 ba, u8 bb);
        void cror(u8 bt, u8 ba, u8 bb);
        void crorc(u8 bt, u8 ba, u8 bb);
        void crnand(u8 bt, u8 ba, u8 bb);
        void crnor(u8 bt, u8 ba, u8 bb);
        void crxor(u8 bt, u8 ba, u8 bb);

        void mcrf(u8 bt, u8 ba);
        void mcrfs(u8 bt, u8 ba);

    private:
        Register::CR m_cr{};
        Register::LR m_lr{};
        Register::CTR m_ctr{};

        proc_ret_cb m_return_cb;
        proc_exception_cb m_exception_cb;
        proc_invalid_cb m_invalid_cb;
    };

    class FixedPointProcessor {
    public:
        friend class SystemDolphin;

        FixedPointProcessor()                            = default;
        FixedPointProcessor(const FixedPointProcessor &) = default;
        FixedPointProcessor(FixedPointProcessor &&)      = default;

        FixedPointProcessor &operator=(FixedPointProcessor &&) = default;

        void onException(proc_exception_cb cb) { m_exception_cb = cb; }
        void onInvalid(proc_invalid_cb cb) { m_invalid_cb = cb; }

    protected:
        // Memory
        void lbz(u8 rt, s16 d, u8 ra, Buffer &storage);
        void lbzu(u8 rt, s16 d, u8 ra, Buffer &storage);
        void lbzx(u8 rt, u8 ra, u8 rb, Buffer &storage);
        void lbzux(u8 rt, u8 ra, u8 rb, Buffer &storage);

        void lhz(u8 rt, s16 d, u8 ra, Buffer &storage);
        void lhzu(u8 rt, s16 d, u8 ra, Buffer &storage);
        void lhzx(u8 rt, u8 ra, u8 rb, Buffer &storage);
        void lhzux(u8 rt, u8 ra, u8 rb, Buffer &storage);
        void lha(u8 rt, s16 d, u8 ra, Buffer &storage);
        void lhau(u8 rt, s16 d, u8 ra, Buffer &storage);
        void lhax(u8 rt, u8 ra, u8 rb, Buffer &storage);
        void lhaux(u8 rt, u8 ra, u8 rb, Buffer &storage);

        void lwz(u8 rt, s16 d, u8 ra, Buffer &storage);
        void lwzu(u8 rt, s16 d, u8 ra, Buffer &storage);
        void lwzx(u8 rt, u8 ra, u8 rb, Buffer &storage);
        void lwzux(u8 rt, u8 ra, u8 rb, Buffer &storage);

        void stb(u8 rs, s16 d, u8 ra, Buffer &storage);
        void stbu(u8 rs, s16 d, u8 ra, Buffer &storage);
        void stbx(u8 rs, u8 ra, u8 rb, Buffer &storage);
        void stbux(u8 rs, u8 ra, u8 rb, Buffer &storage);

        void sth(u8 rs, s16 d, u8 ra, Buffer &storage);
        void sthu(u8 rs, s16 d, u8 ra, Buffer &storage);
        void sthx(u8 rs, u8 ra, u8 rb, Buffer &storage);
        void sthux(u8 rs, u8 ra, u8 rb, Buffer &storage);

        void stw(u8 rs, s16 d, u8 ra, Buffer &storage);
        void stwu(u8 rs, s16 d, u8 ra, Buffer &storage);
        void stwx(u8 rs, u8 ra, u8 rb, Buffer &storage);
        void stwux(u8 rs, u8 ra, u8 rb, Buffer &storage);

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
        void mullw(u8 rt, u8 ra, u8 rb, bool oe, bool rc, Register::CR &cr);
        void mullhw(u8 rt, u8 ra, u8 rb, bool rc, Register::CR &cr);
        void mullhwu(u8 rt, u8 ra, u8 rb, bool rc, Register::CR &cr);

        void divw(u8 rt, u8 ra, u8 rb, bool oe, bool rc, Register::CR &cr);
        void divwu(u8 rt, u8 ra, u8 rb, bool oe, bool rc, Register::CR &cr);

        // Compare

        void cmpi(u8 bf, bool l, u8 ra, s16 si, Register::CR &cr);
        void cmp(u8 bf, bool l, u8 ra, u8 rb, Register::CR &cr);
        void cmpli(u8 bf, bool l, u8 ra, u16 ui, Register::CR &cr);
        void cmpl(u8 bf, bool l, u8 ra, u8 rb, Register::CR &cr);

        // Trap

        void twi(u8 to, u8 ra, s16 si);
        void tw(u8 to, u8 ra, u8 rb);

        // Logic

        void andi(u8 ra, u8 rs, u16 ui, Register::CR &cr);
        void andis(u8 ra, u8 rs, u16 ui, Register::CR &cr);
        void ori(u8 ra, u8 rs, u16 ui);
        void oris(u8 ra, u8 rs, u16 ui);
        void xori(u8 ra, u8 rs, u16 ui);
        void xoris(u8 ra, u8 rs, u16 ui);
        void and_(u8 ra, u8 rs, u8 rb, bool rc, Register::CR &cr);
        void or_(u8 ra, u8 rs, u8 rb, bool rc, Register::CR &cr);
        void xor_(u8 ra, u8 rs, u8 rb, bool rc, Register::CR &cr);
        void nand_(u8 ra, u8 rs, u8 rb, bool rc, Register::CR &cr);
        void nor_(u8 ra, u8 rs, u8 rb, bool rc, Register::CR &cr);
        void eqv_(u8 ra, u8 rs, u8 rb, bool rc, Register::CR &cr);
        void andc(u8 ra, u8 rs, u8 rb, bool rc, Register::CR &cr);
        void orc(u8 ra, u8 rs, u8 rb, bool rc, Register::CR &cr);

        void extsb(u8 ra, u8 rs, bool rc, Register::CR &cr);
        void extsh(u8 ra, u8 rs, bool rc, Register::CR &cr);

        void cntlzw(u8 ra, u8 rs, bool rc, Register::CR &cr);

        // Rotate | shift

        void rlwinm(u8 ra, u8 rs, u8 sh, u8 mb, u8 me, bool rc, Register::CR &cr);
        void rlwnm(u8 ra, u8 rs, u8 rb, u8 mb, u8 me, bool rc, Register::CR &cr);
        void rlwimi(u8 ra, u8 rs, u8 sh, u8 mb, u8 me, bool rc, Register::CR &cr);

        void slw(u8 ra, u8 rs, u8 rb, bool rc, Register::CR &cr);
        void srw(u8 ra, u8 rs, u8 rb, bool rc, Register::CR &cr);
        void srawi(u8 ra, u8 rs, u8 sh, bool rc, Register::CR &cr);
        void sraw(u8 ra, u8 rs, u8 rb, bool rc, Register::CR &cr);

        // SPRs

        void mcrxr(Register::CR &cr, u8 crf);
        void mtspr(Register::SPRType spr, u8 rs, Register::LR &lr, Register::CTR &ctr);
        void mfspr(Register::SPRType spr, u8 rt, const Register::LR &lr, const Register::CTR &ctr);
        void mftb(u8 rt, s16 tbr, const Register::TB &tb);
        void mtcrf(u16 fxm, u8 rs, Register::CR &cr);
        void mfcr(u8 rt, const Register::CR &cr);
        void mtmsr(u8 rs, Register::MSR &msr);
        void mfmsr(u8 rt, const Register::MSR &msr);

        // External control

        void eciwx(u8 rt, u8 ra, u8 rb, Buffer &storage);
        void ecowx(u8 rs, u8 ra, u8 rb, Buffer &storage);

    private:
        Register::XER m_xer{};
        Register::GPR m_gpr[32]{};

        proc_exception_cb m_exception_cb;
        proc_invalid_cb m_invalid_cb;
    };

    class FloatingPointProcessor {
    public:
        friend class SystemDolphin;

        FloatingPointProcessor()                               = default;
        FloatingPointProcessor(const FloatingPointProcessor &) = default;
        FloatingPointProcessor(FloatingPointProcessor &&)      = default;

        FloatingPointProcessor &operator=(FloatingPointProcessor &&) = default;

        void onException(proc_exception_cb cb) { m_exception_cb = cb; }
        void onInvalid(proc_invalid_cb cb) { m_invalid_cb = cb; }

    protected:
        // Memory

        void lfs(u8 frt, s16 d, u8 ra, Register::GPR gpr[32], Buffer &storage);
        void lfsu(u8 frt, s16 d, u8 ra, Register::GPR gpr[32], Buffer &storage);
        void lfsx(u8 frt, u8 ra, u8 rb, Register::GPR gpr[32], Buffer &storage);
        void lfsux(u8 frt, u8 ra, u8 rb, Register::GPR gpr[32], Buffer &storage);

        void lfd(u8 frt, s16 d, u8 ra, Register::GPR gpr[32], Buffer &storage);
        void lfdu(u8 frt, s16 d, u8 ra, Register::GPR gpr[32], Buffer &storage);
        void lfdx(u8 frt, u8 ra, u8 rb, Register::GPR gpr[32], Buffer &storage);
        void lfdux(u8 frt, u8 ra, u8 rb, Register::GPR gpr[32], Buffer &storage);

        void stfs(u8 frs, s16 d, u8 ra, Register::GPR gpr[32], Buffer &storage);
        void stfsu(u8 frs, s16 d, u8 ra, Register::GPR gpr[32], Buffer &storage);
        void stfsx(u8 frs, u8 ra, u8 rb, Register::GPR gpr[32], Buffer &storage);
        void stfsux(u8 frs, u8 ra, u8 rb, Register::GPR gpr[32], Buffer &storage);

        void stfd(u8 frs, s16 d, u8 ra, Register::GPR gpr[32], Buffer &storage);
        void stfdu(u8 frs, s16 d, u8 ra, Register::GPR gpr[32], Buffer &storage);
        void stfdx(u8 frs, u8 ra, u8 rb, Register::GPR gpr[32], Buffer &storage);
        void stfdux(u8 frs, u8 ra, u8 rb, Register::GPR gpr[32], Buffer &storage);

        void stfiwx(u8 frs, u8 ra, u8 rb, Register::GPR gpr[32], Buffer &storage);

        // Move

        void fmr(u8 frt, u8 frb, bool rc, Register::CR &cr, Register::MSR &msr,
                 Register::SRR1 &srr1);
        void fabs(u8 frt, u8 frb, bool rc, Register::CR &cr, Register::MSR &msr,
                  Register::SRR1 &srr1);
        void fneg(u8 frt, u8 frb, bool rc, Register::CR &cr, Register::MSR &msr,
                  Register::SRR1 &srr1);
        void fnabs(u8 frt, u8 frb, bool rc, Register::CR &cr, Register::MSR &msr,
                   Register::SRR1 &srr1);

        // Math

        void fadd(u8 frt, u8 fra, u8 frb, bool rc, Register::CR &cr, Register::MSR &msr,
                  Register::SRR1 &srr1);
        void fadds(u8 frt, u8 fra, u8 frb, bool rc, Register::CR &cr, Register::MSR &msr,
                   Register::SRR1 &srr1);
        void fsub(u8 frt, u8 fra, u8 frb, bool rc, Register::CR &cr, Register::MSR &msr,
                  Register::SRR1 &srr1);
        void fsubs(u8 frt, u8 fra, u8 frb, bool rc, Register::CR &cr, Register::MSR &msr,
                   Register::SRR1 &srr1);
        void fmul(u8 frt, u8 fra, u8 frb, bool rc, Register::CR &cr, Register::MSR &msr,
                  Register::SRR1 &srr1);
        void fmuls(u8 frt, u8 fra, u8 frb, bool rc, Register::CR &cr, Register::MSR &msr,
                   Register::SRR1 &srr1);
        void fdiv(u8 frt, u8 fra, u8 frb, bool rc, Register::CR &cr, Register::MSR &msr,
                  Register::SRR1 &srr1);
        void fdivs(u8 frt, u8 fra, u8 frb, bool rc, Register::CR &cr, Register::MSR &msr,
                   Register::SRR1 &srr1);

        void fmadd(u8 frt, u8 fra, u8 frc, u8 frb, bool rc, Register::CR &cr, Register::MSR &msr,
                   Register::SRR1 &srr1);
        void fmadds(u8 frt, u8 fra, u8 frc, u8 frb, bool rc, Register::CR &cr, Register::MSR &msr,
                    Register::SRR1 &srr1);
        void fmsub(u8 frt, u8 fra, u8 frc, u8 frb, bool rc, Register::CR &cr, Register::MSR &msr,
                   Register::SRR1 &srr1);
        void fmsubs(u8 frt, u8 fra, u8 frc, u8 frb, bool rc, Register::CR &cr, Register::MSR &msr,
                    Register::SRR1 &srr1);
        void fnmadd(u8 frt, u8 fra, u8 frc, u8 frb, bool rc, Register::CR &cr, Register::MSR &msr,
                    Register::SRR1 &srr1);
        void fnmadds(u8 frt, u8 fra, u8 frc, u8 frb, bool rc, Register::CR &cr, Register::MSR &msr,
                     Register::SRR1 &srr1);
        void fnmsub(u8 frt, u8 fra, u8 frc, u8 frb, bool rc, Register::CR &cr, Register::MSR &msr,
                    Register::SRR1 &srr1);
        void fnmsubs(u8 frt, u8 fra, u8 frc, u8 frb, bool rc, Register::CR &cr, Register::MSR &msr,
                     Register::SRR1 &srr1);

        // Rounding and conversion

        void frsp(u8 frt, u8 frb, bool rc, Register::CR &cr, Register::MSR &msr,
                  Register::SRR1 &srr1);
        void fctiw(u8 frt, u8 frb, bool rc, Register::CR &cr, Register::MSR &msr,
                   Register::SRR1 &srr1);
        void fctiwz(u8 frt, u8 frb, bool rc, Register::CR &cr, Register::MSR &msr,
                    Register::SRR1 &srr1);

        // Compare

        void fcmpu(u8 bf, u8 fra, u8 frb, Register::CR &cr, Register::MSR &msr,
                   Register::SRR1 &srr1);
        void fcmpo(u8 bf, u8 fra, u8 frb, Register::CR &cr, Register::MSR &msr,
                   Register::SRR1 &srr1);

        // FPSCR

        void mffs(u8 frt, bool rc, Register::CR &cr, Register::MSR &msr, Register::SRR1 &srr1);
        void mcrfs(u8 bf, u8 bfa, Register::CR &cr, Register::MSR &msr, Register::SRR1 &srr1);
        void mtfsfi(u8 bf, u8 u, bool rc, Register::CR &cr, Register::MSR &msr,
                    Register::SRR1 &srr1);
        void mtfsf(u8 flm, u8 frb, bool rc, Register::CR &cr, Register::MSR &msr,
                   Register::SRR1 &srr1);
        void mtfsb0(u8 bt, bool rc, Register::CR &cr, Register::MSR &msr, Register::SRR1 &srr1);
        void mtfsb1(u8 bt, bool rc, Register::CR &cr, Register::MSR &msr, Register::SRR1 &srr1);

        // Extended

        void fres(u8 frt, u8 frb, bool rc, Register::CR &cr, Register::MSR &msr,
                  Register::SRR1 &srr1);
        void frsqrte(u8 frt, u8 frb, bool rc, Register::CR &cr, Register::MSR &msr,
                     Register::SRR1 &srr1);
        void frsqrtes(u8 frt, u8 frb, bool rc, Register::CR &cr, Register::MSR &msr,
                      Register::SRR1 &srr1);
        void fsel(u8 frt, u8 fra, u8 frc, u8 frb, bool rc, Register::CR &cr, Register::MSR &msr,
                  Register::SRR1 &srr1);

        // Paired-Single

        void helperQuantize(Buffer &storage, u32 addr, u32 instI, u32 instRS, u32 instW);
        void helperDequantize(Buffer &storage, u32 addr, u32 instI, u32 instRD, u32 instW);

        void ps_l(u8 frt, s16 d, u8 i, u8 ra, u8 w, Register::GPR gpr[32], Buffer &storage);
        void ps_lu(u8 frt, s16 d, u8 i, u8 ra, u8 w, Register::GPR gpr[32], Buffer &storage);
        void ps_lx(u8 frt, u8 ix, u8 ra, u8 rb, u8 wx, Register::GPR gpr[32], Buffer &storage);
        void ps_lux(u8 frt, u8 ix, u8 ra, u8 rb, u8 wx, Register::GPR gpr[32], Buffer &storage);
        void ps_st(u8 frt, s16 d, u8 i, u8 ra, u8 w, Register::GPR gpr[32], Buffer &storage);
        void ps_stu(u8 frt, s16 d, u8 i, u8 ra, u8 w, Register::GPR gpr[32], Buffer &storage);
        void ps_stx(u8 frt, u8 ix, u8 ra, u8 rb, u8 wx, Register::GPR gpr[32], Buffer &storage);
        void ps_stux(u8 frt, u8 ix, u8 ra, u8 rb, u8 wx, Register::GPR gpr[32], Buffer &storage);

        void ps_cmpo0(u8 bf, u8 fra, u8 frb, Register::CR &cr, Register::MSR &msr,
                      Register::SRR1 &srr1);
        void ps_cmpu0(u8 bf, u8 fra, u8 frb, Register::CR &cr, Register::MSR &msr,
                      Register::SRR1 &srr1);
        void ps_cmpo1(u8 bf, u8 fra, u8 frb, Register::CR &cr, Register::MSR &msr,
                      Register::SRR1 &srr1);
        void ps_cmpu1(u8 bf, u8 fra, u8 frb, Register::CR &cr, Register::MSR &msr,
                      Register::SRR1 &srr1);

        // Move

        void ps_mr(u8 frt, u8 frb, bool rc, Register::CR &cr, Register::MSR &msr,
                   Register::SRR1 &srr1);
        void ps_abs(u8 frt, u8 frb, bool rc, Register::CR &cr, Register::MSR &msr,
                    Register::SRR1 &srr1);
        void ps_neg(u8 frt, u8 frb, bool rc, Register::CR &cr, Register::MSR &msr,
                    Register::SRR1 &srr1);
        void ps_nabs(u8 frt, u8 frb, bool rc, Register::CR &cr, Register::MSR &msr,
                     Register::SRR1 &srr1);

        // Math

        void ps_add(u8 frt, u8 fra, u8 frb, bool rc, Register::CR &cr, Register::MSR &msr,
                    Register::SRR1 &srr1);
        void ps_sub(u8 frt, u8 fra, u8 frb, bool rc, Register::CR &cr, Register::MSR &msr,
                    Register::SRR1 &srr1);
        void ps_mul(u8 frt, u8 fra, u8 frb, bool rc, Register::CR &cr, Register::MSR &msr,
                    Register::SRR1 &srr1);
        void ps_muls0(u8 frt, u8 fra, u8 frc, bool rc, Register::CR &cr, Register::MSR &msr,
                      Register::SRR1 &srr1);
        void ps_muls1(u8 frt, u8 fra, u8 frc, bool rc, Register::CR &cr, Register::MSR &msr,
                      Register::SRR1 &srr1);
        void ps_div(u8 frt, u8 fra, u8 frb, bool rc, Register::CR &cr, Register::MSR &msr,
                    Register::SRR1 &srr1);

        void ps_msub(u8 frt, u8 fra, u8 frc, u8 frb, bool rc, Register::CR &cr, Register::MSR &msr,
                     Register::SRR1 &srr1);
        void ps_madds0(u8 frt, u8 fra, u8 frc, u8 frb, bool rc, Register::CR &cr,
                       Register::MSR &msr, Register::SRR1 &srr1);
        void ps_madds1(u8 frt, u8 fra, u8 frc, u8 frb, bool rc, Register::CR &cr,
                       Register::MSR &msr, Register::SRR1 &srr1);
        void ps_madd(u8 frt, u8 fra, u8 frc, u8 frb, bool rc, Register::CR &cr, Register::MSR &msr,
                     Register::SRR1 &srr1);
        void ps_nmsub(u8 frt, u8 fra, u8 frc, u8 frb, bool rc, Register::CR &cr, Register::MSR &msr,
                      Register::SRR1 &srr1);
        void ps_nmadd(u8 frt, u8 fra, u8 frc, u8 frb, bool rc, Register::CR &cr, Register::MSR &msr,
                      Register::SRR1 &srr1);

        void ps_sum0(u8 frt, u8 fra, u8 frc, u8 frb, bool rc, Register::CR &cr, Register::MSR &msr,
                     Register::SRR1 &srr1);

        void ps_sum1(u8 frt, u8 fra, u8 frc, u8 frb, bool rc, Register::CR &cr, Register::MSR &msr,
                     Register::SRR1 &srr1);

        void ps_res(u8 frt, u8 frb, bool rc, Register::CR &cr, Register::MSR &msr,
                    Register::SRR1 &srr1);
        void ps_rsqrte(u8 frt, u8 frb, bool rc, Register::CR &cr, Register::MSR &msr,
                       Register::SRR1 &srr1);
        void ps_sel(u8 frt, u8 fra, u8 frc, u8 frb, bool rc, Register::CR &cr, Register::MSR &msr,
                    Register::SRR1 &srr1);

        void ps_merge00(u8 frt, u8 fra, u8 frb, bool rc, Register::CR &cr, Register::MSR &msr,
                        Register::SRR1 &srr1);
        void ps_merge01(u8 frt, u8 fra, u8 frb, bool rc, Register::CR &cr, Register::MSR &msr,
                        Register::SRR1 &srr1);
        void ps_merge10(u8 frt, u8 fra, u8 frb, bool rc, Register::CR &cr, Register::MSR &msr,
                        Register::SRR1 &srr1);
        void ps_merge11(u8 frt, u8 fra, u8 frb, bool rc, Register::CR &cr, Register::MSR &msr,
                        Register::SRR1 &srr1);

    private:
        Register::FPSCR m_fpscr{};
        Register::FPR m_fpr[32]{};
        Register::GQR m_gqr[8]{};

        proc_exception_cb m_exception_cb;
        proc_invalid_cb m_invalid_cb;
    };

}  // namespace Toolbox::Interpreter