#include "Camera.h"

NAMESPACE_UPP

namespace {

struct StereoSourceEntry : Moveable<StereoSourceEntry> {
	String label;
	StereoSourceFactory factory = nullptr;
	int priority = 0;
};

VectorMap<String, StereoSourceEntry>& StereoRegistry() {
	static VectorMap<String, StereoSourceEntry> registry;
	return registry;
}

Mutex& StereoRegistryMutex() {
	static Mutex m;
	return m;
}

}

void RegisterStereoSource(const String& id, const String& label, StereoSourceFactory factory, int priority) {
	if (id.IsEmpty() || !factory)
		return;
	Mutex::Lock __(StereoRegistryMutex());
	int idx = StereoRegistry().Find(id);
	if (idx < 0) {
		StereoSourceEntry& entry = StereoRegistry().Add(id);
		entry.label = label;
		entry.factory = factory;
		entry.priority = priority;
		return;
	}
	StereoSourceEntry& entry = StereoRegistry()[idx];
	entry.label = label;
	entry.factory = factory;
	entry.priority = priority;
}

Vector<StereoSourceInfo> GetStereoSources() {
	Vector<StereoSourceInfo> out;
	{
		Mutex::Lock __(StereoRegistryMutex());
		out.Reserve(StereoRegistry().GetCount());
		for (int i = 0; i < StereoRegistry().GetCount(); i++) {
			StereoSourceInfo info;
			info.id = StereoRegistry().GetKey(i);
			info.label = StereoRegistry()[i].label;
			info.priority = StereoRegistry()[i].priority;
			out.Add(pick(info));
		}
	}
	Sort(out, [](const StereoSourceInfo& a, const StereoSourceInfo& b) {
		if (a.priority != b.priority)
			return a.priority > b.priority;
		return a.label < b.label;
	});
	return out;
}

One<StereoSource> CreateStereoSource(const String& id) {
	Mutex::Lock __(StereoRegistryMutex());
	int idx = StereoRegistry().Find(id);
	if (idx < 0)
		return One<StereoSource>();
	StereoSourceFactory factory = StereoRegistry()[idx].factory;
	if (!factory)
		return One<StereoSource>();
	return (*factory)();
}

bool HasStereoSource(const String& id) {
	Mutex::Lock __(StereoRegistryMutex());
	return StereoRegistry().Find(id) >= 0;
}

bool SplitStereoImage(const Image& src, Image& left, Image& right) {
	Size sz = src.GetSize();
	if (sz.cx < 4 || sz.cy < 4)
		return false;
	if (sz.cx & 1)
		return false;
	Size half(sz.cx / 2, sz.cy);
	left = Crop(src, RectC(0, 0, half.cx, half.cy));
	right = Crop(src, RectC(half.cx, 0, half.cx, half.cy));
	return true;
}

bool JoinStereoImage(const Image& left, const Image& right, Image& out) {
	if (left.IsEmpty() || right.IsEmpty())
		return false;
	int lw = left.GetWidth();
	int rw = right.GetWidth();
	int lh = left.GetHeight();
	int rh = right.GetHeight();
	if (lw <= 0 || rw <= 0 || lh <= 0 || rh <= 0)
		return false;

	int w = lw + rw;
	int h = max(lh, rh);
	ImageBuffer ib(w, h);
	RGBA black = Black();
	RGBA* dst = ib.Begin();
	for (int i = 0, n = w * h; i < n; i++)
		dst[i] = black;

	int copy_h = min(lh, rh);
	for (int y = 0; y < copy_h; y++) {
		const RGBA* lsrc = left[y];
		const RGBA* rsrc = right[y];
		RGBA* row = ib[y];
		memcpy(row, lsrc, lw * sizeof(RGBA));
		memcpy(row + lw, rsrc, rw * sizeof(RGBA));
	}
	out = ib;
	return !out.IsEmpty();
}

END_UPP_NAMESPACE
