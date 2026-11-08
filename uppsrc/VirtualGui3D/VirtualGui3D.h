#ifndef _VirtualGui3D_VirtualGui3D_h_
#define _VirtualGui3D_VirtualGui3D_h_

#include <Draw/Draw.h>




NAMESPACE_UPP

class CtrlEvent;

struct VirtualGui3D {
	//RTTI_DECL0(VirtualGui3D)
	
	virtual bool        Poll(UPP::CtrlEvent& e) = 0;
	virtual Size        GetSize() = 0;
	virtual void        SetTitle(String title) = 0;
	virtual SystemDraw& BeginDraw() = 0;
	virtual void        CommitDraw() = 0;
	virtual uint32      GetTickCount() = 0;
	virtual void        WaitEvent(int ms) = 0;
	virtual void        WakeUpGuiThread() = 0;
	virtual bool        IsWaitingEvent() = 0;
	
};

extern VirtualGui3D* VirtualGui3DPtr;

END_UPP_NAMESPACE

#if 0

NAMESPACE_UPP

class ImageDraw : public SImageDraw {
public:
	ImageDraw(Size sz) : SImageDraw(sz) {}
	ImageDraw(Size sz, int stride) : SImageDraw(sz, stride) {}
	ImageDraw(int cx, int cy) : SImageDraw(cx, cy) {}
	
};



#define GUIPLATFORM_CTRL_TOP_DECLS   Ctrl *owner_window;



#define GUIPLATFORM_PASTECLIP_DECLS \
	bool dnd; \
	friend struct DnDLoop; \

#define GUIPLATFORM_INCLUDE_AFTER <VirtualGui3D/After.h>
#define GUIPLATFORM_INCLUDE_AFTER_ECSLIB <VirtualGui3D/AfterEcsLib.h>


END_UPP_NAMESPACE


#endif
#endif
