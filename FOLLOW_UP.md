# Update: Ember - A Modern IDX Unpacker/Packer

Hey everyone! Following up on my previous guide about customizing Dragon's Prophet textures.

I've built **Ember** - a brand new, modern IDX/P00x unpacker **and** packer from scratch. It's open source, way easier to use than the old 2013 tool, and comes with a clean dark-themed UI.

## What's New

- **No more clunky menus** - just open the app, browse for your `.idx` file, and hit Extract. Done.
- **Packing support** - you can now **repack** your modified files back into IDX archives. No more manual folder structure headaches.
- **Auto compression** - when packing, Ember automatically picks the best compression for each file type (LZMA for data, raw for textures).
- **Live extraction progress** - watch files appear in real-time as they extract, with OK/ERR status per file.
- **All formats supported** - `.ros`, `.ras`, `.dds`, CRN textures, LZMA-compressed files - everything just works.
- **Single .exe, no install** - download and run. No dependencies, no setup.

## Download

Grab the latest release here: **https://github.com/yuhkix/Ember/releases**

## Updated Texture Modding Guide (Using Ember)

The process is now simpler:

1. **Download Ember** from the releases page and run it.
2. **Unpack** - In the **Unpack** tab, click Browse and select your `.idx` file (e.g., `character.idx` or `item.idx` from `Dragon's Prophet\pak\Files`).
3. **Choose output folder** - pick where to extract, or use the default.
4. **Hit Extract All** - watch the files extract with live progress. All textures, models, and data will be unpacked.
5. **Edit your textures** - open any `.dds` file in GIMP/Photoshop, make your changes, and save.
6. **Repack (optional)** - switch to the **Pack** tab, select your modified folder, choose an output `.idx` path, and hit Pack. Ember handles the compression automatically.
7. **Or use the old method** - you can still just drop modified textures into the game directory with the right folder structure (e.g., `Dragon's Prophet\character\dragon\mytheran\texture`). The game loads loose files over packed ones.

## Why Ember?

The old IDX Unpacker from 2013 still works, but:
- It's extract-only (can't repack)
- The UI is dated and clunky
- It doesn't show progress or errors clearly
- It requires you to navigate through File menus

Ember was reverse-engineered from the original tool's binary and DPLib.dll to ensure full format compatibility. The entire IDX/P00x format has been documented - check the [format spec](<https://github.com/yuhkix/Ember/blob/master/IDX_FORMAT_SPEC.md>) if you're curious about the internals.

---

The texture name list from my original post still applies - refer to that for mapping dragon skin names to texture filenames. Happy modding!
