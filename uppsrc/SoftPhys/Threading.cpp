#include "SoftPhys.h"


NAMESPACE_UPP
namespace SoftPhys {


Threading::Threading() {
	
}


void Threading::Add(ThreadPool& p) {
	pools.Add(&p);
	p.threading = this;
}

void Threading::Detach() {
	for (ThreadPool* p : pools)
		p->threading = this;
	pools.Clear();
}

void Threading::DetachWorld() {
	if (world) {
		world->threading = 0;
		world = 0;
	}
}


}
END_UPP_NAMESPACE
