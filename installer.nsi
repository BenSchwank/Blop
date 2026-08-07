!include "MUI2.nsh"

Name "Blop"
OutFile "Blop_Windows_Installer.exe"
InstallDir "$PROGRAMFILES64\Blop"
RequestExecutionLevel admin

; Version: pass /DBLOP_PRODUCT_VERSION=x.y.z from CI (git describe).
; Fallback keeps local makensis working without defines.
!ifndef BLOP_PRODUCT_VERSION
  !define BLOP_PRODUCT_VERSION "0.0.0"
!endif
VIProductVersion "${BLOP_PRODUCT_VERSION}.0"
VIAddVersionKey "ProductName" "Blop"
VIAddVersionKey "FileVersion" "${BLOP_PRODUCT_VERSION}"
VIAddVersionKey "ProductVersion" "${BLOP_PRODUCT_VERSION}"
VIAddVersionKey "FileDescription" "Blop Installer"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!define MUI_FINISHPAGE_RUN "$INSTDIR\Blop.exe"
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_WELCOME
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "German"

; Build-time preflight: fail during makensis if deployment payload is incomplete.
!if /FileExists "deployment\Blop.exe"
!else
  !error "deployment\\Blop.exe missing"
!endif
!if /FileExists "deployment\QtWebEngineProcess.exe"
!else
  !error "deployment\\QtWebEngineProcess.exe missing"
!endif
!if /FileExists "deployment\resources\icudtl.dat"
!else
  !error "deployment\\resources\\icudtl.dat missing"
!endif
!if /FileExists "deployment\resources\qtwebengine_resources.pak"
!else
  !error "deployment\\resources\\qtwebengine_resources.pak missing"
!endif
!if /FileExists "deployment\qtwebengine_locales\en-US.pak"
!else
  !error "deployment\\qtwebengine_locales\\en-US.pak missing"
!endif

Section "Install"
  ; Sicherstellen, dass die App geschlossen ist, bevor Dateien überschrieben werden
  nsExec::ExecToStack 'taskkill /F /IM Blop.exe'
  Sleep 1000

  SetOutPath "$INSTDIR"
  
  ; Kopiere alle Dateien aus deployment (inkl. Unterordner)
  File /r "deployment\*"
  
  ; Uninstaller schreiben
  WriteUninstaller "$INSTDIR\uninstall.exe"
  
  ; Verknüpfungen erstellen
  CreateShortcut "$SMPROGRAMS\Blop.lnk" "$INSTDIR\Blop.exe"
  CreateShortcut "$DESKTOP\Blop.lnk" "$INSTDIR\Blop.exe"
  
  ; Registry Eintrag für Deinstallation (optional, aber gut für Systemsteuerung)
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Blop" "DisplayName" "Blop"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Blop" "DisplayVersion" "${BLOP_PRODUCT_VERSION}"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Blop" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Blop" "QuietUninstallString" '"$INSTDIR\uninstall.exe" /S'
SectionEnd

Section "Uninstall"
  Delete "$INSTDIR\uninstall.exe"
  Delete "$SMPROGRAMS\Blop.lnk"
  Delete "$DESKTOP\Blop.lnk"
  
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Blop"
  
  RMDir /r "$INSTDIR"
SectionEnd
