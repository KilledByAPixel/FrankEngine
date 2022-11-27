////////////////////////////////////////////////////////////////////////////////////////
/*
	Debug Command Console
	Copyright 2013 Frank Force - http://www.frankforce.com
*/
////////////////////////////////////////////////////////////////////////////////////////

#include "frankEngine.h"
#include <fstream>
#include "../core/debugConsole.h"

#define CONSOLE_COLOR_INPUT		(Color(0.5f, 0.5f, 1.0f, 1.0f))
#define CONSOLE_COLOR_NORMAL	(Color(1.0f, 1.0f, 1.0f, 1.0f))
#define CONSOLE_COLOR_ERROR		(Color(1.0f, 0.5f, 0.5f, 1.0f))
#define CONSOLE_COLOR_SUCCESS	(Color(0.5f, 1.0f, 0.5f, 1.0f))
#define CONSOLE_COLOR_SPECIAL	(Color(1.0f, 1.0f, 0.5f, 1.0f))

bool DebugConsole::enable = true;
ConsoleCommandHidden(DebugConsole::enable, consoleEnable);

bool DebugConsole::saveLog = false;
ConsoleCommand(DebugConsole::saveLog, consoleSaveLogOnExit);

ConsoleCommandSimple(Color, consoleColorBack,   Color(0.0f, 0.05f, 0.0f, 0.8f))
ConsoleCommandSimple(Color, consoleColorBorder, Color(0.0f, 0.0f, 0.0f, 0.8f))
ConsoleCommandSimple(float, consoleHeight, 280);

static const wstring consoleMemoryFilename = L"consoleMemory.txt";
static const wstring consolePinnedFilename = L"consolePinned.txt";
static const wstring consoleLogFilename = L"consolLog.txt";

static bool startSkiGame = false;
ConsoleCommandSimplePinnedHidden(int, skiHighScore, 0)

////////////////////////////////////////////////////////////////////////////////////////

DebugConsole& GetDebugConsole()
{
	static DebugConsole console;
	return console;
}

DebugConsole::DebugConsole()
{
	wstring s = wstring(L"Welcome to ") + frankEngineName + wstring(L" ") + frankEngineVersion;
	AddLine(s, CONSOLE_COLOR_SPECIAL);
	AddLine();
}

void DebugConsole::Init()
{
	wstring s = frankEngineName + wstring(L" ") + frankEngineVersion;
	AddLine(L"-------------------------", CONSOLE_COLOR_SPECIAL);
	AddLine(s, CONSOLE_COLOR_SUCCESS);
	AddLine(L"-------------------------", CONSOLE_COLOR_SPECIAL);
	AddLine(L"Type 'help' for more info about the debug console.");
	AddLine();

	Load();
}

void DebugConsole::Update(float delta)
{
	if (!enable && isOpen)
	{
		// ~ key toggles console 
		isOpen = !isOpen;
		findMatches.clear();
		lineMemoryPos = 0;
		g_editorGui.ClearFocus();
		return;
	}

	const float openSpeed = 10;
	openPercent += (isOpen? 1 : -1)*delta * openSpeed;
	openPercent = CapPercent(openPercent);
	
	UpdateSkiGame();
	ProcessInput();

	while (lines.size() > maxLines)
	{
		// get rid of excess lines
		lines.pop_back();
	}
}

void DebugConsole::Save() const
{
	if (lineMemory.empty())
		return;

	{
		ofstream outFile(consoleMemoryFilename);
		if (outFile.fail())
		{
			outFile.close();
			return;
		}

		static const int maxSaveLines = 100;
		int skipLines = lineMemory.size() - maxSaveLines;
		for (const wstring& wline : lineMemory)
		{
			if (skipLines-- > 0)
				continue;

			char line[256];
			wcstombs_s(NULL, line, 256, wline.c_str(), 255);
			outFile << line << endl;
		}
		
		outFile.close();
	}
	if (!hasBeenGliched)
	{
		ofstream outFile(consolePinnedFilename);

		if (outFile.fail())
		{
			outFile.close();
			return;
		}

		for (Command* command : commandList)
		{
			if (!command->isPinned)
				continue;
			
			char line[256];
			wcstombs_s(NULL, line, 256, command->GetValuePrint().c_str(), 255);
			outFile << line << endl;
		}

		outFile.close();
	}

	if (saveLog)
		SaveLog(consoleLogFilename);
}

void DebugConsole::Load()
{
	{
		// load line memory
		ifstream inFile(consoleMemoryFilename);
		while (!inFile.eof() && !inFile.fail())
		{
			char buffer[1024];
			inFile.getline(buffer, 1024);

			if (strlen(buffer) == 0)
				continue;
		
			string line(buffer);
			std::wstring wline;
			wline.assign(line.begin(), line.end());
			lineMemory.push_back(wline);
		}
		
		inFile.close();
	}
	{
		// load pinned commands
		ifstream inFile(consolePinnedFilename);
		while (!inFile.eof() && !inFile.fail())
		{
			char buffer[1024];
			inFile.getline(buffer, 1024);

			if (strlen(buffer) == 0)
				continue;
		
			string line(buffer);
			std::wstring wline;
			wline.assign(line.begin(), line.end());

			wstringstream inputStream(wline);
			wstring name;
			inputStream >> name;

			Command* command = FindCommand(name);
			if (!command)
			{
				AddFormatted(CONSOLE_COLOR_ERROR, L"Error: Pinned Command '%s' not found.", name.c_str());
				continue;
			}

			command->isPinned = true;
			command->Run(inputStream, true, true);
		}
		
		inFile.close();
	}

}

void DebugConsole::ParseInputLine(const wstring& input, bool fromScript)
{
	wstringstream inputStream(input);
	wstring name;
	inputStream >> name;

	if (name.empty())
		return;

	// search for this command
	Command* command = FindCommand(name);
	
	static bool wasDevModeEnabled = GameControlBase::devMode;
	if (GameControlBase::devMode)
		wasDevModeEnabled = true;
	if (!wasDevModeEnabled && command && (command->name == L"devMode") && !fromScript && skiHighScore < 1000)
	{
		AddError(wstring(L"Error: Dev mode locked. Ski 1000 meters to unlock."));
		startSkiGame = true;
		return;
	}

	if (command)
		command->Run(inputStream, fromScript);
	else
		AddError(wstring(L"Command not found"));

	Save();
}

DebugConsole::Command* DebugConsole::FindCommand(const wstring& name)
{
	// do a case insensitive lookup for this command
	wstring nameLower( name );
	transform( nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower );
	
	for (Command* command : commandList)
	{
		wstring commandNameLower( command->name );
		transform( commandNameLower.begin(), commandNameLower.end(), commandNameLower.begin(), ::tolower );
		if (nameLower == commandNameLower)
			return command;
	}
	return NULL;
}

void DebugConsole::AddCommand(Command& command) 
{ 
	ASSERT(!FindCommand(command.name));
	for (Command* commandTest : commandList)
		ASSERT(commandTest->value != command.value); // dupliate command!

	command.defaultValueString = command.GetValueString();
	commandList.push_back(&command); 

	// keep commands sorted by name
	struct SortWrapper
	{
		static bool CommandListCompare(DebugConsole::Command* first, DebugConsole::Command* second) { return (first->name < second->name); }
	};
	commandList.sort(SortWrapper::CommandListCompare);
}

void DebugConsole::RemoveCommand(Command& command)
{
	for (list<Command*>::iterator it = commandList.begin(); it != commandList.end(); ++it) 
	{
		Command& testCommand = **it;
		if (testCommand.name == command.name)
		{
			commandList.erase(it);
			return;
		}
	}
}

wstring DebugConsole::Command::GetValuePrint() const
{
	const wstring valueString = GetValueString();
	wstringstream output;
	output << name;
	if (type != Type_function)
		output << L" = " << valueString;
	return output.str();
}

wstring DebugConsole::Command::GetValueString() const
{
	wstringstream output;
	switch (type)
	{
		case Type_bool:
		{
			output << *(bool*)value;
			break;
		}
		case Type_uint8:
		{
			output << *(uint8*)value;
			break;
		}
		case Type_int8:
		{
			output << *(int8*)value;
			break;
		}
		case Type_int16:
		{
			output << *(int16*)value;
			break;
		}
		case Type_uint16:
		{
			output << *(uint16*)value;
			break;
		}
		case Type_int32:
		{
			output << *(int32*)value;
			break;
		}
		case Type_uint32:
		{
			output << *(uint32*)value;
			break;
		}
		case Type_float:
		{
			output << *(float*)value;
			break;
		}
		case Type_double:
		{
			output << *(double*)value;
			break;
		}
		case Type_intvector2:
		{
			IntVector2& vector = *(IntVector2*)value;
			output << L"(" << vector.x << ", " << vector.y << ")";
			break;
		}
		case Type_vector2:
		{
			Vector2& vector = *(Vector2*)value;
			output << L"(" << vector.x << ", " << vector.y << ")";
			break;
		}
		case Type_vector3:
		{
			Vector3& vector = *(Vector3*)value;
			output << L"(" << vector.x << ", " << vector.y << ", "<< vector.z << ")";
			break;
		}
		case Type_color:
		{
			Color& color = *(Color*)value;
			output << L"(" << color.r << ", " << color.g << ", " << color.b << ", " << color.a << ")";
			break;
		}
		case Type_string:
		{
			char* string = (char*)value;
			output << L"\"" << string << L"\"";
			break;
		}
		case Type_wstring:
		{
			WCHAR* wstring = (WCHAR*)value;
			output << L"\"" << wstring << L"\"";
			break;
		}
		case Type_function:
		{
			output << "(function)";
			break;
		}
		default:
		{
			output << L"(N/A)";
			break;
		}
	}
	return output.str();
}

void DebugConsole::Command::SkipWhitespace(wstringstream& inputStream)
{
	// skip characters that can be ignored by the debug console
	while(true)
	{
		const WCHAR c = inputStream.peek();
		if (c == L' ' || c == '=' || c == '(' || c == ',' || c == '\t')
			inputStream.get();
		else
			break;
	}
}

void DebugConsole::Command::Run(wstringstream& inputStream, bool fromScript, bool hideOutput)
{
	if (type != Type_function)
		SkipWhitespace(inputStream);

	bool error = false;
	wstringstream output;
	switch (type)
	{
		case Type_bool:
		{
			inputStream >> *(bool*)value;
			break;
		}
		case Type_uint8:
		{
			uint16 temp = *(uint8*)value;
			inputStream >> temp;
			*(uint8*)value = (uint8)temp;
			break;
		}
		case Type_int8:
		{
			int16 temp = *(int8*)value;
			inputStream >> temp;
			*(int8*)value = (int8)temp;
			break;
		}
		case Type_int16:
		{
			inputStream >> *(int16*)value;
			break;
		}
		case Type_uint16:
		{
			inputStream >> *(uint16*)value;
			break;
		}
		case Type_int32:
		{
			inputStream >> *(int32*)value;
			break;
		}
		case Type_uint32:
		{
			inputStream >> *(uint32*)value;
			break;
		}
		case Type_float:
		{
			inputStream >> *(float*)value;
			break;
		}
		case Type_double:
		{
			inputStream >> *(double*)value;
			break;
		}
		case Type_intvector2:
		{
			IntVector2& vector = *(IntVector2*)value;
			inputStream >> vector.x;
			if (inputStream.fail())
				vector.x = 0;
			SkipWhitespace(inputStream);
			inputStream >> vector.y;
			if (inputStream.fail())
				vector.y = vector.x;
			break;
		}
		case Type_vector2:
		{
			Vector2& vector = *(Vector2*)value;
			inputStream >> vector.x;
			if (inputStream.fail())
				vector.x = 0;
			SkipWhitespace(inputStream);
			inputStream >> vector.y;
			if (inputStream.fail())
				vector.y = vector.x;
			break;
		}
		case Type_vector3:
		{
			Vector3& vector = *(Vector3*)value;
			inputStream >> vector.x;
			if (inputStream.fail())
				vector.x = 0;
			SkipWhitespace(inputStream);
			inputStream >> vector.y;
			if (inputStream.fail())
				vector.y = vector.x;
			SkipWhitespace(inputStream);
			inputStream >> vector.z;
			if (inputStream.fail())
				vector.z = vector.x;
			break;
		}
		case Type_color:
		{
			Color& color = *(Color*)value;
			inputStream >> color.r;
			if (inputStream.fail())
				color = Color::Black(0);
			SkipWhitespace(inputStream);
			inputStream >> color.g;
			if (inputStream.fail())
				color = Color::Grey(1, color.r);
			SkipWhitespace(inputStream);
			inputStream >> color.b;
			SkipWhitespace(inputStream);
			inputStream >> color.a;
			if (inputStream.fail())
				color.a = 1;
			break;
		}
		case Type_string:
		case Type_wstring:
		{
			wstring buffer;
			inputStream >> buffer;
			int startPos = buffer.find('\"');
			if (startPos >= 0 && buffer.length() > 1)
			{
				int quotePos = buffer.find('\"', startPos+1);
				if (quotePos > 0)
					buffer[quotePos] = '\0';

				++startPos;
			}
			else
				startPos = 0;

			if (type == Type_wstring)
			{
				WCHAR* string = (WCHAR*)value;
				wcsncpy_s(string, 256, &buffer[startPos], _TRUNCATE );
			}
			else
			{
				char* string = (char*)value;
				wcstombs_s(NULL, string, 256, &buffer[startPos], 256);
			}
			break;
		}
		case Type_function:
		{
			WCHAR buffer[1024];
			inputStream.getline(buffer, 1024, 0);
			((Command::CallbackFunction)(value))(wstring(buffer));
			break;
		}
		default:
		{
			error = true;
			break;
		}
	}

	if (type != Type_function)
		output << GetValuePrint();

	if (error)
	{
		if (!hideOutput)
			GetDebugConsole().AddError(output.str());
	}
	else if (type != Type_function)
	{
		if (!fromScript)
			wasModified = true;
		output << " ( default " << defaultValueString << " )";
		if (isPinned)
			output << " pinned";
		if (!hideOutput)
			GetDebugConsole().AddLine(output.str(), CONSOLE_COLOR_SUCCESS);
	}
}

void DebugConsole::ProcessInput()
{
	UINT keyCheck = 0;
	while (ReadNextKey(keyCheck))
	{
		if (keyCheck >= VK_F1 && keyCheck <= VK_F12 || keyCheck == VK_ESCAPE)
			continue; // skip special keys

		if (!isOpen)
			continue;	// only need to process input when open

		if (keyCheck == VK_RETURN)
		{
			if (!inputLine.empty())// && (lineMemory.empty() || *lineMemory.begin() != inputLine))
			{
				// prevent duplicate memory lines
				if (lineMemoryPos < lineMemory.size() && inputLine == lineMemory[lineMemoryPos])
					lineMemory.erase(lineMemory.begin() + lineMemoryPos);
				lineMemory.push_back(inputLine);
			}
			lineMemoryPos = lineMemory.size();
			findMatches.clear();
			AddLine(inputLine, CONSOLE_COLOR_INPUT);
			ParseInputLine(inputLine);
			inputLine.clear();
			lineOffset = 0;
			continue;
		}
		else if (keyCheck == VK_DELETE || keyCheck == VK_BACK)
		{
			// delete the last char
			if (!inputLine.empty())
				inputLine.erase(--inputLine.end());
			continue;
		}
		else if (keyCheck == VK_TAB)
		{
			findMatches.clear();

			wstring inputLower( inputLine );
			transform( inputLower.begin(), inputLower.end(), inputLower.begin(), ::tolower );
			
			for (Command* command : commandList)
			{
				if (command->isHidden)
					continue;

				wstring commandNameLower( command->name );
				transform( commandNameLower.begin(), commandNameLower.end(), commandNameLower.begin(), ::tolower );

				if (commandNameLower.find(inputLower) != string::npos)
				{
					// input matches this command, add to the match list
					findMatches.push_back(command);
				}
			}

			lineMemoryPos = 0;
			if (!findMatches.empty())
			{
				// set first match to input
				inputLine = (*findMatches.begin())->GetValuePrint();
			}

			continue;
		}
		else if (keyCheck == VK_PRIOR)
		{
			lineOffset = Cap(lineOffset + 1, 0, int(lines.size()-1));
		}
		else if (keyCheck == VK_NEXT)
		{
			lineOffset = Cap(lineOffset - 1, 0, int(lines.size()-1));
		}
		else if (keyCheck == VK_DOWN)
		{
			if (!findMatches.empty())
			{
				// go to previous line
				if (lineMemoryPos < findMatches.size() - 1)
					inputLine = findMatches[++lineMemoryPos]->GetValuePrint();
				else
					inputLine = findMatches[lineMemoryPos = 0]->GetValuePrint();
			}
			else if (!lineMemory.empty())
			{
				// go to previous line
				if (lineMemoryPos < lineMemory.size() - 1)
					inputLine = lineMemory[++lineMemoryPos];
				else
					inputLine = lineMemory[lineMemoryPos = 0];
			}
			continue;
		}
		else  if (keyCheck == VK_UP)
		{
			if (!findMatches.empty())
			{
				// go to next line
				if (lineMemoryPos > 0)
					inputLine = findMatches[--lineMemoryPos]->GetValuePrint();
				else
					inputLine = findMatches[lineMemoryPos = findMatches.size() - 1]->GetValuePrint();
			}
			else if (!lineMemory.empty())
			{
				// go to next line
				if (lineMemoryPos > 0)
					inputLine = lineMemory[--lineMemoryPos];
				else
					inputLine = lineMemory[lineMemoryPos = lineMemory.size() - 1];
			}
			continue;
		}

		UINT key = 0;

		if 
		(
			keyCheck >= 'A' && keyCheck <= 'Z' || 
			keyCheck >= 'a' && keyCheck <= 'z' ||
			keyCheck >= '0' && keyCheck <= '9' ||
			keyCheck == ' '
		)
		{
			// normal ascii key
			key = keyCheck;
		}

		switch(keyCheck)
		{
			// do windows virtual keys stuff
			case VK_OEM_MINUS:	key = '-'; break;
			case VK_OEM_PLUS:	key = '='; break;
			case VK_OEM_4:		key = '['; break;
			case VK_OEM_6:		key = ']'; break;
			case VK_OEM_1:		key = ';'; break;
			case VK_OEM_COMMA:	key = ','; break;
			case VK_OEM_PERIOD:	key = '.'; break;
			case VK_OEM_2:		key = '/'; break;
			case VK_SPACE:		key = ' '; break;
			case VK_OEM_5:		key = '\\'; break;
			case VK_OEM_7:		key = '\''; break;
		}

		if (!(GetKeyState(VK_CAPITAL) & 0x0001) && key >= 'A' && key <= 'Z')
		{
			// handle caps lock
			key += 'a' - 'A';
		}

		if ((GetKeyState(VK_SHIFT) & ~0x0001))
		{
			// handle shift
			if (key >= 'A' && key <= 'Z')
				key += 'a' - 'A';
			else if (key >= 'a' && key <= 'z')
				key -= 'a' - 'A';
			else
			{
				switch(key)
				{
					// convert key to shifted version
					case '1': key = '!'; break;
					case '2': key = '@'; break;
					case '3': key = '#'; break;
					case '4': key = '$'; break;
					case '5': key = '%'; break;
					case '6': key = '^'; break;
					case '7': key = '&'; break;
					case '8': key = '*'; break;
					case '9': key = '('; break;
					case '0': key = ')'; break;	
					case '-': key = '_'; break;
					case '=': key = '+'; break;
					case '[': key = '{'; break;
					case ']': key = '}'; break;
					case ';': key = ':'; break;
					case ',': key = '<'; break;
					case '.': key = '>'; break;
					case '/': key = '?'; break;
					case '\\': key = '|'; break;
					case '\'': key = '"'; break;
				}
			}
		}

		if (key)
		{
			// add key to end of line if it was valid
			inputLine += key;
		}
	}
}

void DebugConsole::Render()
{
	if (openPercent == 0)
		return;
	
	CDXUTPerfEventGenerator( DXUT_PERFEVENTCOLOR, L"DebugConsole::Render()" );
	const float minY = 0;
	consoleHeight = Cap(consoleHeight, 30.0f, 600.0f);
	float posY = Lerp(openPercent, minY, consoleHeight-10) - (float)g_textHelper->GetLineHeight();

	{
		// draw background
		const float w = (float)g_backBufferWidth;
		const float h = (float)g_backBufferHeight;
		const float l = (float)g_textHelper->GetLineHeight();
		const Vector2 pos(w/2, posY - consoleHeight/2 + l + 4);
		const Vector2 size(w/2, consoleHeight/2);
		DXUTGetD3D9Device()->SetRenderState(D3DRS_LIGHTING, TRUE);
		g_render->RenderScreenSpaceQuad(pos, size, consoleColorBack);

		Vector2 bottomPos = pos + Vector2(0, size.y+2);
		Vector2 bottomSize(size.x, 2);
		g_render->RenderScreenSpaceQuad(bottomPos, bottomSize, consoleColorBorder);
	}
	
	{
		// print the text
		g_textHelper->Begin();
		g_textHelper->SetInsertionPos( 20, (int)posY );

		// draw the input line
		wstring output = inputLine;
		if ((int)(3*GamePauseTimer::GetTimeGlobal()) % 2)
		{
			// add the blinking cursor
			output += '_';
		}
		g_textHelper->SetForegroundColor( CONSOLE_COLOR_INPUT );
		g_textHelper->DrawTextLine( output.c_str(), true );

		// print all the lines in the text buffer
		const int maxDrawLines = 12;
		int lineCount = 0;
		for (const ConsoleLine& line : lines)
		{
			if (++lineCount <= lineOffset)
				continue;
			if (lineCount > lineOffset + maxDrawLines)
				continue;
			g_textHelper->SetForegroundColor( line.color );
			g_textHelper->DrawTextLine( line.text.c_str(), true, false );
		}
	
		g_textHelper->End();
	}

	if (!findMatches.empty())
	{
		//vector<wstring>& showLines = findMatches.empty()? lineMemory : findMatches;

		// show the found matches
		posY += (float)g_textHelper->GetLineHeight() + 4;

		if (findMatches.size() > 1)
		{
			// draw background
			const float w = 500;
			const float h = (findMatches.size()-1) * (float)g_textHelper->GetLineHeight();
			const Vector2 pos(30 + w/2, posY + h/2 + 4);
			const Vector2 size(w/2 + 6, h/2 + 4);
			DXUTGetD3D9Device()->SetRenderState(D3DRS_LIGHTING, TRUE);
			g_render->RenderScreenSpaceQuad(pos, size, consoleColorBack);
		}

		{
			g_textHelper->Begin();
			g_textHelper->SetForegroundColor( Color::Grey(1.0f, 0.8f) );
			g_textHelper->SetInsertionPos( 30, (int)posY );

			for (unsigned int i = lineMemoryPos+1; i < findMatches.size(); ++i) 
			{
				const wstring& text = findMatches[i]->GetValuePrint();
				g_textHelper->DrawTextLine( text.c_str(), false );
			}

			for (unsigned int i = 0; i < lineMemoryPos; ++i) 
			{
				const wstring& text = findMatches[i]->GetValuePrint();
				g_textHelper->DrawTextLine( text.c_str(), false );
			}

			g_textHelper->End();
		}
	}
}

void DebugConsole::AddLine(const wstring& text, const Color& color)
{
	lines.emplace(lines.begin(), ConsoleLine(text, color));
	linesLog.emplace(linesLog.begin(), ConsoleLine(text, color));
}

void DebugConsole::AddLine(const wstring& text)
{
	AddLine(text, CONSOLE_COLOR_NORMAL);
}

void DebugConsole::AddError(const wstring& text)
{
	AddLine(text, CONSOLE_COLOR_ERROR);
}

void DebugConsole::AddFormatted(const Color& color, const wstring messageFormat, ...)
{
	// fill in the message
	va_list args;
	va_start(args, messageFormat);
	WCHAR output[256];
	StringCchVPrintf( output, 256, messageFormat.c_str(), args );
	va_end(args);
	output[255] = L'\0';

	AddLine(output, color);
}

void DebugConsole::AddFormatted(const wstring messageFormat, ...)
{
	// fill in the message
	va_list args;
	va_start(args, messageFormat);
	WCHAR output[256];
	StringCchVPrintf( output, 256, messageFormat.c_str(), args );
	va_end(args);
	output[255] = L'\0';

	AddLine(output, CONSOLE_COLOR_NORMAL);
}

void DebugConsole::OnKeyboard(UINT nChar)	
{ 
	if (enable && nChar == VK_OEM_3)
	{
		// ~ key toggles console 
		isOpen = !isOpen;
		findMatches.clear();
		lineMemoryPos = 0;
		g_editorGui.ClearFocus();
		return;
	}

	if (g_editorGui.IsEditBoxFocused())
		return;

	if (keyBuffer.size() < 256) 
		keyBuffer.push(nChar); 
}

void DebugConsole::ParseFile(const wstring& filename, bool showFileNotFound)
{		
	ifstream inFile(filename);
	
	if (inFile.fail())
	{
		if (showFileNotFound)
			AddLine(wstring(L"File \"") + filename + wstring(L"\" not found"), CONSOLE_COLOR_ERROR);
		return;
	}
	
	AddLine(wstring(L"Parsing file \"") + filename + wstring(L"\""));

	while (!inFile.eof())
	{
		char buffer[1024];
		inFile.getline(buffer, 1024);
		
		string line(buffer);
		std::wstring wline;
		wline.assign(line.begin(), line.end());
	
		static const WCHAR commentChar =  L'#';
		if (wline.size() > 0 && wline[0] != commentChar)
		{
			AddLine(wline);
			ParseInputLine(wline, true);
		}
	}
	
	inFile.close();
	AddLine(wstring(L"Finished parsing file \"") + filename + wstring(L"\""));
	AddLine();
}

void DebugConsole::Pin(bool isPinned)
{
	if (lineMemory.empty())
		return;

	// find the last valid command
	for (vector<wstring>::reverse_iterator it = lineMemory.rbegin(); it != lineMemory.rend(); ++it)
	{
		wstringstream inputStream(*it);
		wstring name;
		inputStream >> name;

		Command* command = FindCommand(name);
		if (!command || command->type == Command::Type_function)
			continue;

		if (isPinned)
			AddFormatted(L"Pinned: %s", name.c_str());
		else
			AddFormatted(L"Unpinned: %s", name.c_str());
		command->isPinned = isPinned;

		return;
	}
}

void DebugConsole::UnpinAll()
{
	for (Command* command : commandList)
	{
		if (!command->isPinned)
			continue;

		command->isPinned = false;
		AddFormatted(L"Unpinned: %s", command->name.c_str());
	}
}

void DebugConsole::SaveLog(const wstring& filename) const
{
	ofstream outFile(filename);
	if (!outFile.fail())
	{
		for (const ConsoleLine& consoleLine : linesLog)
		{
			char line[256];
			wcstombs_s(NULL, line, 256, consoleLine.text.c_str(), 255);
			outFile << line << endl;
		}
	}
	
	outFile.close();
}

void DebugConsole::Glitch(int minGlitches)
{
	static int totalGlitchCount = 0;

	for (int i = 0; i < 10; ++i)
	{
		wstringstream b;
		for (int i = 0; i < 180; ++i)
		{
			char c = RAND_CHAR;
			b << ((c == '\n') ? ' ' : c);
		}
		AddLine(b.str(), RAND_COLOR);
	}

	int glitchCount = 0;
	while (glitchCount < minGlitches)
	{
		for (DebugConsole::Command* command : commandList)
		{
			const wstring oldValue = command->GetValueString();
			bool wasGlitched = command->Glitch();
			if (!wasGlitched)
				continue;

			++glitchCount;
			++totalGlitchCount;
			const wstring newValue = command->GetValueString();
			AddFormatted(Color::Red(), L"* %s      %s -> %s", command->name.c_str(), oldValue.c_str(), newValue.c_str());
		}
	}
	AddFormatted(CONSOLE_COLOR_SPECIAL, L"YOU HAVE SURVIVED %d GLITCHES!", totalGlitchCount);
	hasBeenGliched = true;
	consoleColorBack = Color(0.05f, 0.0f, 0.05f, 0.8f);
	consoleColorBorder = Color(1, 0.0f, 1, 0.8f);
}

void DebugConsole::RandomizeColors()
{
	int colorCount = 0;
	for (DebugConsole::Command* command : commandList)
	{
		if (command->type != DebugConsole::Command::Type_color)
			continue;
		
		const wstring oldValue = command->GetValueString();
		const Color newColor = RAND_COLOR;
		(*(Color*)(command->value)) = newColor;
		const wstring newValue = command->GetValueString();
		AddFormatted(newColor, L"* %s      %s -> %s", command->name.c_str(), oldValue.c_str(), newValue.c_str());
		++colorCount;
	}

	AddFormatted(RAND_COLOR, L"RANDOMIZED %d COLORS!", colorCount);
	hasBeenGliched = true;
}

bool DebugConsole::Command::Glitch()
{
	if (!RAND_DIE(100))
		return false;

	switch (type)
	{
		case Type_bool:
			*(bool*)(value) = !*(bool*)(value); break;
		case Type_uint8:
			*(uint8*)(value) += RAND_INT_BETWEEN(-1, 1); break;
		case Type_int8:
			*(int8*)(value) += RAND_INT_BETWEEN(-1, 1); break;
		case Type_int16:
			*(int16*)(value) += RAND_INT_BETWEEN(-1, 1); break;
		case Type_uint16:
			*(uint16*)(value) += RAND_INT_BETWEEN(-1, 1); break;
		case Type_int32:
			*(int32*)(value) += RAND_INT_BETWEEN(-1, 1); break;
		case Type_uint32:
			*(uint32*)(value) += RAND_INT_BETWEEN(-1, 1); break;
		case Type_float:
			*(float*)(value) += RAND_BETWEEN(-1.0f, 1.0f); break;
		case Type_double:
			*(double*)(value) += RAND_BETWEEN(-1.0f, 1.0f); break;
		case Type_intvector2:
			(*(IntVector2*)(value)).x += RAND_INT_BETWEEN(-1, 1);
			(*(IntVector2*)(value)).y += RAND_INT_BETWEEN(-1, 1);
			break;
		case Type_vector2:
			(*(Vector2*)(value)).x += RAND_BETWEEN(-1.0f, 1.0f);
			(*(Vector2*)(value)).y += RAND_BETWEEN(-1.0f, 1.0f);
			break;
		case Type_vector3:
			(*(Vector3*)(value)).x += RAND_BETWEEN(-1.0f, 1.0f);
			(*(Vector3*)(value)).y += RAND_BETWEEN(-1.0f, 1.0f);
			(*(Vector3*)(value)).z += RAND_BETWEEN(-1.0f, 1.0f);
			break;
		case Type_color:
			(*(Color*)(value)) = RAND_COLOR;
			break;
		case Type_string:
		case Type_wstring:
		case Type_function:
			return false;
	}
	
	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////

// show some help
ConsoleFunction(help)
{
	GetDebugConsole().AddLine();
	GetDebugConsole().AddLine(L"Press ~ to toggle the debug console.");
	GetDebugConsole().AddLine(L"To search for commands just type a sub string and press tab.");
	GetDebugConsole().AddLine(L"Use up and down to cycle through previous commands.");
	GetDebugConsole().AddLine(L"Use pgup and pgdn to scroll.");
	GetDebugConsole().AddLine(L"Type pin/unpin to toggle auto saving the previously entered variable.");
	GetDebugConsole().AddLine();
}

// list all commands
ConsoleFunction(list)
{
	GetDebugConsole().AddLine();
	for (const DebugConsole::Command* command : GetDebugConsole().commandList)
	{
		if (!command->isHidden)
			GetDebugConsole().AddLine(command->GetValuePrint());
	}
	GetDebugConsole().AddLine();
}

// list all pinned commands
ConsoleFunction(listPins)
{
	GetDebugConsole().AddLine();
	for (const DebugConsole::Command* command : GetDebugConsole().commandList)
		if (command->isPinned && !command->isHidden)
			GetDebugConsole().AddLine(command->GetValuePrint());
	GetDebugConsole().AddLine();
}

// flip a coin
ConsoleFunction(flip)
{
	static bool lostCoin = false;
	if (lostCoin)
		GetDebugConsole().AddError((lostCoin = !RAND_DIE(10)) ? L"You lost the coin." : L"You found the coin!");
	else if (lostCoin = RAND_DIE(10))
		GetDebugConsole().AddError(L"The coin landed on its side and rolled away!");
	else
		GetDebugConsole().AddLine(RAND_DIE(2) ? L"Heads!" : L"Tails!", CONSOLE_COLOR_SPECIAL);
}

// list all modified variables
ConsoleFunction(modified)
{
	GetDebugConsole().AddLine();
	bool found = false;
	for (const DebugConsole::Command* command : GetDebugConsole().commandList)
	{
		if (!command->wasModified)
			continue;
		
		wstringstream output;
		output << command->GetValuePrint();
		output << " ( default " << command->defaultValueString << " ) ";
		if (command->isPinned)
			output << " pinned";
		GetDebugConsole().AddLine(output.str());
		found = true;
	}
	if (!found)
		GetDebugConsole().AddLine(L"No modified variables found.");
	GetDebugConsole().AddLine();
}

// clear console
ConsoleFunction(clear)
{
	GetDebugConsole().Clear();
}

// parse a file
ConsoleFunction(parseFile)
{
	GetDebugConsole().ParseFile(text.c_str());
}

// pins previous variable, saved and reloaded automatically
ConsoleFunction(pin)
{
	GetDebugConsole().Pin(true);
}

ConsoleFunction(unpin)
{
	GetDebugConsole().Pin(false);
}
ConsoleFunction(unpinAll)
{
	GetDebugConsole().UnpinAll();
}

// save console log
ConsoleFunction(saveLog)
{
	wstringstream inputStream(text.c_str());
	wstring name;
	inputStream >> name;
	GetDebugConsole().SaveLog(name.c_str());
}

ConsoleFunction(glitch)
{
	GetDebugConsole().Glitch();
}

ConsoleFunction(recolor)
{
	GetDebugConsole().RandomizeColors();
}

//////////////////////////////////////////////
//            *ASPEN GOLD 2.0*
//
// ''''''^'^'O      ...         O'''^'''''''''
// '''^'''''O      ...         O'''''''''''^''
// ''''''''O      ...         O''''''''^''''''
// ''''''^''O      \°\         O''''^'''^'''''
// ''''''''''O                  O'''''''''^'''
// '''^'''''''O                  O'''^''''''''
// ''''''''''''O                  O'''''''^'''
// ''''^''''''''O                  O''''''''''

static void ConsoleCommandCallback_startSkiGame(const wstring& text) { startSkiGame = true; }
ConsoleCommand(ConsoleCommandCallback_startSkiGame, ski);

void DebugConsole::UpdateSkiGame()
{
	static bool skiGameActive = false;
	static bool skiGameWaitToStart;
	static int wallPos, gapSize, direction, lineCount, playerPos, sizeDirection;
	static queue<pair<int, int>> wallsQueue;
	const int width = 180;
	const wstring playerString[3] = { L"/°/", L"l°l", L"\\°\\" };
	const int playerOffset = 8;

	if (startSkiGame)
	{
		skiGameActive = true;
		startSkiGame = false;
		skiGameWaitToStart = true;
		wallPos = 20;
		direction = 1;
		sizeDirection = -1;
		gapSize = 150;
		lineCount = 0;
		playerPos = wallPos + gapSize/2;
		while (!wallsQueue.empty()) wallsQueue.pop();
		AddLine(L"*ASPEN GOLD 2.0* - Use arrow keys to move and start skiing!");
	}
	if (!skiGameActive)
		return;
	else if (skiGameWaitToStart)
	{
		if (!g_input->IsKeyDown(VK_LEFT) && !g_input->IsKeyDown(VK_RIGHT) && !g_input->IsKeyDown(VK_SPACE))
			return;
		skiGameWaitToStart = false;
	}

	++lineCount;
	
	// draw play field
	wstringstream buffer;
	int cursorPos = 0;
	for (int i = 0; i < wallPos; ++i, ++cursorPos)
		buffer << (RAND_DIE(500) ? L"^" : L"·");
	buffer << L"O";
	for (int i = 0; i < gapSize; ++i, ++cursorPos)
		buffer << L" ";
	buffer << L"O";
	if ((lineCount+49) % 50 >= 45 || lineCount == skiHighScore)
		buffer << L" " << lineCount << L" "; 

	while (buffer.str().length() <= unsigned(width+10))
		buffer << (RAND_DIE(500)? L"^" : L"·");
	buffer << L"\n";
	if (lineCount == skiHighScore)
		AddLine(buffer.str(), CONSOLE_COLOR_SUCCESS);
	else if (lineCount <= skiHighScore && lineCount >= skiHighScore - 50)
		AddLine(buffer.str(), CONSOLE_COLOR_ERROR);
	else if ((lineCount+49) % 50 >= 45)
		AddLine(buffer.str(), CONSOLE_COLOR_SPECIAL);
	else
		AddLine(buffer.str());
	
	{
		// update player
		wstring& stringBuffer1 = lines[Min(playerOffset + 1, int(lines.size())-1)].text;
		while (stringBuffer1.length() <= unsigned(width))
			stringBuffer1 += L" ";  // add padding if string is too short
		for(unsigned i = 0; i < playerString[0].length(); ++i)
			stringBuffer1[playerPos-1+i] = '.'; // clear old player

		const int moveDirection = (g_input->IsKeyDown(VK_LEFT)? -1 : 0) + (g_input->IsKeyDown(VK_RIGHT)? 1 : 0);
		playerPos = Cap(playerPos+moveDirection, 1, width-1);
		wstring& stringBuffer2 = lines[Min(playerOffset, int(lines.size())-1)].text;
		while (stringBuffer2.length() <= unsigned(width))
			stringBuffer2 += L" ";  // add padding if string is too short
		for(unsigned i = 0; i < playerString[0].length(); ++i)
			stringBuffer2[playerPos-1+i] = playerString[1+moveDirection][i]; // draw new player
	}

	wallPos += direction;
	if (wallsQueue.size() > playerOffset + 1)
		wallsQueue.pop();
	wallsQueue.push(pair<int, int>(wallPos + 1, wallPos + gapSize + 2));
	
	if (RAND_DIE(200) || lineCount < 20)
		direction = 0;	// go straight at first
	else if (RAND_DIE(50))
		direction = RAND_INT_SIGN; // randomly change direction
	if (wallPos + gapSize > width && direction > 0 || wallPos < 1 && direction < 0)
		direction *= -1; // change direction if reached width on either side
		
	if ((lineCount % 10 == 0 || lineCount < 50) && gapSize > 1)
	{
		if (gapSize < 30 && sizeDirection < 0 && RAND_DIE(gapSize - 15) || gapSize > 30 && sizeDirection > 0 && RAND_DIE(50 - gapSize))
			sizeDirection *= -1; // randomly change getting wider or thinner
		if (RAND_DIE(2) && wallPos > 1)
			wallPos -= sizeDirection; // random shift when getting smaller to balance
		gapSize += sizeDirection;
	}
	
	pair<int, int> walls = wallsQueue.front();
	if (playerPos + 1 <= walls.first || playerPos + 1 >= walls.second)
	{
		skiGameActive = false;
		int distance = lineCount - playerOffset;
		if (distance > skiHighScore)
		{
			skiHighScore = distance;
			AddFormatted(CONSOLE_COLOR_SUCCESS, L"New high score! You skied %d meters!", distance);
		}
		else
			AddFormatted(L"Game over! You skied %d meters. High score %d meters.", distance, skiHighScore);

		if (distance >= 1000 && !GameControlBase::devMode)
		{
			AddLine(L"Dev mode unlocked!", CONSOLE_COLOR_SPECIAL);
			GameControlBase::devMode = true;
		}
	}

	Sleep(10);
}
