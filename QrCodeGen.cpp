#include "QrCodeGen.h"

#include <iostream>

const char* QrSegment::ALPHANUMERIC_CHARSET = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ $%*+-./:";

QrSegment::QrSegment(int nChars, std::vector<bool> &&dt) :
	numChars(nChars),
	data(std::move(dt))
{}

QrSegment QrSegment::makeAlphanumeric(const char* text) {
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

void QrSegment::EncodeText(const char* text) {

}

void QrCode::Generate() {

}

//void QrCode::EncodeText(std::string text) {
//	size_t len = text.length();
//
//	BitBuffer bb;
//	int i = 0;
//	while (i < len) {
//
//	}
//}

BitBuffer::BitBuffer()
	: std::vector<bool>() {}


void BitBuffer::appendBits(std::uint32_t val, int len) {
	if (len < 0 || len > 31 || val >> len != 0)
		throw std::domain_error("Value out of range");
	for (int i = len - 1; i >= 0; i--)  // Append bit by bit
		this->push_back(((val >> i) & 1) != 0);
}