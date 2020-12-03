#include "QrCodeGen.h"

#include <iostream>

/*---Реализация класса Mode---*/

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
const Mode Mode::Byte         (0x4, 8, 16, 16);

/*---Реализация класса QrSegment---*/

const char* QrSegment::ALPHANUMERIC_CHARSET = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ $%*+-./:";

QrSegment::QrSegment(int nChars, std::vector<bool> &&dt) :
	numChars(nChars),
	data(std::move(dt))
{}

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

	return QrSegment(charCount, std::move(bb));
}

/*---Реализация класса QrCode---*/

#define MIN_VERSION 1
#define MAX_VERSION 40

int getCapacitySize(int ver, int ecl) {
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

QrSegment QrCode::AddModeAndSizeFields(QrSegment &seg, Mode encodingType, int version, int size) {

}

QrCode QrCode::EncodeSegments(QrSegment &seg, int ecl, int mask) {
	/* Добавление служебной информации */

	// Вычисление подходящей версии при выбранном уровне коррекции (ECL) и
	// размере кодируемой информации
	size_t dataSize = seg.GetTotalBits();
	int version = CalcVersion(ecl, dataSize);

	// Добавление полей: способ кодирования, кол-во данных

}

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

const int8_t ECC_CODEWORDS_PER_BLOCK[4][41] = {
	// Version: (note that index 0 is for padding, and is set to an illegal value)
	//0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40    Error correction level
	{-1,  7, 10, 15, 20, 26, 18, 20, 24, 30, 18, 20, 24, 26, 30, 22, 24, 28, 30, 28, 28, 28, 28, 30, 30, 26, 28, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30},  // Low
	{-1, 10, 16, 26, 18, 24, 16, 18, 22, 22, 26, 30, 22, 22, 24, 24, 28, 28, 26, 26, 26, 26, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28},  // Medium
	{-1, 13, 22, 18, 26, 18, 24, 18, 22, 20, 24, 28, 26, 24, 20, 30, 24, 28, 28, 26, 30, 28, 30, 30, 30, 30, 28, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30},  // Quartile
	{-1, 17, 28, 22, 16, 22, 28, 26, 26, 24, 28, 24, 28, 22, 24, 24, 30, 28, 28, 26, 28, 30, 24, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30},  // High
};

const int8_t NUM_ERROR_CORRECTION_BLOCKS[4][41] = {
	// Version: (note that index 0 is for padding, and is set to an illegal value)
	//0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40    Error correction level
	{-1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 4,  4,  4,  4,  4,  6,  6,  6,  6,  7,  8,  8,  9,  9, 10, 12, 12, 12, 13, 14, 15, 16, 17, 18, 19, 19, 20, 21, 22, 24, 25},  // Low
	{-1, 1, 1, 1, 2, 2, 4, 4, 4, 5, 5,  5,  8,  9,  9, 10, 10, 11, 13, 14, 16, 17, 17, 18, 20, 21, 23, 25, 26, 28, 29, 31, 33, 35, 37, 38, 40, 43, 45, 47, 49},  // Medium
	{-1, 1, 1, 2, 2, 4, 4, 6, 6, 8, 8,  8, 10, 12, 16, 12, 17, 16, 18, 21, 20, 23, 23, 25, 27, 29, 34, 34, 35, 38, 40, 43, 45, 48, 51, 53, 56, 59, 62, 65, 68},  // Quartile
	{-1, 1, 1, 2, 4, 4, 4, 5, 6, 8, 8, 11, 11, 16, 16, 18, 16, 19, 21, 25, 25, 25, 34, 30, 32, 35, 37, 40, 42, 45, 48, 51, 54, 57, 60, 63, 66, 70, 74, 77, 81},  // High
};