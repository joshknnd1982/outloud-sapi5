; Inno Setup script for the Outloud TTS SAPI5 voices.
;
; The wizard lets you choose exactly which of the 17 languages and which of the
; 8 voice personalities are installed. Two files record that choice:
;
;   {app}\engine\eci.ini  - generated from eci.template.ini with the sections
;                           of unselected languages removed, so the engine only
;                           ever offers language data that is actually present.
;   {app}\voices.ini      - read by the SAPI5 DLLs and the configuration
;                           utility, so the Windows voice list and the utility
;                           show only the selected language/voice combinations.
;
; Everything uses standard wizard pages, which are screen-reader accessible.

#define MyAppName "Outloud TTS SAPI5"
#define MyAppVersion "1.1.0"
#define MyAppPublisher "Outloud SAPI5 Project"
#ifndef SourceDir
  #define SourceDir "..\output"
#endif

[Setup]
AppId={{df927373-953d-4abb-812b-6495adf64570}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
VersionInfoVersion={#MyAppVersion}
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

[Types]
Name: "full"; Description: "Full - all 17 languages and all 8 voices"
Name: "english"; Description: "English - American and British English, all 8 voices"
Name: "compact"; Description: "Compact - American English with Reed and Shelley"
Name: "custom"; Description: "Custom - choose languages and voices"; Flags: iscustom

[Components]
Name: "core"; Description: "Program files and shared engine (required)"; Types: full english compact custom; Flags: fixed
Name: "core\x64"; Description: "64-bit SAPI5 interface"; Types: full english compact custom; Check: Is64BitInstallMode

Name: "lang"; Description: "Languages"; Types: full english compact custom; Flags: fixed
Name: "lang\enu"; Description: "American English"; Types: full english compact
Name: "lang\eng"; Description: "British English"; Types: full english
Name: "lang\esp"; Description: "Castilian Spanish"; Types: full
Name: "lang\esm"; Description: "Latin American Spanish"; Types: full
Name: "lang\fra"; Description: "French"; Types: full
Name: "lang\frc"; Description: "Canadian French"; Types: full
Name: "lang\deu"; Description: "German"; Types: full
Name: "lang\ita"; Description: "Italian"; Types: full
Name: "lang\chs"; Description: "Mandarin Chinese"; Types: full
Name: "lang\ptb"; Description: "Brazilian Portuguese"; Types: full
Name: "lang\jpn"; Description: "Japanese"; Types: full
Name: "lang\fin"; Description: "Finnish"; Types: full
Name: "lang\kor"; Description: "Korean"; Types: full
Name: "lang\ctt"; Description: "Hong Kong Cantonese"; Types: full
Name: "lang\nor"; Description: "Norwegian"; Types: full
Name: "lang\swe"; Description: "Swedish"; Types: full
Name: "lang\dan"; Description: "Danish"; Types: full

Name: "voice"; Description: "Voices"; Types: full english compact custom; Flags: fixed
Name: "voice\v1"; Description: "Reed (adult male)"; Types: full english compact
Name: "voice\v2"; Description: "Shelley (adult female)"; Types: full english compact
Name: "voice\v3"; Description: "Sandy (child)"; Types: full english
Name: "voice\v4"; Description: "Rocko (adult male)"; Types: full english
Name: "voice\v5"; Description: "Glen (adult male)"; Types: full english
Name: "voice\v6"; Description: "FastFlo (adult female)"; Types: full english
Name: "voice\v7"; Description: "Grandma (senior female)"; Types: full english
Name: "voice\v8"; Description: "Grandpa (senior male)"; Types: full english

Name: "help"; Description: "Legacy IBM engine help files"; Types: full

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop shortcut for the configuration utility"; GroupDescription: "Additional shortcuts:"

[Files]
; --- program files ---
Source: "{#SourceDir}\OutloudSAPI.dll"; DestDir: "{app}"; Flags: ignoreversion regserver 32bit; Components: core
Source: "{#SourceDir}\x64\OutloudSAPI.dll"; DestDir: "{app}\x64"; Flags: ignoreversion regserver 64bit; Check: Is64BitInstallMode; Components: core\x64
Source: "{#SourceDir}\outloud_host.exe"; DestDir: "{app}"; Flags: ignoreversion; Components: core
Source: "{#SourceDir}\OutloudConfig.exe"; DestDir: "{app}"; Flags: ignoreversion; Components: core

; --- engine ---
; The full engine configuration travels with setup but is never installed
; as-is; WriteEngineIni below writes the filtered copy.
Source: "{#SourceDir}\eci.template.ini"; Flags: dontcopy
Source: "{#SourceDir}\install\core\*"; DestDir: "{app}\engine"; Flags: ignoreversion; Components: core

; --- language data ---
Source: "{#SourceDir}\install\lang\ENU\*"; DestDir: "{app}\engine"; Flags: ignoreversion; Components: lang\enu
Source: "{#SourceDir}\install\lang\ENG\*"; DestDir: "{app}\engine"; Flags: ignoreversion; Components: lang\eng
Source: "{#SourceDir}\install\lang\ESP\*"; DestDir: "{app}\engine"; Flags: ignoreversion; Components: lang\esp
Source: "{#SourceDir}\install\lang\ESM\*"; DestDir: "{app}\engine"; Flags: ignoreversion; Components: lang\esm
Source: "{#SourceDir}\install\lang\FRA\*"; DestDir: "{app}\engine"; Flags: ignoreversion; Components: lang\fra
Source: "{#SourceDir}\install\lang\FRC\*"; DestDir: "{app}\engine"; Flags: ignoreversion; Components: lang\frc
Source: "{#SourceDir}\install\lang\DEU\*"; DestDir: "{app}\engine"; Flags: ignoreversion; Components: lang\deu
Source: "{#SourceDir}\install\lang\ITA\*"; DestDir: "{app}\engine"; Flags: ignoreversion; Components: lang\ita
Source: "{#SourceDir}\install\lang\CHS\*"; DestDir: "{app}\engine"; Flags: ignoreversion; Components: lang\chs
Source: "{#SourceDir}\install\dicts\chs\Dicts\*"; DestDir: "{app}\engine\Dicts"; Flags: ignoreversion recursesubdirs createallsubdirs; Components: lang\chs
Source: "{#SourceDir}\install\lang\PTB\*"; DestDir: "{app}\engine"; Flags: ignoreversion; Components: lang\ptb
Source: "{#SourceDir}\install\lang\JPN\*"; DestDir: "{app}\engine"; Flags: ignoreversion; Components: lang\jpn
Source: "{#SourceDir}\install\lang\FIN\*"; DestDir: "{app}\engine"; Flags: ignoreversion; Components: lang\fin
Source: "{#SourceDir}\install\lang\KOR\*"; DestDir: "{app}\engine"; Flags: ignoreversion; Components: lang\kor
Source: "{#SourceDir}\install\lang\CTT\*"; DestDir: "{app}\engine"; Flags: ignoreversion; Components: lang\ctt
Source: "{#SourceDir}\install\lang\NOR\*"; DestDir: "{app}\engine"; Flags: ignoreversion; Components: lang\nor
Source: "{#SourceDir}\install\lang\SWE\*"; DestDir: "{app}\engine"; Flags: ignoreversion; Components: lang\swe
Source: "{#SourceDir}\install\lang\DAN\*"; DestDir: "{app}\engine"; Flags: ignoreversion; Components: lang\dan

; --- optional legacy documentation ---
Source: "{#SourceDir}\install\help\*"; DestDir: "{app}\engine"; Flags: ignoreversion; Components: help

[Icons]
Name: "{group}\Outloud TTS Configuration"; Filename: "{app}\OutloudConfig.exe"
Name: "{autodesktop}\Outloud TTS Configuration"; Filename: "{app}\OutloudConfig.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\OutloudConfig.exe"; Description: "Open the Outloud TTS configuration utility"; Flags: postinstall nowait skipifsilent

[UninstallRun]
Filename: "{app}\outloud_host.exe"; Parameters: "--shutdown"; RunOnceId: "StopOutloudHost"; Flags: runhidden waituntilterminated

[UninstallDelete]
Type: files; Name: "{app}\voices.ini"
Type: filesandordirs; Name: "{app}\engine"
Type: filesandordirs; Name: "{app}\logs"

[Code]
const
  LangCount = 17;
  VariantCount = 8;

var
  LangCode: array[0..LangCount - 1] of String;
  LangSection: array[0..LangCount - 1] of String;

// Language codes as the wrapper knows them, paired with their eci.ini section
// (the engine's dialect number, written major.minor).
procedure InitLanguageTable;
begin
  LangCode[0]  := 'enu'; LangSection[0]  := '1.0';
  LangCode[1]  := 'eng'; LangSection[1]  := '1.1';
  LangCode[2]  := 'esp'; LangSection[2]  := '2.0';
  LangCode[3]  := 'esm'; LangSection[3]  := '2.1';
  LangCode[4]  := 'fra'; LangSection[4]  := '3.0';
  LangCode[5]  := 'frc'; LangSection[5]  := '3.1';
  LangCode[6]  := 'deu'; LangSection[6]  := '4.0';
  LangCode[7]  := 'ita'; LangSection[7]  := '5.0';
  LangCode[8]  := 'chs'; LangSection[8]  := '6.0';
  LangCode[9]  := 'ptb'; LangSection[9]  := '7.0';
  LangCode[10] := 'jpn'; LangSection[10] := '8.0';
  LangCode[11] := 'fin'; LangSection[11] := '9.0';
  LangCode[12] := 'kor'; LangSection[12] := '10.0';
  LangCode[13] := 'ctt'; LangSection[13] := '11.1';
  LangCode[14] := 'nor'; LangSection[14] := '13.0';
  LangCode[15] := 'swe'; LangSection[15] := '14.0';
  LangCode[16] := 'dan'; LangSection[16] := '15.0';
end;

function InitializeSetup(): Boolean;
begin
  InitLanguageTable;
  Result := True;
end;

function IsLangSelected(Code: String): Boolean;
begin
  Result := WizardIsComponentSelected('lang\' + Code);
end;

function IsVariantSelected(Number: Integer): Boolean;
begin
  Result := WizardIsComponentSelected('voice\v' + IntToStr(Number));
end;

function SelectedLanguageCount(): Integer;
var
  i: Integer;
begin
  Result := 0;
  for i := 0 to LangCount - 1 do
    if IsLangSelected(LangCode[i]) then
      Result := Result + 1;
end;

function SelectedVariantCount(): Integer;
var
  i: Integer;
begin
  Result := 0;
  for i := 1 to VariantCount do
    if IsVariantSelected(i) then
      Result := Result + 1;
end;

// Nothing usable would be installed if either list were left empty, so ask for
// a correction instead of producing a silent, voiceless installation.
function NextButtonClick(CurPageID: Integer): Boolean;
begin
  Result := True;
  if CurPageID = wpSelectComponents then
  begin
    if SelectedLanguageCount = 0 then
    begin
      MsgBox('Please select at least one language.', mbError, MB_OK);
      Result := False;
    end
    else if SelectedVariantCount = 0 then
    begin
      MsgBox('Please select at least one voice.', mbError, MB_OK);
      Result := False;
    end;
  end;
end;

// Returns '' for the sections every installation needs.
function SectionLanguage(Section: String): String;
var
  i: Integer;
begin
  Result := '';
  for i := 0 to LangCount - 1 do
    if CompareText(Section, LangSection[i]) = 0 then
    begin
      Result := LangCode[i];
      exit;
    end;
  if CompareText(Section, 'Romanizers\6\0') = 0 then
    Result := 'chs'
  else if CompareText(Section, 'Romanizers\8\0') = 0 then
    Result := 'jpn'
  else if CompareText(Section, 'Romanizers\10\0') = 0 then
    Result := 'kor'
  else if CompareText(Section, 'Romanizers\11\1') = 0 then
    Result := 'ctt';
end;

// Copy eci.template.ini to {app}\engine\eci.ini, dropping the sections of the
// languages that were not selected, so the engine reports exactly the
// languages whose data files are on disk.
procedure WriteEngineIni;
var
  Src, Dst: TArrayOfString;
  i, Kept: Integer;
  Line, Trimmed, Section, Code: String;
  Keep: Boolean;
begin
  ExtractTemporaryFile('eci.template.ini');
  if not LoadStringsFromFile(ExpandConstant('{tmp}\eci.template.ini'), Src) then
    RaiseException('Unable to read the engine configuration template.');

  SetArrayLength(Dst, GetArrayLength(Src));
  Kept := 0;
  Keep := True;
  for i := 0 to GetArrayLength(Src) - 1 do
  begin
    Line := Src[i];
    Trimmed := Trim(Line);
    if (Length(Trimmed) > 2) and (Trimmed[1] = '[') and (Trimmed[Length(Trimmed)] = ']') then
    begin
      Section := Copy(Trimmed, 2, Length(Trimmed) - 2);
      Code := SectionLanguage(Section);
      Keep := (Code = '') or IsLangSelected(Code);
    end;
    if Keep then
    begin
      Dst[Kept] := Line;
      Kept := Kept + 1;
    end;
  end;
  SetArrayLength(Dst, Kept);

  if not SaveStringsToFile(ExpandConstant('{app}\engine\eci.ini'), Dst, False) then
    RaiseException('Unable to write the engine configuration.');
end;

// The manifest the SAPI5 DLLs and the configuration utility read to decide
// which voices exist. Only selected entries are listed.
procedure WriteVoiceManifest;
var
  Lines: TArrayOfString;
  i, n: Integer;
begin
  SetArrayLength(Lines, LangCount + VariantCount + 3);
  n := 0;
  Lines[n] := '; Written by Outloud TTS SAPI5 Setup - do not edit by hand.'; n := n + 1;
  Lines[n] := '[languages]'; n := n + 1;
  for i := 0 to LangCount - 1 do
    if IsLangSelected(LangCode[i]) then
    begin
      Lines[n] := LangCode[i] + '=1';
      n := n + 1;
    end;
  Lines[n] := '[variants]'; n := n + 1;
  for i := 1 to VariantCount do
    if IsVariantSelected(i) then
    begin
      Lines[n] := IntToStr(i) + '=1';
      n := n + 1;
    end;
  SetArrayLength(Lines, n);

  if not SaveStringsToFile(ExpandConstant('{app}\voices.ini'), Lines, False) then
    RaiseException('Unable to write the voice manifest.');
end;

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

procedure CurStepChanged(CurStep: TSetupStep);
var
  LogDir: String;
begin
  if CurStep = ssInstall then
  begin
    // A previous installation may hold languages that are not selected this
    // time. The engine directory is entirely ours, so start from empty and
    // let the selected components fill it again.
    DelTree(ExpandConstant('{app}\engine'), True, True, True);
    DeleteFile(ExpandConstant('{app}\voices.ini'));
  end;

  if CurStep = ssPostInstall then
  begin
    WriteEngineIni;
    WriteVoiceManifest;
  end;

  if CurStep = ssDone then
  begin
    // Preserve the setup log for debugging: copy it into the application folder.
    LogDir := ExpandConstant('{app}\logs');
    if not DirExists(LogDir) then
      CreateDir(LogDir);
    CopyFile(ExpandConstant('{log}'), LogDir + '\install.log', False);
  end;
end;
