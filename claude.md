# K_OS

## Claude 修改紀錄（2026-06-23）

為 thread / timer / sched / switch 補上英文 Doxygen 註解（僅註解，無邏輯變更）：

- **kernel/inc/thread.h**：新增 `@file` 標頭；為 `threadfn`、`enum task_status`（逐項）、`struct task_struct`（逐欄位，含 `kstack` 必為首、`stack_magic` 必為末的說明）、外部變數與全部 `kthread_*` 函式宣告補上完整 `@brief`/`@param`/`@return`。
- **kernel/src/thread.c**：新增 `@file` 標頭；為 `kernel_thread`、`main_thread_init`、`idle_thread` 三個 static 函式補完整說明；外部函式各加一行 `@brief ... @see thread.h`（避免與標頭重複）。
- **kernel/inc/sched.h**：新增 `@file` 標頭；為 `SCHED_RR_TIME_SLICE`、`switch_to` 外部宣告、`schedule` 補完整 Doxygen。
- **kernel/src/sched.c**：新增 `@file` 標頭；`schedule` 加 `@brief ... @see`。
- **kernel/src/timer.c**：`jiffies` 全域、四個換算函式、`msleep`/`usleep`、`timer_init` 補 `@brief`。
- **kernel/inc/timer.h**：為先前無註解的 `msleep`/`usleep` 宣告補完整 Doxygen。
- **kernel/src/switch.s**：補上組語版 `@file` 標頭與 `switch_to` 例程說明（含 4 次 push 後的堆疊偏移、esp 存入 PCB 首欄 kstack 的機制）。

**驗證**：`make all` 編譯連結成功，無錯誤。
