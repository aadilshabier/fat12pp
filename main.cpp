#include <cstdint>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

using namespace std;

const char* FILE_NAME = "intro.img";

constexpr int SECTOR_SIZE = 512;

struct BootSector {
	// BPB
	char ignore1[3];
	char oem[8];
	uint16_t bytes_per_sector;
	uint8_t sectors_per_cluster;
	uint16_t reserved_sectors;
	uint8_t fats;
	uint16_t max_root_dirs;
	uint16_t sector_count;
    char ignore2[1];
	uint16_t sectors_per_fat;
	uint16_t sectors_per_track;
	uint16_t number_of_heads;
	uint32_t hidden_sectors;
	uint32_t total_sector_count_for_fat32;

	// FAT 12 Extended
	uint8_t drive_number;
	char ignore4[1];
	uint8_t boot_signature;
	uint32_t volume_id;
	char volume_label[11];
	char file_system_type[8];
	char boot_code[448];
	uint16_t bootable_partition_signature;
}__attribute__((packed));

struct DirectoryEntry {
	char file_name[8];
	char file_ext[3];

	// File attributes
	uint8_t read_only : 1;
	uint8_t hidden : 1;
	uint8_t system : 1;
	uint8_t volume_id : 1;
	uint8_t subdirectory : 1;
	uint8_t archive : 1;
	uint8_t device : 1;
	uint8_t unused : 1;

	uint8_t reserved;

	uint8_t create_time_high_res; // 10ms units, from 1-199
	uint16_t create_time;
	uint16_t create_date;
	uint16_t last_access_date;

	uint16_t ignore1;

	uint16_t last_write_time;
	uint16_t last_write_date;

	uint16_t first_cluster;
	uint32_t file_size; // in bytes

	bool has_long_name() {
		return (read_only | hidden | system | volume_id);
	}

	string_view get_file_name() {
		return string_view(file_name, sizeof(file_name));
	}

	string_view get_file_ext() {
		return string_view(file_ext, sizeof(file_ext));
	}

}__attribute__((packed));

class Floppy {
public:
    bool read(ifstream& img) {
		/* READ BOOT SECTOR */

		img.read(reinterpret_cast<char*>(&boot), sizeof(boot));
		if (unsigned sig = boot.boot_signature; sig != 0x28 and sig != 0x29) {
			cerr << "ERROR: Signature is not valid: " << sig << endl;
			return false;
		}

		// skip reserved sectors 
		img.seekg((boot.reserved_sectors-1)*SECTOR_SIZE, std::ios_base::cur);


		/* READ FAT */

	    auto fat_table_size = boot.sectors_per_fat*SECTOR_SIZE;

		fat_data.resize(fat_table_size);
		img.read(reinterpret_cast<char*>(fat_data.data()), fat_data.size());

		// check if redundant FAT copies match the first copy
		{
			vector<uint8_t> fat_data_copy(fat_data.size());
			for (int i=0; i<boot.fats-1; i++) {
				img.read(reinterpret_cast<char*>(fat_data_copy.data()), fat_data.size());
				if (fat_data != fat_data_copy) {
					cerr << "FAT copy " << i+1 << " does not match the original." << endl;
					return false;
				}
			}
		}


		/* READ ROOT DIRECTORIES */
		root_dirs.resize(boot.max_root_dirs);
		auto root_dirs_size = sizeof(DirectoryEntry)*root_dirs.size();
		img.read(reinterpret_cast<char*>(root_dirs.data()), root_dirs_size);

		auto *dir = &root_dirs[0];
		// Print volume nam
		if (dir->volume_id) {
			cout << "VolumeName: " << dir->get_file_name() << endl;
		}

		dir = &root_dirs[1];
		cout << "FileName: " << dir->get_file_name() << "." << dir->get_file_ext() << endl;

		/* READ CLUSTERS  */
		auto bytes_per_cluster = boot.sectors_per_cluster * SECTOR_SIZE;
		
		
		return true;
	}
private:
private:
	BootSector boot;
	vector<uint8_t> fat_data;
	vector<DirectoryEntry> root_dirs;
};

int main() {
	ifstream img(FILE_NAME);
    if (not img.is_open()) {
		cerr << "ERROR: Could not open file: " << FILE_NAME << endl;
		return 1;
	}

    Floppy floppy;
	floppy.read(img);

	// read boot sector
	return 0;
}
