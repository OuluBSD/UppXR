#ifndef _ComputerVision_VideoStabilizer_h_
#define _ComputerVision_VideoStabilizer_h_


enum {
	MM_AFFINE,
	MM_HOMOGRAPHY
};



template <class T>
T& GetAt(int idx, int len, Vector<T>& items) {
	if( idx < 0 ) idx -= (((idx-len+1)/len)*len);
	if( idx >= len ) idx %= len;
    return items[idx];
}

int GetRingIndex(int idx, int len);
void GetMotion(FloatMat& M, int from, int to, Vector<FloatMat>& motions);


#endif
