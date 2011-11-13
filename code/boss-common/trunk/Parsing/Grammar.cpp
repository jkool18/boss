/*	Better Oblivion Sorting Software
	
	A "one-click" program for users that quickly optimises and avoids 
	detrimental conflicts in their TES IV: Oblivion, Nehrim - At Fate's Edge, 
	TES V: Skyrim, Fallout 3 and Fallout: New Vegas mod load orders.

    Copyright (C) 2011 BOSS Development Team. Copyright license:
    http://creativecommons.org/licenses/by-nc-nd/3.0/

	$Revision: 2188 $, $Date: 2011-01-20 10:05:16 +0000 (Thu, 20 Jan 2011) $
*/

#include "Parsing/Grammar.h"
#include "Common/Globals.h"
#include "Common/Error.h"
#include "Support/Helpers.h"
#include "Support/Logger.h"
#include "Output/Output.h"

#include <boost/spirit/include/phoenix_core.hpp>
#include <boost/spirit/include/phoenix_operator.hpp>
#include <boost/spirit/home/phoenix/object/construct.hpp>
#include <boost/spirit/include/phoenix_bind.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/regex.hpp>
#include <sstream>

namespace boss {
	namespace unicode = boost::spirit::unicode;
	namespace phoenix = boost::phoenix;

	using namespace qi::labels;

	using qi::skip;
	using qi::eol;
	using qi::eoi;
	using qi::lexeme;
	using qi::on_error;
	using qi::fail;
	using qi::lit;
	using qi::omit;
	using qi::eps;
	using qi::hex;
	using qi::bool_;
	using qi::uint_;
	
	using unicode::char_;
	using unicode::no_case;
	using unicode::space;
	using unicode::xdigit;

	///////////////////////////////
	// Keyword structures
	///////////////////////////////

	masterlistMsgKey_::masterlistMsgKey_() {
		add //New Message keywords.
			("say",SAY)
			("tag",TAG)
			("req",REQ)
			("inc", INC)
			("dirty",DIRTY)
			("warn",WARN)
			("error",ERR)
			//Old message symbols.
			("?",SAY)
			("$",OOOSAY)  //OOO comment
			("^",BCSAY)  //BC comment
			("%",TAG)
			(":",REQ)
			("\"",INC) //Incompatibility
			("*",ERR) //FCOM install error.
		;
	}

	typeKey_::typeKey_() {
		add //Group keywords.
			("begingroup:",BEGINGROUP)  //Needs the colon there unfortunately.
			("endgroup:",ENDGROUP)  //Needs the colon there unfortunately.
			("endgroup",ENDGROUP)
			("\\begingroup\\:",BEGINGROUP)
			("\\endgroup\\\\",ENDGROUP)
			("mod:", MOD)  //Needs the colon there unfortunately.
			("regex:", REGEX)
		;
	}

	metaKey_::metaKey_() {
		add //Condition keywords.
			("if", IF)
			("ifnot", IFNOT)
		;
	}

	ruleKeys_::ruleKeys_() {
		add
			("add",ADD)
			("override",OVERRIDE)
			("for",FOR)
		;
	}

	messageKeys_::messageKeys_() {
		add
			("append",APPEND)
			("replace",REPLACE)
			("before",BEFORE)
			("after",AFTER)
			("top",TOP)
			("bottom",BOTTOM)
		;
	}

	///////////////////////////////
	//Skipper Grammars
	///////////////////////////////
	
	//Constructor for modlist and userlist skipper.
	Skipper::Skipper(bool skipIniComments) : Skipper::base_type(start, "skipper grammar") {

		start = 
			spc
			| UTF8
			| CComment
			| CPlusPlusComment
			| lineComment
			| iniComment
			| eof;
			
		spc = space - eol;

		UTF8 = char_("\xef") >> char_("\xbb") >> char_("\xbf"); //UTF8 BOM

		CComment = "/*" >> *(char_ - "*/") >> "*/";

		if (skipIniComments)
			iniComment = lit("#") >> *(char_ - eol);
		else
			iniComment = CComment;

		CPlusPlusComment = !(lit("http:") | lit("https:")) >> "//" >> *(char_ - eol);

		//Need to skip lines that start with '\', but only if they don't follow with EndGroup or BeginGroup.
		lineComment = 
			lit("\\")
			>> !(lit("EndGroup") | lit("BeginGroup"))
			>> *(char_ - eol);

		eof = *(spc | CComment | CPlusPlusComment | lineComment | eol) >> eoi;
	}

	///////////////////////////////
	//Modlist/Masterlist Grammar
	///////////////////////////////

	//Stores a message, should it be appropriate.
	void modlist_grammar::StoreMessage(vector<Message>& messages, Message currentMessage) {
		if (storeMessage && !currentMessage.data.empty())
			messages.push_back(currentMessage);
		return;
	}

	//Stores the given item, should it be appropriate, and records any changes to open groups.
	void modlist_grammar::StoreItem(vector<Item>& list, Item currentItem) {
		if (currentItem.type == BEGINGROUP) {
			openGroups.push_back(currentItem.name.string());
		} else if (currentItem.type == ENDGROUP) {
			openGroups.pop_back();
		}
		if (storeItem)
			list.push_back(currentItem);
		return;
	}

	//Defines the given masterlist variable, if appropriate.
	void modlist_grammar::StoreVar(const string var) {
		if (storeItem)
			setVars.insert(var);
		return;
	}

	//Stores the global message.
	void modlist_grammar::StoreGlobalMessage(const Message currentMessage) {
		if (storeMessage)
			globalMessageBuffer->push_back(currentMessage);
		return;
	}

	//MF1 compatibility function. Evaluates the MF1 FCOM conditional. Like it says on the tin.
	void modlist_grammar::EvalOldFCOMConditional(bool& result, const char var) {
		result = false;
		boost::unordered_set<string>::iterator pos = setVars.find("FCOM");
		if (var == '>' && pos != setVars.end())
				result = true;
		else if (var == '<' && pos == setVars.end())
				result = true;
		return;
	}

	//MF1 compatibility function. Evaluates the MF1 OOO/BC conditional message symbols.
	void modlist_grammar::EvalMessKey(const keyType key) {
		if (key == OOOSAY) {
			boost::unordered_set<string>::iterator pos = setVars.find("OOO");
			if (pos == setVars.end())
				storeMessage = false;
		} else if (key == BCSAY) {
			boost::unordered_set<string>::iterator pos = setVars.find("BC");
			if (pos == setVars.end())
				storeMessage = false;
		}
		currentMessageType = key;
		return;
	}

	//Checks if a masterlist variable is defined.
	void modlist_grammar::CheckVar(bool& result, const string var) {
		if (setVars.find(var) == setVars.end())
			result = false;
		else
			result = true;
		return;
	}

	//Returns the true path based on what type of file or keyword it is.
	void modlist_grammar::GetPath(fs::path& file_path, string& file) {
		if (file == "OBSE") {
			file_path = "..";
			file = "obse_1_2_416.dll";  //Don't look for the loader because steam users don't need it.
		} else if (file == "FOSE") {
			file_path = "..";
			file = "fose_loader.exe";
		} else if (file == "NVSE") {
			file_path = "..";
			file = "nvse_loader.exe";
		} else if (file == "SKSE") {
			file_path = "..";
			file = "skse_loader.exe";  //Unconfirmed.
		} else if (file == "BOSS") {
			file_path = ".";
			file = "BOSS.exe";
		} else if (file == "TES4") {
			file_path = "..";
			file = "Oblivion.exe";
		} else if (file == "TES5") {
			file_path = "..";
			file = "TESV.exe";
		} else if (file == "FO3") {
			file_path = "..";
			file = "Fallout3.exe";
		} else if (file == "FONV") {
			file_path = "..";
			file = "FalloutNV.exe";
		} else {
			fs::path p(file);
			if (Tidy(p.extension().string()) == ".dll" && p.string().find("/") == string::npos && p.string().find("\\") == string::npos) {
				if (fs::exists(data_path / "OBSE"))
					file_path = data_path / fs::path("OBSE/Plugins");  //Oblivion - OBSE plugins.
				else if (fs::exists(data_path / "FOSE"))
					file_path = data_path / fs::path("FOSE/Plugins");  //Fallout 3 - FOSE plugins.
				else if (fs::exists(data_path / "NVSE"))
					file_path = data_path / fs::path("NVSE/Plugins");  //Fallout: New Vegas - NVSE plugins.
				else if (fs::exists(data_path / "SKSE"))
					file_path = data_path / fs::path("SKSE/Plugins");  //Fallout: New Vegas - NVSE plugins.
			} else
				file_path = data_path;
		}
	}

	//Checks if the given file (plugin or dll/exe) has a version for which the comparison holds true.
	void modlist_grammar::CheckVersion(bool& result, const string var) {
		char comp = var[0];
		size_t pos = var.find("|") + 1;
		string version = var.substr(1,pos-2);
		string file = var.substr(pos);
		result = false;
		fs::path file_path;

		GetPath(file_path,file);

		Item tempItem = Item(file);
		string trueVersion;
		if (tempItem.Exists()) {
			trueVersion = tempItem.GetHeader();
		} else if (fs::exists(file_path / file))
			trueVersion = GetExeDllVersion(file_path / file);
		else
			return;

		switch (comp) {
		case '>':
			if (trueVersion.compare(version) > 0)
				result = true;
			break;
		case '<':
			if (trueVersion.compare(version) < 0)
				result = true;
			break;
		case '=':
			if (version == trueVersion)
				result = true;
			break;
		}
		return;
	}

	//Checks if the given mod has the given checksum.
	void modlist_grammar::CheckSum(bool& result, const unsigned int sum, string file) {
		result = false;
		fs::path file_path;
		unsigned int CRC;

		GetPath(file_path,file);
		boost::unordered_map<string,unsigned int>::iterator iter = fileCRCs.find(file);

		if (iter != fileCRCs.end()) {
			CRC = fileCRCs.at(file);
		} else {
			if (fs::exists(file_path / file))
				CRC = GetCrc32(file_path / file);
			else if (Item(file).IsGhosted())
				CRC = GetCrc32(file_path / fs::path(file + ".ghost"));
			else 
				return;

			fileCRCs.emplace(file,CRC);
		}

		if (sum == CRC)
			result = true;
		return;
	}

	//Checks if the given file exists.
	void modlist_grammar::CheckFile(bool& result, string file) {
		result = false;
		fs::path file_path;
		GetPath(file_path,file);
		result = (fs::exists(file_path / file) || Item(file).IsGhosted());
	}

	//Checks if a file which matches the given regex exists.
	//This might not work when the regex specifies a file and a path, eg. "path/to/file.txt", because characters like '.' need to be escaped in regex
	//so the regex would be "path/to/file\.txt". boost::filesystem might interpret that as a path of "path / to / file / .txt" though.
	//In windows, the above path would be "path\to\file.txt", which would become "path\\to\\file\.txt" in regex. Basically, the extra backslashes need to
	//be removed when getting the path and filename.
	void modlist_grammar::CheckRegex(bool& result, string reg) {
		result = false;
		fs::path file_path;
		//If the regex includes '/' or '\\' then it includes folders. Need to split the regex into the parent path and the filename.
		//No reason for the regex to include both.
		if (reg.find("/") != string::npos) {
			size_t pos1 = reg.rfind("/");
			string p = reg.substr(0,pos1);
			reg = reg.substr(pos1+1);
			file_path = fs::path(p);
		} else if (reg.find("\\\\") != string::npos) {
			size_t pos1 = reg.rfind("\\\\");
			string p = reg.substr(0,pos1);
			reg = reg.substr(pos1+2);
			boost::algorithm::replace_all(p,"\\\\","\\");
			file_path = fs::path(p);
		} else if (Tidy(fs::path(reg).extension().string()) == ".dll") {
			if (fs::exists(data_path / "OBSE"))
				file_path = data_path / fs::path("OBSE/Plugins");  //Oblivion - OBSE plugins.
			else if (fs::exists(data_path / "FOSE"))
				file_path = data_path / fs::path("FOSE/Plugins");  //Fallout 3 - FOSE plugins.
			else if (fs::exists(data_path / "NVSE"))
				file_path = data_path / fs::path("NVSE/Plugins");  //Fallout: New Vegas - NVSE plugins.
			else if (fs::exists(data_path / "SKSE"))
				file_path = data_path / fs::path("SKSE/Plugins");  //Fallout: New Vegas - NVSE plugins.
		} else
			file_path = data_path;
		const boost::regex regex(reg);

		fs::directory_iterator iter_end;
		for (fs::directory_iterator itr(file_path); itr!=iter_end; ++itr) {
			if (fs::is_regular_file(itr->status())) {
				if (boost::regex_match(itr->path().filename().string(),regex)) {
					result = true;
					break;
				}
			}
		}
	}

	//Evaluate a single conditional.
	void modlist_grammar::EvaluateConditional(bool& result, const metaType type, const bool condition) {
		result = false;
		if (type == IF && condition == true)
			result = true;
		else if (type == IFNOT && condition == false)
			result = true;
		return;
	}

	//Evaluate the second half of a complex conditional.
	void modlist_grammar::EvaluateCompoundConditional(bool& result, const string andOr, const bool condition) {
		if (andOr == "||" && condition == true)
			result = true;
		else if (andOr == "&&" && result == true && condition == false)
			result = false;
	}

	//Converts a hex string to an integer using BOOST's Spirit.Qi. Faster than a stringstream conversion.
	unsigned int modlist_grammar::HexStringToInt(string str) {
		string::const_iterator begin, end;
		begin = str.begin();
		end = str.end();
		unsigned int out;
		qi::parse(begin, end, hex[phoenix::ref(out) = _1]);
		return out;
	}

	//Evaluate part of a shorthand conditional message.
	//Most message types would make sense for the message to display if the condition evaluates to true. (eg. incompatibilities)
	//Requirement messages need the condition to eval to false.
	void modlist_grammar::EvaluateConditionalMessage(string& message, string version, string file, const string mod) {
		bool addItem = false;

		if (file[0] == '\"') {  //It's a file.
			file = file.substr(1,file.length()-2);  //Cut off the quotes.
					
			fs::path file_path;
			GetPath(file_path,file);
			if (fs::exists(file_path / file)) {  //File exists. Was a version or checksum given? 
				
				if (!version.empty()) {
					bool versionCheck;
					if (version[0] == '>' || version[0] == '=' || version[0] == '<')  //Version
						CheckVersion(versionCheck, version + "|" + file);
					else
						CheckSum(versionCheck, HexStringToInt(version), file);
					if (versionCheck && currentMessageType != REQ)
						addItem = true;
					else if (!versionCheck && currentMessageType == REQ)
						addItem = true;
				} else if (currentMessageType != REQ)  //No version or checksum given. File exists, condition is true.
					addItem = true;
			} else if (currentMessageType == REQ)  //File isn't installed.
					addItem = true;
		} else if (file[0] == '$') {  //File is actually a masterlist variable.
			file = file.substr(1);  //Cut off the '$'.
			bool varExists;
			CheckVar(varExists, file);
			if (varExists && currentMessageType != REQ)
				addItem = true;
			else if (!varExists && currentMessageType == REQ)
				addItem = true;
		} else {  //File is actually a regex.
			file = file.substr(2,file.length()-3); //Cut off starting r" and ending ".
			bool regexMatch;
			CheckRegex(regexMatch, file);
			if (regexMatch && currentMessageType != REQ)
				addItem = true;
			else if (!regexMatch && currentMessageType == REQ)
				addItem = true;
		}
		if (addItem) {
			if (!message.empty())
				message += ", ";
			if (!mod.empty())  //Was a valid mod name given?
				message += "\"" + mod + "\"";
			else
				message += "\"" + file + "\"";
		}
	}

	//Turns a given string into a path. Can't be done directly because of the openGroups checks.
	void modlist_grammar::path(fs::path& p, string itemName) {
		boost::algorithm::trim(itemName);
		if (itemName.length() == 0 && !openGroups.empty()) 
			p = fs::path(openGroups.back());
		else
			p = fs::path(itemName);
		return;
	}

	modlist_grammar::modlist_grammar() : modlist_grammar::base_type(modList, "modlist grammar") {

		storeItem = true;
		storeMessage = true;
		errorBuffer = NULL;
		masterlistMsgKey_ masterlistMsgKey;
		typeKey_ typeKey;
		metaKey_ metaKey;

		modList = 
			(
				metaLine		[phoenix::bind(&modlist_grammar::StoreVar, this, _1)] 
				| globalMessage	[phoenix::bind(&modlist_grammar::StoreGlobalMessage, this, _1)] 
				| listItem		[phoenix::bind(&modlist_grammar::StoreItem, this, _val, _1)]
			) % +eol;

		metaLine =
			omit[conditionals[phoenix::ref(storeItem) = _1]]
			>> no_case[lit("set")]
			>> (lit(":")
			> charString);

		globalMessage =
			omit[conditionals[phoenix::ref(storeMessage) = _1]]
			>> no_case[lit("global")]
			>> (messageKeyword
			> lit(":")
			> messageString);

		listItem %= 
			omit[(oldConditional | conditionals)[phoenix::ref(storeItem) = _1]]
			> ItemType
			> itemName
			> itemMessages;

		ItemType %= no_case[typeKey] | eps[_val = MOD];

		itemName = 
			charString[phoenix::bind(&modlist_grammar::path, this, _val, _1)]
			| eps[phoenix::bind(&modlist_grammar::path, this, _val, "")];

		itemMessages = 
			(+eol
			>> itemMessage[phoenix::bind(&modlist_grammar::StoreMessage, this, _val, _1)] % +eol)
			| eps[_1 = noMessages];

		itemMessage %= 
			omit[(oldConditional | conditionals)[phoenix::ref(storeMessage) = _1]]
			>>(messageKeyword[phoenix::bind(&modlist_grammar::EvalMessKey, this, _1)]
			> -lit(":")
			> messageString);

		charString %= lexeme[+(char_ - eol)]; //String, with no skipper.

		messageString = 
			((messageVersionCRC
			>> messageModVariable
			>> messageModString
			)[phoenix::bind(&modlist_grammar::EvaluateConditionalMessage, this, _val, _1, _2, _3)] % (lit("|") | lit(",")))	//Conditional message.
			| charString[_val = _1];				//Any other message

		messageVersionCRC %=
			(
				(lexeme[('"' >> (char_('=') | char_('>') | char_('<')) > +(char_ - '"') > lit('"'))] 
				| +(xdigit - ':')) 
				>> ':'
			) 
			| "";

		messageModVariable %= 
			lexeme[(char_('"') > +(char_ - '"') > char_('"'))]
			| (char_('$') > +(char_ - '='))
			| (no_case[char_('r')] >> lexeme[char_('"') > +(char_ - '"') > char_('"')]);

		messageModString %=
			(lit("=") >> file) 
			| "";

		messageKeyword %= no_case[masterlistMsgKey];

		oldConditional = (char_(">") |  char_("<"))		[phoenix::bind(&modlist_grammar::EvalOldFCOMConditional, this, _val, _1)];

		conditionals = 
			(conditional[_val = _1] 
			> *((andOr > conditional)			[phoenix::bind(&modlist_grammar::EvaluateCompoundConditional, this, _val, _1, _2)]))
			| eps[_val = true];

		andOr %= unicode::string("&&") | unicode::string("||");

		conditional = (no_case[metaKey] > '(' > condition > ')')	[phoenix::bind(&modlist_grammar::EvaluateConditional, this, _val, _1, _2)];

		condition = 
			variable									[phoenix::bind(&modlist_grammar::CheckVar, this, _val, _1)]
			| version									[phoenix::bind(&modlist_grammar::CheckVersion, this, _val, _1)]
			| (hex > '|' > file)						[phoenix::bind(&modlist_grammar::CheckSum, this, _val, _1, _2)] //A CRC-32 checksum, as calculated by BOSS, followed by the file it applies to.
			| file										[phoenix::bind(&modlist_grammar::CheckFile, this, _val, _1)]
			| regexFile									[phoenix::bind(&modlist_grammar::CheckRegex, this, _val, _1)]
			;

		variable %= '$' > +(char_ - ')');  //A masterlist variable, prepended by a '$' character to differentiate between vars and mods.

		file %= lexeme['"' > +(char_ - '"') > '"'];  //An OBSE plugin or a mod plugin.

		version %=   //A version, followed by the mod it applies to.
			(char_('=') | char_('>') | char_('<'))
			> lexeme[+(char_ - '|')]
			> char_('|')
			> file;

		regexFile %= no_case['r'] > file;

		modList.name("modList");
		metaLine.name("metaLine");
		listItem.name("listItem");
		ItemType.name("ItemType");
		itemName.name("itemName");
		itemMessages.name("itemMessages");
		itemMessage.name("itemMessage");
		charString.name("charString");
		messageString.name("messageString");
		messageKeyword.name("messageKeyword");
		oldConditional.name("oldConditional");
		conditionals.name("conditional");
		andOr.name("andOr");
		conditional.name("conditional");
		condition.name("condition");
		variable.name("variable");
		file.name("file");
		version.name("version");
		messageVersionCRC.name("conditional shorthand version/CRC");
		messageModString.name("conditional shorthand mod");
			
		on_error<fail>(modList,				phoenix::bind(&modlist_grammar::SyntaxError, this, _1, _2, _3, _4));
		on_error<fail>(metaLine,			phoenix::bind(&modlist_grammar::SyntaxError, this, _1, _2, _3, _4));
		on_error<fail>(listItem,			phoenix::bind(&modlist_grammar::SyntaxError, this, _1, _2, _3, _4));
		on_error<fail>(ItemType,			phoenix::bind(&modlist_grammar::SyntaxError, this, _1, _2, _3, _4));
		on_error<fail>(itemName,			phoenix::bind(&modlist_grammar::SyntaxError, this, _1, _2, _3, _4));
		on_error<fail>(itemMessages,		phoenix::bind(&modlist_grammar::SyntaxError, this, _1, _2, _3, _4));
		on_error<fail>(itemMessage,			phoenix::bind(&modlist_grammar::SyntaxError, this, _1, _2, _3, _4));
		on_error<fail>(charString,			phoenix::bind(&modlist_grammar::SyntaxError, this, _1, _2, _3, _4));
		on_error<fail>(messageString,		phoenix::bind(&modlist_grammar::SyntaxError, this, _1, _2, _3, _4));
		on_error<fail>(messageVersionCRC,	phoenix::bind(&modlist_grammar::SyntaxError, this, _1, _2, _3, _4));
		on_error<fail>(messageModString,	phoenix::bind(&modlist_grammar::SyntaxError, this, _1, _2, _3, _4));
		on_error<fail>(messageModVariable,	phoenix::bind(&modlist_grammar::SyntaxError, this, _1, _2, _3, _4));
		on_error<fail>(messageKeyword,		phoenix::bind(&modlist_grammar::SyntaxError, this, _1, _2, _3, _4));
		on_error<fail>(oldConditional,		phoenix::bind(&modlist_grammar::SyntaxError, this, _1, _2, _3, _4));
		on_error<fail>(conditionals,		phoenix::bind(&modlist_grammar::SyntaxError, this, _1, _2, _3, _4));
		on_error<fail>(andOr,				phoenix::bind(&modlist_grammar::SyntaxError, this, _1, _2, _3, _4));
		on_error<fail>(conditional,			phoenix::bind(&modlist_grammar::SyntaxError, this, _1, _2, _3, _4));
		on_error<fail>(condition,			phoenix::bind(&modlist_grammar::SyntaxError, this, _1, _2, _3, _4));
		on_error<fail>(variable,			phoenix::bind(&modlist_grammar::SyntaxError, this, _1, _2, _3, _4));
		on_error<fail>(file,				phoenix::bind(&modlist_grammar::SyntaxError, this, _1, _2, _3, _4));
		on_error<fail>(version,				phoenix::bind(&modlist_grammar::SyntaxError, this, _1, _2, _3, _4));

	}

	void modlist_grammar::SyntaxError(string::const_iterator const& /*first*/, string::const_iterator const& last, string::const_iterator const& errorpos, boost::spirit::info const& what) {
		if (errorBuffer == NULL || !errorBuffer->Empty())
			return;
		
		ostringstream out;
		out << what;
		string expect = out.str();

		string context(errorpos, min(errorpos +50, last));
		boost::trim_left(context);

		ParsingError e(str(MasterlistParsingErrorHeader % expect), context, MasterlistParsingErrorFooter);
		*errorBuffer = e;
		LOG_ERROR(e.FormatFor(PLAINTEXT).c_str());
		return;
	}

	////////////////////////////
	//Ini Grammar.
	////////////////////////////

	string currentHeading;  //The current ini section heading.

	//Set the boolean BOSS variable values while parsing.
	void ini_grammar::SetBoolVar(string& var, const bool& value) {
		boost::algorithm::trim(var);  //Make sure there are no preceding or trailing spaces.
		if (currentHeading == "BOSS.GeneralSettings") {
			if (var == "bDoStartupUpdateCheck")
				do_startup_update_check = value;
			else if (var == "bUseUserRulesEditor")
				use_user_rules_editor = value;
		} else if (currentHeading == "BOSS.RunOptions") {
			if (var == "bUpdateMasterlist")
				update = value;
			else if (var == "bOnlyUpdateMasterlist")
				update_only = value;
			else if (var == "bSilentRun")
				silent = value;
			else if (var == "bNoVersionParse")
				skip_version_parse = value;
			else if (var == "bDebugWithSourceRefs")
				debug_with_source = value;
			else if (var == "bDisplayCRCs")
				show_CRCs = value;
			else if (var == "bDoTrialRun")
				trial_run = value;
			else if (var == "bLogDebugOutput")
				log_debug_output = value;
		} else if (currentHeading == "BOSSLog.Filters") {
			if (var == "bUseDarkColourScheme")
				UseDarkColourScheme = value;
			else if (var == "bHideVersionNumbers")
				HideVersionNumbers = value;
			else if (var == "bHideGhostedLabel")
				HideGhostedLabel = value;
			else if (var == "bHideChecksums")
				HideChecksums = value;
			else if (var == "bHideMessagelessMods")
				HideMessagelessMods = value;
			else if (var == "bHideGhostedMods")
				HideGhostedMods = value;
			else if (var == "bHideCleanMods")
				HideCleanMods = value;
			else if (var == "bHideRuleWarnings")
				HideRuleWarnings = value;
			else if (var == "bHideAllModMessages")
				HideAllModMessages = value;
			else if (var == "bHideNotes")
				HideNotes = value;
			else if (var == "bHideBashTagSuggestions")
				HideBashTagSuggestions = value;
			else if (var == "bHideRequirements")
				HideRequirements = value;
			else if (var == "bHideIncompatibilities")
				HideIncompatibilities = value;
			else if (var == "bHideDoNotCleanMessages")
				HideDoNotCleanMessages = value;
		}
	}

	//Set the integer BOSS variable values while parsing.
	void ini_grammar::SetIntVar(string& var, const unsigned int& value) {
		boost::algorithm::trim(var);  //Make sure there are no preceding or trailing spaces.
		if (currentHeading == "BOSS.InternetSettings") {
			if (var == "iProxyPort")
				proxy_port = value;
		} else if (currentHeading == "BOSS.RunOptions") {
			if (var == "iRevertLevel") {
				if (value >= 0 && value < 3)
					revert = value;
			} else if (var == "iDebugVerbosity") {
				if (value >= 0 && value < 4)
					debug_verbosity = value;
			} else if (var == "iRunType") {
				if (value >= 0 && value < 3)
					run_type = value;
			}
		}
	}

	//Set the BOSS variable values while parsing.
	void ini_grammar::SetStringVar(string& var, string& value) {
		boost::algorithm::trim(var);  //Make sure there are no preceding or trailing spaces.
		boost::algorithm::trim(value);  //Make sure there are no preceding or trailing spaces.
		if (currentHeading == "BOSS.InternetSettings") {
			if (var == "sProxyHostname")
				proxy_host = value;
			else if (var == "sProxyUsername")
				proxy_user = value;
			else if (var == "sProxyPassword")
				proxy_passwd = value;
		} else if (currentHeading == "BOSS.RunOptions") {
			if (var == "sBOSSLogFormat") {
				if (value == "html")
					log_format = HTML;
				else
					log_format = PLAINTEXT;
			} else if (var == "sGame") {
				if (value == "auto")
					game = AUTODETECT;
				else if (value == "Oblivion")
					game = OBLIVION;
				else if (value == "Nehrim")
					game = NEHRIM;
				else if (value == "Fallout3")
					game = FALLOUT3;
				else if (value == "FalloutNV")
					game = FALLOUTNV;
				else if (value == "Skyrim")
					game = SKYRIM;
			}
		} else if (currentHeading == "BOSSLog.Styles") {	
			if (value.empty())
				return;
			else if (var == "body")
				CSSBody = value;
			else if (var == "#darkBody")
				CSSDarkBody = value;
			else if (var == ".darkLink:link")
				CSSDarkLink = value;
			else if (var == ".darkLink:visited")
				CSSDarkLinkVisited = value;
			else if (var == "#filters")
				CSSFilters = value;
			else if (var == "#filters > li")
				CSSFiltersList = value;
			else if (var == "#darkFilters")
				CSSDarkFilters = value;
			else if (var == "body > div:first-child")
				CSSTitle = value;
			else if (var == "body > div:first-child + div")
				CSSCopyright = value;
			else if (var == "h3 + *")
				CSSSections = value;
			else if (var == "h3")
				CSSSectionTitle = value;
			else if (var == "h3 > span")
				CSSSectionPlusMinus = value;
			else if (var == "#end")
				CSSLastSection = value;
			else if (var == "td")
				CSSTable = value;
			else if (var == "ul")
				CSSList = value;
			else if (var == "ul li")
				CSSListItem = value;
			else if (var == "li ul")
				CSSSubList = value;
			else if (var == "input[type='checkbox']")
				CSSCheckbox = value;
			else if (var == "blockquote")
				CSSBlockquote = value;
			else if (var == ".error")
				CSSError = value;
			else if (var == ".warn")
				CSSWarning = value;
			else if (var == ".success")
				CSSSuccess = value;
			else if (var == ".version")
				CSSVersion = value;
			else if (var == ".ghosted")
				CSSGhost = value;
			else if (var == ".crc")
				CSSCRC = value;
			else if (var == ".tagPrefix")
				CSSTagPrefix = value;
			else if (var == ".dirty")
				CSSDirty = value;
			else if (var == ".message")
				CSSQuotedMessage = value;
			else if (var == ".mod")
				CSSMod = value;
			else if (var == ".tag")
				CSSTag = value;
			else if (var == ".note")
				CSSNote = value;
			else if (var == ".req")
				CSSRequirement = value;
			else if (var == ".inc")
				CSSIncompatibility = value;
		}
	}

	ini_grammar::ini_grammar() : ini_grammar::base_type(ini, "ini grammar") {

		errorBuffer = NULL;

		ini = *eol
				> (heading[phoenix::ref(currentHeading) = _1] | (!lit('[') >> setting)) % +eol
				> *eol;

		heading %= '[' > +(char_ - ']') > ']';

		setting =
				((var > '=') >> uint_)[phoenix::bind(&ini_grammar::SetIntVar, this, _1, _2)]
				| ((var > '=') >> bool_)[phoenix::bind(&ini_grammar::SetBoolVar, this, _1, _2)]
				| ((var > '=') > stringVal)[phoenix::bind(&ini_grammar::SetStringVar, this, _1, _2)];

		var %=
			lexeme[
				('"' > +(char_ - '"') > '"')
				|
				+(char_ - '=')
			];

		stringVal %= lexeme[*(char_ - eol)];
		
		//Give each rule names.
		ini.name("ini");
		section.name("section");
		heading.name("heading");
		setting.name("setting");
		var.name("variable");
		stringVal.name("string value");
		
		//Error handling.
		on_error<fail>(ini,			phoenix::bind(&ini_grammar::SyntaxError,this,_1,_2,_3,_4));
		on_error<fail>(section,		phoenix::bind(&ini_grammar::SyntaxError,this,_1,_2,_3,_4));
		on_error<fail>(heading,		phoenix::bind(&ini_grammar::SyntaxError,this,_1,_2,_3,_4));
		on_error<fail>(setting,		phoenix::bind(&ini_grammar::SyntaxError,this,_1,_2,_3,_4));
		on_error<fail>(var,			phoenix::bind(&ini_grammar::SyntaxError,this,_1,_2,_3,_4));
		on_error<fail>(stringVal,	phoenix::bind(&ini_grammar::SyntaxError,this,_1,_2,_3,_4));
	}

	void ini_grammar::SyntaxError(string::const_iterator const& /*first*/, string::const_iterator const& last, string::const_iterator const& errorpos, info const& what) {
		if (errorBuffer == NULL || !errorBuffer->Empty())
			return;
		
		ostringstream out;
		out << what;
		string expect = out.str();

		string context(errorpos, min(errorpos +50, last));
		boost::trim_left(context);

		ParsingError e(str(IniParsingErrorHeader % expect), context, IniParsingErrorFooter);
		*errorBuffer = e;
		LOG_ERROR(e.FormatFor(PLAINTEXT).c_str());
		return;
	}

	////////////////////////////
	//RuleList Grammar.
	////////////////////////////

	//Rule checker function, checks for syntax (not parsing) errors.
	void userlist_grammar::RuleSyntaxCheck(vector<Rule>& userlist, Rule currentRule) {
		bool skip = false;
		boost::algorithm::trim(currentRule.ruleObject);  //Make sure there are no preceding or trailing spaces.
		string ruleKeyString = currentRule.KeyToString();
		Item ruleObject = Item(currentRule.ruleObject);
		try {
			if (ruleObject.IsPlugin()) {
				if (ruleKeyString != "FOR" && ruleObject.IsMasterFile())
					throw ParsingError((RuleListSyntaxErrorMessage % ruleKeyString % ruleObject.name.string() % ESortingMasterEsm).str());
			} else {
				if (Tidy(ruleObject.name.string()) == "esms")
					throw ParsingError((RuleListSyntaxErrorMessage % ruleKeyString % ruleObject.name.string() % ESortingGroupEsms).str());
				if (ruleKeyString == "ADD" && !ruleObject.IsPlugin())
					throw ParsingError((RuleListSyntaxErrorMessage % ruleKeyString % ruleObject.name.string() % EAddingModGroup).str());
				else if (ruleKeyString == "FOR")
					throw ParsingError((RuleListSyntaxErrorMessage % ruleKeyString % ruleObject.name.string() % EAttachingMessageToGroup).str());
			}
			size_t size = currentRule.lines.size();
			bool hasSortLine = false, hasReplaceLine = false;
			for (size_t i=0; i<size; i++) {
				boost::algorithm::trim(currentRule.lines[i].object);  //Make sure there are no preceding or trailing spaces.
				Item subject = Item(currentRule.lines[i].object);
				if (currentRule.lines[i].key == BEFORE || currentRule.lines[i].key == AFTER) {
					if (hasSortLine)
						throw ParsingError((RuleListSyntaxErrorMessage % ruleKeyString % ruleObject.name.string() % EMultipleSortLines).str());
					if (i != 0)
						throw ParsingError((RuleListSyntaxErrorMessage % ruleKeyString % ruleObject.name.string() % ESortNotSecond).str());
					if (ruleKeyString == "FOR")
						throw ParsingError((RuleListSyntaxErrorMessage % ruleKeyString % ruleObject.name.string() % ESortLineInForRule).str());
					if (ruleObject.name == subject.name)
						throw ParsingError((RuleListSyntaxErrorMessage % ruleKeyString % ruleObject.name.string() % ESortingToItself).str());
					if ((ruleObject.IsPlugin() && !subject.IsPlugin()) || (!ruleObject.IsPlugin() && subject.IsPlugin()))
						throw ParsingError((RuleListSyntaxErrorMessage % ruleKeyString % ruleObject.name.string() % EReferencingModAndGroup).str());
					if (currentRule.lines[i].key == BEFORE) {
						if (Tidy(subject.name.string()) == "esms")
							throw ParsingError((RuleListSyntaxErrorMessage % ruleKeyString % ruleObject.name.string() % ESortingGroupBeforeEsms).str());
						else if (subject.IsMasterFile())
							throw ParsingError((RuleListSyntaxErrorMessage % ruleKeyString % ruleObject.name.string() % ESortingModBeforeGameMaster).str());
					}
					hasSortLine = true;
				} else if (currentRule.lines[i].key == TOP || currentRule.lines[i].key == BOTTOM) {
					if (hasSortLine)
						throw ParsingError((RuleListSyntaxErrorMessage % ruleKeyString % ruleObject.name.string() % EMultipleSortLines).str());
					if (i != 0)
						throw ParsingError((RuleListSyntaxErrorMessage % ruleKeyString % ruleObject.name.string() % ESortNotSecond).str());
					if (ruleKeyString == "FOR")
						throw ParsingError((RuleListSyntaxErrorMessage % ruleKeyString % ruleObject.name.string() % ESortLineInForRule).str());
					if (currentRule.lines[i].key == TOP && Tidy(subject.name.string()) == "esms")
						throw ParsingError((RuleListSyntaxErrorMessage % ruleKeyString % ruleObject.name.string() % EInsertingToTopOfEsms).str());
					if (!ruleObject.IsPlugin() || subject.IsPlugin())
						throw ParsingError((RuleListSyntaxErrorMessage % ruleKeyString % ruleObject.name.string() % EInsertingGroupOrIntoMod).str());
					hasSortLine = true;
				} else if (currentRule.lines[i].key == APPEND || currentRule.lines[i].key == REPLACE) {
					if (!ruleObject.IsPlugin())
						throw ParsingError((RuleListSyntaxErrorMessage % ruleKeyString % ruleObject.name.string() % EAttachingMessageToGroup).str());
					if (currentRule.lines[i].key == REPLACE) {
						if (hasReplaceLine)
							throw ParsingError((RuleListSyntaxErrorMessage % ruleKeyString % ruleObject.name.string() % EMultipleReplaceLines).str());
						if ((ruleKeyString == "FOR" && i != 0) || (ruleKeyString != "FOR" && i != 1))
							throw ParsingError((RuleListSyntaxErrorMessage % ruleKeyString % ruleObject.name.string() % EReplaceNotFirst).str());
						hasReplaceLine = true;
					}
				}
			}
		} catch (ParsingError & e) {
			skip = true;
			if (syntaxErrorBuffer != NULL)
				syntaxErrorBuffer->push_back(e);
			LOG_ERROR(e.FormatFor(PLAINTEXT).c_str());
		}
		if (!skip)
			userlist.push_back(currentRule);
		return;
	}

	userlist_grammar::userlist_grammar() : userlist_grammar::base_type(ruleList, "userlist grammar") {

		syntaxErrorBuffer = NULL;
		parsingErrorBuffer = NULL;
		ruleKeys_ ruleKeys;
		messageKeys_ sortOrMessageKeys;

		//A list is a vector of rules. Rules are separated by line endings.
		ruleList = 
			*eol 
			> (eoi | (userlistRule[phoenix::bind(&userlist_grammar::RuleSyntaxCheck, this, _val, _1)] % eol)); 

		//A rule consists of a rule line containing a rule keyword and a rule object, followed by one or more message or sort lines.
		userlistRule %=
			*eol
			> stateKey > ruleKey > ':' > object
			> +eol
			> sortOrMessageLine % +eol;

		sortOrMessageLine %=
			sortOrMessageKey
			> ':'
			> object;

		object %= lexeme[+(char_ - eol)]; //String, with no skipper.

		ruleKey %= no_case[ruleKeys];

		sortOrMessageKey %= no_case[sortOrMessageKeys];

		stateKey = no_case[lit("disable")][_val = false] | eps[_val = true];

		//Give each rule names.
		ruleList.name("rules");
		userlistRule.name("rule");
		sortOrMessageLine.name("sort or message line");
		object.name("line object");
		ruleKey.name("rule keyword");
		sortOrMessageKey.name("sort or message keyword");
		stateKey.name("state keyword");
		
		on_error<fail>(ruleList,			phoenix::bind(&userlist_grammar::SyntaxError,this,_1,_2,_3,_4));
		on_error<fail>(userlistRule,		phoenix::bind(&userlist_grammar::SyntaxError,this,_1,_2,_3,_4));
		on_error<fail>(sortOrMessageLine,	phoenix::bind(&userlist_grammar::SyntaxError,this,_1,_2,_3,_4));
		on_error<fail>(object,				phoenix::bind(&userlist_grammar::SyntaxError,this,_1,_2,_3,_4));
		on_error<fail>(ruleKey,				phoenix::bind(&userlist_grammar::SyntaxError,this,_1,_2,_3,_4));
		on_error<fail>(sortOrMessageKey,	phoenix::bind(&userlist_grammar::SyntaxError,this,_1,_2,_3,_4));
		on_error<fail>(stateKey,			phoenix::bind(&userlist_grammar::SyntaxError,this,_1,_2,_3,_4));
	}

	void userlist_grammar::SyntaxError(string::const_iterator const& /*first*/, string::const_iterator const& last, string::const_iterator const& errorpos, info const& what) {
		if (parsingErrorBuffer == NULL || !parsingErrorBuffer->Empty())
			return;
		
		ostringstream out;
		out << what;
		string expect = out.str();

		string context(errorpos, min(errorpos +50, last));
		boost::trim_left(context);

		ParsingError e(str(RuleListParsingErrorHeader % expect), context, RuleListParsingErrorFooter);
		*parsingErrorBuffer = e;
		LOG_ERROR(e.FormatFor(PLAINTEXT).c_str());
		return;
	}
}