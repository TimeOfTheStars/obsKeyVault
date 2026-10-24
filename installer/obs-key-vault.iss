; Inno Setup script for OBS Key Vault.
; Built on CI, see .github/workflows/build-project.yaml.
;
; Required defines (passed via ISCC /D...):
;   MyAppVersion        - plugin version from buildspec.json
;   SourceDir           - install tree of the plugin (contains bin\ and data\)
;   OutputDir           - where to put the installer
;   OutputBaseFilename  - installer file name without extension

#ifndef MyAppVersion
  #define MyAppVersion "0.0.0"
#endif
#ifndef SourceDir
  #error SourceDir must be defined
#endif
#ifndef OutputDir
  #define OutputDir "."
#endif
#ifndef OutputBaseFilename
  #define OutputBaseFilename "obs-key-vault-installer"
#endif

[Setup]
AppId={{8C2B6E0A-6F1D-4C5B-9C7E-3A1F2D4B5E60}
AppName=OBS Key Vault
AppVersion={#MyAppVersion}
AppPublisher=Alexey
AppPublisherURL=https://github.com/TimeOfTheStars/obsKeyVault
; Standard plugin location for OBS 28+
DefaultDirName={commonappdata}\obs-studio\plugins\obs-key-vault
DisableDirPage=yes
DisableProgramGroupPage=yes
PrivilegesRequired=admin
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
OutputDir={#OutputDir}
OutputBaseFilename={#OutputBaseFilename}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName=OBS Key Vault
CloseApplications=yes

[Languages]
Name: "russian"; MessagesFile: "compiler:Languages\Russian.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
Source: "{#SourceDir}\*"; Excludes: "*.pdb"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Messages]
russian.FinishedLabel=Плагин установлен. Перезапустите OBS и откройте панель через меню «Док-панели» → «Key Vault».
english.FinishedLabel=The plugin is installed. Restart OBS and open the panel via Docks → Key Vault.
