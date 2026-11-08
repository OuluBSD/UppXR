#include "XRClient.h"

NAMESPACE_UPP

static bool SendAndRequireAck(TcpSocket& peer, int sequence, const String& request)
{
	if(!peer.PutAll(request + '\n')) {
		Cerr() << "XRClient: send failed: " << peer.GetErrorDesc() << '\n';
		return false;
	}
	String response = TrimRight(peer.GetLine(1 << 20));
	Value parsed = ParseJSON(response);
	if(parsed.IsError() || !IsValueMap(parsed)) {
		Cerr() << "XRClient: invalid response: " << response << '\n';
		return false;
	}
	ValueMap map(parsed);
	if(String(map["type"]) != "ack" || int(map["sequence"]) != sequence) {
		Cerr() << "XRClient: expected acknowledgement for sequence " << sequence << ": " << response << '\n';
		return false;
	}
	Cout() << response << '\n';
	return true;
}

bool RunXrClientCliDemo(const String& demo, int listen_port, int wait_ms)
{
	if(demo != "cube" && demo != "grid") {
		Cerr() << "XRClient: supported demos are cube and grid.\n";
		return false;
	}
	TcpSocket listener;
	listener.Timeout(wait_ms);
	if(!listener.Listen(listen_port)) {
		Cerr() << "XRClient: cannot listen on port " << listen_port << ": " << listener.GetErrorDesc() << '\n';
		return false;
	}
	Cout() << "XRClient: waiting for XRServer reverse connection on TCP " << listen_port << "\n";
	TcpSocket peer;
	peer.Timeout(wait_ms);
	if(!peer.Accept(listener)) {
		Cerr() << "XRClient: XRServer did not connect within " << wait_ms << "ms: " << peer.GetErrorDesc() << '\n';
		return false;
	}
	Cout() << "XRClient: connected " << peer.GetPeerAddr() << '\n';
	if(!SendAndRequireAck(peer, 1, "{\"protocol\":1,\"sequence\":1,\"type\":\"hello\"}")) return false;
	String snapshot;
	if(demo == "cube")
		snapshot = "{\"protocol\":1,\"sequence\":2,\"type\":\"snapshot\",\"objects\":[{\"id\":\"rotating-cube\",\"type\":\"cube\",\"position\":[0,0,-3],\"scale\":[1,1,1],\"color\":[0.2,0.8,1,1],\"rotationSpeed\":0.7}]}";
	else
		snapshot = "{\"protocol\":1,\"sequence\":2,\"type\":\"snapshot\",\"objects\":[{\"id\":\"surrounding-grid\",\"type\":\"grid\",\"position\":[0,0,0],\"scale\":[1,1,1],\"color\":[0.12,0.55,1,1]}]}";
	if(!SendAndRequireAck(peer, 2, snapshot)) return false;
	if(!SendAndRequireAck(peer, 3, "{\"protocol\":1,\"sequence\":3,\"type\":\"framebuffer_ascii\"}")) return false;
	Sleep(1000);
	if(!SendAndRequireAck(peer, 4, "{\"protocol\":1,\"sequence\":4,\"type\":\"diagnostics\"}")) return false;
	return true;
}

CalibrationPane::CalibrationPane()
{
	Title("Calibration");
	AddRow("Left viewport X", -250, 250, 0);
	AddRow("Right viewport X", -250, 250, 0);
	AddRow("Viewport Y", -250, 250, 0);
	AddRow("Horizontal rotation", -180, 180, 0);
	AddRow("Vertical rotation", -180, 180, 0);
	AddRow("Pupil distance (mm)", 53, 70, 64);
	AddRow("FOV (degrees x10)", 300, 900, 450);
}

void CalibrationPane::AddRow(const String& text, int min, int max, int value)
{
	Row& row = rows.Add();
	int i = rows.GetCount() - 1;
	row.label.SetLabel(text);
	row.slider.MinMax(min, max);
	row.slider.SetData(value);
	row.slider.WhenAction = [=] { UpdateRow(i); WhenChanged(); };
	Add(row.label.HSizePos(8, 8).TopPos(8 + i * 58, 18));
	Add(row.slider.HSizePos(8, 72).TopPos(27 + i * 58, 20));
	Add(row.value.RightPos(8, 62).TopPos(27 + i * 58, 20));
	UpdateRow(i);
}

void CalibrationPane::UpdateRow(int i) { rows[i].value.SetLabel(AsString((int)rows[i].slider.GetData())); }

void CalibrationPane::SetState(const CalibrationState& s)
{
	int values[] = {s.viewport_left_x, s.viewport_right_x, s.viewport_y, s.horizontal_rotation, s.vertical_rotation, s.pupil_distance_mm, s.fov_tenths};
	for(int i = 0; i < rows.GetCount(); i++) { rows[i].slider.SetData(values[i]); UpdateRow(i); }
}

CalibrationState CalibrationPane::GetState() const
{
	CalibrationState s;
	int* values[] = {&s.viewport_left_x, &s.viewport_right_x, &s.viewport_y, &s.horizontal_rotation, &s.vertical_rotation, &s.pupil_distance_mm, &s.fov_tenths};
	for(int i = 0; i < rows.GetCount(); i++) *values[i] = (int)rows[i].slider.GetData();
	return s;
}

XRClientWindow::XRClientWindow()
{
	Title("XRClient Calibration").Sizeable().Zoomable();
	AddFrame(menu); menu.Set(THISBACK(MainMenu));
	AddFrame(toolbar); toolbar.Set(THISBACK(UpdateToolbar));
	AddFrame(status); status = "Waiting for XRServer reverse connection on TCP 8889";
	telemetry.Add(telemetry_table.SizePos()); telemetry.Title("Sensors");
	telemetry_table.AddColumn("Sensor"); telemetry_table.AddColumn("Value");
	telemetry_table.Add("Orientation", "Waiting for XRServer telemetry");
	telemetry_table.Add("Compass", "Waiting for XRServer telemetry");
	telemetry_table.Add("Accelerometer", "Waiting for XRServer telemetry");
	log.Add(log_table.SizePos()); log.Title("Connection log");
	log_table.AddColumn("Event");
	calibration.WhenChanged = THISBACK(ApplyCalibration);
}

void XRClientWindow::DockInit()
{
	Register(calibration); Register(telemetry); Register(log);
	DockLeft(calibration); DockRight(telemetry); DockBottom(log);
	LoadState();
}

void XRClientWindow::MainMenu(Bar& bar) { bar.Sub("XRClient", [=](Bar& sub) { sub.Add("Apply calibration", THISBACK(ApplyCalibration)); sub.Add("Exit", [=] { Break(); }); }); bar.Sub("Window", [=](Bar& sub) { DockWindowMenu(sub); }); }
void XRClientWindow::UpdateToolbar(Bar& bar) { bar.Add("Apply", THISBACK(ApplyCalibration)); }
void XRClientWindow::ApplyCalibration() { state = calibration.GetState(); log_table.Add(Format("Calibration staged: IPD %d mm, FOV %.1f", state.pupil_distance_mm, state.fov_tenths / 10.0)); }
void XRClientWindow::LoadState() { registry.LoadJson("calibration.state", state); calibration.SetState(state); }
void XRClientWindow::SaveState() { state = calibration.GetState(); registry.SaveJson("calibration.state", state); registry.Save(); }
void XRClientWindow::Close() { SaveState(); TopWindow::Close(); }

END_UPP_NAMESPACE
