#include <XRClient/XRClient.h>

using namespace Upp;

CONSOLE_APP_MAIN
{
	StdLogSetup(LOG_COUT | LOG_FILE);
	String demo = GetCommandLineArgValue("--send-demo");
	if(demo.IsEmpty()) {
		Cerr() << "Usage: XRClientCli --send-demo=cube|grid [--listen-port=8889] [--wait-for-server-ms=15000]\n";
		SetExitCode(2);
		return;
	}
	int port = GetCommandLineArgInt("--listen-port", 8889);
	int wait_ms = GetCommandLineArgInt("--wait-for-server-ms", 15000);
	if(!RunXrClientCliDemo(demo, port, wait_ms)) SetExitCode(1);
}
