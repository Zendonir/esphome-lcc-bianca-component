# esphome-lcc-bianca-component

## open_lcc_rp2040_updater

Updates the RP2040 firmware over Wi-Fi. It downloads `smart_lcc_app.bin` (by default from the newest release of
[Zendonir/open-lcc-rp2040-bianca](https://github.com/Zendonir/open-lcc-rp2040-bianca/releases)) into memory, checks
it, and only then reboots the RP2040 into the serial bootloader and flashes it. If anything fails, the RP2040 stays in
the bootloader (the machine does not heat) and the update can simply be started again.

```yaml
http_request:
  verify_ssl: true
  timeout: 20s
  buffer_size_tx: 2048   # GitHub redirects to long URLs
  buffer_size_rx: 2048

open_lcc_rp2040_updater:
  id: rp2040_updater
  # url: https://github.com/<owner>/<repo>/releases/latest/download/smart_lcc_app.bin
  status:
    name: RP2040 Update Status
```

Actions: `open_lcc_rp2040_updater.download`, `open_lcc_rp2040_updater.flash`.
Condition: `open_lcc_rp2040_updater.is_downloaded`.
See `esphome.yaml` in [open-lcc-esphome-bianca](https://github.com/Zendonir/open-lcc-esphome-bianca) for the complete
update script (download, reboot into the serial bootloader, flash).
