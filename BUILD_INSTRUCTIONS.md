# Build Instructions for v3.18.21

## Android WebView Fix
This version replaces QQuickWidget with QQuickView to fix the Android SurfaceView compositing issue that caused the blank screen during login.

## Local Build (Qt Creator)
1. Open Qt Creator
2. File → Open Project → select CMakeLists.txt
3. Configure for Android kit
4. Build → Clean All
5. Build → Rebuild All
6. Run on Android device

## Manual APK Build
If Qt Creator fails, you can build manually:

```bash
# In project root
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
cmake --build . --target Blop

# Then in android-build directory
cd android-build-Blop
./gradlew assembleDebug
```

## GitHub Actions Issue
The current GitHub Actions workflow may fail due to Qt installer corruption. This is a known issue with aqtinstall and doesn't affect the actual code functionality.

## Testing
After installation, test the login screen:
- Open Blop Study
- Login screen should be visible (not blank)
- Fullscreen mode should work correctly
- WebView content should be interactive

If issues persist, please provide Android Studio logcat output with filter "com.benschwank.blop".
