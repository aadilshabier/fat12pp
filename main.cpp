#include <cstdint>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <string>
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
	uint16_t root_dirs;
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

class Floppy {
public:
    bool read(ifstream& img) {
		img.read(reinterpret_cast<char*>(&boot), sizeof(boot));
		if (unsigned sig = boot.boot_signature; sig != 0x28 and sig != 0x29) {
			cerr << "ERROR: Signature is not valid: " << sig << endl;
			return false;
		}

		// skip reserved sectors 
		img.seekg((boot.reserved_sectors-1)*SECTOR_SIZE, std::ios_base::cur);

		// read FAT data
		fat_data.resize(SECTOR_SIZE*boot.fats*sizeof(uint8_t)/sizeof(fat_data[0]));
		img.read(reinterpret_cast<char*>(fat_data.data()), fat_data.size()*sizeof(fat_data[0]));
		int active_cluster = 2;
		int fat_offset = active_cluster + active_cluster/2;
		auto fat_sector = 0 + (fat_offset / SECTOR_SIZE);
		auto ent_offset = fat_offset % SECTOR_SIZE;

		auto table_value = *(unsigned short*)&fat_data[ent_offset];

		table_value = (active_cluster & 1) ? table_value >> 4 : table_value & 0xfff;
		// cout << (table_value == 0xFF8) << endl;
		cout << hex << table_value << endl;
		cout << dec;
		
		// abc, DEF -> BC, AF, DE
 // for (int i=0; i<fat_data.size(); i++) {
			// if (i % 2 == 0) {
			// } else {
			// }
		// }

		char lmao[33];
		img.read(lmao, 33);
		cout << lmao << endl;
		
		return true;
	}
private:
private:
	BootSector boot;
	vector<uint8_t> fat_data;
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
