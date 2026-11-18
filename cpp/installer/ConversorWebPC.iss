; Instalador de Conversor WebP C++ (Inno Setup 6)
; Compilar con: ISCC.exe installer\ConversorWebPC.iss
; Genera dist\ConversorWebPC-2.9.0-setup.exe

#define MiApp "Conversor WebP C++"
#define MiVersion "2.9.0"
#define MiExe "ConversorWebPC.exe"

[Setup]
AppId={{7C6E3A2F-6B1D-4C7E-9E42-2D9F6A1B8C31}
AppName={#MiApp}
AppVersion={#MiVersion}
AppPublisher=Conversor WebP
DefaultDirName={autopf}\Conversor WebP C++
DefaultGroupName={#MiApp}
DisableProgramGroupPage=yes
OutputDir=..\dist
OutputBaseFilename=ConversorWebPC-{#MiVersion}-setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=lowest
UninstallDisplayIcon={app}\{#MiExe}
SetupIconFile=..\bin\icon.ico

[Languages]
Name: "spanish"; MessagesFile: "compiler:Languages\Spanish.isl"

[Tasks]
Name: "contextmenu"; Description: "Anadir 'Convertir a WebP' al menu contextual"; GroupDescription: "Integracion con Windows:"; Flags: unchecked
Name: "sendto"; Description: "Crear acceso en 'Enviar a'"; GroupDescription: "Integracion con Windows:"; Flags: unchecked
Name: "desktopicon"; Description: "Crear un acceso directo en el escritorio"; GroupDescription: "Accesos directos:"; Flags: unchecked

[Files]
Source: "..\bin\{#MiExe}"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\bin\MANUAL_DE_USO.html"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\bin\icon.ico"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\bin\7zr.exe"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "..\bin\LEEME.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\bin\ConversorWebPC.exe.manifest"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "..\README.md"; DestDir: "{app}"; DestName: "LEEME_C++.md"; Flags: ignoreversion isreadme
Source: "..\BENCHMARK.md"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist

[Icons]
Name: "{group}\{#MiApp}"; Filename: "{app}\{#MiExe}"
Name: "{group}\Manual de uso"; Filename: "{app}\MANUAL_DE_USO.html"
Name: "{group}\Desinstalar {#MiApp}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MiApp}"; Filename: "{app}\{#MiExe}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MiExe}"; Parameters: "--install-context"; Tasks: contextmenu; Flags: runhidden nowait
Filename: "{app}\{#MiExe}"; Parameters: "--install-sendto"; Tasks: sendto; Flags: runhidden nowait
Filename: "{app}\{#MiExe}"; Description: "Iniciar {#MiApp}"; Flags: nowait postinstall skipifsilent

[UninstallRun]
Filename: "{app}\{#MiExe}"; Parameters: "--uninstall-context"; RunOnceId: "QuitarContexto"; Flags: runhidden
Filename: "{app}\{#MiExe}"; Parameters: "--uninstall-sendto"; RunOnceId: "QuitarSendTo"; Flags: runhidden
