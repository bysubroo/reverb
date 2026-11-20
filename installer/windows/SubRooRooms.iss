; SubRoo Rooms - Windows installer (Inno Setup 6.5 or newer)
;
; Build:  iscc /DAppVersion=1.0.0 /DBuildDir=<...>\SubRooRooms_artefacts\Release /DPackKey=<key> SubRooRooms.iss
;
; - Installs the VST3 + CLAP plug-ins and the standalone app (static runtime, nothing else needed).
; - Downloads the measured location packs from GitHub, checks their SHA-256, decrypts and unpacks them.
;   Packs are only downloaded again when the pack version changes.
; - A newer installer installs over the previous version (same AppId).

#ifndef AppVersion
  #define AppVersion "1.0.0"
#endif
#ifndef BuildDir
  #define BuildDir "..\..\build\SubRooRooms_artefacts\Release"
#endif
#ifndef NumericVersion
  #define NumericVersion AppVersion
#endif
#ifndef OutputDir
  #define OutputDir "..\..\dist-installer"
#endif

#include "packs.iss"

#define AppName "SubRoo Rooms"
#define AppExe "SubRoo Rooms.exe"
#define PacksDir "{commonappdata}\SubRoo Rooms\Packs"

[Setup]
AppId={{6E0C2B7A-4F1D-4B8E-9C3A-51D0F2A7E9B4}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher=SubRoo
AppPublisherURL=https://subroo.xyz
AppSupportURL=https://subroo.xyz
AppContact=contact@subroo.xyz
AppCopyright=Copyright (C) 2026 SubRoo
VersionInfoVersion={#NumericVersion}
VersionInfoProductName={#AppName}
VersionInfoDescription={#AppName} installer
DefaultDirName={autopf}\SubRoo\{#AppName}
DisableDirPage=yes
DisableProgramGroupPage=yes
UsePreviousSetupType=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
MinVersion=10.0
OutputDir={#OutputDir}
OutputBaseFilename=SubRoo-Rooms-{#AppVersion}-Windows-Setup
SetupIconFile=SubRooRooms.ico
UninstallDisplayIcon={app}\{#AppExe}
UninstallDisplayName={#AppName}
WizardStyle=modern
WizardSizePercent=110
WizardImageFile=wizard-large-100.bmp,wizard-large-150.bmp,wizard-large-200.bmp
WizardSmallImageFile=wizard-small-100.bmp,wizard-small-150.bmp,wizard-small-200.bmp
Compression=lzma2/ultra64
SolidCompression=yes
ArchiveExtraction=enhanced
CloseApplications=yes
RestartApplications=no
ShowLanguageDialog=no

[Languages]
Name: "en"; MessagesFile: "compiler:Default.isl"

[Messages]
WelcomeLabel2=This will install [name/ver] on your computer.%n%nThe VST3 and CLAP plug-ins, the standalone app and all measured location packs are installed in one go. If an older version is installed, it is updated in place - your projects and presets keep working.%n%nPlease close your DAW before continuing.
FinishedLabel=[name] is installed.%n%nRescan plug-ins in your DAW if it does not show up right away. The plug-in is listed under SubRoo.

[Types]
Name: "full"; Description: "Complete installation (recommended)"
Name: "custom"; Description: "Custom installation"; Flags: iscustom

[Components]
Name: "vst3"; Description: "VST3 plug-in"; Types: full custom
Name: "clap"; Description: "CLAP plug-in"; Types: full
Name: "standalone"; Description: "Standalone application"; Types: full
#ifdef PackSha256
Name: "packs"; Description: "Measured location packs ({#PackCount} rooms, ~{#PackDownloadMB} MB download, skipped if up to date)"; Types: full; ExtraDiskSpaceRequired: {#PackUnpackedSize}
#endif

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut for the standalone app"; Components: standalone; Flags: unchecked

[InstallDelete]
; clean plug-in bundle before copying the new one (no stale files from older versions)
Type: filesandordirs; Name: "{commoncf64}\VST3\{#AppName}.vst3"; Components: vst3
#ifdef PackSha256
Type: filesandordirs; Name: "{#PacksDir}"; Components: packs; Check: PacksNeedUpdate
#endif

[Files]
Source: "{#BuildDir}\VST3\{#AppName}.vst3\*"; DestDir: "{commoncf64}\VST3\{#AppName}.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#BuildDir}\CLAP\{#AppName}.clap"; DestDir: "{commoncf64}\CLAP"; Components: clap; Flags: ignoreversion
Source: "{#BuildDir}\Standalone\{#AppExe}"; DestDir: "{app}"; Components: standalone; Flags: ignoreversion
#ifdef PackSha256
Source: "{#PackUrl}"; DestName: "{#PackFile}"; DestDir: "{#PacksDir}"; ExternalSize: {#PackUnpackedSize}; Hash: "{#PackSha256}"; ExtractArchivePassword: "{#PackKey}"; Components: packs; Check: PacksNeedUpdate; Flags: external download extractarchive recursesubdirs createallsubdirs ignoreversion
#endif

[Icons]
Name: "{autoprograms}\{#AppName}"; Filename: "{app}\{#AppExe}"; Components: standalone
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExe}"; Components: standalone; Tasks: desktopicon

[Run]
Filename: "{app}\{#AppExe}"; Description: "Launch {#AppName}"; Components: standalone; Flags: nowait postinstall skipifsilent unchecked

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\{#AppName}.vst3"
Type: filesandordirs; Name: "{#PacksDir}"
Type: dirifempty; Name: "{commonappdata}\SubRoo Rooms"
Type: dirifempty; Name: "{app}"
Type: dirifempty; Name: "{autopf}\SubRoo"

[Code]
#ifdef PackSha256
var
  PacksChecked, PacksNeed: Boolean;

function PacksVersionFile: String;
begin
  Result := ExpandConstant('{#PacksDir}\.packs-version');
end;

function PacksNeedUpdate: Boolean;
var
  S: AnsiString;
begin
  if not PacksChecked then
  begin
    PacksNeed := True;
    if LoadStringFromFile(PacksVersionFile, S) then
      PacksNeed := Trim(String(S)) <> '{#PackVersion}';
    PacksChecked := True;
  end;
  Result := PacksNeed;
end;

{ Older builds told users to unzip packs into Documents\SubRoo Rooms\Packs.
  Those copies are now installed for all users, so remove the duplicates. }
procedure RemoveOldUserPackCopies;
var
  FindRec: TFindRec;
  UserPacks: String;
begin
  UserPacks := ExpandConstant('{userdocs}\SubRoo Rooms\Packs');
  if not DirExists(UserPacks) then Exit;
  if FindFirst(UserPacks + '\*', FindRec) then
  try
    repeat
      if (FindRec.Attributes and FILE_ATTRIBUTE_DIRECTORY <> 0) and (FindRec.Name <> '.') and (FindRec.Name <> '..') then
        if FileExists(ExpandConstant('{#PacksDir}\') + FindRec.Name + '\location.json') then
          DelTree(UserPacks + '\' + FindRec.Name, True, True, True);
    until not FindNext(FindRec);
  finally
    FindClose(FindRec);
  end;
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if (CurStep = ssPostInstall) and WizardIsComponentSelected('packs') then
  begin
    if PacksNeedUpdate then
      SaveStringToFile(PacksVersionFile, '{#PackVersion}', False);
    RemoveOldUserPackCopies;
  end;
end;
#endif
