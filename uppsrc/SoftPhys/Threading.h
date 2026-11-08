#ifndef _SoftPhys_Threading_h_
#define _SoftPhys_Threading_h_

NAMESPACE_UPP
namespace SoftPhys {


struct Threading : Object {
	//RTTI_DECL1(Threading, Object)
	using Object::Object;
	
	Vector<ThreadPool*> pools;
	World* world = 0;
	
	
	Threading();
	
	void Visit(Vis& v) override {VIS_THIS(Object);}
	void Add(ThreadPool& p);
	void Detach();
	void DetachWorld();
	
};


}
END_UPP_NAMESPACE

#endif
