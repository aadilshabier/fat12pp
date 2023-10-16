from collections import OrderedDict

FILE = "intro.img"
# FILE = "/tmp/fat12/images/floppy.img"

SECTOR_SIZE = 512

def dump(d):
    for x, y in d.items():
        print(f"{x}: {y}")


def main():
    with open(FILE, "rb") as f:
        # BOOT SECTOR
        boot_sector = OrderedDict()
        boot_sector["ignore1"] = f.read(3)
        boot_sector["oem"] = f.read(8)
        boot_sector["bytes_per_sector"] = int.from_bytes(f.read(2))
        boot_sector["sectors_per_cluster"] = int.from_bytes(f.read(1))
        boot_sector["reserved_sectors"] = int.from_bytes(f.read(2))
        boot_sector["fats"] = int.from_bytes(f.read(1))
        boot_sector["root_dirs"] = int.from_bytes(f.read(2))
        boot_sector["sector_count"] = int.from_bytes(f.read(2))
        boot_sector["ignore2"] = f.read(1)
        boot_sector["sectors_per_fat"] = int.from_bytes(f.read(2))
        boot_sector["sectors_per_track"] = int.from_bytes(f.read(2))
        boot_sector["number_of_heads"] = int.from_bytes(f.read(2))
        boot_sector["hidden_sectors"] = f.read(4)
        boot_sector["total_sector_count_for_fat32"] = int.from_bytes(f.read(4))
        boot_sector["drive_number"] = int.from_bytes(f.read(1))
        boot_sector["ignore4"] = f.read(1)
        boot_sector["boot_signature"] = "0X" + format(int.from_bytes(f.read(1)), 'X')
        boot_sector["volume_id"] = int.from_bytes(f.read(4))
        boot_sector["volume_label"] = f.read(11)
        boot_sector["file_system_type"] = f.read(8)
        boot_sector["boot_code"] = f.read(448)
        boot_sector["bootable_partition_signature"] = "0X" + format(int.from_bytes(f.read(2)), 'X')

        # dump(boot_sector)

        # File Allocation Table
        fat = OrderedDict()
        fat_data = f.read(SECTOR_SIZE * boot_sector["fats"])
        print(fat_data)

        
if __name__ == "__main__":
    main()
