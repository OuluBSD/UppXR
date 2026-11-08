#ifndef _SoftPhys_JointGroup_h_
#define _SoftPhys_JointGroup_h_

NAMESPACE_UPP
namespace SoftPhys {


struct JointGroup : Object {
	//RTTI_DECL1(JointGroup, Object)
	using Object::Object;
	
	World* world = 0;
	Vector<Joint*> joints;
	
	
	JointGroup();
	
	void Visit(Vis& v) override {VIS_THIS(Object);}
	void Detach();
	
};


}
END_UPP_NAMESPACE

#endif
