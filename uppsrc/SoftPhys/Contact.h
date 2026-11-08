#ifndef _SoftPhys_Contact_h_
#define _SoftPhys_Contact_h_

NAMESPACE_UPP
namespace SoftPhys {


struct Contact : Object {
	//RTTI_DECL1(Contact, Object)
	using Object::Object;
	
	
	
	Contact() {}
	
	void Visit(Vis& v) override {VIS_THIS(Object);}
};


}
END_UPP_NAMESPACE

#endif
