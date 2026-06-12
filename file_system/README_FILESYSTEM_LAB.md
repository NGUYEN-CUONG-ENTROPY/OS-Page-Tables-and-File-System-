# File System Lab - xv6 Doubly Indirect Blocks

## 📌 Mục tiêu

Lab này triển khai hỗ trợ **doubly indirect blocks** trong hệ thống tệp xv6, cho phép lưu trữ các tệp tin **lớn hơn 64KB**.

## 🎯 Vấn đề được giải quyết

Hệ thống tệp xv6 ban đầu chỉ hỗ trợ:
- 11 direct blocks
- 1 indirect block

Điều này giới hạn kích thước tệp tin tối đa ở **~268KB**.

Với **doubly indirect block**, chúng ta có thể lưu trữ tệp tin lên đến **~64MB**.

## 🔧 Các Thay đổi Chính

### 1. File `kernel/file.h`

**Thay đổi cấu trúc Inode:**

```c
// Trước:
uint addrs[NDIRECT+1];      // 11 direct + 1 indirect

// Sau:
uint addrs[NDIRECT+2];      // 11 direct + 1 indirect + 1 doubly indirect
```

### 2. File `kernel/fs.h`

**Định nghĩa hằng số mới:**

```c
#define NDIRECT 11              // Direct blocks
#define NINDIRECT (BSIZE / sizeof(uint))  // 256 indirect blocks per block
#define NDOUBLYINDIRECT (NINDIRECT * NINDIRECT)  // ~64MB capacity
#define MAXFILE (NDIRECT + NINDIRECT + NDOUBLYINDIRECT)
```

### 3. File `kernel/fs.c` - Hàm `bmap()`

**Thêm xử lý doubly indirect blocks:**

```c
// Doubly indirect blocks
bn -= NINDIRECT;

if(bn < NDOUBLYINDIRECT){
    // Level 1: Allocate/load first level indirect block
    if((addr = ip->addrs[NDIRECT + 1]) == 0){
      addr = balloc(ip->dev);
      if(addr == 0) return 0;
      ip->addrs[NDIRECT + 1] = addr;
    }

    // Read level 1 block
    struct buf *bp_l1 = bread(ip->dev, addr);
    uint *a_l1 = (uint*)bp_l1->data;
    uint idx_l1 = bn / NINDIRECT;    // Which block in level 1
    uint idx_l2 = bn % NINDIRECT;    // Which entry in level 2

    // Level 2: Allocate/load second level indirect block
    if((addr = a_l1[idx_l1]) == 0){
      addr = balloc(ip->dev);
      if(addr == 0){
        brelse(bp_l1);
        return 0;
      }
      a_l1[idx_l1] = addr;
      log_write(bp_l1);
    }
    brelse(bp_l1);

    // Read level 2 block and get data block
    struct buf *bp_l2 = bread(ip->dev, addr);
    uint *a_l2 = (uint*)bp_l2->data;
    if((addr = a_l2[idx_l2]) == 0){
      addr = balloc(ip->dev);
      if(addr){
        a_l2[idx_l2] = addr;
        log_write(bp_l2);
      }
    }
    brelse(bp_l2);
    return addr;
  }
```

### 4. File `kernel/fs.c` - Hàm `itrunc()`

**Thêm cleanup cho doubly indirect blocks:**

```c
// Doubly indirect
if(ip->addrs[NDIRECT+1]){
  bp = bread(ip->dev, ip->addrs[NDIRECT+1]);
  a = (uint*)bp->data;
  for(j = 0; j < NINDIRECT; j++){
    if(a[j]){
      // Read level 2 block and free all data blocks
      bp_inner = bread(ip->dev, a[j]);
      a_inner = (uint*)bp_inner->data;
      for(k = 0; k < NINDIRECT; k++){
        if(a_inner[k])
          bfree(ip->dev, a_inner[k]);
      }
      brelse(bp_inner);
      bfree(ip->dev, a[j]);
    }
  }
  brelse(bp);
  bfree(ip->dev, ip->addrs[NDIRECT+1]);
  ip->addrs[NDIRECT+1] = 0;
}
```

### 5. File `kernel/fs.c` - Hàm `bzero()`

**Sửa lỗi:**

```c
// Trước:
log_write(bp);

// Sau:
bwrite(bp);     // Ghi trực tiếp, không log
```

### 6. Thêm Test `user/bigfile.c`

Test case để kiểm tra tệp tin lớn:

```c
// Tạo và ghi vào tệp tin lớn
// Kiểm tra độ dài tệp tin
// Xác minh dữ liệu được ghi đúng
```

### 7. Cập nhật `Makefile`

```makefile
# Thêm bigfile vào danh sách user programs
$U/_bigfile \
```

## 📊 So sánh Capacity

| Loại Block | Số lượng | Dung lượng |
|-----------|---------|-----------|
| Direct | 11 | 11 × 1KB = 11 KB |
| Indirect | 1 × 256 | 256 × 1KB = 256 KB |
| Doubly Indirect | 256 × 256 | 256 × 256 × 1KB = 64 MB |
| **Tổng** | - | **~64 MB** |

## 🧪 Cách Kiểm Tra

### Biên dịch

```bash
cd file_system/xv6-labs-2024
make clean
make
```

### Chạy test

```bash
# Cách 1: Sử dụng make grade
make grade

# Cách 2: Chạy QEMU interactively
make qemu
# Trong shell:
$ bigfile
# Đợi test hoàn thành (~2-3 phút)
```

### Kết quả mong đợi

```
$ bigfile
...
wrote 65803 sectors
bigfile done; ok
```

Điều này chứng minh:
- ✅ Hệ thống có thể lưu trữ tệp tin > 256 KB
- ✅ Doubly indirect blocks hoạt động đúng
- ✅ Dữ liệu được ghi và đọc chính xác

## 🔍 Cấu trúc Block Layout

```
Inode Structure (với doubly indirect):
┌─────────────────────────────────────────────┐
│ Direct Blocks (11)                          │
│ addrs[0-10] → Dữ liệu trực tiếp (~11 KB)   │
├─────────────────────────────────────────────┤
│ Indirect Block (1)                          │
│ addrs[11] → Chứa 256 pointers               │
│           → Dữ liệu (~256 KB)               │
├─────────────────────────────────────────────┤
│ Doubly Indirect Block (1) - NEW             │
│ addrs[12] → Level 1: 256 pointers           │
│   ├─ L1[0] → Level 2: 256 pointers → 256KB │
│   ├─ L1[1] → Level 2: 256 pointers → 256KB │
│   └─ ...                                    │
│           → Dữ liệu (~64 MB) - NEW          │
└─────────────────────────────────────────────┘
```

## 🐛 Debugging Tips

### Sử dụng printf() để trace

```c
printf("bmap: block %d, doubly indirect\n", bn);
printf("Level 1 index: %d, Level 2 index: %d\n", idx_l1, idx_l2);
```

### Kiểm tra inode

```c
// Xem thông tin inode của bigfile
for(i = 0; i < NDIRECT+2; i++){
    if(ip->addrs[i])
        printf("addrs[%d] = %d\n", i, ip->addrs[i]);
}
```

### Test nhỏ hơn

```bash
# Tạo tệp 300KB thay vì 65MB để test nhanh hơn
# Sửa user/bigfile.c:
// #define N (65803*512/1024)  // ~65MB
#define N (300)  // 300KB cho testing nhanh
```

## 📚 File Quan Trọng

| File | Mô tả |
|------|-------|
| `kernel/fs.h` | Định nghĩa cấu trúc FS |
| `kernel/fs.c` | Triển khai FS (bmap, itrunc) |
| `kernel/file.h` | Cấu trúc inode on-disk |
| `user/bigfile.c` | Test case |
| `Makefile` | Build configuration |

## ✅ Checklist Triển khai

- [ ] Update `kernel/file.h` - thêm địa chỉ doubly indirect
- [ ] Update `kernel/fs.h` - định nghĩa NDOUBLYINDIRECT
- [ ] Update `bmap()` - xử lý doubly indirect mapping
- [ ] Update `itrunc()` - cleanup doubly indirect blocks
- [ ] Fix `bzero()` - sử dụng bwrite thay log_write
- [ ] Thêm `user/bigfile.c` test
- [ ] Update Makefile
- [ ] Biên dịch thành công (không lỗi/cảnh báo)
- [ ] `make grade` pass

## 🎓 Kiến thức Chính

### Indirect Block Addressing
- **Direct**: Block được lưu trực tiếp trong inode
- **Indirect**: Inode trỏ tới block chứa các pointers
- **Doubly Indirect**: Inode trỏ tới block chứa pointers tới các blocks chứa pointers

### Memory Allocation
- Sử dụng `balloc()` để cấp phát block
- Sử dụng `bfree()` để giải phóng block
- `log_write()` để ghi log transaction

### Disk I/O
- `bread()` - đọc block vào buffer cache
- `bwrite()` - ghi block ra disk
- `brelse()` - giải phóng buffer

---

**Trạng thái**: ✅ Hoàn thành  
**Ngày**: 2024  
**Author**: Group 24127018, 24127252, 24127337
