#ifndef _XRClient_XRClient_h_
#define _XRClient_XRClient_h_

#include <CtrlLib/CtrlLib.h>
#include <Docking/Docking.h>

NAMESPACE_UPP

struct CalibrationState {
	int viewport_left_x = 0;
	int viewport_right_x = 0;
	int viewport_y = 0;
	int horizontal_rotation = 0;
	int vertical_rotation = 0;
	int pupil_distance_mm = 64;
	int fov_tenths = 450;

	void Jsonize(JsonIO& json) {
		json("viewport_left_x", viewport_left_x)("viewport_right_x", viewport_right_x)("viewport_y", viewport_y)
			("horizontal_rotation", horizontal_rotation)("vertical_rotation", vertical_rotation)
			("pupil_distance_mm", pupil_distance_mm)("fov_tenths", fov_tenths);
	}
};

bool RunXrClientCliDemo(const String& demo, int listen_port, int wait_ms);

class CalibrationPane : public DockableCtrl {
public:
	typedef CalibrationPane CLASSNAME;
	Callback WhenChanged;

	CalibrationPane();
	void SetState(const CalibrationState& state);
	CalibrationState GetState() const;

private:
	struct Row : Moveable<Row> {
		Label label;
		SliderCtrl slider;
		Label value;
	};
	Array<Row> rows;
	void AddRow(const String& label, int min, int max, int value);
	void UpdateRow(int i);
};

class XRClientWindow : public DockWindow {
public:
	typedef XRClientWindow CLASSNAME;
	XRClientWindow();
	void DockInit() override;
	void Close() override;

private:
	MenuBar menu;
	ToolBar toolbar;
	StatusBar status;
	CalibrationPane calibration;
	DockableCtrl telemetry;
	DockableCtrl log;
	ArrayCtrl telemetry_table;
	ArrayCtrl log_table;
	AppRegistry registry;
	CalibrationState state;

	void MainMenu(Bar& bar);
	void UpdateToolbar(Bar& bar);
	void ApplyCalibration();
	void LoadState();
	void SaveState();
};

END_UPP_NAMESPACE
#endif
