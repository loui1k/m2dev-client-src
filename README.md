# Client Source Repository

This repository contains the source code necessary to compile the game client executable.

## How to build

> cmake -S . -B build
>
> cmake --build build

---

## ✨ Key Source Code Fixes

The following fixes address critical issues related to locale compilation and texture loading, ensuring compatibility with new content and correct language output.

### 🖼️ Texture and Resource Loading Fix

* **Fix: Missing Texture Cache:** Resolved an issue in `UserInterface/UserInterface.cpp` by adding the necessary missing resource directory names (`"_texcache"`) within the `PackInitialize` function.
    * **Impact:** This ensures that texture files for mobs and terrain for the new maps (Cape, Bay, Dawn, Thunder) are displayed.

### 🌐 Build Configuration Fix

* **Fix: Locale Build Typos:** Corrected a critical typo in `UserInterface/locale.cpp` where the `RelWithDebInfo` build configuration incorrectly referenced `tr (1253)`.
    * **Impact:** The value was updated to **`gr (1253)`** to correctly reference the Greek locale option before game launch.
