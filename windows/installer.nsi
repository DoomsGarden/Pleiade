Unicode true
Name "Pléiade ${PRODUCT_VERSION}"
OutFile "${OUTPUT_FILE}"
InstallDir "$LOCALAPPDATA\Programs\Pleiade"
RequestExecutionLevel user

!include "MUI2.nsh"
!include "x64.nsh"

VIProductVersion "${PRODUCT_VERSION_NUMERIC}"
VIAddVersionKey /LANG=3082 "ProductName" "Pléiade"
VIAddVersionKey /LANG=3082 "FileDescription" "Instalador del visor WebGL de Pléiade"
VIAddVersionKey /LANG=3082 "FileVersion" "${PRODUCT_VERSION}"
VIAddVersionKey /LANG=3082 "ProductVersion" "${PRODUCT_VERSION}"
VIAddVersionKey /LANG=3082 "LegalCopyright" "DoomsGarden"

!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "SpanishInternational"

!define UNINSTALL_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\Pleiade"

Function .onInit
  ${IfNot} ${RunningX64}
    MessageBox MB_OK|MB_ICONSTOP "Este instalador requiere Windows de 64 bits."
    Abort
  ${EndIf}
  SetRegView 64
FunctionEnd

Section "Pléiade"
  SetOutPath "$INSTDIR"
  SetOverwrite on
  File "${PAYLOAD_DIR}/Pleiade.exe"
  File "${PAYLOAD_DIR}/WebView2Loader.dll"
  File /oname=WebView2.LICENSE.txt "${PAYLOAD_DIR}/WebView2.LICENSE.txt"

  RMDir /r "$INSTDIR\viewer"
  SetOutPath "$INSTDIR\viewer"
  File /r "${DIST_DIR}/*"

  SetOutPath "$INSTDIR"
  WriteUninstaller "$INSTDIR\Uninstall.exe"
  CreateDirectory "$SMPROGRAMS\Pléiade"
  CreateShortCut "$SMPROGRAMS\Pléiade\Visor de Pléiade.lnk" "$INSTDIR\Pleiade.exe"

  WriteRegStr HKCU "${UNINSTALL_KEY}" "DisplayName" "Pléiade"
  WriteRegStr HKCU "${UNINSTALL_KEY}" "DisplayVersion" "${PRODUCT_VERSION}"
  WriteRegStr HKCU "${UNINSTALL_KEY}" "Publisher" "DoomsGarden"
  WriteRegStr HKCU "${UNINSTALL_KEY}" "DisplayIcon" "$INSTDIR\Pleiade.exe,0"
  WriteRegStr HKCU "${UNINSTALL_KEY}" "InstallLocation" "$INSTDIR"
  WriteRegStr HKCU "${UNINSTALL_KEY}" "UninstallString" '$\"$INSTDIR\Uninstall.exe$\"'
  WriteRegDWORD HKCU "${UNINSTALL_KEY}" "NoModify" 1
  WriteRegDWORD HKCU "${UNINSTALL_KEY}" "NoRepair" 1
SectionEnd

Section "Uninstall"
  SetRegView 64
  DeleteRegKey HKCU "${UNINSTALL_KEY}"
  Delete "$SMPROGRAMS\Pléiade\Visor de Pléiade.lnk"
  RMDir "$SMPROGRAMS\Pléiade"
  RMDir /r "$INSTDIR\viewer"
  Delete "$INSTDIR\Pleiade.exe"
  Delete "$INSTDIR\WebView2Loader.dll"
  Delete "$INSTDIR\WebView2.LICENSE.txt"
  Delete "$INSTDIR\Uninstall.exe"
  RMDir "$INSTDIR"
SectionEnd
