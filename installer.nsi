!include "MUI2.nsh"

Name "Blop"
OutFile "Blop_Windows_Installer.exe"
InstallDir "$PROGRAMFILES64\Blop"
RequestExecutionLevel admin

; Version Information for Windows
; APP_VERSION (x.y.z) is passed by CI: makensis /DAPP_VERSION=3.29.47 installer.nsi
!ifndef APP_VERSION
  !define APP_VERSION "0.0.0"
!endif
VIProductVersion "${APP_VERSION}.0"
VIAddVersionKey "ProductName" "Blop"
VIAddVersionKey "FileVersion" "${APP_VERSION}"
VIAddVersionKey "ProductVersion" "${APP_VERSION}"
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
  SetShellVarContext current
  ; Sicherstellen, dass die App geschlossen ist, bevor Dateien überschrieben werden
  nsExec::ExecToStack 'taskkill /IM Blop.exe'
  Sleep 2000

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
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Blop" "DisplayVersion" "${APP_VERSION}"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Blop" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Blop" "QuietUninstallString" '"$INSTDIR\uninstall.exe" /S'

  ; blop:// protocol — return from Google login in the system browser
  WriteRegStr HKCU "Software\Classes\blop" "" "URL:Blop Protocol"
  WriteRegStr HKCU "Software\Classes\blop" "URL Protocol" ""
  WriteRegStr HKCU "Software\Classes\blop\shell\open\command" "" '"$INSTDIR\Blop.exe" "%1"'
SectionEnd

Section "Uninstall"
  SetShellVarContext current
  nsExec::ExecToStack 'taskkill /IM Blop.exe'
  Sleep 2000
  Delete "$INSTDIR\uninstall.exe"
  Delete "$SMPROGRAMS\Blop.lnk"
  Delete "$DESKTOP\Blop.lnk"
  
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Blop"
  DeleteRegKey HKCU "Software\Classes\blop"
  
  RMDir /r "$INSTDIR"
SectionEnd
