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
        }

        void signalEvaluateFunction(u32 function_ptr, u8 gpr_argc, u32 *gpr_argv, u8 fpr_argc,
                                    f64 *fpr_argv, func_ret_cb on_return,
                                    func_exception_cb on_exception) {
            if (m_evaluating.load()) {
                std::unique_lock<std::mutex> lk(m_eval_mutex);
                m_eval_condition.wait(lk);
            }

            m_system_proc.m_pc = function_ptr;
            m_branch_proc.m_lr = 0xDEADBEEF;  // Sentinel for return detection

            for (u8 gpr_arg = 0; gpr_arg < gpr_argc; ++gpr_arg) {
                m_fixed_proc.m_gpr[gpr_arg + 3] = gpr_argv[gpr_arg];
            }

            for (u8 fpr_arg = 0; fpr_arg < fpr_argc; ++fpr_arg) {
                m_float_proc.m_fpr[fpr_arg + 3].fill(fpr_argv[fpr_arg]);
            }

            m_eval_ready.store(true);
        }

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
        void evaluatePairedSingleSubOp(u32 instr);
        void evaluateControlFlowSubOp(u32 instr);
        void evaluateFixedSubOp(u32 instr);
        void evaluateFloatSingleSubOp(u32 instr);
        void evaluateFloatSubOp(u32 instr);

        void internalReturnCB() {
            // If the LR matches the sentinel we know we've returned from
            // the function rather than a child function
            if (m_branch_proc.m_lr == 0xDEADBEEF) {
                Register::RegisterSnapshot snapshot;
                m_evaluating.store(false);
                m_eval_ready.store(false);
                m_system_return_cb(snapshot);
                m_eval_condition.notify_all();
            }
        }

        void internalExceptionCB(ExceptionCause cause) {
            Register::RegisterSnapshot snapshot;
            m_evaluating.store(false);
            m_eval_ready.store(false);
            m_system_exception_cb(m_system_proc.m_pc, cause, snapshot);
            m_eval_condition.notify_all();
        }

        void internalInvalidCB(const std::string &reason) {
            Register::RegisterSnapshot snapshot;
            m_evaluating.store(false);
            m_eval_ready.store(false);
            m_system_invalid_cb(m_system_proc.m_pc, reason, snapshot);
            m_eval_condition.notify_all();
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

        func_ret_cb m_system_return_cb;
        func_exception_cb m_system_exception_cb;
        func_invalid_cb m_system_invalid_cb;
    };

}  // namespace Toolbox::Interpreter