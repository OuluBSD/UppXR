#include "ComputerVision.h"

NAMESPACE_UPP

bool HaveCollinearPoints(const Vector<Keypoint>& points, int count) {
	int j, k, i = 0;
	for (i = 0; i < count; i++) {
		for (j = i + 1; j < count; j++) {
			for (k = j + 1; k < count; k++) {
				double x1 = points[i].x, y1 = points[i].y;
				double x2 = points[j].x, y2 = points[j].y;
				double x3 = points[k].x, y3 = points[k].y;
				if (abs(x1 * (y2 - y3) + x2 * (y3 - y1) + x3 * (y1 - y2)) < 1e-2)
					return true;
			}
		}
	}
	return false;
}

Affine2D::Affine2D() {
	T0.SetSize(3, 3, 1);
	T1.SetSize(3, 3, 1);
	AtA.SetSize(6, 6, 1);
	AtB.SetSize(6, 1, 1);
	a_mt.SetSize(6, 6, 1);
	b_mt.SetSize(6, 1, 1);
}

void Affine2D::Error(const Vector<Keypoint>& from, const Vector<Keypoint>& to, const FloatMat& model, Vector<double>& err) {
	const float* m = model.data.Begin();
	auto f = from.Begin();
	auto t = to.Begin();
	ASSERT(from.GetCount() == to.GetCount());
	err.SetCount(from.GetCount());
	for (auto& e : err) {
		const Keypoint& pt0 = *f;
		const Keypoint& pt1 = *t;
		
		double dx = (m[0] * pt0.x + m[1] * pt0.y + m[2]) - pt1.x;
		double dy = (m[3] * pt0.x + m[4] * pt0.y + m[5]) - pt1.y;
		e = (dx * dx + dy * dy);
		
		f++;
		t++;
	}
}

bool Affine2D::CheckSubset(const Vector<Keypoint>& from, const Vector<Keypoint>& to, int count) {
	return true;
}

bool Affine2D::Run(const Vector<Keypoint>& from, Vector<Keypoint>& to, FloatMat& model) {
	int count = from.GetCount();
	if (count < 3)
		return false;
	
	auto& Ata = AtA.data;
	auto& Atb = AtB.data;
	
	for (int i = 0; i < 36; i++) Ata[i] = 0.0;
	for (int i = 0; i < 6; i++) Atb[i] = 0.0;
	
	for (int i = 0; i < count; i++) {
		double x = from[i].x;
		double y = from[i].y;
		double u = to[i].x;
		double v = to[i].y;
		
		Ata[0] += x * x; Ata[1] += x * y; Ata[2] += x;
		Ata[7] += y * y; Ata[8] += y;
		Ata[14] += 1.0;
		
		Atb[0] += x * u; Atb[1] += y * u; Atb[2] += u;
		Atb[3] += x * v; Atb[4] += y * v; Atb[5] += v;
	}
	
	Ata[6] = Ata[1]; Ata[12] = Ata[2]; Ata[13] = Ata[8];
	for (int i = 0; i < 3; i++) {
		for (int j = 0; j < 3; j++) {
			Ata[(i + 3) * 6 + (j + 3)] = Ata[i * 6 + j];
		}
	}
	
	return false; 
}

Homography2D::Homography2D() {
	mLtL.SetSize(9, 9, 1);
	Evec.SetSize(9, 9, 1);
	Eval.SetSize(1, 9, 1);
	T0.SetSize(3, 3, 1);
	T1.SetSize(3, 3, 1);
}

void Homography2D::Error(const Vector<Keypoint>& from, const Vector<Keypoint>& to, const FloatMat& model, Vector<double>& err) {
	const float* m = model.data.Begin();
	auto f = from.Begin();
	auto t = to.Begin();
	ASSERT(from.GetCount() == to.GetCount());
	err.SetCount(from.GetCount());
	for (auto& e : err) {
		const Keypoint& pt0 = *f;
		const Keypoint& pt1 = *t;
		
		double ww = 1.0 / (m[6] * pt0.x + m[7] * pt0.y + m[8]);
		double dx = (m[0] * pt0.x + m[1] * pt0.y + m[2]) * ww - pt1.x;
		double dy = (m[3] * pt0.x + m[4] * pt0.y + m[5]) * ww - pt1.y;
		e = (dx * dx + dy * dy);
		
		f++;
		t++;
	}
}

bool Homography2D::CheckSubset(const Vector<Keypoint>& from, Vector<Keypoint>& to, int count) {
	return true; 
}

bool Homography2D::Run(const Vector<Keypoint>& from, Vector<Keypoint>& to, FloatMat& model) {
	auto& md = model.data;
	auto& t0d = T0.data;
	auto& t1d = T1.data;
	auto& LtL = mLtL.data;
	
	int count = from.GetCount();
	if (count < 4) return false;
	
	double x = 0.0, y = 0.0, X = 0.0, Y = 0.0;
	
	// norm
	double cmx = 0.0, cmy = 0.0, cMx = 0.0, cMy = 0.0;
	
	for (int i = 0; i < count; ++i) {
		cmx += to[i].x;
		cmy += to[i].y;
		cMx += from[i].x;
		cMy += from[i].y;
	}
	
	cmx /= count;
	cmy /= count;
	cMx /= count;
	cMy /= count;
	
	double smx = 0.0, sMx = 0.0;
	for (int i = 0; i < count; ++i) {
		double dx = to[i].x - cmx;
		double dy = to[i].y - cmy;
		smx += sqrt(dx * dx + dy * dy);
		dx = from[i].x - cMx;
		dy = from[i].y - cMy;
		sMx += sqrt(dx * dx + dy * dy);
	}
	
	smx /= count;
	sMx /= count;
	
	if (smx < 1e-6) smx = 1.0;
	if (sMx < 1e-6) sMx = 1.0;
	
	smx = M_SQRT2 / smx;
	sMx = M_SQRT2 / sMx;
	
	t0d[0] = (float)sMx; t0d[1] = 0;          t0d[2] = (float)(-cMx * sMx);
	t0d[3] = 0;          t0d[4] = (float)sMx; t0d[5] = (float)(-cMy * sMx);
	t0d[6] = 0;          t0d[7] = 0;          t0d[8] = 1;
	
	t1d[0] = (float)smx; t1d[1] = 0;          t1d[2] = (float)(-cmx * smx);
	t1d[3] = 0;          t1d[4] = (float)smx; t1d[5] = (float)(-cmy * smx);
	t1d[6] = 0;          t1d[7] = 0;          t1d[8] = 1;

	// construct system
	double mLtL_db[81];
	for (int i = 0; i < 81; i++) mLtL_db[i] = 0.0;

	for (int i = 0; i < count; ++i) {
		X = (from[i].x - cMx) * sMx;
		Y = (from[i].y - cMy) * sMx;
		x = (to[i].x - cmx) * smx;
		y = (to[i].y - cmy) * smx;
		
		double r[2][9] = {
			{ -X, -Y, -1,  0,  0,  0, x*X, x*Y, x },
			{  0,  0,  0, -X, -Y, -1, y*X, y*Y, y }
		};
		
		for (int k = 0; k < 9; k++) {
			for (int l = 0; l < 9; l++) {
				mLtL_db[k * 9 + l] += r[0][k] * r[0][l] + r[1][k] * r[1][l];
			}
		}
	}
	for (int i = 0; i < 81; i++) LtL[i] = (float)mLtL_db[i];
	
	// ensure symmetry
	for (int i = 0; i < 9; i++) {
		for (int j = 0; j < i; j++) {
			LtL[i * 9 + j] = LtL[j * 9 + i];
		}
	}
	
	EigenVV(mLtL, &Evec, &Eval);

	// The smallest eigenvalue is at the end (index 8) because EigenVV sorts them descending.
	// The corresponding eigenvector is the 9th column (index 8) of Evec.
	double h[9];
	for (int i = 0; i < 9; ++i) {
		h[i] = Evec.data[i * 9 + 8];
	}
	
	// manual denormalization
	double s1_inv = 1.0 / smx;
	double s0 = sMx;
	
	// M = T1_inv * H_norm
	double m[9];
	m[0] = s1_inv * h[0] + cmx * h[6];
	m[1] = s1_inv * h[1] + cmx * h[7];
	m[2] = s1_inv * h[2] + cmx * h[8];
	
	m[3] = s1_inv * h[3] + cmy * h[6];
	m[4] = s1_inv * h[4] + cmy * h[7];
	m[5] = s1_inv * h[5] + cmy * h[8];
	
	m[6] = h[6];
	m[7] = h[7];
	m[8] = h[8];
	
	// H = M * T0
	double tx = -cMx * s0;
	double ty = -cMy * s0;
	
	md[0] = (float)(m[0] * s0);
	md[1] = (float)(m[1] * s0);
	md[2] = (float)(m[0] * tx + m[1] * ty + m[2]);
	
	md[3] = (float)(m[3] * s0);
	md[4] = (float)(m[4] * s0);
	md[5] = (float)(m[3] * tx + m[4] * ty + m[5]);
	
	md[6] = (float)(m[6] * s0);
	md[7] = (float)(m[7] * s0);
	md[8] = (float)(m[6] * tx + m[7] * ty + m[8]);
	
	// set bottom right to 1.0 for better numerical stability/display
	if (abs(md[8]) > 1e-10) {
		double norm = 1.0 / md[8];
		for (int i = 0; i < 9; i++) md[i] *= (float)norm;
	}

	return true;
}

END_UPP_NAMESPACE
