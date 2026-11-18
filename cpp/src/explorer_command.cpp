// Extensión COM para el menú contextual moderno de Windows 11.
// Es una DLL pequeña: solo recibe las rutas seleccionadas y lanza ConversorWebPC.exe.
#include <windows.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <shlwapi.h>
#include <shellapi.h>
#include <string>
#include <vector>
#include <new>

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "advapi32.lib")

static const CLSID CLSID_ConversorWebP =
    {0x6f2c7a2e, 0x4d3b, 0x4a6e, {0x9e, 0x2a, 0x7b, 0x1c, 0x5d, 0x8f, 0x3a, 0x91}};
static const wchar_t* CLSID_TEXTO = L"{6F2C7A2E-4D3B-4A6E-9E2A-7B1C5D8F3A91}";
static HINSTANCE g_inst = nullptr;

static std::wstring ruta_dll() {
    wchar_t buf[MAX_PATH] = L"";
    GetModuleFileNameW(g_inst, buf, MAX_PATH);
    return buf;
}

static std::wstring ruta_exe() {
    std::wstring dll = ruta_dll();
    if (dll.empty()) return L"";
    std::wstring::size_type pos = dll.find_last_of(L"\\/");
    if (pos == std::wstring::npos) return L"";
    return dll.substr(0, pos + 1) + L"ConversorWebPC.exe";
}

class ExploradorCommand final : public IExplorerCommand {
    LONG ref_ = 1;

public:
    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        if (riid == IID_IUnknown || riid == IID_IExplorerCommand) {
            *ppv = static_cast<IExplorerCommand*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    IFACEMETHODIMP_(ULONG) AddRef() override { return InterlockedIncrement(&ref_); }
    IFACEMETHODIMP_(ULONG) Release() override {
        ULONG n = InterlockedDecrement(&ref_);
        if (n == 0) delete this;
        return n;
    }

    IFACEMETHODIMP GetTitle(IShellItemArray*, LPWSTR* titulo) override {
        return SHStrDupW(L"Convertir con Conversor WebP", titulo);
    }
    IFACEMETHODIMP GetIcon(IShellItemArray*, LPWSTR* icono) override {
        if (icono) *icono = nullptr;
        return S_FALSE;
    }
    IFACEMETHODIMP GetToolTip(IShellItemArray*, LPWSTR* texto) override {
        return SHStrDupW(L"Convierte imagenes a WebP/AVIF y videos a WebM", texto);
    }
    IFACEMETHODIMP GetCanonicalName(GUID* guid) override {
        if (!guid) return E_POINTER;
        *guid = CLSID_ConversorWebP;
        return S_OK;
    }
    IFACEMETHODIMP GetState(IShellItemArray*, BOOL, EXPCMDSTATE* estado) override {
        if (!estado) return E_POINTER;
        *estado = ECS_ENABLED;
        return S_OK;
    }
    IFACEMETHODIMP Invoke(IShellItemArray* elementos, IBindCtx*) override {
        if (!elementos) return S_OK;
        DWORD cantidad = 0;
        if (FAILED(elementos->GetCount(&cantidad)) || cantidad == 0) return S_OK;
        std::wstring parametros;
        for (DWORD i = 0; i < cantidad; i++) {
            IShellItem* item = nullptr;
            if (FAILED(elementos->GetItemAt(i, &item)) || !item) continue;
            PWSTR ruta = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &ruta)) && ruta) {
                if (!parametros.empty()) parametros += L' ';
                parametros += L'"';
                parametros += ruta;
                parametros += L'"';
                CoTaskMemFree(ruta);
            }
            item->Release();
        }
        std::wstring exe = ruta_exe();
        if (exe.empty()) return E_FAIL;
        HINSTANCE r = ShellExecuteW(nullptr, L"open", exe.c_str(),
                                    parametros.empty() ? nullptr : parametros.c_str(),
                                    nullptr, SW_SHOWNORMAL);
        return reinterpret_cast<INT_PTR>(r) > 32 ? S_OK : E_FAIL;
    }
    IFACEMETHODIMP GetFlags(EXPCMDFLAGS* flags) override {
        if (!flags) return E_POINTER;
        *flags = ECF_DEFAULT;
        return S_OK;
    }
    IFACEMETHODIMP EnumSubCommands(IEnumExplorerCommand** enum_cmd) override {
        if (enum_cmd) *enum_cmd = nullptr;
        return E_NOTIMPL;
    }
};

class ClassFactory final : public IClassFactory {
    LONG ref_ = 1;

public:
    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        if (riid == IID_IUnknown || riid == IID_IClassFactory) {
            *ppv = static_cast<IClassFactory*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    IFACEMETHODIMP_(ULONG) AddRef() override { return InterlockedIncrement(&ref_); }
    IFACEMETHODIMP_(ULONG) Release() override {
        ULONG n = InterlockedDecrement(&ref_);
        if (n == 0) delete this;
        return n;
    }
    IFACEMETHODIMP CreateInstance(IUnknown* outer, REFIID riid, void** ppv) override {
        if (outer) return CLASS_E_NOAGGREGATION;
        ExploradorCommand* cmd = new (std::nothrow) ExploradorCommand();
        if (!cmd) return E_OUTOFMEMORY;
        HRESULT hr = cmd->QueryInterface(riid, ppv);
        cmd->Release();
        return hr;
    }
    IFACEMETHODIMP LockServer(BOOL) override { return S_OK; }
};

static bool escribir_valor(HKEY raiz, const std::wstring& clave, const wchar_t* nombre,
                           const std::wstring& valor) {
    HKEY h = nullptr;
    if (RegCreateKeyExW(raiz, clave.c_str(), 0, nullptr, 0, KEY_WRITE, nullptr, &h,
                        nullptr) != ERROR_SUCCESS)
        return false;
    LONG rc = RegSetValueExW(h, nombre, 0, REG_SZ, (const BYTE*)valor.c_str(),
                             (DWORD)((valor.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(h);
    return rc == ERROR_SUCCESS;
}

static void registrar_handlers(bool instalar) {
    const wchar_t* tipos[] = {
        L"*",
        L"Directory",
        L"SystemFileAssociations\\.zip",
        L"SystemFileAssociations\\.rar",
        L"SystemFileAssociations\\.7z",
    };
    for (const wchar_t* tipo : tipos) {
        std::wstring clave = std::wstring(L"Software\\Classes\\") + tipo +
                             L"\\shellex\\ContextMenuHandlers\\ConversorWebP";
        if (instalar)
            escribir_valor(HKEY_CURRENT_USER, clave, nullptr, CLSID_TEXTO);
        else
            RegDeleteTreeW(HKEY_CURRENT_USER, clave.c_str());
    }
}

extern "C" __declspec(dllexport) HRESULT __stdcall DllRegisterServer() {
    std::wstring dll = ruta_dll();
    if (dll.empty()) return E_FAIL;
    if (!escribir_valor(HKEY_CURRENT_USER,
                        std::wstring(L"Software\\Classes\\CLSID\\") + CLSID_TEXTO +
                            L"\\InprocServer32",
                        nullptr, dll))
        return E_FAIL;
    if (!escribir_valor(HKEY_CURRENT_USER,
                        std::wstring(L"Software\\Classes\\CLSID\\") + CLSID_TEXTO +
                            L"\\InprocServer32",
                        L"ThreadingModel", L"Apartment"))
        return E_FAIL;
    registrar_handlers(true);
    return S_OK;
}

extern "C" __declspec(dllexport) HRESULT __stdcall DllUnregisterServer() {
    registrar_handlers(false);
    RegDeleteTreeW(HKEY_CURRENT_USER,
                   (std::wstring(L"Software\\Classes\\CLSID\\") + CLSID_TEXTO).c_str());
    return S_OK;
}

STDAPI DllGetClassObject(REFCLSID clsid, REFIID riid, LPVOID* ppv) {
    if (!ppv) return E_POINTER;
    if (clsid != CLSID_ConversorWebP) return CLASS_E_CLASSNOTAVAILABLE;
    ClassFactory* fabrica = new (std::nothrow) ClassFactory();
    if (!fabrica) return E_OUTOFMEMORY;
    HRESULT hr = fabrica->QueryInterface(riid, ppv);
    fabrica->Release();
    return hr;
}

STDAPI DllCanUnloadNow(void) {
    return S_FALSE;
}

BOOL APIENTRY DllMain(HINSTANCE inst, DWORD motivo, LPVOID) {
    if (motivo == DLL_PROCESS_ATTACH) {
        g_inst = inst;
        DisableThreadLibraryCalls(inst);
    }
    return TRUE;
}
