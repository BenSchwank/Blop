!include "MUI2.nsh"

Name "Blop"
OutFile "Blop_Windows_Installer.exe"
InstallDir "$PROGRAMFILES64\Blop"
RequestExecutionLevel admin

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_WELCOME
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "German"

Section "Install"
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
