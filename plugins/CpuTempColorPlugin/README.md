# CPU Temperature Color Plugin

Changes all OpenRGB devices to a solid color based on CPU temperature: blue when cool, yellow when warm, and red when hot. Color is uniform across LEDs (no per-LED gradients).

## Building

```bash
cd plugins/CpuTempColorPlugin
mkdir -p build && cd build
qmake ../CpuTempColorPlugin.pro
make -j
```

Copy the resulting plugin (`libCpuTempColorPlugin.so`, `CpuTempColorPlugin.dll`, etc.) into your OpenRGB configuration `plugins` directory (for most Linux users: `~/.config/OpenRGB/plugins/`).
