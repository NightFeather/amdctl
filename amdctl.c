/**
 * Copyright 2015  kevinlekiller
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <inttypes.h>

void printBaseFmt();
void printPstates();
int getDec(const char *);
void getReg(const uint32_t);
void getVidType();
float vidTomV(const int);
int mVToVid(float);
void defineFamily();
void setReg(const uint32_t, const char *, int);
void test();

#define PSTATE_CURRENT_LIMIT 0xc0010061
#define PSTATE_CONTROL       0xc0010062
#define PSTATE_STATUS        0xc0010063
#define PSTATE_BASE          0xc0010064
#define COFVID_STATUS        0xc0010071


#define AMD10H 0x10 // K10
#define AMD11H 0x11 // puma
#define AMD12H 0x12 // llanna/fusion
#define AMD13H 0x13 // unknown
#define AMD14H 0x14 // bobcat
#define AMD15H 0x15 // bulldozer/piledriver/steamroller/excavator/etc
#define AMD16H 0x16 // godvari/kaveri/kabini/jaguar/beema/etc
#define AMD17H 0x17 // zen

#define PSTATE_MAX_VAL_BITS          "6:4"
#define CUR_PSTATE_LIMIT_BITS        "2:0"
#define PSTATE_CMD_BITS              "2:0"
#define CUR_PSTATE_BITS              "2:0"
#define IDD_DIV_BITS                 "41:40"
#define IDD_VALUE_BITS               "39:32"
#define CPU_VID_BITS                 "15:9"
#define COFVID_CUR_PSTATE_LIMIT_BITS "58:56"
#define MAX_CPU_COF_BITS             "54:49"
#define MIN_VID_BITS                 "48:42"
#define MAX_VID_BITS                 "41:35"

char *NB_VID_BITS  = "31:25";
char *CPU_DID_BITS = "8:6";
char *CPU_FID_BITS = "5:0";

int PSTATES = 8;
uint64_t buffer;
int core;
int pvi = 0; // Seems like only some 10h use pvi?
int cpuFamily;
int cpuModel;
int minMaxVid = 1;

void defineFamily() {
	switch (family) {
	case AMD10H:
		getVidType();
		PSTATES = 5;
		break;
	case AMD12H:
		CPU_DID_BITS = "8:4";
		CPU_FID_BITS = "3:0";
		break;
	case AMD11H:
	case AMD13H:
		break;
	case AMD15H:
		if (cpuModel > 0x0f) {
			minMaxVid = 0;
			NB_VID_BITS = "31:24";
		}
		break;
	case AMD16H:
		minMaxVid = 0;
		NB_VID_BITS = "31:24;
		break;
	case AMD14H: // Disabled due to differences in cpu vid / did / fid
	case AMD17H: // Disabled because no BKDG currently.
	default:
		fprintf(stderr, "Unsupported AMD CPU family: %d", family);
		exit(EXIT_FAILURE);
	}
}

// TODO parse args
int main(const int argc, const char *argv[]) {
	family = 16;
#define FSB_MHZ 200
	defineFamily();

//test();

	printf("Voltage ID encodings: %s\n", (pvi ? "PVI (parallel)" : "SVI (serial)"));
	int cores = 1;
	for (core = 0; core < cores; core++) {
		printf("CPU Core %d\n", core);
		printPstates();
	}
	return EXIT_SUCCESS;
}

// Test setting voltage to 1450mV on P-State 0 of core 0
void test() {
	core = 0; // Sets CPU core to 0.
	getReg(PSTATE_BASE);  // Gets the info for P-State 0
	printBaseFmt(); // Prints the info.
	int oldVolt = vidTomV(getDec(CPU_VID_BITS)); // Get voltage of p-state.
	setReg(PSTATE_BASE, CPU_VID_BITS, mVToVid((oldVolt - 25))); // Reduces voltage for CPU 0 P-State 0 by 0.025v
	getReg(PSTATE_BASE); // Get info for P-State 0
	printBaseFmt(); // Print infor for P-State 0.
	setReg(PSTATE_BASE, CPU_VID_BITS, mVToVid(oldVolt)); // Reset voltage to normal.
	getReg(PSTATE_BASE); // Get info for P-State 0
	printBaseFmt(); // Print infor for P-State 0.
	exit(EXIT_SUCCESS);
}

void printPstates() {
	int pstate;
	uint32_t reg = (PSTATE_BASE - 1);
	for (pstate = 0; pstate < PSTATES; pstate++) {
		printf("\tP-State %d\n", pstate);
		reg += 1;
		getReg(reg);
		printBaseFmt();
	}

	printf("\tCOFVID Status P-State\n");
	getReg(COFVID_STATUS);
	printBaseFmt();
}

void printBaseFmt() {
	int CpuVid = getDec(CPU_VID_BITS);
	int CpuDid = getDec(CPU_DID_BITS);
	int CpuFid = getDec(CPU_FID_BITS);
	float CpuVolt = vidTomV(CpuVid);
	int NbVid  = getDec(NB_VID_BITS);
	int CpuMult = ((CpuFid + 0x10) / (2 ^ CpuDid));
	float CpuFreq = FSB_MHZ * CpuMult;

	printf("\t\tCPU voltage id          %d\n", CpuVid);
	printf("\t\tCPU divisor id          %d\n", CpuDid);
	printf("\t\tCPU frequency id        %d\n", CpuFid);
	printf("\t\tCPU multiplier          %dx\n", CpuMult);
	printf("\t\tCPU frequency           %.2fMHz\n", CpuFreq);
	printf("\t\tCPU voltgage            %.2fmV\n", CpuVolt);
	printf("\t\tNorth Bridge voltage id %d\n", NbVid);
}

void getReg(const uint32_t reg) {
	char path[32];
	int fh;
	uint64_t tmp_buffer;

	sprintf(path, "/dev/cpu/%d/msr", core);
	fh = open(path, O_RDONLY);
	if (fh < 0) {
		fprintf(stderr, "Could not open cpu %s!\n", path);
		exit(EXIT_FAILURE);
	}

	if (pread(fh, &tmp_buffer, 8, reg) != sizeof buffer) {
		close(fh);
		fprintf(stderr, "Could not read cpu %s!\n", path);
		exit(EXIT_FAILURE);
	}
	close(fh);
	buffer = tmp_buffer;
}

void setReg(const uint32_t reg, const char *loc, int replacement) {
	int low;
	int high;
	uint64_t temp_buffer = buffer;
	char path[32];
	int fh;

	sscanf(loc, "%d:%d", &high, &low);
	if (low > high) {
		int temp = low;
		low = high;
		high = temp;
	}

	temp_buffer = (temp_buffer & (~(high << low)) | (replacement << low));

	sprintf(path, "/dev/cpu/%d/msr", core);
	fh = open(path, O_WRONLY);
	if (fh < 0) {
		fprintf(stderr, "Could not open cpu %s!\n", path);
		exit(EXIT_FAILURE);
	}

	if (pwrite(fh, &temp_buffer, sizeof temp_buffer, reg) != sizeof temp_buffer) {
		close(fh);
		fprintf(stderr, "Could not write to cpu %s!\n", path);
		exit(EXIT_FAILURE);
	}
	close(fh);
}

int getDec(const char *loc) {
	int high;
	int low;
	int bits;
	uint64_t temp = buffer;

	// From msr-tools.
	sscanf(loc, "%d:%d", &high, &low);
	bits = high - low + 1;
	if (bits < 64) {
		temp >>= low;
		temp &= (1ULL << bits) - 1;
	}
	if (temp & (1ULL << (bits - 1))) {
		temp &= ~(1ULL << (temp - 1));
		temp = -temp;
	}
	return (int)temp;
}

// Ported from k10ctl
float vidTomV(const int vid) {
	if (pvi) {
		if (vid < 32) {
			return (1550 - vid * 25);
		}
		return (1162.5 - (vid > 63 ? 63 : vid) * 12.5);
	}
	return (1550 - (vid > 124 ? 124 : vid) * 12.5);
}

int mVToVid(float mV) {
	int minVid = 0;
	int maxVid = 124;
	int i;
	float tmpv;
	float volt = 1550;
	float mult = 12.5;
	if (mV > 1550 || mV < 1) {
		// TODO CHECK ERROR IN ARGV
		return;
	}
	if (pvi) {
		if (mV > 1162.5) {
			mult = 25;
		} else {
			maxVid = 63;
			volt = 1162.5;
		}
	}
	float min = (mV - (mult / 2));
	float max = (mV + (mult / 2));
	for (i = 1; i <= maxVid; i++) {
		tmpv = volt - i * mult;
		if (tmpv >= min && tmpv <= max) {
			printf("Found vid %d for voltage %.2f\n", i, tmpv);
			return i;
		}
	}
	return 0;
}

// Ported from k10ctl
void getVidType() {
	int fh;
	char *path = "/proc/bus/pci/00/18.3";
	char buff[256];

	fh = open(path, O_RDONLY);
	if (fh < 0) {
		fprintf(stderr, "Could not check voltage identifier encodings in %s!\n", path);
		exit(EXIT_FAILURE);
	}

	if (read(fh, &buff, 256) != 256) {
		close(fh);
		fprintf(stderr, "Could not read voltage identifier encoding from %s!\n", path);
		exit(EXIT_FAILURE);
	}
	close(fh);

	if (buff[3] != 0x12 || buff[2] != 0x3 || buff[1] != 0x10 || buff[0] != 0x22) {
		fprintf(stderr, "Could not read voltage identifier encoding from %s, unsupported CPU?\n", path);
		exit(EXIT_FAILURE);
	}
	pvi = (buff[0xa1] & 1 == 1);
}
