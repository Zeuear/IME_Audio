[Setup]
AppName=ImeAudio (语音输入助手)
AppVersion=1.0.1
DefaultDirName={userpf}\IME Audio
DefaultGroupName=IME Audio
OutputDir=dist\Installer
OutputBaseFilename=ImeAudio_Setup
SetupIconFile=resources\app_icon.ico
UninstallDisplayIcon={app}\ImeAudio.exe
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=lowest

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
Name: "startup"; Description: "开机自动启动 (Run at Startup)"; GroupDescription: "系统选项:"

[Files]
Source: "app\Release\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\Voice IME"; Filename: "{app}\ImeAudio.exe"
Name: "{autodesktop}\Voice IME"; Filename: "{app}\ImeAudio.exe"; Tasks: desktopicon

[Registry]
Root: HKCU; Subkey: "SOFTWARE\Microsoft\Windows\CurrentVersion\Run"; ValueType: string; ValueName: "ImeAudio"; ValueData: """{app}\ImeAudio.exe"""; Tasks: startup

[Run]
Filename: "{app}\ImeAudio.exe"; Description: "{cm:LaunchProgram,Voice IME}"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
Type: filesandordirs; Name: "{app}"