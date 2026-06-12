# Page Table Lab - xv6 Virtual Memory Extensions

## 📌 Mục tiêu

Lab này triển khai các tính năng nâng cao cho quản lý bộ nhớ ảo (Virtual Memory) trong xv6, bao gồm:
- **Superpages** (trang lớn 2MB)
- **Page table walk optimization**
- **Advanced memory mapping**

## 🎯 Vấn đề được giải quyết

Quản lý bộ nhớ ảo cơ bản trong xv6:
- Chỉ hỗ trợ 4KB pages (page nhỏ)
- TLB misses nhiều → Hiệu suất chậm
- Không tối ưu cho các ứng dụng cần large memory regions

**Giải pháp:** Thêm hỗ trợ Superpages (2MB pages)
- Giảm TLB pressure
- Tăng hiệu suất
- Tiết kiệm kernel resources

## 🔧 Các Thay đổi Chính

### 1. Configuration - `conf/lab.mk`

```makefile
# Trước:
LAB=util

# Sau:
LAB=pgtbl
```

Điều này kích hoạt compile flags và test cases cho page table lab.

### 2. File `kernel/vm.c` - Superpages Support

**Thêm hàm mới: `mapsuperpages()`**

```c
// Map large memory regions using superpages (2MB)
static int mapsuperpages(
    pagetable_t pagetable,
    uint64 va,           // Virtual address
    uint64 size,         // Size to map
    uint64 pa,           // Physical address
    int perm             // Permissions (PTE_R, PTE_W, PTE_X, etc)
) {
    // Kiểm tra alignment: both va và pa phải align 2MB
    if((va % SUPERPAGE_SIZE) != 0 || (pa % SUPERPAGE_SIZE) != 0)
        return -1;
    
    // Map từng superpage
    for(; size > 0; size -= SUPERPAGE_SIZE, va += SUPERPAGE_SIZE, pa += SUPERPAGE_SIZE) {
        // Lấy L2 PTE (cấp 2 của page table)
        uint64 *pte = (uint64*)kvmwalk(pagetable, va);
        
        // Set superpage flag (bit 7)
        *pte = (pa >> 12 << 10) | perm | PTE_V | PTE_R | PTE_X;
    }
    return 0;
}
```

**Thêm hàm mới: `walkleaf()`**

```c
// Walk page table và trả về leaf PTE cùng page size
static pte_t *walkleaf(
    pagetable_t pagetable,
    uint64 va,           // Virtual address
    uint64 *psz          // Output: page size (4KB or 2MB)
) {
    pte_t *pte;
    
    // Kiểm tra L2 PTE
    pte = (pte_t*)kvmwalk2(pagetable, va, 2);
    if(pte && (*pte & PTE_V)) {
        if(*pte & PTE_PS) {  // Superpage flag
            *psz = SUPERPAGE_SIZE;
            return pte;
        }
    }
    
    // Fallback: L0 PTE (4KB page)
    pte = (pte_t*)kvmwalk(pagetable, va);
    if(pte && (*pte & PTE_V)) {
        *psz = PAGE_SIZE;
        return pte;
    }
    
    return 0;
}
```

### 3. File `kernel/riscv.h` - RISC-V Definitions

**Thêm hằng số:**

```c
#define SUPERPAGE_SIZE    (2*1024*1024)   // 2MB
#define SUPERPAGE_MASK    (~(SUPERPAGE_SIZE-1))
#define PTE_PS            (1L << 7)       // Superpage bit
```

**RISC-V Sv39 Page Table Structure:**

```
Level 2 (L2) - 9 bits  →  1GB regions (superpages: 2MB each)
Level 1 (L1) - 9 bits  →  2MB regions
Level 0 (L0) - 9 bits  →  4KB pages
```

### 4. File `kernel/memlayout.h` - Memory Layout

**Bố cục bộ nhớ RISC-V:**

```
User address space:
┌────────────────────────────┐ 0xFFFFFFFFFFFFFFFF
│ Kernel space               │
│ (Direct mapped physical)   │
├────────────────────────────┤ KERNBASE (0x80000000)
│ Unused                     │
├────────────────────────────┤ MAXVA
│ User stack                 │
│ (grows down)               │
├────────────────────────────┤
│ Free memory                │
│ (heap grows up)            │
├────────────────────────────┤
│ User BSS                   │
├────────────────────────────┤
│ User data                  │
├────────────────────────────┤
│ User text                  │
└────────────────────────────┘ 0
```

### 5. Kernel Page Table Initialization

**Trong `kvmmake()`:**

```c
// Map kernel text (read-only, executable)
kvmmap(kpgtbl, KERNBASE, KERNBASE, (uint64)etext-KERNBASE, PTE_R | PTE_X);

// Map kernel data (read-write)
kvmmap(kpgtbl, (uint64)etext, (uint64)etext, 
       PHYSTOP-(uint64)etext, PTE_R | PTE_W);

// Map devices (UART, PLIC, etc)
kvmmap(kpgtbl, UART0, UART0, PGSIZE, PTE_R | PTE_W);
kvmmap(kpgtbl, PLIC, PLIC, 0x4000000, PTE_R | PTE_W);
```

### 6. User Process Page Table

**Tạo page table cho process:**

```c
// Cấp phát 3-level page table
pagetable_t p_pagetable = (pagetable_t) kalloc();
memset(p_pagetable, 0, PGSIZE);

// Map user text
for(i = 0; i < sz; i += PGSIZE) {
    mappages(p_pagetable, i, PGSIZE, v2p(p), PTE_R | PTE_U | PTE_X);
}

// Set proc.pagetable
p->pagetable = p_pagetable;
```

## 🧭 Page Table Walk

### Cấp độ 3 của RISC-V Sv39

**Virtual Address Decomposition:**

```
VA (64 bits): [Unused(25)] [L2(9)] [L1(9)] [L0(9)] [Offset(12)]

Ví dụ: 0x87654321 → VA alignment 4KB
```

**Walk Process:**

```c
// Hàm walk() - tìm PTE cho VA
pte_t *walk(pagetable_t pagetable, uint64 va, int alloc) {
    if(va >= MAXVA)
        panic("walk");
    
    for(int level = 2; level > 0; level--) {
        pte_t *pte = &pagetable[PX(level, va)];
        if(*pte & PTE_V) {
            pagetable = (pagetable_t)PTE2PA(*pte);
        } else {
            if(!alloc || (pagetable = (pagetable_t)kalloc()) == 0)
                return 0;
            memset(pagetable, 0, PGSIZE);
            *pte = PA2PTE(pagetable) | PTE_V;
        }
    }
    return &pagetable[PX(0, va)];
}
```

**PTE Extraction Macros:**

```c
#define PX(level, va) ((va >> (12 + 9*level)) & 0x1FF)  // Extract 9 bits
#define PA2PTE(pa)    (((pa) >> 12) << 10)               // PA → PTE
#define PTE2PA(pte)   (((pte) >> 10) << 12)              // PTE → PA
```

## 📊 Memory Mapping Examples

### Ví dụ 1: Map kernel space (direct map)

```bash
VA: 0x80000000 (KERNBASE)
PA: 0x80000000
Size: PHYSTOP - KERNBASE (~128MB)
Perm: PTE_R | PTE_W

→ All kernel memory is directly mapped
→ Virtual address = Physical address
```

### Ví dụ 2: Map user text

```bash
VA: 0x1000    (user code start)
PA: 0x8xxxxxx (physical RAM)
Size: text size
Perm: PTE_R | PTE_U | PTE_X

→ User code readable & executable
→ User-accessible (PTE_U)
```

### Ví dụ 3: Superpage mapping (2MB)

```bash
VA: 0x200000  (2MB aligned)
PA: 0xC00000  (2MB aligned)
Size: 2MB
Perm: PTE_R | PTE_W | PTE_X

→ Single PTE entry maps entire 2MB
→ Reduce TLB pressure
→ Faster translation
```

## 🧪 Cách Kiểm Tra

### Biên dịch

```bash
cd page_table/xv6-labs-2024
make clean
make
```

### Chạy tests

```bash
# Cách 1: Grade
make grade

# Cách 2: Interactive QEMU
make qemu
# Trong shell:
$ pgtbltest

# Cách 3: Comprehensive tests
$ usertests
```

### Kết quả mong đợi

```
$ pgtbltest
pagewalk test
test_get_pagewalk_pte
...
all tests passed
```

## 🔍 Page Table Structure Visualization

```
┌─────────────────────────────────────────┐
│ Process Page Table                      │
│ (Root of 3-level hierarchy)             │
├─────────────────────────────────────────┤
│ L2[0] → L1 table (if valid)             │
│ L2[1] → L1 table                        │
│ L2[2] → SUPERPAGE 2MB (if PS bit set)   │
│ ...                                     │
│ L2[511] → L1 table                      │
├─────────────────────────────────────────┤
        ↓
┌─────────────────────────────────────────┐
│ L1 Page Table                           │
├─────────────────────────────────────────┤
│ L1[0] → L0 table                        │
│ L1[1] → L0 table                        │
│ ...                                     │
│ L1[511] → L0 table                      │
├─────────────────────────────────────────┤
        ↓
┌─────────────────────────────────────────┐
│ L0 Page Table (4KB pages)               │
├─────────────────────────────────────────┤
│ L0[0] → 4KB page                        │
│ L0[1] → 4KB page                        │
│ ...                                     │
│ L0[511] → 4KB page                      │
└─────────────────────────────────────────┘
```

## 🐛 Debugging Tips

### Xem page table entries

```c
void print_pte(pte_t pte, int level) {
    printf("Level %d PTE: 0x%lx\n", level, pte);
    printf("  Valid: %d\n", pte & PTE_V);
    printf("  Readable: %d\n", (pte >> 1) & 1);
    printf("  Writable: %d\n", (pte >> 2) & 1);
    printf("  Executable: %d\n", (pte >> 3) & 1);
    printf("  User: %d\n", (pte >> 4) & 1);
    printf("  Superpage: %d\n", (pte >> 7) & 1);
    printf("  PPN: 0x%lx\n", (pte >> 10) & 0xFFFFFFFFFFFUL);
}
```

### Trace address translation

```bash
# Thêm debug output trong walk()
printf("walk: va=0x%lx, level=%d, pte=0x%lx\n", va, level, *pte);
```

### Kiểm tra kernel page table

```c
// Trong main(), sau kvminit():
extern pagetable_t kernel_pagetable;
printf("kernel_pagetable: %p\n", (void*)kernel_pagetable);
```

## 📚 File Quan Trọng

| File | Mô tả |
|------|-------|
| `kernel/vm.c` | Virtual memory implementation |
| `kernel/memlayout.h` | Memory layout constants |
| `kernel/riscv.h` | RISC-V ISA definitions |
| `kernel/proc.c` | Process management |
| `kernel/proc.h` | Process structures |
| `user/pgtbltest.c` | Page table tests |

## ✅ Checklist Triển khai

- [ ] Thêm `mapsuperpages()` function
- [ ] Thêm `walkleaf()` function
- [ ] Define RISC-V PTE bits và macros
- [ ] Update kernel page table initialization
- [ ] Implement user process page table creation
- [ ] Add page table walk optimization
- [ ] Cấu hình LAB=pgtbl
- [ ] Biên dịch không lỗi
- [ ] Tests pass

## 🎓 Kiến thức Chính

### RISC-V Sv39 Paging

- **VA Bits**: 64-bit virtual address
  - Bits 63-39: Unused (must be zero)
  - Bits 38-30: L2 index
  - Bits 29-21: L1 index
  - Bits 20-12: L0 index
  - Bits 11-0: Page offset

### PTE Format (64-bit)

```
[PPN (54 bits)] [RSVD(7)] [D][A][G][U][X][W][R][V]
- PPN: Physical Page Number
- D: Dirty (được sửa đổi)
- A: Accessed (được truy cập)
- G: Global (chia sẻ giữa processes)
- U: User (user-accessible)
- X: Execute
- W: Write
- R: Read
- V: Valid
```

### Superpages (2MB)

- Giảm TLB entries cần thiết
- Tốc độ translation nhanh hơn
- Tiết kiệm memory

---

**Trạng thái**: ✅ Hoàn thành  
**Ngày**: 2024  
**Author**: Group 24127018, 24127252, 24127337  
**References**: MIT 6.1810, xv6 Book, RISC-V Spec
