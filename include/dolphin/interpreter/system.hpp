#pragma once

#include "core/threaded.hpp"
#include "gui/settings.hpp"
#include "instructions/forms.hpp"
#include "dolphin/process.hpp"
#include "processor.hpp"
#include "registers.hpp"

namespace Toolbox::Interpreter {

    using func_ret_cb       = std::function<void(const Register::RegisterSnapshot &snapshot)>;
    using func_exception_cb = std::function<void(u32 bad_instr_ptr, ExceptionCause cause,
                                                 const Register::RegisterSnapshot &snapshot)>;
    using func_invalid_cb   = std::function<void(u32 bad_instr_ptr, const std::string &reason,
                                               const Register::RegisterSnapshot &snapshot)>;

    class SystemDolphin {
    public:
        SystemDolphin();

        // Use this when you want to evaluate directly upon Dolphin's memory
        explicit SystemDolphin(const Dolphin::DolphinCommunicator &);

        Register::RegisterSnapshot evaluateFunction(u32 function_ptr, u8 gpr_argc, u32 *gpr_argv,
                                                    u8 fpr_argc, f64 *fpr_argv);

        Buffer &getMemoryBuffer() { return m_storage; }
        void setMemoryBuffer(void *buf, size_t size) { m_storage.setBuf(buf, size); }
        void setStackPointer(u32 sp) { m_fixed_proc.m_gpr[1] = sp; }
        void setGlobalsPointerR(u32 r2) { m_fixed_proc.m_gpr[2] = r2; }
        void setGlobalsPointerRW(u32 r13) { m_fixed_proc.m_gpr[13] = r13; }

        void onReturn(func_ret_cb cb) { m_system_return_cb = cb; }
        void onException(func_exception_cb cb) { m_system_exception_cb = cb; }
        void onInvalid(func_invalid_cb cb) { m_system_invalid_cb = cb; }

        void applyMemory(const void *buf, size_t size) {
            std::memcpy(m_storage.buf<void>(), buf, size);
        }

        template <typename T> T read(u32 address) const {
            T data;
            readBytes(reinterpret_cast<char *>(&data), address, sizeof(T));
            return std::byteswap<T>(data);
        }

        template <typename T> void write(u32 address, const T &value) {
            T swapped_v = std::byteswap<T>(value);
            writeBytes(reinterpret_cast<const char *>(std::addressof(swapped_v)), address,
                       sizeof(T));
        }

        template <typename T> void write(u32 address, T &&value) {
            T swapped_v = std::byteswap<T>(value);
            writeBytes(reinterpret_cast<const char *>(std::addressof(swapped_v)), address,
                       sizeof(T));
        }

        void readBytes(char *buf, u32 address, size_t size) const {
            const char *true_address = m_storage.buf<const char>() + (address & 0x7FFFFFFF);
            memcpy(buf, true_address, size);
        }

        void writeBytes(const char *buf, u32 address, size_t size) {
            char *true_address = m_storage.buf<char>() + (address & 0x7FFFFFFF);
            memcpy(true_address, buf, size);
        }

    protected:
        void evalLoop();
        void evaluateInstruction();
        u32 evaluatePairedSingleSubOp(u32 instr);
        u32 evaluateControlFlowSubOp(u32 instr);
        u32 evaluateFixedSubOp(u32 instr);
        u32 evaluateFloatSingleSubOp(u32 instr);
        u32 evaluateFloatSubOp(u32 instr);

        void internalReturnCB() {
            // If the LR matches the sentinel we know we've returned from
            // the function rather than a child function
            if (m_branch_proc.m_lr == 0xDEADBEEF) {
                Register::RegisterSnapshot snapshot = createSnapshot();
                m_system_return_cb(snapshot);
            }
        }

        void internalExceptionCB(ExceptionCause cause) {
            Register::RegisterSnapshot snapshot = createSnapshot();
            m_evaluating                        = false;
            m_system_exception_cb((u32)m_system_proc.m_pc, cause, snapshot);
        }

        void internalInvalidCB(const std::string &reason) {
            Register::RegisterSnapshot snapshot = createSnapshot();
            m_evaluating                        = false;
            m_system_invalid_cb((u32)m_system_proc.m_pc, reason, snapshot);
        }

        Register::RegisterSnapshot createSnapshot() const {
            Register::RegisterSnapshot snapshot;
            snapshot.m_cr  = m_branch_proc.m_cr;
            snapshot.m_ctr = m_branch_proc.m_ctr;
            snapshot.m_lr  = m_branch_proc.m_lr;

            snapshot.m_pc    = m_system_proc.m_pc;
            snapshot.m_dar   = m_system_proc.m_dar;
            snapshot.m_dsisr = m_system_proc.m_dsisr;
            snapshot.m_msr   = m_system_proc.m_msr;
            snapshot.m_srr0  = m_system_proc.m_srr0;
            snapshot.m_srr1  = m_system_proc.m_srr1;
            snapshot.m_tb    = m_system_proc.m_tb;

            std::memcpy(snapshot.m_gpr, m_fixed_proc.m_gpr, sizeof(Register::GPR) * 32);
            snapshot.m_xer = m_fixed_proc.m_xer;

            std::memcpy(snapshot.m_fpr, m_float_proc.m_fpr, sizeof(Register::FPR) * 32);
            snapshot.m_fpscr = m_float_proc.m_fpscr;
            return snapshot;
        }

    private:
        Buffer m_storage;

        BranchProcessor m_branch_proc;
        FixedPointProcessor m_fixed_proc;
        FloatingPointProcessor m_float_proc;
        SystemProcessor m_system_proc;

        std::mutex m_eval_mutex;
        bool m_evaluating;

        func_ret_cb m_system_return_cb          = [](const Register::RegisterSnapshot &) {};
        func_exception_cb m_system_exception_cb = [](u32, ExceptionCause,
                                                     const Register::RegisterSnapshot &) {};
        func_invalid_cb m_system_invalid_cb     = [](u32, const std::string &,
                                                 const Register::RegisterSnapshot &) {};
    };

}  // namespace Toolbox::Interpreter