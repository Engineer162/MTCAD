[Setup]
AppName=MTCAD
AppVersion=0.2.2
DefaultDirName={autopf}\MTCAD
DefaultGroupName=MTCAD
OutputBaseFilename=MTCAD_Setup
Compression=lzma
SolidCompression=yes

ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64

[Files]

Source: "..\release\mtcad_gui.exe"; DestDir: "{app}"
Source: "..\release\mtcad_kernel.dll"; DestDir: "{app}"
Source: "..\release\uv.dll"; DestDir: "{app}"

Source: "..\release\assets\*"; DestDir: "{app}\assets"; Flags: recursesubdirs

; ONLY install if user doesn't already have one
Source: "resources\default_imgui.ini"; \
    DestDir: "{userappdata}\MTCAD"; \
    DestName: "imgui.ini"; \
    Flags: onlyifdoesntexist

[Icons]

Name: "{group}\MTCAD"; Filename: "{app}\mtcad_gui.exe"

Name: "{commondesktop}\MTCAD"; Filename: "{app}\mtcad_gui.exe"