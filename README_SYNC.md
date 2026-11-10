Sync tflm_esp32 to DATN_Docs

This folder contains `sync_to_docs.sh` which copies the `tflm_esp32` tree to
`/home/namng/DATN_Docs/tflm_esp32_lib`, commits changes to a time-stamped
branch, and pushes to `git@github.com:nammannam/DATN_tflm_esp32_lib.git`.

Prerequisites
- git
- rsync
- (optional for watch mode) inotify-tools

One-time sync
```bash
cd /home/namng/Arduino/libraries/tflm_esp32
./sync_to_docs.sh --once
```

Watch mode (automatic)
```bash
sudo apt install inotify-tools
./sync_to_docs.sh --watch
```

To run as a background service, create a `systemd` unit (example below) and enable it.

Example systemd unit (save as `/etc/systemd/system/tflm-sync.service`):
```
[Unit]
Description=Auto sync tflm_esp32 to DATN_Docs
After=network.target

[Service]
Type=simple
User=namng
WorkingDirectory=/home/namng/Arduino/libraries/tflm_esp32
ExecStart=/home/namng/Arduino/libraries/tflm_esp32/sync_to_docs.sh --watch
Restart=on-failure

[Install]
WantedBy=multi-user.target
```

Then enable and start:
```bash
sudo systemctl daemon-reload
sudo systemctl enable --now tflm-sync.service
```
