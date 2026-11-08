#ifndef _SoftHMD_Daemon_h_
#define _SoftHMD_Daemon_h_

NAMESPACE_HMD_BEGIN


class SoftHMDService : public DaemonService {
	HMD::System sys;
	
	
public:
	typedef SoftHMDService CLASSNAME;
	SoftHMDService();
	bool Init(String name) override;
	void Update() override;
	void Stop() override;
	void Deinit() override;
	
	void SetSensorCallback(Callback1<GeomEvent&> cb);
	
};


NAMESPACE_HMD_END

#endif
