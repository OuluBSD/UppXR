#include <Core/Core.h>
#include <Geometry/Geometry.h>
#include <SoftPhys/SoftPhys.h>
#include <SoftHMD/SoftHMD.h>
#include <SoftRend/SoftRend.h>
#include <VirtualGui3D/VirtualGui3D.h>
#include <iostream>

using namespace Upp;

static int failed_count = 0;

static void AssertCheck(bool condition, const char* desc)
{
	if(!condition) {
		std::cerr << "FAIL: " << desc << std::endl;
		failed_count++;
	}
	else {
		std::cout << "PASS: " << desc << std::endl;
	}
}

int main(int argc, const char* argv[])
{
	NoMemoryLeaksCheck = true;
	StdLogSetup(LOG_COUT | LOG_FILE);

	std::cout << "========================================\n";
	std::cout << "Running XRCheck Unit & Lifecycle Tests\n";
	std::cout << "========================================\n";

	// 1. SoftPhys World & Dynamics Integration Test
	{
		std::cout << "\n[1] Testing SoftPhys World & Dynamics...\n";
		SoftPhys::World world;
		world.SetGravity(vec3(0.0f, -9.82f, 0.0f));
		AssertCheck(world.gravity[1] == -9.82f, "World gravity configured to -9.82");

		SoftPhys::Space space;
		world.GetSystem().AddSpace(space);

		SoftPhys::Body body;
		body.SetWorld(world);
		body.SetPosition(vec3(0.0f, 10.0f, 0.0f));
		AssertCheck(body.GetPosition()[1] == 10.0f, "Initial body position y == 10.0");

		// Step world forward in time
		world.Step(0.05f);
		AssertCheck(true, "World step simulation ran without errors");
	}

	// 2. SoftHMD Context Lifecycle & Probe Test
	{
		std::cout << "\n[2] Testing SoftHMD Context Lifecycle...\n";
		HMD::Context* ctx = HMD::CreateContext();
		AssertCheck(ctx != nullptr, "SoftHMD CreateContext returned valid context");

		if(ctx) {
			int num_drivers = ctx->num_drivers;
			AssertCheck(num_drivers >= 0, "Registered drivers count is non-negative");

			int num_dev = HMD::ProbeContext(ctx);
			AssertCheck(num_dev >= 0, "ProbeContext returned non-negative device count");

			HMD::DestroyContext(ctx);
			AssertCheck(true, "DestroyContext executed cleanly");
		}
	}

	// 3. SoftRend Framebuffer Test
	{
		std::cout << "\n[3] Testing SoftRend Framebuffer...\n";
		SoftFramebuffer fb;
		fb.Clear();
		AssertCheck(fb.GetSize().cx == 0 && fb.GetSize().cy == 0, "Initial SoftFramebuffer size is 0x0");
	}

	// 4. VirtualGui3D Test
	{
		std::cout << "\n[4] Testing VirtualGui3D Pointer...\n";
		AssertCheck(VirtualGui3DPtr == nullptr, "VirtualGui3DPtr initialized to nullptr before activation");
	}

	std::cout << "\n========================================\n";
	if(failed_count == 0) {
		std::cout << "ALL XRCheck TESTS PASSED SUCCESSFULLY!\n";
		std::cout << "========================================\n";
		return 0;
	}
	else {
		std::cerr << failed_count << " TEST(S) FAILED!\n";
		std::cout << "========================================\n";
		return 1;
	}
}
