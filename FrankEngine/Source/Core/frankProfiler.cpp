////////////////////////////////////////////////////////////////////////////////////////
/*
	Frank Engine Profiling Tool
	Copyright 2013 Frank Force - http://www.frankforce.com
*/
////////////////////////////////////////////////////////////////////////////////////////

#include "frankEngine.h"

bool FrankProfiler::showProfileDisplay = false;
ConsoleCommand(FrankProfiler::showProfileDisplay, profilerEnable);

void FrankProfiler::Render()
{
	FrankProfilerEntryDefine(L"FrankProfiler::Render()", Color::Yellow(), -100);
	CDXUTPerfEventGenerator( DXUT_PERFEVENTCOLOR, L"FrankProfiler::Render()" );

	list<FrankProfilerEntry*>& entries = GetEntries();
	if (showProfileDisplay)
	{
		const int nameGapSize = 400;
		const int timeGapSize = 130;
		const int highTimeGapSize = 130;
		const int callCountGapSize = 80;
		int x = 400, y = 50;
	
		Box2AABB box(Vector2(float(x-20),float(y-20)), Vector2(float(x)+700,float(y)+700));
		g_render->RenderScreenSpaceQuad(box, Color::Black(0.7f));

		g_textHelper->Begin();
		g_textHelper->SetInsertionPos( x, y );
		g_textHelper->SetForegroundColor( Color::White() );
		g_textHelper->DrawFormattedTextLine( L"Profiler Entry Name" );
		g_textHelper->SetInsertionPos( x + nameGapSize, y);
		g_textHelper->DrawFormattedTextLine( L"Ave Time" );
		g_textHelper->SetInsertionPos( x + nameGapSize + timeGapSize, y );
		g_textHelper->DrawFormattedTextLine( L"High Time" );
		g_textHelper->SetInsertionPos( x + nameGapSize + timeGapSize + highTimeGapSize, y );
		g_textHelper->DrawFormattedTextLine( L"Calls" );
		//g_textHelper->SetInsertionPos( x + nameGapSize + timeGapSize + highTimeGapSize + callCountGapSize, y );
		//g_textHelper->DrawFormattedTextLine( L"Ave Calls" );
		POINT insertionPos = g_textHelper->GetInsertionPos();
		g_textHelper->SetInsertionPos( x, insertionPos.y + 15 );

		for (FrankProfilerEntry* entry : entries) 
		{
			POINT insertionPos = g_textHelper->GetInsertionPos();
			g_textHelper->SetInsertionPos( x, insertionPos.y );
			g_textHelper->SetForegroundColor( entry->color );
			g_textHelper->DrawTextLine( entry->name);
			
			g_textHelper->SetInsertionPos( x + nameGapSize, insertionPos.y );
			g_textHelper->DrawFormattedTextLine( L"%.3f", 1000*entry->timeAverage );

			if (entry->timeHigh > 2*entry->timeAverage && 1000*entry->timeHigh > 0.1f)
			{
				// show spikes in red
				g_textHelper->SetForegroundColor( Color::Red() );
			}

			g_textHelper->SetInsertionPos( x + nameGapSize + timeGapSize, insertionPos.y );
			g_textHelper->DrawFormattedTextLine( L"%.3f", 1000*entry->timeHigh );
			
			g_textHelper->SetForegroundColor( Color::White() );
			g_textHelper->SetInsertionPos( x + nameGapSize + timeGapSize + highTimeGapSize, insertionPos.y );
			g_textHelper->DrawFormattedTextLine( L"%d", entry->callCount );

			//g_textHelper->SetInsertionPos( x + nameGapSize + timeGapSize + highTimeGapSize + callCountGapSize, insertionPos.y );
			//g_textHelper->DrawFormattedTextLine( L"%.2f", entry->callCountAverage );
		}
		g_textHelper->End();
	}
	
	for (FrankProfilerEntry* entry : entries) 
	{
		if (GetFocus() == DXUTGetHWND())
		{
			if (entry->time > entry->timeHigh)
			{
				entry->timeHigh = entry->time;
				entry->timerHighTimer.Set();
			}
			if (entry->timerHighTimer > 5.0f)
				entry->timeHigh = entry->time;

			entry->timeAverage = Lerp(0.5f, entry->time, entry->timeAverage);
			entry->callCountAverage = Lerp(0.5f, float(entry->callCount), entry->callCountAverage);
		}

		entry->time = 0;
		entry->callCount = 0;
	}
}

void FrankProfiler::AddEntry(FrankProfilerEntry& entry)
{
	list<FrankProfilerEntry*>& entries = GetEntries();

	entries.push_back(&entry); 
	entries.sort(FrankProfilerEntry::SortCompare);
}

float FrankProfiler::GetTotalFrameTime()
{
	list<FrankProfilerEntry*>& entries = GetEntries();
	if (entries.empty())
		return 0;

	// hack: just use the top entry
	return entries.front()->timeAverage;
}