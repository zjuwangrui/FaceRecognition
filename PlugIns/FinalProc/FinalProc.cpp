// FinalProc.cpp : fatigue judgment and alarm plugin.

#include "stdafx.h"
#include "FinalProc.h"
#include "BufStruct.h"
#include "FatigueState.h"
#include "ImageProc.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

BEGIN_MESSAGE_MAP(CFinalProcApp, CWinApp)
	//{{AFX_MSG_MAP(CFinalProcApp)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

CFinalProcApp::CFinalProcApp()
{
}

CFinalProcApp theApp;
char sInfo[] = "Plugin5-FinalProc: PERCLOS fatigue judgment and alarm";
bool bLastPlugin = false;

static int ClosureToLevel(int closure)
{
	if (closure >= 80) return 3;
	if (closure >= 70) return 2;
	if (closure >= 50) return 1;
	return 0;
}

static const char* FatigueStateName(int state)
{
	switch (state)
	{
	case FATIGUE_STATE_NORMAL: return "NORMAL";
	case FATIGUE_STATE_SUSPECT: return "SUSPECT";
	case FATIGUE_STATE_ALARM: return "ALARM";
	case FATIGUE_STATE_NO_FACE: return "NO_FACE";
	default: return "UNKNOWN";
	}
}

static void RemoveWindowSample(FATIGUE_SHARED_STATE* state, int index)
{
	if (!GetFatigueWindowValid(state, index))
		return;
	int level = GetFatigueWindowLevel(state, index);
	if (state->validCount > 0) state->validCount--;
	if (level >= 1 && state->emCount > 0) state->emCount--;
	if (level >= 2 && state->p70Count > 0) state->p70Count--;
	if (level >= 3 && state->p80Count > 0) state->p80Count--;
}

static void AddWindowSample(FATIGUE_SHARED_STATE* state, bool valid, int closure)
{
	int index = state->windowIndex;
	if (state->windowCount >= FATIGUE_WINDOW_FRAMES)
		RemoveWindowSample(state, index);
	else
		state->windowCount++;

	if (valid)
	{
		int level = ClosureToLevel(closure);
		SetFatigueWindowValid(state, index, true);
		SetFatigueWindowLevel(state, index, level);
		state->validCount++;
		if (level >= 1) state->emCount++;
		if (level >= 2) state->p70Count++;
		if (level >= 3) state->p80Count++;
	}
	else
	{
		SetFatigueWindowValid(state, index, false);
		SetFatigueWindowLevel(state, index, 0);
	}
	state->windowIndex = (WORD)((index + 1) % FATIGUE_WINDOW_FRAMES);
}

static int Percentage(int count, int total)
{
	return total > 0 ? count * 100 / total : 0;
}

DLL_EXP void ON_PLUGIN_BELAST(bool bLast)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	bLastPlugin = bLast;
}

DLL_EXP LPCTSTR ON_PLUGININFO(void)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	return sInfo;
}

DLL_EXP void ON_INITPLUGIN(LPVOID lpParameter)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
}

DLL_EXP int ON_PLUGINCTRL(int nMode, void* pParameter)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	return 0;
}

DLL_EXP void ON_PLUGINRUN(int w, int h, BYTE* pYBits, BYTE* pUBits, BYTE* pVBits, BYTE* pBuffer)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	if (!pBuffer || !pYBits || w <= 0 || h <= 0)
		return;

	BUF_STRUCT* pBS = (BUF_STRUCT*)pBuffer;
	if (pBS->bNotInited || !pBS->pOtherVars)
		return;

	FATIGUE_SHARED_STATE* state = GetFatigueState(pBS);
	if (!IsFatigueStateValid(state) || state->frameSequence == state->lastProcessedFrame)
		return;

	state->lastProcessedFrame = state->frameSequence;
	state->alarmEdge = 0;
	int previousState = state->fatigueState;
	bool noFace = !state->faceValid || state->noFaceFrames >= FATIGUE_NO_FACE_FRAMES;

	if (state->noFaceFrames >= FATIGUE_RESET_NO_FACE_FRAMES)
		ResetFatigueMetrics(state);

	if (noFace)
	{
		state->fatigueState = state->noFaceFrames >= FATIGUE_NO_FACE_FRAMES ?
			FATIGUE_STATE_NO_FACE : FATIGUE_STATE_UNKNOWN;
		state->recoveryOpenFrames = 0;
		state->continuousClosedFrames = 0;
	}
	else
	{
		bool valid = state->observationValid != 0;
		int closure = state->closurePercent;
		AddWindowSample(state, valid, closure);

		if (valid && closure >= 70)
		{
			if (state->continuousClosedFrames < 65535)
				state->continuousClosedFrames++;
			state->recoveryOpenFrames = 0;
		}
		else if (valid && closure <= 35)
		{
			state->continuousClosedFrames = 0;
			if (state->recoveryOpenFrames < 65535)
				state->recoveryOpenFrames++;
		}
		else if (!valid)
		{
			state->continuousClosedFrames = 0;
			state->recoveryOpenFrames = 0;
		}

		int p70 = Percentage(state->p70Count, state->validCount);
		int p80 = Percentage(state->p80Count, state->validCount);
		int em = Percentage(state->emCount, state->validCount);
		bool enoughSamples = state->validCount >= FATIGUE_MIN_VALID_FRAMES;
		bool metricExceeded = enoughSamples &&
			(p70 >= FATIGUE_P70_PERCENT || p80 >= FATIGUE_P80_PERCENT ||
			 em >= FATIGUE_EM_PERCENT);

		if (state->continuousClosedFrames >= FATIGUE_ALARM_CLOSED_FRAMES)
		{
			state->fatigueState = FATIGUE_STATE_ALARM;
		}
		else if (state->continuousClosedFrames >= FATIGUE_SUSPECT_CLOSED_FRAMES)
		{
			state->fatigueState = FATIGUE_STATE_SUSPECT;
		}
		else
		{
			state->evaluationFrames++;
			if (state->evaluationFrames >= FATIGUE_EVALUATION_FRAMES)
			{
				state->evaluationFrames = 0;
				if (metricExceeded)
				{
					if (state->qualifyingWindows < 65535)
						state->qualifyingWindows++;
				}
				else
				{
					state->qualifyingWindows = 0;
				}
				ShowDebugMessage("FinalProc: blink=%lu avg=%.2fs P70=%d%% P80=%d%% EM=%d%% valid=%d",
					state->blinkCount,
					state->blinkCount ?
						(double)state->totalBlinkDurationFrames * 0.033 / state->blinkCount : 0.0,
					p70, p80, em, state->validCount);
			}

			if (state->qualifyingWindows >= 2)
				state->fatigueState = FATIGUE_STATE_ALARM;
			else if (state->qualifyingWindows == 1)
				state->fatigueState = FATIGUE_STATE_SUSPECT;
			else if (previousState == FATIGUE_STATE_ALARM &&
				state->recoveryOpenFrames < FATIGUE_RECOVERY_OPEN_FRAMES)
				state->fatigueState = FATIGUE_STATE_ALARM;
			else if (!enoughSamples)
				state->fatigueState = FATIGUE_STATE_UNKNOWN;
			else
				state->fatigueState = FATIGUE_STATE_NORMAL;
		}
	}

	bool enteringAlarm = state->fatigueState == FATIGUE_STATE_ALARM &&
		previousState != FATIGUE_STATE_ALARM;
	bool enteringNoFace = state->fatigueState == FATIGUE_STATE_NO_FACE &&
		previousState != FATIGUE_STATE_NO_FACE;
	if (enteringAlarm || enteringNoFace)
	{
		state->alarmEdge = 1;
		MessageBeep(enteringAlarm ? MB_ICONEXCLAMATION : MB_ICONASTERISK);
	}
	if (state->fatigueState != previousState)
	{
		ShowDebugMessage("FinalProc: state %s -> %s",
			FatigueStateName(previousState), FatigueStateName(state->fatigueState));
	}

	if (state->fatigueState == FATIGUE_STATE_ALARM ||
		state->fatigueState == FATIGUE_STATE_NO_FACE)
	{
		aRect border = {2, 2, w - 4, h - 4};
		DrawRectangle(pYBits, w, h, border, RGB(255, 255, 255), true);
		border.left += 2;
		border.top += 2;
		border.width -= 4;
		border.height -= 4;
		DrawRectangle(pYBits, w, h, border, RGB(255, 255, 255), true);
	}
}

DLL_EXP void ON_PLUGINEXIT()
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
}
