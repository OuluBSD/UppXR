#ifndef _VirtualGuiAtom_VirtualGuiAtom_h
#define _VirtualGuiAtom_VirtualGuiAtom_h

#include <CtrlLib/CtrlLib.h>
#include <Eon/Eon.h>

namespace UPP {
	
struct AtomVirtualGui : VirtualGui {
	virtual dword       GetOptions();
	virtual Size        GetSize();
	virtual dword       GetMouseButtons();
	virtual dword       GetModKeys();
	virtual bool        IsMouseIn();
	virtual bool        ProcessEvent(bool *quit);
	virtual void        WaitEvent(int ms);
	virtual bool        IsWaitingEvent();
	virtual void        WakeUpGuiThread();
	virtual void        SetMouseCursor(const Image& image);
	virtual SystemDraw& BeginDraw();
	virtual void        CommitDraw();

	virtual void        Quit();
	//virtual void        HandleSDLEvent(SDL_Event* event);
	
	SystemDraw          sysdraw;
	WindowSystemRef		wins;
	WindowManagerRef	mgr;
	
	void SetTarget(Draw& d);
	//void Attach(SDL_Window *win, SDL_GLContext glcontext);
	//void Detach();

	bool Create(const Rect& rect, const char *title);
	void Destroy();
	
	AtomVirtualGui();
	~AtomVirtualGui();
};

};

#endif
