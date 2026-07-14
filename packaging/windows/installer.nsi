; METAL NAM GEAR PLAYER - NSIS installer script
; Build:
;   makensis -DVERSION=0.1.1 \
;            -DSTAGING=/path/to/staging \
;            -DOUTFILE=/path/to/MetalNAMGearPlayer-windows-x64-setup.exe \
;            packaging/windows/installer.nsi
;
; STAGING must contain:
;   NAM Custom.exe
;   NAM Custom.vst3/          (VST3 bundle, real dir tree)
;   docs/
;   LICENSE
;   README.txt

Unicode true
SetCompressor /SOLID lzma

!ifndef VERSION
  !define VERSION "0.0.0"
!endif
!ifndef STAGING
  !error "STAGING not defined"
!endif
!ifndef OUTFILE
  !define OUTFILE "MetalNAMGearPlayer-windows-x64-setup.exe"
!endif

!define APPNAME       "METAL NAM GEAR PLAYER"
!define VENDOR        "fabionet"
!define REGKEY        "Software\Microsoft\Windows\CurrentVersion\Uninstall\MetalNAMGearPlayer"
!define URL_HOMEPAGE  "https://github.com/fabionet/metal-nam-gear-player"

Name        "${APPNAME}"
BrandingText "${APPNAME} v${VERSION} - fabionet"
OutFile     "${OUTFILE}"
InstallDir  "$PROGRAMFILES64\MetalNAMGearPlayer"
InstallDirRegKey HKLM "${REGKEY}" "InstallLocation"

RequestExecutionLevel admin
ShowInstDetails show
ShowUnInstDetails show

VIProductVersion  "0.1.1.0"
VIAddVersionKey   "ProductName"     "${APPNAME}"
VIAddVersionKey   "CompanyName"     "${VENDOR}"
VIAddVersionKey   "LegalCopyright"  "AGPL-3.0-or-later"
VIAddVersionKey   "FileDescription" "${APPNAME} installer"
VIAddVersionKey   "FileVersion"     "${VERSION}"
VIAddVersionKey   "ProductVersion"  "${VERSION}"

!include "MUI2.nsh"
!include "x64.nsh"
!include "FileFunc.nsh"

!define MUI_ICON   "${NSISDIR}\Contrib\Graphics\Icons\modern-install.ico"
!define MUI_UNICON "${NSISDIR}\Contrib\Graphics\Icons\modern-uninstall.ico"
!define MUI_ABORTWARNING
!define MUI_FINISHPAGE_LINK          "Visit the project on GitHub"
!define MUI_FINISHPAGE_LINK_LOCATION "${URL_HOMEPAGE}"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE     "${STAGING}\LICENSE"
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"
!insertmacro MUI_LANGUAGE "Italian"

Function .onInit
  ${IfNot} ${RunningX64}
    MessageBox MB_ICONSTOP "This build targets 64-bit Windows only."
    Abort
  ${EndIf}
  SetRegView 64
FunctionEnd

Function un.onInit
  SetRegView 64
FunctionEnd

Section "Standalone application" SEC_STANDALONE
  SectionIn RO
  SetOutPath "$INSTDIR"
  File "${STAGING}\NAM Custom.exe"
  File "${STAGING}\LICENSE"
  File "${STAGING}\README.txt"

  SetOutPath "$INSTDIR\docs"
  File /r "${STAGING}\docs\*.*"

  ; Start Menu shortcut for the standalone
  CreateDirectory "$SMPROGRAMS\METAL NAM GEAR PLAYER"
  CreateShortCut  "$SMPROGRAMS\METAL NAM GEAR PLAYER\NAM Custom (Standalone).lnk" \
                  "$INSTDIR\NAM Custom.exe"
  CreateShortCut  "$SMPROGRAMS\METAL NAM GEAR PLAYER\User guide (Italian).lnk" \
                  "$INSTDIR\docs\guida-rapida.pdf"
  CreateShortCut  "$SMPROGRAMS\METAL NAM GEAR PLAYER\Uninstall.lnk" \
                  "$INSTDIR\uninstall.exe"

  ; Uninstaller + Add/Remove Programs entry
  WriteUninstaller "$INSTDIR\uninstall.exe"
  ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2

  WriteRegStr   HKLM "${REGKEY}" "DisplayName"       "${APPNAME}"
  WriteRegStr   HKLM "${REGKEY}" "DisplayVersion"    "${VERSION}"
  WriteRegStr   HKLM "${REGKEY}" "Publisher"         "${VENDOR}"
  WriteRegStr   HKLM "${REGKEY}" "URLInfoAbout"      "${URL_HOMEPAGE}"
  WriteRegStr   HKLM "${REGKEY}" "InstallLocation"   "$INSTDIR"
  WriteRegStr   HKLM "${REGKEY}" "UninstallString"   '"$INSTDIR\uninstall.exe"'
  WriteRegStr   HKLM "${REGKEY}" "QuietUninstallString" '"$INSTDIR\uninstall.exe" /S'
  WriteRegDWORD HKLM "${REGKEY}" "EstimatedSize"     $0
  WriteRegDWORD HKLM "${REGKEY}" "NoModify"          1
  WriteRegDWORD HKLM "${REGKEY}" "NoRepair"          1
SectionEnd

Section "VST3 plugin (system-wide)" SEC_VST3
  ; System-wide VST3 install path is $COMMONFILES64\VST3\
  SetOutPath "$COMMONFILES64\VST3"
  File /r "${STAGING}\NAM Custom.vst3"
SectionEnd

; Section descriptions
LangString DESC_STANDALONE ${LANG_ENGLISH} "Standalone JACK/ASIO application, documentation and uninstaller (required)."
LangString DESC_VST3       ${LANG_ENGLISH} "VST3 plugin installed to the system-wide VST3 folder (Common Files\VST3)."
LangString DESC_STANDALONE ${LANG_ITALIAN} "Applicazione Standalone, documentazione e disinstallatore (obbligatorio)."
LangString DESC_VST3       ${LANG_ITALIAN} "Plugin VST3 nella cartella di sistema (Common Files\VST3)."

!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC_STANDALONE} $(DESC_STANDALONE)
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC_VST3}       $(DESC_VST3)
!insertmacro MUI_FUNCTION_DESCRIPTION_END

Section "Uninstall"
  SetRegView 64

  ; Standalone tree
  Delete "$INSTDIR\NAM Custom.exe"
  Delete "$INSTDIR\LICENSE"
  Delete "$INSTDIR\README.txt"
  Delete "$INSTDIR\uninstall.exe"
  RMDir /r "$INSTDIR\docs"
  RMDir "$INSTDIR"

  ; VST3 bundle
  RMDir /r "$COMMONFILES64\VST3\NAM Custom.vst3"

  ; Start Menu
  Delete   "$SMPROGRAMS\METAL NAM GEAR PLAYER\NAM Custom (Standalone).lnk"
  Delete   "$SMPROGRAMS\METAL NAM GEAR PLAYER\User guide (Italian).lnk"
  Delete   "$SMPROGRAMS\METAL NAM GEAR PLAYER\Uninstall.lnk"
  RMDir    "$SMPROGRAMS\METAL NAM GEAR PLAYER"

  DeleteRegKey HKLM "${REGKEY}"
SectionEnd
