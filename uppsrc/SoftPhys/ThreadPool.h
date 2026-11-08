#ifndef _SoftPhys_ThreadPool_h_
#define _SoftPhys_ThreadPool_h_

NAMESPACE_UPP
namespace SoftPhys {


struct ThreadPool : Object {
	//RTTI_DECL1(ThreadPool, Object)
	using Object::Object;
	
	Threading* threading = 0;
	
	
	ThreadPool() {}
	
	void ClearThreading() {TODO}
	void Visit(Vis& v) override {VIS_THIS(Object);}
};


}
END_UPP_NAMESPACE

#endif
