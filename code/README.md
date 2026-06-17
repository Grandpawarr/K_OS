# K_OS

---

## 開機流程總覽

```
BIOS → MBR (0x7C00) → Loader (0x0A00) → Protected Mode → Paging → Kernel (0xC0001000)
```

---

## 磁碟佈局

| 磁區 (LBA) | 內容 | 大小 |
|---|---|---|
| 0 | MBR | 1 sector / 512 B |
| 1 ~ 4 | Loader | 4 sectors / 2 KB |
| 5 ~ 204 | Kernel ELF | 200 sectors / 100 KB |

---

## MBR（Master Boot Record）

### 功能說明

MBR 是磁碟第 0 個磁區（512 bytes），由 BIOS 在開機時自動載入至實體記憶體 `0x7C00`，並從該位址開始執行。  
MBR 的主要職責只有一件事：**把 Loader 從磁碟讀進記憶體，然後跳過去**。

| 功能 | 說明 |
|---|---|
| 初始化段暫存器 | DS / ES / SS / FS / GS 全部清為 0，確保定址正確 |
| 設定堆疊 | SP = 0x8000，往低位址成長，避免和 MBR 程式碼碰撞 |
| 印出訊息 | 使用 BIOS INT 0x10 在螢幕印出 `1 MBR` |
| 讀取 Loader | 以 LBA 模式透過 ATA PIO 從磁碟讀取 Loader 至 0x800 |
| 跳轉 Loader | `jmp 0x0A00`（0x800 起頭 512 bytes 為資料區，程式從 0xA00 開始） |

### read_hd — ATA PIO 讀碟流程

MBR 透過直接操作 ATA 控制器的 I/O port 讀磁碟，不依賴 BIOS：

```
step 1 │ port 0x1F2 ← sector count（cl）
step 2 │ port 0x1F3 ← LBA[7:0]
       │ port 0x1F4 ← LBA[15:8]
       │ port 0x1F5 ← LBA[23:16]
       │ port 0x1F6 ← 0xE0 | LBA[27:24]   ; 0xE0 = master drive + LBA mode
step 3 │ port 0x1F7 ← 0x20                ; read with retry
step 4 │ poll 0x1F7 直到 BSY=0 且 DRQ=1
step 5 │ 從 port 0x1F0 每次讀 2 bytes，共讀 sector_count × 256 次
```

### 運作流程

```
[BIOS 上電]
    │
    ▼
載入 MBR 至 0x7C00，跳轉執行
    │
    ├─ 初始化 DS/ES/SS/FS/GS = 0
    ├─ SP = 0x8000
    ├─ BIOS INT 0x10 取得游標位置
    ├─ BIOS INT 0x10 印出 "1 MBR"
    │
    ├─ eax = LOADER_START_SECTOR (1)
    ├─ bx  = LOADER_BASE_ADDR   (0x800)
    ├─ cl  = LOADER_SECTOR_COUNT (4)
    ├─ call read_hd              → Loader 載入至 0x800~0x9FF
    │
    └─ jmp LOADER_START_ADDR    → 0xA00
```

### 結構（512 bytes）

```
0x7C00 ┌─────────────────────────┐
       │ 程式碼（初始化+讀碟+跳轉） │
       │ msg db "1 MBR"           │
       │ read_hd function         │
       │ padding (0x00)           │
0x7DFE ├─────────────────────────┤
       │ 0x55 0xAA  (開機簽章)   │
0x7E00 └─────────────────────────┘
```

---

## Loader（bootloader 第二階段）

### 功能說明

Loader 由 MBR 載入，負責把 CPU 從 16-bit 實模式帶到 32-bit 保護模式並開啟分頁，最後把 Kernel ELF 從磁碟讀進來、解析並跳轉至核心進入點。

| 功能 | 說明 |
|---|---|
| 設定 GDT | 建立 code / data / video 三個段描述子 |
| 取得記憶體大小 | 呼叫 BIOS INT 0x15/E820 取得 ARDS，計算最大可用記憶體 |
| 進入保護模式 | 開啟 A20、載入 GDT、設定 CR0.PE=1，遠跳轉至 32-bit 程式碼 |
| 建立分頁表 | 建立二層 (PD + PT) 分頁結構，核心映射至虛擬位址 0xC0000000 |
| 開啟分頁 | 設定 CR3，設定 CR0.PG=1 |
| 更新 GDT | 分頁啟動後修正 video 段與 GDT 基底位址（加上 0xC0000000） |
| 讀取 Kernel | ATA PIO 讀取 200 個磁區至 0x70000 |
| 解析 ELF | 依據 ELF program header 把各段複製到正確的虛擬位址 |
| 跳轉核心 | `jmp 0xC0001000`（KERNEL_START_ADDR） |

### GDT 段描述子

| 索引 | 名稱 | 基底 | 大小 | 說明 |
|---|---|---|---|---|
| 0 | Null | — | — | 規定第一個必須為空 |
| 1 | Code | 0x0 | 4 GB | 32-bit 可執行，DPL=0 |
| 2 | Data | 0x0 | 4 GB | 32-bit 可讀寫，DPL=0 |
| 3 | Video | 0xB8000 | 32 KB | VGA 文字模式顯示緩衝 |

> 分頁啟動後，video_desc 的基底和 GDT 本身的位址都會加上 `0xC0000000`，使其在虛擬位址空間中正確可存取。

### 分頁表配置

採用 x86 二層分頁（Page Directory + Page Table），每頁 4 KB：

```
虛擬位址                 → 實體位址
─────────────────────────────────────────────────
0x0000_0000 ~ 0x000F_FFFF  → 0x0000_0000 ~ 0x000F_FFFF  (identity map，保護模式初期使用)
0xC000_0000 ~ 0xC00F_FFFF  → 0x0000_0000 ~ 0x000F_FFFF  (核心低 1MB 映射)
0xC010_0000 ~ 0xFFBF_FFFF  → 0x0010_2000 ~               (核心保留空間 PDE 769~1022)
0xFFC0_0000 ~ 0xFFFF_FFFF  → 0x0010_0000                 (PDE[1023] 指向自身，遞迴映射)
```

Page Directory（PD）位於 `0x0010_0000`：

| PDE 索引 | 指向 | 說明 |
|---|---|---|
| 0 | PT0 @ 0x101000 | 低 1MB identity map |
| 768 | PT0 @ 0x101000 | 核心 0xC0000000 映射同一個 PT0 |
| 769 ~ 1022 | PT1 ~ PT254 | 核心保留頁表空間 |
| 1023 | PD 自身 | 遞迴映射，方便日後動態修改頁表 |

### ELF 解析流程（kernel_decode_elf）

```
1. 從 ELF header 讀取：
     e_phoff    : program header table 起始位移
     e_phnum    : program header 數量
     e_phentsize: 每個 program header 的大小

2. 遍歷每個 program header：
     若 p_type == PT_LOAD (1)：
         mem_cpy(p_vaddr, KERNEL_BASE_ADDR + p_offset, p_filesz)

3. 核心各段被複製至正確的虛擬位址（0xC000_1000 起）
```

### 運作流程

```
[從 MBR 跳入 0xA00]
    │
    ├─ SP = LOADER_STACK_TOP (0x800)
    ├─ BIOS INT 0x10 印出 "2 Loader"
    │
    ├─ BIOS INT 0x15/E820 取得所有 ARDS
    │     └─ 找出最大記憶體位址 → 存入 ram_size
    │
    ├─ 開啟 A20 gate（port 0x92 bit1 = 1）
    ├─ lgdt [gdtr]
    ├─ CR0.PE = 1
    └─ far jump → p_mode_start [32-bit]
         │
         ├─ DS/ES/SS/FS/GS = SELECTOR_DATA
         ├─ ESP = LOADER_STACK_TOP
         ├─ 直寫 VGA 印出 "3 Protected"
         │
         ├─ call setup_page
         │     ├─ 清空 PD (0x100000)
         │     ├─ PDE[0] = PDE[768] = PT0 | P|RW|S
         │     ├─ PDE[769~1022] = PT1~PT254 | P|RW|S
         │     └─ PDE[1023] = PD | P|RW|S
         │     └─ 建立 PT0 的 256 個 PTE (0x0 ~ 0xFF000)
         │
         ├─ CR3 = PAGE_DIR_ADDR (0x100000)
         ├─ CR0.PG = 1
         │
         ├─ 更新 video_desc base += 0xC0000000
         ├─ 更新 gdtr.base    += 0xC0000000
         ├─ lgdt [gdtr]
         ├─ 直寫 VGA 印出 "4 Paging"
         │
         ├─ read_hd_32: sector 5, 200 sectors → 0x70000
         ├─ call kernel_decode_elf
         │     └─ 解析 ELF，複製各 PT_LOAD segment 至虛擬位址
         │
         ├─ ESP = KERNEL_STACK_TOP (0x9F000)
         ├─ 直寫 VGA 印出 "5 Kernel"
         └─ jmp KERNEL_START_ADDR → 0xC000_1000
```

---

## 記憶體配置圖

```
實體位址空間（開機至 Loader 階段）

0x0000_0000 ┌─────────────────────────────┐
            │ IVT (Interrupt Vector Table)│ 1 KB (BIOS)
0x0000_0400 ├─────────────────────────────┤
            │ BIOS Data Area              │ 256 B
0x0000_0500 ├─────────────────────────────┤
            │ (可用區域)                  │
0x0000_0800 ├─────────────────────────────┤
            │ Loader GDT + 資料區         │ 512 B  ← LOADER_BASE_ADDR
            │   0x800 : GDT table         │
            │   0x900 : ram_size          │
            │   0x904 : ards_nr           │
            │   0x908 : ards_buf          │
0x0000_0A00 ├─────────────────────────────┤
            │ Loader 程式碼               │ ← LOADER_START_ADDR
            │ (stack 從 0x800 往下成長)   │
0x0000_7C00 ├─────────────────────────────┤
            │ MBR                         │ 512 B
0x0000_7E00 ├─────────────────────────────┤
            │ MBR stack (往下成長至 0x7E00)│ ← SP = 0x8000
0x0000_8000 ├─────────────────────────────┤
            │ (可用區域)                  │
0x0007_0000 ├─────────────────────────────┤
            │ Kernel ELF 原始載入區       │ 100 KB (200 sectors)
            │ ← KERNEL_BASE_ADDR          │
0x0008_8800 ├─────────────────────────────┤
            │                             │
0x0009_F000 ├─────────────────────────────┤
            │ Kernel stack top            │ ← KERNEL_STACK_TOP
0x000A_0000 ├─────────────────────────────┤
            │ VGA / ROM / BIOS Reserved   │ 384 KB
0x000B_8000 │   └─ VGA 文字模式緩衝       │ 32 KB
0x0010_0000 ├─────────────────────────────┤ ← 1 MB 界線
            │ Page Directory (PD)         │ 4 KB ← PAGE_DIR_ADDR
0x0010_1000 ├─────────────────────────────┤
            │ Page Table 0 (PT0)          │ 4 KB ← PAGE_TABLE_ADDR
            │   涵蓋實體 0x0 ~ 0xFFFFF   │
0x0010_2000 ├─────────────────────────────┤
            │ Page Table 1 ~ 254          │ 254 × 4 KB
            │   (核心保留空間 PDE 769~1022)│
0x001F_F000 ├─────────────────────────────┤
            │ ...                         │
```

```
虛擬位址空間（分頁啟動後）

0x0000_0000 ┌─────────────────────────────┐
            │ 低 1MB identity map         │ → 實體 0x0 ~ 0xFFFFF
0x0010_0000 ├─────────────────────────────┤
            │ (未映射)                    │
0xC000_0000 ├─────────────────────────────┤
            │ 核心虛擬位址起點            │ → 實體 0x0 ~ 0xFFFFF
0xC000_1000 │   └─ KERNEL_START_ADDR      │ 核心進入點
0xFFBF_FFFF ├─────────────────────────────┤
0xFFC0_0000 ├─────────────────────────────┤
            │ 遞迴頁表映射區              │ PDE[1023] 指向 PD 自身
0xFFFF_FFFF └─────────────────────────────┘
```

---

## 實作流程指引

### 1. MBR 實作要點

- **必須恰好 512 bytes**，最後兩個 byte 為 `0x55 0xAA`（由 `times 510-($-$$) db 0` 補齊）
- `vstart=0x7C00`：告訴組譯器所有標號的基底位址，使 `msg` 等符號取得正確的絕對位址
- 段暫存器需要明確初始化，因為 BIOS 不保證其值
- ATA PIO 讀碟時 `eax` 需要先備份到 `esi`，因為 `out dx, al` 只用 AL 但 LBA 需要分次移位 EAX

### 2. Loader GDT 配置要點

- GDT 放在 Loader 的最前面（`vstart=LOADER_BASE_ADDR`），固定在 `0x800`
- `times 256-($-$$) db 0`：預留空間給 32 個 GDT 項目（最多擴充用）
- `times 512-($-$$) db 0`：確保程式碼從第二個磁區（`loader_start`）開始，對應 `LOADER_START_ADDR = 0xA00`
- ram_size / ards_nr / ards_buf 固定在 GDT 區塊後（`0x900`），Kernel 之後可以從固定位址讀取

### 3. 保護模式切換要點

- 開啟 A20：寫入 port `0x92` bit 1，否則只能定址 1 MB
- `lgdt` 需要在 CR0.PE 設定**前**完成
- 設定 CR0.PE=1 後立即做 **far jump**（`jmp SELECTOR_CODE:p_mode_start`）清除指令管線
- far jump 後才進入真正的 32-bit 保護模式，此後所有記憶體存取都經過 GDT

### 4. 分頁表建立要點

- 頁表本身存放於 1 MB 以上（`0x100000`），不會和開機區域衝突
- PDE[0] 和 PDE[768] 指向同一個 PT0，讓低位址和高位址（0xC0000000）同時可存取相同的實體記憶體，使保護模式轉分頁的跳轉不會 page fault
- PDE[1023] 指向 PD 自身（遞迴映射），日後核心可以透過特定虛擬位址修改頁表，不需要再知道實體位址
- 開啟分頁後，video_desc 的基底位址必須加上 `0xC0000000`，否則後續透過 SELECTOR_VIDEO 寫螢幕時定址錯誤

### 5. ELF 解析要點

- Kernel 以 ELF32 格式編譯，先整個載入至暫存區 (`0x70000`)，再由 Loader 解析
- 只處理 `p_type == PT_LOAD` 的段（type=1），略過其他段
- 複製目標位址是 `p_vaddr`（虛擬位址，如 `0xC0001000`），此時分頁已開啟，可以直接寫入
- 使用 `rep movsb` 做 byte-by-byte 複製（mem_cpy）

---

## 專案目錄結構

```
code/
├── boot/                  ← 開機階段（16-bit / 32-bit 組語）
│   ├── boot.inc           ← 共用常數（載入位址、磁區數、GDT selector、分頁位址）
│   ├── mbr.s              ← MBR：讀取 Loader 並跳轉
│   └── loader.s           ← Loader：保護模式、分頁、解析 ELF、跳轉核心
├── kernel/
│   ├── inc/               ← 核心標頭（doxygen 風格註解，介面說明見各 .h）
│   └── src/
│       ├── main.c         ← 核心進入點 main()
│       ├── init.c         ← kernel_init()：依序初始化各子系統
│       ├── print.s        ← VGA 文字模式輸出（組語實作）
│       ├── kernel.s       ← 中斷進入點 stub（VECTOR 巨集）與 syscall_entry
│       ├── interrupt.c    ← IDT、8259A PIC、handler 註冊與分派
│       ├── syscall_sys.c  ← kernel 端 syscall 分派表與實作
│       ├── memory.c       ← 實體/虛擬記憶體池、分頁配置、slab 配置器
│       └── bitmap.c       ← 位圖（記憶體池的底層資料結構）
├── lib/                   ← 通用函式庫
│   ├── inc/               ← stdint.h / stddef.h / stdbool.h / string.h / list.h
│   └── src/               ← string.c、list.c
├── usr/                   ← user 端程式碼
│   ├── inc/syscall_usr.h  ← user 端 syscall 編號與介面
│   └── src/syscall_usr.c  ← int 0x80 wrapper（_syscall0 ~ _syscall3）
├── test/                  ← 各子系統測試
│   ├── inc/test.h
│   └── src/               ← test_all.c（總開關）+ test_print / string / bitmap / list / memory / interrupt
├── makefile
├── build/                 ← .o 中間檔
└── out/                   ← mbr.bin / loader.bin / kernel.bin
```

---

## Kernel 架構

### 初始化流程

Loader 跳轉至 `0xC0001000` 後，從 `main()` 開始執行：

```
main()                          (kernel/src/main.c)
  │
  ├─ put_str("Kernel main()")
  │
  ├─ kernel_init()              (kernel/src/init.c)
  │     ├─ mem_init()           記憶體池 + slab 描述子初始化
  │     ├─ intr_init()          IDT 建立 + 8259A PIC 初始化
  │     └─ syscall_init()       syscall 分派表註冊
  │
  ├─ test_all()                 (test/src/test_all.c) 執行選定的子系統測試
  │
  └─ while(1)                   閒置迴圈（尚未實作排程器）
```

### 顯示輸出（print.s）

以組語直接操作 VGA 文字模式緩衝（透過 GDT 的 video 段選擇子，基底 `0xB8000`），
不依賴 BIOS（保護模式下 BIOS INT 0x10 已不可用）：

| 函式 | 功能 |
|---|---|
| `put_char(c)` | 印出單一字元；處理 `\n`、`\b`、游標推進與整頁捲動 |
| `put_str(s)` | 印出以 `\0` 結尾的字串 |
| `put_int(n)` | 以 16 進位印出 32-bit 整數 |
| `set_cursor(pos)` | 透過 CRT controller（port 0x3D4/0x3D5）設定游標 |
| `cls_screen()` | 清空畫面 |

### 中斷子系統（interrupt.c + kernel.s）

```
中斷發生
  │
  ▼
IDT[vec] → intr_%1_entry        (kernel.s VECTOR 巨集產生的 stub)
  │   ├─ push error code（CPU 沒推的補 0，對齊堆疊框架）
  │   ├─ 保存 ds/es/fs/gs + pushad
  │   ├─ 對 8259A 送 EOI
  │   ├─ push 中斷號
  │   └─ call [intr_func + vec*4]   → C handler
  │
  └─ 還原暫存器 → iretd
```

- **IDT**：共 `IDT_DESC_CNT (0x30)` 個組語 stub，涵蓋 0x00–0x1F CPU 例外與
  0x20–0x2F 外部中斷，另外在 `0x80` 安裝 syscall gate（DPL=3）。
- **C 端分派表 `intr_func[]`**：預設 handler 印出 vector number；
  各驅動用 `register_handler(vec, fn)` 換上自己的 handler。
- **8259A PIC**（`pic_init()`）：以 ICW1–ICW4 初始化，
  master 把 IRQ0–7 重映射到 `0x20–0x27`、slave 把 IRQ8–15 重映射到 `0x28–0x2F`（slave 接在 IRQ2）。
  IMR 遮罩 master = `0xFE`（僅開放 IRQ0 timer）、slave = `0xFF`（全遮罩）。
- **開關中斷**：`intr_set_status(true/false)` 對應 `sti`/`cli`，
  `intr_get_status()` 讀 EFLAGS.IF。

### 系統呼叫（int 0x80）

呼叫約定（ABI）：

| 暫存器 | 用途 |
|---|---|
| EAX | syscall 編號（`enum SyscallNR`，上限 `SYSCALL_MAX = 32`） |
| EBX / ECX / EDX | 第 1 / 2 / 3 個參數 |
| EAX（返回後） | 回傳值 |

```
user 端                              kernel 端
─────────────────────────────────────────────────────────────
test_syscall1(a)                     IDT[0x80] (DPL=3)
  (usr/src/syscall_usr.c)               │
  │                                     ▼
  └─ _syscall1: EAX=SYS_TEST1        syscall_entry (kernel.s)
     EBX=a → int $0x80   ──────────►   ├─ push 0（補 error code）
                                       ├─ 保存 ds/es/fs/gs + pushad
                                       ├─ push edx, ecx, ebx（3 個參數）
                                       ├─ call [syscall_func + eax*4]
                                       │    └─ sys_test_syscall1()  (syscall_sys.c)
                                       ├─ mov [esp+7*4], eax ← 回傳值寫回
     EAX = 0x9527        ◄──────────   │   pushad 保存的 EAX 槽位
                                       └─ 還原暫存器 → iretd
```

- **kernel 端**（`kernel/src/syscall_sys.c`）：`syscall_func[]` 為分派表，
  `syscall_init()` 在開中斷前註冊 `SYS_TEST0~3`（未註冊槽位為 NULL，誤呼叫會 page fault）。
- **user 端**（`usr/src/syscall_usr.c`）：以 inline assembly 巨集
  `_syscall0 ~ _syscall3` 包裝 `int $0x80`，提供 `test_syscall0~3()`。
  `usr/inc/syscall_usr.h` 的編號必須與 kernel 的 `enum SyscallNR` 保持同步。
- 測試回傳值以魔術數字驗證：`0x9527 / 0x9528 / 0x9529`。

### 記憶體管理（memory.c）

初始化（`mem_init()`）讀取 Loader 存在 `0x900`（`MEM_SIZE_ADDR`）的實體記憶體大小，
建立三個池與 slab 描述子：

| 池 | 說明 |
|---|---|
| `k_p_pool` | kernel 實體池：可用頁面的一半（上限 0x40000 頁 = 1 GB） |
| `u_p_pool` | user 實體池：剩餘可用頁面（user process 尚未實作） |
| `k_v_pool` | kernel 虛擬位址池：從 `K_HEAP_START (0xC0100000)` 起 |

底層皆以 bitmap 管理（1 bit = 4 KB 頁）：

```
0xC0076000  實體池 bitmap 區（保留 128 KB，可管理至 4 GB 實體記憶體）
0xC0096000  kernel 虛擬池 bitmap（32 KB，涵蓋 0xC0000000 起 1 GB 虛擬空間）
0xC0100000  kernel heap 起點（低 1 MB 保留給核心程式碼 / MMIO）
```

兩層配置介面：

- **頁面層級** — `page_malloc(pf, vaddr, cnt)` / `page_free(pf, vaddr, cnt)`：
  從虛擬池取位址、實體池取頁框，再寫 PTE 綁定兩者；
  修改頁表時利用 PDE[1023] 遞迴映射取得 PDE/PTE 的虛擬位址
  （`PDE_IDX(addr)` = bits 31–22、`PTE_IDX(addr)` = bits 21–12）。
  釋放時清 PTE 並以 `invlpg` 失效 TLB。
- **位元組層級（slab 風格）** — `sys_malloc(size)` / `sys_free(p)`：
  - `size ≤ 1024`：依 7 種 block 大小（16/32/64/128/256/512/1024 bytes）取整，
    從對應 `mem_block_desc` 的 free_list 取 block；free_list 空了就再切一頁新 arena。
  - `size > 1024`：直接配置整數頁，頁首放 arena header。
  - `sys_free()` 由位址回推 arena header 判斷歸還方式；
    arena 內 block 全數歸還時整頁釋放回池。

### 通用函式庫（lib/）

| 模組 | 內容 |
|---|---|
| `string.c` | `memset / memcpy / memcmp / strcpy / strlen / strcmp / strchr / strcat …` |
| `list.c` | 侵入式雙向鏈結串列（head/tail 哨兵節點）；`elem2entry` 巨集由節點回推外層結構，供 slab free_list 等使用 |
| `bitmap.c`（位於 kernel/） | `bitmap_init / set / check / acquire / release`：記憶體池的底層配置機制，`acquire` 尋找連續 n 個空閒 bit |

### 測試框架（test/）

每個子系統有對應的 `test_*.c`，由 `test/src/test_all.c` 統一開關（取消註解即啟用）：

```c
void test_all(void)
{
    // test_string();
    // test_bitmap();
    // test_list();
    // test_memory(1, 0);
    test_intr();        /* 中斷 + int 0x80 系統呼叫測試 */
}
```

`test_intr()` 會開啟中斷、依序測試 `test_syscall0~3()` 並驗證參數傳遞與回傳值；
測試後畫面出現 `vector number: 0x20` 為 timer（IRQ0）進入預設 handler，屬預期行為。

---

## 建置與寫入

```bash
# 編譯所有目標
make all

# 寫入磁碟映像（Bochs 開機磁碟 ../bochs-2.8/60mb.img）
dd if=out/mbr.bin    of=../bochs-2.8/60mb.img bs=512 count=1  conv=notrunc
dd if=out/loader.bin of=../bochs-2.8/60mb.img bs=512 seek=1   conv=notrunc
dd if=out/kernel.bin of=../bochs-2.8/60mb.img bs=512 seek=5   conv=notrunc

# 啟動模擬器（在 debugger 提示符輸入 c 開始執行）
cd ../bochs-2.8 && bochs -q -f .bochsrc
```

> 完整的環境安裝與使用說明見上層目錄的 [`../readme.md`](../readme.md)。

---

## 開發紀錄

ch03 loader (part2)
- jump to kernel: source code

ch04 kernel
- makefile: easy to compile all function
- Print log by print.s
- String function library

ch05 Memory Management
- bitmap implement
- link list implement
- memory management 
- test

ch05 Interrupt
- interrupt implement
- test interrupt
- interrupt - 8296 Implement
- test 8295
- interrupt - syscall 
- test syscall
- syscall_usr：user 端 int 0x80 wrapper（usr/src/syscall_usr.c），test_syscall0~3 驗證通過