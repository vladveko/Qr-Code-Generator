#pragma once

#include "resource.h"
#include <vector>

class QrCode {

private:

	void EncodeText(std::string);

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