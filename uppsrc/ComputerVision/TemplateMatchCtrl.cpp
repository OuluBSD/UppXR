#ifdef GUI
#include "TemplateMatchCtrl.h"

NAMESPACE_UPP

void TemplateMatchCtrl::SetInputs(const Image& full, const Image& templ) {
	full_img_  = full;
	templ_img_ = templ;
	Compute();
}

void TemplateMatchCtrl::SetMethod(TemplateMatchMethod m) {
	method_ = m;
	Compute();
}

void TemplateMatchCtrl::Compute() {
	computed_ = false;
	score_img_.Clear();

	if(full_img_.IsEmpty() || templ_img_.IsEmpty()) {
		Refresh();
		return;
	}

	ByteMat full_m, templ_m;
	ImageToGrayByteMat(full_img_,  full_m);
	ImageToGrayByteMat(templ_img_, templ_m);

	if(full_m.cols < templ_m.cols || full_m.rows < templ_m.rows) {
		Refresh();
		return;
	}

	FloatMat res;
	MatchTemplate(full_m, templ_m, res, method_);
	Point min_p, max_p;
	MinMaxLoc(res, &min_val_, &max_val_, &min_p, &max_p);

	bool sqdiff = (method_ == TM_SQDIFF || method_ == TM_SQDIFF_NORMED);
	best_pt_ = sqdiff ? min_p : max_p;
	score_img_ = FloatMatToGrayImage(res, sqdiff);
	computed_  = !score_img_.IsEmpty();
	Refresh();
}

const char* TemplateMatchCtrl::MethodName(TemplateMatchMethod m) {
	switch(m) {
	case TM_CCOEFF:        return "TM_CCOEFF";
	case TM_CCOEFF_NORMED: return "TM_CCOEFF_NORMED";
	case TM_CCORR:         return "TM_CCORR";
	case TM_CCORR_NORMED:  return "TM_CCORR_NORMED";
	case TM_SQDIFF:        return "TM_SQDIFF";
	case TM_SQDIFF_NORMED: return "TM_SQDIFF_NORMED";
	default:               return "unknown";
	}
}

Rect TemplateMatchCtrl::FitRect(Size isz, const Rect& area) {
	if(isz.cx <= 0 || isz.cy <= 0 || area.GetWidth() <= 0 || area.GetHeight() <= 0)
		return area;
	double sx = (double)area.GetWidth()  / isz.cx;
	double sy = (double)area.GetHeight() / isz.cy;
	double sc = min(sx, sy);
	int w = max(1, (int)(isz.cx * sc));
	int h = max(1, (int)(isz.cy * sc));
	int x = area.left + (area.GetWidth()  - w) / 2;
	int y = area.top  + (area.GetHeight() - h) / 2;
	return RectC(x, y, w, h);
}

void TemplateMatchCtrl::DrawBestMarker(Draw& w, const Rect& dr, Size src_sz, Point p, Color c) {
	double sx = (double)dr.GetWidth()  / src_sz.cx;
	double sy = (double)dr.GetHeight() / src_sz.cy;
	int x = dr.left + (int)((p.x + 0.5) * sx);
	int y = dr.top  + (int)((p.y + 0.5) * sy);
	w.DrawLine(x - 8, y, x + 8, y, 2, c);
	w.DrawLine(x, y - 8, x, y + 8, 2, c);
}

void TemplateMatchCtrl::DrawMatchRect(Draw& w, const Rect& dr, Size src_sz,
                                      Point top_left, Size templ_sz, Color c) {
	double sx = (double)dr.GetWidth()  / src_sz.cx;
	double sy = (double)dr.GetHeight() / src_sz.cy;
	int x  = dr.left + (int)(top_left.x  * sx);
	int y  = dr.top  + (int)(top_left.y  * sy);
	int rw = max(2, (int)(templ_sz.cx * sx));
	int rh = max(2, (int)(templ_sz.cy * sy));
	w.DrawRect(x, y,        rw, 2,  c);
	w.DrawRect(x, y+rh-2,   rw, 2,  c);
	w.DrawRect(x, y,        2,  rh, c);
	w.DrawRect(x+rw-2, y,   2,  rh, c);
}

void TemplateMatchCtrl::DrawPanel(Draw& w, const Rect& panel, const Image& img,
                                  const char* title, bool show_crosshair,
                                  Point best, Size templ_sz, const Image& templ_img) {
	bool dark = IsDarkColorFace();
	Color bg     = dark ? Color(32, 32, 32) : White();
	Color border = dark ? Color(72, 72, 72) : SColorShadow();
	Color text_c = dark ? White() : Black();

	w.DrawRect(panel, bg);
	w.DrawRect(panel.left,           panel.top,      panel.GetWidth(), 1, border);
	w.DrawRect(panel.left,           panel.bottom-1, panel.GetWidth(), 1, border);
	w.DrawRect(panel.left,           panel.top,      1, panel.GetHeight(), border);
	w.DrawRect(panel.right-1,        panel.top,      1, panel.GetHeight(), border);

	w.DrawText(panel.left + 6, panel.top + 4, title, Arial(12).Bold(), text_c);

	Rect canvas = panel;
	canvas.top += 24;
	if(canvas.GetWidth() <= 4 || canvas.GetHeight() <= 4)
		return;

	if(!img.IsEmpty()) {
		Rect dr = FitRect(img.GetSize(), canvas);
		w.DrawImage(dr.left, dr.top, dr.GetWidth(), dr.GetHeight(), img);

		if(show_crosshair) {
			DrawBestMarker(w, dr, img.GetSize(), best, LtYellow());
		} else {
			DrawMatchRect(w, dr, img.GetSize(), best, templ_sz, Yellow());
			// Draw template thumbnail in lower-right corner if available
			if(!templ_img.IsEmpty()) {
				Size tsz  = templ_img.GetSize();
				int  tdisp = min(64, min(canvas.GetWidth() / 4, canvas.GetHeight() / 4));
				if(tdisp >= 8) {
					int tw = max(1, (int)((double)tsz.cx / tsz.cy * tdisp));
					int th = tdisp;
					int tx = panel.right  - tw - 6;
					int ty = panel.bottom - th - 6;
					w.DrawImage(tx, ty, tw, th, templ_img);
					w.DrawRect(tx-1,    ty-1,   tw+2, 1,     border);
					w.DrawRect(tx-1,    ty+th,  tw+2, 1,     border);
					w.DrawRect(tx-1,    ty-1,   1,    th+2,  border);
					w.DrawRect(tx+tw,   ty-1,   1,    th+2,  border);
				}
			}
		}
	} else {
		w.DrawText(canvas.left + 8, canvas.top + 8, "No image", Arial(11), border);
	}
}

void TemplateMatchCtrl::Paint(Draw& w) {
	Size sz = GetSize();
	bool dark = IsDarkColorFace();
	w.DrawRect(sz, dark ? Color(24, 24, 24) : SColorPaper());

	if(!computed_) {
		String msg = (full_img_.IsEmpty() || templ_img_.IsEmpty())
		           ? "Set full image and template to begin."
		           : "Template larger than image — no match possible.";
		w.DrawText(10, 10, msg, Arial(12), dark ? LtGray() : Gray());
		return;
	}

	const int gap = 8;
	int half = sz.cx / 2;
	Rect left  = RectC(gap,           gap, max(4, half - gap * 2), max(4, sz.cy - gap * 2));
	Rect right = RectC(half + gap,    gap, max(4, sz.cx - half - gap * 2), max(4, sz.cy - gap * 2));

	String score_title = String("Response map — ") + MethodName(method_);
	DrawPanel(w, left,  score_img_, score_title,                           true,  best_pt_, templ_img_.GetSize(), templ_img_);
	DrawPanel(w, right, full_img_,  "Full image (yellow = best match)",    false, best_pt_, templ_img_.GetSize(), templ_img_);

	// Footer: best location + score range
	String info = Format("best=(%d,%d)  min=%.4g  max=%.4g",
	                     best_pt_.x, best_pt_.y, min_val_, max_val_);
	Color text_c = dark ? LtGray() : SColorText();
	w.DrawText(gap, sz.cy - 18, info, Arial(10), text_c);
}

void TemplateMatchCtrl::RightDown(Point, dword) {
	TemplateMatchMethod cur = method_;
	struct M { const char* label; TemplateMatchMethod m; };
	static const M meths[] = {
		{"TM_CCOEFF",        TM_CCOEFF},
		{"TM_CCOEFF_NORMED", TM_CCOEFF_NORMED},
		{"TM_CCORR",         TM_CCORR},
		{"TM_CCORR_NORMED",  TM_CCORR_NORMED},
		{"TM_SQDIFF",        TM_SQDIFF},
		{"TM_SQDIFF_NORMED", TM_SQDIFF_NORMED},
	};
	MenuBar::Execute([&](Bar& bar) {
		for(auto& e : meths) {
			TemplateMatchMethod m = e.m;
			bar.Add(e.label, [=] { SetMethod(m); }).Check(cur == m);
		}
	});
}

END_UPP_NAMESPACE
#endif
