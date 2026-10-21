# 🛡️ CYBERBOX AI - CHATBOT SANDBOX C++ SIÊU TỐC & AN TOÀN TUYỆT ĐỐI

> **100% Thuần C++ (C++17)** | **Tốc độ cực hạn** | **Cô lập Sandbox 2 tầng** | **Internet Egress Gateway (Air-Gap / Online)** | **Hỗ trợ chuẩn OpenAI & Mọi Local LLM (Ollama, LM Studio, DeepSeek)**

---

## 🌟 Điểm Nổi Bật

1. **Hiệu năng cực đại (Pure C++17)**:
   - Không phụ thuộc vào Python, Node.js hay bất kỳ runtime cồng kềnh nào.
   - Khởi động trong **< 50ms**, chiếm dụng RAM **< 15MB**.
   - Tự động tích hợp Native WinSock2 HTTP Server, WinHTTP Client và embedded Modern Web UI.

2. **Cơ chế Sandbox Cô Lập Tuyệt Đối (Toàn quyền bên trong - Không thoát ra ngoài)**:
   - **Chế độ 1: Docker Engine Container (Linux OS)**:
     - Giao tiếp trực tiếp với Docker Daemon qua **Windows Named Pipe** (`\\.\pipe\docker_engine`).
     - AI có **toàn quyền Root** trong container: cài package (`apt-get`), biên dịch mã nguồn, chạy background daemon.
     - **Không mount bất kỳ thư mục nào của máy thật**: Ngăn chặn 100% nguy cơ leak file hoặc phá hỏng hệ điều hành host.
     - Giới hạn cứng: RAM 2GB, CPU 2 Cores, tối đa 128 PIDs (chống fork-bomb).
   - **Chế độ 2: Win32 Job Object Jail (Native Windows Sandbox)**:
     - Dành cho máy không cài Docker hoặc muốn tốc độ tức thì.
     - Sử dụng API Windows Kernel: `CreateJobObjectW`, `SetInformationJobObject`, `AssignProcessToJobObject`.
     - Chặn tuyệt đối Path Traversal (`..`), giam lỏng môi trường làm việc trong thư mục `sandbox_workspace/`.

3. **Cổng Kiểm Soát Internet Egress (Air-Gap Switch)**:
   - Một công tắc vật lý trực quan ngay trên giao diện:
     - 🔴 **AIR-GAP (OFFLINE)**: Ngắt hoàn toàn mọi kết nối mạng ra ngoài Internet. AI chỉ làm việc offline bên trong sandbox, không thể gửi dữ liệu ra máy chủ lạ.
     - 🟢 **INTERNET (ONLINE)**: Mở cổng Internet khi người dùng cho phép để AI có thể tải package (pip, npm, apt, git clone), fetch API,...

4. **Port Gateway (Mở cổng ra ngoài cho người dùng)**:
   - Quản lý và forward các cổng tùy chọn (vd: Cổng `8080`, `3000`, `5000` của web app do AI tạo ra) ra ngoài máy thật.
   - Người dùng có thể click "Mở Web" để test ngay ứng dụng AI vừa code và chạy trong sandbox.

5. **Hỗ trợ Tất Cả Chuẩn OpenAI Compatible**:
   - Tương thích hoàn toàn với:
     - **OpenAI**: `gpt-4o`, `gpt-4o-mini`, `o1`, `o3`
     - **Ollama (Local Offline)**: `http://localhost:11434/v1` (Llama 3, Qwen 2.5 Coder, Mistral, DeepSeek-R1, Phi)
     - **DeepSeek API**: `https://api.deepseek.com/v1`
     - **LM Studio / LocalAI / vLLM / OpenRouter / Groq**
   - Hỗ trợ Autonomous ReAct Agent Loop: AI tự sinh lệnh shell/bash, nhận stdout/stderr trực tiếp từ sandbox và tự sửa lỗi đến khi hoàn thành.

6. **Giao diện Web UI Chuẩn Meta Muse Edition**:
   - **Side-Chat Switcher**: Chia nhánh nhiều luồng trò chuyện cho từng project độc lập.
   - **Tab Library (Tạo tác / Artifacts)**: Tự động gom nhóm và cho phép xem trước trực tiếp các file Web HTML, C++, Python, Video MP4, Audio, Phụ đề SRT do Agent tạo ra.
   - **Tab Ideas (Kịch bản mẫu)**: Bộ công thức dựng sẵn gồm Video AI Intelligence (bóc băng âm thanh, trích xuất slide), Web dev và Benchmark C++.
   - **Tab Activity Log (Vết kiểm toán Sentinel)**: Bảng theo dõi minh bạch mọi URL, công cụ, lệnh thực thi và mã trạng thái.
   - **Nút Stop Task & Take Control**: Ngắt tác vụ ngay lập tức hoặc chiếm quyền gõ lệnh tay trong sandbox.
   - **Live Terminal & Port Gateway**: Bảng điều khiển song song thời gian thực.

---

## 📂 Cấu Trúc Dự Án

```
joyful-faraday/
├── include/
│   └── simple_json.hpp         # Bộ giải mã & sinh JSON C++17 siêu tốc (zero dependency)
├── src/
│   ├── main.cpp                # Điểm khởi chạy, cấu hình UTF-8, tự động mở Browser
│   ├── app_server.hpp/.cpp     # HTTP Server WinSock2 đa luồng & Agent ReAct Loop
│   ├── ai_client.hpp/.cpp      # Client WinHTTP chuẩn OpenAI Compatible & trích xuất lệnh
│   ├── sandbox_engine.hpp      # Giao diện chuẩn cho các loại Sandbox
│   ├── sandbox_manager.hpp/.cpp# Điều phối & chuyển đổi linh hoạt giữa Docker và Win32
│   ├── docker_sandbox.hpp/.cpp # Engine Docker qua Named Pipe (Root, Egress switch, Port map)
│   ├── win32_jail_sandbox.hpp/.cpp # Engine Win32 Job Object Kernel Jail
│   └── web_ui.hpp              # Giao diện Web Dark-mode nhúng thẳng vào nhị phân
├── build.bat                   # Script biên dịch tự động 1-click (MSVC / MinGW / Clang / CMake)
├── CMakeLists.txt              # Cấu hình CMake chuẩn
└── README.md
```

---

## 🚀 Hướng Dẫn Biên Dịch & Chạy

### Cách 1: Sử dụng `build.bat` (Khuyến nghị - 1 Click)

Mở terminal (CMD hoặc PowerShell) trong thư mục dự án và chạy:

```cmd
build.bat
```

Script sẽ tự động nhận diện trình biên dịch hiện có trên máy của bạn (Visual Studio `cl.exe`, MinGW `g++`, Clang `clang++`, hoặc `cmake`) và tạo ra file thực thi:
👉 **`cyberbox_ai.exe`**

### Cách 2: Biên dịch thủ công bằng MSVC (Visual Studio Developer Command Prompt)

```cmd
cl /EHsc /std:c++17 /O2 /I include /I src src\main.cpp src\app_server.cpp src\ai_client.cpp src\sandbox_manager.cpp src\docker_sandbox.cpp src\win32_jail_sandbox.cpp /Fe:cyberbox_ai.exe ws2_32.lib winhttp.lib shell32.lib shlwapi.lib
```

### Cách 3: Biên dịch bằng MinGW-w64 (GCC)

```cmd
g++ -std=c++17 -O3 -Iinclude -Isrc src/main.cpp src/app_server.cpp src/ai_client.cpp src/sandbox_manager.cpp src/docker_sandbox.cpp src/win32_jail_sandbox.cpp -o cyberbox_ai.exe -lws2_32 -lwinhttp -lshell32 -lshlwapi
```

### Cách 4: Biên dịch bằng CMake

```cmd
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

---

## 🎮 Cách Sử Dụng Ứng Dụng

1. **Khởi chạy ứng dụng**:
   ```cmd
   cyberbox_ai.exe
   ```
   Ứng dụng sẽ tự động khởi động server trên cổng `8080` và **tự động mở trình duyệt web** tại `http://127.0.0.1:8080`.

2. **Cài đặt Model AI**:
   - Bấm vào nút **⚙️ Settings** ở góc trên bên phải.
   - **Dùng Ollama (Miễn phí, 100% offline trên máy)**:
     - Base URL: `http://localhost:11434/v1`
     - Model: `llama3.2`, `qwen2.5-coder`, hoặc `deepseek-r1:latest`
     - API Key: *(Để trống)*
   - **Dùng OpenAI**:
     - Base URL: `https://api.openai.com/v1`
     - Model: `gpt-4o-mini` hoặc `gpt-4o`
     - API Key: `sk-...`
   - **Dùng DeepSeek API**:
     - Base URL: `https://api.deepseek.com/v1`
     - Model: `deepseek-chat`
     - API Key: `sk-...`

3. **Điều khiển Internet Egress (Nút Switch trên đầu)**:
   - Chế độ mặc định: **🔴 AIR-GAP (OFFLINE)**: Tuyệt đối không gửi gói tin nào ra ngoài.
   - Khi cần AI tải thư viện hoặc cào dữ liệu: Nhấp chuột vào nút chuyển thành **🟢 INTERNET (ONLINE)**.

4. **Mở Cổng Truy Cập (Port Gateway)**:
   - Vào tab **Port Gateway** ở khung bên phải.
   - Nhập Container Port và Host Port (ví dụ: `8080` -> `8080`).
   - Nhờ AI: *"Tạo một web server hiển thị dashboard hệ thống trên cổng 8080"*.
   - Sau khi AI chạy xong, nhấn nút **"Mở Web"** để tương tác với web app ngay lập tức!
