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
AppId={{308D75A7-6921-47C0-A6A4-B21769A5487E}
AppName=CyCode
AppVersion={#AppVersion}
AppPublisher=Cyberalien
AppPublisherURL=https://github.com/MrMybal/CyCode
AppSupportURL=https://github.com/MrMybal/CyCode/issues
AppUpdatesURL=https://github.com/MrMybal/CyCode/releases
DefaultDirName={localappdata}\Programs\CyCode
DefaultGroupName=CyCode
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
OutputDir={#OutputDir}
OutputBaseFilename=CyCode-{#AppVersion}-windows-x64-setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
LicenseFile={#SourceDir}\LICENSE
SetupIconFile={#SourceDir}\icons\CyCode.ico
WizardSmallImageFile={#SourceDir}\icons\CyCode.png
UninstallDisplayIcon={app}\CyCode.exe
CloseApplications=no
RestartApplications=no
SetupLogging=yes
DisableProgramGroupPage=yes
[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "french"; MessagesFile: "compiler:Languages\French.isl"
[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
[Icons]
Name: "{autoprograms}\CyCode"; Filename: "{app}\CyCode.exe"
Name: "{autodesktop}\CyCode"; Filename: "{app}\CyCode.exe"; Tasks: desktopicon
[CustomMessages]
english.CloseInstances=Close all CyCode instances using this folder, then retry.
french.CloseInstances=Fermez toutes les instances de CyCode utilisant ce dossier, puis réessayez.
english.RecoveryRequired=An interrupted update requires recovery before installation or removal. Keep the existing installation and use its update recovery tools.
french.RecoveryRequired=Une mise à jour interrompue nécessite une récupération avant installation ou désinstallation. Conservez cette installation et utilisez ses outils de récupération.
english.DevelopmentDirectory=Choose a separate installation folder outside a Git checkout.
french.DevelopmentDirectory=Choisissez un dossier d'installation distinct, hors d'un dépôt Git.
[Code]
var InstallationHandle: THandle;
function OpenLease(Name: String; Access, Sharing, Security, Creation, Flags, Template: LongWord): THandle;
  external 'CreateFileW@kernel32.dll stdcall';
function CloseLease(Handle: THandle): Boolean;
  external 'CloseHandle@kernel32.dll stdcall';
function AcquireInstallation(): String;
begin
  Result := '';
  if (InstallationHandle <> 0) and (InstallationHandle <> -1) then exit;
  if DirExists(ExpandConstant('{app}\.git')) or FileExists(ExpandConstant('{app}\.git')) then begin
    Result := CustomMessage('DevelopmentDirectory'); exit;
  end;
  if FileExists(ExpandConstant('{app}\data\updates\pending.json')) then begin
    Result := CustomMessage('RecoveryRequired'); exit;
  end;
  if not ForceDirectories(ExpandConstant('{app}\data\updates')) then begin
    Result := CustomMessage('CloseInstances'); exit;
  end;
  { Hold the same installation ownership file exclusively through Setup/uninstall.
    CyCode's native shared leases cannot open it until this handle is closed. }
  InstallationHandle := OpenLease(ExpandConstant('{app}\data\updates\instances.lock'), $C0000000, 0, 0, 4, $00200080, 0);
  if InstallationHandle = -1 then Result := CustomMessage('CloseInstances');
end;
procedure ReleaseInstallation();
begin
  if (InstallationHandle <> 0) and (InstallationHandle <> -1) then CloseLease(InstallationHandle);
  InstallationHandle := 0;
end;
function PrepareToInstall(var NeedsRestart: Boolean): String;
begin
  Result := AcquireInstallation();
end;
procedure DeinitializeSetup();
begin
  ReleaseInstallation();
end;
function InitializeUninstall(): Boolean;
var Reason: String;
begin
  Reason := AcquireInstallation(); Result := Reason = '';
  if not Result then MsgBox(Reason, mbError, MB_OK);
end;
procedure DeinitializeUninstall();
begin
  ReleaseInstallation();
end;
