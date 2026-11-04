# Lunarite launcher

The game launcher for Lunarite, the community server for Huxley: The Dystopia. It keeps a player's
game files up to date from the server's patch server, shows the website's news page, and starts
the game.

## Install

Download from the [latest release](https://github.com/edenfps/lunarite-launcher/releases/latest):

- `LunariteLauncher-win-Setup.exe`: installs the launcher (default `C:\Games\Lunarite`) with
  shortcuts and an uninstaller.
- `LunariteLauncher-win-Portable.zip`: the same files; unzip them into an empty folder.

Then run `WZLauncher.exe` (or the Lunarite Launcher shortcut). The first start downloads the whole
game, about 22 GB, into the launcher's folder; no existing Huxley install is needed. Keep the folder
path short (under about 140 characters).

On a Korean Windows the launcher, its news page and the game are in Korean; elsewhere English. Set
the environment variable `LUNARITE_LANGUAGE` to `ko` or `en` to choose yourself.

The launcher starts the game DPI-aware (like the Compatibility tab's "Override high DPI scaling
behavior: Application"), so resolution changes work on scaled desktops. Game updates arrive by
themselves: when the server's files change, the next launch downloads only the changed files.

## Build

Needs Visual Studio with the C++ desktop tools (x86).

```
build.cmd
```

Output: `out\WZLauncher.exe`.

## Make a release

```powershell
.\Make-LauncherBundle.ps1 -Server <server address> -KoreanInstall <Korean Huxley folder> [-Port 8090] [-WebUrl https://<site>/launcher.html] [-Version 1.0.0]
```

Builds the launcher bundle (`out\bundle`) and the two release files,
`out\LunariteLauncher-win-Setup.exe` and `out\LunariteLauncher-win-Portable.zip`. The setup needs
[Inno Setup 6](https://jrsoftware.org/isinfo.php); its script is `installer/LunariteLauncher.iss`.

The bundle holds `WZLauncher.exe`, its encrypted config `WZLauncher.wzc` (made from
`bundle/WZLauncher.xml`), the launcher images, and the helper DLLs the launcher loads
(`AesCrypt.dll`, `ZlibAdapter.dll`, `RTPatchAdapter.dll`, `patchw32.dll`). The helper DLLs and the
stock `LauncherImage` folder are not in this repository; they are copied from the Korean Huxley
client's install folder (`-KoreanInstall`), the one with `WZLauncher.exe` in it.
`bundle/LauncherImage` replaces the frame image.

```powershell
gh release create v1.0.0 out\LunariteLauncher-win-Setup.exe out\LunariteLauncher-win-Portable.zip --title "Lunarite launcher 1.0.0"
```

The website links to `releases/latest/download/` for both files, so every new release is picked up
without changing the site.

## License

The launcher source is licensed under the GNU Affero General Public License v3.0 (`LICENSE`).
Huxley: The Dystopia and its art and trademarks belong to their owners; this is a non-commercial
fan project.
