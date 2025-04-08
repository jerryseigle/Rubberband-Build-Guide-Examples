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

Before building, you need to create a cross file for **iOS Simulator ARM64** if it does not exist yet. Save this file as `cross/ios-simulator-arm64.txt`:

```
[constants]
sysroot = '/Applications/Xcode.app/Contents/Developer/Platforms/iPhoneSimulator.platform/Developer/SDKs/iPhoneSimulator.sdk'

[host_machine]
system = 'darwin'
cpu_family = 'aarch64'
cpu = 'arm64'
endian = 'little'

[properties]
needs_exe_wrapper = true

[binaries]
c = 'cc'
cpp = 'c++'
strip = 'strip'

[built-in options]
c_args = [ '-isysroot', sysroot, '-arch', 'arm64', '-mios-simulator-version-min=8', '-fembed-bitcode' ]
cpp_args = [ '-isysroot', sysroot, '-arch', 'arm64', '-mios-simulator-version-min=8', '-stdlib=libc++', '-fembed-bitcode' ]
cpp_link_args = [ '-isysroot', sysroot, '-arch', 'arm64', '-mios-simulator-version-min=8', '-stdlib=libc++', '-framework', 'CoreFoundation']
```

You may choose to skip the macOS build if you do not need it.

---

We need to build the platform for iOS device. This is for running the library on a physical iOS device:

```bash
meson setup build/ios-device --cross-file cross/ios.txt --buildtype=release
ninja -C build/ios-device
```

Now we build the platform for iOS simulator (ARM64). This is for M1/M2/M3 Mac simulators:

```bash
meson setup build/ios-simulator-arm64 --cross-file cross/ios-simulator-arm64.txt --buildtype=release
ninja -C build/ios-simulator-arm64
```

Next, we build the platform for iOS simulator (x86_64). This is for Intel-based Mac simulators:

```bash
meson setup build/ios-simulator-x86_64 --cross-file cross/ios-simulator.txt --buildtype=release
ninja -C build/ios-simulator-x86_64
```

Optional: Build for macOS (universal). This supports both Intel and Apple Silicon Macs:

```bash
meson setup build/macos-universal --cross-file cross/macos-universal.txt --buildtype=release
ninja -C build/macos-universal
```

> ⚠️ If you are only building one specific target (e.g., only for simulator), you're done after this step. Your `.a` files are located in their respective `build/` directories.  
> However, keep in mind that **a simulator-only build will only work on simulators of the architecture you built** (e.g., arm64 or x86_64).

---

### 3. 🧬 Merge iOS Simulator Slices into a Universal Simulator Library

If you built both arm64 and x86_64 versions of the simulator, merge them into one universal `.a` file that supports both architectures:

```bash
lipo -create   build/ios-simulator-x86_64/librubberband.a   build/ios-simulator-arm64/librubberband.a   -output build/ios-simulator-universal/librubberband.a
```

---

### 4. 🏷️ Tag All Compiled Libraries with Platform Metadata

Only tag the libraries that you built in step 2. Remove any command here if you didn’t build that platform.

> 📝 **Note:** If you did not create a universal simulator build in step 3, you need to change the simulator tagging line below.  
> Replace the universal simulator path (`build/ios-simulator-universal/librubberband.a`) with the path to the actual simulator `.a` file you built (either arm64 or x86_64).

```bash
xcrun --sdk iphoneos lipo -create -output build/xcframework-slices/librubberband_device.arm64.a build/ios-device/librubberband.a

xcrun --sdk iphonesimulator lipo -create -output build/xcframework-slices/librubberband_simulator.universal.a build/ios-simulator-universal/librubberband.a

xcrun --sdk macosx lipo -create -output build/xcframework-slices/librubberband_macos_universal.a build/macos-universal/librubberband.a
```

---

### 5. 📦 Create the Final XCFramework

Only include the platform libraries you actually tagged in step 4. If you did not build or tag a simulator universal or macOS library, remove those lines accordingly.

> 📝 For example, if you only built and tagged for iOS device and arm64 simulator, use the path to the `arm64` simulator `.a` file you tagged instead of the universal one.

```bash
xcodebuild -create-xcframework   -library build/xcframework-slices/librubberband_device.arm64.a -headers rubberband   -library build/xcframework-slices/librubberband_simulator.universal.a -headers rubberband   -library build/xcframework-slices/librubberband_macos_universal.a -headers rubberband   -output build/Rubberband.xcframework
```

> The final XCFramework will be located at: `rubberband-4.0.0/build/Rubberband.xcframework`

---

## ✅ Complete!
You now have a universal `Rubberband.xcframework` ready to use in iOS/macOS projects with full simulator and device support.
