# 🔧 Guide: Building the Rubberband Static Library and XCFramework (iOS + macOS)

## 📘 Description:
This guide walks you step-by-step through building the Rubberband audio library for multiple Apple platforms (iOS device, iOS simulators [arm64 + x86_64], and macOS universal). It includes compiling each platform variant, tagging them properly, combining simulator slices, and creating a universal `.xcframework`. This uses a clean `build/` directory structure for organized output.

---

## ✅ Required Tools:
Ensure these tools are installed on your system before continuing:

- [Homebrew](https://brew.sh/)
- Meson
- Ninja
- Python 3
- Xcode (with command line tools)

> You can find many YouTube tutorials or documentation online to learn how to install these tools via Homebrew.

---

## 📁 Directory Structure Overview:

- **Project root:** `rubberband-4.0.0/`
  - **Headers:** `rubberband-4.0.0/rubberband/`
  - **Cross files:** `rubberband-4.0.0/cross/`
  - **Build output:** `rubberband-4.0.0/build/`

---

## 🪜 Step-by-Step Instructions:

### 1. 📂 Manually Create Only Required Build Subdirectories

```bash
cd rubberband-4.0.0

mkdir -p build/ios-simulator-universal
mkdir -p build/xcframework-slices
```

> All other platform-specific build directories will be created by Meson automatically.

---

### 2. 🏗️ Build Each Platform Using Meson

```bash
# iOS Device
meson setup build/ios-device --cross-file cross/ios.txt --buildtype=release
ninja -C build/ios-device

# iOS Simulator (ARM64)
meson setup build/ios-simulator-arm64 --cross-file cross/ios-simulator-arm64.txt --buildtype=release
ninja -C build/ios-simulator-arm64

# iOS Simulator (x86_64)
meson setup build/ios-simulator-x86_64 --cross-file cross/ios-simulator.txt --buildtype=release
ninja -C build/ios-simulator-x86_64

# macOS Universal
meson setup build/macos-universal --cross-file cross/macos-universal.txt --buildtype=release
ninja -C build/macos-universal
```

---

### 3. 🏷️ Tag All Compiled Libraries with Platform Metadata

```bash
# iOS Device
xcrun --sdk iphoneos lipo -create -output build/xcframework-slices/librubberband_device.arm64.a build/ios-device/librubberband.a

# iOS Simulator (arm64)
xcrun --sdk iphonesimulator lipo -create -output build/xcframework-slices/librubberband_simulator.arm64.a build/ios-simulator-arm64/librubberband.a

# iOS Simulator (x86_64)
xcrun --sdk iphonesimulator lipo -create -output build/xcframework-slices/librubberband_simulator.x86_64.a build/ios-simulator-x86_64/librubberband.a

# macOS Universal
xcrun --sdk macosx lipo -create -output build/xcframework-slices/librubberband_macos_universal.a build/macos-universal/librubberband.a
```

---

### 4. 🧬 Merge iOS Simulator Slices into a Universal Simulator Library

```bash
lipo -create \
  build/xcframework-slices/librubberband_simulator.arm64.a \
  build/xcframework-slices/librubberband_simulator.x86_64.a \
  -output build/xcframework-slices/librubberband_simulator.universal.a
```

---

### 5. 📦 Create the Final XCFramework

```bash
xcodebuild -create-xcframework \
  -library build/xcframework-slices/librubberband_device.arm64.a -headers rubberband \
  -library build/xcframework-slices/librubberband_simulator.universal.a -headers rubberband \
  -library build/xcframework-slices/librubberband_macos_universal.a -headers rubberband \
  -output build/Rubberband.xcframework
```

> The final XCFramework will be located at: `rubberband-4.0.0/build/Rubberband.xcframework`

---

## ✅ Complete!
You now have a universal `Rubberband.xcframework` ready to use in iOS/macOS projects with full simulator and device support.
