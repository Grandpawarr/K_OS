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

## 建置與寫入

```bash
# 編譯所有目標
make all

# 寫入磁碟映像（假設 disk.img 已建立）
dd if=out/mbr.bin    of=disk.img bs=512 count=1  conv=notrunc
dd if=out/loader.bin of=disk.img bs=512 seek=1   conv=notrunc
dd if=out/kernel.bin of=disk.img bs=512 seek=5   conv=notrunc
```

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