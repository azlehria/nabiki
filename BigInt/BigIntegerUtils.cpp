/*
- C++ Big Integer library
  https://mattmccutchen.net/bigint/

Original license statement:

  I, Matt McCutchen, the sole author of the original Big Integer
  Library, waive my copyright to it, placing it in the public domain.
  The library comes with absolutely no warranty.
*/

#include "BigIntegerUtils.hh"
#include "BigUnsignedInABase.hh"

#include <iostream>

using namespace std::string_literals;

std::string bigUnsignedToString(const BigUnsigned &x) {
	return std::string(BigUnsignedInABase(x, 10));
}

BigUnsigned stringToBigUnsigned(const std::string &s) {
	return BigUnsigned(BigUnsignedInABase(s, 10));
}

std::ostream &operator <<(std::ostream &os, const BigUnsigned &x) {
	BigUnsignedInABase::Base base;
	int32_t osFlags = os.flags();
	if (osFlags & os.dec)
		base = 10;
	else if (osFlags & os.hex) {
		base = 16;
		if (osFlags & os.showbase)
			os << "0x"s;
	} else if (osFlags & os.oct) {
		base = 8;
		if (osFlags & os.showbase)
			os << '0';
	} else
		throw "std::ostream << BigUnsigned: Could not determine the desired base from output-stream flags";
	std::string s(BigUnsignedInABase(x, base));
	os << s;
	return os;
}
