#include <XRClient/XRClient.h>

using namespace Upp;

GUI_APP_MAIN
{
	StdLogSetup(LOG_FILE);
	String demo = GetCommandLineArgValue("--send-demo");
	if(!demo.IsEmpty()) {
		int port = GetCommandLineArgInt("--listen-port", 8889);
		int wait_ms = GetCommandLineArgInt("--wait-for-server-ms", 15000);
		if(!RunXrClientCliDemo(demo, port, wait_ms)) SetExitCode(1);
		return;
	}
	XRClientWindow().Run();
}
