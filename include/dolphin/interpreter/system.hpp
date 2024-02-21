#pragma once

#include "core/threaded.hpp"
#include "gui/settings.hpp"
#include "instructions/forms.hpp"
#include "processor.hpp"
#include "registers.hpp"

namespace Toolbox::Interpreter {

    using func_ret_cb       = std::function<void(const Register::RegisterSnapshot &snapshot)>;
    using func_exception_cb = std::function<void(u32 bad_instr_ptr, ExceptionCause cause,
                                                 const Register::RegisterSnapshot &snapshot)>;
    using func_invalid_cb   = std::function<void(u32 bad_instr_ptr, const std::string &reason,
                                               const Register::RegisterSnapshot &snapshot)>;

    class SystemDolphin : public Threaded<void> {
    public:
        SystemDolphin() {
            m_branch_proc.onReturn(TOOLBOX_BIND_EVENT_FN(internalReturnCB));
            m_branch_proc.onException(TOOLBOX_BIND_EVENT_FN(internalExceptionCB));
            m_fixed_proc.onException(TOOLBOX_BIND_EVENT_FN(internalExceptionCB));
            m_float_proc.onException(TOOLBOX_BIND_EVENT_FN(internalExceptionCB));
            m_system_proc.onException(TOOLBOX_BIND_EVENT_FN(internalExceptionCB));
            m_branch_proc.onInvalid(TOOLBOX_BIND_EVENT_FN(internalInvalidCB));
            m_fixed_proc.onInvalid(TOOLBOX_BIND_EVENT_FN(internalInvalidCB));
            m_float_proc.onInvalid(TOOLBOX_BIND_EVENT_FN(internalInvalidCB));
            m_system_proc.onInvalid(TOOLBOX_BIND_EVENT_FN(internalInvalidCB));
            m_storage.alloc(0x1800000);
        }

        void signalEvaluateFunction(u32 function_ptr, u8 gpr_argc, u32 *gpr_argv, u8 fpr_argc,
                                    f64 *fpr_argv, func_ret_cb on_return);

        void setMemoryBuffer(void *buf, size_t size) { m_storage.setBuf(buf, size); }
        void setStackPointer(u32 sp) { m_fixed_proc.m_gpr[1] = sp; }
        void setGlobalsPointerR(u32 r2) { m_fixed_proc.m_gpr[2] = r2; }
        void setGlobalsPointerRW(u32 r13) { m_fixed_proc.m_gpr[13] = r13; }

        void onReturn(func_ret_cb cb) { m_system_return_cb = cb; }
        void onException(func_exception_cb cb) { m_system_exception_cb = cb; }
        void onInvalid(func_invalid_cb cb) { m_system_invalid_cb = cb; }

    protected:
        void tRun(void *param) override {
            (void)param;

            while (!tIsKilled()) {
                AppSettings &settings = SettingsManager::instance().getCurrentProfile();

                if (!m_eval_ready.load()) {
                    std::this_thread::sleep_for(
                        std::chrono::milliseconds(settings.m_dolphin_refresh_rate));
                    continue;
                }

                evaluateFunction();
            }
        }

        void evaluateFunction();
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
            m_evaluating.store(false);
            m_eval_ready.store(false);
            m_system_exception_cb((u32)m_system_proc.m_pc, cause, snapshot);
            m_eval_condition.notify_all();
        }

        void internalInvalidCB(const std::string &reason) {
            Register::RegisterSnapshot snapshot = createSnapshot();
            m_evaluating.store(false);
            m_eval_ready.store(false);
            m_system_invalid_cb((u32)m_system_proc.m_pc, reason, snapshot);
            m_eval_condition.notify_all();
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
        std::atomic<bool> m_eval_ready;
        std::atomic<bool> m_evaluating;
        std::condition_variable m_eval_condition;

        func_ret_cb m_system_return_cb          = [](const Register::RegisterSnapshot &) {};
        func_exception_cb m_system_exception_cb = [](u32, ExceptionCause,
                                                     const Register::RegisterSnapshot &) {};
        func_invalid_cb m_system_invalid_cb     = [](u32, const std::string &,
                                                 const Register::RegisterSnapshot &) {};
    };

}  // namespace Toolbox::Interpreter