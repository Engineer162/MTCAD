[Setup]
AppName=MTCAD
AppVersion=0.4.0
DefaultDirName={autopf}\MTCAD
DefaultGroupName=MTCAD
OutputBaseFilename=MTCAD_Setup
Compression=lzma
SolidCompression=yes

ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible

[Installer]
SetupIconFile=resources\mtcad.ico

UninstallDisplayIcon={app}\resources\mtcad.ico


[Files]
Source: "..\release\mtcad.exe"; DestDir: "{app}"
Source: "..\release\mtkernel.dll"; DestDir: "{app}"
Source: "..\release\uv.dll"; DestDir: "{app}"

Source: "..\release\assets\*"; DestDir: "{app}\assets"; Flags: recursesubdirs

Source: "resources\mtcad.ico"; DestDir: "{app}\resources"; Flags: ignoreversion

; ONLY install if user doesn't already have one
Source: "resources\default_imgui.ini"; \
    DestDir: "{userappdata}\MTCAD"; \
    DestName: "imgui.ini"; \
    Flags: onlyifdoesntexist


[Icons]
Name: "{group}\MTCAD"; Filename: "{app}\mtcad.exe"; IconFilename: "{app}\resources\mtcad.ico"

Name: "{commondesktop}\MTCAD"; Filename: "{app}\mtcad.exe"; IconFilename: "{app}\resources\mtcad.ico"