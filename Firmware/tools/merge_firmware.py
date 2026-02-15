#!/usr/bin/env python3
import argparse
import subprocess
import sys

def merge_binaries(args):
    cmd = [
        "esptool.py",
        "--chip", "esp32s3",
        "merge_bin",
        "-o", args.output,
        "--flash_mode", "dio",
        "--flash_size", "keep",
        "--flash_freq", "80m",
        "0x0", args.bootloader,
        "0x8000", args.partition_table,
        "0xe000", args.ota,
        "0x10000", args.app,
        "0x390000", args.spiffs
    ]

    print(f"Running: {' '.join(cmd)}")
    try:
        subprocess.check_call(cmd)
        print(f"Successfully created {args.output}")
    except subprocess.CalledProcessError as e:
        print(f"Error merging binaries: {e}")
        sys.exit(1)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Merge ESP32 firmware binaries")
    parser.add_argument("--output", required=True, help="Output file path")
    parser.add_argument("--bootloader", required=True, help="Path to bootloader.bin")
    parser.add_argument("--partition-table", required=True, help="Path to partition-table.bin")
    parser.add_argument("--ota", required=True, help="Path to ota_data_initial.bin")
    parser.add_argument("--app", required=True, help="Path to application binary")
    parser.add_argument("--spiffs", required=True, help="Path to spiffs.bin")

    args = parser.parse_args()
    merge_binaries(args)
