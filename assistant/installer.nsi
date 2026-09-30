Unicode True
!include "MUI2.nsh"

!define APP_NAME "Blop Assistent"
!define APP_EXE "BlopAssistent.exe"
!define APP_VERSION "1.0.0"
!define APP_PUBLISHER "Blop"

Name "${APP_NAME}"
OutFile "BlopAssistent_Windows_Installer.exe"
InstallDir "$LOCALAPPDATA\Blop Assistent"
InstallDirRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\BlopAssistent" "InstallLocation"
RequestExecutionLevel user
ShowInstDetails show
ShowUninstDetails show

VIProductVersion "1.0.0.0"
VIAddVersionKey "ProductName" "${APP_NAME}"
VIAddVersionKey "CompanyName" "${APP_PUBLISHER}"
VIAddVersionKey "FileVersion" "${APP_VERSION}"
VIAddVersionKey "ProductVersion" "${APP_VERSION}"
VIAddVersionKey "FileDescription" "Blop Assistent Installer"
VIAddVersionKey "LegalCopyright" "${APP_PUBLISHER}"

!define MUI_ABORTWARNING
!define MUI_UNABORTWARNING
!define MUI_WELCOMEPAGE_TITLE "Blop Assistent einrichten"
!define MUI_WELCOMEPAGE_TEXT "Der Blop Assistent ist eine eigene App im Blop-Ökosystem.$\r$\n$\r$\nNach der Einrichtung sitzt eine kleine Notch am oberen Bildschirmrand. Per Tippen oder Sprache öffnest du Ordner, den Browser, bekannte Programme und speicherst Blop-Notizen.$\r$\n$\r$\nKlicke auf Weiter, um fortzufahren."
!define MUI_FINISHPAGE_TITLE "Blop Assistent ist bereit"
!define MUI_FINISHPAGE_TEXT "Die Einrichtung ist abgeschlossen.$\r$\n$\r$\nStarte den Assistenten mit der Verknüpfung oder über Strg+Leertaste, sobald er läuft."
!define MUI_FINISHPAGE_RUN "$INSTDIR\${APP_EXE}"
!define MUI_FINISHPAGE_RUN_TEXT "Blop Assistent jetzt starten"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_WELCOME
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "German"

!if /FileExists "deployment\${APP_EXE}"
!else
  !error "deployment\\BlopAssistent.exe missing. Run installer.ps1 first."
!endif
!if /FileExists "deployment\platforms\qwindows.dll"
!else
  !error "deployment\\platforms\\qwindows.dll missing."
!endif

Section "Blop Assistent" SecApp
  SectionIn RO
  SetShellVarContext current

  nsExec::ExecToStack 'taskkill /F /IM ${APP_EXE}'
  Sleep 800

  SetOutPath "$INSTDIR"
  File /r "deployment\*"

  WriteUninstaller "$INSTDIR\uninstall.exe"

  CreateDirectory "$SMPROGRAMS\Blop Assistent"
  CreateShortcut "$SMPROGRAMS\Blop Assistent\Blop Assistent.lnk" "$INSTDIR\${APP_EXE}" "" "$INSTDIR\${APP_EXE}" 0
  CreateShortcut "$SMPROGRAMS\Blop Assistent\Deinstallieren.lnk" "$INSTDIR\uninstall.exe"
  CreateShortcut "$DESKTOP\Blop Assistent.lnk" "$INSTDIR\${APP_EXE}" "" "$INSTDIR\${APP_EXE}" 0

  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\BlopAssistent" "DisplayName" "${APP_NAME}"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\BlopAssistent" "DisplayVersion" "${APP_VERSION}"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\BlopAssistent" "Publisher" "${APP_PUBLISHER}"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\BlopAssistent" "DisplayIcon" "$INSTDIR\${APP_EXE}"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\BlopAssistent" "InstallLocation" "$INSTDIR"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\BlopAssistent" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\BlopAssistent" "QuietUninstallString" '"$INSTDIR\uninstall.exe" /S'
  WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\BlopAssistent" "NoModify" 1
  WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\BlopAssistent" "NoRepair" 1
SectionEnd

Section "Uninstall"
  SetShellVarContext current
  nsExec::ExecToStack 'taskkill /F /IM ${APP_EXE}'
  Sleep 800

  Delete "$INSTDIR\uninstall.exe"
  Delete "$DESKTOP\Blop Assistent.lnk"
  Delete "$SMPROGRAMS\Blop Assistent\Blop Assistent.lnk"
  Delete "$SMPROGRAMS\Blop Assistent\Deinstallieren.lnk"
  RMDir "$SMPROGRAMS\Blop Assistent"

  DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\BlopAssistent"

  RMDir /r "$INSTDIR"
SectionEnd
