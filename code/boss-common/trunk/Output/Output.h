/*	Better Oblivion Sorting Software
	
	A "one-click" program for users that quickly optimises and avoids 
	detrimental conflicts in their TES IV: Oblivion, Nehrim - At Fate's Edge, 
	TES V: Skyrim, Fallout 3 and Fallout: New Vegas mod load orders.

    Copyright (C) 2011  Random/Random007/jpearce, WrinklyNinja & the BOSS 
	development team. Copyright license:
    http://creativecommons.org/licenses/by-nc-nd/3.0/

	$Revision: 2188 $, $Date: 2011-01-20 10:05:16 +0000 (Thu, 20 Jan 2011) $
*/

//Contains the various functions/classes required for varied BOSSlog output formattings,etc.

#ifndef __BOSS_BOSSLOG_H__
#define __BOSS_BOSSLOG_H__

#include <string>
#include <fstream>
#include <iostream>
#include <sstream>

#include <boost/format.hpp>

#include "Common/Classes.h"
#include "Common/DllDef.h"

namespace boss {
	using namespace std;

	//Default filter options.
	BOSS_COMMON_EXP extern bool UseDarkColourScheme;
	BOSS_COMMON_EXP extern bool HideVersionNumbers;
	BOSS_COMMON_EXP extern bool HideGhostedLabel;
	BOSS_COMMON_EXP extern bool HideChecksums;
	BOSS_COMMON_EXP extern bool HideMessagelessMods;
	BOSS_COMMON_EXP extern bool HideGhostedMods;
	BOSS_COMMON_EXP extern bool HideCleanMods;
	BOSS_COMMON_EXP extern bool HideRuleWarnings;
	BOSS_COMMON_EXP extern bool HideAllModMessages;
	BOSS_COMMON_EXP extern bool HideNotes;
	BOSS_COMMON_EXP extern bool HideBashTagSuggestions;
	BOSS_COMMON_EXP extern bool HideRequirements;
	BOSS_COMMON_EXP extern bool HideIncompatibilities;
	BOSS_COMMON_EXP extern bool HideDoNotCleanMessages;

	//Default CSS.
	BOSS_COMMON_EXP extern string CSSBody;
	BOSS_COMMON_EXP extern string CSSDarkBody;
	BOSS_COMMON_EXP extern string CSSDarkLink;
	BOSS_COMMON_EXP extern string CSSDarkLinkVisited;
	BOSS_COMMON_EXP extern string CSSFilters;
	BOSS_COMMON_EXP extern string CSSFiltersList;
	BOSS_COMMON_EXP extern string CSSDarkFilters;
	BOSS_COMMON_EXP extern string CSSTitle;
	BOSS_COMMON_EXP extern string CSSCopyright;
	BOSS_COMMON_EXP extern string CSSSections;
	BOSS_COMMON_EXP extern string CSSSectionTitle;
	BOSS_COMMON_EXP extern string CSSSectionPlusMinus;
	BOSS_COMMON_EXP extern string CSSLastSection;
	BOSS_COMMON_EXP extern string CSSTable;
	BOSS_COMMON_EXP extern string CSSList;
	BOSS_COMMON_EXP extern string CSSListItem;
	BOSS_COMMON_EXP extern string CSSSubList;
	BOSS_COMMON_EXP extern string CSSCheckbox;
	BOSS_COMMON_EXP extern string CSSBlockquote;
	BOSS_COMMON_EXP extern string CSSError;
	BOSS_COMMON_EXP extern string CSSWarning;
	BOSS_COMMON_EXP extern string CSSSuccess;
	BOSS_COMMON_EXP extern string CSSVersion;
	BOSS_COMMON_EXP extern string CSSGhost;
	BOSS_COMMON_EXP extern string CSSCRC;
	BOSS_COMMON_EXP extern string CSSTagPrefix;
	BOSS_COMMON_EXP extern string CSSDirty;
	BOSS_COMMON_EXP extern string CSSQuotedMessage;
	BOSS_COMMON_EXP extern string CSSMod;
	BOSS_COMMON_EXP extern string CSSTag;
	BOSS_COMMON_EXP extern string CSSNote;
	BOSS_COMMON_EXP extern string CSSRequirement;
	BOSS_COMMON_EXP extern string CSSIncompatibility;

	enum logFormatting {
		DIV_OPEN,
		DIV_CLOSE,
		LINE_BREAK,
		TABLE_OPEN,
		TABLE_CLOSE,
		TABLE_ROW,
		TABLE_DATA,
		LIST_ID_RECOGNISED_OPEN,
		LIST_ID_USERLIST_MESSAGES_OPEN,
		LIST_OPEN,
		LIST_CLOSE,
		LIST_ITEM,
		LIST_ITEM_CLASS_SUCCESS,
		LIST_ITEM_CLASS_WARN,
		LIST_ITEM_CLASS_ERROR,
		LIST_ITEM_SPAN_CLASS_MOD_OPEN,
		HEADING_ID_END_OPEN,
		HEADING_OPEN,
		HEADING_CLOSE,
		PARAGRAPH,
		SPAN_CLASS_VERSION_OPEN,
		SPAN_CLASS_GHOSTED_OPEN,
		SPAN_CLASS_CRC_OPEN,
		SPAN_CLASS_ERROR_OPEN,
		SPAN_CLASS_MESSAGE_OPEN,
		SPAN_CLOSE,
		ITALIC_OPEN,
		ITALIC_CLOSE,
		BLOCKQUOTE_OPEN,
		BLOCKQUOTE_CLOSE
	};

	class Outputter {
	public:
		Outputter();
		Outputter(unsigned int format);

		void SetFormat(unsigned int format);	//Sets the formatting type of the output.
		void SetHTMLSpecialEscape(bool shouldEscape);	//Set when formatting is set, generally, but this can be used to override.
		void Clear();			//Erase all current content.

		void PrintHeader();		//Prints BOSS Log header.
		void PrintFooter();		//Prints BOSS Log footer.

		void Save(fs::path file, bool overwrite);		//Saves contents to file. 
														//Throws boss_error exception on fail.
		string AsString();				//Outputs contents as a string.
		
		Outputter& operator<< (const string s);
		Outputter& operator<< (const char * s);
		Outputter& operator<< (const char c);
		Outputter& operator<< (const logFormatting l);
		Outputter& operator<< (const int i);
		Outputter& operator<< (const unsigned int i);
		Outputter& operator<< (const bool b);
		Outputter& operator<< (const fs::path p);
		Outputter& operator<< (const Message m);
	private:
		stringstream outStream;
		unsigned int outFormat;			//The formatting type of the output.
		bool escapeHTMLSpecialChars;	//Should special characters be escaped from non-formatting input?
	
		string EscapeHTMLSpecial(string text);	//Performs the HTML escaping.
		string EscapeHTMLSpecial(char c);
	};
}
#endif