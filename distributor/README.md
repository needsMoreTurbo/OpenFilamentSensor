# Centauri Firmware Distributor

Static Web Serial front-end that mirrors the Centauri dashboard aesthetic and lets end users flash an ESP32/S3 with the firmware you ship. Drop this folder onto any VM, serve it with Nginx (or the provided Dockerfile), and point it at the binaries for each board variant.

## Features

- Matches the gradients, typography, and card layout used in the dashboard UI.
- Board type dropdown is populated from `firmware/boards.json`.
- Uses [`esp-web-tools`](https://esphome.github.io/esp-web-tools/) to talk to the device over Web Serial with zero local tooling.
- Captures the installer console output and mirrors it into the “Live console” pane for auditing.
- Ships with container-ready assets and a minimal Dockerfile for quick hosting.

## Directory layout

```
distributor/
├── index.html            # UI shell + layout
├── styles.css            # Dashboard-inspired theme
├── app.js                # Board selection + log capture logic
├── favicon.ico
├── firmware/
│   ├── boards.json       # Describes each board entry
│   ├── esp32s3/
│   │   ├── manifest.json
│   │   └── *.bin         # Placeholder binaries – replace with your build artifacts
│   └── esp32/
│       ├── manifest.json
│       └── *.bin
└── Dockerfile            # Nginx based static container
```

### Adding or updating a board

1. Drop the binaries (`bootloader.bin`, `partitions.bin`, `firmware.bin`, or whichever layout you use) into `firmware/<board-id>/`.
2. Update `firmware/<board-id>/manifest.json` so the `parts` array points at those files and offsets.
3. Edit `firmware/boards.json` to add/update that board entry (human friendly name, variant label, release metadata, etc.).
4. (Optional) extend the release notes array so the UI can show what changed.

The JSON files are cached-every-request (`fetch(..., {cache: 'no-store'})`) so reloading the page after editing the files is enough.

## Local development

Any static file server works:

```bash
cd distributor
python -m http.server 8000
# visit http://localhost:8000 in Chrome/Edge to test Web Serial
```

## Docker usage

Build and run:

```bash
cd distributor
docker build -t centauri-distributor .
docker run --rm -p 8080:80 centauri-distributor
# Now hit http://localhost:8080
```

Push new firmware to the container by mounting in an updated `firmware/` directory or baking a new image with the changed files.

## Browser requirements

Web Serial only works in Chromium browsers on desktop and the page must be served from HTTPS (or `localhost`). The `<esp-web-install-button>` component surfaces a friendly warning for unsupported environments, and the log pane records those messages as well.
