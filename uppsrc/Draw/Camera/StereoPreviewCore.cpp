#include "Camera.h"
#include <cmath>

NAMESPACE_UPP

namespace {

RGBA SampleBilinear(const Image& img, float x, float y) {
	Size sz = img.GetSize();
	if (sz.cx <= 0 || sz.cy <= 0 || !std::isfinite(x) || !std::isfinite(y))
		return RGBA{0,0,0,0};
	x = Clamp(x, 0.0f, (float)(sz.cx - 1));
	y = Clamp(y, 0.0f, (float)(sz.cy - 1));
	int x0 = (int)floor(x);
	int y0 = (int)floor(y);
	int x1 = min(x0 + 1, sz.cx - 1);
	int y1 = min(y0 + 1, sz.cy - 1);
	float fx = x - x0;
	float fy = y - y0;
	const RGBA* row0 = img[y0];
	const RGBA* row1 = img[y1];
	auto lerp = [](float a, float b, float t) { return a + (b - a) * t; };
	RGBA c00 = row0[x0];
	RGBA c10 = row0[x1];
	RGBA c01 = row1[x0];
	RGBA c11 = row1[x1];
	RGBA out;
	out.r = (byte)Clamp((int)lerp(lerp(c00.r, c10.r, fx), lerp(c01.r, c11.r, fx), fy), 0, 255);
	out.g = (byte)Clamp((int)lerp(lerp(c00.g, c10.g, fx), lerp(c01.g, c11.g, fx), fy), 0, 255);
	out.b = (byte)Clamp((int)lerp(lerp(c00.b, c10.b, fx), lerp(c01.b, c11.b, fx), fy), 0, 255);
	out.a = (byte)Clamp((int)lerp(lerp(c00.a, c10.a, fx), lerp(c01.a, c11.a, fx), fy), 0, 255);
	return out;
}

}

Image RectifyAndRotateOnePass(const Image& src, const StereoPreviewLensParams& lp, float yaw, float pitch, float roll, Size out_sz) {
	if (src.IsEmpty() || out_sz.cx <= 0 || out_sz.cy <= 0 || lp.f <= 1e-6f)
		return Image();

	ImageBuffer out(out_sz);
	float cx_out = out_sz.cx * 0.5f;
	float cy_out = out_sz.cy * 0.5f;
	mat4 rot = AxesMat(yaw, pitch, roll);
	mat4 rot_inv = rot.GetTransposed();

	for (int y = 0; y < out_sz.cy; y++) {
		RGBA* dst = out[y];
		float dy_out = y - cy_out;
		for (int x = 0; x < out_sz.cx; x++) {
			float dx_out = x - cx_out;
			vec3 dir_out(dx_out, -dy_out, IS_NEGATIVE_Z ? -lp.f : lp.f);
			dir_out.Normalize();
			vec3 dir_cam = (rot_inv * dir_out.Embed()).Splice();
			dir_cam.Normalize();
			float zf = IS_NEGATIVE_Z ? -dir_cam[2] : dir_cam[2];
			if (zf <= 1e-3f) {
				dst[x] = RGBA{0,0,0,255};
				continue;
			}
			float nx = dir_cam[0] / zf;
			float ny = -dir_cam[1] / zf;
			float r2 = nx*nx + ny*ny;
			float dist_scale = 1.0f + lp.k1 * r2 + lp.k2 * r2 * r2;
			if (dist_scale < 0.1f) dist_scale = 0.1f;
			if (dist_scale > 10.0f) dist_scale = 10.0f;
			float sx = lp.cx + nx * dist_scale * lp.f;
			float sy = lp.cy + ny * dist_scale * lp.f;
			dst[x] = SampleBilinear(src, sx, sy);
		}
	}
	return out;
}

Pointf ProjectPointOnePass(Pointf src_norm, Size src_sz, const StereoPreviewLensParams& lp, float yaw, float pitch, float roll) {
	if (src_sz.cx <= 0 || src_sz.cy <= 0 || lp.f <= 1e-6f)
		return Pointf(-1, -1);

	float sx = (float)(src_norm.x * src_sz.cx);
	float sy = (float)(src_norm.y * src_sz.cy);

	float dx = (sx - lp.cx) / lp.f;
	float dy = (sy - lp.cy) / lp.f;
	double rd = sqrt(dx*dx + dy*dy);

	double ru = rd;
	for (int i = 0; i < 10; i++) {
		double r2 = ru*ru;
		double val = ru * (1.0 + lp.k1*r2 + lp.k2*r2*r2) - rd;
		double deriv = 1.0 + 3.0*lp.k1*r2 + 5.0*lp.k2*r2*r2;
		if (fabs(deriv) < 1e-6) break;
		double step = val / deriv;
		ru -= step;
		if (fabs(step) < 1e-7) break;
	}

	double scale = (rd > 1e-9) ? (ru / rd) : 1.0;
	vec3 dir_cam((float)(dx * scale), (float)(dy * scale), (float)(IS_NEGATIVE_Z ? -1.0 : 1.0));
	dir_cam.Normalize();

	mat4 rot = AxesMat(yaw, pitch, roll);
	vec3 dir_rect = (rot * dir_cam.Embed()).Splice();

	float zf = IS_NEGATIVE_Z ? -dir_rect[2] : dir_rect[2];
	if (zf <= 1e-3f) return Pointf(-1, -1);

	float px = dir_rect[0] / zf * lp.f + lp.cx;
	float py = -dir_rect[1] / zf * lp.f + lp.cy;

	return Pointf(px, py);
}

Pointf UnprojectPointOnePass(Pointf rect_px, Size rect_sz, const StereoPreviewLensParams& lp, float yaw, float pitch, float roll) {
	if (rect_sz.cx <= 0 || rect_sz.cy <= 0 || lp.f <= 1e-6f)
		return Pointf(-1, -1);

	float cx_out = rect_sz.cx * 0.5f;
	float cy_out = rect_sz.cy * 0.5f;
	float dx_out = (float)rect_px.x - cx_out;
	float dy_out = (float)rect_px.y - cy_out;

	vec3 dir_out(dx_out, -dy_out, IS_NEGATIVE_Z ? -lp.f : lp.f);
	dir_out.Normalize();

	mat4 rot = AxesMat(yaw, pitch, roll);
	mat4 rot_inv = rot.GetTransposed();
	vec3 dir_cam = (rot_inv * dir_out.Embed()).Splice();
	dir_cam.Normalize();

	float zf = IS_NEGATIVE_Z ? -dir_cam[2] : dir_cam[2];
	if (zf <= 1e-3f) return Pointf(-1, -1);

	float nx = dir_cam[0] / zf;
	float ny = -dir_cam[1] / zf;

	float r2 = nx*nx + ny*ny;
	float dist_scale = 1.0f + lp.k1 * r2 + lp.k2 * r2 * r2;

	float sx = lp.cx + nx * dist_scale * lp.f;
	float sy = lp.cy + ny * dist_scale * lp.f;

	return Pointf(sx, sy);
}

void StereoPreviewEngine::SetCalibration(const StereoCalibrationData& data) {
	calib = data;
	has_calib = calib.is_enabled;
}

StereoPreviewLensParams StereoPreviewEngine::BuildLensParams(const StereoCalibrationData& data, const Size& sz) {
	StereoPreviewLensParams lp;
	lp.f = (float)data.angle_to_pixel[0];
	if (fabs(lp.f) < 1e-6f)
		lp.f = (float)sz.cx * 0.5f;
	lp.cx = data.principal_point[0] > 0 ? data.principal_point[0] : sz.cx * 0.5f;
	lp.cy = data.principal_point[1] > 0 ? data.principal_point[1] : sz.cy * 0.5f;
	if (fabs(lp.f) > 1e-6f) {
		lp.k1 = (float)(data.angle_to_pixel[2] / lp.f);
		lp.k2 = (float)(data.angle_to_pixel[3] / lp.f);
	}
	return lp;
}

Image StereoPreviewEngine::ApplyProcessingScale(const Image& src, float scale) {
	if (src.IsEmpty() || scale >= 0.999f)
		return src;
	Size sz = src.GetSize();
	int w = max(1, (int)roundf(sz.cx * scale));
	int h = max(1, (int)roundf(sz.cy * scale));
	return Rescale(src, Size(w, h));
}

Image StereoPreviewEngine::ApplyTintGreen(const Image& src) {
	if (src.IsEmpty())
		return Image();
	ImageBuffer ib(src.GetSize());
	Copy(ib, Point(0,0), src, src.GetSize());
	for (RGBA& p : ib) {
		p.r = (byte)(p.r * 0.3);
		p.b = (byte)(p.b * 0.3);
	}
	return ib;
}

Image StereoPreviewEngine::ApplyTintViolet(const Image& src) {
	if (src.IsEmpty())
		return Image();
	ImageBuffer ib(src.GetSize());
	Copy(ib, Point(0,0), src, src.GetSize());
	for (RGBA& p : ib) {
		p.g = (byte)(p.g * 0.3);
	}
	return ib;
}

Image StereoPreviewEngine::BlendImages(const Image& a, const Image& b, float alpha) {
	if (a.IsEmpty())
		return b;
	if (b.IsEmpty())
		return a;
	Size sz = a.GetSize();
	if (b.GetSize() != sz)
		return a;
	alpha = Clamp(alpha, 0.0f, 1.0f);
	ImageBuffer out(sz);
	const RGBA* pa = ~a;
	const RGBA* pb = ~b;
	RGBA* po = out.Begin();
	int count = sz.cx * sz.cy;
	for (int i = 0; i < count; i++) {
		po[i].r = (byte)Clamp((int)(pa[i].r * alpha + pb[i].r * (1.0f - alpha)), 0, 255);
		po[i].g = (byte)Clamp((int)(pa[i].g * alpha + pb[i].g * (1.0f - alpha)), 0, 255);
		po[i].b = (byte)Clamp((int)(pa[i].b * alpha + pb[i].b * (1.0f - alpha)), 0, 255);
		po[i].a = 255;
	}
	return out;
}

Image StereoPreviewEngine::ComputeDiff(const Image& a, const Image& b) {
	if (a.IsEmpty() || b.IsEmpty())
		return Image();
	Size sz = a.GetSize();
	if (b.GetSize() != sz)
		return Image();
	ImageBuffer out(sz);
	const RGBA* pa = ~a;
	const RGBA* pb = ~b;
	RGBA* po = out.Begin();
	int count = sz.cx * sz.cy;
	for (int i = 0; i < count; i++) {
		po[i].r = (byte)abs((int)pa[i].r - (int)pb[i].r);
		po[i].g = (byte)abs((int)pa[i].g - (int)pb[i].g);
		po[i].b = (byte)abs((int)pa[i].b - (int)pb[i].b);
		po[i].a = 255;
	}
	return out;
}

Image StereoPreviewEngine::DrawCrosshair(const Image& src) {
	if (src.IsEmpty())
		return Image();
	ImageBuffer out(src.GetSize());
	Copy(out, Point(0,0), src, src.GetSize());
	int cx = out.GetWidth() / 2;
	int cy = out.GetHeight() / 2;
	for (int x = max(0, cx - 20); x < min(out.GetWidth(), cx + 20); x++)
		out[cy][x] = RGBA{255, 255, 255, 255};
	for (int y = max(0, cy - 20); y < min(out.GetHeight(), cy + 20); y++)
		out[y][cx] = RGBA{255, 255, 255, 255};
	return out;
}

Image StereoPreviewEngine::DrawEpipolar(const Image& src, int step) {
	if (src.IsEmpty() || step <= 0)
		return Image();
	ImageBuffer out(src.GetSize());
	Copy(out, Point(0,0), src, src.GetSize());
	for (int y = step; y < out.GetHeight(); y += step) {
		for (int x = 0; x < out.GetWidth(); x++)
			out[y][x] = RGBA{0, 255, 255, 255};
	}
	return out;
}

void StereoPreviewEngine::Process(const Image& left_in, const Image& right_in, StereoPreviewOutput& out) {
	out.left = Image();
	out.right = Image();
	out.overlay_mode = false;

	Image left = ApplyProcessingScale(left_in, settings.processing_scale);
	Image right = ApplyProcessingScale(right_in, settings.processing_scale);
	if (left.IsEmpty() || right.IsEmpty())
		return;

	Image left_display = left;
	Image right_display = right;

	if (settings.rectified_overlay && has_calib) {
		StereoPreviewLensParams lp = BuildLensParams(calib, left.GetSize());
		float yaw_l = (float)-calib.outward_angle;
		float yaw_r = (float)calib.outward_angle;
		float pitch_l = 0.0f;
		float roll_l = 0.0f;
		float pitch_r = (float)calib.right_pitch;
		float roll_r = (float)calib.right_roll;
		left_display = RectifyAndRotateOnePass(left, lp, yaw_l, pitch_l, roll_l, left.GetSize());
		right_display = RectifyAndRotateOnePass(right, lp, yaw_r, pitch_r, roll_r, right.GetSize());
	}

	if (settings.show_difference) {
		out.left = ComputeDiff(left_display, right_display);
		out.right = Image();
		out.overlay_mode = true;
		return;
	}

	Image overlay_left = left_display;
	Image overlay_right = right_display;
	if (settings.tint_overlay) {
		overlay_left = ApplyTintGreen(overlay_left);
		overlay_right = ApplyTintViolet(overlay_right);
	}

	if (settings.overlay_eyes) {
		if (settings.overlay_swap)
			out.left = BlendImages(overlay_right, overlay_left, settings.overlay_alpha);
		else
			out.left = BlendImages(overlay_left, overlay_right, settings.overlay_alpha);
		out.right = Image();
		out.overlay_mode = true;
	} else {
		out.left = overlay_left;
		out.right = overlay_right;
	}

	if (settings.show_crosshair) {
		out.left = DrawCrosshair(out.left);
		if (!out.right.IsEmpty())
			out.right = DrawCrosshair(out.right);
	}
	if (settings.show_epipolar) {
		out.left = DrawEpipolar(out.left);
		if (!out.right.IsEmpty())
			out.right = DrawEpipolar(out.right);
	}
}

END_UPP_NAMESPACE
