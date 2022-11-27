////////////////////////////////////////////////////////////////////////////////////////
/*
	Debug Command Console
	Copyright 2013 Frank Force - http://www.frankforce.com

	text console where user can tweak variables or call functions
	- simple to expose a variable or callback with provided macro
	- transforms raw keyboard input into plain text
	- uses stl string stream to process user input
	- user can flip through previous input lines or search commands
	- renders via directx and dxut
*/
////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <list>
#include <vector>
#include <queue>
#include <sstream>

////////////////////////////////////////////////////////////////////////////////////////

// use this macro to add console commands for variables
#define ConsoleCommand(variable, name) static DebugConsole::Command _##name##_CONSOLE_COMMAND(variable, L#name);
#define ConsoleCommandPinned(variable, name) static DebugConsole::Command _##name##_CONSOLE_COMMAND(variable, L#name, true, false);
#define ConsoleCommandHidden(variable, name) static DebugConsole::Command _##name##_CONSOLE_COMMAND(variable, L#name, false, true);
#define ConsoleCommandPinnedHidden(variable, name) static DebugConsole::Command _##name##_CONSOLE_COMMAND(variable, L#name, true, true);

#define ConsoleCommandSimple(theType, variable, value)	\
static theType variable = value;						\
static DebugConsole::Command _##variable##_CONSOLE_COMMAND(variable, L#variable);

#define ConsoleCommandSimplePinned(theType, variable, value)	\
static theType variable = value;								\
static DebugConsole::Command _##variable##_CONSOLE_COMMAND(variable, L#variable, true, false);

#define ConsoleCommandSimpleHidden(theType, variable, value)	\
static theType variable = value;								\
static DebugConsole::Command _##variable##_CONSOLE_COMMAND(variable, L#variable, false, true);

#define ConsoleCommandSimplePinnedHidden(theType, variable, value)	\
static theType variable = value;								\
static DebugConsole::Command _##variable##_CONSOLE_COMMAND(variable, L#variable, true, true);

// shortcut for function style commands
#define ConsoleFunction(name)									\
static void _##name##CONSOLE_FUNCTION(const wstring& text);		\
ConsoleCommand(_##name##CONSOLE_FUNCTION, name);				\
static void _##name##CONSOLE_FUNCTION(const wstring& text)

// global access to the debug console
class DebugConsole& GetDebugConsole();

////////////////////////////////////////////////////////////////////////////////////////

class DebugConsole
{
public:

	DebugConsole();

	void Init();
	void Update(float delta);
	void Render();

	void Clear()			{ lines.clear(); }
	bool IsOpen() const		{ return isOpen; }
	void ParseFile(const wstring& filename, bool showFileNotFound = true);
	void Save() const;
	void SaveLog(const wstring& filenamee) const;
	void Load();
	void Pin(bool isPinned);
	void UnpinAll();
	void ParseInputLine(const wstring& input, bool fromScript = false);

	void AddLine(const wstring& text = wstring(L""));
	void AddLine(const wstring& text, const Color& color);
	void AddError(const wstring& text = wstring(L""));
	void AddFormatted(const wstring messageFormat, ...);
	void AddFormatted(const Color& color, const wstring messageFormat, ...);

	void OnKeyboard(UINT nChar);
	void Glitch(int minGlitches = 1);
	void RandomizeColors();

public:	// commands

	// nornaly this class should only be called via provided macros
	struct Command
	{
		enum Type
		{
			Type_invalid,
			Type_bool,
			Type_int8,
			Type_uint8,
			Type_int16,
			Type_uint16,
			Type_int32,
			Type_uint32,
			Type_float,
			Type_double,
			Type_intvector2,
			Type_vector2,
			Type_vector3,
			Type_color,
			Type_string,
			Type_wstring,
			Type_function
		};

		typedef void (*CallbackFunction)(const wstring&);

		Command(void* _value,		const wstring& _name, Type _type)	: name(_name), value(_value), type(_type)				{ GetDebugConsole().AddCommand(*this); }
		Command(bool& _value,		const wstring& _name, bool _isPinned = false, bool _isHidden = false)			: name(_name), value((void*)&_value), type(Type_bool)		, isPinned(_isPinned), isHidden(_isHidden) { GetDebugConsole().AddCommand(*this); }
		Command(int8& _value,		const wstring& _name, bool _isPinned = false, bool _isHidden = false)			: name(_name), value((void*)&_value), type(Type_int8)		, isPinned(_isPinned), isHidden(_isHidden) { GetDebugConsole().AddCommand(*this); }
		Command(uint8& _value,		const wstring& _name, bool _isPinned = false, bool _isHidden = false)			: name(_name), value((void*)&_value), type(Type_uint8)		, isPinned(_isPinned), isHidden(_isHidden) { GetDebugConsole().AddCommand(*this); }
		Command(int16& _value,		const wstring& _name, bool _isPinned = false, bool _isHidden = false)			: name(_name), value((void*)&_value), type(Type_int16)		, isPinned(_isPinned), isHidden(_isHidden) { GetDebugConsole().AddCommand(*this); }
		Command(uint16& _value,		const wstring& _name, bool _isPinned = false, bool _isHidden = false)			: name(_name), value((void*)&_value), type(Type_uint16)		, isPinned(_isPinned), isHidden(_isHidden) { GetDebugConsole().AddCommand(*this); }
		Command(int32& _value,		const wstring& _name, bool _isPinned = false, bool _isHidden = false)			: name(_name), value((void*)&_value), type(Type_int32)		, isPinned(_isPinned), isHidden(_isHidden) { GetDebugConsole().AddCommand(*this); }
		Command(uint32& _value,		const wstring& _name, bool _isPinned = false, bool _isHidden = false)			: name(_name), value((void*)&_value), type(Type_uint32)		, isPinned(_isPinned), isHidden(_isHidden) { GetDebugConsole().AddCommand(*this); }
		Command(float& _value,		const wstring& _name, bool _isPinned = false, bool _isHidden = false)			: name(_name), value((void*)&_value), type(Type_float)		, isPinned(_isPinned), isHidden(_isHidden) { GetDebugConsole().AddCommand(*this); }
		Command(double& _value,		const wstring& _name, bool _isPinned = false, bool _isHidden = false)			: name(_name), value((void*)&_value), type(Type_double)		, isPinned(_isPinned), isHidden(_isHidden) { GetDebugConsole().AddCommand(*this); }
		Command(IntVector2& _value,	const wstring& _name, bool _isPinned = false, bool _isHidden = false)			: name(_name), value((void*)&_value), type(Type_intvector2)	, isPinned(_isPinned), isHidden(_isHidden) { GetDebugConsole().AddCommand(*this); }
		Command(Vector2& _value,	const wstring& _name, bool _isPinned = false, bool _isHidden = false)			: name(_name), value((void*)&_value), type(Type_vector2)	, isPinned(_isPinned), isHidden(_isHidden) { GetDebugConsole().AddCommand(*this); }
		Command(Vector3& _value,	const wstring& _name, bool _isPinned = false, bool _isHidden = false)			: name(_name), value((void*)&_value), type(Type_vector3)	, isPinned(_isPinned), isHidden(_isHidden) { GetDebugConsole().AddCommand(*this); }
		Command(Color& _value,		const wstring& _name, bool _isPinned = false, bool _isHidden = false)			: name(_name), value((void*)&_value), type(Type_color)		, isPinned(_isPinned), isHidden(_isHidden) { GetDebugConsole().AddCommand(*this); }
		Command(char* _value,		const wstring& _name, bool _isPinned = false, bool _isHidden = false)			: name(_name), value((void*)_value),  type(Type_string)		, isPinned(_isPinned), isHidden(_isHidden) { GetDebugConsole().AddCommand(*this); }
		Command(WCHAR* _value,		const wstring& _name, bool _isPinned = false, bool _isHidden = false)			: name(_name), value((void*)_value),  type(Type_wstring)	, isPinned(_isPinned), isHidden(_isHidden) 
		{ 
			GetDebugConsole().AddCommand(*this); 
		}
		Command(CallbackFunction _function,	const wstring& _name)	: name(_name), value((void*)_function), type(Type_function)	{ GetDebugConsole().AddCommand(*this); }
		~Command()	{ ASSERT(true); GetDebugConsole().RemoveCommand(*this); }

		void Run(wstringstream& inputStream, bool fromScript = false, bool hideOutput = false);
		void SkipWhitespace(wstringstream& inputStream);
		
		wstring GetValuePrint() const;
		wstring GetValueString() const;
		bool Glitch();

		wstring name;
		void* value;
		Type type;
		wstring defaultValueString;
		bool isPinned = false;
		bool isHidden = false;
		bool wasModified = false;
	};

	Command* FindCommand(const wstring& name);
	void AddCommand(Command& command);
	void RemoveCommand(Command& command);

	list<Command*> commandList;

private:	// input processing

	void ProcessInput();
	void UpdateSkiGame();

	bool ReadNextKey(UINT& key) 
	{ 
		if (keyBuffer.empty())
			return false;
		key = keyBuffer.front();
		keyBuffer.pop();
		return true;
	}

	// keep a buffer each time a key is pushed
	queue<UINT> keyBuffer;

private:	// console lines

	struct ConsoleLine
	{
		ConsoleLine(const wstring& _text, const Color& _color = Color::White()) : text(_text), color(_color) {}

		wstring text;
		Color color;
	};

	static const int maxLines = 512;
	vector<ConsoleLine> lines;			// list of all the input lines in the buffer
	vector<ConsoleLine> linesLog;		// list of all the previous console lines
	wstring inputLine;					// current input line being typed by the user

	vector<Command*> findMatches;		// all matches found when user pressed search (tab)
	vector<wstring> lineMemory;			// list of user's previous commands
	unsigned int lineMemoryPos = 0;		// user can flip through previous commands

	bool isOpen = false;				// is console curretly open
	float openPercent = 0;				// used to move console into view when it is opened
	int lineOffset = 0;					// line offset is used with page up/down keys
	bool hasBeenGliched = false;		// has the glitch command been run?

public:

	static bool enable;					// allow use of the console
	static bool saveLog;				// save console log when user quits
};