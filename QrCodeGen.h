#pragma once

#include "resource.h"
#include <vector>
#include <map>
#include <array>
#include <string>

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
	QrCode(int ver, int ecl, int mask);

	/* Основная функция, вызов которой генерирует QR-код */
	static QrCode Generate(const char* text, int ecl);
	string toSvgString(int border) const;


private:
	int version;
	
	int ecl;
	
	int mask;
	
	int size;
	
	std::vector<std::vector<bool> > modules;
	
	std::vector<std::vector<bool> > isFunction;

	bool module(int x, int y) const;

	void DrawModules(const vector<uint8_t>& data);

	void AddFinderPatterns();
	void AddAlignmentPatterns();
	void AddTimingPatterns();
	void AddVersion();
	void AddData(const vector<uint8_t>& data);

	void AddMaskAndECL(int msk);

	void DrawFinderPattern(int x, int y);

	void DrawSeparators();

	void DrawAlignmentPattern(int x, int y);

	void DrawVersion(long bits, int size);

	void ApplyMask(int mask);

	int CalcBestMask();

	void ClearMaskAndECL();

	bool getModule(int x, int y) const;

	// Calculates and returns the penalty score based on state of this QR Code's current modules.
	// This is used by the automatic mask choice algorithm to find the mask pattern that yields the lowest score.
	long getPenaltyScore();

	// Can only be called immediately after a white run is added, and
	// returns either 0, 1, or 2. A helper function for getPenaltyScore().
	int finderPenaltyCountPatterns(const array<int, 7> & runHistory);


	// Must be called at the end of a line (row or column) of modules. A helper function for getPenaltyScore().
	int finderPenaltyTerminateAndCount(bool currentRunColor, int currentRunLength, array<int, 7> & runHistory);


	// Pushes the given value to the front and drops the last value. A helper function for getPenaltyScore().
	void finderPenaltyAddHistory(int currentRunLength, array<int, 7> & runHistory);
	

	static bool getBit(long x, int i);

	static int CalcVersion(int ecl, int size);

	static int getCapacitySize(int ver, int ecl);
	
	static QrSegment AddIndicators(const vector<bool>& data, Mode mode, int size,int ecl, int version);
	
	static vector<uint8_t> GetDataBytes(const vector<bool>& data);
	
	static vector<vector<uint8_t>> DivideToBlocks(const vector<uint8_t>& data, int ecl, int ver);
	
	static vector<vector<uint8_t>> CalcECBytes(const vector<vector<uint8_t>>& data, int ecl, int ver);

	static vector<uint8_t> ConcatBlocks(const vector<vector<uint8_t>>& data, int dataSize, const vector<vector<uint8_t>>& ECBlocks, int ECBlockSize);

	static QrCode EncodeSegments(QrSegment &seg, int ecl);

	const static int8_t ECC_CODEWORDS_PER_BLOCK[4][41];
	const static int8_t NUM_ERROR_CORRECTION_BLOCKS[4][41];

	const static map<int, vector<uint8_t>> RS_DIVISORS;
	const static uint8_t GALOIS_FIELD[256];
	const static uint8_t REVERSE_GALOIS_FIELD[256];

	const static vector<uint8_t> ALIGNMENT_TABLE[41];
	const static bool FINDER_PATTERN[7][7];
	const static bool ALIGNMENT_PATTERN[5][5];
	const static long MASK_ECL_TABLE[4][8];

	// For use in getPenaltyScore(), when evaluating which mask is best.
	static const int PENALTY_N1;
	static const int PENALTY_N2;
	static const int PENALTY_N3;
	static const int PENALTY_N4;
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