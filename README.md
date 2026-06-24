# K_OS

K_OS 是一個從零開始打造的 x86（IA-32）32-bit 教學用作業系統，從 MBR、Bootloader、保護模式、分頁機制一路實作到核心的記憶體管理、中斷子系統與系統呼叫（`int 0x80`）。開發與測試環境使用 [Bochs 2.8](https://bochs.sourceforge.io/) x86 模擬器。

```
BIOS → MBR (0x7C00) → Loader (0x0A00) → Protected Mode → Paging → Kernel (0xC0001000)
```

> 核心架構與各子系統的詳細說明請見 [`code/ARCHITECTURE.md`](code/ARCHITECTURE.md)。

---

## 專案結構

```
K_OS/
├── readme.md          ← 本文件（安裝、架設、使用）
├── bochs-2.8/         ← Bochs 2.8 原始碼（已編譯），含模擬器設定檔與磁碟映像
│   ├── .bochsrc       ← Bochs 設定檔（CPU / 記憶體 / 硬碟 / 開機方式）
│   └── 60mb.img       ← 60 MB 硬碟映像（K_OS 的開機磁碟）
└── code/              ← K_OS 原始碼
    ├── boot/          ← MBR 與 Loader（NASM 組語）
    ├── kernel/        ← 核心（中斷、記憶體管理、系統呼叫、VGA 輸出）
    ├── lib/           ← 通用函式庫（string、list）
    ├── usr/           ← User 端程式碼（syscall wrapper）
    ├── test/          ← 各子系統測試
    ├── makefile
    ├── build/         ← 編譯中間檔（.o）
    └── out/           ← 編譯產出（mbr.bin / loader.bin / kernel.bin）
```

---

## 環境需求

| 工具 | 用途 | 版本參考 |
|---|---|---|
| `nasm` | 組譯 MBR / Loader / 核心組語 | 任意近代版本 |
| `gcc`（含 32-bit multilib） | 編譯核心 C 程式碼（`-m32`） | 任意近代版本 |
| `ld`（binutils） | 連結核心 ELF | 任意近代版本 |
| `bochs` + `bximage` | x86 模擬器與磁碟映像工具 | 2.8 |

Ubuntu / Debian 安裝編譯工具：

```bash
sudo apt update
sudo apt install build-essential nasm gcc-multilib
```

---

## 安裝架設

### 1. 編譯並安裝 Bochs 2.8

本專案的 `bochs-2.8/` 已附上原始碼。需要啟用 **debugger** 與 X11 顯示（編譯前請先安裝 X11 開發套件，如 `xorg-dev` / `libgtk2.0-dev`）：

```bash
cd bochs-2.8
./configure --enable-debugger --enable-x86-debugger --enable-iodebug \
            --enable-all-optimizations --enable-cpu-level=6 \
            --enable-pci --enable-plugins --enable-show-ips \
            --with-all-libs --with-x --with-x11
make -j$(nproc)
sudo make install        # 安裝至 /usr/local/bin/bochs、/usr/local/bin/bximage
```

> 若 `bochs-2.8/` 內已有編譯好的 `bochs` 執行檔且系統已安裝過，可略過此步驟。

### 2. 建立硬碟映像（已存在可略過）

```bash
cd bochs-2.8
bximage
# 選擇 1 (Create new floppy or hard disk image)
# hd → flat → 容量 60M → 檔名 60mb.img
```

`.bochsrc` 中對應的設定為：

```
ata0-master: type=disk, mode=flat, path="60mb.img"
boot: disk
```

---

## 編譯 K_OS

```bash
cd code
make all
```

會依序產出三個檔案到 `out/`：

| 檔案 | 內容 | 寫入磁區 (LBA) |
|---|---|---|
| `mbr.bin` | MBR 開機磁區（512 bytes，結尾 `0x55AA`） | 0 |
| `loader.bin` | 第二階段 Bootloader | 1 ~ 4 |
| `kernel.bin` | 核心 ELF | 5 ~ 204 |

清除編譯產物：

```bash
make clean
```

---

## 寫入磁碟映像

把三個 binary 依磁碟佈局寫入開機映像，`bochs-2.8/gen.sh` 已包好這三道指令：

```bash
cd bochs-2.8
sh gen.sh
```

等同於：

```bash
cd code
dd if=out/mbr.bin    of=../bochs-2.8/60mb.img bs=512 count=1 conv=notrunc
dd if=out/loader.bin of=../bochs-2.8/60mb.img bs=512 seek=1  conv=notrunc
dd if=out/kernel.bin of=../bochs-2.8/60mb.img bs=512 seek=5  conv=notrunc
```

---

## 啟動 K_OS

```bash
cd bochs-2.8
bochs -q -f .bochsrc
```

由於 Bochs 編譯時啟用了 debugger，啟動後會先停在 debugger 提示符，輸入 `c`（continue）開始模擬：

```
<bochs:1> c
```

常用 debugger 指令：

| 指令 | 功能 |
|---|---|
| `c` | 繼續執行 |
| `Ctrl-C` | 中斷執行回到 debugger |
| `b 0x7c00` | 在實體位址設中斷點（如 MBR 進入點） |
| `lb 0xC0001000` | 在線性位址設中斷點（如核心進入點） |
| `xp /10wx 0xb8000` | 檢視實體記憶體（VGA 文字緩衝） |
| `info tab` / `info gdt` / `info idt` | 檢視分頁表 / GDT / IDT |
| `q` | 離開 Bochs |

### 預期畫面

開機過程依序印出 `1 MBR` → `2 Loader` → `3 Protected` → `4 Paging` → `5 Kernel`，
進入核心後執行 `test/` 內的測試（預設為中斷與系統呼叫測試）：

```
Kernel main()
...
syscall_init()
intr_get_status: 1
test syscall0()
sys_test_syscall0() called
test_syscall1()
sys_test_syscall1()
argument 1: 0x111
ret= 0x9527
...
ret= 0x9529
```

之後畫面出現 `vector number: 0x20` 訊息屬正常現象：開啟中斷後 timer（IRQ0）觸發，目前尚未註冊 timer handler，因此進入預設 handler。

---

## 開發流程

修改程式碼後的完整迭代：

```bash
cd code
make clean && make all
cd ../bochs-2.8
sh gen.sh
bochs -q -f .bochsrc
```

### 切換測試項目

要執行哪些子系統測試由 `code/test/src/test_all.c` 控制，取消註解即可啟用：

```c
void test_all(void)
{
    // test_string();
    // test_bitmap();
    // test_list();
    // test_memory(1, 0);
    test_intr();        /* 中斷 + 系統呼叫測試 */
}
```

---

## 疑難排解

| 症狀 | 原因與解法 |
|---|---|
| `gcc: error: ... -m32` 或缺 32-bit header | 未安裝 multilib：`sudo apt install gcc-multilib` |
| `error: 'bool' cannot be defined via 'typedef'` | GCC 15 起預設 C23，`bool` 成為關鍵字，與 `lib/inc/stdbool.h` 衝突。makefile 的 `CFLAGS` 已加 `-std=gnu11`，勿移除（`.vscode` 的 `cStandard` 只影響 IntelliSense，不影響實際編譯） |
| `No rule to make target 'build/xxx.o'` | makefile 列出的 `.o` 找不到對應原始檔，確認 `.c`/`.s` 位於規則涵蓋的 `*/src/` 目錄（曾發生檔案誤放 `inc/` 且檔名帶隱藏字元） |
| 編譯 Bochs 報 `X11/Xlib.h: No such file or directory` | 缺 X11 開發標頭：`sudo apt install libx11-dev` |
| Bochs 一啟動就退出，log 出現 `>>PANIC<< Could not open wave output device` | `.bochsrc` 的 sound 指向已淘汰的 `/dev/dsp`，已改為 `sound: driver=dummy` |
| Bochs 讀不到 MBR / 無法開機 | `.bochsrc` 的 `ata0-master` 路徑須指向 `60mb.img`（曾誤指不存在的 `30M.sample`）；且映像必須先以 `dd if=/dev/zero` 或 `bximage` 建立完整 60 MB，僅靠 `gen.sh` 寫入的 37 KB 會使磁碟幾何偵測失敗 |
| Bochs 啟動報 `ROM: couldn't open ROM image` | `BXSHARE` 指不到 BIOS 檔，`export BXSHARE=/usr/local/share/bochs` 或指向 `bochs-2.8/bios/` |
| 畫面停在 `2 Loader` 或黑畫面 | `60mb.img` 內容過舊，重新執行 `sh gen.sh` |
| Bochs 出現 panic 詢問視窗 | `.bochsrc` 設定 `panic: action=ask`，可依提示選擇 continue 或進 debugger 排查 |
| `ld: cannot find -lgcc` 等連結錯誤 | 同 multilib 問題，確認 `build-essential` 與 `gcc-multilib` 已安裝 |

> 排查 Bochs 啟動問題時，先看 `bochs-2.8/bochsout.txt`。

---

## 目前進度

- ch03 loader (part2)
  - jump to kernel: source code
- ch04 kernel
  - makefile: easy to compile all function
  - print log by print.s
  - string function library
- ch05 memory management
  - bitmap implement
  - linked list implement
  - memory management
  - test
- ch05 interrupt
  - interrupt implement + test
  - 8259A PIC implement + test
  - syscall (`int 0x80`) implement + test
  - syscall_usr (user-side wrapper) for test

- cho06 scheduler
  - system timer + test_timer
  - schedule + test_thread
  - lock + test_thread(add lock)
  - printk + test_printk(待看完測試code)
