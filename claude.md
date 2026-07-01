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

## 2026-06-24：修正 test_thread.c 號誌死結
- **test/src/test_thread.c**：`thread_aaa` 的 `sema_up`/`mutex_unlock` 原本在 `while(1)` 迴圈外（永遠執行不到），導致它 `sema_down` 後從不釋放號誌；A 在第二輪 `sema_down` 阻塞，B~F 也全部阻塞 → ready list 清空 → 退回 `g_idle_task`，畫面卡在 idle。已將釋放搬進迴圈內，與 B~F 一致。
- 釐清：cursor 80 的 "idle" 是 `kthread_init()` 建立的核心 `g_idle_task`，與 test 內被註解的 `task_idle` 無關。

## 2026-06-24：補上 lock.h / lock.c 註解
- **kernel/inc/lock.h**：external function 一律在此補完整 Doxygen（spinlock/mutex/semaphore 三組 init/lock/unlock、`xchg`、三個 struct 與成員）。
- **kernel/src/lock.c**：只補 `@file` 標頭、internal `lock_waiter` struct（說明 waiter 配在阻塞者自己堆疊上為何安全），以及 spin_lock 自旋重試、mutex/sema「掛 wait_list→放鎖→block→醒來重檢」迴圈的 inline 註解；不重複 .h 的函式說明。
- **驗證**：`make all` 編譯連結成功。

## 2026-06-24：補上 print / stdio 註解
- **kernel/inc/print.h**：five external VGA 輸出函式補完整 Doxygen（put_char/put_str/put_int/set_cursor/cls_screen）。
- **kernel/src/print.s**：補 `@file` 標頭，說明 video selector、CRTC cursor port、每格 2 bytes、80x25 捲動機制（既有逐行註解保留）。
- **lib/inc/stdio.h**：external `vsprintf`/`sprintf`、`va_start/va_arg/va_end` 巨集、`va_list` typedef 補 Doxygen。
- **lib/src/stdio.c**：補 `@file` 標頭與 internal static `itoa` 完整說明（遞迴先處理商數→數字自然順序、cursor 前進、不寫 NUL）；vsprintf/sprintf 說明留在 .h 不重複。
- **驗證**：`make all` 編譯連結成功。

## 2026-06-24：stdio.h 同步 vsprintf 修正
- **lib/inc/stdio.h**：使用者已修好 stdio.c（補 `*out='\0'`、改回傳 `out - str`）。同步更新 `vsprintf`/`sprintf` 註解：說明會寫結尾 NUL、`@return` 改為「寫入字元數（不含 NUL）」。
- print.h 上一輪已補註解，內容與 print.s 一致（put_char 捲動/BS/CR/LF、put_int 為十六進位去前導零等），本輪確認無需再改。
- **驗證**：`make all` 編譯連結成功。

## 2026-06-27：新增 .clang-format
- 根目錄新增 `.clang-format`：`BasedOnStyle: LLVM` + `ColumnLimit: 80` + `IndentWidth: 4`（對齊 `.vscode/settings.json` 的 `tabSize: 4`）。cpptools 存檔時會自動套用。

## 2026-06-27：string Doxygen 註解
- **lib/inc/string.h**：原本無註解，補上全部函式的 Doxygen（@brief/@param/@return）。
- **lib/src/string.c**：原有 Doxygen 但 @param/@return 為空，全部填上說明。僅註解，無邏輯變更。
- **lib/src/string.c**：修 `memcmp` bug — n 個 byte 全相等時原本未 return（UB），補上 `return 0;`。單獨編譯零警告通過。

## 2026-06-27：printk.h Doxygen 註解
- **kernel/inc/printk.h**：為 `printk` / `printk_init` 補上 Doxygen 註解（含 thread-safe、需先呼叫 init、1024-byte buffer 限制）。僅註解，無邏輯變更。

## 2026-06-29：io.h 註解修正（outb→outw）
- **kernel/inc/io.h**：`outw` 註解原本還寫 `outb`／`AL`／8-bit byte，改為 `outw`／`AX`／16-bit word。
- `outsw`／`insw` 的 bytes 全部改為 words（字串傳輸計數單位是 word）；並修 `insw` 註解內的 `insb` 筆誤為 `insw`。僅註解。

## 2026-06-29：ide.c Doxygen 註解
- **kernel/src/ide.c**：為 `@file`、`ptn_table`／`boot_sector` struct、`g_ide_ch`／`ptn_list` 全域、及全部 internal 函式補上 Doxygen 註解（風格對齊 io.h）。
- 順手修兩處編譯阻斷筆誤：`ide_set_drive` 內 `| z DRIVE_LBA` 的多餘 `z`；`static void_set_lba(...)` 改為 `static void ide_set_lba(...)`。
- 未處理（超出本次範圍，待確認）：程式用 `inw`／`inb` 讀 port，但 io.h 目前只定義 `inb`，`inw` 尚未宣告。

## 2026-06-30：ide.c / ide.h 補完 Doxygen
- **kernel/inc/ide.h**：為 `ide_ptn`／`ide_hd`／`ide_ch` struct 及成員補 Doxygen（含 `irq` 存的是 remap 後的 IDT vector 0x20+IRQ line 而非原始 IRQ 線號的提醒）、`ptn_list` 全域、`ide_read`／`ide_write`／`ide_init` 宣告補 @brief/@param。
- **kernel/src/ide.c**：為先前無註解的 `ide_get_partition`／`ide_ch_init`／`ide_read`／`ide_write`／`ide_init` 補 Doxygen（風格對齊既有 io.h／ide.c 註解）。`ide_set_drive` 既有 brief 已正確（select/set active drive），維持不變。
- 僅註解，無邏輯變更。
