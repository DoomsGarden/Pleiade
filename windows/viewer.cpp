#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shlobj.h>
#include <WebView2.h>

#include <filesystem>
#include <functional>
#include <new>
#include <utility>

namespace {

constexpr wchar_t windowClass[] = L"PleiadeViewer";
constexpr wchar_t viewerHost[] = L"pleiade-viewer.local";
constexpr wchar_t viewerUrl[] = L"https://pleiade-viewer.local/index.html";

template<class Interface, class Value>
class Completion final : public Interface {
public:
    using Handler = std::function<HRESULT(HRESULT, Value*)>;

    Completion(REFIID iid, Handler handler) : iid_(iid), handler_(std::move(handler)) {}

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** result) override {
        if (!result) return E_POINTER;
        if (IsEqualIID(iid, IID_IUnknown) || IsEqualIID(iid, iid_)) {
            *result = static_cast<Interface*>(this);
            AddRef();
            return S_OK;
        }
        *result = nullptr;
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() override { return InterlockedIncrement(&references_); }

    ULONG STDMETHODCALLTYPE Release() override {
        const ULONG remaining = InterlockedDecrement(&references_);
        if (!remaining) delete this;
        return remaining;
    }

    HRESULT STDMETHODCALLTYPE Invoke(HRESULT result, Value* value) override {
        return handler_(result, value);
    }

private:
    const IID iid_;
    Handler handler_;
    LONG references_ = 1;
};

using EnvironmentCompletion = Completion<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler,
                                         ICoreWebView2Environment>;
using ControllerCompletion = Completion<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler,
                                        ICoreWebView2Controller>;

class Viewer {
public:
    ~Viewer() {
        CloseController();
        if (loader_) FreeLibrary(loader_);
    }

    bool Start(HINSTANCE instance) {
        WNDCLASSW window{};
        window.lpfnWndProc = HandleWindowMessage;
        window.hInstance = instance;
        window.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
        window.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        window.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
        window.lpszClassName = windowClass;
        if (!RegisterClassW(&window)) return false;
        handle_ = CreateWindowExW(0, windowClass, L"Pléiade", WS_OVERLAPPEDWINDOW,
                                 CW_USEDEFAULT, CW_USEDEFAULT, 1280, 800, nullptr,
                                 nullptr, instance, this);
        return handle_ != nullptr;
    }

    void Run(int showCommand) {
        ShowWindow(handle_, showCommand);
        InitializeWebView();

        MSG message{};
        while (GetMessageW(&message, nullptr, 0, 0) > 0) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        handle_ = nullptr;
        CloseController();
    }

private:
    static LRESULT CALLBACK HandleWindowMessage(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
        if (message == WM_NCCREATE) {
            auto* creation = reinterpret_cast<CREATESTRUCTW*>(lParam);
            SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(creation->lpCreateParams));
        }

        auto* viewer = reinterpret_cast<Viewer*>(GetWindowLongPtrW(window, GWLP_USERDATA));
        if (viewer) {
            switch (message) {
            case WM_SIZE:
                viewer->Resize();
                return 0;
            case WM_SETFOCUS:
                if (viewer->controller_) viewer->controller_->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
                return 0;
            case WM_MOVE:
                if (viewer->controller_) viewer->controller_->NotifyParentWindowPositionChanged();
                return 0;
            case WM_DESTROY:
                viewer->handle_ = nullptr;
                viewer->CloseController();
                PostQuitMessage(0);
                return 0;
            }
        }
        return DefWindowProcW(window, message, wParam, lParam);
    }

    void InitializeWebView() {
        wchar_t modulePath[32768];
        const DWORD length = GetModuleFileNameW(nullptr, modulePath, ARRAYSIZE(modulePath));
        if (!length || length >= ARRAYSIZE(modulePath)) {
            Fail(L"No se pudo encontrar la carpeta de instalación de Pléiade.");
            return;
        }

        PWSTR localAppData = nullptr;
        if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_DEFAULT, nullptr, &localAppData))) {
            Fail(L"No se pudo encontrar la carpeta de datos local de Windows.");
            return;
        }
        const auto profile = std::filesystem::path(localAppData) / L"Pleiade" / L"WebView2";
        CoTaskMemFree(localAppData);

        const auto installPath = std::filesystem::path(modulePath).parent_path();
        viewerFolder_ = installPath / L"viewer";
        loader_ = LoadLibraryW((installPath / L"WebView2Loader.dll").c_str());
        if (!loader_) {
            Fail(L"Falta WebView2Loader.dll. Vuelve a ejecutar el instalador de Pléiade.");
            return;
        }
        const auto createEnvironment = reinterpret_cast<decltype(&CreateCoreWebView2EnvironmentWithOptions)>(
            GetProcAddress(loader_, "CreateCoreWebView2EnvironmentWithOptions"));
        if (!createEnvironment) {
            Fail(L"No se pudo inicializar el cargador de WebView2.");
            return;
        }

        auto* completed = new (std::nothrow) EnvironmentCompletion(
            IID_ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler,
            [this](HRESULT result, ICoreWebView2Environment* environment) {
                return EnvironmentReady(result, environment);
            });
        if (!completed) {
            Fail(L"No hay memoria suficiente para inicializar el visor.");
            return;
        }
        const HRESULT result = createEnvironment(nullptr, profile.c_str(), nullptr, completed);
        completed->Release();
        if (FAILED(result)) ReportEnvironmentError(result);
    }

    HRESULT EnvironmentReady(HRESULT result, ICoreWebView2Environment* environment) {
        if (!handle_) return S_OK;
        if (FAILED(result) || !environment) {
            ReportEnvironmentError(result);
            return S_OK;
        }

        auto* completed = new (std::nothrow) ControllerCompletion(
            IID_ICoreWebView2CreateCoreWebView2ControllerCompletedHandler,
            [this](HRESULT status, ICoreWebView2Controller* controller) {
                return ControllerReady(status, controller);
            });
        if (!completed) {
            Fail(L"No hay memoria suficiente para abrir el visor.");
            return S_OK;
        }
        result = environment->CreateCoreWebView2Controller(handle_, completed);
        completed->Release();
        if (FAILED(result)) Fail(L"No se pudo crear la ventana de WebView2.");
        return S_OK;
    }

    HRESULT ControllerReady(HRESULT result, ICoreWebView2Controller* controller) {
        if (!handle_) return S_OK;
        if (FAILED(result) || !controller) {
            Fail(L"No se pudo abrir la ventana de WebView2.");
            return S_OK;
        }
        controller_ = controller;
        controller_->AddRef();

        ICoreWebView2* browser = nullptr;
        result = controller_->get_CoreWebView2(&browser);
        if (FAILED(result) || !browser) {
            Fail(L"No se pudo acceder al visor de WebView2.");
            return S_OK;
        }
        ICoreWebView2_3* localBrowser = nullptr;
        result = browser->QueryInterface(IID_ICoreWebView2_3, reinterpret_cast<void**>(&localBrowser));
        if (SUCCEEDED(result)) {
            result = localBrowser->SetVirtualHostNameToFolderMapping(
                viewerHost, viewerFolder_.c_str(), COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_DENY);
            localBrowser->Release();
        }
        if (SUCCEEDED(result)) result = browser->Navigate(viewerUrl);
        browser->Release();
        if (FAILED(result)) {
            Fail(L"No se pudo cargar el visor local. Actualiza WebView2 y vuelve a abrir Pléiade.");
            return S_OK;
        }

        Resize();
        controller_->put_IsVisible(TRUE);
        if (GetFocus() == handle_) controller_->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
        return S_OK;
    }

    void Resize() const {
        if (!controller_ || !handle_) return;
        RECT bounds{};
        GetClientRect(handle_, &bounds);
        controller_->put_Bounds(bounds);
    }

    void CloseController() {
        if (!controller_) return;
        controller_->Close();
        controller_->Release();
        controller_ = nullptr;
    }

    void ReportEnvironmentError(HRESULT status) const {
        if (status == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)) {
            Fail(L"Instala Microsoft Edge WebView2 Runtime para ejecutar Pléiade.\n\n"
                 L"https://developer.microsoft.com/microsoft-edge/webview2/");
            return;
        }
        Fail(L"No se pudo iniciar WebView2. Comprueba la instalación de WebView2 y los permisos sobre los datos locales.");
    }

    void Fail(const wchar_t* message) const {
        if (!handle_) return;
        MessageBoxW(handle_, message, L"Pléiade", MB_OK | MB_ICONERROR);
        PostMessageW(handle_, WM_CLOSE, 0, 0);
    }

    HWND handle_ = nullptr;
    HMODULE loader_ = nullptr;
    ICoreWebView2Controller* controller_ = nullptr;
    std::filesystem::path viewerFolder_;
};

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand) {
    if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))) {
        MessageBoxW(nullptr, L"No se pudo inicializar WebView2.", L"Pléiade", MB_OK | MB_ICONERROR);
        return 1;
    }

    int exitCode = 0;
    Viewer viewer;
    if (viewer.Start(instance)) {
        viewer.Run(showCommand);
    } else {
        MessageBoxW(nullptr, L"No se pudo crear la ventana de Pléiade.", L"Pléiade", MB_OK | MB_ICONERROR);
        exitCode = 1;
    }
    CoUninitialize();
    return exitCode;
}
