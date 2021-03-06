/*	BOSS

	A "one-click" program for users that quickly optimises and avoids
	detrimental conflicts in their TES IV: Oblivion, Nehrim - At Fate's Edge,
	TES V: Skyrim, Fallout 3 and Fallout: New Vegas mod load orders.

	Copyright (C) 2009-2012    BOSS Development Team.

	This file is part of BOSS.

	BOSS is free software: you can redistribute
	it and/or modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation, either version 3 of
	the License, or (at your option) any later version.

	BOSS is distributed in the hope that it will
	be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with BOSS.  If not, see
	<http://www.gnu.org/licenses/>.

	$Revision: 3163 $, $Date: 2011-08-21 22:03:18 +0100 (Sun, 21 Aug 2011) $
*/

#ifndef SUPPORT_HELPERS_H_
#define SUPPORT_HELPERS_H_

#include <cstdint>

#include <string>

#include <boost/filesystem.hpp>

#include "common/dll_def.h"

namespace boss {

//////////////////////////////////////////////////////////////////////////
// Helper functions
//////////////////////////////////////////////////////////////////////////

// Calculate the CRC of the given file for comparison purposes.
std::uint32_t GetCrc32(const boost::filesystem::path &filename);

// Reads an entire file into a string buffer.
void fileToBuffer(const boost::filesystem::path file, std::string &buffer);

// Converts an integer to a string using BOOST's Spirit.Karma. Faster than a stringstream conversion.
BOSS_COMMON std::string IntToString(const std::uint32_t n);

// Converts an integer to a hex string using BOOST's Spirit.Karma. Faster than a stringstream conversion.
std::string IntToHexString(const std::uint32_t n);

// Converts a boolean to a string representation (true/false)
std::string BoolToString(bool b);

// Turns "true", "false", "1", "0" into booleans.
bool StringToBool(std::string str);

// Convert a Windows-1252 string to UTF-8.
std::string From1252ToUTF8(const std::string &str);

// Convert a UTF-8 string to Windows-1252.
std::string FromUTF8To1252(const std::string &str);

// Check if registry subkey exists.
BOSS_COMMON bool RegKeyExists(std::string keyStr, std::string subkey,
                              std::string value);

// Get registry subkey value string.
std::string RegKeyStringValue(std::string keyStr, std::string subkey,
                              std::string value);

// Version class for more robust version comparisons.
class Version {
 public:
	Version();
	explicit Version(const char *ver);
	explicit Version(const std::string ver);
	explicit Version(const boost::filesystem::path file);

	std::string AsString() const;

	bool operator > (Version);
	bool operator < (Version);
	bool operator >= (Version);
	bool operator == (Version);
	bool operator != (Version);
 private:
	std::string verString;
};

}  // namespace boss
#endif  // SUPPORT_HELPERS_H_
