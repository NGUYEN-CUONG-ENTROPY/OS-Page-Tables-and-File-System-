# OS: Page Tables and File System Labs (xv6-labs-2024)

Đây là dự án thực hành về **Hệ thống File** và **Bảng Trang (Page Table)** trong một hệ điều hành Unix-like (xv6) chạy trên RISC-V.

## 📋 Nội dung Dự án

Dự án bao gồm hai phần lab chính:

### 1. **File System Lab** (`file_system/`)
Triển khai hệ thống tệp tin với hỗ trợ **doubly indirect blocks** cho phép lưu trữ các tệp lớn hơn.

**Các tính năng chính:**
- **Inode Structure**: Cấu trúc dữ liệu quản lý tệp với:
  - 11 direct blocks (địa chỉ trực tiếp)
  - 1 indirect block (gián tiếp cấp 1)
  - 1 doubly indirect block (gián tiếp cấp 2) - **NEW**
  
- **Disk Layout**:
  ```
  [ Boot Block | Super Block | Log | Inode Blocks | Bitmap | Data Blocks ]
  ```

- **Phiên bản sửa:**
  - Thêm doubly indirect block support vào hàm `bmap()` để hỗ trợ tệp tin lớn hơn
  - Thêm test `bigfile` để kiểm tra tệp tin lớn
  - Sửa lỗi trong hàm `bzero()` (sử dụng `bwrite` thay vì `log_write`)
  - Cập nhật `itrunc()` để xóa doubly indirect blocks

**File quan trọng:**
- `kernel/fs.h` - Định nghĩa cấu trúc tệp tin
- `kernel/fs.c` - Logic hệ thống tệp
- `kernel/file.c` - Quản lý tệp mở
- `user/bigfile.c` - Test case cho tệp tin lớn

### 2. **Page Table Lab** (`page_table/`)
Triển khai quản lý bộ nhớ ảo với các tính năng nâng cao.

**Các tính năng chính:**
- **Virtual Memory Mapping**:
  - Kernel page table initialization
  - User space page table creation
  - Address translation (Virtual → Physical)
  
- **RISC-V Sv39 Scheme**:
  - 3 level page table
  - 512 PTEs (Page Table Entries) per table
  - 4KB page size
  
- **Tối ưu hóa:**
  - Superpages (trang lớn) hỗ trợ
  - Walk leaf optimization
  - Lab flag: `LAB_PGTBL`

**File quan trọng:**
- `kernel/vm.c` - Virtual memory manager
- `kernel/vm.h` - Định nghĩa VM
- `kernel/memlayout.h` - Bố cục bộ nhớ
- `kernel/riscv.h` - RISC-V specific

## 🏗️ Cấu trúc Dự án

```
OS-Page-Tables-and-File-System/
├── file_system/              # Lab hệ thống tệp tin
│   ├── 24127018_24127252_24127337.patch
│   └── xv6-labs-2024/       # Source code
│       ├── kernel/          # OS kernel (C)
│       ├── user/            # User programs
│       ├── mkfs/            # File system maker
│       ├── Makefile
│       └── conf/            # Configuration
│
├── page_table/               # Lab bảng trang
│   ├── 24127018_24127252_24127337.patch
│   ├── 24127018_24127252_24127337_Report.pdf
│   └── xv6-labs-2024/       # Source code
│       ├── kernel/
│       ├── user/
│       ├── mkfs/
│       └── Makefile
│
└── README.md                 # File này
```

## 🔧 Cách Chạy

### Điều kiện tiên quyết

**Cài đặt RISC-V toolchain và QEMU:**

```bash
# Cài RISC-V cross-compiler
git clone https://github.com/riscv/riscv-gnu-toolchain
cd riscv-gnu-toolchain
./configure --prefix=/opt/riscv
make

# Cài QEMU cho RISC-V
sudo apt-get install qemu-system-riscv64
# hoặc từ source: https://www.qemu.org/download/

# Cập nhật PATH
export PATH="/opt/riscv/bin:$PATH"
```

**Kiểm tra cài đặt:**
```bash
riscv64-unknown-elf-gcc --version
qemu-system-riscv64 --version
```

### Chạy File System Lab

```bash
cd file_system/xv6-labs-2024

# Biên dịch kernel
make clean
make

# Chạy trong QEMU
make qemu

# Chạy tests
make grade

# Chạy test bigfile cụ thể
make qemu
# Trong QEMU shell:
$ bigfile
```

### Chạy Page Table Lab

```bash
cd page_table/xv6-labs-2024

# Biên dịch kernel
make clean
make

# Chạy trong QEMU
make qemu

# Chạy tests
make grade

# Chạy test page table cụ thể
make qemu
# Trong QEMU shell:
$ pgtbltest
```

## 📝 Các Lệnh Chính

| Lệnh | Mô tả |
|------|-------|
| `make` | Biên dịch kernel và user programs |
| `make clean` | Xóa các file biên dịch |
| `make qemu` | Chạy xv6 trong QEMU emulator |
| `make grade` | Chạy test suite |
| `make fs.img` | Tạo file system image |
| `make kernel` | Biên dịch chỉ kernel |
| `make TOOLPREFIX=riscv64-linux-gnu-` | Chỉ định toolchain |

**Trong QEMU shell:**
```bash
$ ls              # List files
$ cat <file>      # Read file
$ mkdir <dir>     # Create directory
$ rm <file>       # Remove file
$ bigfile         # Test large file (File System Lab)
$ pgtbltest       # Test page table (Page Table Lab)
$ usertests       # Run comprehensive tests
$ exit            # Quit QEMU
```

## 📊 Các Test Chính

### File System Tests
- **copyin/copyout**: Kiểm tra copy dữ liệu từ/đến user space
- **bigfile**: Kiểm tra tệp tin lớn với doubly indirect blocks
- **inode operations**: Tạo, xóa, cập nhật tệp tin
- **directory operations**: Thao tác thư mục

### Page Table Tests
- **pgtbltest**: Kiểm tra virtual memory mapping
- **sbrk**: Test tăng heap
- **stack**: Kiểm tra stack alignment
- **address translation**: Validation địa chỉ ảo → vật lý

## 🔍 Chi tiết Kỹ thuật

### File System - Doubly Indirect Blocks

**Cấu trúc lưu trữ:**
```
Direct (11 blocks)        → 11 blocks
Indirect (1 block)        → NINDIRECT (256) blocks = 256KB
Doubly Indirect (1 block) → NINDIRECT² blocks = 64MB (NEW)
Total max file size: ~64MB
```

**Hàm `bmap()` trong `kernel/fs.c`:**
- Ánh xạ block number lên disk block
- Xử lý direct, indirect, và doubly indirect addressing
- Allocate blocks nếu cần thiết

### Page Table - RISC-V Sv39

**Cấu trúc địa chỉ ảo (64-bit):**
```
[Unused(25)] [L2(9)] [L1(9)] [L0(9)] [Offset(12)]
```

**Hàm chính:**
- `kvmmake()`: Tạo kernel page table
- `kvmmap()`: Map virtual → physical address
- `walk()`: Traverse page table để tìm PTE
- `mappages()`: Map range of pages

## 🐛 Debug

### Sử dụng GDB

```bash
# Terminal 1: Chạy QEMU với GDB stub
make qemu-gdb

# Terminal 2: Attach GDB
riscv64-unknown-elf-gdb kernel/kernel
(gdb) target remote localhost:1234
(gdb) break main
(gdb) continue
```

### Logging Kernel

Trong code, sử dụng:
```c
printf("Debug message: %d\n", value);
```

## 📖 Tài liệu Tham khảo

- **xv6 Book**: https://pdos.csail.mit.edu/6.1810/2024/xv6/
- **RISC-V Spec**: https://riscv.org/technical/specifications/
- **MIT 6.1810 Course**: https://pdos.csail.mit.edu/6.1810/

## 👥 Nhóm Thực hiện

- Student IDs: 24127018, 24127252, 24127337

## 📄 Patches

- File System: `file_system/24127018_24127252_24127337.patch`
- Page Table: `page_table/24127018_24127252_24127337.patch`
- Report: `page_table/24127018_24127252_24127337_Report.pdf`

---

**Last Updated**: 2024  
**Base Project**: xv6-labs-2024 (MIT)  
**Platform**: RISC-V 64-bit