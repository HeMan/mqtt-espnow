# Development environment

The development container pins ESP-IDF 5.5.3 by image digest, uv 0.7.13,
PlatformIO 6.1.18 through `uv.lock`, and PlatformIO ESP32 6.13.0 with its
ESP-IDF, compiler, and build-tool packages in `platformio.ini`. Open the
repository in VS Code and choose **Reopen in Container** to create the
environment.

Build from the container terminal:

```sh
uv run --locked pio run
```

To flash the connected board or open the serial monitor:

```sh
uv run --locked pio run --target upload
uv run --locked pio device monitor
```

USB serial devices are not automatically passed through to the container.
Configure device access in your container runtime if you need to flash or
monitor from inside it.
