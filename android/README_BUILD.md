<project xmlns="http://maven.apache.org/POM/4.0.0">
# Hướng dẫn Build APK Android Native C++ (Phương án B)

Cấu trúc dự án Android NativeActivity thuần C++ (100% Native, không Java/Kotlin) đã được tạo sẵn tại thư mục:
`android/`

### 1. Cấu trúc mã nguồn đã tạo:
- `android/app/src/main/AndroidManifest.xml`: Khai báo `android:hasCode="false"` và nạp trực tiếp `android.app.NativeActivity` liên kết với `libcyberbox_native.so`.
- `android/app/src/main/cpp/CMakeLists.txt`: Script build CMake NDK biên dịch thư viện C++ thuần với EGL, GLESv3, Log và `android_native_app_glue`.
- `src/android_main.cpp`: Entry point `android_main` xử lý EGL, render loop, touch gestures và khởi tạo Sandbox linh hoạt.
- `src/android_native_sandbox.hpp` & `src/android_native_sandbox.cpp`: Engine sandbox cho Android Linux hỗ trợ chuyển đổi linh hoạt thư mục làm việc (App Private / SDCard / Custom path).

---

### 2. Cách Build ra file APK:

#### Cách 1: Sử dụng Android Studio (Khuyên dùng)
1. Mở **Android Studio**.
2. Chọn **Open**, trỏ tới thư mục: `c:\Users\Tri\antigravity\joyful-faraday\android`.
3. Android Studio sẽ tự động đồng bộ Gradle và NDK CMake.
4. Chọn menu **Build > Build Bundle(s) / APK(s) > Build APK(s)**.
5. File APK sẽ được xuất ra tại: `android/app/build/outputs/apk/debug/app-debug.apk`.

#### Cách 2: Sử dụng dòng lệnh (Nếu máy đã có Android SDK/NDK)
Chạy lệnh sau trong PowerShell:
```powershell
cd android
./gradlew assembleDebug
```
Hoặc nếu dùng NDK trực tiếp:
```powershell
$NDK = "$env:ANDROID_NDK_HOME"
cmake -B build_android -DCMAKE_TOOLCHAIN_FILE="$NDK/build/cmake/android.toolchain.cmake" -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-24
cmake --build build_android
```
