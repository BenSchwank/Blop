!include "MUI2.nsh"

Name "Blop"
OutFile "Blop_Windows_Installer.exe"
InstallDir "$PROGRAMFILES64\Blop"
RequestExecutionLevel admin

; Version Information for Windows
VIProductVersion "3.11.9.0"
VIAddVersionKey "ProductName" "Blop"
VIAddVersionKey "FileVersion" "3.11.9"
VIAddVersionKey "ProductVersion" "3.11.9"
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

Section "Install"
  ; Sicherstellen, dass die App geschlossen ist, bevor Dateien überschrieben werden
  nsExec::ExecToStack 'taskkill /F /IM Blop.exe'
  Sleep 1000

  ; Preflight: fail-fast wenn kritische Deployment-Dateien fehlen
  IfFileExists "deployment\Blop.exe" +2 0
    Abort "deployment\\Blop.exe missing"
  IfFileExists "deployment\QtWebEngineProcess.exe" +2 0
    Abort "deployment\\QtWebEngineProcess.exe missing"
  IfFileExists "deployment\resources\icudtl.dat" +2 0
    Abort "deployment\\resources\\icudtl.dat missing"
  IfFileExists "deployment\resources\qtwebengine_resources.pak" +2 0
    Abort "deployment\\resources\\qtwebengine_resources.pak missing"
  IfFileExists "deployment\qtwebengine_locales\en-US.pak" +2 0
    Abort "deployment\\qtwebengine_locales\\en-US.pak missing"

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
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Blop" "DisplayVersion" "3.11.9"
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
