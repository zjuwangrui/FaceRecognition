#ifndef __ANTISLEEP_FATIGUESTATE_H__
#define __ANTISLEEP_FATIGUESTATE_H__

#include "BufStruct.h"
#include <string.h>

#define FATIGUE_STATE_MAGIC              0x46544753UL
#define FATIGUE_STATE_VERSION            1
#define FATIGUE_STATE_STORAGE_BYTES      (256 * 2)
#define FATIGUE_WINDOW_FRAMES            900
#define FATIGUE_MIN_VALID_FRAMES         450
#define FATIGUE_EVALUATION_FRAMES        30
#define FATIGUE_SUSPECT_CLOSED_FRAMES    30
#define FATIGUE_ALARM_CLOSED_FRAMES      45
#define FATIGUE_RECOVERY_OPEN_FRAMES     30
#define FATIGUE_NO_FACE_FRAMES           30
#define FATIGUE_RESET_NO_FACE_FRAMES     90
#define FATIGUE_P70_PERCENT              30
#define FATIGUE_P80_PERCENT              15
#define FATIGUE_EM_PERCENT               40

#define FATIGUE_VALID_BYTES ((FATIGUE_WINDOW_FRAMES + 7) / 8)
#define FATIGUE_LEVEL_BYTES ((FATIGUE_WINDOW_FRAMES + 3) / 4)

enum EYE_OBSERVATION_STATE
{
	EYE_STATE_UNKNOWN = 0,
	EYE_STATE_OPEN,
	EYE_STATE_CLOSING,
	EYE_STATE_CLOSED,
	EYE_STATE_REOPENING
};

enum FATIGUE_JUDGMENT_STATE
{
	FATIGUE_STATE_UNKNOWN = 0,
	FATIGUE_STATE_NORMAL,
	FATIGUE_STATE_SUSPECT,
	FATIGUE_STATE_ALARM,
	FATIGUE_STATE_NO_FACE
};

struct FATIGUE_SHARED_STATE
{
	DWORD magic;
	WORD version;
	WORD structSize;
	DWORD frameSequence;
	DWORD lastProcessedFrame;

	aBYTE observationValid;
	aBYTE faceValid;
	aBYTE modelValid;
	aBYTE closurePercent;
	aBYTE eyeState;
	aBYTE fatigueState;
	aBYTE blinkEvent;
	aBYTE alarmEdge;

	WORD noFaceFrames;
	WORD continuousClosedFrames;
	WORD currentBlinkDurationFrames;
	WORD lastBlinkDurationFrames;
	WORD recoveryOpenFrames;
	WORD qualifyingWindows;
	WORD evaluationFrames;

	DWORD blinkCount;
	DWORD totalBlinkDurationFrames;

	WORD windowIndex;
	WORD windowCount;
	WORD validCount;
	WORD emCount;
	WORD p70Count;
	WORD p80Count;

	aBYTE validWindow[FATIGUE_VALID_BYTES];
	aBYTE closureWindow[FATIGUE_LEVEL_BYTES];
};

typedef char FATIGUE_STATE_MUST_FIT_CLRLOC_BUFFER[
	(sizeof(FATIGUE_SHARED_STATE) <= FATIGUE_STATE_STORAGE_BYTES) ? 1 : -1];

inline FATIGUE_SHARED_STATE* GetFatigueState(BUF_STRUCT* pBS)
{
	if (!pBS || !pBS->pOtherVars)
		return NULL;
	return (FATIGUE_SHARED_STATE*)pBS->pOtherVars->ClrLocBuf;
}

inline void ResetFatigueState(FATIGUE_SHARED_STATE* pState)
{
	if (!pState)
		return;
	memset(pState, 0, sizeof(FATIGUE_SHARED_STATE));
	pState->magic = FATIGUE_STATE_MAGIC;
	pState->version = FATIGUE_STATE_VERSION;
	pState->structSize = (WORD)sizeof(FATIGUE_SHARED_STATE);
	pState->eyeState = EYE_STATE_UNKNOWN;
	pState->fatigueState = FATIGUE_STATE_UNKNOWN;
}

inline bool IsFatigueStateValid(const FATIGUE_SHARED_STATE* pState)
{
	return pState &&
		pState->magic == FATIGUE_STATE_MAGIC &&
		pState->version == FATIGUE_STATE_VERSION &&
		pState->structSize == sizeof(FATIGUE_SHARED_STATE);
}

inline void EnsureFatigueState(FATIGUE_SHARED_STATE* pState)
{
	if (!IsFatigueStateValid(pState))
		ResetFatigueState(pState);
}

inline bool GetFatigueWindowValid(const FATIGUE_SHARED_STATE* pState, int index)
{
	return (pState->validWindow[index >> 3] & (1 << (index & 7))) != 0;
}

inline void SetFatigueWindowValid(FATIGUE_SHARED_STATE* pState, int index, bool valid)
{
	aBYTE mask = (aBYTE)(1 << (index & 7));
	if (valid)
		pState->validWindow[index >> 3] |= mask;
	else
		pState->validWindow[index >> 3] &= (aBYTE)~mask;
}

inline int GetFatigueWindowLevel(const FATIGUE_SHARED_STATE* pState, int index)
{
	int shift = (index & 3) * 2;
	return (pState->closureWindow[index >> 2] >> shift) & 3;
}

inline void SetFatigueWindowLevel(FATIGUE_SHARED_STATE* pState, int index, int level)
{
	int shift = (index & 3) * 2;
	aBYTE mask = (aBYTE)(3 << shift);
	pState->closureWindow[index >> 2] =
		(aBYTE)((pState->closureWindow[index >> 2] & ~mask) | ((level & 3) << shift));
}

inline void ResetFatigueMetrics(FATIGUE_SHARED_STATE* pState)
{
	if (!pState)
		return;
	pState->lastProcessedFrame = pState->frameSequence;
	pState->continuousClosedFrames = 0;
	pState->currentBlinkDurationFrames = 0;
	pState->lastBlinkDurationFrames = 0;
	pState->recoveryOpenFrames = 0;
	pState->qualifyingWindows = 0;
	pState->evaluationFrames = 0;
	pState->blinkCount = 0;
	pState->totalBlinkDurationFrames = 0;
	pState->windowIndex = 0;
	pState->windowCount = 0;
	pState->validCount = 0;
	pState->emCount = 0;
	pState->p70Count = 0;
	pState->p80Count = 0;
	memset(pState->validWindow, 0, sizeof(pState->validWindow));
	memset(pState->closureWindow, 0, sizeof(pState->closureWindow));
	pState->fatigueState = FATIGUE_STATE_UNKNOWN;
	pState->alarmEdge = 0;
}

#endif
