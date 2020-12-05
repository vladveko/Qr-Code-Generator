#include "QrCodeGen.h"

#include <iostream>
#include <list>

using namespace std;

/*-- Реализация класса Mode --*/

Mode::Mode() {

}

Mode::Mode(int mCode, int dfLen1, int dfLen2, int dfLen3) {
	modeCode = mCode;
	dfLen[1] = dfLen1;
	dfLen[2] = dfLen2;
	dfLen[3] = dfLen3;
}

int Mode::GetModeCode() {
	return modeCode;
}

int Mode::GetDFLen(int ver) {
	return dfLen[(ver + 7) / 17];
}

const Mode Mode::Numeric	  (0x1, 10, 12, 14);
const Mode Mode::Alphanumeric (0x2, 9, 11, 13);
const Mode Mode::Byte		  (0x4, 8, 16, 16);

/*-- Реализация класса QrSegment --*/

const char* QrSegment::ALPHANUMERIC_CHARSET = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ $%*+-./:";

QrSegment::QrSegment(int nChars, vector<bool>&& dt, Mode md) {
	numChars = nChars;
	data = std::move(dt);
	mode = md;
}

Mode QrSegment::GetMode() {
	return mode;
}

std::vector<bool> QrSegment::GetData() {
	return data;
}

size_t QrSegment::GetTotalBits() {
	return data.size();
}

size_t QrSegment::GetTotalBytes() {
	return data.size() / 8;
}

int QrSegment::GetNumChars() {
	return numChars;
}

QrSegment QrSegment::convertAlphanumeric(const char* text) {
	BitBuffer bb;
	int accumData = 0;
	int accumCount = 0;
	int charCount = 0;

	for (; *text != '\0'; text++, charCount++) {
		const char* temp = std::strchr(ALPHANUMERIC_CHARSET, *text);
		if (temp == nullptr)
			throw std::domain_error("String contains unencodable characters in alphanumeric mode");

		accumData = accumData * 45 + static_cast<int>(temp - ALPHANUMERIC_CHARSET);
		accumCount++;

		if (accumCount == 2) {
			bb.appendBits(static_cast<uint32_t>(accumData), 11);
			accumData = 0;
			accumCount = 0;
		}
	}

	if (accumCount > 0)  // 1 character remaining
		bb.appendBits(static_cast<uint32_t>(accumData), 6);

	return QrSegment(charCount, std::move(bb), Mode::Alphanumeric);
}

/*---Реализация класса QrCode---*/

#define MIN_VERSION 1
#define MAX_VERSION 40

int QrCode::getCapacitySize(int ver, int ecl) {
	if (ver < MIN_VERSION || ver > MAX_VERSION)
		throw std::domain_error("Version number out of range");

	int result = (16 * ver + 128) * ver + 64;
	if (ver >= 2) {
		int numAlign = ver / 7 + 2;
		result -= (25 * numAlign - 10) * numAlign - 55;

		if (ver >= 7)
			result -= 36;
	}

	if (!(208 <= result && result <= 29648))
		throw std::logic_error("Assertion error");

	return result / 8
		- ECC_CODEWORDS_PER_BLOCK[static_cast<int>(ecl)][ver]
		* NUM_ERROR_CORRECTION_BLOCKS[static_cast<int>(ecl)][ver];;
}

int QrCode::CalcVersion(int ecl, int size) {

	int version, dataUsedBits = size;
	bool found = false;

	for (version = MIN_VERSION; !found; version++) {
		int dataCapacityBits = getCapacitySize(version, ecl) * 8;  // Number of data bits available
		
		if (dataUsedBits != -1 && dataUsedBits <= dataCapacityBits)
			found = true;  // This version number is found to be suitable

		//if (version >= MAX_VERSION) {  // All versions in the range could not fit the given data
		//	std::ostringstream sb;
		//	if (dataUsedBits == -1)
		//		sb << "Segment too long";
		//	else {
		//		sb << "Data length = " << dataUsedBits << " bits, ";
		//		sb << "Max capacity = " << dataCapacityBits << " bits";
		//	}
		//	throw data_too_long(sb.str());
		//}
	}

	return version;
}

QrSegment QrCode::AddIndicators(const std::vector<bool>& data, Mode mode, int size, int ecl) {
	
	// Вычисление подходящей версии при выбранном уровне коррекции (ECL) и
	// размере кодируемой информации
	size_t dataSize = data.size();
	int version = CalcVersion(ecl, dataSize);

	int mCode = mode.GetModeCode();
	int dfLen = mode.GetDFLen(version);
	
	BitBuffer newBB;
	// Добавление поля способа кодирования
	newBB.appendBits(static_cast<uint32_t>(mCode), 4);
	// Добавление поля кол-во данных
	newBB.appendBits(static_cast<uint32_t>(size), dfLen);
	// Добавление данных
	newBB.insert(newBB.end(), data.begin(), data.end());

	newBB.appendBits(0, (8 - newBB.size() % 8));

	for (uint8_t padByte = 0xEC; newBB.size() < dataSize; padByte ^= 0xEC ^ 0x11)
		newBB.appendBits(padByte, 8);

	return QrSegment(size, std::move(newBB), mode);
}

std::vector<uint8_t> QrCode::GetDataBytes(const std::vector<bool>& data) {

	std::vector<uint8_t> dataBytes(data.size() / 8);
	for (size_t i = 0; i < data.size(); i++)
		dataBytes[i >> 3] |= (data.at(i) ? 1 : 0) << (7 - (i & 7));

	return dataBytes;
}

std::vector<std::vector<uint8_t>> QrCode::DivideToBlocks(const std::vector<uint8_t>& data, int ecl, int ver) {

	int8_t nBlocks = NUM_ERROR_CORRECTION_BLOCKS[ecl][ver];
	int dataSize = data.size();
	int nBytesPerBlock = dataSize / nBlocks;
	int nLongBlocks = dataSize % nBlocks;
	
	std::vector<std::vector<uint8_t>> blocks;
	
	for (int i = 0; i < (nBlocks - nLongBlocks); i++) {
		std::vector<uint8_t> block;
		for (int b = 0; b < nBytesPerBlock; b++) {
			block.push_back(data.at(b));
		}
		blocks.push_back(block);
	}

	for (int i = 0; i < nLongBlocks; i++) {
		std::vector<uint8_t> block;
		for (int b = 0; b < (nBytesPerBlock + 1); b++) {
			block.push_back(data.at(b));
		}
		blocks.push_back(block);
	}

	return blocks;
}


vector<vector<uint8_t>> QrCode::CalcECBytes(const vector<vector<uint8_t>>& data, int ecl, int ver) {

	int nECBytesPerBlock = ECC_CODEWORDS_PER_BLOCK[ecl][ver];
	map<int, vector<uint8_t>> :: const_iterator it = RS_DIVISORS.find(nECBytesPerBlock);

	vector<vector<uint8_t>> result;
	int ecbAdded = 0;
	for (vector<uint8_t> block : data) {
		list<uint8_t> ECBytes(nECBytesPerBlock, 0);
		copy(block.begin(), block.end(), ECBytes.begin());
	
		vector<uint8_t> RSDiv = it->second;
		vector<uint8_t> buf(nECBytesPerBlock);
		for (int i = 0; i < block.size();i++) {
			uint8_t elem = ECBytes.front();
			ECBytes.pop_front();
			ECBytes.push_back(0);

			if (elem != 0) {
				uint8_t matchElem = REVERSE_GALOIS_FIELD[static_cast<int>(elem)];

				for (int idx = 0; idx < buf.size(); idx++) {
					buf[idx] = (RSDiv[idx] + matchElem) % 255;
					buf[idx] = GALOIS_FIELD[buf[idx]];
				}

				int idx = 0;
				list<uint8_t> temp;
				for (uint8_t byte : ECBytes) {
					uint8_t newElem = buf[idx++] ^ byte;
					temp.push_back(newElem);
				}
				copy(temp.begin(), temp.end(), ECBytes.begin());
			}

		}

		copy(ECBytes.begin(), ECBytes.end(), buf.begin());
		result.push_back(buf);
	}
	
	return result;
}

//QrCode QrCode::EncodeSegments(QrSegment &seg, int ecl, int mask) {
//	/* Добавление служебной информации */
//
//	// Добавление полей: способ кодирования, кол-во данных
//	seg = AddIndicators(seg.GetData(), seg.GetMode(), seg.GetNumChars(), ecl);
//
//	std::vector<uint8_t> dataInBytes = GetDataBytes(seg.GetData());
//
//
//}

void QrCode::Generate() {

}



BitBuffer::BitBuffer()
	: std::vector<bool>() {}

void BitBuffer::appendBits(std::uint32_t val, int len) {
	if (len < 0 || len > 31 || val >> len != 0)
		throw std::domain_error("Value out of range");
	for (int i = len - 1; i >= 0; i--)  // Append bit by bit
		this->push_back(((val >> i) & 1) != 0);
}

const int8_t QrCode::ECC_CODEWORDS_PER_BLOCK[4][41] = {
	// Version: (note that index 0 is for padding, and is set to an illegal value)
	//0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40    Error correction level
	{-1,  7, 10, 15, 20, 26, 18, 20, 24, 30, 18, 20, 24, 26, 30, 22, 24, 28, 30, 28, 28, 28, 28, 30, 30, 26, 28, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30},  // Low
	{-1, 10, 16, 26, 18, 24, 16, 18, 22, 22, 26, 30, 22, 22, 24, 24, 28, 28, 26, 26, 26, 26, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28},  // Medium
	{-1, 13, 22, 18, 26, 18, 24, 18, 22, 20, 24, 28, 26, 24, 20, 30, 24, 28, 28, 26, 30, 28, 30, 30, 30, 30, 28, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30},  // Quartile
	{-1, 17, 28, 22, 16, 22, 28, 26, 26, 24, 28, 24, 28, 22, 24, 24, 30, 28, 28, 26, 28, 30, 24, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30},  // High
};

const int8_t QrCode::NUM_ERROR_CORRECTION_BLOCKS[4][41] = {
	// Version: (note that index 0 is for padding, and is set to an illegal value)
	//0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40    Error correction level
	{-1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 4,  4,  4,  4,  4,  6,  6,  6,  6,  7,  8,  8,  9,  9, 10, 12, 12, 12, 13, 14, 15, 16, 17, 18, 19, 19, 20, 21, 22, 24, 25},  // Low
	{-1, 1, 1, 1, 2, 2, 4, 4, 4, 5, 5,  5,  8,  9,  9, 10, 10, 11, 13, 14, 16, 17, 17, 18, 20, 21, 23, 25, 26, 28, 29, 31, 33, 35, 37, 38, 40, 43, 45, 47, 49},  // Medium
	{-1, 1, 1, 2, 2, 4, 4, 6, 6, 8, 8,  8, 10, 12, 16, 12, 17, 16, 18, 21, 20, 23, 23, 25, 27, 29, 34, 34, 35, 38, 40, 43, 45, 48, 51, 53, 56, 59, 62, 65, 68},  // Quartile
	{-1, 1, 1, 2, 4, 4, 4, 5, 6, 8, 8, 11, 11, 16, 16, 18, 16, 19, 21, 25, 25, 25, 34, 30, 32, 35, 37, 40, 42, 45, 48, 51, 54, 57, 60, 63, 66, 70, 74, 77, 81},  // High
};

const map<int, vector<uint8_t>> QrCode::RS_DIVISORS = {
	{ 7, {87, 229, 146, 149, 238, 102, 21}},
	{10, {251, 67, 46, 61, 118, 70, 64, 94, 32, 45}},
	{13, {74, 152, 176, 100, 86, 100, 106, 104, 130, 218, 206, 140, 78}},
	{15, {8, 183, 61, 91, 202, 37, 51, 58, 58, 237, 140, 124, 5, 99, 105}},
	{16, {120, 104, 107, 109, 102, 161, 76, 3, 91, 191, 147, 169, 182, 194, 225, 120}},
	{17, {43, 139, 206, 78, 43, 239, 123, 206, 214, 147, 24, 99, 150, 39, 243, 163, 136}},
	{18, {215, 234, 158, 94, 184, 97, 118, 170, 79, 187, 152, 148, 252, 179, 5, 98, 96, 153}},
	{20, {17, 60, 79, 50, 61, 163, 26, 187, 202, 180, 221, 225, 83, 239, 156, 164, 212, 212, 188, 190}},
	{22, {210, 171, 247, 242, 93, 230, 14, 109, 221, 53, 200, 74, 8, 172, 98, 80, 219, 134, 160, 105, 165, 231}},
	{24, {229, 121, 135, 48, 211, 117, 251, 126, 159, 180, 169, 152, 192, 226, 228, 218, 111, 0, 117, 232, 87, 96, 227, 21}},
	{26, {173, 125, 158, 2, 103, 182, 118, 17, 145, 201, 111, 28, 165, 53, 161, 21, 245, 142, 13, 102, 48, 227, 153, 145, 218, 70}},
	{28, {168, 223, 200, 104, 224, 234, 108, 180, 110, 190, 195, 147, 205, 27, 232, 201, 21, 43, 245, 87, 42, 195, 212, 119, 242, 37, 9, 123}},
	{30, {41, 173, 145, 152, 216, 31, 179, 182, 50, 48, 110, 86, 239, 96, 222, 125, 42, 173, 226, 193, 224, 130, 156, 37, 251, 216, 238, 40, 192, 180}}
};

const uint8_t QrCode::GALOIS_FIELD[256] = { 1, 2, 4, 8, 16, 32, 64, 128, 29, 58, 116, 232, 205, 135, 19, 38, 76, 152, 45, 90, 180, 117, 234, 201, 
									143, 3, 6, 12, 24, 48, 96, 192, 157, 39, 78, 156, 37, 74, 148, 53, 106, 212, 181, 119, 238, 193, 159, 
									35, 70, 140, 5, 10, 20, 40, 80, 160, 93, 186, 105, 210, 185, 111, 222, 161, 95, 190, 97, 194, 153, 47, 
									94, 188, 101, 202, 137, 15, 30, 60, 120, 240, 253, 231, 211, 187, 107, 214, 177, 127, 254, 225, 223, 163, 
									91, 182, 113, 226, 217, 175, 67, 134, 17, 34, 68, 136, 13, 26, 52, 104, 208, 189, 103, 206, 129, 31, 62, 
									124, 248, 237, 199, 147, 59, 118, 236, 197, 151, 51, 102, 204, 133, 23, 46, 92, 184, 109, 218, 169, 79, 158, 
									33, 66, 132, 21, 42, 84, 168, 77, 154, 41, 82, 164, 85, 170, 73, 146, 57, 114, 228, 213, 183, 115, 230, 209, 
									191, 99, 198, 145, 63, 126, 252, 229, 215, 179, 123, 246, 241, 255, 227, 219, 171, 75, 150, 49, 98, 196, 149, 
									55, 110, 220, 165, 87, 174, 65, 130, 25, 50, 100, 200, 141, 7, 14, 28, 56, 112, 224, 221, 167, 83, 166, 81, 162, 
									89, 178, 121, 242, 249, 239, 195, 155, 43, 86, 172, 69, 138, 9, 18, 36, 72, 144, 61, 122, 244, 245, 247, 243, 251, 
									235, 203, 139, 11, 22, 44, 88, 176, 125, 250, 233, 207, 131, 27, 54, 108, 216, 173, 71, 142, 1 };

const uint8_t QrCode::REVERSE_GALOIS_FIELD[256] = { -1, 0, 1, 25, 2, 50, 26, 198, 3, 223, 51, 238, 27, 104, 199, 75, 4, 100, 224, 14, 52, 141, 239, 129, 28,
											193, 105, 248, 200, 8, 76, 113, 5, 138, 101, 47, 225, 36, 15, 33, 53, 147, 142, 218, 240, 18, 130, 69,
											29, 181, 194, 125, 106, 39, 249, 185, 201, 154, 9, 120, 77, 228, 114, 166, 6, 191, 139, 98, 102, 221, 48,
											253, 226, 152, 37, 179, 16, 145, 34, 136, 54, 208, 148, 206, 143, 150, 219, 189, 241, 210, 19, 92, 131, 56,
											70, 64, 30, 66, 182, 163, 195, 72, 126, 110, 107, 58, 40, 84, 250, 133, 186, 61, 202, 94, 155, 159, 10, 21,
											121, 43, 78, 212, 229, 172, 115, 243, 167, 87, 7, 112, 192, 247, 140, 128, 99, 13, 103, 74, 222, 237, 49, 197,
											254, 24, 227, 165, 153, 119, 38, 184, 180, 124, 17, 68, 146, 217, 35, 32, 137, 46, 55, 63, 209, 91, 149, 188,
											207, 205, 144, 135, 151, 178, 220, 252, 190, 97, 242, 86, 211, 171, 20, 42, 93, 158, 132, 60, 57, 83, 71, 109,
											65, 162, 31, 45, 67, 216, 183, 123, 164, 118, 196, 23, 73, 236, 127, 12, 111, 246, 108, 161, 59, 82, 41, 157, 85,
											170, 251, 96, 134, 177, 187, 204, 62, 90, 203, 89, 95, 176, 156, 169, 160, 81, 11, 245, 22, 235, 122, 117, 44, 215,
											79, 174, 213, 233, 230, 231, 173, 232, 116, 214, 244, 234, 168, 80, 88, 175
};