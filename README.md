# V Spoofer

Per-game device spoofer for rooted Android (Magisk / KernelSU / Zygisk).
Make selected games see a different phone model to unlock higher graphics and FPS options, while every other app keeps seeing your real device.

By **@Xverno** · Telegram: https://t.me/VernoTalks

## Features
- Spoofs only the games you pick (Zygisk engine)
- Simple WebUI: master switch, device picker, game list
- Presets for ROG Phone, OnePlus, Samsung, Pixel, Xiaomi and Red Magic, plus a Custom device
- Script engine as a fallback for setups without Zygisk
- Changes are not saved to the system, so a reboot returns everything to stock

## Requirements
- Magisk, KernelSU or a compatible root manager
- Zygisk enabled (Zygisk Next or Magisk's built-in Zygisk) for the Zygisk engine

## Install
1. Download the latest zip from the Releases page.
2. Flash it in your root manager and reboot.
3. Open the module's WebUI, turn on the switch, choose a device and tick your games.
4. Fully close the game, then open it again.

## Notes
- Games read the device name when they start, so always restart the game after changing settings.
- Only the model, brand, manufacturer, device and product name are changed. Hardware, chipset and board values stay real, and a game that checks them will still see your real device.
- A spoofed device may be against a game's rules. Use it at your own risk.

## License
MIT
