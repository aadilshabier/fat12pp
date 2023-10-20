#include <cstdint>
#include <cstring>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

using namespace std;

constexpr int SECTOR_SIZE = 512;
const char* INDENT = "\t";

// Create string from char* padded with ' ' and maximum possible size
string get_padded_str(const char* s, int max_size) {
	string result;
	for (int i=0; i<max_size and s[i]!=' '; i++) {
		result += s[i];
	}
	return result;
}

/* PACKED STRUCTS */

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
}__attribute__((packed));

/* CONVENIENCE FUNCTIONS */

struct FileSystemItem {
	DirectoryEntry dirent;
	// int cluster = -1; // -1 for root directories

	FileSystemItem(DirectoryEntry _dirent)
		: dirent(_dirent)
	{}

	virtual string name() = 0;

	virtual void print_contents(int level) = 0;
};

struct File : FileSystemItem {
	vector<uint8_t> contents;
	File(DirectoryEntry dirent, const vector<uint8_t>& cluster_data)
		: FileSystemItem(dirent)
	{
		// cout << "FILENAME: " << name() << endl;
		auto cluster = dirent.first_cluster;
		cluster -= 2; // first 2 clusters are ignored
		auto file_size = dirent.file_size;
		auto it_begin = cluster_data.begin()+SECTOR_SIZE*cluster;
		auto it_end = it_begin + file_size;
		contents = vector<uint8_t>(it_begin, it_end);
	}

	/* Print contents of file */
	void print_contents(int level) override {
		for (int i=0; i<level; i++) cout << INDENT;
		cout << "- File: " << name() << endl;
		for (int i=0; i<level; i++) cout << INDENT;
		cout << "  Contents: " << (char*)contents.data() << endl;
	}

	string name() override {
		return get_padded_str(dirent.file_name, sizeof(dirent.file_name)) + "."
			+ get_padded_str(dirent.file_ext, sizeof(dirent.file_ext));
;
	}
};

struct Directory : FileSystemItem {
	vector<FileSystemItem*> child_dirs;
	Directory(DirectoryEntry dirent, const vector<uint8_t>& cluster_data)
		: FileSystemItem(dirent)
	{
		auto cluster = dirent.first_cluster;
		cluster -= 2; // first 2 clusters are ignored
		auto child_dirs_data = (DirectoryEntry*)&cluster_data[cluster*SECTOR_SIZE];
		const auto MAX_CHILD_DIRS = SECTOR_SIZE / 32;
		for (size_t i=0; i<MAX_CHILD_DIRS; i++) {
			auto *d = &child_dirs_data[i];
			if (d->file_name[0] == 0x0) {
				break;
			} else if ((uint8_t)d->file_name[0] == 0XE5) {
				// entry deleted but still available, we can skip this
				continue;
			}
		    FileSystemItem *dir;
			if (d->subdirectory) {
				const auto *name = d->file_name;
				// Either "." or ".."
				if (name[0] == '.')
					continue;
				dir = new Directory(*d, cluster_data);
			} else {
				dir = new File(*d, cluster_data);
			}
			child_dirs.push_back(dir);
		}
	}

	/* Recursively print contents
	 */
	void print_contents(int level) override
	{
		for (int i=0; i<level; i++)
			cout << INDENT;
		cout << "- Dir: " << name() << endl;
		for (auto *child : child_dirs) {
			auto name = child->name();
			if (name == "." or name == "..")
				continue;
			child->print_contents(level+1);
		}
	}

	string name() override {
		return get_padded_str(dirent.file_name, sizeof(dirent.file_name));
	}
};

struct VolumeID : FileSystemItem {
	VolumeID(DirectoryEntry d)
		: FileSystemItem(d)
	{}

	void print_contents(int level) override {
		for(int i=0; i<level; i++) cout << INDENT;
		cout << "- VolumeID: " << name() << endl;
	}

	string name() override {
		return get_padded_str(dirent.file_name, sizeof(dirent.file_name));
	}
};


/* DISK CLASS */

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

		/* READ DIRECTORIES AND CLUSTERS */
		vector<DirectoryEntry> root_dirs_data(boot.max_root_dirs);
		auto root_dirs_size = sizeof(DirectoryEntry)*root_dirs_data.size();
		img.read(reinterpret_cast<char*>(root_dirs_data.data()), root_dirs_size);

		auto sectors_read = boot.reserved_sectors + boot.sectors_per_fat*boot.fats + (boot.max_root_dirs*sizeof(DirectoryEntry)/SECTOR_SIZE);
		// clusters yet to read
		auto cluster_count = (boot.sector_count - sectors_read) / boot.sectors_per_cluster;
		auto bytes_per_cluster = boot.sectors_per_cluster*SECTOR_SIZE;
		cluster_data.resize(cluster_count*bytes_per_cluster);
		img.read(reinterpret_cast<char*>(cluster_data.data()), cluster_data.size());

		/* Recursively read directories */
		read_dirs(root_dirs_data, cluster_data);

		return true;
	}

	void describe_disk(bool contents=true) {
		cout << "OEM: " << get_padded_str(boot.oem, sizeof(boot.oem)) << endl;
		cout << "Bytes per sector: " << boot.bytes_per_sector << endl;
		cout << "Sectors per cluster: " << (int)boot.sectors_per_cluster << endl;
		cout << "Reserved Sectors: " << boot.reserved_sectors << endl;
		cout << "Number of FATs: " << (int)boot.fats << endl;
		cout << "Max Root Directories: " << boot.max_root_dirs << endl;
		cout << "Sector count: " << boot.sector_count << endl;
		cout << "Sectors per FAT: " << boot.sectors_per_fat << endl;
		cout << "Hidden Sectors: " << boot.hidden_sectors << endl;
		cout << "Volume label: " << get_padded_str(boot.volume_label, sizeof(boot.volume_label)) << endl;
		cout << "File system type: " << get_padded_str(boot.file_system_type, sizeof(boot.file_system_type)) << endl;

		if (contents) {
			cout << "\n";

			for (auto *dir : root_dirs) {
			    dir->print_contents(0);
			}
		}
	}
private:
	/* Read all the directories in the volume
	 */
	void read_dirs(vector<DirectoryEntry>& root_dirs_data,
						const vector<uint8_t>& cluster_data) {
		for (size_t i=0; i<root_dirs_data.size(); i++) {
			auto *d = &root_dirs_data[i];
			if (d->file_name[0] == 0x0) {
				break;
			} else if ((uint8_t)d->file_name[0] == 0XE5) {
				// entry deleted but still available, we can skip this
				continue;
			}
		    FileSystemItem *dir;
			if (d->volume_id) {
				dir = new VolumeID(*d);
			} else if (d->subdirectory) {
				dir = new Directory(*d, cluster_data);
			} else {
				dir = new File(*d, cluster_data);
			}
			root_dirs.push_back(dir);
		}
	}

private:
	BootSector boot;
	vector<uint8_t> fat_data;
	vector<FileSystemItem*> root_dirs;
	vector<uint8_t> cluster_data;
};

int main(int argc, char** argv) {
	if (argc < 2) {
		cout << "USAGE:\n";
		cout << "  " << argv[0] << " [FILE]\n";
		return 1;
	}
	const auto *file_name = argv[1];
	ifstream img(file_name);
    if (not img.is_open()) {
		cerr << "ERROR: Could not open file: " << file_name << endl;
		return 1;
	}

    Floppy floppy;
	floppy.read(img);

	floppy.describe_disk(true);

	return 0;
}
