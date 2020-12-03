#pragma once

#include "resource.h"
#include <vector>

class Mode {
private:
	int modeCode;
	int dfLen[3];

public:
	Mode(int mCode, int dfLen1, int dfLen2, int dfLen3);

	int GetModeCode();
	int GetDFLen(int version);

	const static Mode Numeric;
	const static Mode Alphanumeric;
	const static Mode Byte;
};

class QrSegment {
public:
	QrSegment(int nChars, std::vector<bool> &&dt);

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
	std::vector<bool> data;

	static const char* ALPHANUMERIC_CHARSET;
};

class QrCode {
public:
	QrCode();
	~QrCode();

	/* �������� �������, ����� ������� ���������� QR-��� */
	static void Generate();
	

private:
	int version;
	int ecl;

	static int CalcVersion(int ecl, int size);
	static QrSegment AddIndicators(const std::vector<bool> data, Mode mode, int size,int ecl);
	static std::vector<uint8_t> GetDataBytes(const std::vector<bool> data);
	static QrCode EncodeSegments(QrSegment &seg, int ecl, int mask);
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