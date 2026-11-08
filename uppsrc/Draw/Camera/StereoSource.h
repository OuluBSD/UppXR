#ifndef _Draw_Camera_StereoSource_h_
#define _Draw_Camera_StereoSource_h_

struct StereoSourceInfo : Moveable<StereoSourceInfo> {
	String id;
	String label;
	int priority = 0;
};

class CameraEffectChain;

class StereoSource {
public:
	virtual ~StereoSource() = default;
	virtual String GetName() const = 0;
	virtual bool Start() = 0;
	virtual void Stop() = 0;
	virtual bool IsRunning() const = 0;
	virtual bool ReadFrame(CameraFrame& left, CameraFrame& right, bool prefer_bright = false) = 0;
	virtual bool PeakFrame(CameraFrame& left, CameraFrame& right, bool prefer_bright = false) = 0;
	virtual void SetVerbose(bool v) {}
	virtual bool SupportsBrightDark() const { return false; }
	virtual bool SetOption(const String& key, int value) { return false; }
	virtual bool SetOption(const String& key, const String& value) { return false; }
	virtual void GetStatsMap(VectorMap<String, String>& out) {}
	virtual void SetEffects(CameraEffectChain* chain) { effects = chain; }
	CameraEffectChain* GetEffects() const { return effects; }

protected:
	void ApplyEffects(CameraFrame& frame) { if (effects) effects->Process(frame); }

private:
	CameraEffectChain* effects = nullptr;
};

typedef One<StereoSource> (*StereoSourceFactory)();

void RegisterStereoSource(const String& id, const String& label, StereoSourceFactory factory, int priority = 0);
Vector<StereoSourceInfo> GetStereoSources();
One<StereoSource> CreateStereoSource(const String& id);
bool HasStereoSource(const String& id);

bool SplitStereoImage(const Image& src, Image& left, Image& right);
bool JoinStereoImage(const Image& left, const Image& right, Image& out);

#endif
