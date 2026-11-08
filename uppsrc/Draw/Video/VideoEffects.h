#ifndef _Draw_Video_VideoEffects_h_
#define _Draw_Video_VideoEffects_h_

class VideoEffect {
public:
	virtual ~VideoEffect() = default;
	virtual void Process(VideoFrame& frame) = 0;
};

class VideoEffectChain {
	Vector<VideoEffect*> effects;

public:
	void Clear() { effects.Clear(); }
	void Add(VideoEffect& eff) { effects.Add(&eff); }
	int GetCount() const { return effects.GetCount(); }
	void Process(VideoFrame& frame) {
		for (int i = 0; i < effects.GetCount(); i++)
			effects[i]->Process(frame);
	}
};

#endif
