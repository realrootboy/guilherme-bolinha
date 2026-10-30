#define AppVersion GetEnv('GB_VERSION')
[Setup]
AppId=com.guilhermebolinha.vst
AppName=Guilherme Bolinha VST
AppVersion={#AppVersion}
DefaultDirName={autopf}\Guilherme Bolinha
OutputDir=..\dist
OutputBaseFilename=Guilherme-Bolinha-Windows
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
Compression=lzma2
SolidCompression=yes
UninstallDisplayIcon={app}\Guilherme Bolinha.exe
[Files]
Source: "..\build\GuilhermeBolinha_artefacts\Release\VST3\Guilherme Bolinha.vst3\*"; DestDir: "{commoncf64}\VST3\Guilherme Bolinha.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\build\GuilhermeBolinha_artefacts\Release\Standalone\Guilherme Bolinha.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\README.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\assets\logo.png"; DestDir: "{app}\assets"; Flags: ignoreversion
Source: "..\Resources\Cure\*"; DestDir: "{commonappdata}\Guilherme Bolinha\Cure"; Flags: ignoreversion recursesubdirs createallsubdirs
[Icons]
Name: "{group}\Guilherme Bolinha"; Filename: "{app}\Guilherme Bolinha.exe"
