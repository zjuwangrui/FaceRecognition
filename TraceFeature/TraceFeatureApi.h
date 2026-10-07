#ifndef __ANTISLEEP_TRACEFEATUREAPI_H__
#define __ANTISLEEP_TRACEFEATUREAPI_H__

#include "BufStruct.h"

#ifdef TRACEFEATURE_EXPORTS
#define TRACEFEATURE_API DLL_EXP
#else
#define TRACEFEATURE_API DLL_INP
#endif

TRACEFEATURE_API void InitFeatureBuffer(BUF_STRUCT* pBS);
TRACEFEATURE_API void InitFeatureVector(FeatureVector* pThis);
TRACEFEATURE_API bool ExtractFeatureFromImage(FeatureVector* pFV, const aBYTE* pImageBits,
	int nLineW, int nH, aRect rcSample);
TRACEFEATURE_API bool CopyFeatureVector(FeatureVector* pDest, const FeatureVector* pSource);
TRACEFEATURE_API bool UpdateVectorsFrom(FeatureVector* pFV, FeatureVector* aFV, int nOrgWeight);
TRACEFEATURE_API aPOINT CompareFromImage(FeatureVector* pFV, aBYTE* pImageBits, int nLineW,
	int nH, aRect rcSampleRC, aRect rcRange, int* nMinDist, FeatureVector* theMinFV);
TRACEFEATURE_API int FV_Distance(FeatureVector* pFV, FeatureVector* aFV,
	int nFaceClrWeight, int nLevelWeight);

#undef TRACEFEATURE_API

#endif
