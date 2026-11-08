# XRServer - Android XR Server Package

## Overview
Java implementation of an Android XR Server for Google Cardboard and Chinese AR helmets. Based on GCVR/HRKit SDK C# code.

## Architecture
- `XRServer.java` - Main Activity, concurrent protocol server, and rotation-vector integration
- `XrPose.java` - Thread-safe Android rotation-vector pose snapshot
- `XrScene.java` - Validated immutable scene snapshots received over TCP
- `XrRenderer.java` - GLES2 split-eye renderer for cube and grid scene objects

## Protocol
Connect via TCP port 8888 using newline-delimited JSON. Every request requires
`protocol: 1`, `sequence`, and `type`; the server responds with an `ack` or an
explicit `error` carrying the same sequence. Supported types are `hello`,
`ping`, `snapshot`, and `diagnostics`. A snapshot replaces the complete scene
atomically and accepts `cube` and `grid` objects. The most recently accepted
snapshot stays rendered if the client disconnects.

## Reverse Client Discovery
XRServer also scans the active `192.168.x.0/24` Wi-Fi subnet for an XRClient
TCP listener on port 8889. It tries the last discovered client first on later
launches, then scans the subnet if that host is unavailable. The reverse socket
uses the same JSON protocol as inbound connections and lets a desktop client
work around a network that blocks inbound phone connections.

## Optical Pose Mapping
The AR helmet has the phone tilted 45 degrees down and reflects the display
through mirrors. `XrPose` preserves the original GcVR transform from
`src_GCVR/GcVRSdk/GcArMatrix.cs`: it remaps the sensor to display orientation,
then applies `Rx(135) * pose * Rz(180)`. The optical FOV is 45 degrees, as in
the original HRKit camera manager. A final basis conjugation maps GVR's
Android yaw/roll convention back to the rendered camera yaw/roll convention.
The default per-eye offset is 32 mm (64 mm total), matching HRKit's default
pupil distance.

## Network Settings Prompt
On its first start, XRServer explains that reverse XRClient access needs
unrestricted Wi-Fi and opens the Android app-details page on confirmation. The
OS owns restricted-settings/Data Saver consent; the app cannot grant it.

## ADB Network Policy Recovery
`android.permission.INTERNET` and `ACCESS_WIFI_STATE` are declared in
`AndroidManifest.xml` and are granted at install time. They do not appear in
the Android Settings "Permissions" screen because neither is a runtime
permission.

Some devices can assign a sideloaded ADB installation a UID netpolicy of
`REJECT_ALL`, which blocks Java TCP sockets despite `INTERNET` being granted.
This is device policy, not an AndroidManifest setting. Check the installed
package UID and its policy before testing reverse XRClient connectivity:

```sh
SERIAL=<adb-device-serial>
UID=$(adb -s "$SERIAL" shell dumpsys package org.upp.XRServer | grep userId= | cut -d= -f2 | tr -d '\r')
adb -s "$SERIAL" shell dumpsys netpolicy | grep -C 2 "$UID"
```

If the output contains `REJECT_ALL` or effective `RESTRICTED_MODE`, whitelist
the UID through the development ADB shell, then restart XRServer:

```sh
adb -s "$SERIAL" shell cmd netpolicy add restrict-background-whitelist "$UID"
adb -s "$SERIAL" shell cmd netpolicy add app-idle-whitelist "$UID"
adb -s "$SERIAL" shell am force-stop org.upp.XRServer
adb -s "$SERIAL" shell am start -n org.upp.XRServer/.XRServer
```

Recheck `dumpsys netpolicy`: the policy should no longer be `REJECT_ALL` and
the restricted firewall rule should be `restricted-allow`. Do not apply these
commands blindly on production devices; they modify device-wide policy.

## Default Behavior
On startup, renders a rotating cube until a client submits a snapshot.

## Build
Use U++ build system: `bin/build -m 2 android/XRServer`

## Dependencies
- Android SDK API 21+
- OpenGL ES 2.0
- Java 8
