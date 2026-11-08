# XRClient

Reusable desktop calibration client for `stdsrc/android/XRServer`. The calibration dock
contains vertical `Label` plus `SliderCtrl` rows. It will own the reverse TCP
listener on port 8889 and send the same JSON protocol accepted by XRServer.

The GUI owns presentation only; calibration values are stored in the shared
`CalibrationState` model and persisted through `AppRegistry`. `examples/XRClientApp`
supplies the GUI entry point; `examples/XRClientCli` is its headless test twin.
