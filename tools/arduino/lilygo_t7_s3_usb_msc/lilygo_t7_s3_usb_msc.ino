#include <Arduino.h>

#ifndef ARDUINO_USB_MODE
#error "This board/core does not support native USB MSC"
#elif ARDUINO_USB_MODE == 1
#error "Select Tools > USB Mode > USB-OTG (TinyUSB), not Hardware CDC and JTAG"
#endif

#include <USB.h>
#include <USBMSC.h>
#include <SPI.h>
#include <ff.h>
#include <sd_diskio.h>

namespace {

constexpr int kSdSck = 45;
constexpr int kSdMosi = 47;
constexpr int kSdMiso = 48;
constexpr int kSdCs = 46;

constexpr uint32_t kSpiFrequencyHz = 10 * 1000 * 1000;
constexpr uint32_t kSerialPromptMs = 5000;
constexpr size_t kSectorSize = 512;

enum class FormatMode {
  None,
  Auto,
  Fat32,
};

SPIClass storageSpi(FSPI);
USBMSC msc;
FATFS fatfs;

uint8_t storageDrive = 0xFF;
uint32_t sectorCount = 0;
char driveName[3] = {'0', ':', '\0'};

void halt(const char *message) {
  Serial.println(message);
  while (true) {
    delay(1000);
  }
}

FormatMode promptForFormat() {
  Serial.println();
  Serial.println("Type 'format' for auto FAT formatting, or 'fat32' to force FAT32, within 5 seconds.");

  String input;
  const uint32_t deadline = millis() + kSerialPromptMs;
  while (millis() < deadline) {
    while (Serial.available() > 0) {
      const char ch = static_cast<char>(Serial.read());
      if (ch == '\r' || ch == '\n') {
        if (input.equalsIgnoreCase("format")) {
          Serial.println("Auto format requested.");
          return FormatMode::Auto;
        }
        if (input.equalsIgnoreCase("fat32")) {
          Serial.println("FAT32 format requested.");
          return FormatMode::Fat32;
        }
        input = "";
      } else if (isPrintable(static_cast<unsigned char>(ch))) {
        input += ch;
      }
    }
    delay(10);
  }

  Serial.println("Continuing without formatting.");
  return FormatMode::None;
}

bool probeCard() {
  storageDrive = sdcard_init(kSdCs, &storageSpi, kSpiFrequencyHz);
  if (storageDrive == 0xFF) {
    return false;
  }

  driveName[0] = static_cast<char>('0' + storageDrive);

  FRESULT result = f_mount(&fatfs, driveName, 1);
  if (result != FR_OK) {
    Serial.printf("Initial mount returned %d. Raw export can still work.\n", result);
  }

  sectorCount = sdcard_num_sectors(storageDrive);
  Serial.printf("Drive %s sectors=%lu sector_size=%u\n", driveName, static_cast<unsigned long>(sectorCount), static_cast<unsigned>(kSectorSize));

  f_mount(nullptr, driveName, 0);
  return sectorCount > 0;
}

bool formatFatVolume(FormatMode mode) {
  Serial.println(mode == FormatMode::Fat32 ? "Formatting media as FAT32..." : "Formatting media...");

  BYTE *work = static_cast<BYTE *>(malloc(FF_MAX_SS));
  if (!work) {
    Serial.println("Failed to allocate mkfs work buffer.");
    return false;
  }

  BYTE formatType = FM_ANY;
  DWORD allocationUnit = 0;

#ifdef FM_FAT32
  if (mode == FormatMode::Fat32) {
    formatType = FM_FAT32;
    allocationUnit = 4096;
  }
#endif

  const MKFS_PARM options = {
    formatType,
    0,
    0,
    0,
    allocationUnit,
  };

  const FRESULT mkfsResult = f_mkfs(driveName, &options, work, FF_MAX_SS);
  free(work);
  if (mkfsResult != FR_OK) {
    Serial.printf("f_mkfs failed with %d\n", mkfsResult);
    return false;
  }

  FRESULT mountResult = f_mount(&fatfs, driveName, 1);
  if (mountResult != FR_OK) {
    Serial.printf("Verification mount failed with %d\n", mountResult);
    return false;
  }

  f_mount(nullptr, driveName, 0);
  Serial.println("Format complete.");
  return true;
}

int32_t onRead(uint32_t lba, uint32_t offset, void *buffer, uint32_t bufsize) {
  if ((offset % kSectorSize) != 0 || (bufsize % kSectorSize) != 0) {
    return -1;
  }

  auto *dst = static_cast<uint8_t *>(buffer);
  const uint32_t startSector = lba + (offset / kSectorSize);
  const uint32_t sectors = bufsize / kSectorSize;

  for (uint32_t i = 0; i < sectors; ++i) {
    if (!sd_read_raw(storageDrive, dst + (i * kSectorSize), startSector + i)) {
      return -1;
    }
  }

  return static_cast<int32_t>(bufsize);
}

int32_t onWrite(uint32_t lba, uint32_t offset, uint8_t *buffer, uint32_t bufsize) {
  if ((offset % kSectorSize) != 0 || (bufsize % kSectorSize) != 0) {
    return -1;
  }

  const uint32_t startSector = lba + (offset / kSectorSize);
  const uint32_t sectors = bufsize / kSectorSize;

  for (uint32_t i = 0; i < sectors; ++i) {
    if (!sd_write_raw(storageDrive, buffer + (i * kSectorSize), startSector + i)) {
      return -1;
    }
  }

  return static_cast<int32_t>(bufsize);
}

bool onStartStop(uint8_t powerCondition, bool start, bool loadEject) {
  Serial.printf("MSC start/stop power=%u start=%u eject=%u\n", powerCondition, start, loadEject);
  return true;
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("LILYGO T7-S3 USB MSC bridge");

  pinMode(kSdCs, OUTPUT);
  digitalWrite(kSdCs, HIGH);
  storageSpi.begin(kSdSck, kSdMiso, kSdMosi, kSdCs);

  if (!probeCard()) {
    halt("Failed to initialize storage.");
  }

  const FormatMode formatMode = promptForFormat();
  if (formatMode != FormatMode::None && !formatFatVolume(formatMode)) {
    halt("Format failed.");
  }

  msc.vendorID("LILYGO");
  msc.productID("T7S3-STOR");
  msc.productRevision("1.0");
  msc.onRead(onRead);
  msc.onWrite(onWrite);
  msc.onStartStop(onStartStop);
  msc.isWritable(true);
  msc.mediaPresent(true);

  if (!msc.begin(sectorCount, kSectorSize)) {
    halt("USB MSC init failed.");
  }

  if (!USB.begin()) {
    halt("USB begin failed.");
  }

  Serial.println("USB MSC ready.");
}

void loop() {
  delay(1000);
}
