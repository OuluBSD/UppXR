#ifndef _ComputerVision_TemplateMatchCtrl_h_
#define _ComputerVision_TemplateMatchCtrl_h_

#ifdef GUI
#include <CtrlLib/CtrlLib.h>
#include <ComputerVision/ComputerVision.h>

NAMESPACE_UPP

// TemplateMatchCtrl — interactive template-matching visualizer.
//
// Shows two side-by-side panels:
//   Left:  response map (higher coefficient → brighter; inverted for SQDIFF).
//          A crosshair marks the best-match location.
//   Right: full input image with a yellow rectangle drawn at the match position.
//
// Right-clicking the ctrl pops a method-selection menu; the chosen method is
// stored locally and does not affect any external state.
//
// Usage:
//   TemplateMatchCtrl ctrl;
//   ctrl.SetInputs(full_image, template_image);   // triggers recompute
//   ctrl.SetMethod(TM_SQDIFF_NORMED);              // override; also recomputes

class TemplateMatchCtrl : public Ctrl {
public:
	typedef TemplateMatchCtrl CLASSNAME;

	// Set both images and recompute the match.
	void SetInputs(const Image& full, const Image& templ);
	// Change just the match method and recompute.
	void SetMethod(TemplateMatchMethod m);

	TemplateMatchMethod GetMethod() const { return method_; }

	virtual void Paint(Draw& w) override;
	virtual void RightDown(Point, dword) override;

private:
	Image  full_img_;
	Image  templ_img_;
	Image  score_img_;
	TemplateMatchMethod method_ = TM_CCOEFF_NORMED;
	Point  best_pt_  = {0, 0};
	double min_val_  = 0;
	double max_val_  = 0;
	bool   computed_ = false;

	void Compute();

	static Rect  FitRect(Size isz, const Rect& area);
	static void  DrawPanel(Draw& w, const Rect& panel, const Image& img,
	                       const char* title, bool show_crosshair,
	                       Point best, Size templ_sz, const Image& templ_img);
	static void  DrawBestMarker(Draw& w, const Rect& dr, Size src_sz, Point p, Color c);
	static void  DrawMatchRect(Draw& w, const Rect& dr, Size src_sz,
	                           Point top_left, Size templ_sz, Color c);
	static const char* MethodName(TemplateMatchMethod m);
};

END_UPP_NAMESPACE

#endif
#endif
