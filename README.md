# UppXR

Cross-Platform Spatial Computing, Head-Mounted Display (HMD) Runtimes, Software 3D Rasterization, Rigid-Body Physics, and Networked XR Streaming for the U++ Ecosystem.

Part of the [OuluBSD](https://github.com/OuluBSD) project.

---

## Overview

`UppXR` provides a fully decoupled, hardware-adaptable XR (Virtual, Augmented, and Mixed Reality) engine designed to run seamlessly from embedded/headless Linux devices to high-end desktop workstations and Android XR headsets:

| Component | Capabilities |
|---|---|
| **SoftVR** | Spatial tracking interfaces, 6DoF head pose estimation, stereoscopic viewport projection, controller input abstraction. |
| **SoftHMD** | Multi-vendor HMD driver subsystem (Oculus Rift DK1/DK2/CV1, HTC Vive, PSVR, WMR, Deepoon, NOLO VR) with fallback stubs for headless CI. |
| **SoftRend** | High-performance software 3D rasterizer with multi-threaded scanline rendering, Z-buffering, perspective-correct texturing, and `SoftFramebuffer`. |
| **SoftPhys** | 3D rigid-body dynamics, collision detection, and constraint solver built on ODE abstractions. |
| **SoftGraphics** | Software graphics primitives, lighting models, and raster drawing pipeline. |
| **VirtualGui3D** | Interactive 3D spatial user interfaces, raycast pointer interactions, and 2D-in-3D GUI control projection. |
| **XRClient & XRServer** | Networked XR protocol and streaming client/server architecture for ultra-low-latency remote rendering and input sync (includes Android XRServer). |
| **ComputerVision** | Feature detection (ORB), optical tracking, camera calibration, and visual-inertial odometry helpers. |
| **Draw/Camera & Draw/Video** | Camera capture abstractions and `libv4l2` hardware plugin for live optical sensor feeds. |

---

## Building & Verification

`UppXR` uses [UppBuilder](https://github.com/OuluBSD/UppBuilder) along with [BazaarCore](https://github.com/OuluBSD/BazaarCore) and [UppGeometry](https://github.com/OuluBSD/UppGeometry):

```bash
# Clone dependencies
git clone https://github.com/OuluBSD/UppBootstrap.git
git clone https://github.com/OuluBSD/UppBuilder.git
git clone https://github.com/OuluBSD/BazaarCore.git
git clone https://github.com/OuluBSD/UppGeometry.git
git clone --depth 1 https://github.com/ultimatepp/ultimatepp.git

# Bootstrap build tools
(cd UppBootstrap && ./bootstrap.sh --upp-root "$PWD/../ultimatepp")
(cd UppBuilder && ./bootstrap.sh)

# Materialize patched upstream U++ packages
cd BazaarCore
../UppBuilder/uppbuilder --apply-patches --upstream "$PWD/../ultimatepp/uppsrc" --applied-patches "$PWD/patched-uppsrc" -v

# Build and execute test suite
cd ../UppXR
../UppBuilder/uppbuilder tests/XRCheck --bazaarcore "$PWD/../BazaarCore" --uppgeometry "$PWD/../UppGeometry" -v
./bin/XRCheck
```

---

## Continuous Integration

`UppXR` is continuously tested on every commit and pull request across four compiler and operating system targets:
- **Ubuntu Clang** (`ubuntu-latest`, `clang++`)
- **Ubuntu GCC** (`ubuntu-latest`, `g++`)
- **macOS Clang** (`macos-latest`, Apple Silicon `clang++`)
- **Windows MSVC** (`windows-latest`, Visual Studio 2022 `cl.exe`)

---

## License

This repository is distributed under the BSD-2-Clause / Ultimate++ License. See [LICENSE](LICENSE) for details.
