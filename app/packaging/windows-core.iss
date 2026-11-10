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
DefaultDirName={localappdata}\Programs\CyCodeCore
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
UninstallDisplayIcon={app}\icons\CyCode.ico
CloseApplications=no
RestartApplications=no
DisableProgramGroupPage=yes
[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "french"; MessagesFile: "compiler:Languages\French.isl"
[Components]
Name: "core"; Description: "CyCodeCore"; Types: full compact custom; Flags: fixed
Name: "updater"; Description: "{cm:UpdaterComponent}"; Types: full
[Tasks]
Name: "updaterstartup"; Description: "{cm:UpdaterStartup}"; Components: updater; Flags: unchecked
[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Excludes: "apps\CyCodeUpdater\*"; Components: core; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#SourceDir}\apps\CyCodeUpdater\*"; DestDir: "{app}\apps\CyCodeUpdater"; Components: updater; Flags: ignoreversion recursesubdirs createallsubdirs
[Icons]
Name: "{group}\CyCodeUpdater"; Filename: "{app}\tools\python\pythonw.exe"; Parameters: "-B ""{app}\apps\CyCodeUpdater\main.py"" --language {cm:UpdaterLanguage}"; WorkingDir: "{app}"; IconFilename: "{app}\icons\CyCode.ico"; Components: updater
[Registry]
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueName: "CyCodeCoreUpdater"; Flags: deletevalue
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: string; ValueName: "CyCodeCoreUpdater"; ValueData: """{app}\tools\python\pythonw.exe"" -B ""{app}\apps\CyCodeUpdater\main.py"" --background --language {cm:UpdaterLanguage}"; Flags: uninsdeletevalue; Components: updater; Tasks: updaterstartup
[CustomMessages]
english.UpdaterComponent=CyCodeUpdater - GitHub updates
french.UpdaterComponent=CyCodeUpdater - mises à jour GitHub
english.UpdaterStartup=Check for updates when I sign in to Windows
french.UpdaterStartup=Vérifier les mises à jour à l'ouverture de Windows
english.UpdaterLanguage=en
french.UpdaterLanguage=fr
english.AgentProject=Connect an AI agent
french.AgentProject=Connecter un agent IA
english.AgentDescription=Choose an existing project for the local Codex MCP connection, or leave empty to install only the skill.
french.AgentDescription=Choisissez un projet existant pour la connexion MCP locale de Codex, ou laissez vide pour installer uniquement le skill.
english.AgentFolder=Existing project folder:
french.AgentFolder=Dossier du projet existant :
english.InvalidProject=Select an existing project folder or leave the field empty.
french.InvalidProject=Choisissez un dossier de projet existant ou laissez le champ vide.
english.RegistrationFailed=Files were installed, but agent registration failed. See INSTALL.md and register the project manually; existing settings were preserved.
french.RegistrationFailed=Les fichiers sont installés, mais la connexion de l'agent a échoué. Consultez INSTALL.md pour la configuration manuelle ; les réglages existants ont été conservés.
[Code]
var ProjectPage: TInputDirWizardPage;
procedure InitializeWizard();
begin
  ProjectPage := CreateInputDirPage(wpSelectDir, CustomMessage('AgentProject'),
    CustomMessage('AgentProject'), CustomMessage('AgentDescription'), False, '');
  ProjectPage.Add(CustomMessage('AgentFolder'));
  ProjectPage.Values[0] := '';
end;
function HasProject(): Boolean;
begin
  Result := ProjectPage.Values[0] <> '';
end;
function ProjectDirectory(Param: String): String;
begin
  Result := ProjectPage.Values[0];
end;
function NextButtonClick(CurPageID: Integer): Boolean;
begin
  Result := True;
  if (CurPageID = ProjectPage.ID) and HasProject() and not DirExists(ProjectPage.Values[0]) then begin
    MsgBox(CustomMessage('InvalidProject'), mbError, MB_OK);
    Result := False;
  end;
end;
procedure CurStepChanged(CurStep: TSetupStep);
var Code: Integer; Arguments: String; Succeeded: Boolean;
begin
  if CurStep = ssPostInstall then begin
    Arguments := '-B "' + ExpandConstant('{app}\app\scripts\install_core_agents.py') + '"';
    Succeeded := Exec(ExpandConstant('{app}\tools\python\python.exe'), Arguments + ' --skill-only', '',
      SW_HIDE, ewWaitUntilTerminated, Code);
    if not Succeeded or (Code <> 0) then begin
      Log('CyCodeCore skill registration failed.');
      MsgBox(CustomMessage('RegistrationFailed'), mbError, MB_OK);
      exit;
    end;
    if HasProject() then begin
      Succeeded := Exec(ExpandConstant('{app}\tools\python\python.exe'),
        Arguments + ' --workspace "' + ProjectPage.Values[0] + '" --access edit --enable', '',
        SW_HIDE, ewWaitUntilTerminated, Code);
      if not Succeeded or (Code <> 0) then begin
        Log('CyCodeCore MCP project registration failed.');
        MsgBox(CustomMessage('RegistrationFailed'), mbError, MB_OK);
      end;
    end;
  end;
end;
