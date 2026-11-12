#ifndef AppVersion
  #error AppVersion is required
#endif
#ifndef SourceDir
  #error SourceDir is required
#endif
#ifndef OutputDir
  #error OutputDir is required
#endif
[Setup]
AppId={{D5FE75B5-F802-4894-94C3-2FAF466183D7}
AppName=CyCodeCore
AppVersion={#AppVersion}
AppPublisher=Cyberalien
AppPublisherURL=https://github.com/MrMybal/CyCode
AppSupportURL=https://github.com/MrMybal/CyCode/issues
AppUpdatesURL=https://github.com/MrMybal/CyCode/releases
DefaultDirName={code:DefaultRoot}
UsePreviousAppDir=no
DefaultGroupName=CyCodeCore
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
OutputDir={#OutputDir}
OutputBaseFilename=CyCodeCore-{#AppVersion}-windows-x64-setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
LicenseFile={#SourceDir}\LICENSE
SetupIconFile={#SourceDir}\icons\CyCode.ico
UninstallDisplayIcon={code:PayloadDirectory}\icons\CyCode.ico
CloseApplications=no
RestartApplications=no
SetupMutex=Local\CyCodeCoreSetup
DisableProgramGroupPage=yes
[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "french"; MessagesFile: "compiler:Languages\French.isl"
[Components]
Name: "core"; Description: "CyCodeCore"; Types: full compact custom; Flags: fixed
Name: "manager"; Description: "{cm:ManagerShortcuts}"; Types: full compact
[Tasks]
Name: "updaterstartup"; Description: "{cm:CheckAtLogin}"; Components: manager; Flags: unchecked
[Files]
Source: "{#SourceDir}\*"; DestDir: "{code:PayloadDirectory}"; Components: core; Flags: ignoreversion recursesubdirs createallsubdirs
[Icons]
Name: "{group}\CyCodeCore Manager"; Filename: "{code:PayloadDirectory}\tools\python\pythonw.exe"; Parameters: "-B ""{code:PayloadDirectory}\apps\CyCodeManager\main.py"" --installation-root ""{app}"" --language {cm:ManagerLanguage}"; WorkingDir: "{app}"; IconFilename: "{code:PayloadDirectory}\icons\CyCode.ico"; Components: manager
[InstallDelete]
Type: files; Name: "{group}\CyCodeUpdater.lnk"
[UninstallDelete]
Type: files; Name: "{app}\current.json"
[Registry]
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueName: "CyCodeCoreUpdater"; Flags: deletevalue
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueName: "CyCodeCoreManager"; Flags: deletevalue
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: string; ValueName: "CyCodeCoreManager"; ValueData: """{code:PayloadDirectory}\tools\python\pythonw.exe"" -B ""{code:PayloadDirectory}\apps\CyCodeManager\main.py"" --installation-root ""{app}"" --background --language {cm:ManagerLanguage}"; Flags: uninsdeletevalue; Components: manager; Tasks: updaterstartup
[Run]
Filename: "{code:PayloadDirectory}\tools\python\pythonw.exe"; Parameters: "-B ""{code:PayloadDirectory}\apps\CyCodeManager\main.py"" --installation-root ""{app}"" --language {cm:ManagerLanguage}"; Description: "{cm:OpenManager}"; Flags: nowait postinstall skipifsilent; Components: manager; Check: ShouldLaunchManager
[CustomMessages]
english.ManagerShortcuts=CyCodeCore Manager shortcuts
french.ManagerShortcuts=Raccourcis CyCodeCore Manager
english.CheckAtLogin=Check for updates when I sign in to Windows
french.CheckAtLogin=Vérifier les mises à jour à l'ouverture de Windows
english.ManagerLanguage=en
french.ManagerLanguage=fr
english.OpenManager=Open CyCodeCore Manager (agents, skills and updates)
french.OpenManager=Ouvrir CyCodeCore Manager (agents, skills et mises à jour)
english.ActivationFailed=The new payload could not be verified or activated. The previously selected Core is preserved. See the installation log; do not remove older versions.
french.ActivationFailed=Le nouveau moteur n'a pas pu être vérifié ou activé. Le Core précédemment sélectionné est conservé. Consultez le journal d'installation ; ne supprimez pas les anciennes versions.
english.RedirectedRoot=Choose an ordinary local installation directory, without links or junctions.
french.RedirectedRoot=Choisissez un dossier local ordinaire, sans lien symbolique ni jonction.
[Code]
type TCoreGuid = record
  D1: LongWord;
  D2: Word;
  D3: Word;
  D4: array[0..7] of Byte;
end;
function CoCreateGuid(var Value: TCoreGuid): Integer;
  external 'CoCreateGuid@ole32.dll stdcall';
function CoreFileAttributes(Name: String): LongWord;
  external 'GetFileAttributesW@kernel32.dll stdcall';
var Generation: String;

function NormalizeRoot(Value: String): String;
var Parent: String;
begin
  Result := RemoveBackslashUnlessRoot(ExpandFileName(Value));
  Parent := ExtractFileDir(Result);
  if (CompareText(ExtractFileName(Parent), 'versions') = 0) and
     FileExists(AddBackslash(Result) + 'core-distribution.json') then
    Result := ExtractFileDir(Parent);
end;

function DefaultRoot(Param: String): String;
var Previous: String;
begin
  Result := ExpandConstant('{localappdata}\Programs\CyCodeCore');
  if RegQueryStringValue(HKCU, 'Software\Microsoft\Windows\CurrentVersion\Uninstall\{D5FE75B5-F802-4894-94C3-2FAF466183D7}_is1', 'InstallLocation', Previous) then
    Result := NormalizeRoot(Previous);
end;

function PayloadDirectory(Param: String): String;
begin
  Result := AddBackslash(ExpandConstant('{app}')) + 'versions\{#AppVersion}-' + Generation;
end;

function InitializeSetup(): Boolean;
var Id: TCoreGuid;
begin
  Result := CoCreateGuid(Id) = 0;
  if Result then
    Generation := Lowercase(Format('%.8x%.4x%.4x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x', [Id.D1,
      Id.D2, Id.D3, Id.D4[0], Id.D4[1], Id.D4[2], Id.D4[3], Id.D4[4], Id.D4[5], Id.D4[6], Id.D4[7]]));
end;

function NextButtonClick(CurPageID: Integer): Boolean;
begin
  Result := True;
  if CurPageID = wpSelectDir then
    WizardForm.DirEdit.Text := NormalizeRoot(WizardDirValue);
end;

procedure InitializeWizard();
begin
  { Normalize /DIR from old updaters too, even if the directory page is skipped. }
  WizardForm.DirEdit.Text := NormalizeRoot(WizardDirValue);
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
var Item, Parent: String; Attributes: LongWord;
begin
  Result := '';
  Item := NormalizeRoot(ExpandConstant('{app}'));
  if CompareText(Item, RemoveBackslashUnlessRoot(ExpandConstant('{app}'))) <> 0 then begin
    Result := CustomMessage('RedirectedRoot');
    exit;
  end;
  Item := PayloadDirectory('');
  if DirExists(Item) or FileExists(Item) then begin
    Result := 'The new Core generation already exists. Restart Setup.';
    exit;
  end;
  while Item <> '' do begin
    Attributes := CoreFileAttributes(Item);
    if (Attributes <> $FFFFFFFF) and ((Attributes and $400) <> 0) then begin
      Result := CustomMessage('RedirectedRoot');
      exit;
    end;
    Parent := ExtractFileDir(Item);
    if Parent = Item then break;
    Item := Parent;
  end;
end;

function ShouldLaunchManager(): Boolean;
begin
  Result := ExpandConstant('{param:MANAGERUPDATE|0}') <> '1';
end;

procedure CurStepChanged(CurStep: TSetupStep);
var Code: Integer; Payload, Arguments: String; Succeeded: Boolean;
begin
  if CurStep = ssPostInstall then begin
    Payload := PayloadDirectory('');
    Arguments := '-B "' + AddBackslash(Payload) + 'app\core_installation.py" --root "' +
      ExpandConstant('{app}') + '" --payload "' + Payload + '" --version {#AppVersion}';
    Succeeded := Exec(AddBackslash(Payload) + 'tools\python\python.exe', Arguments, '',
      SW_HIDE, ewWaitUntilTerminated, Code);
    if not Succeeded or (Code <> 0) then
      RaiseException(CustomMessage('ActivationFailed'));
  end;
end;
