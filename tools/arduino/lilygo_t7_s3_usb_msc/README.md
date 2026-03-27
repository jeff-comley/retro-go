# LilyGO T7-S3 USB MSC Utility

This Arduino sketch exposes the T7-S3's SPI storage over native USB Mass Storage so you can copy files from a desktop OS.

Pin mapping in the sketch matches the current Retro-Go `lilygo-t7-s3` target:

- `SCK = GPIO45`
- `MOSI = GPIO47`
- `MISO = GPIO48`
- `CS = GPIO46`

Behavior:

- On boot, the sketch waits 5 seconds on serial.
- If you type `format` and press Enter, it runs `f_mkfs` with FatFs auto-selection before exposing the drive.
- If you type `fat32` and press Enter, it forces a FAT32 format with a small allocation unit suitable for sub-1 GB media.
- Otherwise it exports the storage as-is.

Notes:

- Formatting is destructive.
- The sketch does not keep the volume mounted locally after setup, which avoids fighting the host computer over the FAT filesystem.
- If the media is not actually SD protocol compatible, this sketch will not work without a different low-level driver.

Typical use:

1. Open serial at `115200`.
2. Reset the board.
3. Type `format` during the prompt window if you want a fresh FAT volume.
4. Let the host mount the new USB drive.
