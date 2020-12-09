#include "QrCodeGen.h"

#include <iostream>
#include <list>
#include <sstream>

using namespace std;

/*-- Реализация класса Mode --*/

Mode::Mode() {

}

Mode::Mode(int mCode, int dfLen1, int dfLen2, int dfLen3) {
	modeCode = mCode;
	dfLen[0] = dfLen1;
	dfLen[1] = dfLen2;
	dfLen[2] = dfLen3;
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

QrCode::QrCode(int ver, int errorCL, int msk) {
	version = ver;
	ecl = errorCL;
	mask = msk;
	size = version * 4 + 17;
}

string QrCode::toSvgString(int border) const {
	
	if (border < 0)
		throw std::domain_error("Border must be non-negative");
	
	if (border > INT_MAX / 2 || border * 2 > INT_MAX - size)
		throw std::overflow_error("Border too large");

	std::ostringstream sb;
	sb << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
	sb << "<!DOCTYPE svg PUBLIC \"-//W3C//DTD SVG 1.1//EN\" \"http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd\">\n";
	sb << "<svg xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\" viewBox=\"0 0 ";
	sb << (size + border * 2) << " " << (size + border * 2) << "\" stroke=\"none\">\n";
	sb << "\t<rect width=\"100%\" height=\"100%\" fill=\"#FFFFFF\"/>\n";
	sb << "\t<path d=\"";
	for (int y = 0; y < size; y++) {
		for (int x = 0; x < size; x++) {
			if (getModule(x, y)) {
				if (x != 0 || y != 0)
					sb << " ";
				sb << "M" << (x + border) << "," << (y + border) << "h1v1h-1z";
			}
		}
	}
	sb << "\" fill=\"#000000\"/>\n";
	sb << "</svg>\n";
	return sb.str();
}

bool QrCode::getModule(int x, int y) const {
	return 0 <= x && x < size && 0 <= y && y < size && module(x, y);
}

bool QrCode::module(int x, int y) const {
	return modules.at(static_cast<size_t>(y)).at(static_cast<size_t>(x));
}

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

	for (version = MIN_VERSION; version != MAX_VERSION; version++) {
		int dataCapacityBits = getCapacitySize(version, ecl) * 8;  // Number of data bits available
		
		if (dataUsedBits != -1 && dataUsedBits <= dataCapacityBits)
			return version;  // This version number is found to be suitable
	}
}

QrSegment QrCode::AddIndicators(const std::vector<bool>& data, Mode mode, int size, int ecl, int version) {
	
	// Вычисление подходящей версии при выбранном уровне коррекции (ECL) и
	// размере кодируемой информации
	size_t dataSize = data.size();

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

	int dataCapacitySize = getCapacitySize(version, ecl) * 8;
	for (uint8_t padByte = 0xEC; newBB.size() < dataCapacitySize; padByte ^= 0xEC ^ 0x11)
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
		
		int dataBlockSize = block.size();
		int currECBlockSize = nECBytesPerBlock;
		if (dataBlockSize > nECBytesPerBlock) {
			currECBlockSize = dataBlockSize;
		}

		list<uint8_t> ECBytes(currECBlockSize, 0);
		copy(block.begin(), block.end(), ECBytes.begin());
	
		vector<uint8_t> RSDiv = it->second;
		vector<uint8_t> buf(currECBlockSize);
		for (int i = 0; i < dataBlockSize; i++) {
			uint8_t elem = ECBytes.front();
			ECBytes.pop_front();
			ECBytes.push_back(0);

			if (elem != 0) {
				uint8_t matchElem = REVERSE_GALOIS_FIELD[static_cast<int>(elem)];

				for (int idx = 0; idx < nECBytesPerBlock; idx++) {
					buf[idx] = (RSDiv[idx] + matchElem) % 255;
					buf[idx] = GALOIS_FIELD[buf[idx]];
				}

				int idx = 0;
				list<uint8_t> temp;
				for (uint8_t byte : ECBytes) {
					uint8_t newElem = buf[idx++] ^ byte;
					temp.push_back(newElem);

					if (idx > nECBytesPerBlock)
						break;
				}
				copy(temp.begin(), temp.end(), ECBytes.begin());
			}

		}

		copy(ECBytes.begin(), ECBytes.end(), buf.begin());
		buf.resize(nECBytesPerBlock);
		result.push_back(buf);
	}
	
	return result;
}

vector<uint8_t> QrCode::ConcatBlocks(const vector<vector<uint8_t>>& dataBlocks, int dataSize, const vector<vector<uint8_t>>& ECBlocks, int ECBlockSize) {

	vector<uint8_t> result;
	int blockSize = dataSize / dataBlocks.size();

	// Объединение блоков данных
	for (int i = 0; i < blockSize + 1; i++) {
		for (vector<uint8_t> block : dataBlocks) {
			if (i < block.size()) {
				result.push_back(block.at(i));
			}
		}
	}

	//Объединение блоков коррекции
	for (int i = 0; i < ECBlockSize; i++) {
		for (vector<uint8_t> block : ECBlocks) {
			if (i < block.size()) {
				result.push_back(block.at(i));
			}
		}
	}

	return result;
}

QrCode QrCode::EncodeSegments(QrSegment &seg, int ecl) {
	/* Добавление служебной информации */
	size_t dataSize = seg.GetData().size();
	int ver = CalcVersion(ecl, dataSize);

	bool correctVer = true;
	do {
		// Добавление полей: способ кодирования, кол-во данных
		QrSegment tempSeg = AddIndicators(seg.GetData(), seg.GetMode(), seg.GetNumChars(), ecl, ver);

		if (tempSeg.GetData().size() > getCapacitySize(ver, ecl) * 8) {
			correctVer = false;
			ver++;
		}
		else {
			correctVer = true;
			seg = tempSeg;
		}

	} while (!correctVer);

	vector<uint8_t> dataInBytes = GetDataBytes(seg.GetData());

	vector<vector<uint8_t>> dataBlocks = DivideToBlocks(dataInBytes, ecl, ver);

	vector<vector<uint8_t>> ECBlocks = CalcECBytes(dataBlocks, ecl, ver);

	dataInBytes = ConcatBlocks(dataBlocks, dataInBytes.size(), ECBlocks, ECC_CODEWORDS_PER_BLOCK[ecl][ver]);

	QrCode qrcode(ver, ecl, 0);
	qrcode.DrawModules(dataInBytes);

	return qrcode;
}

QrCode QrCode::Generate(const char* text, int ecl) {
	
	QrSegment seg = QrSegment::convertAlphanumeric(text);

	return EncodeSegments(seg, ecl);
}

void QrCode::DrawModules(const vector<uint8_t>& data) {
	
	size_t sz = static_cast<size_t>(size);
	modules = vector<vector<bool> >(sz, vector<bool>(sz));
	isFunction = vector<vector<bool> >(sz, vector<bool>(sz));

	AddFinderPatterns();
	AddAlignmentPatterns();
	AddTimingPatterns();
	AddVersion();
	AddMaskAndECL(mask);

	AddData(data);
	
	mask = CalcBestMask();
	ClearMaskAndECL();
	
	AddMaskAndECL(mask);
	ApplyMask(mask);
}

void QrCode::AddFinderPatterns() {

	DrawFinderPattern(0, 0);
	DrawFinderPattern(0, size - 7);
	DrawFinderPattern(size - 7, 0);

	DrawSeparators();
}

void QrCode::AddAlignmentPatterns() {

	if (version == 1)
		return;

	vector<uint8_t> coordinates = ALIGNMENT_TABLE[version];

	for (uint8_t row : coordinates) {
		for (uint8_t colunm : coordinates) {

			if (!isFunction.at(row).at(colunm))
				DrawAlignmentPattern(static_cast<int>(row), static_cast<int>(colunm));
		}
	}
}

void QrCode::AddTimingPatterns() {

	bool module = 1; //  изначально черный
	for (int i = 8; i < size - 7; i++) {
		
		if (!isFunction.at(6).at(i)) {
			modules.at(6).at(i) = module;
			isFunction.at(6).at(i) = true;
		}
		module = !module;
	}

	module = 1;
	for (int i = 8; i < size - 7; i++) {

		if (!isFunction.at(i).at(6)) {
			modules.at(i).at(6) = module;
			isFunction.at(i).at(6) = true;
		}
		module = !module;
	}
}

void QrCode::AddVersion() {

	if (version < 7)
		return;

	int rem = version;  // версия - uint6, в диапазоне [7, 40]

	for (int i = 0; i < 12; i++)
		rem = (rem << 1) ^ ((rem >> 11) * 0x1F25);
	
	long bits = static_cast<long>(version) << 12 | rem;  // uint18
	
	if (bits >> 18 != 0)
		throw std::logic_error("Assertion error");

	DrawVersion(bits, size);
}

void QrCode::AddMaskAndECL(int msk) {

	long code = MASK_ECL_TABLE[ecl][msk];

	int bitpos = 14;
	for (int i = 0; i < 8; i++) {
		if (!isFunction.at(8).at(i)) {
			modules.at(8).at(i) = getBit(code, bitpos--);
			isFunction.at(8).at(i) = true;
		}
	}

	for (int i = 8; i > -1; i--) {
		if (!isFunction.at(i).at(8)) {
			modules.at(i).at(8) = getBit(code, bitpos--);
			isFunction.at(i).at(8) = true;
		}
	}

	bitpos = 14;
	for (int i = size - 1; i > size - 1 - 7; i--) {
		modules.at(i).at(8) = getBit(code, bitpos--);
		isFunction.at(i).at(8) = true;
	}

	// Dark Module
	modules.at(size - 8).at(8) = 1;
	isFunction.at(size - 8).at(8) = true;

	for (int i = size - 8; i < size; i++) {
		modules.at(8).at(i) = getBit(code, bitpos--);
		isFunction.at(8).at(i) = true;
	}
}

void QrCode::ClearMaskAndECL() {

	for (int i = 0; i < 8; i++) {
		if (i != 6) {
			isFunction.at(8).at(i) = false;
		}
	}

	for (int i = 8; i > -1; i--) {
		if (i != 6) {
			isFunction.at(i).at(8) = false;
		}
	}

	for (int i = size - 1; i > size - 1 - 7; i--) {
		isFunction.at(i).at(8) = false;
	}

	for (int i = size - 8; i < size; i++) {
		isFunction.at(8).at(i) = false;
	}
}

void QrCode::AddData(const vector<uint8_t>& data) {

	size_t i = 0;  // Bit index into the data
	// Do the funny zigzag scan
	for (int right = size - 1; right >= 1; right -= 2) {  // Index of right column in each column pair
		
		if (right == 6)
			right = 5;

		for (int vert = 0; vert < size; vert++) {  // Vertical counter
			for (int j = 0; j < 2; j++) {

				size_t x = static_cast<size_t>(right - j);  // Actual x coordinate
				bool upward = ((right + 1) & 2) == 0;
				size_t y = static_cast<size_t>(upward ? size - 1 - vert : vert);  // Actual y coordinate
				
				if (!isFunction.at(y).at(x) && i < data.size() * 8) {
					modules.at(y).at(x) = getBit(data.at(i >> 3), 7 - static_cast<int>(i & 7));
					i++;
				}
				// If this QR Code has any remainder bits (0 to 7), they were assigned as
				// 0/false/white by the constructor and are left unchanged by this method
			}
		}
	}
}

void QrCode::DrawFinderPattern(int x, int y) {

	for (int i = 0; i < 7; i++) {
		for (int j = 0; j < 7; j++) {
			modules.at(x + i).at(y + j) = FINDER_PATTERN[i][j];
			isFunction.at(x + i).at(y + j) = true;
		}
	}
}

void QrCode::DrawSeparators() {
	
	// top left separator
	for (int i = 0; i < 8; i++) {
		isFunction.at(7).at(i) = true;
	}
	for (int i = 7; i >= 0; i--) {
		isFunction.at(i).at(7) = true;
	}

	// top right separator
	for (int i = size - 8; i < size; i++) {
		isFunction.at(7).at(i) = true;
	}
	for (int i = 0; i < 8; i++) {
		isFunction.at(i).at(size - 8) = true;
	}

	// bottom left separator
	for (int i = 0; i < 8; i++) {
		isFunction.at(size - 8).at(i) = true;
	}
	for (int i = size - 8; i < size; i++) {
		isFunction.at(i).at(7) = true;
	}
}

void QrCode::DrawAlignmentPattern(int x, int y) {
	
	int lcx = x - 2;
	int lcy = y - 2;

	for (int i = 0; i < 5; i++) {
		for (int j = 0; j < 5; j++) {
			modules.at(lcx + i).at(lcy + j) = ALIGNMENT_PATTERN[i][j];
			isFunction.at(lcx + i).at(lcy + j) = true;
		}
	}
}

void QrCode::DrawVersion(long bits, int size) {

	int lcx = 0, lcy = size - 11;

	for (int r = 0; r < 3; r++) {
		for (int c = 0; c < 6; c++) {
			bool bit = getBit(bits, r * c);

			modules.at(lcx + r).at(lcy + c) = bit;
			isFunction.at(lcx + r).at(lcy + c) = true;

			// Так код версии зеркален
			modules.at(lcy + r).at(lcx + c) = bit;
			isFunction.at(lcy + r).at(lcx + c) = true;
		}
	}
}

int QrCode::CalcBestMask() {
	long minPenalty = LONG_MAX;
	int msk = 0;

	for (int i = 0; i < 8; i++) {
		ClearMaskAndECL();

		AddMaskAndECL(i);
		ApplyMask(i);

		long penalty = getPenaltyScore();
		
		if (penalty < minPenalty) {
			msk = i;
			minPenalty = penalty;
		}
		ApplyMask(i);
	}

	return msk;
}

void QrCode::ApplyMask(int msk) {
	size_t sz = static_cast<size_t>(size);

	for (size_t y = 0; y < sz; y++) {
		for (size_t x = 0; x < sz; x++) {

			bool invert;

			switch (msk) {
				case 0:  invert = (x + y) % 2 == 0;                    break;

				case 1:  invert = y % 2 == 0;                          break;
				
				case 2:  invert = x % 3 == 0;                          break;
				
				case 3:  invert = (x + y) % 3 == 0;                    break;
				
				case 4:  invert = (x / 3 + y / 2) % 2 == 0;            break;
				
				case 5:  invert = x * y % 2 + x * y % 3 == 0;          break;
				
				case 6:  invert = (x * y % 2 + x * y % 3) % 2 == 0;    break;
				
				case 7:  invert = ((x + y) % 2 + x * y % 3) % 2 == 0;  break;
				
				default:  throw std::logic_error("Assertion error");
			}

			modules.at(y).at(x) = modules.at(y).at(x) ^ (invert & !isFunction.at(y).at(x));
		}
	}
}

long QrCode::getPenaltyScore() {
	long result = 0;

	// Adjacent modules in row having same color, and finder-like patterns
	for (int y = 0; y < size; y++) {
		bool runColor = false;
		int runX = 0;
		std::array<int, 7> runHistory = {};
		for (int x = 0; x < size; x++) {
			if (module(x, y) == runColor) {
				runX++;
				if (runX == 5)
					result += PENALTY_N1;
				else if (runX > 5)
					result++;
			}
			else {
				finderPenaltyAddHistory(runX, runHistory);
				if (!runColor)
					result += finderPenaltyCountPatterns(runHistory) * PENALTY_N3;
				runColor = module(x, y);
				runX = 1;
			}
		}
		result += finderPenaltyTerminateAndCount(runColor, runX, runHistory) * PENALTY_N3;
	}
	// Adjacent modules in column having same color, and finder-like patterns
	for (int x = 0; x < size; x++) {
		bool runColor = false;
		int runY = 0;
		std::array<int, 7> runHistory = {};
		for (int y = 0; y < size; y++) {
			if (module(x, y) == runColor) {
				runY++;
				if (runY == 5)
					result += PENALTY_N1;
				else if (runY > 5)
					result++;
			}
			else {
				finderPenaltyAddHistory(runY, runHistory);
				if (!runColor)
					result += finderPenaltyCountPatterns(runHistory) * PENALTY_N3;
				runColor = module(x, y);
				runY = 1;
			}
		}
		result += finderPenaltyTerminateAndCount(runColor, runY, runHistory) * PENALTY_N3;
	}

	// 2*2 blocks of modules having same color
	for (int y = 0; y < size - 1; y++) {
		for (int x = 0; x < size - 1; x++) {
			bool  color = module(x, y);
			if (color == module(x + 1, y) &&
				color == module(x, y + 1) &&
				color == module(x + 1, y + 1))
				result += PENALTY_N2;
		}
	}

	// Balance of black and white modules
	int black = 0;
	for (const vector<bool>& row : modules) {
		for (bool color : row) {
			if (color)
				black++;
		}
	}
	int total = size * size;  // Note that size is odd, so black/total != 1/2
	// Compute the smallest integer k >= 0 such that (45-5k)% <= black/total <= (55+5k)%
	int k = static_cast<int>((std::abs(black * 20L - total * 10L) + total - 1) / total) - 1;
	result += k * PENALTY_N4;
	return result;
}

int QrCode::finderPenaltyCountPatterns(const std::array<int, 7> & runHistory) {
	int n = runHistory.at(1);
	if (n > size * 3)
		throw std::logic_error("Assertion error");
	bool core = n > 0 && runHistory.at(2) == n && runHistory.at(3) == n * 3 && runHistory.at(4) == n && runHistory.at(5) == n;
	return (core && runHistory.at(0) >= n * 4 && runHistory.at(6) >= n ? 1 : 0)
		+ (core && runHistory.at(6) >= n * 4 && runHistory.at(0) >= n ? 1 : 0);
}


int QrCode::finderPenaltyTerminateAndCount(bool currentRunColor, int currentRunLength, std::array<int, 7> & runHistory) {
	if (currentRunColor) {  // Terminate black run
		finderPenaltyAddHistory(currentRunLength, runHistory);
		currentRunLength = 0;
	}
	currentRunLength += size;  // Add white border to final run
	finderPenaltyAddHistory(currentRunLength, runHistory);
	return finderPenaltyCountPatterns(runHistory);
}


void QrCode::finderPenaltyAddHistory(int currentRunLength, std::array<int, 7> & runHistory) {
	if (runHistory.at(0) == 0)
		currentRunLength += size;  // Add white border to initial run
	std::copy_backward(runHistory.cbegin(), runHistory.cend() - 1, runHistory.end());
	runHistory.at(0) = currentRunLength;
}

bool QrCode::getBit(long x, int i) {
	return ((x >> i) & 1) != 0;
}



BitBuffer::BitBuffer()
	: std::vector<bool>() {}

void BitBuffer::appendBits(std::uint32_t val, int len) {
	if (len < 0 || len > 31 || val >> len != 0)
		throw std::domain_error("Value out of range");
	
	for (int i = len - 1; i >= 0; i--)  // Append bit by bit
		this->push_back(((val >> i) & 1) != 0);
}

const int QrCode::PENALTY_N1 = 3;
const int QrCode::PENALTY_N2 = 3;
const int QrCode::PENALTY_N3 = 40;
const int QrCode::PENALTY_N4 = 10;

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


const vector<uint8_t> QrCode::ALIGNMENT_TABLE[41] = {
	{}, {}, {18}, {22}, {26}, {30}, {34}, {6, 22, 38}, {6, 24, 42 }, {6, 26, 46}, {6, 28, 50}, {6, 30, 54}, {6, 32, 58}, {6, 34, 62}, {6, 26, 46, 66},{6, 26, 48, 70},
	{6, 26, 50, 74}, {6, 30, 54, 78}, {6, 30, 56, 82}, {6, 30, 56, 82}, {6, 34, 62, 90}, {6, 28, 50, 72, 94}, {6, 26, 50, 74, 98}, {6, 30, 54, 78, 102}, {6, 28, 54, 80, 106},
	{6, 32, 58, 84, 110}, {6, 30, 58, 86, 114}, {6, 34, 62, 90, 118}, {6, 26, 50, 74, 98, 122}, {6, 30, 54, 78, 102, 126}, {6, 26, 52, 78, 104, 130}, {6, 30, 56, 82, 108, 134},
	{6, 34, 60, 86, 112, 138}, {6, 30, 58, 86, 114, 142}, {6, 34, 62, 90, 118, 146}, {6, 30, 54, 78, 102, 126, 150}, {6, 24, 50, 76, 102, 128, 154}, {6, 28, 54, 80, 106, 132, 158},
	{6, 32, 58, 84, 110, 136, 162}, {6, 26, 54, 82, 110, 138, 166}, {6, 30, 58, 86, 114, 142, 170}
};

const bool QrCode::FINDER_PATTERN[7][7] = {
	{ 1, 1, 1, 1, 1, 1, 1},
	{ 1, 0, 0, 0, 0, 0, 1},
	{ 1, 0, 1, 1, 1, 0, 1},
	{ 1, 0, 1, 1, 1, 0, 1},
	{ 1, 0, 1, 1, 1, 0, 1},
	{ 1, 0, 0, 0, 0, 0, 1},
	{ 1, 1, 1, 1, 1, 1, 1},
};

const bool QrCode::ALIGNMENT_PATTERN[5][5] = {
	{ 1, 1, 1, 1, 1},
	{ 1, 0, 0, 0, 1},
	{ 1, 0, 1, 0, 1},
	{ 1, 0, 0, 0, 1},
	{ 1, 1, 1, 1, 1},
};

const long QrCode::MASK_ECL_TABLE[4][8] = {
	{30660, 29427, 32170, 30877, 26159, 25368, 27713, 26998},
	{21522, 20773, 24188, 23371, 17913, 16590, 20375, 19104},
	{13663, 12392, 16177, 14854,  9396,  8579, 11994, 11245},
	{ 5769,  5054,  7399,  6608,  1890,   597,  3340,  2107},
};