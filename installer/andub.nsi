; Andub Windows installer (NSIS 3, Unicode). Produces a single setup .exe.
;
;   makensis -DVERSION=2.0.0 -DBUNDLE=<dir with andub.exe> -DOUTFILE=<setup.exe> installer/andub.nsi
;
; Installs per user into %LOCALAPPDATA%\Programs\Andub (no admin rights / UAC),
; adds Start menu + desktop shortcuts and an entry in "Apps & features".
; User data in %LOCALAPPDATA%\anime-gui (favorites, history, settings) is kept on uninstall.

Unicode true
ManifestDPIAware true
SetCompressor /SOLID lzma
SetCompressorDictSize 64

!ifndef VERSION
  !define VERSION "2.0.0"
!endif
!ifndef BUNDLE
  !error "Pass -DBUNDLE=<path to the deployed app folder>"
!endif
!ifndef OUTFILE
  !define OUTFILE "Andub-Setup-x86_64.exe"
!endif

!define APPNAME "Andub"
!define PUBLISHER "navik677"
!define WEBSITE "https://github.com/navik677/andub"
!define UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}"
!define ICON "${__FILEDIR__}\..\resources\andub.ico"

Name "${APPNAME}"
OutFile "${OUTFILE}"
RequestExecutionLevel user
InstallDir "$LOCALAPPDATA\Programs\${APPNAME}"
InstallDirRegKey HKCU "Software\${APPNAME}" "InstallDir"
BrandingText "${APPNAME} ${VERSION}"

VIProductVersion "${VERSION}.0"
VIAddVersionKey /LANG=0 "ProductName" "${APPNAME}"
VIAddVersionKey /LANG=0 "FileDescription" "${APPNAME} Setup"
VIAddVersionKey /LANG=0 "FileVersion" "${VERSION}"
VIAddVersionKey /LANG=0 "ProductVersion" "${VERSION}"
VIAddVersionKey /LANG=0 "CompanyName" "${PUBLISHER}"
VIAddVersionKey /LANG=0 "LegalCopyright" "${PUBLISHER}"

!include "MUI2.nsh"
!include "FileFunc.nsh"

!define MUI_ICON "${ICON}"
!define MUI_UNICON "${ICON}"
!define MUI_ABORTWARNING
!define MUI_FINISHPAGE_RUN "$INSTDIR\andub.exe"
!define MUI_FINISHPAGE_SHOWREADME ""
!define MUI_FINISHPAGE_SHOWREADME_TEXT "$(DESKTOP_SHORTCUT)"
!define MUI_FINISHPAGE_SHOWREADME_FUNCTION CreateDesktopShortcut
!define MUI_FINISHPAGE_LINK "github.com/navik677/andub"
!define MUI_FINISHPAGE_LINK_LOCATION "${WEBSITE}"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "Ukrainian"
!insertmacro MUI_LANGUAGE "English"

LangString DESKTOP_SHORTCUT ${LANG_UKRAINIAN} "Ярлик на робочому столі"
LangString DESKTOP_SHORTCUT ${LANG_ENGLISH} "Desktop shortcut"
LangString APP_RUNNING ${LANG_UKRAINIAN} "Andub зараз запущено. Закрийте його та натисніть «Повторити»."
LangString APP_RUNNING ${LANG_ENGLISH} "Andub is running. Close it and press Retry."

Function CreateDesktopShortcut
  CreateShortcut "$DESKTOP\${APPNAME}.lnk" "$INSTDIR\andub.exe" "" "$INSTDIR\andub.exe" 0
FunctionEnd

; Refuse to overwrite files of a running copy (Windows locks loaded DLLs)
!macro CheckNotRunning
  check:
  ClearErrors
  IfFileExists "$INSTDIR\andub.exe" 0 done
  FileOpen $0 "$INSTDIR\andub.exe" a
  IfErrors 0 close
  MessageBox MB_RETRYCANCEL|MB_ICONEXCLAMATION "$(APP_RUNNING)" IDRETRY check
  Abort
  close:
  FileClose $0
  done:
!macroend

Section "Install"
  !insertmacro CheckNotRunning

  SetOutPath "$INSTDIR"
  ; Drop files of the previous version (Qt/ffmpeg DLL names change between releases)
  IfFileExists "$INSTDIR\andub.exe" 0 +4
    RMDir /r "$INSTDIR\qml"
    RMDir /r "$INSTDIR\plugins"
    Delete "$INSTDIR\*.dll"
  File /r "${BUNDLE}\*.*"

  WriteUninstaller "$INSTDIR\Uninstall.exe"
  WriteRegStr HKCU "Software\${APPNAME}" "InstallDir" "$INSTDIR"

  CreateShortcut "$SMPROGRAMS\${APPNAME}.lnk" "$INSTDIR\andub.exe" "" "$INSTDIR\andub.exe" 0

  ; Apps & features entry
  WriteRegStr HKCU "${UNINST_KEY}" "DisplayName" "${APPNAME}"
  WriteRegStr HKCU "${UNINST_KEY}" "DisplayVersion" "${VERSION}"
  WriteRegStr HKCU "${UNINST_KEY}" "Publisher" "${PUBLISHER}"
  WriteRegStr HKCU "${UNINST_KEY}" "URLInfoAbout" "${WEBSITE}"
  WriteRegStr HKCU "${UNINST_KEY}" "DisplayIcon" "$INSTDIR\andub.exe"
  WriteRegStr HKCU "${UNINST_KEY}" "InstallLocation" "$INSTDIR"
  WriteRegStr HKCU "${UNINST_KEY}" "UninstallString" '"$INSTDIR\Uninstall.exe"'
  WriteRegStr HKCU "${UNINST_KEY}" "QuietUninstallString" '"$INSTDIR\Uninstall.exe" /S'
  WriteRegDWORD HKCU "${UNINST_KEY}" "NoModify" 1
  WriteRegDWORD HKCU "${UNINST_KEY}" "NoRepair" 1
  ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
  IntFmt $0 "0x%08X" $0
  WriteRegDWORD HKCU "${UNINST_KEY}" "EstimatedSize" "$0"
SectionEnd

Section "Uninstall"
  !insertmacro CheckNotRunning
  Delete "$SMPROGRAMS\${APPNAME}.lnk"
  Delete "$DESKTOP\${APPNAME}.lnk"
  DeleteRegKey HKCU "${UNINST_KEY}"
  DeleteRegKey HKCU "Software\${APPNAME}"

  ; Only wipe the folder if it really is ours
  IfFileExists "$INSTDIR\andub.exe" 0 +3
    RMDir /r "$INSTDIR"
    Goto +2
  Delete "$INSTDIR\Uninstall.exe"
SectionEnd
