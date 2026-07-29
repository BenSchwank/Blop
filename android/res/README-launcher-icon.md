# Android launcher icon

Play Store listing artwork is uploaded separately in Play Console.
The **on-device home-screen icon** comes from the APK/AAB resources below.

## Sources

- Brand master: `assets/logo.jpg`
- Generated densites: `android/res/mipmap-*/ic_launcher.png` (+ `_round`)
- Adaptive (API 26+): `android/res/mipmap-anydpi-v26/ic_launcher.xml`
  - layers: `drawable/ic_launcher_background.png`, `drawable/ic_launcher_foreground.png`
- Manifest: `android:icon` / `android:roundIcon` → `@mipmap/ic_launcher(_round)`

## Regenerate after logo change

```bash
pip install pillow
python3 scripts/generate_android_launcher_icons.py
```

Then rebuild the Android APK/AAB and reinstall (launcher caches icons — may need
uninstall or reboot to see the new icon).
