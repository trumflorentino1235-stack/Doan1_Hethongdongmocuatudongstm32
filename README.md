# Doan1_Hethongdongmocuatudongstm32
Sử dụng RFID,keypad,vân tay AS608 để mở cửa tự động


## Cấu trúc thư mục và chức năng từng file

| Tên file | Nhiệm vụ |
|----------|----------|
| `main.c` | Chương trình chính: khởi tạo hệ thống, vòng lặp chính điều khiển luồng xác thực, xử lý sự kiện từ các thiết bị ngoại vi, điều khiển trạng thái cửa. |
| `stm32f1_rc522.c` / `.h` | Thư viện driver giao tiếp với module RFID RC522 qua SPI. Cung cấp các hàm đọc/ghi thẻ, xác thực khóa, đọc UID, truy xuất dữ liệu khối (block) trên thẻ MIFARE. |
| `as608.c` / `.h` | Thư viện driver cho cảm biến vân tay AS608 giao tiếp qua UART. Xử lý các lệnh: đăng ký vân tay, tìm kiếm vân tay, xóa vân tay, nhận dạng người dùng. |
| `liquidcrystal_i2c.c` / `.h` | Driver điều khiển màn hình LCD 16x2 (hoặc 20x4) qua giao tiếp I2C. Hiển thị trạng thái hệ thống, hướng dẫn thao tác, kết quả xác thực (thành công/thất bại). |
| `EventRecorderStub.scvd` | File cấu hình cho công cụ debug Event Recorder của Keil (dùng để ghi log sự kiện theo thời gian thực). |
| `RFID.uvprojx` | Project file của Keil µVision – chứa tất cả cài đặt dự án, danh sách file, đường dẫn thư viện, tùy chọn biên dịch. |
| `RFID.uvoptx` | File tùy chọn debug và cấu hình môi trường Keil (ví dụ: cài đặt debugger, breakpoint, cổng COM). |


---

## Yêu cầu phần cứng
- Vi điều khiển: STM32F103C8T6 (BluePill).
- Module RFID: RC522 (giao tiếp SPI).
- Cảm biến vân tay: AS608 (giao tiếp UART).
- Bàn phím ma trận 3x4 để nhập mật khẩu.
- Màn hình LCD I2C 16x2.
- Servo/Relay điều khiển chốt cửa.
- Nguồn 5V cho các module.

---

## Hướng dẫn sử dụng (cơ bản)

### 1. Mở cửa bằng RFID
- Quét thẻ RFID lên đầu đọc.
- Hệ thống so sánh UID với danh sách thẻ đã đăng ký.
- Nếu hợp lệ → hiển thị “Door unlocked” và mở cửa.

### 2. Mở cửa bằng mật khẩu
- Nhập mật khẩu trên bàn phím ma trận (mặc định: `1234`).
- Nếu đúng → mở cửa; sai → hiển thị lỗi.

### 3. Mở cửa bằng vân tay
- Đặt ngón tay lên cảm biến AS608.
- Hệ thống tìm kiếm trong cơ sở dữ liệu vân tay.
- Nếu khớp → mở cửa.

### 4. Chế độ quản trị
- Dùng thẻ mastercard để vào menu quản trị.
- Có thể: thêm/xóa thẻ RFID, đăng ký/xóa vân tay, đổi mật khẩu, xem log.

---

## Ghi chú phát triển
- Mã nguồn được viết trên Keil µVision 5 với compiler ARMCC.
- Sử dụng thư viện STM32F1 Standard Peripheral Library.
- Các driver đều được viết ở mức thấp (HAL hoặc register) để tiết kiệm tài nguyên.

---

## Tác giả
[Tạ Duy Tân - Thái Thế Nhân].

---
