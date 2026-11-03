# GW2 Radial

[![GitHub all releases](https://img.shields.io/github/downloads/Friendly0Fire/GW2Radial/total)](https://github.com/Friendly0Fire/GW2Radial/releases/latest) [![GitHub Workflow Status](https://img.shields.io/github/workflow/status/Friendly0Fire/GW2Radial/CI)](https://github.com/Friendly0Fire/GW2Radial/actions/workflows/main.yml) [![Discord](https://img.shields.io/discord/384735285197537290?label=Discord)](https://discord.gg/zqeHCEg)


An [*ArenaNET-approved<sup>TM</sup>*](https://www.reddit.com/r/Guildwars2/comments/746mar/mount_radial_menu_addon_very_alpha_much_untested/dnwqj9x/) addon to show a convenient, customizable radial menu overlay to select a mount, novelty item and more, on the fly, for *Guild Wars 2*.

[![GW2Radial Demo](https://thumbs.gfycat.com/IgnorantIllfatedCrocodileskink-size_restricted.gif)](https://gfycat.com/ignorantillfatedcrocodileskink)
(click to see a better video)

**Highlights include:**
* Predefined menus for mounts, novelties and markers
* Simple system to add custom radial menus and share them
* Conditional menus, making it possible to have multiple menus on the same key depending on the situation
* Smart automount: Skimmer is auto-selected when underwater or Warclaw when in WvW
* Input queuing: combat (and underwater without the Skimmer mastery) prevents mount usage, so the selected mount is "queued" until out of combat:

[![Input Queuing GW2Radial Demo](https://thumbs.gfycat.com/NegativeBlushingHake-size_restricted.gif)](https://gfycat.com/negativeblushinghake)
(click to see a better video)

## Installation

This is the Nexus build of GW2Radial. It runs on [Nexus](https://github.com/RaidcoreGG/Nexus) and needs neither the addon loader nor the d3d wrapper.

- Install [Nexus](https://raidcore.gg/Nexus).
- Remove the old build if you had one: ``addons\gw2radial\gw2addon_gw2radial.dll`` (leave the rest of that folder in place).
- Copy ``GW2Radial.dll`` into your game's ``addons`` folder (with the default install path, ``C:\Program Files\Guild Wars 2\addons``).
- Run the game, open the Nexus library and make sure GW2Radial is enabled. It can be loaded and unloaded while the game is running.

Your settings, keybinds and custom wheels live in ``addons\gw2radial`` (``config.ini``) and are picked up as they were, so you do not need to reconfigure anything.

The settings window opens with Shift+Alt+M, from the *Open GW2Radial settings* button in the addon's Nexus options, or from a key bound to *GW2Radial settings* in Nexus' keybind menu.

#### Linux Support
GW2Radial is **not** developed to support Linux, but with a simple tweak you can get radial working.
- Using `protontricks`, add the following `dll` overrides:
  - `d3dcompiler_43`, `d3dcompiler_47`, `d3d11_43`

<details>
  <summary>Screenshot showing which dlls to install</summary>
  <img height="400px" src="https://user-images.githubusercontent.com/5796109/229198400-49fe3322-d160-4d65-b59d-ee4025ab03ff.png" />
</details>

## Usage
- When in game, press ``Shift+Alt+M`` for the options menu.
- Set a keybind to use the overlay. This is the only keybind you will use in practice.
- Set each game keybind for the mounts (you must set these first in your game options, under controls, after you've unlocked the mounts). These are used by the plugin to trigger mounting. They should be matching up like so: ![Setting your keybinds](https://i.imgur.com/gvQPQfX.png)
- Hold the main key to show the overlay, move the mouse in the desired direction, then release the key to trigger mounting/dismounting.

## Custom Radial Menus [New!]
Please refer to the readme in the `custom_examples` folder for more information on how to create or download new menus.

## Credits
- @QuitarHero and @TanukiSoup for extensive testing
- The Xunlai Addons Discord server members for their continued support
- Cerulean Dream for providing me with the initial motivator and inspiration in making his AutoHotkey-based radial menu
- Ghost for the new mount art found in v0.3+
- Skiff and fishing icons by John Mausson
- [freepik](https://www.freepik.com/) for novelty art
- deltaconnected and Bhagawan for their amazing addons which helped frame this and direct the approach to take
- WoodenPotatoes for the great feedback and exposure, as well as the Spud Club for helping with testing
- /u/that_shaman for another really nice radial menu concept which requires far better Photoshop skills than I have to reproduce

## FAQ

### Q: I want to thank you in some way, how do I do that?

A: I do this for fun/because I wanted this to exist. If you really want to, feel free to throw me a few dollars on [Ko-fi](https://ko-fi.com/friendlyfire) or send me mail in game to my account ``FriendlyFire.6275`` and toss a bit of gold or mats my way!

### Q: The addon won't load or crashes.

A: There can be a lot of reasons for this. Try the following debugging steps:

1. Look in ``<Guild Wars 2>\addons\gw2radial`` for a file named ``gw2radial.log``.
2. If it exists, look inside the log file for details on what might be going wrong. Also check Nexus' own log window and ``Nexus.log`` for a "GW2Radial" entry.
3. If it does not exist, the addon was not loaded: make sure ``GW2Radial.dll`` is directly in the ``addons`` folder and enabled in the Nexus library.

If all else fails, please open an issue or [reach out on Discord](https://discord.gg/NrnW29fVpn).

### Q: What is the "show in center" keybind used for?

A: It's a convenience feature for Action Camera users. When using Action Camera, the cursor is hidden and replaced with a targeting reticule fixed in the middle of the screen. Since that means the radial menu would appear all over the place, that keybind can be used to make it show up in the middle of the screen instead. As a bonus, it'll recenter the mouse to the middle of the radial menu so it's as easy as possible to select a mount *and* it'll show a temporary cursor so you know exactly where you're pointing.

Unfortunately, I can't make this automatic (i.e. switching to that mode when Action Camera is enabled) without hooking game functions, which would then require this become closed source again to avoid cheaters using it. On top of that, I'd be far more likely to break the EULA, so I'm afraid it's off limits without an official API.

### Q: Can you make it so selecting a mount while already mounted will directly swap to the new mount?

No. I talked with ArenaNet devs about this and they've decided that the can of worms it could potentially open is not worth it. Unfortunately, you'll need to separately unmount then select the new mount you want to use. This will not change unless ArenaNet's policy on addons changes.

### Q: Can I use this code for my project?

A: Absolutely! This whole repository is MIT licensed, so everything here is up for grabs. I would of course appreciate a small note or crediting if you do end up using something, but none of that is required.

### Q: I'm getting a bug/crash/issue that's not mentioned here, what do I do?

A: Please make an issue in the Github page and label it as ``bug`` or ``question`` as relevant to let me know, I'll do my best to help! You can also [reach out on Discord](https://discord.gg/NrnW29fVpn).

### Q: I have an idea for a new feature/new addon!

A: Feel free to make an issue in the Github page and label it as ``suggestion``! You can also [reach out on Discord](https://discord.gg/n2fgSAXkEc).
