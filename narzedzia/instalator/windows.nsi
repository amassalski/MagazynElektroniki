; Instalator Domowego Magazynu (NSIS 3, Unicode). Parametry przekazuje zbuduj_windows.sh przez makensis -D:
;   WERSJA, STAGE (katalog z gotowym programem), WYNIK (plik wyjściowy), IKONA (opcjonalnie, .ico)
Unicode true
!include "MUI2.nsh"

Name "Domowy Magazyn"
OutFile "${WYNIK}"
InstallDir "$PROGRAMFILES64\DomowyMagazyn"
InstallDirRegKey HKLM "Software\DomowyMagazyn" "Katalog"
RequestExecutionLevel admin
SetCompressor /SOLID lzma

VIProductVersion "${WERSJA}.0"
VIAddVersionKey /LANG=1045 "ProductName" "Domowy Magazyn"
VIAddVersionKey /LANG=1045 "FileDescription" "Instalator programu Domowy Magazyn"
VIAddVersionKey /LANG=1045 "FileVersion" "${WERSJA}"
VIAddVersionKey /LANG=1045 "LegalCopyright" "Prywatny projekt autora"

!ifdef IKONA
  !define MUI_ICON "${IKONA}"
  !define MUI_UNICON "${IKONA}"
!endif
!define MUI_ABORTWARNING
!define MUI_FINISHPAGE_RUN "$INSTDIR\DomowyMagazyn.exe"
!define MUI_FINISHPAGE_RUN_TEXT "Uruchom Domowy Magazyn"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "Polish"

Section "Program" SecProgram
  SectionIn RO
  SetOutPath "$INSTDIR"
  File /r "${STAGE}\*.*"
  WriteUninstaller "$INSTDIR\Odinstaluj.exe"

  CreateDirectory "$SMPROGRAMS\Domowy Magazyn"
  CreateShortcut "$SMPROGRAMS\Domowy Magazyn\Domowy Magazyn.lnk" "$INSTDIR\DomowyMagazyn.exe"
  CreateShortcut "$SMPROGRAMS\Domowy Magazyn\Odinstaluj.lnk" "$INSTDIR\Odinstaluj.exe"
  CreateShortcut "$DESKTOP\Domowy Magazyn.lnk" "$INSTDIR\DomowyMagazyn.exe"

  WriteRegStr HKLM "Software\DomowyMagazyn" "Katalog" "$INSTDIR"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\DomowyMagazyn" "DisplayName" "Domowy Magazyn"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\DomowyMagazyn" "DisplayVersion" "${WERSJA}"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\DomowyMagazyn" "UninstallString" '"$INSTDIR\Odinstaluj.exe"'
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\DomowyMagazyn" "DisplayIcon" "$INSTDIR\DomowyMagazyn.exe"
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\DomowyMagazyn" "NoModify" 1
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\DomowyMagazyn" "NoRepair" 1
SectionEnd

Section "Uninstall"
  ; Dane użytkownika (konfiguracja w %APPDATA%\DomowyMagazyn) zostają, baza danych też.
  Delete "$DESKTOP\Domowy Magazyn.lnk"
  RMDir /r "$SMPROGRAMS\Domowy Magazyn"
  RMDir /r "$INSTDIR"
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\DomowyMagazyn"
  DeleteRegKey HKLM "Software\DomowyMagazyn"
SectionEnd
