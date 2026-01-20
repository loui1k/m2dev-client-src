# Client Source Repository

This repository contains the source code necessary to compile the game client executable.

## How to build

> cmake -S . -B build
>
> cmake --build build

---

## 📋 Changelog

### 🐛 Bug Fixes
* **Effect adapting to semi-transparent meshes:** Effect adapting to semi-transparent meshes has been removed as it was causing artifacts when opacity was lower than 1
