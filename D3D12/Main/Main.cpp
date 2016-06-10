#include <memory>           
#include <windows.h>

#include <ShapesApp\ShapesApp.h>

#include <tbb\tbb_thread.h>

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE /*hPrevInstance*/, _In_ LPSTR /*lpCmdLine*/, _In_ int /*nShowCmd*/) {
	ShapesApp app{ hInstance };
	app.Initialize();
	app.Run();

	return 0;
}