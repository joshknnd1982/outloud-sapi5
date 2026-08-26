; Inno Setup script for the Outloud TTS SAPI5 voices.
; Installs the complete engine (all 17 languages, all voice data), the 32-bit
; and 64-bit SAPI5 interfaces, the engine host and the configuration utility.
; The wizard uses only standard pages, which are screen-reader accessible.

#define MyAppName "Outloud TTS SAPI5"
#define MyAppVersion "1.0.0"
#define MyAppPublisher "Outloud SAPI5 Project"
#ifndef SourceDir
  #define SourceDir "..\output"
#endif

[Setup]
AppId={{df927373-953d-4abb-812b-6495adf64570}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\OutloudSAPI
DefaultGroupName=Outloud TTS
DisableProgramGroupPage=yes
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
OutputBaseFilename=OutloudSAPI_Setup
OutputDir=.
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
SetupLogging=yes
UninstallDisplayIcon={app}\OutloudConfig.exe
UninstallDisplayName={#MyAppName}

[Files]
Source: "{#SourceDir}\OutloudSAPI.dll"; DestDir: "{app}"; Flags: ignoreversion regserver 32bit
Source: "{#SourceDir}\x64\OutloudSAPI.dll"; DestDir: "{app}\x64"; Flags: ignoreversion regserver 64bit; Check: Is64BitInstallMode
Source: "{#SourceDir}\outloud_host.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\OutloudConfig.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\engine\*"; DestDir: "{app}\engine"; Flags: ignoreversion recursesubdirs

[Icons]
Name: "{group}\Outloud TTS Configuration"; Filename: "{app}\OutloudConfig.exe"
Name: "{autodesktop}\Outloud TTS Configuration"; Filename: "{app}\OutloudConfig.exe"

[Run]
Filename: "{app}\OutloudConfig.exe"; Description: "Open the Outloud TTS configuration utility"; Flags: postinstall nowait skipifsilent

[UninstallRun]
Filename: "{app}\outloud_host.exe"; Parameters: "--shutdown"; RunOnceId: "StopOutloudHost"; Flags: runhidden waituntilterminated

[Code]
// Stop a running engine host so files can be replaced on upgrade.
function PrepareToInstall(var NeedsRestart: Boolean): String;
var
  HostPath: String;
  ResultCode: Integer;
begin
  Result := '';
  HostPath := ExpandConstant('{app}\outloud_host.exe');
  if FileExists(HostPath) then
    Exec(HostPath, '--shutdown', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
end;

// Preserve the setup log for debugging: copy it into the application folder.
procedure CurStepChanged(CurStep: TSetupStep);
var
  LogDir: String;
begin
  if CurStep = ssDone then
  begin
    LogDir := ExpandConstant('{app}\logs');
    if not DirExists(LogDir) then
      CreateDir(LogDir);
    CopyFile(ExpandConstant('{log}'), LogDir + '\install.log', False);
  end;
end;
