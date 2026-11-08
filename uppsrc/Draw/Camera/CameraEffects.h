#ifndef _Draw_Camera_CameraEffects_h_
#define _Draw_Camera_CameraEffects_h_

class CameraEffect : public VideoEffect {
public:
	virtual ~CameraEffect() = default;
	virtual void Process(CameraFrame& frame) = 0;
	void Process(VideoFrame& frame) override { Process(static_cast<CameraFrame&>(frame)); }
};

class CameraEffectChain {
	Vector<CameraEffect*> effects;
	bool enabled = true;

public:
	void Clear() { effects.Clear(); }
	void Add(CameraEffect& eff) { effects.Add(&eff); }
	int GetCount() const { return effects.GetCount(); }
	void SetEnabled(bool e) { enabled = e; }
	bool IsEnabled() const { return enabled; }
	void Process(CameraFrame& frame) {
		if (!enabled)
			return;
		for (int i = 0; i < effects.GetCount(); i++)
			effects[i]->Process(frame);
	}
};

class CameraNoopEffect : public CameraEffect {
public:
	void Process(CameraFrame& frame) override { (void)frame; }
};

#endif
