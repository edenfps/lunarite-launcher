# Lunarite launcher

The game launcher for Lunarite, the community server for Huxley: The Dystopia. It keeps a player's
game files up to date from the server's patch server, shows the website's news page, and starts
the game.

## Build

Needs Visual Studio with the C++ desktop tools (x86).

```
build.cmd
```

Output: `out\WZLauncher.exe`.

## Make a player bundle

```powershell
.\Make-LauncherBundle.ps1 -Server <server address> -KoreanInstall <Korean Huxley folder> [-Port 8090] [-WebUrl https://<site>/launcher.html]
```

Builds `out\bundle` and `out\bundle.zip` for players: `WZLauncher.exe`, its encrypted config
`WZLauncher.wzc` (made from `bundle/WZLauncher.xml`), the launcher images, and the helper DLLs the
launcher loads (`AesCrypt.dll`, `ZlibAdapter.dll`, `RTPatchAdapter.dll`, `patchw32.dll`).

The helper DLLs and the stock `LauncherImage` folder are not in this repository; they are copied
from the Korean Huxley client's install folder (`-KoreanInstall`), the one with `WZLauncher.exe`
in it. `bundle/LauncherImage` replaces the frame image.

Players copy the bundle into their Huxley folder (the one with `Binaries`) and run
`WZLauncher.exe`.

Huxley: The Dystopia and its art and trademarks belong to their owners; this is a non-commercial
fan project.
