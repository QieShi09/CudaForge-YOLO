#include <QApplication>
#include "mainwindow.h"
#include <signal.h>
#include <execinfo.h>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <vector>
#include <cuda_runtime.h>

static void crash_handler(int sig, siginfo_t*, void*)
{
    // 使用 write() 而不是 fprintf，保证 async-signal-safe
    const char* msg1 = "\n========== CRASH SIGNAL ==========\n";
    write(STDERR_FILENO, msg1, __builtin_strlen(msg1));

    // Print CUDA last error via printf (not AS-safe but useful)
    cudaError_t ce = cudaGetLastError();
    if (ce != cudaSuccess) {
        fprintf(stderr, "  CUDA last error: %s\n", cudaGetErrorString(ce));
        fflush(stderr);
    }

    void* frames[64];
    int n = backtrace(frames, 64);
    backtrace_symbols_fd(frames, n, STDERR_FILENO);

    const char* msg2 = "==========================================\n";
    write(STDERR_FILENO, msg2, __builtin_strlen(msg2));

    // 使用默认行为杀死进程（产生 core dump）
    struct sigaction sa{};
    sa.sa_handler = SIG_DFL;
    sigaction(sig, &sa, nullptr);
    raise(sig);
}

static void install_crash_handlers()
{
    // [\u5173\u952e] \u4f7f\u7528\u5907\u7528\u6808\uff0c\u9632\u6b62\u6808\u6ea2\u51fa\u65f6\u4fe1\u53f7\u5904\u7406\u5668\u672c\u8eab\u65e0\u6cd5\u8fd0\u884c
    // \u5982\u679c\u4e0d\u8bbe\u7f6e sigaltstack\uff0c\u6808\u6ea2\u51fa\u5bfc\u81f4\u7684 SIGSEGV \u4e2d\u4fe1\u53f7\u5904\u7406\u5668\u4e5f\u4f1a\u518d\u5ea6\u5d29\u6e83\uff0c\u5bfc\u81f4\u65e0\u4efb\u4f55\u65e5\u5fd7\u8f93\u51fa
    static std::vector<uint8_t> alt_stack_buf(64 * 1024); // 64KB \u5907\u7528\u6808
    stack_t alt_stack{};
    alt_stack.ss_sp = alt_stack_buf.data();
    alt_stack.ss_size = alt_stack_buf.size();
    alt_stack.ss_flags = 0;
    if (sigaltstack(&alt_stack, nullptr) != 0) {
        perror("sigaltstack failed");
    }

    struct sigaction sa{};
    sa.sa_sigaction = crash_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_SIGINFO | SA_NODEFER | SA_ONSTACK; // SA_ONSTACK: \u5728\u5907\u7528\u6808\u4e0a\u8fd0\u884c
    sigaction(SIGSEGV, &sa, nullptr);
    sigaction(SIGABRT, &sa, nullptr);
    sigaction(SIGFPE,  &sa, nullptr);
    sigaction(SIGBUS,  &sa, nullptr);
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("CudaForge-YOLO");
    app.setApplicationVersion("1.0.0");

    // 在 Qt 初始化完成之后安装信号处理器，覆盖 Qt 的 crash handler
    install_crash_handlers();

    MainWindow w;
    w.show();

    
    return app.exec();
}