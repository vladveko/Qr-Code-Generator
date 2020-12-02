#pragma once

#include "resource.h"
#include <vector>

class QrSegment {
public:
	QrSegment(int nChars, std::vector<bool> &&dt);

	void GetData();
	void GetNumChars();

	int numChars;

	/* The data bits of this segment. Accessed through getData(). */
	std::vector<bool> data;

	//static void EncodeText(const char* text);
	static QrSegment makeAlphanumeric(const char* text);
	static const char* ALPHANUMERIC_CHARSET;
};

class QrCode {

private:

public:
	QrCode();
	~QrCode();

	/* Основная функция, вызов которой генерирует QR-код */
	void Generate();
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