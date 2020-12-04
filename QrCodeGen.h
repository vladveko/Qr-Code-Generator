#pragma once

#include "resource.h"
#include <vector>

using namespace std;

class Mode {
private:
	int modeCode;
	int dfLen[3];

public:
	Mode(int mCode, int dfLen1, int dfLen2, int dfLen3);
	Mode();

	int GetModeCode();
	int GetDFLen(int version);

	const static Mode Numeric;
	const static Mode Alphanumeric;
	const static Mode Byte;
};

class QrSegment {
public:
	QrSegment(int nChars, vector<bool> &&dt, Mode mode);

	Mode GetMode();
	std::vector<bool> GetData();
	int GetNumChars();
	size_t GetTotalBits();
	size_t GetTotalBytes();

	static QrSegment convertAlphanumeric(const char* text);

private:
	Mode mode;
	int numChars;

	/* The data bits of this segment. Accessed through getData(). */
	vector<bool> data;

	static const char* ALPHANUMERIC_CHARSET;
};

class QrCode {
public:
	QrCode();
	~QrCode();

	/* �������� �������, ����� ������� ���������� QR-��� */
	static void Generate();
	static vector<uint8_t> reedSolomonComputeDivisor(int degree);
	

private:
	int version;
	int ecl;

	static int CalcVersion(int ecl, int size);

	static int getCapacitySize(int ver, int ecl);
	
	static QrSegment AddIndicators(const vector<bool>& data, Mode mode, int size,int ecl);
	
	static vector<uint8_t> GetDataBytes(const vector<bool>& data);
	
	static vector<vector<uint8_t>> DivideToBlocks(const vector<uint8_t>& data, int ecl, int ver);
	
	static vector<vector<uint8_t>> CalcECBytes(const vector<vector<uint8_t>>& data, int ecl, int ver);

	//static vector<uint8_t> reedSolomonComputeDivisor(int degree);
	static uint8_t reedSolomonMultiply(uint8_t x, uint8_t y);

	static QrCode EncodeSegments(QrSegment &seg, int ecl, int mask);

	const static int8_t ECC_CODEWORDS_PER_BLOCK[4][41];
	const static int8_t NUM_ERROR_CORRECTION_BLOCKS[4][41];
};


class BitBuffer final : public std::vector<bool> {

	/*---- Constructor ----*/

	// Creates an empty bit buffer (length 0).
public: BitBuffer();

		/*---- Method ----*/

		// Appends the given number of low-order bits of the given value
		// to this buffer. Requires 0 <= len <= 31 and val < 2^len.
public: 
	void appendBits(std::uint32_t val, int len);

};