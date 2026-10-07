// BlinkEyeCheck.cpp : blink detection and eye/nose localization plugin.

#include "stdafx.h"
#include "BlinkEyeCheck.h"
#include "BufStruct.h"
#include "FatigueState.h"
#include "ImageProc.h"
#include "TraceFeatureApi.h"

#define HISTORY_FRAME_OFFSET       4
#define HISTOGRAM_PIXEL_COUNT      50
#define MIN_DIFFERENCE_THRESHOLD   10
#define MAX_EYE_Y_DIFF             4
#define MIN_EYE_X_DIFF             15
#define MAX_EYE_X_DIFF             30
#define MAX_EYE_SIZE               200
#define MIN_COMPONENT_PIXELS       2
#define EYE_PATCH_WIDTH            32
#define EYE_PATCH_HEIGHT           24
#define NOSE_PATCH_WIDTH           32
#define NOSE_PATCH_HEIGHT          48
#define OPEN_CLOSURE_THRESHOLD     35
#define CLOSED_CLOSURE_THRESHOLD   70
#define TEMPLATE_UPDATE_DIVISOR    16
#define MODEL_REJECT_DISTANCE      170
#define BLINK_NO_FACE_RESET        FATIGUE_RESET_NO_FACE_FRAMES

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

BEGIN_MESSAGE_MAP(CBlinkEyeCheckApp, CWinApp)
	//{{AFX_MSG_MAP(CBlinkEyeCheckApp)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

CBlinkEyeCheckApp::CBlinkEyeCheckApp()
{
}

CBlinkEyeCheckApp theApp;

#define PC_MODE
#ifdef PC_MODE
aBYTE open_eye_left[EYE_PATCH_WIDTH * EYE_PATCH_HEIGHT * 2];
aBYTE open_eye_right[EYE_PATCH_WIDTH * EYE_PATCH_HEIGHT * 2];
aBYTE close_eye_left[EYE_PATCH_WIDTH * EYE_PATCH_HEIGHT];
aBYTE close_eye_right[EYE_PATCH_WIDTH * EYE_PATCH_HEIGHT];
aBYTE st_nose[NOSE_PATCH_WIDTH * NOSE_PATCH_HEIGHT * 2];
#endif

char sInfo[] = "Plugin4-BlinkEyeCheck: blink detection and eye/nose localization";
bool bLastPlugin = false;

static bool sModelValid = false;
static bool sHasEyeRects = false;
static aRect sLeftEyeRect = {0, 0, 0, 0};
static aRect sRightEyeRect = {0, 0, 0, 0};
static aRect sNoseRect = {0, 0, 0, 0};
static int sEyeMachineState = EYE_STATE_UNKNOWN;
static int sClosingFrames = 0;
static int sClosedFrames = 0;
static int sReopeningFrames = 0;
static int sNoFaceFrames = 0;
static const BYTE* sSharedBuffer = NULL;
static int sFrameWidth = 0;
static int sFrameHeight = 0;
static bool sLoggedInitialization = false;

struct COMPONENT_INFO
{
	int count;
	int minX;
	int maxX;
	int minY;
	int maxY;
	int sumX;
	int sumY;
};

static int AbsInt(int value)
{
	return value < 0 ? -value : value;
}

static int ClampInt(int value, int minimum, int maximum)
{
	if (value < minimum) return minimum;
	if (value > maximum) return maximum;
	return value;
}

static bool IsRectValid(const aRect& rc, int w, int h)
{
	return rc.width > 0 && rc.height > 0 && rc.left >= 0 && rc.top >= 0 &&
		rc.left + rc.width <= w && rc.top + rc.height <= h;
}

static bool IsFaceValid(const BUF_STRUCT* pBS, int w, int h)
{
	return pBS && pBS->rcnFace.width > 0 && pBS->rcnFace.height > 0 &&
		pBS->rcnFace.left >= 0 && pBS->rcnFace.top >= 0 &&
		pBS->rcnFace.left + pBS->rcnFace.width <= w / 2 &&
		pBS->rcnFace.top + pBS->rcnFace.height <= h / 4;
}

static void InvalidateDetection(BUF_STRUCT* pBS)
{
	pBS->EyePosConfirm = false;
	pBS->EyeBallConfirm = false;
	pBS->ptTheLeftEye.x = pBS->ptTheLeftEye.y = -1;
	pBS->ptTheRightEye.x = pBS->ptTheRightEye.y = -1;
	pBS->ptTheNose.x = pBS->ptTheNose.y = -1;
}

static void ResetLocalState(bool clearModel)
{
	sHasEyeRects = false;
	sEyeMachineState = EYE_STATE_UNKNOWN;
	sClosingFrames = 0;
	sClosedFrames = 0;
	sReopeningFrames = 0;
	sNoFaceFrames = 0;
	memset(&sLeftEyeRect, 0, sizeof(sLeftEyeRect));
	memset(&sRightEyeRect, 0, sizeof(sRightEyeRect));
	memset(&sNoseRect, 0, sizeof(sNoseRect));
	if (clearModel)
	{
		sModelValid = false;
		memset(open_eye_left, 0, sizeof(open_eye_left));
		memset(open_eye_right, 0, sizeof(open_eye_right));
		memset(close_eye_left, 0, sizeof(close_eye_left));
		memset(close_eye_right, 0, sizeof(close_eye_right));
		memset(st_nose, 0, sizeof(st_nose));
	}
}

static void BlurGrayImage(const aBYTE* source, aBYTE* destination, int w, int h)
{
	for (int y = 0; y < h; y++)
	{
		for (int x = 0; x < w; x++)
		{
			int xm = x > 0 ? x - 1 : x;
			int xp = x + 1 < w ? x + 1 : x;
			int ym = y > 0 ? y - 1 : y;
			int yp = y + 1 < h ? y + 1 : y;
			int value = source[ym * w + xm] + 2 * source[ym * w + x] + source[ym * w + xp] +
				2 * source[y * w + xm] + 4 * source[y * w + x] + 2 * source[y * w + xp] +
				source[yp * w + xm] + 2 * source[yp * w + x] + source[yp * w + xp];
			destination[y * w + x] = (aBYTE)((value + 8) / 16);
		}
	}
}

static int SelectDifferenceThreshold(const aBYTE* image, int w, int h, int wantedPixels)
{
	int histogram[256];
	memset(histogram, 0, sizeof(histogram));
	for (int i = 0; i < w * h; i++)
		histogram[image[i]]++;

	int target = wantedPixels;
	if (target > w * h) target = w * h;
	int total = 0;
	for (int value = 255; value >= 0; value--)
	{
		total += histogram[value];
		if (total >= target)
			return value;
	}
	return 0;
}

static void HorizontalOpen(aBYTE* image, int w, int h, aBYTE* temporary)
{
	memset(temporary, 0, w * h);
	for (int y = 0; y < h; y++)
		for (int x = 1; x < w - 1; x++)
			if (image[y * w + x - 1] && image[y * w + x] && image[y * w + x + 1])
				temporary[y * w + x] = 255;

	memset(image, 0, w * h);
	for (int y = 0; y < h; y++)
		for (int x = 0; x < w; x++)
			if (temporary[y * w + x])
			{
				if (x > 0) image[y * w + x - 1] = 255;
				image[y * w + x] = 255;
				if (x + 1 < w) image[y * w + x + 1] = 255;
			}
}

static bool PointInsideFace(const aPOINT& point, const aRect& face)
{
	int x = point.x / 2;
	int y = point.y / 4;
	return x >= face.left && x <= RECT_RIGHT(face) &&
		y >= face.top && y <= RECT_BOTTOM(face);
}

static aRect MakeEyeRect(const aPOINT& center, int eyeDistance, int w, int h)
{
	aRect rc;
	rc.width = eyeDistance * 2 / 3;
	rc.height = eyeDistance / 2;
	if (rc.width < 8) rc.width = 8;
	if (rc.height < 8) rc.height = 8;
	rc.width = (rc.width + 3) / 4 * 4;
	rc.height = (rc.height + 3) / 4 * 4;
	rc.left = center.x - rc.width / 2;
	rc.top = center.y - rc.height / 2;
	if (rc.left < 0) rc.left = 0;
	if (rc.top < 0) rc.top = 0;
	if (rc.left + rc.width > w) rc.left = w - rc.width;
	if (rc.top + rc.height > h) rc.top = h - rc.height;
	return rc;
}

static aRect MakeNoseRect(const aPOINT& leftEye, const aPOINT& rightEye,
	int eyeDistance, const aRect& face, int w, int h)
{
	int faceLeft = face.left * 2;
	int faceTop = face.top * 4;
	int faceRight = (face.left + face.width) * 2;
	int faceBottom = (face.top + face.height) * 4;
	int centerX = (leftEye.x + rightEye.x) / 2;
	int centerY = (leftEye.y + rightEye.y) / 2 + eyeDistance * 2 / 3;

	aRect rc;
	rc.width = eyeDistance / 2;
	rc.height = eyeDistance;
	if (rc.width < 8) rc.width = 8;
	if (rc.height < 16) rc.height = 16;
	rc.width = (rc.width + 3) / 4 * 4;
	rc.height = (rc.height + 3) / 4 * 4;
	if (rc.width > faceRight - faceLeft) rc.width = (faceRight - faceLeft) / 4 * 4;
	if (rc.height > faceBottom - faceTop) rc.height = (faceBottom - faceTop) / 4 * 4;
	if (rc.width < 4) rc.width = 4;
	if (rc.height < 4) rc.height = 4;
	rc.left = centerX - rc.width / 2;
	rc.top = centerY - rc.height / 2;
	rc.left = ClampInt(rc.left, faceLeft, faceRight - rc.width);
	rc.top = ClampInt(rc.top, faceTop, faceBottom - rc.height);
	rc.left = ClampInt(rc.left, 0, w - rc.width);
	rc.top = ClampInt(rc.top, 0, h - rc.height);
	return rc;
}

static void ExtractGrayPatch(const aBYTE* image, int imageW, int imageH,
	const aRect& rc, int patchW, int patchH, aBYTE* result)
{
	for (int y = 0; y < patchH; y++)
	{
		int sy = rc.top + y * rc.height / patchH;
		for (int x = 0; x < patchW; x++)
		{
			int sx = rc.left + x * rc.width / patchW;
			result[y * patchW + x] = image[sy * imageW + sx];
		}
	}
}

static void ExtractPlanarPatch(const aBYTE* image, int imageW, int imageH,
	const aRect& rc, int patchW, int patchH, aBYTE* result, bool includeColor)
{
	int ySize = patchW * patchH;
	int uvSize = patchW * patchH / 2;
	const aBYTE* sourceY = image;
	const aBYTE* sourceU = image + imageW * imageH;
	const aBYTE* sourceV = sourceU + imageW * imageH / 2;
	for (int y = 0; y < patchH; y++)
	{
		int sy = rc.top + y * rc.height / patchH;
		for (int x = 0; x < patchW; x++)
		{
			int sx = rc.left + x * rc.width / patchW;
			result[y * patchW + x] = sourceY[sy * imageW + sx];
		}
	}
	if (!includeColor)
		return;
	for (int y = 0; y < patchH; y++)
	{
		int sy = rc.top + y * rc.height / patchH;
		for (int x = 0; x < patchW / 2; x++)
		{
			int sx = rc.left + (x * 2) * rc.width / patchW;
			result[y * (patchW / 2) + x + ySize] = sourceU[sy * (imageW / 2) + sx / 2];
			result[y * (patchW / 2) + x + ySize + uvSize] = sourceV[sy * (imageW / 2) + sx / 2];
		}
	}
}

static bool ConfirmEyePigment(const aBYTE* patch, aPOINT* center)
{
	const aBYTE* yPlane = patch;
	const aBYTE* uPlane = patch + EYE_PATCH_WIDTH * EYE_PATCH_HEIGHT;
	const aBYTE* vPlane = uPlane + EYE_PATCH_WIDTH * EYE_PATCH_HEIGHT / 2;
	int facePixels = 0;
	int eyePixels = 0;
	int sumX = 0;
	int sumY = 0;
	for (int y = 0; y < EYE_PATCH_HEIGHT; y++)
	{
		for (int x = 0; x < EYE_PATCH_WIDTH / 2; x++)
		{
			int index = y * (EYE_PATCH_WIDTH / 2) + x;
			int u = uPlane[index];
			int v = vPlane[index];
			if (u >= 85 && u <= 126 && v >= 130 && v <= 165)
				facePixels++;
			bool pigment = (u >= 124 && u <= 131) || (v >= 121 && v <= 134);
			int luminance = (yPlane[y * EYE_PATCH_WIDTH + x * 2] +
				yPlane[y * EYE_PATCH_WIDTH + x * 2 + 1]) / 2;
			if (pigment && luminance < 140)
			{
				eyePixels++;
				sumX += x * 2 + 1;
				sumY += y;
			}
		}
	}
	if (facePixels < 200 || eyePixels < 10 || eyePixels > 60)
	{
		center->x = center->y = -1;
		return false;
	}
	center->x = sumX / eyePixels;
	center->y = sumY / eyePixels;
	return true;
}

static int GrayPatchDistance(const aBYTE* first, const aBYTE* second, int count)
{
	int sum = 0;
	int firstMean = 0;
	int secondMean = 0;
	for (int i = 0; i < count; i++)
	{
		firstMean += first[i];
		secondMean += second[i];
	}
	firstMean /= count;
	secondMean /= count;
	for (int i = 0; i < count; i++)
		sum += AbsInt(((int)first[i] - firstMean) - ((int)second[i] - secondMean));
	return sum / count;
}

static int EstimateClosure(const aBYTE* leftPatch, const aBYTE* rightPatch)
{
	int count = EYE_PATCH_WIDTH * EYE_PATCH_HEIGHT;
	int openDistance = (GrayPatchDistance(leftPatch, open_eye_left, count) +
		GrayPatchDistance(rightPatch, open_eye_right, count)) / 2;
	int closedDistance = (GrayPatchDistance(leftPatch, close_eye_left, count) +
		GrayPatchDistance(rightPatch, close_eye_right, count)) / 2;
	int total = openDistance + closedDistance;
	if (total <= 0)
		return 0;
	return ClampInt(openDistance * 100 / total, 0, 100);
}

static void UpdateTemplate(aBYTE* target, const aBYTE* source, int count)
{
	for (int i = 0; i < count; i++)
		target[i] = (aBYTE)((target[i] * (TEMPLATE_UPDATE_DIVISOR - 1) + source[i]) /
			TEMPLATE_UPDATE_DIVISOR);
}

static bool FindEyePair(aBYTE* labels, int tw, int th, int regionCount,
	const aRect& face, aPOINT* leftEye, aPOINT* rightEye)
{
	COMPONENT_INFO components[256];
	for (int i = 0; i < 256; i++)
	{
		components[i].count = 0;
		components[i].minX = tw;
		components[i].maxX = -1;
		components[i].minY = th;
		components[i].maxY = -1;
		components[i].sumX = 0;
		components[i].sumY = 0;
	}
	for (int y = 0; y < th; y++)
		for (int x = 0; x < tw; x++)
		{
			aBYTE label = labels[y * tw + x];
			if (!label) continue;
			COMPONENT_INFO& info = components[label];
			info.count++;
			info.sumX += x;
			info.sumY += y;
			if (x < info.minX) info.minX = x;
			if (x > info.maxX) info.maxX = x;
			if (y < info.minY) info.minY = y;
			if (y > info.maxY) info.maxY = y;
		}

	int faceLeft = face.left / 2;
	int faceRight = (face.left + face.width) / 2;
	int faceTop = face.top;
	int faceBottom = face.top + face.height * 3 / 5;
	int bestScore = INT_MAX;
	bool found = false;
	for (int i = 1; i <= regionCount && i < 256; i++)
	{
		if (components[i].count < MIN_COMPONENT_PIXELS) continue;
		int ix = components[i].sumX / components[i].count;
		int iy = components[i].sumY / components[i].count;
		int iw = components[i].maxX - components[i].minX + 1;
		int ih = components[i].maxY - components[i].minY + 1;
		int isize = iw * iw + ih * ih;
		if (isize >= MAX_EYE_SIZE || ix < faceLeft || ix > faceRight ||
			iy < faceTop || iy > faceBottom) continue;
		for (int j = i + 1; j <= regionCount && j < 256; j++)
		{
			if (components[j].count < MIN_COMPONENT_PIXELS) continue;
			int jx = components[j].sumX / components[j].count;
			int jy = components[j].sumY / components[j].count;
			int jw = components[j].maxX - components[j].minX + 1;
			int jh = components[j].maxY - components[j].minY + 1;
			int jsize = jw * jw + jh * jh;
			int dx = AbsInt(ix - jx);
			int dy = AbsInt(iy - jy);
			if (jsize >= MAX_EYE_SIZE || dy >= MAX_EYE_Y_DIFF ||
				dx <= MIN_EYE_X_DIFF || dx >= MAX_EYE_X_DIFF ||
				jx < faceLeft || jx > faceRight || jy < faceTop || jy > faceBottom)
				continue;
			int score = dy * 20 + AbsInt(dx - 22) * 3 +
				AbsInt(components[i].count - components[j].count);
			if (score < bestScore)
			{
				bestScore = score;
				leftEye->x = (ix < jx ? ix : jx) * 4;
				leftEye->y = (ix < jx ? iy : jy) * 4;
				rightEye->x = (ix < jx ? jx : ix) * 4;
				rightEye->y = (ix < jx ? jy : iy) * 4;
				found = true;
			}
		}
	}
	return found;
}

static bool InitializeTraceModels(BUF_STRUCT* pBS, const aRect& leftRect,
	const aRect& rightRect, const aRect& noseRect, int w, int h)
{
	TRACE_OBJECT* objects[3] = {
		&pBS->pOtherVars->objLefteye, &pBS->pOtherVars->objRighteye,
		&pBS->pOtherVars->objNose};
	aRect rects[3] = {leftRect, rightRect, noseRect};
	FeatureVector features[3];
	for (int i = 0; i < 3; i++)
	{
		if (!IsRectValid(rects[i], w, h) ||
			!ExtractFeatureFromImage(&features[i], pBS->grayBmp, w, h, rects[i]))
			return false;
	}
	for (int i = 0; i < 3; i++)
	{
		CopyFeatureVector(&objects[i]->fvObject_org, &features[i]);
		CopyFeatureVector(&objects[i]->fvObject, &features[i]);
		objects[i]->rcObject = rects[i];
		objects[i]->spdxObj = objects[i]->spdyObj = 0;
		objects[i]->nMinDist = 0;
		objects[i]->bBrokenTrace = false;
		objects[i]->bSaveit = false;
		objects[i]->nBrokenTimes = 0;
	}
	return true;
}


static void PublishEyeObservation(FATIGUE_SHARED_STATE* state, bool valid,
	int closure, bool blinkEvent, int blinkDuration)
{
	state->frameSequence++;
	state->faceValid = 1;
	state->observationValid = valid ? 1 : 0;
	state->modelValid = sModelValid ? 1 : 0;
	state->blinkEvent = blinkEvent ? 1 : 0;
	state->closurePercent = valid ? (aBYTE)ClampInt(closure, 0, 100) : 0;
	state->eyeState = valid ? (aBYTE)sEyeMachineState : (aBYTE)EYE_STATE_UNKNOWN;
	if (blinkEvent)
	{
		state->lastBlinkDurationFrames = (WORD)blinkDuration;
		state->blinkCount++;
		state->totalBlinkDurationFrames += blinkDuration;
	}
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
	ResetLocalState(true);
	sLoggedInitialization = false;
}

DLL_EXP int ON_PLUGINCTRL(int nMode, void* pParameter)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	return 0;
}

DLL_EXP void ON_PLUGINRUN(int w, int h, BYTE* pYBits, BYTE* pUBits, BYTE* pVBits, BYTE* pBuffer)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	if (!pBuffer || !pYBits || w < 64 || h < 64 || (w & 15))
		return;

	BUF_STRUCT* pBS = (BUF_STRUCT*)pBuffer;
	if (pBS->bNotInited || !pBS->grayBmp_1d16 || !pBS->TempImage1d8 ||
		!pBS->colorBmp || !pBS->pOtherVars)
		return;

	if (sSharedBuffer != pBuffer || sFrameWidth != w || sFrameHeight != h)
	{
		ResetLocalState(true);
		sSharedBuffer = pBuffer;
		sFrameWidth = w;
		sFrameHeight = h;
		pBS->nImageQueueIndex = -1;
		pBS->nLastImageIndex = -1;
	}

	FATIGUE_SHARED_STATE* state = GetFatigueState(pBS);
	EnsureFatigueState(state);
	state->blinkEvent = 0;
	state->alarmEdge = 0;

	int tw = w / 4;
	int th = h / 4;
	int imageSize = tw * th;
	bool faceValid = IsFaceValid(pBS, w, h);
	if (!faceValid)
	{
		InvalidateDetection(pBS);
		pBS->nFVTop = 0;
		pBS->bLastEyeChecked = false;
		sNoFaceFrames++;
		state->frameSequence++;
		state->faceValid = 0;
		state->observationValid = 0;
		state->eyeState = EYE_STATE_UNKNOWN;
		state->closurePercent = 0;
		state->noFaceFrames = (WORD)ClampInt(sNoFaceFrames, 0, 65535);
		if (sNoFaceFrames >= BLINK_NO_FACE_RESET)
		{
			int noFaceFrames = sNoFaceFrames;
			ResetLocalState(true);
			sNoFaceFrames = noFaceFrames;
			pBS->nImageQueueIndex = -1;
			pBS->nLastImageIndex = -1;
			state->modelValid = 0;
		}
		return;
	}

	sNoFaceFrames = 0;
	state->faceValid = 1;
	state->noFaceFrames = 0;

	aBYTE* filtered = myHeapAlloc(imageSize);
	if (!filtered)
	{
		PublishEyeObservation(state, false, 0, false, 0);
		return;
	}
	BlurGrayImage(pBS->grayBmp_1d16, filtered, tw, th);

	if (pBS->nImageQueueIndex < 0 || pBS->nImageQueueIndex >= 8)
	{
		for (int i = 0; i < 8; i++)
			memcpy(pBS->pImageQueue[i], filtered, imageSize);
		pBS->nImageQueueIndex = 0;
		pBS->nLastImageIndex = 0;
		memset(pBS->TempImage1d8, 0, imageSize);
		PublishEyeObservation(state, false, 0, false, 0);
		myHeapFree(filtered);
		if (!sLoggedInitialization)
		{
			ShowDebugMessage("BlinkEyeCheck: history initialized (%dx%d)", tw, th);
			sLoggedInitialization = true;
		}
		return;
	}

	int nextIndex = (pBS->nImageQueueIndex + 1) & 7;
	int lastIndex = (nextIndex - HISTORY_FRAME_OFFSET + 8) & 7;
	aBYTE* historical = pBS->pImageQueue[lastIndex];
	pBS->nLastImageIndex = lastIndex;
	for (int i = 0; i < imageSize; i++)
		pBS->TempImage1d8[i] = (aBYTE)AbsInt((int)filtered[i] - historical[i]);
	memcpy(pBS->pImageQueue[nextIndex], filtered, imageSize);
	pBS->nImageQueueIndex = nextIndex;

	int threshold = SelectDifferenceThreshold(pBS->TempImage1d8, tw, th,
		HISTOGRAM_PIXEL_COUNT);
	aBYTE* mask = myHeapAlloc(imageSize);
	if (!mask)
	{
		PublishEyeObservation(state, false, 0, false, 0);
		myHeapFree(filtered);
		return;
	}
	memset(mask, 0, imageSize);
	if (threshold >= MIN_DIFFERENCE_THRESHOLD)
	{
		for (int y = 0; y < th; y++)
		{
			for (int x = 0; x < tw; x++)
			{
				int index = y * tw + x;
				bool inFace = x >= pBS->rcnFace.left / 2 &&
					x <= (pBS->rcnFace.left + pBS->rcnFace.width) / 2 &&
					y >= pBS->rcnFace.top &&
					y <= pBS->rcnFace.top + pBS->rcnFace.height * 3 / 5;
				mask[index] = inFace && pBS->TempImage1d8[index] > threshold ? 255 : 0;
			}
		}
	}

	aBYTE* morphologyTemp = myHeapAlloc(imageSize);
	if (!morphologyTemp)
	{
		PublishEyeObservation(state, false, 0, false, 0);
		myHeapFree(mask);
		myHeapFree(filtered);
		return;
	}
	HorizontalOpen(mask, tw, th, morphologyTemp);
	int regionCount = RegionMark(mask, tw, th);
	aPOINT detectedLeft = {-1, -1};
	aPOINT detectedRight = {-1, -1};
	bool pairFound = FindEyePair(mask, tw, th, regionCount, pBS->rcnFace,
		&detectedLeft, &detectedRight);

	bool trackerReady = pBS->nFVTop == 1 && pBS->bLastEyeChecked &&
		!pBS->pOtherVars->objLefteye.bBrokenTrace &&
		!pBS->pOtherVars->objRighteye.bBrokenTrace &&
		!pBS->pOtherVars->objNose.bBrokenTrace;
	if (trackerReady)
	{
		sLeftEyeRect = pBS->pOtherVars->objLefteye.rcObject;
		sRightEyeRect = pBS->pOtherVars->objRighteye.rcObject;
		sNoseRect = pBS->pOtherVars->objNose.rcObject;
		sHasEyeRects = IsRectValid(sLeftEyeRect, w, h) &&
			IsRectValid(sRightEyeRect, w, h) && IsRectValid(sNoseRect, w, h);
		if (!sHasEyeRects)
		{
			trackerReady = false;
			pBS->bLastEyeChecked = false;
		}
	}

	if (!trackerReady)
	{
		sHasEyeRects = false;
		if (sModelValid)
		{
			sModelValid = false;
			sEyeMachineState = EYE_STATE_UNKNOWN;
			sClosingFrames = sClosedFrames = sReopeningFrames = 0;
		}
	}

	bool pigmentConfirmed = false;
	if (!trackerReady && pairFound)
	{
		int eyeDistance = detectedRight.x - detectedLeft.x;
		aRect leftRect = MakeEyeRect(detectedLeft, eyeDistance, w, h);
		aRect rightRect = MakeEyeRect(detectedRight, eyeDistance, w, h);
		aRect noseRect = MakeNoseRect(detectedLeft, detectedRight, eyeDistance,
			pBS->rcnFace, w, h);
		bool geometryValid = eyeDistance > 0 && eyeDistance <= w / 2 &&
			leftRect.width <= w / 4 && leftRect.height <= h / 4 &&
			IsRectValid(leftRect, w, h) && IsRectValid(rightRect, w, h) &&
			IsRectValid(noseRect, w, h) && PointInsideFace(detectedLeft, pBS->rcnFace) &&
			PointInsideFace(detectedRight, pBS->rcnFace);
		if (geometryValid)
		{
			aBYTE leftPatch[EYE_PATCH_WIDTH * EYE_PATCH_HEIGHT * 2];
			aBYTE rightPatch[EYE_PATCH_WIDTH * EYE_PATCH_HEIGHT * 2];
			ExtractPlanarPatch(pBS->colorBmp, w, h, leftRect,
				EYE_PATCH_WIDTH, EYE_PATCH_HEIGHT, leftPatch, true);
			ExtractPlanarPatch(pBS->colorBmp, w, h, rightRect,
				EYE_PATCH_WIDTH, EYE_PATCH_HEIGHT, rightPatch, true);
			aPOINT leftPupil, rightPupil;
			bool leftConfirmed = ConfirmEyePigment(leftPatch, &leftPupil);
			bool rightConfirmed = ConfirmEyePigment(rightPatch, &rightPupil);
			pigmentConfirmed = leftConfirmed && rightConfirmed;
			if (pigmentConfirmed)
			{
				aRect historyLeft = leftRect;
				historyLeft.left /= 4; historyLeft.top /= 4;
				historyLeft.width /= 4; historyLeft.height /= 4;
				aRect historyRight = rightRect;
				historyRight.left /= 4; historyRight.top /= 4;
				historyRight.width /= 4; historyRight.height /= 4;
				bool historyValid = IsRectValid(historyLeft, tw, th) &&
					IsRectValid(historyRight, tw, th);
				if (historyValid && InitializeTraceModels(pBS, leftRect, rightRect,
					noseRect, w, h))
				{
					sLeftEyeRect = leftRect;
					sRightEyeRect = rightRect;
					sNoseRect = noseRect;
					sHasEyeRects = true;
					pBS->ptTheLeftEye = detectedLeft;
					pBS->ptTheRightEye = detectedRight;
					pBS->ptTheNose.x = noseRect.left + noseRect.width / 2;
					pBS->ptTheNose.y = noseRect.top + noseRect.height / 2;
					pBS->EyePosConfirm = true;
					pBS->EyeBallConfirm = true;
					ExtractPlanarPatch(pBS->colorBmp, w, h, noseRect,
						NOSE_PATCH_WIDTH, NOSE_PATCH_HEIGHT, st_nose, true);

					// The motion pair represents the reopening phase. The historical
					// image is used to initialize the corresponding closed-eye templates.
					memcpy(open_eye_left, leftPatch, sizeof(open_eye_left));
					memcpy(open_eye_right, rightPatch, sizeof(open_eye_right));
					ExtractGrayPatch(historical, tw, th, historyLeft,
						EYE_PATCH_WIDTH, EYE_PATCH_HEIGHT, close_eye_left);
					ExtractGrayPatch(historical, tw, th, historyRight,
						EYE_PATCH_WIDTH, EYE_PATCH_HEIGHT, close_eye_right);
					sModelValid = true;
					trackerReady = true;
					pBS->nFVTop = 1;
					pBS->bLastEyeChecked = true;
				}
			}
		}
	}

	bool observationValid = false;
	int closure = 0;
	bool blinkEvent = false;
	int blinkDuration = 0;
	if (sModelValid && sHasEyeRects && IsRectValid(sLeftEyeRect, w, h) &&
		IsRectValid(sRightEyeRect, w, h))
	{
		aBYTE currentLeft[EYE_PATCH_WIDTH * EYE_PATCH_HEIGHT * 2];
		aBYTE currentRight[EYE_PATCH_WIDTH * EYE_PATCH_HEIGHT * 2];
		ExtractPlanarPatch(pBS->colorBmp, w, h, sLeftEyeRect,
			EYE_PATCH_WIDTH, EYE_PATCH_HEIGHT, currentLeft, true);
		ExtractPlanarPatch(pBS->colorBmp, w, h, sRightEyeRect,
			EYE_PATCH_WIDTH, EYE_PATCH_HEIGHT, currentRight, true);
		closure = EstimateClosure(currentLeft, currentRight);
		int openDistance = (GrayPatchDistance(currentLeft, open_eye_left,
			EYE_PATCH_WIDTH * EYE_PATCH_HEIGHT) + GrayPatchDistance(currentRight,
			open_eye_right, EYE_PATCH_WIDTH * EYE_PATCH_HEIGHT)) / 2;
		int closedDistance = (GrayPatchDistance(currentLeft, close_eye_left,
			EYE_PATCH_WIDTH * EYE_PATCH_HEIGHT) + GrayPatchDistance(currentRight,
			close_eye_right, EYE_PATCH_WIDTH * EYE_PATCH_HEIGHT)) / 2;
		observationValid = openDistance < MODEL_REJECT_DISTANCE &&
			closedDistance < MODEL_REJECT_DISTANCE;
		if (observationValid)
		{
			switch (sEyeMachineState)
			{
			case EYE_STATE_UNKNOWN:
				if (closure <= OPEN_CLOSURE_THRESHOLD || pigmentConfirmed)
					sEyeMachineState = EYE_STATE_OPEN;
				else if (closure >= CLOSED_CLOSURE_THRESHOLD)
					sEyeMachineState = EYE_STATE_CLOSING;
				break;
			case EYE_STATE_OPEN:
				if (closure >= CLOSED_CLOSURE_THRESHOLD)
				{
					sEyeMachineState = EYE_STATE_CLOSING;
					sClosingFrames = 1;
					sClosedFrames = 0;
				}
				break;
			case EYE_STATE_CLOSING:
				if (closure >= CLOSED_CLOSURE_THRESHOLD)
				{
					sClosingFrames++;
					if (sClosingFrames >= 2)
					{
						sEyeMachineState = EYE_STATE_CLOSED;
						sClosedFrames = sClosingFrames;
					}
				}
				else if (closure <= OPEN_CLOSURE_THRESHOLD)
				{
					sEyeMachineState = EYE_STATE_OPEN;
					sClosingFrames = 0;
				}
				break;
			case EYE_STATE_CLOSED:
				if (closure <= OPEN_CLOSURE_THRESHOLD)
				{
					sEyeMachineState = EYE_STATE_REOPENING;
					sReopeningFrames = 1;
				}
				else
				{
					sClosedFrames++;
				}
				break;
			case EYE_STATE_REOPENING:
				if (closure <= OPEN_CLOSURE_THRESHOLD)
				{
					sReopeningFrames++;
					if (sReopeningFrames >= 2)
					{
						blinkEvent = true;
						blinkDuration = sClosedFrames + sReopeningFrames;
						sEyeMachineState = EYE_STATE_OPEN;
						sClosingFrames = sClosedFrames = sReopeningFrames = 0;
					}
				}
				else if (closure >= CLOSED_CLOSURE_THRESHOLD)
				{
					sEyeMachineState = EYE_STATE_CLOSED;
					sReopeningFrames = 0;
				}
				break;
			}
			if (sEyeMachineState == EYE_STATE_OPEN && closure <= OPEN_CLOSURE_THRESHOLD)
			{
				UpdateTemplate(open_eye_left, currentLeft,
					EYE_PATCH_WIDTH * EYE_PATCH_HEIGHT);
				UpdateTemplate(open_eye_right, currentRight,
					EYE_PATCH_WIDTH * EYE_PATCH_HEIGHT);
			}
			else if (sEyeMachineState == EYE_STATE_CLOSED && closure >= CLOSED_CLOSURE_THRESHOLD)
			{
				UpdateTemplate(close_eye_left, currentLeft,
					EYE_PATCH_WIDTH * EYE_PATCH_HEIGHT);
				UpdateTemplate(close_eye_right, currentRight,
					EYE_PATCH_WIDTH * EYE_PATCH_HEIGHT);
			}
		}
	}

	if (!observationValid)
	{
		sEyeMachineState = EYE_STATE_UNKNOWN;
		sClosingFrames = 0;
		sClosedFrames = 0;
		sReopeningFrames = 0;
	}
	if (!sHasEyeRects || !observationValid)
		InvalidateDetection(pBS);
	pBS->bLastEyeChecked = sModelValid && sHasEyeRects && trackerReady;
	if (!pBS->bLastEyeChecked)
		pBS->nFVTop = 0;
	state->modelValid = sModelValid ? 1 : 0;
	PublishEyeObservation(state, observationValid, closure, blinkEvent, blinkDuration);
	if (blinkEvent)
		ShowDebugMessage("BlinkEyeCheck: blink #%lu duration=%d frames",
			state->blinkCount, blinkDuration);

	if (sHasEyeRects)
	{
		DrawRectangle(pYBits, w, h, sLeftEyeRect, RGB(255, 210, 0), true);
		DrawRectangle(pYBits, w, h, sRightEyeRect, RGB(255, 210, 0), true);
		DrawRectangle(pYBits, w, h, sNoseRect, RGB(160, 160, 0), true);
		DrawCross(pYBits, w, h, pBS->ptTheLeftEye.x, pBS->ptTheLeftEye.y,
			5, RGB(255, 255, 255), true);
		DrawCross(pYBits, w, h, pBS->ptTheRightEye.x, pBS->ptTheRightEye.y,
			5, RGB(255, 255, 255), true);
		DrawCross(pYBits, w, h, pBS->ptTheNose.x, pBS->ptTheNose.y,
			5, RGB(180, 180, 180), true);
	}
	if (bLastPlugin)
	{
		for (int i = 0; i < imageSize; i++)
			morphologyTemp[i] = mask[i] ? 255 : 0;
		CopyToRect(morphologyTemp, pYBits, tw, th, w, h, 0, 0, true);
	}

	myHeapFree(morphologyTemp);
	myHeapFree(mask);
	myHeapFree(filtered);
}

DLL_EXP void ON_PLUGINEXIT()
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	ResetLocalState(true);
}
