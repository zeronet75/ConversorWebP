// Conversor WebP - version C++ de prueba (portable, sin git)
// Win32 + WIC (decodificacion) + libwebp (codificacion)

#define _WIN32_WINNT 0x0A00
#define WINVER 0x0A00
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <bcrypt.h>
#include <commctrl.h>
#include <commdlg.h>
#define _RICHEDIT_VER 0x0500
#include <richedit.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <winhttp.h>
#include <shobjidl.h>
#include <wincodec.h>
#include <uxtheme.h>
#include <vssym32.h>
#include <dwmapi.h>
#include <wintrust.h>
#include <softpub.h>
#include <wrl/client.h>
#include <webp/encode.h>
#include <webp/decode.h>
#include <webp/mux.h>
#include <avif/avif.h>
#include <lcms2.h>
#include <zlib.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iterator>
#include <map>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "uuid.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "wintrust.lib")
#pragma comment(lib, "propsys.lib")
#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "msimg32.lib")
#pragma comment(lib, "avif.lib")
#pragma comment(lib, "aom.lib")
#pragma comment(lib, "lcms2.lib")
#pragma comment(lib, "zlibstatic.lib")
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

using Microsoft::WRL::ComPtr;
namespace fs = std::filesystem;

static const wchar_t* APP_NAME = L"Conversor WebP";
static const wchar_t* APP_VERSION = L"2.9.0";

// ---------------------------------------------------------------------------
//  Utilidades generales
// ---------------------------------------------------------------------------
static std::wstring lower(std::wstring s) {
    std::transform(s.begin(), s.end(), s.begin(), towlower);
    return s;
}

static std::wstring recortar(std::wstring s) {
    auto espacio = [](wchar_t c) { return c == L' ' || c == L'\t' || c == L'\r' || c == L'\n'; };
    while (!s.empty() && espacio(s.front())) s.erase(s.begin());
    while (!s.empty() && espacio(s.back())) s.pop_back();
    return s;
}

static std::string a_utf8(const std::wstring& w) {
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (n <= 1) return {};
    std::string s((size_t)n, '\0');
    int escritos = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, s.data(), n, nullptr, nullptr);
    if (escritos <= 1) return {};
    s.resize((size_t)escritos - 1);
    return s;
}

static std::wstring a_wstring(const char* texto) {
    if (!texto || !*texto) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, texto, -1, nullptr, 0);
    if (n <= 1) return L"";
    std::wstring s((size_t)n, L'\0');
    int escritos = MultiByteToWideChar(CP_UTF8, 0, texto, -1, s.data(), n);
    if (escritos <= 1) return L"";
    s.resize((size_t)escritos - 1);
    return s;
}

static std::wstring sha1_hex(const fs::path& ruta) {
    BCRYPT_ALG_HANDLE alg = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    DWORD obj_len = 0, hash_len = 0, res = 0;
    std::vector<BYTE> objeto, digest;
    std::wstring salida;

    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA1_ALGORITHM, nullptr, 0) < 0) return salida;
    BCryptGetProperty(alg, BCRYPT_OBJECT_LENGTH, (PUCHAR)&obj_len, sizeof(obj_len), &res, 0);
    BCryptGetProperty(alg, BCRYPT_HASH_LENGTH, (PUCHAR)&hash_len, sizeof(hash_len), &res, 0);
    objeto.resize(obj_len);
    digest.resize(hash_len);
    if (BCryptCreateHash(alg, &hash, objeto.data(), obj_len, nullptr, 0, 0) >= 0) {
        std::ifstream f(ruta, std::ios::binary);
        std::vector<char> buf(1 << 20);
        while (f) {
            f.read(buf.data(), (std::streamsize)buf.size());
            std::streamsize n = f.gcount();
            if (n > 0) BCryptHashData(hash, (PUCHAR)buf.data(), (ULONG)n, 0);
        }
        if (BCryptFinishHash(hash, digest.data(), hash_len, 0) >= 0) {
            wchar_t hex[3];
            for (BYTE b : digest) {
                swprintf_s(hex, L"%02x", b);
                salida += hex;
            }
        }
        BCryptDestroyHash(hash);
    }
    BCryptCloseAlgorithmProvider(alg, 0);
    return salida;
}

static std::wstring ext_lower(const fs::path& p) {
    return lower(p.extension().wstring());
}

static bool es_imagen(const fs::path& p) {
    static const std::set<std::wstring> exts = {
        L".jpg", L".jpeg", L".jfif", L".png", L".bmp", L".dib", L".gif",
        L".tif", L".tiff", L".ico", L".webp", L".avif", L".heic", L".heif",
        L".tga", L".pcx", L".dds", L".ppm", L".pgm", L".pbm"};
    return exts.count(ext_lower(p)) > 0;
}

static std::wstring fmt_kb(double kb) {
    wchar_t buf[64];
    swprintf_s(buf, L"%.1f KB", kb);
    return buf;
}

static std::string csv_campo(const std::string& valor) {
    if (valor.find_first_of(";\"\r\n") == std::string::npos) return valor;
    std::string salida;
    salida.reserve(valor.size() + 2);
    salida.push_back('"');
    for (char c : valor) {
        if (c == '"') salida.push_back('"');
        salida.push_back(c);
    }
    salida.push_back('"');
    return salida;
}

static std::wstring nombre_seguro(std::wstring nombre, const std::wstring& defecto) {
    size_t pos = nombre.find_last_of(L"\\/");
    if (pos != std::wstring::npos) nombre = nombre.substr(pos + 1);
    for (auto& c : nombre) {
        if (c == L'<' || c == L'>' || c == L':' || c == L'"' || c == L'|' ||
            c == L'?' || c == L'*' || c < 32) {
            c = L'_';
        }
    }
    while (!nombre.empty() && (nombre.back() == L' ' || nombre.back() == L'.')) nombre.pop_back();
    if (nombre.empty()) nombre = defecto;
    static const wchar_t* reservados[] = {
        L"con", L"prn", L"aux", L"nul",
        L"com1", L"com2", L"com3", L"com4", L"com5", L"com6", L"com7", L"com8", L"com9",
        L"lpt1", L"lpt2", L"lpt3", L"lpt4", L"lpt5", L"lpt6", L"lpt7", L"lpt8", L"lpt9"};
    size_t punto = nombre.find(L'.');
    std::wstring raiz = lower(nombre.substr(0, punto));
    for (const wchar_t* r : reservados) {
        if (raiz == r) {
            nombre = L"_" + nombre;
            break;
        }
    }
    if (nombre.size() > 200) {
        std::wstring ext = fs::path(nombre).extension().wstring();
        nombre = nombre.substr(0, 200 - ext.size()) + ext;
    }
    return nombre;
}

static std::wstring fecha_compacta() {
    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t buf[32];
    swprintf_s(buf, L"%04d%02d%02d-%02d%02d%02d", st.wYear, st.wMonth, st.wDay,
               st.wHour, st.wMinute, st.wSecond);
    return buf;
}

static std::wstring fecha_archivo() {
    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t buf[32];
    swprintf_s(buf, L"%04d%02d%02d_%02d%02d%02d", st.wYear, st.wMonth, st.wDay,
               st.wHour, st.wMinute, st.wSecond);
    return buf;
}

static std::wstring fecha_zip() {
    return fecha_archivo().substr(0, 13);
}

// ---------------------------------------------------------------------------
//  ZIP (metodo almacenado; WebP ya esta comprimido)
// ---------------------------------------------------------------------------
static uint32_t crc32_tabla[256];
static void crc32_init() {
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int k = 0; k < 8; k++) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
        crc32_tabla[i] = c;
    }
}

static void put16(std::ofstream& o, uint16_t v) { o.write((char*)&v, 2); }
static void put32(std::ofstream& o, uint32_t v) { o.write((char*)&v, 4); }

struct ZipEntrada {
    std::string nombre;
    uint64_t tam = 0;
    uint32_t crc = 0;
    uint32_t offset = 0;
};

static bool escribir_zip(const fs::path& zip, const std::vector<fs::path>& archivos,
                         const fs::path& base) {
    if (archivos.empty() || archivos.size() > 65000) return false;

    std::vector<ZipEntrada> entradas;
    entradas.reserve(archivos.size());
    for (const auto& a : archivos) {
        std::error_code ec_rel;
        std::wstring rel = fs::relative(a, base, ec_rel).wstring();
        if (ec_rel || rel.empty()) rel = a.filename().wstring();
        std::replace(rel.begin(), rel.end(), L'\\', L'/');

        std::ifstream f(a, std::ios::binary);
        if (!f) return false;
        uint32_t crc = 0xFFFFFFFFu;
        uint64_t total = 0;
        std::array<char, 1 << 16> buf;
        while (f) {
            f.read(buf.data(), (std::streamsize)buf.size());
            std::streamsize n = f.gcount();
            for (std::streamsize i = 0; i < n; i++)
                crc = crc32_tabla[(crc ^ (uint8_t)buf[(size_t)i]) & 0xFF] ^ (crc >> 8);
            total += (uint64_t)n;
        }
        if (!f.eof() || total > 0xFFFFFFFFull) return false;
        entradas.push_back({a_utf8(rel), total, crc ^ 0xFFFFFFFFu, 0});
    }

    std::ofstream o(zip, std::ios::binary | std::ios::trunc);
    if (!o) return false;
    auto fallo = [&]() {
        o.close();
        std::error_code ec;
        fs::remove(zip, ec);
        return false;
    };

    SYSTEMTIME st;
    GetLocalTime(&st);
    uint16_t dos_time = (uint16_t)((st.wHour << 11) | (st.wMinute << 5) | (st.wSecond / 2));
    uint16_t dos_date = (uint16_t)(((st.wYear - 1980) << 9) | (st.wMonth << 5) | st.wDay);

    for (size_t i = 0; i < entradas.size(); i++) {
        auto& e = entradas[i];
        auto pos = o.tellp();
        if (pos < 0 || (uint64_t)pos > 0xFFFFFFFFull) return fallo();
        e.offset = (uint32_t)pos;

        put32(o, 0x04034b50);
        put16(o, 20);
        put16(o, 0x0800);
        put16(o, 0);
        put16(o, dos_time);
        put16(o, dos_date);
        put32(o, e.crc);
        put32(o, (uint32_t)e.tam);
        put32(o, (uint32_t)e.tam);
        put16(o, (uint16_t)e.nombre.size());
        put16(o, 0);
        o.write(e.nombre.data(), (std::streamsize)e.nombre.size());

        std::ifstream f(archivos[i], std::ios::binary);
        if (!f) return fallo();
        std::array<char, 1 << 16> buf;
        uint64_t escritos = 0;
        while (f) {
            f.read(buf.data(), (std::streamsize)buf.size());
            std::streamsize n = f.gcount();
            if (n > 0) {
                o.write(buf.data(), n);
                escritos += (uint64_t)n;
            }
        }
        if (!f.eof() || escritos != e.tam) return fallo();
    }

    auto pos_central = o.tellp();
    if (pos_central < 0 || (uint64_t)pos_central > 0xFFFFFFFFull) return fallo();
    uint32_t inicio_central = (uint32_t)pos_central;
    for (auto& e : entradas) {
        put32(o, 0x02014b50);
        put16(o, 20);
        put16(o, 20);
        put16(o, 0x0800);
        put16(o, 0);
        put16(o, dos_time);
        put16(o, dos_date);
        put32(o, e.crc);
        put32(o, (uint32_t)e.tam);
        put32(o, (uint32_t)e.tam);
        put16(o, (uint16_t)e.nombre.size());
        put16(o, 0);
        put16(o, 0);
        put16(o, 0);
        put16(o, 0);
        put32(o, 0);
        put32(o, e.offset);
        o.write(e.nombre.data(), (std::streamsize)e.nombre.size());
    }
    auto pos_fin = o.tellp();
    if (pos_fin < 0 || (uint64_t)pos_fin > 0xFFFFFFFFull) return fallo();
    uint32_t fin_central = (uint32_t)pos_fin;

    put32(o, 0x06054b50);
    put16(o, 0);
    put16(o, 0);
    put16(o, (uint16_t)entradas.size());
    put16(o, (uint16_t)entradas.size());
    put32(o, fin_central - inicio_central);
    put32(o, inicio_central);
    put16(o, 0);
    o.flush();
    if (!o) return fallo();
    return true;
}

// ---------------------------------------------------------------------------
//  Imagenes: WIC (decodificar/redimensionar) + libwebp (codificar)
// ---------------------------------------------------------------------------
struct Bitmap {
    UINT w = 0, h = 0;
    bool alfa = false;
    int frames = 1;
    std::vector<uint8_t> rgba;
    std::vector<uint8_t> icc;
};

static bool perfil_es_rgb(const std::vector<uint8_t>& icc) {
    if (icc.empty()) return false;
    cmsHPROFILE perfil = cmsOpenProfileFromMem(icc.data(), (cmsUInt32Number)icc.size());
    if (!perfil) return false;
    cmsColorSpaceSignature espacio = cmsGetColorSpace(perfil);
    cmsCloseProfile(perfil);
    return espacio == cmsSigRgbData;
}

static std::vector<uint8_t> leer_icc_png(const std::wstring& ruta) {
    std::ifstream f(ruta, std::ios::binary);
    if (!f) return {};
    std::vector<uint8_t> datos((std::istreambuf_iterator<char>(f)),
                               std::istreambuf_iterator<char>());
    static const uint8_t firma[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    if (datos.size() < 8 || memcmp(datos.data(), firma, 8) != 0) return {};

    size_t pos = 8;
    while (pos + 8 <= datos.size()) {
        uint32_t longitud = ((uint32_t)datos[pos] << 24) | ((uint32_t)datos[pos + 1] << 16) |
                            ((uint32_t)datos[pos + 2] << 8) | datos[pos + 3];
        const uint8_t* tipo = datos.data() + pos + 4;
        if (pos + 12 + longitud > datos.size()) break;
        if (memcmp(tipo, "iCCP", 4) == 0 && longitud > 2) {
            const uint8_t* trozo = datos.data() + pos + 8;
            size_t nombre = 0;
            while (nombre < longitud && trozo[nombre] != 0) nombre++;
            if (nombre + 2 > longitud) return {};
            if (trozo[nombre + 1] != 0) return {};
            const uint8_t* comprimido = trozo + nombre + 2;
            size_t tam_comp = longitud - nombre - 2;
            uLongf destino = (uLongf)std::max<size_t>(tam_comp * 4, 65536);
            for (int intento = 0; intento < 6; intento++) {
                std::vector<uint8_t> perfil(destino);
                uLongf salida = destino;
                int rc = uncompress(perfil.data(), &salida, comprimido,
                                    tam_comp > 0xFFFFFFFFull ? 0 : (uLong)tam_comp);
                if (rc == Z_OK) {
                    perfil.resize(salida);
                    return perfil;
                }
                if (rc != Z_BUF_ERROR) break;
                destino *= 2;
            }
            return {};
        }
        if (memcmp(tipo, "IEND", 4) == 0) break;
        pos += 12 + longitud;
    }
    return {};
}

static std::vector<uint8_t> leer_icc_tiff(const std::wstring& ruta) {
    std::ifstream f(ruta, std::ios::binary);
    if (!f) return {};
    std::vector<uint8_t> datos((std::istreambuf_iterator<char>(f)),
                               std::istreambuf_iterator<char>());
    if (datos.size() < 8) return {};
    bool little;
    if (datos[0] == 'I' && datos[1] == 'I') little = true;
    else if (datos[0] == 'M' && datos[1] == 'M') little = false;
    else return {};
    auto leer16 = [&](size_t p) -> uint16_t {
        if (p + 2 > datos.size()) return 0;
        if (little) return (uint16_t)(datos[p] | (datos[p + 1] << 8));
        return (uint16_t)((datos[p] << 8) | datos[p + 1]);
    };
    auto leer32 = [&](size_t p) -> uint32_t {
        if (p + 4 > datos.size()) return 0;
        if (little)
            return (uint32_t)datos[p] | ((uint32_t)datos[p + 1] << 8) |
                   ((uint32_t)datos[p + 2] << 16) | ((uint32_t)datos[p + 3] << 24);
        return ((uint32_t)datos[p] << 24) | ((uint32_t)datos[p + 1] << 16) |
               ((uint32_t)datos[p + 2] << 8) | (uint32_t)datos[p + 3];
    };
    if (leer16(2) != 42) return {};
    size_t ifd = leer32(4);
    if (ifd + 2 > datos.size()) return {};
    uint16_t entradas = leer16(ifd);
    for (uint16_t i = 0; i < entradas; i++) {
        size_t p = ifd + 2 + (size_t)i * 12;
        if (p + 12 > datos.size()) break;
        uint16_t tag = leer16(p);
        if (tag != 34675) continue;
        uint16_t tipo = leer16(p + 2);
        uint32_t cuenta = leer32(p + 4);
        if (tipo != 7 || cuenta == 0 || cuenta > 16 * 1024 * 1024) return {};
        size_t inicio = cuenta <= 4 ? p + 8 : leer32(p + 8);
        if (inicio + cuenta > datos.size()) return {};
        return std::vector<uint8_t>(datos.begin() + inicio, datos.begin() + inicio + cuenta);
    }
    return {};
}

static std::vector<uint8_t> leer_icc_jpeg(const std::wstring& ruta) {
    std::ifstream f(ruta, std::ios::binary);
    if (!f) return {};
    std::vector<uint8_t> datos((std::istreambuf_iterator<char>(f)),
                               std::istreambuf_iterator<char>());
    std::map<int, std::vector<uint8_t>> trozos;
    int total = 0;
    size_t i = 2;
    while (i + 4 <= datos.size()) {
        if (datos[i] != 0xFF) { i++; continue; }
        uint8_t marcador = datos[i + 1];
        if (marcador == 0xD8 || marcador == 0xD9 || (marcador >= 0xD0 && marcador <= 0xD7)) {
            i += 2;
            continue;
        }
        if (i + 4 > datos.size()) break;
        size_t longitud = ((size_t)datos[i + 2] << 8) | datos[i + 3];
        if (longitud < 2 || i + 2 + longitud > datos.size()) break;
        if (marcador == 0xE2 && longitud >= 16 &&
            memcmp(datos.data() + i + 4, "ICC_PROFILE\0", 12) == 0) {
            int secuencia = datos[i + 16];
            int cuantos = datos[i + 17];
            if (cuantos > 0) total = cuantos;
            size_t inicio = i + 18;
            size_t fin = i + 2 + longitud;
            if (inicio <= fin)
                trozos[secuencia].assign(datos.begin() + inicio, datos.begin() + fin);
        }
        if (marcador == 0xDA) break;  // inicio de los datos de imagen
        i += 2 + longitud;
    }
    std::vector<uint8_t> perfil;
    for (int n = 1; n <= total; n++) {
        auto it = trozos.find(n);
        if (it == trozos.end()) return {};
        perfil.insert(perfil.end(), it->second.begin(), it->second.end());
    }
    return perfil;
}

static bool decodificar_wic(IWICImagingFactory* factory, const std::wstring& ruta,
                            Bitmap& bmp, std::wstring& error) {
    ComPtr<IWICBitmapDecoder> decoder;
    HRESULT hr = factory->CreateDecoderFromFilename(ruta.c_str(), nullptr, GENERIC_READ,
                                                    WICDecodeMetadataCacheOnLoad, &decoder);
    if (FAILED(hr)) { error = L"no se pudo abrir el archivo"; return false; }

    ComPtr<IWICBitmapFrameDecode> frame;
    if (FAILED(decoder->GetFrame(0, &frame))) { error = L"sin fotogramas"; return false; }

    UINT n_fotogramas = 1;
    if (SUCCEEDED(decoder->GetFrameCount(&n_fotogramas)) && n_fotogramas > 0)
        bmp.frames = (int)n_fotogramas;

    UINT n_contextos = 0;
    frame->GetColorContexts(0, nullptr, &n_contextos);
    if (n_contextos > 0) {
        std::vector<IWICColorContext*> contextos(n_contextos, nullptr);
        if (SUCCEEDED(frame->GetColorContexts(n_contextos, contextos.data(), &n_contextos))) {
            for (UINT i = 0; i < n_contextos; i++) {
                if (!contextos[i]) continue;
                WICColorContextType tipo = WICColorContextUninitialized;
                contextos[i]->GetType(&tipo);
                if (tipo == WICColorContextProfile) {
                    UINT cb = 0;
                    contextos[i]->GetProfileBytes(0, nullptr, &cb);
                    if (cb > 0) {
                        bmp.icc.resize(cb);
                        if (FAILED(contextos[i]->GetProfileBytes(cb, bmp.icc.data(), &cb))) {
                            bmp.icc.clear();
                        } else {
                            bmp.icc.resize(cb);
                        }
                    }
                }
            }
        }
        for (auto* c : contextos) if (c) c->Release();
    }

    UINT orientacion = 1;
    ComPtr<IWICMetadataQueryReader> meta;
    if (SUCCEEDED(frame->GetMetadataQueryReader(&meta))) {
        PROPVARIANT v;
        PropVariantInit(&v);
        if (SUCCEEDED(meta->GetMetadataByName(L"/app1/ifd/{ushort=274}", &v)) && v.vt == VT_UI2)
            orientacion = v.uiVal;
        else if (SUCCEEDED(meta->GetMetadataByName(L"/{ushort=274}", &v)) && v.vt == VT_UI2)
            orientacion = v.uiVal;
        PropVariantClear(&v);
    }

    WICPixelFormatGUID formato_origen{};
    frame->GetPixelFormat(&formato_origen);
    if (IsEqualGUID(formato_origen, GUID_WICPixelFormat32bppCMYK) && !bmp.icc.empty()) {
        UINT cw = 0, ch = 0;
        if (SUCCEEDED(frame->GetSize(&cw, &ch)) && cw > 0 && ch > 0 &&
            (uint64_t)cw * ch <= 120000000ull) {
            std::vector<uint8_t> cmyk((size_t)cw * ch * 4);
            if (SUCCEEDED(frame->CopyPixels(nullptr, cw * 4, (UINT)cmyk.size(), cmyk.data()))) {
                cmsHPROFILE perfil = cmsOpenProfileFromMem(bmp.icc.data(), (cmsUInt32Number)bmp.icc.size());
                cmsHPROFILE srgb = cmsCreate_sRGBProfile();
                if (perfil && srgb) {
                    cmsHTRANSFORM transformacion = cmsCreateTransform(
                        perfil, TYPE_CMYK_8, srgb, TYPE_RGBA_8, INTENT_PERCEPTUAL, 0);
                    if (transformacion) {
                        bmp.w = cw;
                        bmp.h = ch;
                        bmp.alfa = false;
                        bmp.rgba.resize((size_t)cw * ch * 4);
                        cmsDoTransform(transformacion, cmyk.data(), bmp.rgba.data(), cw * ch);
                        cmsDeleteTransform(transformacion);
                        cmsCloseProfile(perfil);
                        cmsCloseProfile(srgb);
                        bmp.icc.clear();
                        return true;
                    }
                }
                if (perfil) cmsCloseProfile(perfil);
                if (srgb) cmsCloseProfile(srgb);
            }
        }
    }

    ComPtr<IWICFormatConverter> conv;
    if (FAILED(factory->CreateFormatConverter(&conv))) { error = L"WIC converter"; return false; }
    hr = conv->Initialize(frame.Get(), GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone,
                          nullptr, 0.0, WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) { error = L"conversion de pixel format"; return false; }

    ComPtr<IWICBitmapSource> fuente = conv;
    if (orientacion >= 2 && orientacion <= 8) {
        WICBitmapTransformOptions opt = WICBitmapTransformRotate0;
        switch (orientacion) {
            case 2: opt = WICBitmapTransformFlipHorizontal; break;
            case 3: opt = WICBitmapTransformRotate180; break;
            case 4: opt = WICBitmapTransformFlipVertical; break;
            case 5: opt = (WICBitmapTransformOptions)(WICBitmapTransformRotate270 | WICBitmapTransformFlipHorizontal); break;
            case 6: opt = WICBitmapTransformRotate90; break;
            case 7: opt = (WICBitmapTransformOptions)(WICBitmapTransformRotate90 | WICBitmapTransformFlipHorizontal); break;
            case 8: opt = WICBitmapTransformRotate270; break;
        }
        ComPtr<IWICBitmapFlipRotator> rot;
        if (SUCCEEDED(factory->CreateBitmapFlipRotator(&rot)) &&
            SUCCEEDED(rot->Initialize(fuente.Get(), opt))) {
            fuente = rot;
        }
    }

    UINT w = 0, h = 0;
    if (FAILED(fuente->GetSize(&w, &h)) || w == 0 || h == 0) { error = L"tamano no valido"; return false; }
    if ((uint64_t)w * h > 120000000ull) { error = L"imagen demasiado grande"; return false; }

    bmp.w = w;
    bmp.h = h;
    bmp.rgba.resize((size_t)w * h * 4);
    hr = fuente->CopyPixels(nullptr, w * 4, (UINT)bmp.rgba.size(), bmp.rgba.data());
    if (FAILED(hr)) { error = L"lectura de pixeles"; return false; }

    bool alfa = false;
    for (size_t i = 0; i < (size_t)w * h; i++) {
        uint8_t b = bmp.rgba[i * 4 + 0], g = bmp.rgba[i * 4 + 1], r = bmp.rgba[i * 4 + 2],
                a = bmp.rgba[i * 4 + 3];
        bmp.rgba[i * 4 + 0] = r;
        bmp.rgba[i * 4 + 1] = g;
        bmp.rgba[i * 4 + 2] = b;
        bmp.rgba[i * 4 + 3] = a;
        if (a < 255) alfa = true;
    }
    bmp.alfa = alfa;
    if (bmp.icc.empty()) {
        std::wstring ext = lower(fs::path(ruta).extension().wstring());
        if (ext == L".jpg" || ext == L".jpeg" || ext == L".jfif")
            bmp.icc = leer_icc_jpeg(ruta);
        else if (ext == L".png")
            bmp.icc = leer_icc_png(ruta);
        else if (ext == L".tif" || ext == L".tiff")
            bmp.icc = leer_icc_tiff(ruta);
        if (!bmp.icc.empty() && !perfil_es_rgb(bmp.icc)) bmp.icc.clear();
    }
    return true;
}

static bool decodificar_avif_datos(const std::vector<uint8_t>& datos, Bitmap& bmp,
                                   std::wstring& error) {
    if (datos.empty()) { error = L"AVIF vacio"; return false; }

    avifDecoder* decoder = avifDecoderCreate();
    if (!decoder) { error = L"avifDecoderCreate"; return false; }
    decoder->maxThreads = 4;
    avifResult r = avifDecoderSetIOMemory(decoder, datos.data(), datos.size());
    if (r == AVIF_RESULT_OK) r = avifDecoderParse(decoder);
    if (r == AVIF_RESULT_OK) r = avifDecoderNextImage(decoder);
    if (r != AVIF_RESULT_OK) {
        error = L"AVIF no valido: ";
        for (const char* p = avifResultToString(r); p && *p; p++)
            error += (wchar_t)(unsigned char)*p;
        avifDecoderDestroy(decoder);
        return false;
    }

    const avifImage* imagen = decoder->image;
    avifRGBImage rgb;
    avifRGBImageSetDefaults(&rgb, imagen);
    rgb.depth = 8;
    rgb.format = AVIF_RGB_FORMAT_RGBA;
    if ((uint64_t)rgb.width * rgb.height > 120000000ull) {
        error = L"imagen AVIF demasiado grande";
        avifDecoderDestroy(decoder);
        return false;
    }
    if (avifRGBImageAllocatePixels(&rgb) != AVIF_RESULT_OK || !rgb.pixels) {
        error = L"sin memoria para el AVIF";
        avifDecoderDestroy(decoder);
        return false;
    }
    r = avifImageYUVToRGB(imagen, &rgb);
    if (r != AVIF_RESULT_OK) {
        error = L"AVIF conversion RGB";
        avifRGBImageFreePixels(&rgb);
        avifDecoderDestroy(decoder);
        return false;
    }

    bmp.w = rgb.width;
    bmp.h = rgb.height;
    bmp.alfa = decoder->alphaPresent != 0;
    if (imagen->icc.size && imagen->icc.data)
        bmp.icc.assign(imagen->icc.data, imagen->icc.data + imagen->icc.size);
    bmp.rgba.resize((size_t)bmp.w * bmp.h * 4);
    for (size_t y = 0; y < bmp.h; y++) {
        const uint8_t* src = rgb.pixels + (size_t)y * rgb.rowBytes;
        std::copy(src, src + (size_t)bmp.w * 4, bmp.rgba.data() + (size_t)y * bmp.w * 4);
    }
    avifRGBImageFreePixels(&rgb);
    avifDecoderDestroy(decoder);
    return true;
}

static bool decodificar_avif(const std::wstring& ruta, Bitmap& bmp, std::wstring& error) {
    std::ifstream f(ruta, std::ios::binary);
    if (!f) { error = L"no se pudo abrir el AVIF"; return false; }
    std::vector<uint8_t> datos((std::istreambuf_iterator<char>(f)),
                               std::istreambuf_iterator<char>());
    return decodificar_avif_datos(datos, bmp, error);
}

static bool decodificar_wic_animado(IWICImagingFactory* factory, const std::wstring& ruta,
                                    std::vector<Bitmap>& frames,
                                    std::vector<int>& duraciones,
                                    std::wstring& error) {
    ComPtr<IWICBitmapDecoder> decoder;
    HRESULT hr = factory->CreateDecoderFromFilename(ruta.c_str(), nullptr, GENERIC_READ,
                                                    WICDecodeMetadataCacheOnLoad, &decoder);
    if (FAILED(hr)) { error = L"no se pudo abrir el archivo"; return false; }
    UINT total = 0;
    if (FAILED(decoder->GetFrameCount(&total)) || total < 2 || total > 1000)
        return false;
    for (UINT i = 0; i < total; i++) {
        ComPtr<IWICBitmapFrameDecode> frame;
        if (FAILED(decoder->GetFrame(i, &frame))) return false;
        UINT w = 0, h = 0;
        if (FAILED(frame->GetSize(&w, &h)) || w == 0 || h == 0) return false;
        if ((uint64_t)w * h > 120000000ull) {
            error = L"imagen animada demasiado grande";
            return false;
        }
        ComPtr<IWICFormatConverter> conv;
        if (FAILED(factory->CreateFormatConverter(&conv))) return false;
        if (FAILED(conv->Initialize(frame.Get(), GUID_WICPixelFormat32bppBGRA,
                                    WICBitmapDitherTypeNone, nullptr, 0.0,
                                    WICBitmapPaletteTypeCustom)))
            return false;
        Bitmap bmp;
        bmp.w = w;
        bmp.h = h;
        bmp.rgba.resize((size_t)w * h * 4);
        if (FAILED(conv->CopyPixels(nullptr, w * 4, (UINT)bmp.rgba.size(),
                                    bmp.rgba.data())))
            return false;
        bmp.alfa = false;
        for (size_t px = 0; px < (size_t)w * h; px++) {
            if (bmp.rgba[px * 4 + 3] < 255) { bmp.alfa = true; break; }
        }
        int duracion = 100;
        ComPtr<IWICMetadataQueryReader> meta;
        if (SUCCEEDED(frame->GetMetadataQueryReader(&meta))) {
            PROPVARIANT v;
            PropVariantInit(&v);
            if (SUCCEEDED(meta->GetMetadataByName(L"/grctlext/Delay", &v)) &&
                v.vt == VT_UI2 && v.uiVal > 0)
                duracion = (int)v.uiVal * 10;
            else if (SUCCEEDED(meta->GetMetadataByName(L"/Delay", &v)) &&
                     v.vt == VT_UI2 && v.uiVal > 0)
                duracion = (int)v.uiVal * 10;
            PropVariantClear(&v);
        }
        frames.push_back(std::move(bmp));
        duraciones.push_back(std::clamp(duracion, 10, 10000));
    }
    return frames.size() > 1;
}

static void aplanar_bitmap(Bitmap& bmp, COLORREF color) {
    if (!bmp.alfa) return;
    BYTE fr = GetRValue(color), fg = GetGValue(color), fb = GetBValue(color);
    for (size_t px = 0; px < (size_t)bmp.w * bmp.h; px++) {
        uint8_t* p = bmp.rgba.data() + px * 4;
        unsigned a = p[3];
        p[0] = (uint8_t)((p[0] * a + fr * (255 - a)) / 255);
        p[1] = (uint8_t)((p[1] * a + fg * (255 - a)) / 255);
        p[2] = (uint8_t)((p[2] * a + fb * (255 - a)) / 255);
        p[3] = 255;
    }
    bmp.alfa = false;
}

static std::vector<uint8_t> codificar_webp_animado(
    const std::vector<Bitmap>& frames, const std::vector<int>& duraciones,
    int calidad, int metodo, bool sin_perdida, std::wstring& error) {
    if (frames.empty()) return {};
    int w = (int)frames[0].w, h = (int)frames[0].h;
    WebPAnimEncoderOptions eopt;
    if (!WebPAnimEncoderOptionsInit(&eopt)) { error = L"WebPAnimEncoderOptionsInit"; return {}; }
    eopt.minimize_size = 1;
    WebPAnimEncoder* enc = WebPAnimEncoderNew(w, h, &eopt);
    if (!enc) { error = L"WebPAnimEncoderNew"; return {}; }
    int timestamp = 0;
    for (size_t i = 0; i < frames.size(); i++) {
        const Bitmap& f = frames[i];
        if ((int)f.w != w || (int)f.h != h) {
            error = L"fotogramas con tamanos distintos";
            WebPAnimEncoderDelete(enc);
            return {};
        }
        WebPConfig config;
        if (!WebPConfigInit(&config)) {
            error = L"WebPConfigInit";
            WebPAnimEncoderDelete(enc);
            return {};
        }
        config.quality = (float)std::clamp(calidad, 1, 100);
        config.method = std::clamp(metodo, 0, 6);
        config.lossless = sin_perdida ? 1 : 0;
        config.exact = sin_perdida ? 1 : (f.alfa ? 1 : 0);
        config.alpha_quality = 100;
        config.alpha_compression = 1;
        config.alpha_filtering = 1;
        WebPPicture pic;
        if (!WebPPictureInit(&pic)) {
            error = L"WebPPictureInit";
            WebPAnimEncoderDelete(enc);
            return {};
        }
        pic.width = w;
        pic.height = h;
        pic.use_argb = 1;
        std::vector<uint8_t> rgb;
        bool ok;
        if (f.alfa) {
            ok = WebPPictureImportRGBA(&pic, f.rgba.data(), w * 4) != 0;
        } else {
            rgb.resize((size_t)w * h * 3);
            for (size_t px = 0; px < (size_t)w * h; px++) {
                rgb[px * 3 + 0] = f.rgba[px * 4 + 0];
                rgb[px * 3 + 1] = f.rgba[px * 4 + 1];
                rgb[px * 3 + 2] = f.rgba[px * 4 + 2];
            }
            ok = WebPPictureImportRGB(&pic, rgb.data(), w * 3) != 0;
        }
        if (!ok || !WebPAnimEncoderAdd(enc, &pic, timestamp, &config)) {
            error = L"fallo al anadir fotograma WebP";
            WebPPictureFree(&pic);
            WebPAnimEncoderDelete(enc);
            return {};
        }
        WebPPictureFree(&pic);
        timestamp += duraciones[i];
    }
    if (!WebPAnimEncoderAdd(enc, nullptr, timestamp, nullptr)) {
        error = L"fallo al cerrar la animacion WebP";
        WebPAnimEncoderDelete(enc);
        return {};
    }
    WebPData data;
    WebPDataInit(&data);
    if (!WebPAnimEncoderAssemble(enc, &data) || !data.bytes || data.size == 0) {
        error = L"fallo al ensamblar la animacion WebP";
        WebPAnimEncoderDelete(enc);
        return {};
    }
    std::vector<uint8_t> salida(data.bytes, data.bytes + data.size);
    WebPDataClear(&data);
    WebPAnimEncoderDelete(enc);
    return salida;
}

static bool decodificar_tga(const std::wstring& ruta, Bitmap& bmp, std::wstring& error) {
    std::ifstream f(ruta, std::ios::binary);
    if (!f) { error = L"no se pudo abrir el TGA"; return false; }
    std::vector<uint8_t> d((std::istreambuf_iterator<char>(f)),
                           std::istreambuf_iterator<char>());
    if (d.size() < 18 || d[1] != 0) { error = L"TGA no soportado"; return false; }
    int tipo = d[2];
    int w = d[12] | (d[13] << 8), h = d[14] | (d[15] << 8);
    int bpp = d[16], canales = bpp / 8;
    if (w <= 0 || h <= 0 || (uint64_t)w * h > 120000000ull) {
        error = L"tamano TGA no valido"; return false;
    }
    if (!(tipo == 2 || tipo == 3 || tipo == 10 || tipo == 11) ||
        (canales != 1 && canales != 3 && canales != 4)) {
        error = L"TGA comprimido no soportado"; return false;
    }
    size_t pos = 18 + d[0];
    size_t total = (size_t)w * h;
    std::vector<uint8_t> pixeles(total * (size_t)canales);
    if (tipo == 2 || tipo == 3) {
        size_t bytes = total * (size_t)canales;
        if (pos + bytes > d.size()) { error = L"TGA truncado"; return false; }
        std::copy(d.begin() + pos, d.begin() + pos + bytes, pixeles.begin());
    } else {
        size_t salida = 0;
        while (salida < total) {
            if (pos >= d.size()) { error = L"TGA truncado"; return false; }
            int cabecera = d[pos++];
            int cuenta = (cabecera & 0x7F) + 1;
            if (cabecera & 0x80) {
                if (pos + canales > d.size()) { error = L"TGA truncado"; return false; }
                for (int i = 0; i < cuenta && salida < total; i++, salida++)
                    for (int c = 0; c < canales; c++)
                        pixeles[salida * canales + c] = d[pos + c];
                pos += canales;
            } else {
                size_t bytes = (size_t)cuenta * canales;
                if (pos + bytes > d.size()) { error = L"TGA truncado"; return false; }
                size_t restantes = total - salida;
                size_t copiar = std::min((size_t)cuenta, restantes);
                std::copy(d.begin() + pos, d.begin() + pos + copiar * canales,
                          pixeles.begin() + salida * canales);
                pos += bytes; salida += cuenta;
            }
        }
    }
    bmp.w = w; bmp.h = h; bmp.alfa = false;
    bmp.rgba.resize(total * 4);
    bool origen_superior = (d[17] & 0x20) != 0;
    for (int y = 0; y < h; y++) {
        int sy = origen_superior ? y : h - 1 - y;
        for (int x = 0; x < w; x++) {
            const uint8_t* p = pixeles.data() + ((size_t)sy * w + x) * canales;
            uint8_t* q = bmp.rgba.data() + ((size_t)y * w + x) * 4;
            if (canales == 1) { q[0] = q[1] = q[2] = p[0]; q[3] = 255; }
            else {
                q[0] = p[2]; q[1] = p[1]; q[2] = p[0];
                q[3] = canales == 4 ? p[3] : 255;
                if (canales == 4 && p[3] < 255) bmp.alfa = true;
            }
        }
    }
    return true;
}

static bool decodificar_ppm(const std::wstring& ruta, Bitmap& bmp, std::wstring& error) {
    std::ifstream f(ruta, std::ios::binary);
    if (!f) { error = L"no se pudo abrir el PPM"; return false; }
    std::vector<uint8_t> d((std::istreambuf_iterator<char>(f)),
                           std::istreambuf_iterator<char>());
    size_t pos = 0;
    auto token = [&]() -> std::string {
        while (pos < d.size()) {
            if (d[pos] == '#') {
                while (pos < d.size() && d[pos] != '\n') pos++;
            } else if (isspace((unsigned char)d[pos])) {
                pos++;
            } else break;
        }
        size_t ini = pos;
        while (pos < d.size() && !isspace((unsigned char)d[pos]) && d[pos] != '#') pos++;
        return std::string((const char*)d.data() + ini, pos - ini);
    };
    std::string magia = token();
    if (magia.size() != 2 || magia[0] != 'P' || magia[1] < '1' || magia[1] > '6') {
        error = L"PPM no valido"; return false;
    }
    int tipo = magia[1] - '0';
    auto token_int = [&]() {
        std::string t = token();
        return atoi(t.c_str());
    };
    int w = token_int();
    int h = token_int();
    if (w <= 0 || h <= 0 || (uint64_t)w * h > 120000000ull) {
        error = L"tamano PPM no valido"; return false;
    }
    int maximo = 255;
    if (tipo != 1 && tipo != 4) {
        maximo = token_int();
        if (maximo <= 0) maximo = 255;
    }
    while (pos < d.size() && isspace((unsigned char)d[pos])) pos++;
    bmp.w = w; bmp.h = h; bmp.alfa = false;
    bmp.rgba.resize((size_t)w * h * 4);
    size_t total = (size_t)w * h;
    if (tipo == 6 || tipo == 5) {
        int canales = tipo == 6 ? 3 : 1;
        if (pos + total * canales > d.size()) { error = L"PPM truncado"; return false; }
        for (size_t i = 0; i < total; i++) {
            const uint8_t* p = d.data() + pos + i * canales;
            uint8_t* q = bmp.rgba.data() + i * 4;
            if (canales == 3) {
                q[0] = (uint8_t)(p[0] * 255 / maximo);
                q[1] = (uint8_t)(p[1] * 255 / maximo);
                q[2] = (uint8_t)(p[2] * 255 / maximo);
            } else {
                q[0] = q[1] = q[2] = (uint8_t)(p[0] * 255 / maximo);
            }
            q[3] = 255;
        }
    } else if (tipo == 4) {
        size_t bytes = (total + 7) / 8;
        if (pos + bytes > d.size()) { error = L"PBM truncado"; return false; }
        for (size_t i = 0; i < total; i++) {
            bool negro = (d[pos + i / 8] & (0x80 >> (i % 8))) != 0;
            uint8_t* q = bmp.rgba.data() + i * 4;
            q[0] = q[1] = q[2] = negro ? 0 : 255;
            q[3] = 255;
        }
    } else {
        int canales = (tipo == 3) ? 3 : 1;
        for (size_t i = 0; i < total; i++) {
            int valores[3] = {0, 0, 0};
            for (int c = 0; c < canales; c++) {
                std::string t = token();
                if (t.empty()) { error = L"PPM truncado"; return false; }
                valores[c] = atoi(t.c_str());
            }
            uint8_t* q = bmp.rgba.data() + i * 4;
            if (canales == 3) {
                q[0] = (uint8_t)(valores[0] * 255 / maximo);
                q[1] = (uint8_t)(valores[1] * 255 / maximo);
                q[2] = (uint8_t)(valores[2] * 255 / maximo);
            } else {
                q[0] = q[1] = q[2] = (uint8_t)(valores[0] * 255 / maximo);
            }
            q[3] = 255;
        }
    }
    return true;
}

static bool decodificar_pcx(const std::wstring& ruta, Bitmap& bmp, std::wstring& error) {
    std::ifstream f(ruta, std::ios::binary);
    if (!f) { error = L"no se pudo abrir el PCX"; return false; }
    std::vector<uint8_t> d((std::istreambuf_iterator<char>(f)),
                           std::istreambuf_iterator<char>());
    if (d.size() < 128 || d[0] != 0x0A || d[2] != 1) {
        error = L"PCX no soportado"; return false;
    }
    int w = (d[8] | (d[9] << 8)) + 1;
    int h = (d[10] | (d[11] << 8)) + 1;
    int planos = d[65];
    int bytes_linea = d[66] | (d[67] << 8);
    if (w <= 0 || h <= 0 || (uint64_t)w * h > 120000000ull ||
        (planos != 1 && planos != 3) || bytes_linea < w) {
        error = L"PCX no valido"; return false;
    }
    std::vector<uint8_t> datos;
    datos.reserve((size_t)h * planos * bytes_linea);
    size_t pos = 128;
    while (pos < d.size() && datos.size() < (size_t)h * planos * bytes_linea) {
        uint8_t b = d[pos++];
        if ((b & 0xC0) == 0xC0) {
            int cuenta = b & 0x3F;
            if (pos >= d.size()) { error = L"PCX truncado"; return false; }
            uint8_t valor = d[pos++];
            for (int i = 0; i < cuenta; i++) datos.push_back(valor);
        } else {
            datos.push_back(b);
        }
    }
    if (datos.size() < (size_t)h * planos * bytes_linea) {
        error = L"PCX truncado"; return false;
    }
    std::vector<uint8_t> paleta;
    if (planos == 1) {
        if (d.size() >= 769 && d[d.size() - 769] == 0x0C)
            paleta.assign(d.end() - 768, d.end());
        else
            paleta.assign(d.begin() + 16, d.begin() + 64);
    }
    bmp.w = w; bmp.h = h; bmp.alfa = false;
    bmp.rgba.resize((size_t)w * h * 4);
    for (int y = 0; y < h; y++) {
        const uint8_t* linea = datos.data() + (size_t)y * planos * bytes_linea;
        for (int x = 0; x < w; x++) {
            uint8_t* q = bmp.rgba.data() + ((size_t)y * w + x) * 4;
            if (planos == 3) {
                q[0] = linea[x + bytes_linea * 0];
                q[1] = linea[x + bytes_linea * 1];
                q[2] = linea[x + bytes_linea * 2];
            } else {
                int idx = linea[x];
                if (!paleta.empty() && (size_t)idx * 3 + 2 < paleta.size()) {
                    q[0] = paleta[idx * 3];
                    q[1] = paleta[idx * 3 + 1];
                    q[2] = paleta[idx * 3 + 2];
                } else {
                    q[0] = q[1] = q[2] = (uint8_t)idx;
                }
            }
            q[3] = 255;
        }
    }
    return true;
}

static bool decodificar_dds(const std::wstring& ruta, Bitmap& bmp, std::wstring& error) {
    std::ifstream f(ruta, std::ios::binary);
    if (!f) { error = L"no se pudo abrir el DDS"; return false; }
    std::vector<uint8_t> d((std::istreambuf_iterator<char>(f)),
                           std::istreambuf_iterator<char>());
    if (d.size() < 128 || memcmp(d.data(), "DDS ", 4) != 0) {
        error = L"DDS no valido"; return false;
    }
    auto u32 = [&](size_t p) -> uint32_t {
        if (p + 4 > d.size()) return 0;
        return (uint32_t)d[p] | ((uint32_t)d[p + 1] << 8) |
               ((uint32_t)d[p + 2] << 16) | ((uint32_t)d[p + 3] << 24);
    };
    int h = (int)u32(12), w = (int)u32(16);
    if (w <= 0 || h <= 0 || (uint64_t)w * h > 120000000ull) {
        error = L"tamano DDS no valido"; return false;
    }
    uint32_t pf_flags = u32(80), fourcc = u32(84), bits = u32(88);
    uint32_t rm = u32(92), gm = u32(96), bm = u32(100), am = u32(104);
    bmp.w = w; bmp.h = h; bmp.alfa = false;
    bmp.rgba.assign((size_t)w * h * 4, 255);
    size_t pos = 128;
    if (fourcc == 0x31545844 || fourcc == 0x33545844 || fourcc == 0x35545844) {
        int bloques_x = (w + 3) / 4, bloques_y = (h + 3) / 4;
        int tam_bloque = fourcc == 0x31545844 ? 8 : 16;
        if (pos + (size_t)bloques_x * bloques_y * tam_bloque > d.size()) {
            error = L"DDS truncado"; return false;
        }
        auto c565 = [](uint16_t c, uint8_t& r, uint8_t& g, uint8_t& b) {
            r = (uint8_t)(((c >> 11) & 31) * 255 / 31);
            g = (uint8_t)(((c >> 5) & 63) * 255 / 63);
            b = (uint8_t)((c & 31) * 255 / 31);
        };
        for (int by = 0; by < bloques_y; by++) {
            for (int bx = 0; bx < bloques_x; bx++) {
                const uint8_t* bloque = d.data() + pos;
                uint8_t alfa[16];
                for (int i = 0; i < 16; i++) alfa[i] = 255;
                const uint8_t* colores = bloque;
                if (fourcc == 0x33545844) {
                    for (int i = 0; i < 16; i++) {
                        int nibble = (bloque[i / 2] >> ((i & 1) * 4)) & 0xF;
                        alfa[i] = (uint8_t)(nibble * 17);
                    }
                    colores = bloque + 8;
                } else if (fourcc == 0x35545844) {
                    uint8_t a0 = bloque[0], a1 = bloque[1];
                    uint8_t tabla[8] = {a0, a1};
                    if (a0 > a1) {
                        for (int i = 1; i <= 6; i++)
                            tabla[i + 1] = (uint8_t)(((7 - i) * a0 + i * a1) / 7);
                    } else {
                        for (int i = 1; i <= 4; i++)
                            tabla[i + 1] = (uint8_t)(((5 - i) * a0 + i * a1) / 5);
                        tabla[6] = 0;
                        tabla[7] = 255;
                    }
                    uint64_t bits_alfa = 0;
                    for (int i = 0; i < 6; i++) bits_alfa |= (uint64_t)bloque[2 + i] << (8 * i);
                    for (int i = 0; i < 16; i++)
                        alfa[i] = tabla[(bits_alfa >> (3 * i)) & 7];
                    colores = bloque + 8;
                }
                uint16_t c0 = (uint16_t)(colores[0] | (colores[1] << 8));
                uint16_t c1 = (uint16_t)(colores[2] | (colores[3] << 8));
                uint8_t pal[4][4];
                c565(c0, pal[0][0], pal[0][1], pal[0][2]);
                c565(c1, pal[1][0], pal[1][1], pal[1][2]);
                pal[0][3] = pal[1][3] = 255;
                if (c0 > c1) {
                    for (int c = 0; c < 3; c++) {
                        pal[2][c] = (uint8_t)((2 * pal[0][c] + pal[1][c]) / 3);
                        pal[3][c] = (uint8_t)((pal[0][c] + 2 * pal[1][c]) / 3);
                    }
                    pal[2][3] = pal[3][3] = 255;
                } else {
                    for (int c = 0; c < 3; c++)
                        pal[2][c] = (uint8_t)((pal[0][c] + pal[1][c]) / 2);
                    pal[2][3] = 255;
                    pal[3][0] = pal[3][1] = pal[3][2] = 0;
                    pal[3][3] = 0;
                }
                uint32_t indices = u32(pos + (fourcc == 0x31545844 ? 4 : 12));
                for (int py = 0; py < 4; py++) {
                    for (int px = 0; px < 4; px++) {
                        int x = bx * 4 + px, y = by * 4 + py;
                        if (x >= w || y >= h) continue;
                        int idx = (indices >> (2 * (py * 4 + px))) & 3;
                        uint8_t* q = bmp.rgba.data() + ((size_t)y * w + x) * 4;
                        q[0] = pal[idx][0]; q[1] = pal[idx][1]; q[2] = pal[idx][2];
                        q[3] = (uint8_t)((pal[idx][3] * alfa[py * 4 + px]) / 255);
                        if (q[3] < 255) bmp.alfa = true;
                    }
                }
                pos += tam_bloque;
            }
        }
        return true;
    }
    if (pf_flags & 0x40 && (bits == 24 || bits == 32)) {
        int canales = bits / 8;
        if (pos + (size_t)w * h * canales > d.size()) {
            error = L"DDS truncado"; return false;
        }
        auto canal = [](uint32_t v, uint32_t mascara, uint8_t por_defecto) -> uint8_t {
            if (!mascara) return por_defecto;
            int shift = 0;
            while (((mascara >> shift) & 1) == 0) shift++;
            uint32_t maximo = mascara >> shift;
            return (uint8_t)(((v >> shift) & maximo) * 255 / maximo);
        };
        for (size_t i = 0; i < (size_t)w * h; i++) {
            uint32_t v = 0;
            for (int c = 0; c < canales; c++) v |= (uint32_t)d[pos + i * canales + c] << (8 * c);
            uint8_t* q = bmp.rgba.data() + i * 4;
            q[0] = canal(v, rm, 0); q[1] = canal(v, gm, 0); q[2] = canal(v, bm, 0);
            q[3] = am ? canal(v, am, 255) : 255;
            if (q[3] < 255) bmp.alfa = true;
        }
        return true;
    }
    error = L"formato DDS no soportado";
    return false;
}

static bool decodificar_imagen(IWICImagingFactory* factory, const std::wstring& ruta,
                               Bitmap& bmp, std::wstring& error) {
    std::wstring ext = lower(fs::path(ruta).extension().wstring());
    if (ext == L".avif") return decodificar_avif(ruta, bmp, error);
    if (ext == L".tga") return decodificar_tga(ruta, bmp, error);
    if (ext == L".pcx") return decodificar_pcx(ruta, bmp, error);
    if (ext == L".dds") return decodificar_dds(ruta, bmp, error);
    if (ext == L".ppm" || ext == L".pgm" || ext == L".pbm")
        return decodificar_ppm(ruta, bmp, error);
    return decodificar_wic(factory, ruta, bmp, error);
}

struct Coeficientes {
    int ksize = 0;
    std::vector<int> bounds;
    std::vector<int> k;
};

static double filtro_lanczos(double x) {
    if (x < -3.0 || x >= 3.0) return 0.0;
    auto sinc = [](double v) {
        if (v == 0.0) return 1.0;
        v *= 3.14159265358979323846;
        return sin(v) / v;
    };
    return sinc(x) * sinc(x / 3.0);
}

static Coeficientes precomputar_coeficientes(int in_size, double in1, int out_size) {
    Coeficientes c;
    double scale = in1 / out_size;
    double filterscale = std::max(1.0, scale);
    double support = 3.0 * filterscale;
    c.ksize = (int)ceil(support) * 2 + 1;
    c.bounds.resize((size_t)out_size * 2);
    std::vector<double> pre((size_t)out_size * c.ksize, 0.0);
    double inv = 1.0 / filterscale;
    for (int xx = 0; xx < out_size; xx++) {
        double center = (xx + 0.5) * scale;
        double ww = 0.0;
        int xmin = (int)(center - support + 0.5);
        if (xmin < 0) xmin = 0;
        int xmax = (int)(center + support + 0.5);
        if (xmax > in_size) xmax = in_size;
        int usados = xmax - xmin;
        double* k = &pre[(size_t)xx * c.ksize];
        for (int x = 0; x < usados; x++) {
            double w = filtro_lanczos((x + xmin - center + 0.5) * inv);
            k[x] = w;
            ww += w;
        }
        if (ww != 0.0)
            for (int x = 0; x < usados; x++) k[x] /= ww;
        c.bounds[(size_t)xx * 2 + 0] = xmin;
        c.bounds[(size_t)xx * 2 + 1] = usados;
    }
    c.k.resize(pre.size());
    for (size_t i = 0; i < pre.size(); i++) {
        double v = pre[i] * (double)(1 << 22);
        c.k[i] = (int)(v < 0 ? -0.5 + v : 0.5 + v);
    }
    return c;
}

static inline uint8_t clip8_pillow(int v) {
    int n = v >> 22;
    if (n < -640) n = -640;
    if (n > 639) n = 639;
    return (uint8_t)(n < 0 ? 0 : (n > 255 ? 255 : n));
}

static void pasar_horizontal(const std::vector<uint8_t>& src, int sw, int sy0, int filas,
                             std::vector<uint8_t>& dst, int dw,
                             const Coeficientes& c) {
    dst.resize((size_t)dw * filas * 4);
    for (int y = 0; y < filas; y++) {
        const uint8_t* linea = src.data() + (size_t)(sy0 + y) * sw * 4;
        uint8_t* salida = dst.data() + (size_t)y * dw * 4;
        for (int x = 0; x < dw; x++) {
            int xmin = c.bounds[(size_t)x * 2];
            int usados = c.bounds[(size_t)x * 2 + 1];
            const int* k = &c.k[(size_t)x * c.ksize];
            int s0 = 1 << 21, s1 = 1 << 21, s2 = 1 << 21, s3 = 1 << 21;
            for (int i = 0; i < usados; i++) {
                const uint8_t* p = linea + (size_t)(xmin + i) * 4;
                s0 += p[0] * k[i];
                s1 += p[1] * k[i];
                s2 += p[2] * k[i];
                s3 += p[3] * k[i];
            }
            salida[x * 4 + 0] = clip8_pillow(s0);
            salida[x * 4 + 1] = clip8_pillow(s1);
            salida[x * 4 + 2] = clip8_pillow(s2);
            salida[x * 4 + 3] = clip8_pillow(s3);
        }
    }
}

static void pasar_vertical(const std::vector<uint8_t>& src, int sw, int,
                           std::vector<uint8_t>& dst, int dh,
                           const Coeficientes& c) {
    dst.resize((size_t)sw * dh * 4);
    for (int y = 0; y < dh; y++) {
        int ymin = c.bounds[(size_t)y * 2];
        int usados = c.bounds[(size_t)y * 2 + 1];
        const int* k = &c.k[(size_t)y * c.ksize];
        uint8_t* salida = dst.data() + (size_t)y * sw * 4;
        for (int x = 0; x < sw; x++) {
            int s0 = 1 << 21, s1 = 1 << 21, s2 = 1 << 21, s3 = 1 << 21;
            for (int i = 0; i < usados; i++) {
                const uint8_t* p = src.data() + ((size_t)(ymin + i) * sw + x) * 4;
                s0 += p[0] * k[i];
                s1 += p[1] * k[i];
                s2 += p[2] * k[i];
                s3 += p[3] * k[i];
            }
            salida[x * 4 + 0] = clip8_pillow(s0);
            salida[x * 4 + 1] = clip8_pillow(s1);
            salida[x * 4 + 2] = clip8_pillow(s2);
            salida[x * 4 + 3] = clip8_pillow(s3);
        }
    }
}

static int pillow_redondear_lado(double numero, bool horizontal, double aspecto, int fijo) {
    int abajo = (int)floor(numero);
    int arriba = (int)ceil(numero);
    auto clave = [&](int n) -> double {
        if (horizontal) return fabs(aspecto - (double)n / fijo);
        return n == 0 ? 0.0 : fabs(aspecto - (double)fijo / n);
    };
    int elegido = clave(arriba) < clave(abajo) ? arriba : abajo;
    return std::max(elegido, 1);
}

static void premultiplicar(std::vector<uint8_t>& px) {
    for (size_t i = 0; i < px.size(); i += 4) {
        unsigned a = px[i + 3];
        for (int c = 0; c < 3; c++) {
            unsigned tmp = px[i + c] * a + 128;
            px[i + c] = (uint8_t)(((tmp >> 8) + tmp) >> 8);
        }
    }
}

static void despremultiplicar(std::vector<uint8_t>& px) {
    for (size_t i = 0; i < px.size(); i += 4) {
        unsigned a = px[i + 3];
        if (a != 0 && a != 255) {
            for (int c = 0; c < 3; c++) {
                int v = (255 * px[i + c]) / (int)a;
                px[i + c] = (uint8_t)(v > 255 ? 255 : v);
            }
        }
    }
}

static void reducir_pillow(const std::vector<uint8_t>& src, int w, int h, int fx, int fy,
                           std::vector<uint8_t>& dst, int& dw, int& dh) {
    dw = w / fx;
    dh = h / fy;
    dst.assign((size_t)dw * dh * 4, 0);
    unsigned area = (unsigned)fx * fy;
    unsigned multiplier = (unsigned)(4294967296.0f / (float)(256 * area));
    unsigned amend = area / 2;
    for (int y = 0; y < dh; y++) {
        for (int x = 0; x < dw; x++) {
            for (int c = 0; c < 4; c++) {
                unsigned suma = amend;
                for (int yy = 0; yy < fy; yy++) {
                    const uint8_t* linea = src.data() +
                        ((size_t)(y * fy + yy) * w + x * fx) * 4;
                    for (int xx = 0; xx < fx; xx++) suma += linea[xx * 4 + c];
                }
                dst[((size_t)y * dw + x) * 4 + c] =
                    (uint8_t)(((uint64_t)suma * multiplier) >> 24);
            }
        }
    }
}

static bool redimensionar_lanczos(const Bitmap& in, int maxdim, Bitmap& out,
                                  std::wstring&) {
    if (maxdim <= 0 || (int)std::max(in.w, in.h) <= maxdim) { out = in; return true; }
    int w = (int)in.w, h = (int)in.h;
    double aspecto = (double)w / h;
    int nw = maxdim, nh = maxdim;
    if (maxdim >= w && maxdim >= h) { out = in; return true; }
    if ((double)maxdim / maxdim >= aspecto) {
        nh = maxdim;
        nw = pillow_redondear_lado((double)maxdim * aspecto, true, aspecto, maxdim);
    } else {
        nw = maxdim;
        nh = pillow_redondear_lado((double)maxdim / aspecto, false, aspecto, maxdim);
    }
    nw = std::clamp(nw, 1, w);
    nh = std::clamp(nh, 1, h);
    if (nw == w && nh == h) { out = in; return true; }

    std::vector<uint8_t> actual = in.rgba;
    int aw = w, ah = h;
    if (in.alfa) premultiplicar(actual);

    int fx = (int)((double)w / nw / 2.0);
    int fy = (int)((double)h / nh / 2.0);
    if (fx < 1) fx = 1;
    if (fy < 1) fy = 1;
    if (fx > 1 || fy > 1) {
        std::vector<uint8_t> reducido;
        reducir_pillow(actual, aw, ah, fx, fy, reducido, aw, ah);
        actual.swap(reducido);
    }

    bool need_h = nw != aw;
    bool need_v = nh != ah;
    double in1_x = (double)w / fx;
    double in1_y = (double)h / fy;
    bool horizontal_first = !((h - nh) > 0 && (h - nh) > (w - nw) * 2);

    Coeficientes cv, ch;
    if (need_v) cv = precomputar_coeficientes(ah, in1_y, nh);
    if (need_h) ch = precomputar_coeficientes(aw, in1_x, nw);

    std::vector<uint8_t> temp;
    if (horizontal_first) {
        if (need_h) {
            int y0 = 0, filas = ah;
            if (need_v) {
                y0 = cv.bounds[0];
                filas = cv.bounds[(size_t)nh * 2 - 2] + cv.bounds[(size_t)nh * 2 - 1] - y0;
                if (y0 != 0)
                    for (int i = 0; i < nh; i++) cv.bounds[(size_t)i * 2] -= y0;
            }
            pasar_horizontal(actual, aw, y0, filas, temp, nw, ch);
            actual.swap(temp);
            aw = nw;
            ah = filas;
        }
        if (need_v) {
            pasar_vertical(actual, aw, ah, temp, nh, cv);
            actual.swap(temp);
            ah = nh;
        }
    } else {
        if (need_v) {
            pasar_vertical(actual, aw, ah, temp, nh, cv);
            actual.swap(temp);
            ah = nh;
        }
        if (need_h) {
            pasar_horizontal(actual, aw, 0, ah, temp, nw, ch);
            actual.swap(temp);
            aw = nw;
        }
    }

    if (in.alfa) despremultiplicar(actual);
    out.w = aw;
    out.h = ah;
    out.alfa = in.alfa;
    out.icc = in.icc;
    out.rgba = std::move(actual);
    return true;
}

static bool dimensiones_wic(IWICImagingFactory* factory, const std::wstring& ruta,
                            UINT& ancho, UINT& alto) {
    ComPtr<IWICBitmapDecoder> decoder;
    if (FAILED(factory->CreateDecoderFromFilename(ruta.c_str(), nullptr, GENERIC_READ,
                                                  WICDecodeMetadataCacheOnDemand, &decoder)))
        return false;
    ComPtr<IWICBitmapFrameDecode> frame;
    if (FAILED(decoder->GetFrame(0, &frame))) return false;
    return SUCCEEDED(frame->GetSize(&ancho, &alto));
}

static long redondear_python(double v) {
    double abajo = floor(v);
    double frac = v - abajo;
    if (frac > 0.5) return (long)abajo + 1;
    if (frac < 0.5) return (long)abajo;
    return ((long)abajo % 2 == 0) ? (long)abajo : (long)abajo + 1;
}

static std::wstring aplicar_patron(const std::wstring& patron, IWICImagingFactory* factory,
                                   const fs::path& src, const fs::path& rel,
                                   int maxdim, int calidad, const std::wstring& formato) {
    if (patron.empty()) return rel.wstring();

    UINT ancho = 0, alto = 0;
    bool necesita_dim = patron.find(L"{ancho}") != std::wstring::npos ||
                        patron.find(L"{alto}") != std::wstring::npos;
    if (necesita_dim) {
        bool ok = dimensiones_wic(factory, src.wstring(), ancho, alto);
        if (!ok || ancho == 0 || alto == 0) {
            Bitmap tmp;
            std::wstring error;
            if (decodificar_imagen(factory, src.wstring(), tmp, error)) {
                ancho = tmp.w;
                alto = tmp.h;
            }
        }
    }
    if (maxdim > 0 && (int)std::max(ancho, alto) > maxdim) {
        double escala = (double)maxdim / (double)std::max(ancho, alto);
        ancho = (UINT)std::max<long>(1, redondear_python(ancho * escala));
        alto = (UINT)std::max<long>(1, redondear_python(alto * escala));
    }

    std::wstring texto = patron;
    auto reemplazar = [&](const wchar_t* clave, const std::wstring& valor) {
        size_t pos;
        while ((pos = texto.find(clave)) != std::wstring::npos)
            texto.replace(pos, wcslen(clave), valor);
    };
    reemplazar(L"{nombre}", rel.stem().wstring());
    reemplazar(L"{ancho}", std::to_wstring(ancho));
    reemplazar(L"{alto}", std::to_wstring(alto));
    reemplazar(L"{formato}", formato == L"AVIF" ? L"avif" : L"webp");
    reemplazar(L"{calidad}", std::to_wstring(calidad));
    reemplazar(L"{fecha}", fecha_compacta());
    if (texto.find(L"{hash}") != std::wstring::npos)
        reemplazar(L"{hash}", sha1_hex(src).substr(0, 10));

    std::wstring nombre = nombre_seguro(texto, rel.stem().wstring());
    return (rel.parent_path() / (nombre + (formato == L"AVIF" ? L".avif" : L".webp"))).wstring();
}

static std::vector<uint8_t> codificar_webp(const Bitmap& bmp, int calidad, int metodo,
                                           bool sin_perdida, bool alfa, std::wstring& error) {
    WebPConfig config;
    if (!WebPConfigInit(&config)) { error = L"WebPConfigInit"; return {}; }
    config.quality = (float)calidad;
    config.method = metodo;
    config.lossless = sin_perdida ? 1 : 0;
    config.exact = alfa ? 1 : 0;
    config.alpha_quality = 100;
    config.alpha_compression = 1;
    config.alpha_filtering = 1;

    WebPPicture pic;
    if (!WebPPictureInit(&pic)) { error = L"WebPPictureInit"; return {}; }
    pic.width = (int)bmp.w;
    pic.height = (int)bmp.h;
    pic.use_argb = 1;

    WebPMemoryWriter writer;
    WebPMemoryWriterInit(&writer);
    pic.writer = WebPMemoryWrite;
    pic.custom_ptr = &writer;

    bool ok;
    if (alfa) {
        ok = WebPPictureImportRGBA(&pic, bmp.rgba.data(), (int)bmp.w * 4) != 0;
    } else {
        std::vector<uint8_t> rgb((size_t)bmp.w * bmp.h * 3);
        for (size_t i = 0; i < (size_t)bmp.w * bmp.h; i++) {
            rgb[i * 3 + 0] = bmp.rgba[i * 4 + 0];
            rgb[i * 3 + 1] = bmp.rgba[i * 4 + 1];
            rgb[i * 3 + 2] = bmp.rgba[i * 4 + 2];
        }
        ok = WebPPictureImportRGB(&pic, rgb.data(), (int)bmp.w * 3) != 0;
    }
    if (ok) ok = WebPEncode(&config, &pic) != 0;
    if (!ok) {
        error = L"WebPEncode fallo";
        WebPMemoryWriterClear(&writer);
        WebPPictureFree(&pic);
        return {};
    }

    std::vector<uint8_t> datos(writer.mem, writer.mem + writer.size);
    if (!bmp.icc.empty()) {
        WebPData entrada{datos.data(), datos.size()};
        WebPMux* mux = WebPMuxCreate(&entrada, 1);
        if (mux) {
            WebPData perfil{bmp.icc.data(), bmp.icc.size()};
            if (WebPMuxSetChunk(mux, "ICCP", &perfil, 1) == WEBP_MUX_OK) {
                WebPData ensamblado{nullptr, 0};
                if (WebPMuxAssemble(mux, &ensamblado) == WEBP_MUX_OK &&
                    ensamblado.bytes && ensamblado.size) {
                    datos.assign(ensamblado.bytes, ensamblado.bytes + ensamblado.size);
                }
                WebPDataClear(&ensamblado);
            }
            WebPMuxDelete(mux);
        }
    }
    WebPMemoryWriterClear(&writer);
    WebPPictureFree(&pic);
    return datos;
}

static thread_local int t_hilos_avif = 4;

static std::vector<uint8_t> codificar_avif(const Bitmap& bmp, int calidad, int metodo,
                                           bool sin_perdida, bool alfa, std::wstring& error) {
    avifPixelFormat formato = (sin_perdida || alfa) ? AVIF_PIXEL_FORMAT_YUV444
                                                    : AVIF_PIXEL_FORMAT_YUV420;
    avifImage* imagen = avifImageCreate((uint32_t)bmp.w, (uint32_t)bmp.h, 8, formato);
    if (!imagen) { error = L"avifImageCreate"; return {}; }
    if (sin_perdida) {
        imagen->matrixCoefficients = AVIF_MATRIX_COEFFICIENTS_IDENTITY;
        imagen->yuvRange = AVIF_RANGE_FULL;
    }
    if (!bmp.icc.empty())
        avifImageSetProfileICC(imagen, bmp.icc.data(), bmp.icc.size());
    avifResult r = avifImageAllocatePlanes(
        imagen, alfa ? (AVIF_PLANES_YUV | AVIF_PLANES_A) : AVIF_PLANES_YUV);
    if (r != AVIF_RESULT_OK) {
        avifImageDestroy(imagen);
        error = L"avifImageAllocatePlanes";
        return {};
    }

    avifRGBImage rgb;
    avifRGBImageSetDefaults(&rgb, imagen);
    rgb.depth = 8;
    rgb.format = alfa ? AVIF_RGB_FORMAT_RGBA : AVIF_RGB_FORMAT_RGB;
    rgb.alphaPremultiplied = AVIF_FALSE;
    rgb.rowBytes = (uint32_t)bmp.w * (alfa ? 4 : 3);
    std::vector<uint8_t> pixeles;
    if (alfa) {
        pixeles = bmp.rgba;
    } else {
        pixeles.resize((size_t)bmp.w * bmp.h * 3);
        for (size_t i = 0; i < (size_t)bmp.w * bmp.h; i++) {
            pixeles[i * 3 + 0] = bmp.rgba[i * 4 + 0];
            pixeles[i * 3 + 1] = bmp.rgba[i * 4 + 1];
            pixeles[i * 3 + 2] = bmp.rgba[i * 4 + 2];
        }
    }
    rgb.pixels = pixeles.data();
    r = avifImageRGBToYUV(imagen, &rgb);
    if (r != AVIF_RESULT_OK) {
        avifImageDestroy(imagen);
        error = L"avifImageRGBToYUV";
        return {};
    }

    avifEncoder* encoder = avifEncoderCreate();
    if (!encoder) {
        avifImageDestroy(imagen);
        error = L"avifEncoderCreate";
        return {};
    }
    encoder->quality = std::clamp(calidad, 1, 100);
    encoder->qualityAlpha = std::clamp(calidad, 1, 100);
    encoder->speed = (metodo >= 4) ? 6 : 8;
    encoder->maxThreads = t_hilos_avif;
    if (sin_perdida) {
        encoder->minQuantizer = 0;
        encoder->maxQuantizer = 0;
        encoder->minQuantizerAlpha = 0;
        encoder->maxQuantizerAlpha = 0;
    }

    avifRWData salida = AVIF_DATA_EMPTY;
    r = avifEncoderAddImage(encoder, imagen, 1, AVIF_ADD_IMAGE_FLAG_SINGLE);
    if (r == AVIF_RESULT_OK) r = avifEncoderFinish(encoder, &salida);
    std::vector<uint8_t> datos;
    if (r == AVIF_RESULT_OK && salida.data && salida.size)
        datos.assign(salida.data, salida.data + salida.size);
    else {
        error = L"avifEncoder: ";
        for (const char* p = avifResultToString(r); p && *p; p++)
            error += (wchar_t)(unsigned char)*p;
    }
    avifRWDataFree(&salida);
    avifEncoderDestroy(encoder);
    avifImageDestroy(imagen);
    return datos;
}

static std::vector<uint8_t> codificar_objetivo(const Bitmap& bmp, int calidad_max, int metodo,
                                               bool sin_perdida, bool alfa, int objetivo_kb,
                                               bool es_avif, std::wstring& aviso,
                                               std::wstring& error) {
    auto codificar = [&](int q) {
        return es_avif ? codificar_avif(bmp, q, metodo, sin_perdida, alfa, error)
                       : codificar_webp(bmp, q, metodo, sin_perdida, alfa, error);
    };
    if (objetivo_kb <= 0 || sin_perdida) {
        return codificar(calidad_max);
    }
    size_t objetivo = (size_t)objetivo_kb * 1024;
    int bajo = 1, alto = std::max(1, calidad_max);
    std::vector<uint8_t> mejor;
    for (int i = 0; i < 7 && bajo <= alto; i++) {
        int medio = (bajo + alto) / 2;
        auto datos = codificar(medio);
        if (error.size()) return {};
        if (datos.size() <= objetivo) {
            mejor = std::move(datos);
            bajo = medio + 1;
        } else {
            alto = medio - 1;
        }
    }
    if (!mejor.empty()) return mejor;
    auto datos = codificar(1);
    if (datos.size() > objetivo) {
        wchar_t buf[96];
        swprintf_s(buf, L"no se alcanzo %d KB (minimo %.0f KB)", objetivo_kb, datos.size() / 1024.0);
        aviso = buf;
    }
    return datos;
}

// ---------------------------------------------------------------------------
//  Estado de la aplicacion
// ---------------------------------------------------------------------------
struct Options {
    int calidad = 85;
    int metodo = 6;
    bool sin_perdida = false;
    bool reencode = false;
    int max_dim = 0;
    int objetivo_kb = 0;
    bool mantener = true;
    bool sobrescribir = true;
    bool zip = false;
    bool beep = true;
    bool csv = false;
    std::wstring salida;
    std::wstring zip_nombre = L"imagenes_2.9.0-cpp.zip";
    std::wstring patron;
    std::wstring formato = L"WEBP";
    bool dry_run = false;
    bool flatten = false;
    COLORREF color = RGB(255, 255, 255);
    bool apagar = false;
    bool preservar_fecha = false;
    int jobs = 0;
};

struct Preset {
    std::wstring nombre;
    Options opt;
    bool propio = false;
};

struct HistoryEntry {
    std::wstring fecha;
    std::vector<std::wstring> origenes;
    std::wstring salida;
    Options opt;
    int convertidas = 0;
};

static std::vector<Preset> g_presets;

static std::wstring serializar_opciones(const Options& o) {
    std::wostringstream s;
    s << L"calidad=" << o.calidad
      << L"|metodo=" << o.metodo
      << L"|lossless=" << (o.sin_perdida ? 1 : 0)
      << L"|reencode=" << (o.reencode ? 1 : 0)
      << L"|maxdim=" << o.max_dim
      << L"|objetivo=" << o.objetivo_kb
      << L"|mantener=" << (o.mantener ? 1 : 0)
      << L"|sobrescribir=" << (o.sobrescribir ? 1 : 0)
      << L"|zip=" << (o.zip ? 1 : 0)
      << L"|beep=" << (o.beep ? 1 : 0)
      << L"|csv=" << (o.csv ? 1 : 0)
      << L"|dry=" << (o.dry_run ? 1 : 0)
      << L"|flatten=" << (o.flatten ? 1 : 0)
      << L"|color=" << (unsigned long)o.color
      << L"|apagar=" << (o.apagar ? 1 : 0)
      << L"|fecha=" << (o.preservar_fecha ? 1 : 0)
      << L"|jobs=" << o.jobs
      << L"|salida=" << o.salida
      << L"|zipnombre=" << o.zip_nombre
      << L"|patron=" << o.patron
      << L"|formato=" << o.formato;
    return s.str();
}

static void normalizar_opciones(Options& o) {
    o.calidad = std::clamp(o.calidad, 1, 100);
    o.metodo = std::clamp(o.metodo, 0, 6);
    if (o.max_dim < 0) o.max_dim = 0;
    if (o.max_dim > 0) {
        static const int dims[] = {2560, 1920, 1600, 1200, 800, 512};
        bool valido = false;
        for (int d : dims) if (o.max_dim == d) valido = true;
        if (!valido) o.max_dim = 0;
    }
    o.objetivo_kb = std::clamp(o.objetivo_kb, 0, 1000000);
    o.jobs = std::clamp(o.jobs, 0, 32);
    o.formato = (_wcsicmp(o.formato.c_str(), L"AVIF") == 0) ? L"AVIF" : L"WEBP";
    if (o.zip_nombre.empty()) o.zip_nombre = L"webp.zip";
    if (o.patron.size() > 512) o.patron.resize(512);
}

static Options deserializar_opciones(const std::wstring& texto) {
    Options o;
    std::wstringstream ss(texto);
    std::wstring campo;
    while (std::getline(ss, campo, L'|')) {
        size_t pos = campo.find(L'=');
        if (pos == std::wstring::npos) continue;
        std::wstring clave = campo.substr(0, pos);
        std::wstring valor = campo.substr(pos + 1);
        if (clave == L"calidad") o.calidad = _wtoi(valor.c_str());
        else if (clave == L"metodo") o.metodo = _wtoi(valor.c_str());
        else if (clave == L"lossless") o.sin_perdida = _wtoi(valor.c_str()) != 0;
        else if (clave == L"reencode") o.reencode = _wtoi(valor.c_str()) != 0;
        else if (clave == L"maxdim") o.max_dim = _wtoi(valor.c_str());
        else if (clave == L"objetivo") o.objetivo_kb = _wtoi(valor.c_str());
        else if (clave == L"mantener") o.mantener = _wtoi(valor.c_str()) != 0;
        else if (clave == L"sobrescribir") o.sobrescribir = _wtoi(valor.c_str()) != 0;
        else if (clave == L"zip") o.zip = _wtoi(valor.c_str()) != 0;
        else if (clave == L"beep") o.beep = _wtoi(valor.c_str()) != 0;
        else if (clave == L"csv") o.csv = _wtoi(valor.c_str()) != 0;
        else if (clave == L"dry") o.dry_run = _wtoi(valor.c_str()) != 0;
        else if (clave == L"flatten") o.flatten = _wtoi(valor.c_str()) != 0;
        else if (clave == L"color") o.color = (COLORREF)wcstoul(valor.c_str(), nullptr, 10);
        else if (clave == L"apagar") o.apagar = _wtoi(valor.c_str()) != 0;
        else if (clave == L"fecha") o.preservar_fecha = _wtoi(valor.c_str()) != 0;
        else if (clave == L"jobs") o.jobs = _wtoi(valor.c_str());
        else if (clave == L"salida") o.salida = valor;
        else if (clave == L"zipnombre") o.zip_nombre = valor;
        else if (clave == L"patron") o.patron = valor;
        else if (clave == L"formato") o.formato = (valor == L"AVIF") ? L"AVIF" : L"WEBP";
    }
    normalizar_opciones(o);
    return o;
}

static void inicializar_presets() {
    g_presets.clear();
    Options web;
    web.calidad = 85;
    web.max_dim = 1920;
    g_presets.push_back({L"Web (85 - 1920 px)", web, false});

    Options catalogo;
    catalogo.calidad = 80;
    catalogo.max_dim = 1200;
    g_presets.push_back({L"Catalogo (80 - 1200 px)", catalogo, false});

    Options iconos;
    iconos.sin_perdida = true;
    g_presets.push_back({L"Iconos sin perdida", iconos, false});

    Options maxima;
    maxima.calidad = 95;
    g_presets.push_back({L"Maxima calidad (95)", maxima, false});
}

struct Row {
    std::wstring archivo;
    std::wstring origen;
    double kb_in = 0, kb_out = 0, ahorro = 0;
    std::wstring estado;
    std::wstring ruta;
    std::wstring nota;
};

struct Evento {
    enum Tipo { LOG, PROGRESO, ESTADO, FILA, FIN, ACTUALIZACION, DESCARGA,
                DESCARGA_LISTA, DESCARGA_ERROR } tipo;
    std::wstring texto;
    std::wstring extra;
    std::wstring extra2;
    std::wstring extra3;
    int a = 0, b = 0;
    int c = 0;
    int modo = 0;
    Row fila;
};

static HWND g_hwnd = nullptr;
static HWND g_lista_origen = nullptr;
static HWND g_lista_resultados = nullptr;
static HWND g_edit_log = nullptr;
static HWND g_tab = nullptr;
static int g_tab_sel = 0;
static HWND g_edit_salida = nullptr;
static HWND g_edit_zip = nullptr;
static HWND g_edit_objetivo = nullptr;
static HWND g_track_calidad = nullptr;
static HWND g_check_reencode = nullptr;
static HWND g_lbl_calidad = nullptr;
static HWND g_combo_metodo = nullptr;
static HWND g_combo_dim = nullptr;
static HWND g_progress = nullptr;
static HWND g_status = nullptr;
static HWND g_btn_convertir = nullptr;
static HWND g_btn_cancelar = nullptr;
static HWND g_btn_pausa = nullptr;
static HWND g_check_lossless = nullptr;
static HWND g_check_sobrescribir = nullptr;
static HWND g_check_estructura = nullptr;
static HWND g_check_zip = nullptr;
static HWND g_check_csv = nullptr;
static HWND g_check_beep = nullptr;
static bool g_ui_beep = true;
static bool g_ui_apagar = false;
static HWND g_lbl_count = nullptr;
static std::map<HWND, std::wstring> g_tips;
static std::wstring g_tip_drop_texto;
static HWND g_tip_hwnd = nullptr;
static HWND g_tip_control = nullptr;
static DWORD g_tip_inicio = 0;
static std::wstring g_tip_texto_actual;static HWND g_edit_patron = nullptr;
static HWND g_check_dry = nullptr;
static HWND g_check_fecha = nullptr;
static HWND g_check_flatten = nullptr;
static HWND g_btn_color = nullptr;
static HWND g_check_apagar = nullptr;
static HWND g_combo_preset = nullptr;
static HWND g_combo_formato = nullptr;
static HWND g_btn_save_preset = nullptr;
static HWND g_btn_del_preset = nullptr;
static HWND g_btn_archive = nullptr;
static HWND g_btn_pattern_help = nullptr;
static HWND g_lbl_extractor = nullptr;
static HWND g_scroll_origen = nullptr;
static HMENU g_menu_hist = nullptr;
static HMENU g_menu_principal = nullptr;
static HMENU g_menu_ayuda = nullptr;
static int g_menu_hot = -1;
static std::vector<HistoryEntry> g_historial;
static std::wstring g_patron_temporal;
static bool g_aplicando_preset = false;
static COLORREF g_color = RGB(255, 255, 255);

static HFONT g_font = nullptr, g_font_bold = nullptr, g_font_title = nullptr, g_font_mono = nullptr;
static HFONT g_font_section = nullptr, g_font_muted = nullptr, g_font_start = nullptr;
static HFONT g_font_tip = nullptr;
static HFONT g_font_drop = nullptr;
static HFONT g_font_drop_hot = nullptr;
static bool g_drop_hot = false;
static bool g_ui_lista = false;
static HBRUSH g_brush_bg = nullptr, g_brush_card = nullptr, g_brush_dark = nullptr, g_brush_log = nullptr;
static HBRUSH g_brush_disabled = nullptr;
static HBRUSH g_brush_border = nullptr, g_brush_input = nullptr;

struct PaletaCpp {
    COLORREF bg, card, text, muted, accent, accent_dark, border, input;
    COLORREF drop, drop_hot, sel, log_bg, log_fg, hover, ok, err, disabled;
};
static PaletaCpp g_pal{};
static bool g_oscuro = false;

static void aplicar_modo_global_cpp(bool oscuro) {
    HMODULE ux = GetModuleHandleW(L"uxtheme.dll");
    if (!ux) return;
    using PrefFn = int(WINAPI*)(int);
    using FlushFn = void(WINAPI*)();
    auto pref = (PrefFn)GetProcAddress(ux, MAKEINTRESOURCEA(135));
    auto flush = (FlushFn)GetProcAddress(ux, MAKEINTRESOURCEA(136));
    if (pref) pref(oscuro ? 2 : 3);
    if (flush) flush();
}

static void convertir_botones_tema(bool oscuro);

static void aplicar_modo_oscuro_nativo(HWND hwnd, bool oscuro) {
    BOOL valor = oscuro ? TRUE : FALSE;
    DwmSetWindowAttribute(hwnd, 20, &valor, sizeof(valor));
    DwmSetWindowAttribute(hwnd, 19, &valor, sizeof(valor));
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    HMODULE ux = GetModuleHandleW(L"uxtheme.dll");
    if (ux) {
        using PrefFn = int(WINAPI*)(int);
        using AllowFn = bool(WINAPI*)(HWND, bool);
        using FlushFn = void(WINAPI*)();
        auto pref = (PrefFn)GetProcAddress(ux, MAKEINTRESOURCEA(135));
        auto allow = (AllowFn)GetProcAddress(ux, MAKEINTRESOURCEA(133));
        auto flush = (FlushFn)GetProcAddress(ux, MAKEINTRESOURCEA(136));
        if (pref) pref(oscuro ? 2 : 3);
        if (allow) allow(hwnd, oscuro);
        if (flush) flush();
    }
    if (hwnd == g_hwnd && g_menu_principal) {
        SetMenu(hwnd, oscuro ? nullptr : g_menu_principal);
        DrawMenuBar(hwnd);
    }
    if (hwnd == g_hwnd && g_tab) {
        ShowWindow(g_tab, oscuro ? SW_HIDE : SW_SHOW);
        TabCtrl_SetCurSel(g_tab, g_tab_sel);
        ShowWindow(g_lista_resultados, g_tab_sel == 0 ? SW_SHOW : SW_HIDE);
        ShowWindow(g_edit_log, g_tab_sel == 1 ? SW_SHOW : SW_HIDE);
    }
    SetWindowTheme(hwnd, oscuro ? L"DarkMode_Explorer" : L"Explorer", nullptr);
    EnumChildWindows(hwnd, [](HWND child, LPARAM lp) -> BOOL {
        bool oscuro = lp != 0;
        wchar_t clase[64]{};
        GetClassNameW(child, clase, 64);
        if (oscuro) {
            HMODULE ux = GetModuleHandleW(L"uxtheme.dll");
            if (ux) {
                using AllowFn = bool(WINAPI*)(HWND, bool);
                auto allow = (AllowFn)GetProcAddress(ux, MAKEINTRESOURCEA(133));
                if (allow) allow(child, true);
            }
            if (lstrcmpiW(clase, L"ComboBox") == 0)
                SetWindowTheme(child, L"DarkMode_CFD", nullptr);
            else
                SetWindowTheme(child, L"DarkMode_Explorer", nullptr);
        } else {
            if (lstrcmpiW(clase, L"ComboBox") == 0)
                SetWindowTheme(child, L"", nullptr);
            else
                SetWindowTheme(child, L"Explorer", nullptr);
        }
        return TRUE;
    }, oscuro ? 1 : 0);
    if (g_lista_origen) {
        ListView_SetBkColor(g_lista_origen, g_pal.card);
        ListView_SetTextBkColor(g_lista_origen, g_pal.card);
        ListView_SetTextColor(g_lista_origen, g_pal.text);
    }
    if (g_lista_resultados) {
        ListView_SetBkColor(g_lista_resultados, g_pal.card);
        ListView_SetTextBkColor(g_lista_resultados, g_pal.card);
        ListView_SetTextColor(g_lista_resultados, g_pal.text);
    }
    if (g_progress) {
        SetWindowTheme(g_progress, oscuro ? L"" : L"Explorer", nullptr);
        SendMessageW(g_progress, PBM_SETBARCOLOR, 0, (LPARAM)g_pal.accent);
        SendMessageW(g_progress, PBM_SETBKCOLOR, 0, (LPARAM)g_pal.border);
    }
    convertir_botones_tema(oscuro);
    InvalidateRect(hwnd, nullptr, TRUE);
    RedrawWindow(hwnd, nullptr, nullptr,
                 RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_ERASE | RDW_UPDATENOW);
}

static void dibujar_icono_tema(const DRAWITEMSTRUCT* di) {
    FillRect(di->hDC, &di->rcItem, g_brush_bg);
    RECT rc = di->rcItem;
    int cx = (rc.left + rc.right) / 2;
    int cy = (rc.top + rc.bottom) / 2;
    if (g_oscuro) {
        HBRUSH claro = CreateSolidBrush(RGB(226, 232, 240));
        HBRUSH fondo = CreateSolidBrush(g_pal.bg);
        HGDIOBJ viejo = SelectObject(di->hDC, claro);
        Ellipse(di->hDC, cx - 9, cy - 9, cx + 9, cy + 9);
        SelectObject(di->hDC, fondo);
        Ellipse(di->hDC, cx - 2, cy - 12, cx + 15, cy + 5);
        SelectObject(di->hDC, viejo);
        DeleteObject(claro);
        DeleteObject(fondo);
    } else {
        HBRUSH sol = CreateSolidBrush(RGB(246, 195, 67));
        HPEN rayo = CreatePen(PS_SOLID, 2, RGB(246, 195, 67));
        HGDIOBJ viejoB = SelectObject(di->hDC, sol);
        HGDIOBJ viejoP = SelectObject(di->hDC, rayo);
        Ellipse(di->hDC, cx - 5, cy - 5, cx + 5, cy + 5);
        const int puntos[8][4] = {
            {cx, cy - 12, cx, cy - 8}, {cx, cy + 8, cx, cy + 12},
            {cx - 12, cy, cx - 8, cy}, {cx + 8, cy, cx + 12, cy},
            {cx - 9, cy - 9, cx - 6, cy - 6}, {cx + 6, cy + 6, cx + 9, cy + 9},
            {cx + 9, cy - 9, cx + 6, cy - 6}, {cx - 6, cy + 6, cx - 9, cy + 9},
        };
        for (auto& p : puntos) {
            MoveToEx(di->hDC, p[0], p[1], nullptr);
            LineTo(di->hDC, p[2], p[3]);
        }
        SelectObject(di->hDC, viejoP);
        SelectObject(di->hDC, viejoB);
        DeleteObject(rayo);
        DeleteObject(sol);
    }
    if (di->itemState & ODS_FOCUS) DrawFocusRect(di->hDC, &di->rcItem);
}

static void convertir_botones_tema(bool oscuro) {
    EnumChildWindows(g_hwnd, [](HWND child, LPARAM lp) -> BOOL {
        wchar_t clase[64]{};
        GetClassNameW(child, clase, 64);
        if (_wcsicmp(clase, L"Button") != 0) return TRUE;
        LONG_PTR estilo = GetWindowLongPtrW(child, GWL_STYLE);
        LONG_PTR tipo = estilo & BS_TYPEMASK;
        if (tipo != BS_PUSHBUTTON && tipo != BS_DEFPUSHBUTTON) return TRUE;
        LONG_PTR nuevo = (estilo & ~BS_TYPEMASK) | (lp ? BS_OWNERDRAW : BS_PUSHBUTTON);
        SetWindowLongPtrW(child, GWL_STYLE, nuevo);
        SetWindowPos(child, nullptr, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
        InvalidateRect(child, nullptr, TRUE);
        return TRUE;
    }, oscuro ? 1 : 0);
}

static void dibujar_combo(const DRAWITEMSTRUCT* di) {
    bool abierto = di->itemID != (UINT)-1;
    wchar_t texto[256]{};
    if (abierto) {
        SendMessageW(di->hwndItem, CB_GETLBTEXT, (WPARAM)di->itemID, (LPARAM)texto);
    } else {
        int sel = (int)SendMessageW(di->hwndItem, CB_GETCURSEL, 0, 0);
        if (sel >= 0)
            SendMessageW(di->hwndItem, CB_GETLBTEXT, (WPARAM)sel, (LPARAM)texto);
    }
    bool seleccionado = abierto && (di->itemState & ODS_SELECTED);
    COLORREF fondo = seleccionado ? g_pal.sel : g_pal.input;
    if (!IsWindowEnabled(di->hwndItem)) fondo = g_pal.disabled;
    HBRUSH br = CreateSolidBrush(fondo);
    FillRect(di->hDC, &di->rcItem, br);
    DeleteObject(br);
    if (!abierto) {
        FrameRect(di->hDC, &di->rcItem, g_brush_border);
        int cx = di->rcItem.right - 11;
        int cy = (di->rcItem.top + di->rcItem.bottom) / 2;
        POINT flecha[3] = {{cx - 4, cy - 2}, {cx + 4, cy - 2}, {cx, cy + 3}};
        HBRUSH color = CreateSolidBrush(IsWindowEnabled(di->hwndItem) ? g_pal.muted
                                                                      : g_pal.border);
        HGDIOBJ viejo = SelectObject(di->hDC, color);
        Polygon(di->hDC, flecha, 3);
        SelectObject(di->hDC, viejo);
        DeleteObject(color);
    }
    RECT zona = di->rcItem;
    zona.left += 6;
    zona.right -= abierto ? 6 : 20;
    SetBkMode(di->hDC, TRANSPARENT);
    SetTextColor(di->hDC, IsWindowEnabled(di->hwndItem) ? g_pal.text : g_pal.muted);
    HFONT vieja = (HFONT)SelectObject(di->hDC, g_font);
    DrawTextW(di->hDC, texto, -1, &zona,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(di->hDC, vieja);
}

static void dibujar_boton_tema(const DRAWITEMSTRUCT* di) {
    RECT wr{};
    GetWindowRect(di->hwndItem, &wr);
    HWND padre = GetParent(di->hwndItem);
    if (!padre) padre = g_hwnd;
    POINT esquina{wr.left, wr.top};
    ScreenToClient(padre, &esquina);
    RECT pc{};
    GetClientRect(padre, &pc);
    bool en_pie = pc.bottom > 0 && esquina.y >= pc.bottom / 2;
    HBRUSH fondo = CreateSolidBrush(en_pie ? g_pal.bg : g_pal.card);
    FillRect(di->hDC, &di->rcItem, fondo);
    DeleteObject(fondo);
    RECT r = di->rcItem;
    InflateRect(&r, -1, -1);
    COLORREF relleno = (di->itemState & ODS_DISABLED)
                           ? g_pal.disabled
                           : ((di->itemState & ODS_SELECTED)
                                  ? g_pal.hover
                                  : (en_pie ? g_pal.bg : g_pal.card));
    HBRUSH br = CreateSolidBrush(relleno);
    HPEN pen = CreatePen(PS_SOLID, 1, g_pal.border);
    HGDIOBJ viejoB = SelectObject(di->hDC, br);
    HGDIOBJ viejoP = SelectObject(di->hDC, pen);
    RoundRect(di->hDC, r.left, r.top, r.right, r.bottom, 8, 8);
    SelectObject(di->hDC, viejoP);
    SelectObject(di->hDC, viejoB);
    DeleteObject(pen);
    DeleteObject(br);
    wchar_t texto[160]{};
    GetWindowTextW(di->hwndItem, texto, 160);
    SetBkMode(di->hDC, TRANSPARENT);
    SetTextColor(di->hDC, (di->itemState & ODS_DISABLED) ? g_pal.muted : g_pal.text);
    HFONT vieja = (HFONT)SelectObject(di->hDC, g_font);
    RECT zona = di->rcItem;
    DrawTextW(di->hDC, texto, -1, &zona,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(di->hDC, vieja);
    if (di->itemState & ODS_FOCUS) DrawFocusRect(di->hDC, &r);
}

static void aplicar_paleta_cpp(bool oscuro) {
    g_oscuro = oscuro;
    if (oscuro) {
        g_pal = {RGB(15, 23, 42), RGB(27, 36, 51), RGB(230, 237, 247), RGB(154, 168, 187),
                 RGB(76, 154, 255), RGB(143, 194, 255), RGB(46, 58, 77), RGB(17, 26, 41),
                 RGB(20, 30, 46), RGB(29, 42, 63), RGB(41, 64, 95), RGB(10, 18, 32),
                 RGB(203, 213, 225), RGB(38, 54, 77), RGB(74, 222, 128), RGB(248, 113, 113),
                 RGB(35, 45, 60)};
    } else {
        g_pal = {RGB(238, 241, 246), RGB(255, 255, 255), RGB(31, 41, 51), RGB(95, 107, 122),
                 RGB(26, 115, 232), RGB(21, 87, 176), RGB(221, 227, 234), RGB(255, 255, 255),
                 RGB(250, 251, 253), RGB(242, 247, 255), RGB(215, 227, 251), RGB(15, 23, 42),
                 RGB(203, 213, 225), RGB(232, 238, 246), RGB(19, 115, 51), RGB(197, 34, 31),
                 RGB(240, 240, 240)};
    }
    for (HBRUSH* b : {&g_brush_bg, &g_brush_card, &g_brush_dark, &g_brush_log,
                      &g_brush_disabled, &g_brush_border, &g_brush_input}) {
        if (*b) {
            DeleteObject(*b);
            *b = nullptr;
        }
    }
    g_brush_bg = CreateSolidBrush(g_pal.bg);
    g_brush_card = CreateSolidBrush(g_pal.card);
    g_brush_dark = CreateSolidBrush(g_pal.log_bg);
    g_brush_log = CreateSolidBrush(g_pal.log_bg);
    g_brush_disabled = CreateSolidBrush(g_pal.disabled);
    g_brush_border = CreateSolidBrush(g_pal.border);
    g_brush_input = CreateSolidBrush(g_pal.input);
}
static HICON g_icono = nullptr;

static std::vector<std::wstring> g_origenes;
static std::vector<std::wstring> g_fallidos_ultimos;
static std::vector<std::wstring> g_rutas_resultados;
static std::thread g_hilo;
static std::atomic<bool> g_corriendo{false};
static std::atomic<bool> g_cancelar{false};
static std::atomic<bool> g_pausa{false};
static ULONGLONG g_inicio_trabajo = 0;
static std::mutex g_mutex_eventos;
static std::deque<Evento> g_eventos;
static std::atomic<bool> g_modo_cli{false};
static std::atomic<bool> g_cli_quiet{false};
static std::mutex g_mutex_consola;
static UINT_PTR g_timer = 0;

static Options g_opt;
static std::wstring g_ini;

// IDs
enum {
    ID_ADD_FILES = 1001, ID_ADD_FOLDER, ID_REMOVE, ID_CLEAR,
    ID_OUTPUT, ID_BROWSE, ID_STRUCT, ID_OVERWRITE, ID_ZIP, ID_ZIPNAME,
    ID_FORMAT, ID_CSV,
    ID_PRESET, ID_SAVE_PRESET, ID_DEL_PRESET, ID_CALITY, ID_METHOD,
    ID_LOSSLESS, ID_REENCODE, ID_DIM, ID_TARGET, ID_TARGET_EDIT,
    ID_START, ID_CANCEL, ID_OPEN, ID_BEEP,
    ID_TAB, ID_RESULTS, ID_LOG,
    ID_MENU_MANUAL, ID_MENU_ABOUT,
    ID_PATRON, ID_DRY, ID_FLATTEN, ID_COLOR, ID_APAGAR,
    ID_MENU_CONTEXT_ON, ID_MENU_CONTEXT_OFF, ID_MENU_SENDTO_ON, ID_MENU_SENDTO_OFF,
    ID_MENU_UPDATE, ID_MENU_WATCH, ID_MENU_PREVIEW,
    ID_ADD_ARCHIVE = 3901, ID_PATTERN_HELP = 3902, ID_SCROLL_ORIGEN = 3904,
    ID_TEMA = 3910, ID_PAUSA = 3911, ID_RETRY = 3912,
    ID_ORIGEN_UP = 3913, ID_ORIGEN_DOWN = 3914, ID_FECHA = 3915,
    ID_SECTION_ORIGEN = 1105, ID_LBL_EXTRACTOR = 1106,
    ID_HIST_BASE = 40000, ID_HIST_CLEAR = 40050
};

// ---------------------------------------------------------------------------
//  Config portable (INI junto al exe)
// ---------------------------------------------------------------------------
static std::wstring ruta_ejecutable() {
    std::wstring buf(MAX_PATH, L'\0');
    for (;;) {
        DWORD n = GetModuleFileNameW(nullptr, buf.data(), (DWORD)buf.size());
        if (n == 0) return {};
        if (n < buf.size()) {
            buf.resize(n);
            return buf;
        }
        buf.resize(buf.size() * 2);
    }
}

static bool escribir_registro(HKEY raiz, const std::wstring& subclave,
                              const wchar_t* nombre, const std::wstring& valor) {
    HKEY clave = nullptr;
    if (RegCreateKeyExW(raiz, subclave.c_str(), 0, nullptr, 0, KEY_WRITE, nullptr,
                        &clave, nullptr) != ERROR_SUCCESS)
        return false;
    LONG rc = RegSetValueExW(clave, nombre, 0, REG_SZ, (const BYTE*)valor.c_str(),
                             (DWORD)((valor.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(clave);
    return rc == ERROR_SUCCESS;
}

static bool instalar_menu_contextual(bool instalar) {
    static const wchar_t* extensiones[] = {
        L".jpg", L".jpeg", L".png", L".bmp", L".gif", L".tif", L".tiff",
        L".ico", L".webp", L".avif", L".heic", L".heif"
    };
    std::wstring exe = ruta_ejecutable();
    std::wstring comando = L"\"" + exe + L"\" \"%1\"";
    bool ok = true;

    for (const wchar_t* ext : extensiones) {
        std::wstring base = std::wstring(L"Software\\Classes\\SystemFileAssociations\\") +
                            ext + L"\\shell\\ConversorWebP";
        if (!instalar) {
            RegDeleteTreeW(HKEY_CURRENT_USER, base.c_str());
            continue;
        }
        ok = escribir_registro(HKEY_CURRENT_USER, base, nullptr, L"Convertir a WebP con Conversor WebP") && ok;
        ok = escribir_registro(HKEY_CURRENT_USER, base, L"Icon", L"\"" + exe + L"\",0") && ok;
        ok = escribir_registro(HKEY_CURRENT_USER, base + L"\\command", nullptr, comando) && ok;
    }

    const wchar_t* carpetas[] = {
        L"Software\\Classes\\Directory\\shell\\ConversorWebP",
        L"Software\\Classes\\Directory\\Background\\shell\\ConversorWebP"
    };
    for (int ci = 0; ci < 2; ci++) {
        const wchar_t* base = carpetas[ci];
        if (!instalar) {
            RegDeleteTreeW(HKEY_CURRENT_USER, base);
            continue;
        }
        ok = escribir_registro(HKEY_CURRENT_USER, base, nullptr, L"Convertir esta carpeta a WebP") && ok;
        ok = escribir_registro(HKEY_CURRENT_USER, base, L"Icon", L"\"" + exe + L"\",0") && ok;
        ok = escribir_registro(HKEY_CURRENT_USER, std::wstring(base) + L"\\command", nullptr,
                               L"\"" + exe + L"\" \"" + (ci == 0 ? L"%1" : L"%V") + L"\"") && ok;
    }
    return ok;
}

static bool instalar_sendto(bool instalar) {
    wchar_t sendto[MAX_PATH] = L"";
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_SENDTO, nullptr, SHGFP_TYPE_CURRENT, sendto)))
        return false;
    fs::path lnk = fs::path(sendto) / L"Conversor WebP.lnk";
    if (!instalar) {
        std::error_code ec;
        fs::remove(lnk, ec);
        return !ec;
    }

    IShellLinkW* link = nullptr;
    if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&link))))
        return false;
    std::wstring exe = ruta_ejecutable();
    link->SetPath(exe.c_str());
    link->SetDescription(L"Convertir imagenes a WebP");
    link->SetIconLocation(exe.c_str(), 0);
    IPersistFile* pf = nullptr;
    bool ok = SUCCEEDED(link->QueryInterface(IID_PPV_ARGS(&pf)));
    if (ok) {
        ok = SUCCEEDED(pf->Save(lnk.wstring().c_str(), TRUE));
        pf->Release();
    }
    link->Release();
    return ok;
}

static std::string http_get(const std::wstring& url, std::wstring& error) {
    URL_COMPONENTS uc{};
    uc.dwStructSize = sizeof(uc);
    uc.dwHostNameLength = (DWORD)-1;
    uc.dwUrlPathLength = (DWORD)-1;
    uc.dwExtraInfoLength = (DWORD)-1;
    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &uc)) {
        error = L"URL no valida";
        return {};
    }
    if (!uc.lpszHostName || !uc.lpszUrlPath ||
        (uc.dwExtraInfoLength && !uc.lpszExtraInfo)) {
        error = L"URL incompleta";
        return {};
    }
    std::wstring host(uc.lpszHostName, uc.dwHostNameLength);
    std::wstring ruta(uc.lpszUrlPath, uc.dwUrlPathLength);
    if (uc.dwExtraInfoLength) ruta.append(uc.lpszExtraInfo, uc.dwExtraInfoLength);

    HINTERNET sesion = WinHttpOpen(L"ConversorWebP-Cpp/2.6",
                                   WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                   WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!sesion) { error = L"WinHttpOpen"; return {}; }
    WinHttpSetTimeouts(sesion, 4000, 4000, 6000, 6000);

    HINTERNET conexion = WinHttpConnect(sesion, host.c_str(), uc.nPort, 0);
    if (!conexion) {
        WinHttpCloseHandle(sesion);
        error = L"No se pudo conectar";
        return {};
    }
    DWORD flags = (uc.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET peticion = WinHttpOpenRequest(conexion, L"GET", ruta.c_str(), nullptr,
                                            WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                            flags);
    if (!peticion) {
        WinHttpCloseHandle(conexion);
        WinHttpCloseHandle(sesion);
        error = L"No se pudo crear la peticion";
        return {};
    }
    WinHttpAddRequestHeaders(peticion, L"User-Agent: ConversorWebP-Cpp", (DWORD)-1,
                             WINHTTP_ADDREQ_FLAG_ADD);
    WinHttpAddRequestHeaders(peticion, L"Accept: application/vnd.github+json", (DWORD)-1,
                             WINHTTP_ADDREQ_FLAG_ADD);

    std::string cuerpo;
    if (!WinHttpSendRequest(peticion, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                            WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(peticion, nullptr)) {
        error = L"Fallo de red al consultar GitHub";
    } else {
        DWORD codigo = 0, tam = sizeof(codigo);
        WinHttpQueryHeaders(peticion, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX, &codigo, &tam,
                            WINHTTP_NO_HEADER_INDEX);
        if (codigo != 200) {
            error = L"GitHub respondio HTTP " + std::to_wstring(codigo);
        } else {
            for (;;) {
                DWORD disponibles = 0;
                if (!WinHttpQueryDataAvailable(peticion, &disponibles) || disponibles == 0)
                    break;
                std::string trozo(disponibles, '\0');
                DWORD leidos = 0;
                if (!WinHttpReadData(peticion, trozo.data(), disponibles, &leidos) || leidos == 0)
                    break;
                trozo.resize(leidos);
                cuerpo += trozo;
                if (cuerpo.size() > 2 * 1024 * 1024) break;
            }
        }
    }
    WinHttpCloseHandle(peticion);
    WinHttpCloseHandle(conexion);
    WinHttpCloseHandle(sesion);
    return cuerpo;
}

static std::vector<int> version_numeros(std::wstring texto) {
    if (!texto.empty() && (texto[0] == L'v' || texto[0] == L'V')) texto.erase(0, 1);
    std::vector<int> partes;
    int actual = -1;
    for (wchar_t c : texto) {
        if (c >= L'0' && c <= L'9') {
            if (actual < 0) actual = 0;
            actual = actual * 10 + (c - L'0');
        } else if (c == L'.') {
            partes.push_back(actual < 0 ? 0 : actual);
            actual = -1;
        } else if (actual >= 0) {
            break;
        }
    }
    if (actual >= 0) partes.push_back(actual);
    while (!partes.empty() && partes.size() > 3) partes.pop_back();
    return partes;
}

static int comparar_versiones(const std::wstring& a, const std::wstring& b) {
    auto va = version_numeros(a), vb = version_numeros(b);
    for (size_t i = 0; i < 3; i++) {
        int x = i < va.size() ? va[i] : 0;
        int y = i < vb.size() ? vb[i] : 0;
        if (x != y) return x < y ? -1 : 1;
    }
    return 0;
}

static std::string extraer_json_texto(const std::string& cuerpo, const std::string& clave) {
    size_t p = cuerpo.find("\"" + clave + "\"");
    if (p == std::string::npos) return {};
    p = cuerpo.find(':', p);
    if (p == std::string::npos) return {};
    p = cuerpo.find('"', p);
    if (p == std::string::npos) return {};
    size_t fin = cuerpo.find('"', p + 1);
    if (fin == std::string::npos) return {};
    std::string valor = cuerpo.substr(p + 1, fin - p - 1);
    std::string limpio;
    for (size_t i = 0; i < valor.size(); i++) {
        if (valor[i] == '\\' && i + 1 < valor.size() && valor[i + 1] == '/') {
            limpio += '/';
            i++;
        } else {
            limpio += valor[i];
        }
    }
    return limpio;
}

static void emitir(Evento e);

static std::atomic<bool> g_update_en_curso{false};
static std::atomic<bool> g_actualizando{false};

static int modo_actualizacion() {
    std::wstring exe = ruta_ejecutable();
    if (exe.empty()) return 3;
    fs::path dir = fs::path(exe).parent_path();
    std::error_code ec;
    if (fs::exists(dir / L"unins000.exe", ec)) return 1;              // instalado con Inno
    if (fs::exists(dir / L"7zr.exe", ec) ||
        fs::exists(dir / L"ConversorWebPC.exe.manifest", ec)) return 2;  // portable
    return 3;                                                          // exe suelto
}

static std::wstring version_sin_v(std::wstring v) {
    if (!v.empty() && (v[0] == L'v' || v[0] == L'V')) v.erase(0, 1);
    return v;
}

static std::wstring asset_para_modo(int modo, const std::wstring& version) {
    if (modo == 1) return L"ConversorWebPC-" + version + L"-setup.exe";
    if (modo == 2) return L"ConversorWebPC-" + version + L"-cpp-portable.zip";
    return L"ConversorWebPC.exe";
}

static bool buscar_asset_json(const std::string& json, const std::string& nombre,
                              std::wstring& url, std::wstring& digest) {
    size_t p = json.find("\"name\": \"" + nombre + "\"");
    if (p == std::string::npos)
        p = json.find("\"name\":\"" + nombre + "\"");
    if (p == std::string::npos) return false;
    size_t siguiente = json.find("\"name\"", p + 8);
    size_t q = json.find("\"browser_download_url\"", p);
    if (q == std::string::npos) return false;
    if (siguiente != std::string::npos && q > siguiente) return false;
    size_t dos_puntos = json.find(':', q);
    if (dos_puntos == std::string::npos) return false;
    size_t c = json.find('"', dos_puntos + 1);
    size_t f = (c == std::string::npos) ? std::string::npos : json.find('"', c + 1);
    if (c == std::string::npos || f == std::string::npos) return false;
    std::string u = json.substr(c + 1, f - c - 1);
    if (u.rfind("https://github.com/", 0) != 0) return false;
    url.assign(u.begin(), u.end());
    size_t d = json.rfind("\"digest\"", q);
    if (d == std::string::npos || d < p)
        d = json.find("\"digest\"", q);
    if (d != std::string::npos && (siguiente == std::string::npos || d < siguiente)) {
        size_t dd = json.find(':', d);
        size_t dc = (dd == std::string::npos) ? std::string::npos : json.find('"', dd + 1);
        size_t df = (dc == std::string::npos) ? std::string::npos : json.find('"', dc + 1);
        if (dc != std::string::npos && df != std::string::npos) {
            std::string dig = json.substr(dc + 1, df - dc - 1);
            if (dig.rfind("sha256:", 0) == 0)
                digest.assign(dig.begin(), dig.end());
        }
    }
    return true;
}

static bool descargar_archivo(const std::wstring& url, const fs::path& destino,
                              std::wstring& error) {
    URL_COMPONENTS uc{};
    uc.dwStructSize = sizeof(uc);
    uc.dwHostNameLength = (DWORD)-1;
    uc.dwUrlPathLength = (DWORD)-1;
    uc.dwExtraInfoLength = (DWORD)-1;
    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &uc)) {
        error = L"URL de descarga no valida";
        return false;
    }
    if (!uc.lpszHostName || !uc.lpszUrlPath ||
        (uc.dwExtraInfoLength && !uc.lpszExtraInfo)) {
        error = L"URL de descarga incompleta";
        return false;
    }
    std::wstring host(uc.lpszHostName, uc.dwHostNameLength);
    std::wstring ruta(uc.lpszUrlPath, uc.dwUrlPathLength);
    if (uc.dwExtraInfoLength) ruta.append(uc.lpszExtraInfo, uc.dwExtraInfoLength);

    struct CerrarHandle {
        HINTERNET h;
        ~CerrarHandle() { if (h) WinHttpCloseHandle(h); }
    };
    std::wstring agente = L"ConversorWebPC/" + std::wstring(APP_VERSION);
    CerrarHandle sesion{WinHttpOpen(agente.c_str(),
                                    WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                    WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0)};
    if (!sesion.h) { error = L"WinHttpOpen"; return false; }
    DWORD politica = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
    WinHttpSetOption(sesion.h, WINHTTP_OPTION_REDIRECT_POLICY, &politica, sizeof(politica));
    WinHttpSetTimeouts(sesion.h, 5000, 5000, 10000, 30000);

    CerrarHandle conexion{WinHttpConnect(sesion.h, host.c_str(), uc.nPort, 0)};
    if (!conexion.h) { error = L"No se pudo conectar con GitHub"; return false; }
    DWORD flags = (uc.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    CerrarHandle peticion{WinHttpOpenRequest(conexion.h, L"GET", ruta.c_str(), nullptr,
                                            WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                            flags)};
    if (!peticion.h) { error = L"No se pudo crear la peticion de descarga"; return false; }
    WinHttpAddRequestHeaders(peticion.h, L"User-Agent: ConversorWebP", (DWORD)-1,
                             WINHTTP_ADDREQ_FLAG_ADD);

    if (!WinHttpSendRequest(peticion.h, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                            WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(peticion.h, nullptr)) {
        error = L"Fallo de red al descargar la actualizacion";
        return false;
    }
    DWORD codigo = 0, tam_codigo = sizeof(codigo);
    WinHttpQueryHeaders(peticion.h, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &codigo, &tam_codigo,
                        WINHTTP_NO_HEADER_INDEX);
    if (codigo != 200) {
        error = L"GitHub respondio HTTP " + std::to_wstring(codigo);
        return false;
    }

    unsigned long long total = 0;
    wchar_t texto_largo[64]{};
    DWORD tam_largo = sizeof(texto_largo);
    if (WinHttpQueryHeaders(peticion.h, WINHTTP_QUERY_CONTENT_LENGTH,
                            WINHTTP_HEADER_NAME_BY_INDEX, texto_largo, &tam_largo,
                            WINHTTP_NO_HEADER_INDEX))
        total = _wcstoui64(texto_largo, nullptr, 10);

    std::ofstream salida(destino, std::ios::binary | std::ios::trunc);
    if (!salida) { error = L"No se pudo crear el archivo temporal"; return false; }

    unsigned long long recibidos = 0;
    int ultimo_pct = -1;
    for (;;) {
        DWORD disponibles = 0;
        if (!WinHttpQueryDataAvailable(peticion.h, &disponibles) || disponibles == 0)
            break;
        std::vector<char> trozo(disponibles);
        DWORD leidos = 0;
        if (!WinHttpReadData(peticion.h, trozo.data(), disponibles, &leidos) || leidos == 0) {
            error = L"Fallo al leer la descarga";
            salida.close();
            return false;
        }
        salida.write(trozo.data(), (std::streamsize)leidos);
        if (!salida) {
            error = L"No se pudo escribir la actualizacion";
            salida.close();
            return false;
        }
        recibidos += leidos;
        int pct = total ? (int)((recibidos * 100) / total) : -1;
        if (pct < 0 || pct == 100 || pct - ultimo_pct >= 2) {
            ultimo_pct = pct;
            Evento ev;
            ev.tipo = Evento::DESCARGA;
            ev.a = pct;
            emitir(std::move(ev));
        }
    }
    salida.close();
    if (!salida) { error = L"No se pudo cerrar el archivo descargado"; return false; }
    if (recibidos == 0) { error = L"La descarga quedo vacia"; return false; }
    return true;
}

static bool sha256_archivo(const fs::path& ruta, std::wstring& hex, std::wstring& error) {
    BCRYPT_ALG_HANDLE alg = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    DWORD objeto_len = 0, hash_len = 0, res = 0;
    std::vector<BYTE> objeto, digest;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0) {
        error = L"No se pudo iniciar SHA-256";
        return false;
    }
    BCryptGetProperty(alg, BCRYPT_OBJECT_LENGTH, (PUCHAR)&objeto_len, sizeof(objeto_len), &res, 0);
    BCryptGetProperty(alg, BCRYPT_HASH_LENGTH, (PUCHAR)&hash_len, sizeof(hash_len), &res, 0);
    objeto.resize(objeto_len);
    digest.resize(hash_len);
    bool ok = BCryptCreateHash(alg, &hash, objeto.data(), objeto_len, nullptr, 0, 0) >= 0;
    if (ok) {
        std::ifstream f(ruta, std::ios::binary);
        if (!f) {
            ok = false;
            error = L"No se pudo abrir la actualizacion descargada";
        } else {
            std::vector<char> buf(1 << 20);
            while (f) {
                f.read(buf.data(), (std::streamsize)buf.size());
                std::streamsize n = f.gcount();
                if (n > 0 && BCryptHashData(hash, (PUCHAR)buf.data(), (ULONG)n, 0) < 0) {
                    ok = false;
                    error = L"Fallo al calcular SHA-256";
                    break;
                }
            }
            if (ok && !f.eof()) {
                ok = false;
                error = L"Fallo al leer la actualizacion";
            }
        }
        if (ok && BCryptFinishHash(hash, digest.data(), hash_len, 0) < 0) {
            ok = false;
            error = L"Fallo al finalizar SHA-256";
        }
        BCryptDestroyHash(hash);
    } else {
        error = L"No se pudo calcular SHA-256";
    }
    BCryptCloseAlgorithmProvider(alg, 0);
    if (!ok) return false;
    hex.clear();
    for (BYTE b : digest) {
        wchar_t par[3];
        swprintf_s(par, L"%02x", b);
        hex += par;
    }
    return true;
}

static int verificar_firma_authenticode(const std::wstring& ruta) {
    WINTRUST_FILE_INFO archivo{};
    archivo.cbStruct = sizeof(archivo);
    archivo.pcwszFilePath = ruta.c_str();
    WINTRUST_DATA datos{};
    datos.cbStruct = sizeof(datos);
    datos.dwUIChoice = WTD_UI_NONE;
    datos.fdwRevocationChecks = WTD_REVOKE_NONE;
    datos.dwUnionChoice = WTD_CHOICE_FILE;
    datos.pFile = &archivo;
    datos.dwStateAction = WTD_STATEACTION_VERIFY;
    GUID politica = WINTRUST_ACTION_GENERIC_VERIFY_V2;
    LONG rc = WinVerifyTrust(nullptr, &politica, &datos);
    datos.dwStateAction = WTD_STATEACTION_CLOSE;
    WinVerifyTrust(nullptr, &politica, &datos);
    if (rc == ERROR_SUCCESS) return 1;       // firmada y valida
    if (rc == TRUST_E_NOSIGNATURE) return 0; // sin firmar
    return -1;                                // firma invalida o manipulada
}

static std::wstring ps_cadena(const std::wstring& s) {
    std::wstring r;
    for (wchar_t c : s) {
        if (c == L'\'') r += L"''";
        else r += c;
    }
    return r;
}

static bool lanzar_actualizador(const std::wstring& archivo, int modo, std::wstring& error) {
    std::wstring exe = ruta_ejecutable();
    if (exe.empty() || !fs::exists(archivo)) {
        error = L"No se encontro el archivo de actualizacion";
        return false;
    }
    fs::path dir_exe = fs::path(exe).parent_path();
    wchar_t temp[MAX_PATH];
    GetTempPathW(MAX_PATH, temp);
    fs::path dir_temp = fs::path(temp) / L"ConversorWebP_update";
    std::error_code ec;
    fs::create_directories(dir_temp, ec);
    fs::path ps1 = dir_temp / (L"actualizar_" + std::to_wstring(GetCurrentProcessId()) + L".ps1");
    fs::path log = dir_temp / L"actualizar.log";

    std::wostringstream ps;
    ps << L"$ErrorActionPreference = 'Stop'\r\n";
    ps << L"$log = '" << ps_cadena(log.wstring()) << L"'\r\n";
    ps << L"function Registrar([string]$m) { Add-Content -LiteralPath $log -Value $m -Encoding UTF8 }\r\n";
    ps << L"try {\r\n";
    ps << L"  $proceso = Get-Process -Id " << GetCurrentProcessId() << L" -ErrorAction SilentlyContinue\r\n";
    ps << L"  if ($proceso) { $proceso.WaitForExit(60000) | Out-Null }\r\n";
    ps << L"  Start-Sleep -Milliseconds 900\r\n";
    if (modo == 1) {
        ps << L"  $inst = Start-Process -FilePath '" << ps_cadena(archivo) << L"' "
              L"-ArgumentList @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART',"
              L"'/CLOSEAPPLICATIONS','/NOCANCEL') -Wait -PassThru\r\n";
        ps << L"  if ($inst.ExitCode -ne 0) { throw ('El instalador termino con codigo ' + $inst.ExitCode) }\r\n";
    } else if (modo == 2) {
        ps << L"  $stage = Join-Path $env:TEMP ('ConversorWebP_stage_' + [guid]::NewGuid().ToString('N'))\r\n";
        ps << L"  if (Test-Path -LiteralPath $stage) { Remove-Item -LiteralPath $stage -Recurse -Force }\r\n";
        ps << L"  Add-Type -AssemblyName System.IO.Compression.FileSystem\r\n";
        ps << L"  [System.IO.Compression.ZipFile]::ExtractToDirectory('"
           << ps_cadena(archivo) << L"', $stage)\r\n";
        ps << L"  $origen = $stage\r\n";
        ps << L"  $hijos = @(Get-ChildItem -LiteralPath $stage -Force)\r\n";
        ps << L"  if ($hijos.Count -eq 1 -and $hijos[0].PSIsContainer) { $origen = $hijos[0].FullName }\r\n";
        ps << L"  Copy-Item -Path (Join-Path $origen '*') -Destination '"
           << ps_cadena(dir_exe.wstring()) << L"' -Recurse -Force\r\n";
        ps << L"  Remove-Item -LiteralPath $stage -Recurse -Force -ErrorAction SilentlyContinue\r\n";
    } else {
        ps << L"  Copy-Item -LiteralPath '" << ps_cadena(archivo) << L"' -Destination '"
           << ps_cadena(exe) << L"' -Force\r\n";
    }
    ps << L"  Start-Process -FilePath '" << ps_cadena(exe) << L"' -WorkingDirectory '"
       << ps_cadena(dir_exe.wstring()) << L"'\r\n";
    ps << L"  Registrar 'ok'\r\n";
    ps << L"} catch { Registrar ('ERROR: ' + $_.Exception.Message); exit 1 } "
          L"finally { Remove-Item -LiteralPath $PSCommandPath -Force -ErrorAction SilentlyContinue }\r\n";

    std::ofstream f(ps1, std::ios::binary | std::ios::trunc);
    if (!f) { error = L"No se pudo crear el script de actualizacion"; return false; }
    const char bom[] = "\xEF\xBB\xBF";
    f.write(bom, 3);
    std::string contenido = a_utf8(ps.str());
    f.write(contenido.data(), (std::streamsize)contenido.size());
    f.close();
    if (!f) { error = L"No se pudo escribir el script de actualizacion"; return false; }

    std::wstring cmd = L"powershell.exe -NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File \"" +
                       ps1.wstring() + L"\"";
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    if (!CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
                        nullptr, nullptr, &si, &pi)) {
        error = L"No se pudo iniciar el actualizador";
        return false;
    }
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return true;
}

static void iniciar_descarga_actualizacion(const Evento& info) {
    if (g_actualizando.exchange(true)) return;
    SetWindowTextW(g_status, L"Descargando actualizacion...");
    SendMessageW(g_progress, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
    SendMessageW(g_progress, PBM_SETPOS, 0, 0);
    EnableWindow(g_btn_convertir, FALSE);
    std::wstring url = info.extra;
    std::wstring asset = info.extra2;
    std::wstring digest = info.extra3;
    int modo = info.modo;
    try {
        std::thread([url, asset, digest, modo] {
            try {
                wchar_t temp[MAX_PATH];
                GetTempPathW(MAX_PATH, temp);
                fs::path destino = fs::path(temp) / L"ConversorWebP_update" / asset;
                std::error_code ec;
                fs::create_directories(destino.parent_path(), ec);
                std::wstring error;
                bool ok = descargar_archivo(url, destino, error);
                if (ok) {
                    std::wstring hex;
                    ok = sha256_archivo(destino, hex, error);
                    if (ok) {
                        std::wstring esperado = digest;
                        if (esperado.rfind(L"sha256:", 0) == 0) esperado = esperado.substr(7);
                        if (esperado.size() != 64 || _wcsicmp(hex.c_str(), esperado.c_str()) != 0) {
                            ok = false;
                            error = L"El archivo descargado no coincide con su SHA-256.";
                        }
                    }
                }
                Evento ev;
                ev.tipo = ok ? Evento::DESCARGA_LISTA : Evento::DESCARGA_ERROR;
                ev.texto = error;
                ev.extra = destino.wstring();
                ev.extra2 = asset;
                ev.modo = modo;
                emitir(std::move(ev));
            } catch (const std::exception& ex) {
                Evento ev;
                ev.tipo = Evento::DESCARGA_ERROR;
                ev.texto = L"Error inesperado al descargar: " + a_wstring(ex.what());
                emitir(std::move(ev));
            } catch (...) {
                Evento ev;
                ev.tipo = Evento::DESCARGA_ERROR;
                ev.texto = L"Error inesperado al descargar la actualizacion.";
                emitir(std::move(ev));
            }
        }).detach();
    } catch (...) {
        g_actualizando = false;
        EnableWindow(g_btn_convertir, TRUE);
        SetWindowTextW(g_status, L"No se pudo iniciar la descarga.");
    }
}

static void comprobar_actualizaciones(bool manual = false) {
    if (g_update_en_curso.exchange(true)) return;
    int modo = modo_actualizacion();
    SetWindowTextW(g_status, L"Buscando actualizaciones...");
    try {
        std::thread([manual, modo] {
            Evento e;
            e.tipo = Evento::ACTUALIZACION;
            e.a = manual ? 1 : 0;
            e.modo = modo;
            try {
                std::wstring error;
                std::string cuerpo = http_get(
                    L"https://api.github.com/repos/zeronet75/ConversorWebP/releases/latest",
                    error);
                if (cuerpo.empty()) {
                    e.b = 2;
                    e.texto = L"No se pudo comprobar si hay actualizaciones.";
                    if (!error.empty()) e.texto += L"\n\n" + error;
                } else {
                    std::string etiqueta = extraer_json_texto(cuerpo, "tag_name");
                    std::wstring wetiqueta(etiqueta.begin(), etiqueta.end());
                    if (wetiqueta.empty()) {
                        e.b = 2;
                        e.texto = L"La respuesta de GitHub no incluye una version.";
                    } else if (comparar_versiones(wetiqueta, APP_VERSION) > 0) {
                        std::wstring version = version_sin_v(wetiqueta);
                        std::wstring asset = asset_para_modo(modo, version);
                        std::wstring url, digest;
                        if (!buscar_asset_json(cuerpo, a_utf8(asset), url, digest) ||
                            digest.empty()) {
                            e.b = 2;
                            e.texto = L"No se encontro el archivo de actualizacion adecuado "
                                      L"para esta instalacion.";
                        } else {
                            e.b = 1;
                            e.texto = L"Hay una version nueva: " + wetiqueta +
                                      L"\n\nLa tuya es " + APP_VERSION +
                                      L"\n\n¿Descargar e instalar ahora?\n"
                                      L"La aplicacion se cerrara y volvera a abrirse automaticamente.";
                            e.extra = url;
                            e.extra2 = asset;
                            e.extra3 = digest;
                        }
                    } else {
                        e.b = 0;
                        e.texto = L"Estas al dia.\n\nVersion instalada: " +
                                  std::wstring(APP_VERSION);
                    }
                }
            } catch (...) {
                e.b = 2;
                e.texto = L"No se pudo comprobar si hay actualizaciones.";
            }
            emitir(std::move(e));
        }).detach();
    } catch (...) {
        g_update_en_curso = false;
        SetWindowTextW(g_status, L"No se pudo iniciar la comprobacion.");
    }
}

static std::vector<std::wstring> dividir(const std::wstring& texto, wchar_t sep) {    std::vector<std::wstring> partes;
    std::wstring actual;
    for (wchar_t c : texto) {
        if (c == sep) {
            if (!actual.empty()) partes.push_back(actual);
            actual.clear();
        } else {
            actual += c;
        }
    }
    if (!actual.empty()) partes.push_back(actual);
    return partes;
}

static void cargar_config() {
    std::wstring ruta = ruta_ejecutable();
    fs::path p = ruta.empty() ? fs::current_path() : fs::path(ruta);
    g_ini = (p.parent_path() / L"config.ini").wstring();
    g_oscuro = GetPrivateProfileIntW(L"opciones", L"tema", 0, g_ini.c_str()) != 0;

    wchar_t salida[MAX_PATH] = L"";
    GetPrivateProfileStringW(L"opciones", L"salida", L"", salida, MAX_PATH, g_ini.c_str());
    g_opt.salida = salida;
    g_opt.calidad = GetPrivateProfileIntW(L"opciones", L"calidad", 85, g_ini.c_str());
    g_opt.metodo = GetPrivateProfileIntW(L"opciones", L"metodo", 6, g_ini.c_str());
    g_opt.sin_perdida = GetPrivateProfileIntW(L"opciones", L"lossless", 0, g_ini.c_str()) != 0;
    g_opt.max_dim = GetPrivateProfileIntW(L"opciones", L"maxdim", 0, g_ini.c_str());
    g_opt.objetivo_kb = GetPrivateProfileIntW(L"opciones", L"objetivo", 0, g_ini.c_str());
    g_opt.mantener = GetPrivateProfileIntW(L"opciones", L"mantener", 1, g_ini.c_str()) != 0;
    g_opt.sobrescribir = GetPrivateProfileIntW(L"opciones", L"sobrescribir", 1, g_ini.c_str()) != 0;
    g_opt.zip = GetPrivateProfileIntW(L"opciones", L"zip", 1, g_ini.c_str()) != 0;
    g_opt.beep = GetPrivateProfileIntW(L"opciones", L"beep", 1, g_ini.c_str()) != 0;
    g_opt.csv = GetPrivateProfileIntW(L"opciones", L"csv", 0, g_ini.c_str()) != 0;
    g_opt.dry_run = GetPrivateProfileIntW(L"opciones", L"dry", 0, g_ini.c_str()) != 0;
    g_opt.flatten = GetPrivateProfileIntW(L"opciones", L"flatten", 0, g_ini.c_str()) != 0;
    g_opt.color = (COLORREF)GetPrivateProfileIntW(L"opciones", L"color", RGB(255, 255, 255), g_ini.c_str());
    g_opt.apagar = GetPrivateProfileIntW(L"opciones", L"apagar", 0, g_ini.c_str()) != 0;
    g_opt.preservar_fecha = GetPrivateProfileIntW(L"opciones", L"fecha", 0, g_ini.c_str()) != 0;
    std::wstring zn_defecto = L"webp_" + fecha_zip() + L".zip";
    wchar_t zn[MAX_PATH];
    GetPrivateProfileStringW(L"opciones", L"zipnombre", zn_defecto.c_str(), zn, MAX_PATH,
                             g_ini.c_str());
    g_opt.zip_nombre = zn;
    wchar_t patron[1024] = L"";
    GetPrivateProfileStringW(L"opciones", L"patron", L"", patron, 1024, g_ini.c_str());
    g_opt.patron = patron;
    wchar_t formato[16] = L"WEBP";
    GetPrivateProfileStringW(L"opciones", L"formato", L"WEBP", formato, 16, g_ini.c_str());
    g_opt.formato = (_wcsicmp(formato, L"AVIF") == 0) ? L"AVIF" : L"WEBP";

    // Presets propios
    inicializar_presets();
    int np = std::clamp((int)GetPrivateProfileIntW(L"presets", L"count", 0, g_ini.c_str()), 0, 64);
    for (int i = 0; i < np; i++) {
        wchar_t clave[64];
        wchar_t nombre[256] = L"";
        wchar_t datos[4096] = L"";
        swprintf_s(clave, L"p%dnombre", i);
        GetPrivateProfileStringW(L"presets", clave, L"", nombre, 256, g_ini.c_str());
        swprintf_s(clave, L"p%ddatos", i);
        GetPrivateProfileStringW(L"presets", clave, L"", datos, 4096, g_ini.c_str());
        if (wcslen(nombre) > 0)
            g_presets.push_back({nombre, deserializar_opciones(datos), true});
    }

    // Historial
    g_historial.clear();
    int nh = std::clamp((int)GetPrivateProfileIntW(L"historial", L"count", 0, g_ini.c_str()), 0, 10);
    for (int i = 0; i < nh; i++) {
        wchar_t clave[64];
        wchar_t fecha[64] = L"";
        wchar_t origenes[8192] = L"";
        wchar_t salida_h[1024] = L"";
        wchar_t datos[4096] = L"";
        swprintf_s(clave, L"h%dfecha", i);
        GetPrivateProfileStringW(L"historial", clave, L"", fecha, 64, g_ini.c_str());
        swprintf_s(clave, L"h%dorigenes", i);
        GetPrivateProfileStringW(L"historial", clave, L"", origenes, 8192, g_ini.c_str());
        swprintf_s(clave, L"h%dsalida", i);
        GetPrivateProfileStringW(L"historial", clave, L"", salida_h, 1024, g_ini.c_str());
        swprintf_s(clave, L"h%ddatos", i);
        GetPrivateProfileStringW(L"historial", clave, L"", datos, 4096, g_ini.c_str());
        swprintf_s(clave, L"h%dconv", i);
        int conv = GetPrivateProfileIntW(L"historial", clave, 0, g_ini.c_str());
        HistoryEntry h;
        h.fecha = fecha;
        h.origenes = dividir(origenes, L'|');
        h.salida = salida_h;
        h.opt = deserializar_opciones(datos);
        h.convertidas = conv;
        if (!h.origenes.empty()) g_historial.push_back(h);
    }
    normalizar_opciones(g_opt);
}

static void guardar_config() {
    if (g_ini.empty()) return;
    std::wstring destino = g_ini + L".tmp";
    wchar_t buf[32];
    swprintf_s(buf, L"%d", g_opt.calidad);
    WritePrivateProfileStringW(L"opciones", L"calidad", buf, destino.c_str());
    swprintf_s(buf, L"%d", g_opt.metodo);
    WritePrivateProfileStringW(L"opciones", L"metodo", buf, destino.c_str());
    WritePrivateProfileStringW(L"opciones", L"lossless", g_opt.sin_perdida ? L"1" : L"0", destino.c_str());
    swprintf_s(buf, L"%d", g_opt.max_dim);
    WritePrivateProfileStringW(L"opciones", L"maxdim", buf, destino.c_str());
    swprintf_s(buf, L"%d", g_opt.objetivo_kb);
    WritePrivateProfileStringW(L"opciones", L"objetivo", buf, destino.c_str());
    WritePrivateProfileStringW(L"opciones", L"mantener", g_opt.mantener ? L"1" : L"0", destino.c_str());
    WritePrivateProfileStringW(L"opciones", L"sobrescribir", g_opt.sobrescribir ? L"1" : L"0", destino.c_str());
    WritePrivateProfileStringW(L"opciones", L"zip", g_opt.zip ? L"1" : L"0", destino.c_str());
    WritePrivateProfileStringW(L"opciones", L"beep", g_opt.beep ? L"1" : L"0", destino.c_str());
    WritePrivateProfileStringW(L"opciones", L"csv", g_opt.csv ? L"1" : L"0", destino.c_str());
    WritePrivateProfileStringW(L"opciones", L"dry", g_opt.dry_run ? L"1" : L"0", destino.c_str());
    WritePrivateProfileStringW(L"opciones", L"flatten", g_opt.flatten ? L"1" : L"0", destino.c_str());
    swprintf_s(buf, L"%lu", (unsigned long)g_opt.color);
    WritePrivateProfileStringW(L"opciones", L"color", buf, destino.c_str());
    WritePrivateProfileStringW(L"opciones", L"apagar", g_opt.apagar ? L"1" : L"0", destino.c_str());
    WritePrivateProfileStringW(L"opciones", L"fecha", g_opt.preservar_fecha ? L"1" : L"0", destino.c_str());
    WritePrivateProfileStringW(L"opciones", L"tema", g_oscuro ? L"1" : L"0", destino.c_str());
    WritePrivateProfileStringW(L"opciones", L"salida", g_opt.salida.c_str(), destino.c_str());
    WritePrivateProfileStringW(L"opciones", L"zipnombre", g_opt.zip_nombre.c_str(), destino.c_str());
    WritePrivateProfileStringW(L"opciones", L"patron", g_opt.patron.c_str(), destino.c_str());
    WritePrivateProfileStringW(L"opciones", L"formato", g_opt.formato.c_str(), destino.c_str());

    WritePrivateProfileStringW(L"presets", nullptr, nullptr, destino.c_str());
    int idx = 0;
    for (auto& p : g_presets) {
        if (!p.propio) continue;
        wchar_t clave[64];
        swprintf_s(clave, L"p%d", idx);
        std::wstring kname = std::wstring(clave) + L"nombre";
        std::wstring kdatos = std::wstring(clave) + L"datos";
        WritePrivateProfileStringW(L"presets", kname.c_str(), p.nombre.c_str(), destino.c_str());
        WritePrivateProfileStringW(L"presets", kdatos.c_str(),
                                   serializar_opciones(p.opt).c_str(), destino.c_str());
        idx++;
    }
    swprintf_s(buf, L"%d", idx);
    WritePrivateProfileStringW(L"presets", L"count", buf, destino.c_str());

    WritePrivateProfileStringW(L"historial", nullptr, nullptr, destino.c_str());
    for (size_t i = 0; i < g_historial.size(); i++) {
        auto& h = g_historial[i];
        wchar_t clave[64];
        swprintf_s(clave, L"h%d", (int)i);
        std::wstring base(clave);
        std::wstring origenes;
        for (size_t j = 0; j < h.origenes.size(); j++) {
            if (j) origenes += L"|";
            origenes += h.origenes[j];
        }
        wchar_t conv[32];
        swprintf_s(conv, L"%d", h.convertidas);
        WritePrivateProfileStringW(L"historial", (base + L"fecha").c_str(), h.fecha.c_str(), destino.c_str());
        WritePrivateProfileStringW(L"historial", (base + L"origenes").c_str(), origenes.c_str(), destino.c_str());
        WritePrivateProfileStringW(L"historial", (base + L"salida").c_str(), h.salida.c_str(), destino.c_str());
        WritePrivateProfileStringW(L"historial", (base + L"datos").c_str(),
                                   serializar_opciones(h.opt).c_str(), destino.c_str());
        WritePrivateProfileStringW(L"historial", (base + L"conv").c_str(), conv, destino.c_str());
    }
    swprintf_s(buf, L"%d", (int)g_historial.size());
    WritePrivateProfileStringW(L"historial", L"count", buf, destino.c_str());
    MoveFileExW(destino.c_str(), g_ini.c_str(), MOVEFILE_REPLACE_EXISTING);
}

// ---------------------------------------------------------------------------
//  UI: helpers
// ---------------------------------------------------------------------------
static HWND crear(const wchar_t* clase, const wchar_t* texto, DWORD estilo, int x, int y,
                  int w, int h, int id, DWORD ex = 0) {
    HWND hwnd = CreateWindowExW(ex, clase, texto, WS_CHILD | WS_VISIBLE | estilo, x, y, w, h,
                                g_hwnd, (HMENU)(INT_PTR)id, GetModuleHandleW(nullptr), nullptr);
    if (hwnd && g_font) SendMessageW(hwnd, WM_SETFONT, (WPARAM)g_font, TRUE);
    return hwnd;
}

static HWND boton(const wchar_t* texto, int x, int y, int w, int h, int id, DWORD estilo = BS_PUSHBUTTON) {
    return crear(L"BUTTON", texto, estilo | BS_OWNERDRAW | WS_TABSTOP, x, y, w, h, id);
}
static HWND check(const wchar_t* texto, int x, int y, int w, int h, int id, bool marcado) {
    HWND control = crear(L"BUTTON", texto, BS_AUTOCHECKBOX | WS_TABSTOP, x, y, w, h, id);
    SendMessageW(control, BM_SETCHECK, marcado ? BST_CHECKED : BST_UNCHECKED, 0);
    return control;
}
static HWND etiqueta(const wchar_t* texto, int x, int y, int w, int h, int id, HFONT f = nullptr, DWORD estilo = SS_LEFT) {
    HWND control = crear(L"STATIC", texto, estilo, x, y, w, h, id);
    if (control && f) SendMessageW(control, WM_SETFONT, (WPARAM)f, TRUE);
    return control;
}
static HWND edit(int x, int y, int w, int h, int id, const wchar_t* texto = L"", DWORD estilo = ES_AUTOHSCROLL) {
    return crear(L"EDIT", texto, estilo | WS_BORDER | WS_TABSTOP, x, y, w, h, id, WS_EX_CLIENTEDGE);
}
static HWND combo(int x, int y, int w, int h, int id) {
    HWND control = crear(L"COMBOBOX", L"",
                         CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS |
                             WS_VSCROLL | WS_TABSTOP,
                         x, y, w, h, id);
    if (control) {
        SendMessageW(control, CB_SETITEMHEIGHT, (WPARAM)-1, 22);
        SendMessageW(control, CB_SETITEMHEIGHT, 0, 22);
    }
    return control;
}
static HWND listview(int x, int y, int w, int h, int id) {
    return crear(WC_LISTVIEWW, L"", LVS_REPORT | LVS_SHOWSELALWAYS | WS_BORDER | WS_VSCROLL | WS_TABSTOP, x, y, w, h, id, WS_EX_CLIENTEDGE);
}

static std::wstring texto_ventana(HWND h) {
    int n = GetWindowTextLengthW(h) + 1;
    std::wstring s(n, L'\0');
    GetWindowTextW(h, s.data(), n);
    s.resize(wcslen(s.c_str()));
    return s;
}

static void anadir_lista(HWND lista, const std::wstring& texto) {
    LVITEMW item{};
    item.mask = LVIF_TEXT;
    item.iItem = ListView_GetItemCount(lista);
    item.pszText = const_cast<wchar_t*>(texto.c_str());
    ListView_InsertItem(lista, &item);
}

static std::wstring buscar_extractor();
static void guardar_cola();
static void borrar_cola();

static void refrescar_origenes() {
    ListView_DeleteAllItems(g_lista_origen);
    for (auto& s : g_origenes) anadir_lista(g_lista_origen, s);
    if (g_origenes.empty()) {
        SetWindowTextW(g_lbl_count, L"Sin origen seleccionado");
    } else {
        std::wstring nombres;
        for (size_t i = 0; i < g_origenes.size() && i < 2; i++) {
            if (i) nombres += L", ";
            nombres += fs::path(g_origenes[i]).filename().wstring();
        }
        if (g_origenes.size() > 2)
            nombres += L" +" + std::to_wstring(g_origenes.size() - 2);
        SetWindowTextW(g_lbl_count, (L"Origen: " + nombres).c_str());
    }
    if (g_lbl_extractor) {
        std::wstring herramienta = buscar_extractor();
        if (herramienta.empty()) {
            SetWindowTextW(g_lbl_extractor, L"Sin 7-Zip/WinRAR: solo .zip");
        } else {
            SetWindowTextW(g_lbl_extractor,
                           (L"Extractor: " + fs::path(herramienta).filename().wstring()).c_str());
        }
    }
    if (g_scroll_origen) {
        SCROLLINFO si{};
        si.cbSize = sizeof(si);
        si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
        si.nMin = 0;
        if (g_origenes.empty()) {
            si.nMax = 100;
            si.nPage = 20;
        } else {
            si.nMax = std::max(0, (int)g_origenes.size() - 1);
            si.nPage = 4;
        }
        si.nPos = 0;
        SetScrollInfo(g_scroll_origen, SB_CTL, &si, TRUE);
        InvalidateRect(g_scroll_origen, nullptr, FALSE);
    }
}

static void anadir_origen(const std::wstring& ruta) {
    if (ruta.empty()) return;
    for (auto& s : g_origenes) if (_wcsicmp(s.c_str(), ruta.c_str()) == 0) return;
    g_origenes.push_back(ruta);
    refrescar_origenes();
    std::error_code ec_cola;
    if (fs::exists(fs::path(g_ini).parent_path() / L"queue.ini", ec_cola))
        guardar_cola();
    if (texto_ventana(g_edit_salida).empty()) {
        fs::path p(ruta);
        fs::path base = fs::is_directory(p) ? p : p.parent_path();
        std::wstring nombre = fs::is_directory(p) ? p.filename().wstring() : p.stem().wstring();
        if (nombre.empty()) nombre = L"salida";
        std::wstring salida = (base / (nombre + L"_webp")).wstring();
        SetWindowTextW(g_edit_salida, salida.c_str());
    }
}

static void anadir_fila_resultado(const Row& r) {
    int i = ListView_GetItemCount(g_lista_resultados);
    std::wstring kb_in = r.kb_in > 0 ? fmt_kb(r.kb_in).substr(0, fmt_kb(r.kb_in).size() - 3) : L"";
    std::wstring kb_out = r.kb_out > 0 ? fmt_kb(r.kb_out).substr(0, fmt_kb(r.kb_out).size() - 3) : L"";
    wchar_t ahorro[32] = L"";
    if (r.ahorro != 0) swprintf_s(ahorro, L"%.1f", r.ahorro);
    LVITEMW item{};
    item.mask = LVIF_TEXT;
    item.iItem = i;
    item.pszText = const_cast<wchar_t*>(r.archivo.c_str());
    ListView_InsertItem(g_lista_resultados, &item);
    ListView_SetItemText(g_lista_resultados, i, 1, const_cast<wchar_t*>(kb_in.c_str()));
    ListView_SetItemText(g_lista_resultados, i, 2, const_cast<wchar_t*>(kb_out.c_str()));
    ListView_SetItemText(g_lista_resultados, i, 3, ahorro);
    ListView_SetItemText(g_lista_resultados, i, 4, const_cast<wchar_t*>(r.estado.c_str()));
    if (!r.ruta.empty()) {
        if ((int)g_rutas_resultados.size() <= i) g_rutas_resultados.resize(i + 1);
        g_rutas_resultados[i] = r.ruta;
    }
}

static void log(const std::wstring& texto) {
    int len = GetWindowTextLengthW(g_edit_log);
    SendMessageW(g_edit_log, EM_SETSEL, len, len);
    SendMessageW(g_edit_log, EM_REPLACESEL, FALSE, (LPARAM)(texto + L"\r\n").c_str());
    SendMessageW(g_edit_log, EM_SCROLLCARET, 0, 0);
}

static void emitir(Evento e) {
    if (g_modo_cli) {
        if (e.tipo == Evento::LOG) {
            if (!g_cli_quiet) {
                std::lock_guard<std::mutex> lock(g_mutex_consola);
                wprintf(L"%s\n", e.texto.c_str());
                fflush(stdout);
            }
            return;
        }
        if (e.tipo == Evento::PROGRESO || e.tipo == Evento::ESTADO || e.tipo == Evento::FILA)
            return;
    }
    std::lock_guard<std::mutex> lock(g_mutex_eventos);
    g_eventos.push_back(std::move(e));
}

static void emitir_log(const std::wstring& t) { Evento e; e.tipo = Evento::LOG; e.texto = t; emitir(std::move(e)); }

// ---------------------------------------------------------------------------
//  Conversion
// ---------------------------------------------------------------------------
struct Tarea {
    fs::path src;
    fs::path rel;
    fs::path dst;
};

static bool es_comprimido(const fs::path& p) {
    std::wstring ext = ext_lower(p);
    return ext == L".zip" || ext == L".rar" || ext == L".7z";
}

static std::mutex g_mutex_temporales;
static std::vector<fs::path> g_temporales;

static void limpiar_temporales() {
    std::lock_guard<std::mutex> lock(g_mutex_temporales);
    for (auto& p : g_temporales) {
        std::error_code ec;
        fs::remove_all(p, ec);
    }
    g_temporales.clear();
}

static bool reemplazar_archivo(const fs::path& parcial, const fs::path& destino) {
    if (MoveFileExW(parcial.c_str(), destino.c_str(), MOVEFILE_REPLACE_EXISTING))
        return true;
    std::error_code ec;
    fs::remove(parcial, ec);
    return false;
}

static void copiar_fecha(const fs::path& origen, const fs::path& destino) {
    std::error_code ec;
    auto fecha = fs::last_write_time(origen, ec);
    if (!ec) fs::last_write_time(destino, fecha, ec);
}

static bool ejecutar_oculto(const std::wstring& comando) {
    std::wstring cmd = comando;
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    if (!CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
                        nullptr, nullptr, &si, &pi))
        return false;
    DWORD espera = WaitForSingleObject(pi.hProcess, 120000);
    if (espera == WAIT_TIMEOUT) TerminateProcess(pi.hProcess, 1);
    DWORD codigo = 1;
    GetExitCodeProcess(pi.hProcess, &codigo);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return espera != WAIT_TIMEOUT && codigo == 0;
}

static std::wstring buscar_extractor() {
    const wchar_t* candidatos[] = {
        L"7z.exe", L"7za.exe",
        L"C:\\Program Files\\7-Zip\\7z.exe",
        L"C:\\Program Files (x86)\\7-Zip\\7z.exe",
        L"C:\\Program Files\\WinRAR\\UnRAR.exe",
        L"C:\\Program Files\\WinRAR\\WinRAR.exe",
        L"C:\\Program Files (x86)\\WinRAR\\UnRAR.exe"
    };
    for (const wchar_t* c : candidatos) {
        wchar_t encontrado[MAX_PATH];
        DWORD n = SearchPathW(nullptr, c, nullptr, MAX_PATH, encontrado, nullptr);
        if (n > 0 && n < MAX_PATH) return encontrado;
        if (fs::exists(c)) return c;
    }
    // 7zr incluido en las herramientas de compilacion (util para .7z)
    std::wstring exe = ruta_ejecutable();
    if (!exe.empty()) {
        fs::path local = fs::path(exe).parent_path() / L"7zr.exe";
        if (fs::exists(local)) return local.wstring();
    }
    return L"";
}

static fs::path extraer_comprimido(const fs::path& archivo, int indice,
                                   const std::function<void(const std::wstring&)>& logfn) {
    wchar_t temp[MAX_PATH];
    GetTempPathW(MAX_PATH, temp);
    fs::path destino = fs::path(temp) / L"ConversorWebP_extraccion" /
                       (std::to_wstring(GetCurrentProcessId()) + L"_" +
                        std::to_wstring(indice) + L"_" + archivo.stem().wstring());
    std::error_code ec;
    fs::remove_all(destino, ec);
    fs::create_directories(destino, ec);
    if (ec) return {};

    logfn(L"Extrayendo " + archivo.filename().wstring() + L" ...");
    bool ok = false;
    std::wstring ext = ext_lower(archivo);
    if (ext == L".zip") {
        ok = ejecutar_oculto(L"tar.exe -xf \"" + archivo.wstring() + L"\" -C \"" +
                             destino.wstring() + L"\"");
    }
    if (!ok) {
        std::wstring herramienta = buscar_extractor();
        if (!herramienta.empty()) {
            std::wstring nombre = lower(fs::path(herramienta).filename().wstring());
            if (nombre == L"unrar.exe")
                ok = ejecutar_oculto(L"\"" + herramienta + L"\" x -y \"" +
                                     archivo.wstring() + L"\" \"" + destino.wstring() + L"\\\"");
            else if (nombre == L"winrar.exe")
                ok = ejecutar_oculto(L"\"" + herramienta + L"\" x -ibck -y \"" +
                                     archivo.wstring() + L"\" \"" + destino.wstring() + L"\\\"");
            else
                ok = ejecutar_oculto(L"\"" + herramienta + L"\" x \"" +
                                     archivo.wstring() + L"\" -o\"" + destino.wstring() + L"\" -y");
        }
    }
    if (!ok && ext == L".zip") {
        std::wstring ruta_ps = archivo.wstring();
        std::wstring destino_ps = destino.wstring();
        for (auto* texto : {&ruta_ps, &destino_ps}) {
            for (size_t pos = texto->find(L'\''); pos != std::wstring::npos;
                 pos = texto->find(L'\'', pos + 2)) {
                texto->insert(pos, 1, L'\'');
            }
        }
        ok = ejecutar_oculto(
            L"powershell.exe -NoProfile -ExecutionPolicy Bypass -Command \"Expand-Archive -LiteralPath '" +
            ruta_ps + L"' -DestinationPath '" + destino_ps + L"' -Force\"");
    }
    if (!ok) {
        fs::remove_all(destino, ec);
        logfn(L"FALLO al extraer " + archivo.filename().wstring() +
              L" (instala 7-Zip o WinRAR para .rar/.7z)");
        return {};
    }

    bool hay_imagen = false;
    for (fs::recursive_directory_iterator it(destino,
             fs::directory_options::skip_permission_denied, ec), fin;
         it != fin; it.increment(ec)) {
        if (ec) { ec.clear(); continue; }
        std::error_code ec2;
        if (it->is_regular_file(ec2) && es_imagen(it->path())) { hay_imagen = true; break; }
    }
    if (!hay_imagen) {
        fs::remove_all(destino, ec);
        logfn(L"El comprimido " + archivo.filename().wstring() + L" no contiene imagenes.");
        return {};
    }
    {
        std::lock_guard<std::mutex> lock(g_mutex_temporales);
        g_temporales.push_back(destino);
    }
    return destino;
}

static bool preparar_tareas(IWICImagingFactory* factory, const Options& opt,
                            const std::vector<std::wstring>& origenes,
                            std::vector<Tarea>& tareas,
                            int& omitidas, uint64_t& total_bytes,
                            const std::function<void(const std::wstring&)>& logfn) {
    std::set<std::wstring> usados;
    std::set<std::wstring> vistos;
    auto anadir = [&](const fs::path& p, const fs::path& rel) {
        if (!es_imagen(p)) return;
        std::wstring ext_destino = opt.formato == L"AVIF" ? L".avif" : L".webp";
        if (ext_lower(p) == ext_destino && !opt.reencode) return;
        std::error_code ec_abs;
        std::wstring clave_src = lower(fs::absolute(p, ec_abs).wstring());
        if (vistos.count(clave_src)) return;
        vistos.insert(clave_src);
        fs::path rel_final = opt.mantener ? rel : rel.filename();
        std::wstring rel_patron = aplicar_patron(opt.patron, factory, p, rel_final,
                                                 opt.max_dim, opt.calidad, opt.formato);
        fs::path dst = fs::path(opt.salida) / rel_patron;
        if (lower(dst.extension().wstring()) != ext_destino) dst.replace_extension(ext_destino);
        std::wstring clave = lower(dst.wstring());
        if (usados.count(clave)) {
            fs::path base_dst = dst.parent_path() /
                                (dst.stem().wstring() + L"_" + ext_lower(p).substr(1) + ext_destino);
            dst = base_dst;
            clave = lower(dst.wstring());
            int intento = 1;
            while (usados.count(clave) && intento < 1000) {
                ++intento;
                dst = base_dst.parent_path() /
                      (base_dst.stem().wstring() + L"_" + std::to_wstring(intento) + ext_destino);
                clave = lower(dst.wstring());
            }
        }
        usados.insert(clave);
        try { total_bytes += fs::file_size(p); } catch (...) {}
        std::error_code ec_existe;
        if (!opt.dry_run && !opt.sobrescribir && fs::exists(dst, ec_existe)) {
            omitidas++;
            logfn(L"OMITIDA  " + rel_final.wstring() + L"  (ya existe)");
            return;
        }
        tareas.push_back({p, rel_final, dst});
    };

    int n_comprimidos = 0;
    for (auto& s : origenes)
        if (es_comprimido(fs::path(s))) n_comprimidos++;
    bool prefijar_comprimidos = n_comprimidos > 1;
    int indice_comprimido = 0;

    for (auto& s : origenes) {
        fs::path p(s);
        std::error_code ec;
        if (fs::is_directory(p, ec)) {
            for (auto it = fs::recursive_directory_iterator(p, fs::directory_options::skip_permission_denied, ec);
                 it != fs::recursive_directory_iterator(); it.increment(ec)) {
                if (ec) break;
                if (g_cancelar) return false;
                if (!it->is_regular_file()) continue;
                std::error_code ec_rel;
                anadir(it->path(), fs::relative(it->path(), p, ec_rel));
            }
        } else if (fs::is_regular_file(p, ec) && es_comprimido(p)) {
            if (g_cancelar) return false;
            indice_comprimido++;
            fs::path extraido = extraer_comprimido(p, indice_comprimido, logfn);
            if (extraido.empty()) continue;
            fs::path base = prefijar_comprimidos ? fs::path(p.stem()) : fs::path();
            std::error_code ec_rec;
            for (auto it = fs::recursive_directory_iterator(extraido,
                     fs::directory_options::skip_permission_denied, ec_rec);
                 it != fs::recursive_directory_iterator(); it.increment(ec_rec)) {
                if (ec_rec) { ec_rec.clear(); continue; }
                if (g_cancelar) return false;
                std::error_code ec_file;
                if (!it->is_regular_file(ec_file)) continue;
                std::error_code ec_rel;
                anadir(it->path(), base / fs::relative(it->path(), extraido, ec_rel));
            }
        } else if (fs::is_regular_file(p, ec)) {
            anadir(p, p.filename());
        } else {
            logfn(L"OMITIDA  " + p.filename().wstring() + L"  (no es imagen ni comprimido)");
        }
    }
    return true;
}

static bool procesar_imagen(IWICImagingFactory* factory, const Tarea& t, const Options& opt,
                            Row& fila, std::vector<uint8_t>& datos, std::wstring& error) {
    std::error_code ec;
    uintmax_t tam_in = fs::file_size(t.src, ec);
    fila.archivo = fs::relative(t.dst, opt.salida, ec).wstring();
    if (ec || fila.archivo.empty()) fila.archivo = t.dst.filename().wstring();
    fila.origen = t.src.filename().wstring();

    Bitmap bmp;
    if (!decodificar_imagen(factory, t.src.wstring(), bmp, error)) return false;
    std::wstring nota_anim;
    if (bmp.frames > 1)
        nota_anim = L"animada (" + std::to_wstring(bmp.frames) +
                    L" fotogramas), se usa el primero";

    if (bmp.frames > 1 && opt.formato == L"WEBP") {
        std::vector<Bitmap> fotogramas;
        std::vector<int> duraciones;
        if (decodificar_wic_animado(factory, t.src.wstring(), fotogramas, duraciones,
                                    error)) {
            std::vector<Bitmap> finales;
            finales.reserve(fotogramas.size());
            for (auto& frame : fotogramas) {
                Bitmap reducido;
                if (!redimensionar_lanczos(frame, opt.max_dim, reducido, error))
                    return false;
                if (opt.flatten) aplanar_bitmap(reducido, opt.color);
                finales.push_back(std::move(reducido));
            }
            auto datos_anim = codificar_webp_animado(finales, duraciones, opt.calidad,
                                                     opt.metodo, opt.sin_perdida, error);
            if (datos_anim.empty()) return false;
            datos = std::move(datos_anim);
            double kb_in = (double)tam_in / 1024.0;
            double kb_out = (double)datos.size() / 1024.0;
            fila.kb_in = kb_in;
            fila.kb_out = kb_out;
            fila.ahorro = kb_in > 0 ? (1.0 - kb_out / kb_in) * 100.0 : 0;
            fila.estado = opt.dry_run ? L"analizada" : L"convertida";
            fila.ruta = opt.dry_run ? t.src.wstring() : t.dst.wstring();
            fila.nota = L"animada (" + std::to_wstring(finales.size()) + L" fotogramas)";
            if (opt.objetivo_kb > 0)
                fila.nota += L"; el peso maximo no se aplica a animados";
            return true;
        }
    }

    Bitmap final;
    if (!redimensionar_lanczos(bmp, opt.max_dim, final, error)) return false;

    if (opt.flatten && final.alfa) {
        BYTE fr = GetRValue(opt.color), fg = GetGValue(opt.color), fb = GetBValue(opt.color);
        for (size_t px = 0; px < (size_t)final.w * final.h; px++) {
            uint8_t* p = final.rgba.data() + px * 4;
            unsigned a = p[3];
            p[0] = (uint8_t)((p[0] * a + fr * (255 - a)) / 255);
            p[1] = (uint8_t)((p[1] * a + fg * (255 - a)) / 255);
            p[2] = (uint8_t)((p[2] * a + fb * (255 - a)) / 255);
            p[3] = 255;
        }
        final.alfa = false;
    }

    std::wstring aviso;
    datos = codificar_objetivo(final, opt.calidad, opt.metodo, opt.sin_perdida,
                               final.alfa, opt.objetivo_kb, opt.formato == L"AVIF",
                               aviso, error);
    if (datos.empty()) return false;

    double kb_in = (double)tam_in / 1024.0;
    double kb_out = (double)datos.size() / 1024.0;
    fila.kb_in = kb_in;
    fila.kb_out = kb_out;
    fila.ahorro = kb_in > 0 ? (1.0 - kb_out / kb_in) * 100.0 : 0;
    fila.estado = opt.dry_run ? L"analizada" : L"convertida";
    fila.ruta = opt.dry_run ? t.src.wstring() : t.dst.wstring();
    fila.nota = nota_anim;
    if (!aviso.empty()) fila.nota += (fila.nota.empty() ? L"" : L"; ") + aviso;
    return true;
}

static void hilo_conversion(Options opt_trabajo, std::vector<std::wstring> origenes_trabajo) {
    normalizar_opciones(opt_trabajo);
    HRESULT hr_com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    ComPtr<IWICImagingFactory> factory;
    CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
    if (!factory.Get()) {
        emitir_log(L"No se pudo iniciar WIC.");
        Evento fin;
        fin.tipo = Evento::FIN;
        fin.texto = L"No se pudo iniciar WIC";
        fin.a = 0;
        fin.b = 1;
        emitir(std::move(fin));
        if (SUCCEEDED(hr_com)) CoUninitialize();
        return;
    }

    std::vector<Tarea> tareas;
    int omitidas = 0;
    uint64_t total_bytes = 0;
    emitir_log(L"Planificando...");
    if (!preparar_tareas(factory.Get(), opt_trabajo, origenes_trabajo, tareas, omitidas,
                         total_bytes, [](const std::wstring& t) { emitir_log(t); }))
        emitir_log(L"Cancelado durante la planificacion.");

    int total = (int)tareas.size();
    {
        Evento e; e.tipo = Evento::PROGRESO; e.a = 0; e.b = total; emitir(std::move(e));
    }
    emitir_log(L"Imagenes a convertir: " + std::to_wstring(total));
    if (total_bytes > 0) {
        wchar_t tam_txt[64];
        swprintf_s(tam_txt, L"Tamano total: %.2f MB", total_bytes / (1024.0 * 1024.0));
        emitir_log(tam_txt);
    }

    double entrada = 0, salida = 0;
    int convertidas = 0, fallidas = 0;
    std::vector<fs::path> producidas;
    std::vector<Row> filas;

    if (!opt_trabajo.dry_run) fs::create_directories(opt_trabajo.salida);

    std::mutex mutex_resultados;
    std::mutex mutex_progreso;
    std::atomic<int> siguiente{0};
    std::atomic<int> hechas{0};
    int n_hilos = std::min(8, std::max(1, total));
    if (opt_trabajo.jobs > 0)
        n_hilos = std::min(32, std::max(1, opt_trabajo.jobs));

    auto marcar_progreso = [&]() {
        std::lock_guard<std::mutex> lock(mutex_progreso);
        Evento pr;
        pr.tipo = Evento::PROGRESO;
        pr.a = hechas.fetch_add(1) + 1;
        pr.b = total;
        emitir(std::move(pr));
    };

    auto trabajador = [&]() {
        HRESULT hr_hilo = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        ComPtr<IWICImagingFactory> fabrica_hilo;
        CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                         IID_PPV_ARGS(&fabrica_hilo));
        t_hilos_avif = (n_hilos > 1) ? 1 : 4;

        for (;;) {
            while (g_pausa && !g_cancelar) Sleep(100);
            if (g_cancelar) break;
            int i = siguiente.fetch_add(1);
            if (i >= total) break;
            const Tarea& t = tareas[(size_t)i];

            Evento est;
            est.tipo = Evento::ESTADO;
            est.texto = (opt_trabajo.dry_run ? L"Analizando " : L"Convirtiendo ") +
                        std::to_wstring(i + 1) + L"/" + std::to_wstring(total);
            emitir(std::move(est));

            Row fila;
            std::vector<uint8_t> datos;
            std::wstring error;
            bool correcta = false;
            try {
                correcta = fabrica_hilo.Get() != nullptr &&
                           procesar_imagen(fabrica_hilo.Get(), t, opt_trabajo, fila, datos, error);
                if (!correcta && error.empty()) error = L"no se pudo crear la factoria WIC";
                if (correcta && !opt_trabajo.dry_run) {
                    std::error_code ec2;
                    fs::create_directories(t.dst.parent_path(), ec2);
                    fs::path parcial = t.dst;
                    parcial += L".part";
                    std::ofstream out(parcial, std::ios::binary | std::ios::trunc);
                    bool escrito = false;
                    if (out) {
                        out.write((char*)datos.data(), (std::streamsize)datos.size());
                        out.close();
                        escrito = out.good();
                    }
                    if (escrito) {
                        escrito = reemplazar_archivo(parcial, t.dst);
                        if (escrito && opt_trabajo.preservar_fecha)
                            copiar_fecha(t.src, t.dst);
                    }
                    if (!escrito) {
                        std::error_code ec_limpieza;
                        fs::remove(parcial, ec_limpieza);
                        correcta = false;
                        error = L"no se pudo escribir el archivo";
                    }
                }
            } catch (const std::exception& ex) {
                correcta = false;
                error = L"excepcion: " + a_wstring(ex.what());
            } catch (...) {
                correcta = false;
                error = L"excepcion desconocida";
            }
            if (!correcta) {
                fila.estado = L"fallo";
                if (fila.archivo.empty()) fila.archivo = t.dst.filename().wstring();
                {
                    std::lock_guard<std::mutex> lock(mutex_resultados);
                    fallidas++;
                }
                emitir_log(L"FALLO    " + t.src.filename().wstring() + L"   " + error);
                Evento fe; fe.tipo = Evento::FILA; fe.fila = fila; emitir(std::move(fe));
                marcar_progreso();
                continue;
            }

            {
                std::lock_guard<std::mutex> lock(mutex_resultados);
                convertidas++;
                entrada += fila.kb_in * 1024.0;
                salida += (double)datos.size();
                filas.push_back(fila);
                if (!opt_trabajo.dry_run) producidas.push_back(t.dst);
            }

            wchar_t linea[512];
            std::wstring nota = fila.nota.empty() ? L"" : (L"  (" + fila.nota + L")");
            swprintf_s(linea, L"OK       %s   %.1f KB -> %.1f KB%s",
                       fila.archivo.c_str(), fila.kb_in, fila.kb_out, nota.c_str());
            emitir_log(linea);
            Evento fe; fe.tipo = Evento::FILA; fe.fila = fila; emitir(std::move(fe));
            marcar_progreso();
        }
        if (SUCCEEDED(hr_hilo)) CoUninitialize();
    };

    std::vector<std::thread> hilos;
    hilos.reserve((size_t)n_hilos);
    for (int i = 0; i < n_hilos; i++) hilos.emplace_back(trabajador);
    for (auto& hilo : hilos) hilo.join();

    std::sort(filas.begin(), filas.end(), [](const Row& a, const Row& b) {
        return a.archivo < b.archivo;
    });
    std::sort(producidas.begin(), producidas.end());

    if (opt_trabajo.csv && !filas.empty()) {
        std::error_code ec_csv;
        fs::create_directories(opt_trabajo.salida, ec_csv);
        std::wstring prefijo = opt_trabajo.dry_run ? L"analisis_" : L"informe_";
        fs::path csv = fs::path(opt_trabajo.salida) / (prefijo + fecha_archivo() + L".csv");
        std::ofstream f(csv, std::ios::binary);
        f << "\xEF\xBB\xBF";
        f << "Archivo;Origen;KB original;KB resultado;Ahorro %;Estado;Nota\r\n";
        for (auto& r : filas) {
            f << csv_campo(a_utf8(r.archivo)) << ";" << csv_campo(a_utf8(r.origen)) << ";"
              << std::fixed << std::setprecision(1) << r.kb_in << ";" << r.kb_out << ";"
              << r.ahorro << ";" << csv_campo(a_utf8(r.estado)) << ";"
              << csv_campo(a_utf8(r.nota)) << "\r\n";
        }
        emitir_log(L"Informe CSV: " + csv.wstring());
    }

    if (opt_trabajo.zip && !producidas.empty() && !g_cancelar) {
        std::wstring nombre = nombre_seguro(opt_trabajo.zip_nombre, L"webp.zip");
        if (lower(nombre).size() < 4 || lower(nombre).substr(lower(nombre).size() - 4) != L".zip")
            nombre += L".zip";
        fs::path zip = fs::path(opt_trabajo.salida) / nombre;
        if (escribir_zip(zip, producidas, opt_trabajo.salida))
            emitir_log(L"ZIP creado: " + zip.wstring() + L"  (" + std::to_wstring(producidas.size()) + L" archivos)");
        else
            emitir_log(L"FALLO al crear el ZIP");
    }

    limpiar_temporales();

    Evento fin;
    fin.tipo = Evento::FIN;
    wchar_t resumen[256];
    swprintf_s(resumen, opt_trabajo.dry_run ? L"Analizadas: %d   Omitidas: %d   Fallidas: %d"
                                     : L"Convertidas: %d   Omitidas: %d   Fallidas: %d",
               convertidas, omitidas, fallidas);
    fin.texto = resumen;
    fin.a = convertidas;
    fin.b = fallidas;
    fin.c = omitidas;
    fin.fila.kb_in = entrada / 1024.0;
    fin.fila.kb_out = salida / 1024.0;
    emitir(std::move(fin));
    if (SUCCEEDED(hr_com)) CoUninitialize();
}

static void hilo_conversion_seguro(Options opt, std::vector<std::wstring> origenes) {
    try {
        hilo_conversion(std::move(opt), std::move(origenes));
    } catch (const std::exception& ex) {
        emitir_log(L"ERROR inesperado: " + a_wstring(ex.what()));
        limpiar_temporales();
        Evento fin;
        fin.tipo = Evento::FIN;
        fin.texto = L"Error inesperado durante la conversion";
        fin.a = 0;
        fin.b = 1;
        emitir(std::move(fin));
    } catch (...) {
        emitir_log(L"ERROR inesperado durante la conversion");
        limpiar_temporales();
        Evento fin;
        fin.tipo = Evento::FIN;
        fin.texto = L"Error inesperado durante la conversion";
        fin.a = 0;
        fin.b = 1;
        emitir(std::move(fin));
    }
}

// ---------------------------------------------------------------------------
//  Vigilante de carpeta
// ---------------------------------------------------------------------------
static Options leer_opciones_controles();

static std::atomic<bool> g_watch_activo{false};
static std::thread g_watch_hilo;
static std::wstring g_watch_carpeta;
static HMENU g_menu_herramientas = nullptr;

static void hilo_vigilante(std::wstring carpeta, Options opt,
                           std::set<std::wstring> vistos) {
    HRESULT hr_com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    ComPtr<IWICImagingFactory> factory;
    CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                     IID_PPV_ARGS(&factory));
    if (!factory.Get()) {
        emitir_log(L"Vigilante: no se pudo iniciar WIC.");
        g_watch_activo = false;
        if (SUCCEEDED(hr_com)) CoUninitialize();
        return;
    }
    std::map<std::wstring, int> fallos;
    std::set<std::wstring> destinos;
    std::map<std::wstring, std::pair<uintmax_t, fs::file_time_type>> huellas;

    while (g_watch_activo) {
        for (int i = 0; i < 15 && g_watch_activo; i++) Sleep(100);
        if (!g_watch_activo) break;
        if (g_corriendo) continue;

        std::error_code ec_dir;
        for (fs::recursive_directory_iterator it(carpeta,
                 fs::directory_options::skip_permission_denied, ec_dir), fin;
             it != fin; it.increment(ec_dir)) {
            if (!g_watch_activo) break;
            if (ec_dir) { ec_dir.clear(); continue; }
            std::error_code ec;
            if (!it->is_regular_file(ec)) continue;
            fs::path src = it->path();
            if (!es_imagen(src)) continue;
            std::wstring ext_destino = opt.formato == L"AVIF" ? L".avif" : L".webp";
            if (ext_lower(src) == ext_destino && !opt.reencode) continue;

            std::wstring clave = lower(fs::absolute(src, ec).wstring());
            if (clave.empty()) clave = lower(src.wstring());
            if (vistos.count(clave)) continue;

            std::error_code ec_tam, ec_fecha;
            uintmax_t tam_src = fs::file_size(src, ec_tam);
            auto fecha_src = fs::last_write_time(src, ec_fecha);
            if (ec_tam || ec_fecha) continue;
            auto huella = std::make_pair(tam_src, fecha_src);
            auto it_huella = huellas.find(clave);
            if (it_huella == huellas.end() || it_huella->second != huella) {
                huellas[clave] = huella;
                continue;  // espera a que el archivo deje de cambiar
            }

            fs::path rel = fs::relative(src, carpeta, ec);
            if (ec || rel.empty()) rel = src.filename();
            if (!opt.mantener) rel = rel.filename();
            std::wstring rel_patron = aplicar_patron(opt.patron, factory.Get(), src, rel,
                                                     opt.max_dim, opt.calidad, opt.formato);
            fs::path dst = fs::path(opt.salida) / rel_patron;
            if (lower(dst.extension().wstring()) != ext_destino) dst.replace_extension(ext_destino);
            fs::path base_dst = dst;
            int intento = 1;
            while (destinos.count(lower(dst.wstring()))) {
                ++intento;
                dst = base_dst.parent_path() / (base_dst.stem().wstring() + L"_" +
                                                std::to_wstring(intento) + ext_destino);
            }
            std::error_code ec_ex;
            if (!opt.sobrescribir && fs::exists(dst, ec_ex)) {
                emitir_log(L"VIGILANTE OMITIDA  " + src.filename().wstring() + L"  (ya existe)");
                vistos.insert(clave);
                continue;
            }
            destinos.insert(lower(dst.wstring()));

            Tarea t{src, rel, dst};
            Row fila;
            std::vector<uint8_t> datos;
            std::wstring error;
            bool correcta = false;
            try {
                correcta = procesar_imagen(factory.Get(), t, opt, fila, datos, error);
                if (correcta) {
                    std::error_code ec_w;
                    fs::create_directories(dst.parent_path(), ec_w);
                    fs::path parcial = dst;
                    parcial += L".part";
                    std::ofstream out(parcial, std::ios::binary | std::ios::trunc);
                    bool escrito = false;
                    if (out) {
                        out.write((char*)datos.data(), (std::streamsize)datos.size());
                        out.close();
                        escrito = out.good();
                    }
                    if (escrito) {
                        escrito = reemplazar_archivo(parcial, dst);
                        if (escrito && opt.preservar_fecha)
                            copiar_fecha(src, dst);
                    }
                    if (!escrito) {
                        std::error_code ec_limpieza;
                        fs::remove(parcial, ec_limpieza);
                        correcta = false;
                        error = L"no se pudo escribir el archivo";
                    }
                }
            } catch (const std::exception& ex) {
                correcta = false;
                error = L"excepcion: " + a_wstring(ex.what());
            } catch (...) {
                correcta = false;
                error = L"excepcion desconocida";
            }
            if (!correcta) {
                int n_fallos = ++fallos[clave];
                emitir_log(L"VIGILANTE FALLO  " + src.filename().wstring() + L"   " + error);
                if (n_fallos >= 3) vistos.insert(clave);
                continue;
            }
            vistos.insert(clave);
            fallos.erase(clave);
            wchar_t linea[512];
            swprintf_s(linea, L"VIGILANTE  %s -> %s   %.1f KB",
                       rel.wstring().c_str(), dst.filename().wstring().c_str(), fila.kb_out);
            emitir_log(linea);
            Evento fe; fe.tipo = Evento::FILA; fe.fila = fila; emitir(std::move(fe));
        }
    }
    if (SUCCEEDED(hr_com)) CoUninitialize();
}

static void detener_vigilante(bool actualizar_ui) {
    g_watch_activo = false;
    if (g_watch_hilo.joinable()) g_watch_hilo.join();
    if (actualizar_ui) {
        if (g_menu_herramientas)
            CheckMenuItem(g_menu_herramientas, ID_MENU_WATCH,
                          MF_BYCOMMAND | MF_UNCHECKED);
        SetWindowTextW(g_status, L"Carpeta vigilada detenida.");
        emitir_log(L"Vigilante: detenido.");
    }
}

static void iniciar_vigilante_ui() {
    if (g_watch_activo) {
        detener_vigilante(true);
        return;
    }
    if (g_corriendo) {
        MessageBoxW(g_hwnd, L"Espera a que termine la conversion en curso.",
                    APP_NAME, MB_ICONINFORMATION);
        return;
    }
    Options opt = leer_opciones_controles();
    if (opt.salida.empty()) {
        MessageBoxW(g_hwnd, L"Elige primero la carpeta de salida.", APP_NAME, MB_ICONWARNING);
        return;
    }
    if (opt.dry_run) {
        MessageBoxW(g_hwnd, L"El vigilante no funciona en modo 'Solo analizar'.",
                    APP_NAME, MB_ICONINFORMATION);
        return;
    }

    std::wstring carpeta;
    IFileOpenDialog* dlg = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                   IID_PPV_ARGS(&dlg)))) {
        DWORD opts;
        dlg->GetOptions(&opts);
        dlg->SetOptions(opts | FOS_PICKFOLDERS | FOS_PATHMUSTEXIST);
        dlg->SetTitle(L"Carpeta que quieres vigilar");
        if (SUCCEEDED(dlg->Show(g_hwnd))) {
            IShellItem* it = nullptr;
            if (SUCCEEDED(dlg->GetResult(&it))) {
                PWSTR ruta = nullptr;
                if (SUCCEEDED(it->GetDisplayName(SIGDN_FILESYSPATH, &ruta))) {
                    carpeta = ruta;
                    CoTaskMemFree(ruta);
                }
                it->Release();
            }
        }
        dlg->Release();
    }
    if (carpeta.empty()) return;

    std::set<std::wstring> vistos;
    std::error_code ec;
    for (fs::recursive_directory_iterator it(carpeta,
             fs::directory_options::skip_permission_denied, ec), fin;
         it != fin; it.increment(ec)) {
        if (ec) { ec.clear(); continue; }
        std::error_code ec2;
        if (!it->is_regular_file(ec2) || !es_imagen(it->path())) continue;
        vistos.insert(lower(fs::absolute(it->path(), ec2).wstring()));
    }

    g_watch_carpeta = carpeta;
    g_watch_activo = true;
    if (g_watch_hilo.joinable()) g_watch_hilo.join();
    g_watch_hilo = std::thread(hilo_vigilante, carpeta, opt, std::move(vistos));
    if (g_menu_herramientas)
        CheckMenuItem(g_menu_herramientas, ID_MENU_WATCH, MF_BYCOMMAND | MF_CHECKED);
    SetWindowTextW(g_status, (L"Vigilando: " + carpeta).c_str());
    emitir_log(L"Vigilante activo: " + carpeta + L"  ->  " + opt.salida);
}

// ---------------------------------------------------------------------------
//  GUI
// ---------------------------------------------------------------------------
static int maxdim_indice(int valor) {
    switch (valor) {
        case 2560: return 1;
        case 1920: return 2;
        case 1600: return 3;
        case 1200: return 4;
        case 800: return 5;
        case 512: return 6;
        default: return 0;
    }
}

static void refrescar_combo_preset() {
    if (!g_combo_preset) return;
    SendMessageW(g_combo_preset, CB_RESETCONTENT, 0, 0);
    SendMessageW(g_combo_preset, CB_ADDSTRING, 0, (LPARAM)L"Personalizado");
    for (auto& p : g_presets)
        SendMessageW(g_combo_preset, CB_ADDSTRING, 0, (LPARAM)p.nombre.c_str());
    SendMessageW(g_combo_preset, CB_SETCURSEL, 0, 0);
}

static void aplicar_opciones_controles(const Options& o, bool incluir_salida) {
    if (g_track_calidad) {
        SendMessageW(g_track_calidad, TBM_SETPOS, TRUE, o.calidad);
        wchar_t b[8];
        swprintf_s(b, L"%d", o.calidad);
        SetWindowTextW(g_lbl_calidad, b);
    }
    if (g_combo_metodo) SendMessageW(g_combo_metodo, CB_SETCURSEL, o.metodo, 0);
    if (g_combo_formato) SendMessageW(g_combo_formato, CB_SETCURSEL,
                                      o.formato == L"AVIF" ? 1 : 0, 0);
    if (g_btn_convertir) InvalidateRect(g_btn_convertir, nullptr, TRUE);
    if (g_combo_dim) SendMessageW(g_combo_dim, CB_SETCURSEL, maxdim_indice(o.max_dim), 0);
    if (g_check_lossless) SendMessageW(g_check_lossless, BM_SETCHECK, o.sin_perdida ? BST_CHECKED : BST_UNCHECKED, 0);
    if (g_check_reencode) SendMessageW(g_check_reencode, BM_SETCHECK, o.reencode ? BST_CHECKED : BST_UNCHECKED, 0);
    if (g_check_estructura) SendMessageW(g_check_estructura, BM_SETCHECK, o.mantener ? BST_CHECKED : BST_UNCHECKED, 0);
    if (g_check_sobrescribir) SendMessageW(g_check_sobrescribir, BM_SETCHECK, o.sobrescribir ? BST_CHECKED : BST_UNCHECKED, 0);
    if (g_check_zip) SendMessageW(g_check_zip, BM_SETCHECK, o.zip ? BST_CHECKED : BST_UNCHECKED, 0);
    if (g_check_csv) SendMessageW(g_check_csv, BM_SETCHECK, o.csv ? BST_CHECKED : BST_UNCHECKED, 0);
    g_ui_beep = o.beep;
    if (g_check_beep) InvalidateRect(g_check_beep, nullptr, TRUE);
    if (g_check_dry) SendMessageW(g_check_dry, BM_SETCHECK, o.dry_run ? BST_CHECKED : BST_UNCHECKED, 0);
    if (g_check_fecha) SendMessageW(g_check_fecha, BM_SETCHECK, o.preservar_fecha ? BST_CHECKED : BST_UNCHECKED, 0);
    if (g_check_flatten) SendMessageW(g_check_flatten, BM_SETCHECK, o.flatten ? BST_CHECKED : BST_UNCHECKED, 0);
    g_ui_apagar = o.apagar;
    if (g_check_apagar) InvalidateRect(g_check_apagar, nullptr, TRUE);
    if (g_edit_objetivo) {
        wchar_t kb[16];
        swprintf_s(kb, L"%d", o.objetivo_kb > 0 ? o.objetivo_kb : 150);
        SetWindowTextW(g_edit_objetivo, kb);
    }
    if (g_edit_patron) SetWindowTextW(g_edit_patron, o.patron.c_str());
    g_color = o.color;
    if (g_btn_color) InvalidateRect(g_btn_color, nullptr, TRUE);
    if (g_btn_color) EnableWindow(g_btn_color, o.flatten);
    if (g_track_calidad) EnableWindow(g_track_calidad, !o.sin_perdida);
    if (g_combo_metodo) EnableWindow(g_combo_metodo, !o.sin_perdida);
    if (g_edit_objetivo) EnableWindow(g_edit_objetivo, o.objetivo_kb > 0);
    if (g_edit_objetivo) EnableWindow(g_edit_objetivo, o.objetivo_kb > 0);
    if (g_check_zip) EnableWindow(g_check_zip, !o.dry_run);
    if (g_edit_zip) EnableWindow(g_edit_zip, o.zip && !o.dry_run);
    if (incluir_salida) {
        if (g_edit_salida) SetWindowTextW(g_edit_salida, o.salida.c_str());
        if (g_edit_zip) SetWindowTextW(g_edit_zip, o.zip_nombre.c_str());
    }
}

static Options leer_opciones_controles() {
    Options o = g_opt;
    if (g_track_calidad) o.calidad = (int)SendMessageW(g_track_calidad, TBM_GETPOS, 0, 0);
    if (g_combo_metodo) {
        int sel = (int)SendMessageW(g_combo_metodo, CB_GETCURSEL, 0, 0);
        if (sel >= 0) o.metodo = sel;
    }
    if (g_combo_formato) {
        int sel = (int)SendMessageW(g_combo_formato, CB_GETCURSEL, 0, 0);
        o.formato = sel == 1 ? L"AVIF" : L"WEBP";
    }
    if (g_combo_dim) {
        static const int dims[] = {0, 2560, 1920, 1600, 1200, 800, 512};
        int sel = (int)SendMessageW(g_combo_dim, CB_GETCURSEL, 0, 0);
        o.max_dim = (sel >= 0 && sel < 7) ? dims[sel] : 0;
    }
    if (g_check_lossless) o.sin_perdida = SendMessageW(g_check_lossless, BM_GETCHECK, 0, 0) == BST_CHECKED;
    if (g_check_reencode) o.reencode = SendMessageW(g_check_reencode, BM_GETCHECK, 0, 0) == BST_CHECKED;
    if (g_check_estructura) o.mantener = SendMessageW(g_check_estructura, BM_GETCHECK, 0, 0) == BST_CHECKED;
    if (g_check_sobrescribir) o.sobrescribir = SendMessageW(g_check_sobrescribir, BM_GETCHECK, 0, 0) == BST_CHECKED;
    if (g_check_zip) o.zip = SendMessageW(g_check_zip, BM_GETCHECK, 0, 0) == BST_CHECKED;
    if (g_check_csv) o.csv = SendMessageW(g_check_csv, BM_GETCHECK, 0, 0) == BST_CHECKED;
    o.beep = g_ui_beep;
    if (g_check_dry) o.dry_run = SendMessageW(g_check_dry, BM_GETCHECK, 0, 0) == BST_CHECKED;
    if (g_check_fecha) o.preservar_fecha = SendMessageW(g_check_fecha, BM_GETCHECK, 0, 0) == BST_CHECKED;
    if (g_check_flatten) o.flatten = SendMessageW(g_check_flatten, BM_GETCHECK, 0, 0) == BST_CHECKED;
    o.apagar = g_ui_apagar;
    if (g_edit_objetivo) {
        bool usar = SendMessageW(GetDlgItem(g_hwnd, ID_TARGET), BM_GETCHECK, 0, 0) == BST_CHECKED;
        o.objetivo_kb = (usar && !o.sin_perdida) ? _wtoi(texto_ventana(g_edit_objetivo).c_str()) : 0;
        if (o.objetivo_kb > 0 && o.objetivo_kb < 5) o.objetivo_kb = 5;
    }
    if (g_edit_patron) o.patron = texto_ventana(g_edit_patron);
    if (g_edit_salida) o.salida = recortar(texto_ventana(g_edit_salida));
    if (g_edit_zip) o.zip_nombre = texto_ventana(g_edit_zip);
    o.color = g_color;
    normalizar_opciones(o);
    return o;
}

static fs::path ruta_cola() { return fs::path(g_ini).parent_path() / L"queue.ini"; }

static void borrar_cola() {
    std::error_code ec;
    fs::remove(ruta_cola(), ec);
}

static void guardar_cola() {
    if (g_origenes.empty()) {
        borrar_cola();
        return;
    }
    std::wstring ruta = ruta_cola().wstring();
    std::wstring cuenta = std::to_wstring(g_origenes.size());
    WritePrivateProfileStringW(L"cola", L"count", cuenta.c_str(), ruta.c_str());
    for (size_t i = 0; i < g_origenes.size(); i++) {
        std::wstring clave = L"origen" + std::to_wstring(i);
        WritePrivateProfileStringW(L"cola", clave.c_str(), g_origenes[i].c_str(),
                                   ruta.c_str());
    }
    std::wstring opciones = serializar_opciones(g_opt);
    WritePrivateProfileStringW(L"cola", L"opciones", opciones.c_str(), ruta.c_str());
}

static void cargar_cola() {
    std::wstring ruta = ruta_cola().wstring();
    int n = std::clamp((int)GetPrivateProfileIntW(L"cola", L"count", 0, ruta.c_str()),
                       0, 10000);
    if (n <= 0) return;
    std::vector<std::wstring> fuentes;
    for (int i = 0; i < n; i++) {
        std::wstring clave = L"origen" + std::to_wstring(i);
        wchar_t valor[32768] = L"";
        GetPrivateProfileStringW(L"cola", clave.c_str(), L"", valor, 32768, ruta.c_str());
        if (valor[0]) {
            std::error_code ec;
            if (fs::exists(valor, ec)) fuentes.push_back(valor);
        }
    }
    if (fuentes.empty()) {
        borrar_cola();
        return;
    }
    wchar_t opciones[8192] = L"";
    GetPrivateProfileStringW(L"cola", L"opciones", L"", opciones, 8192, ruta.c_str());
    g_origenes = fuentes;
    if (opciones[0]) g_opt = deserializar_opciones(opciones);
    aplicar_opciones_controles(g_opt, true);
    refrescar_origenes();
    SetWindowTextW(g_status, L"Cola pendiente restaurada. Pulsa Convertir.");
    log(L"Cola pendiente restaurada: " + std::to_wstring(fuentes.size()) + L" origen(es).");
}

static void mover_origen(int delta) {
    if (g_corriendo) return;
    int sel = ListView_GetNextItem(g_lista_origen, -1, LVNI_SELECTED);
    if (sel < 0) return;
    int destino = sel + delta;
    if (destino < 0 || destino >= (int)g_origenes.size()) return;
    std::swap(g_origenes[(size_t)sel], g_origenes[(size_t)destino]);
    refrescar_origenes();
    ListView_SetItemState(g_lista_origen, destino,
                          LVIS_SELECTED | LVIS_FOCUSED,
                          LVIS_SELECTED | LVIS_FOCUSED);
    ListView_EnsureVisible(g_lista_origen, destino, FALSE);
    std::error_code ec_cola;
    if (fs::exists(ruta_cola(), ec_cola)) guardar_cola();
}

static void reintentar_fallidos() {
    if (g_corriendo) {
        MessageBoxW(g_hwnd, L"Espera a que termine el trabajo en curso.", APP_NAME,
                    MB_ICONINFORMATION);
        return;
    }
    std::vector<std::wstring> fuentes;
    for (auto& p : g_fallidos_ultimos) {
        std::error_code ec;
        if (fs::exists(p, ec)) fuentes.push_back(p);
    }
    if (fuentes.empty()) {
        MessageBoxW(g_hwnd, L"No hay archivos fallidos pendientes de reintentar.", APP_NAME,
                    MB_ICONINFORMATION);
        return;
    }
    g_origenes = fuentes;
    refrescar_origenes();
    SetWindowTextW(g_status, L"Fallidos cargados. Pulsa Convertir para reintentar.");
    log(L"Reintento preparado con " + std::to_wstring(fuentes.size()) + L" archivo(s).");
    std::error_code ec_cola;
    if (fs::exists(ruta_cola(), ec_cola)) guardar_cola();
}

static void marcar_personalizado() {
    if (!g_aplicando_preset && g_combo_preset)
        SendMessageW(g_combo_preset, CB_SETCURSEL, 0, 0);
}

static void aplicar_preset(int indice) {
    if (indice <= 0 || indice > (int)g_presets.size()) return;
    const Preset& p = g_presets[indice - 1];
    g_aplicando_preset = true;
    g_opt.calidad = p.opt.calidad;
    g_opt.metodo = p.opt.metodo;
    g_opt.sin_perdida = p.opt.sin_perdida;
    g_opt.reencode = p.opt.reencode;
    g_opt.max_dim = p.opt.max_dim;
    g_opt.objetivo_kb = p.opt.objetivo_kb;
    g_opt.flatten = p.opt.flatten;
    g_opt.color = p.opt.color;
    g_opt.formato = p.opt.formato;
    aplicar_opciones_controles(g_opt, false);
    SendMessageW(g_combo_preset, CB_SETCURSEL, indice, 0);
    g_aplicando_preset = false;
}

static std::wstring g_input_texto;
static bool g_input_ok = false;
static HWND g_input_edit = nullptr;

static LRESULT CALLBACK InputWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CREATE: {
            CreateWindowExW(0, L"STATIC", L"Nombre:", WS_CHILD | WS_VISIBLE, 16, 18, 340, 18,
                            hwnd, nullptr, nullptr, nullptr);
            g_input_edit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", g_input_texto.c_str(),
                                           WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                                           16, 42, 340, 24, hwnd, (HMENU)100, nullptr, nullptr);
            SendMessageW(g_input_edit, WM_SETFONT, (WPARAM)g_font, TRUE);
            HWND ok = CreateWindowExW(0, L"BUTTON", L"Aceptar",
                                      WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                                      190, 78, 80, 26, hwnd, (HMENU)IDOK, nullptr, nullptr);
            HWND cancel = CreateWindowExW(0, L"BUTTON", L"Cancelar",
                                          WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                                          276, 78, 80, 26, hwnd, (HMENU)IDCANCEL, nullptr, nullptr);
            SendMessageW(ok, WM_SETFONT, (WPARAM)g_font, TRUE);
            SendMessageW(cancel, WM_SETFONT, (WPARAM)g_font, TRUE);
            SetFocus(g_input_edit);
            SendMessageW(g_input_edit, EM_SETSEL, 0, -1);
            return 0;
        }
        case WM_ERASEBKGND: {
            RECT rc{};
            GetClientRect(hwnd, &rc);
            FillRect((HDC)wp, &rc, g_brush_bg);
            return 1;
        }
        case WM_CTLCOLORSTATIC:
            SetBkMode((HDC)wp, TRANSPARENT);
            SetTextColor((HDC)wp, g_pal.text);
            return (LRESULT)g_brush_bg;
        case WM_CTLCOLOREDIT:
            SetBkColor((HDC)wp, g_pal.input);
            SetTextColor((HDC)wp, g_pal.text);
            return (LRESULT)g_brush_input;
        case WM_DRAWITEM: {
            DRAWITEMSTRUCT* di = (DRAWITEMSTRUCT*)lp;
            if (di && (di->CtlID == IDOK || di->CtlID == IDCANCEL)) {
                dibujar_boton_tema(di);
                return TRUE;
            }
            break;
        }
        case WM_COMMAND:
            if (LOWORD(wp) == IDOK) {
                g_input_texto = texto_ventana(g_input_edit);
                g_input_ok = true;
                DestroyWindow(hwnd);
            } else if (LOWORD(wp) == IDCANCEL) {
                DestroyWindow(hwnd);
            }
            return 0;
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static std::wstring pedir_texto(HWND parent, const wchar_t* titulo, const std::wstring& inicial) {
    static bool registrada = false;
    if (!registrada) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = InputWndProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr;
        wc.lpszClassName = L"ConversorInputWnd";
        RegisterClassExW(&wc);
        registrada = true;
    }
    g_input_texto = inicial;
    g_input_ok = false;
    RECT pr{};
    GetWindowRect(parent, &pr);
    int ancho = 380, alto = 150;
    int x = pr.left + ((pr.right - pr.left) - ancho) / 2;
    int y = pr.top + ((pr.bottom - pr.top) - alto) / 2;
    HWND w = CreateWindowExW(WS_EX_DLGMODALFRAME, L"ConversorInputWnd", titulo,
                             WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE, x, y, ancho, alto,
                             parent, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!w) return L"";
    aplicar_modo_oscuro_nativo(w, g_oscuro);
    InvalidateRect(w, nullptr, TRUE);
    EnableWindow(parent, FALSE);
    MSG msg;
    while (IsWindow(w) && GetMessageW(&msg, nullptr, 0, 0)) {
        if (!IsDialogMessageW(w, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    EnableWindow(parent, TRUE);
    SetForegroundWindow(parent);
    return g_input_ok ? g_input_texto : L"";
}

static void elegir_color() {
    static COLORREF personalizados[16] = {};
    CHOOSECOLORW cc{};
    cc.lStructSize = sizeof(cc);
    cc.hwndOwner = g_hwnd;
    cc.rgbResult = g_color;
    cc.lpCustColors = personalizados;
    cc.Flags = CC_FULLOPEN | CC_RGBINIT;
    if (ChooseColorW(&cc)) {
        g_color = cc.rgbResult;
        g_opt.color = g_color;
            InvalidateRect(g_btn_color, nullptr, TRUE);
    }
}

static void guardar_preset_ui() {
    std::wstring nombre = pedir_texto(g_hwnd, L"Nombre del preset", L"Mi preset");
    if (nombre.empty()) return;
    for (auto& p : g_presets) {
        if (_wcsicmp(p.nombre.c_str(), nombre.c_str()) == 0) {
            if (!p.propio) {
                MessageBoxW(g_hwnd, L"Ese nombre esta reservado por un preset incluido.",
                            APP_NAME, MB_ICONINFORMATION);
                return;
            }
            if (MessageBoxW(g_hwnd, L"Ya existe ese preset. ¿Sobrescribirlo?", APP_NAME,
                            MB_ICONQUESTION | MB_YESNO) != IDYES)
                return;
            p.opt = leer_opciones_controles();
            guardar_config();
            refrescar_combo_preset();
            return;
        }
    }
    Preset p;
    p.nombre = nombre;
    p.opt = leer_opciones_controles();
    p.propio = true;
    g_presets.push_back(p);
    guardar_config();
    refrescar_combo_preset();
    SendMessageW(g_combo_preset, CB_SETCURSEL, (WPARAM)g_presets.size(), 0);
}

static void borrar_preset_ui() {
    int sel = (int)SendMessageW(g_combo_preset, CB_GETCURSEL, 0, 0);
    if (sel <= 0 || sel > (int)g_presets.size()) return;
    Preset& p = g_presets[sel - 1];
    if (!p.propio) {
        MessageBoxW(g_hwnd, L"Los presets incluidos no se pueden borrar.", APP_NAME,
                    MB_ICONINFORMATION);
        return;
    }
    if (MessageBoxW(g_hwnd, L"¿Borrar el preset seleccionado?", APP_NAME,
                    MB_ICONQUESTION | MB_YESNO) != IDYES)
        return;
    g_presets.erase(g_presets.begin() + (sel - 1));
    guardar_config();
    refrescar_combo_preset();
}

static std::wstring fecha_corta() {
    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t buf[32];
    swprintf_s(buf, L"%02d/%02d/%04d %02d:%02d", st.wDay, st.wMonth, st.wYear,
               st.wHour, st.wMinute);
    return buf;
}

static void reconstruir_menu_historial() {
    if (!g_menu_hist) return;
    while (GetMenuItemCount(g_menu_hist) > 0)
        DeleteMenu(g_menu_hist, 0, MF_BYPOSITION);
    if (g_historial.empty()) {
        AppendMenuW(g_menu_hist, MF_STRING | MF_GRAYED, 0, L"(sin conversiones todavia)");
    } else {
        for (size_t i = 0; i < g_historial.size(); i++) {
            auto& h = g_historial[i];
            std::wstring nombres;
            for (size_t j = 0; j < h.origenes.size() && j < 2; j++) {
                if (j) nombres += L", ";
                nombres += fs::path(h.origenes[j]).filename().wstring();
            }
            if (h.origenes.size() > 2)
                nombres += L" +" + std::to_wstring(h.origenes.size() - 2);
            std::wstring etiqueta = h.fecha + L"  ·  " + nombres + L"  (" +
                                    std::to_wstring(h.convertidas) + L" img)";
            if (etiqueta.size() > 95) etiqueta.resize(95);
            AppendMenuW(g_menu_hist, MF_STRING, ID_HIST_BASE + (UINT)i, etiqueta.c_str());
        }
    }
    AppendMenuW(g_menu_hist, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(g_menu_hist, MF_STRING, ID_HIST_CLEAR, L"Vaciar historial");
    DrawMenuBar(g_hwnd);
}

static void registrar_historial(int convertidas) {
    if (g_opt.dry_run || convertidas <= 0) return;
    HistoryEntry h;
    h.fecha = fecha_corta();
    h.origenes = g_origenes;
    h.salida = g_opt.salida;
    h.opt = g_opt;
    h.convertidas = convertidas;
    if (!g_historial.empty() && g_historial[0].origenes == h.origenes &&
        g_historial[0].salida == h.salida) {
        g_historial[0] = h;
    } else {
        g_historial.insert(g_historial.begin(), h);
    }
    if (g_historial.size() > 10) g_historial.resize(10);
    guardar_config();
    reconstruir_menu_historial();
}

static void restaurar_historial(int indice) {
    if (indice < 0 || indice >= (int)g_historial.size()) return;
    HistoryEntry h = g_historial[(size_t)indice];
    g_origenes = h.origenes;
    g_opt = h.opt;
    g_opt.salida = h.salida;
    aplicar_opciones_controles(g_opt, true);
    refrescar_origenes();
    SetWindowTextW(g_status, (L"Historial: trabajo del " + h.fecha).c_str());
}

static void vaciar_historial() {
    if (g_historial.empty()) return;
    if (MessageBoxW(g_hwnd, L"¿Vaciar el historial de trabajos?", APP_NAME,
                    MB_ICONQUESTION | MB_YESNO) != IDYES)
        return;
    g_historial.clear();
    guardar_config();
    reconstruir_menu_historial();
}

static bool programar_apagado(int segundos, bool avisar = true) {
    wchar_t cmd[160];
    swprintf_s(cmd, L"/s /t %d /c \"Conversor WebP\"", segundos);
    wchar_t sistema[MAX_PATH] = L"C:\\Windows\\System32";
    GetSystemDirectoryW(sistema, MAX_PATH);
    fs::path shutdown = fs::path(sistema) / L"shutdown.exe";
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    if (!CreateProcessW(shutdown.c_str(), cmd, nullptr, nullptr, FALSE,
                        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        if (avisar)
            MessageBoxW(g_hwnd, L"No se pudo programar el apagado.", APP_NAME, MB_ICONERROR);
        return false;
    }
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return true;
}

static HWND g_apagado_hwnd = nullptr;
static HWND g_apagado_label = nullptr;
static int g_apagado_restante = 30;

static LRESULT CALLBACK ApagadoWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CREATE: {
            g_apagado_hwnd = hwnd;
            g_apagado_restante = 30;
            CreateWindowExW(0, L"STATIC", L"Conversion completada", WS_CHILD | WS_VISIBLE,
                            18, 14, 320, 24, hwnd, nullptr, nullptr, nullptr);
            g_apagado_label = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE,
                                              18, 44, 320, 24, hwnd, (HMENU)200, nullptr, nullptr);
            SendMessageW(g_apagado_label, WM_SETFONT, (WPARAM)g_font_bold, TRUE);
            HWND cancel = CreateWindowExW(0, L"BUTTON", L"Cancelar apagado",
                                          WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                                          60, 80, 130, 28, hwnd, (HMENU)IDCANCEL, nullptr, nullptr);
            HWND ahora = CreateWindowExW(0, L"BUTTON", L"Apagar ahora",
                                         WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                                         198, 80, 110, 28, hwnd, (HMENU)IDOK, nullptr, nullptr);
            SendMessageW(cancel, WM_SETFONT, (WPARAM)g_font, TRUE);
            SendMessageW(ahora, WM_SETFONT, (WPARAM)g_font, TRUE);
            SetWindowTextW(g_apagado_label, L"El equipo se apagara en 30 s");
            SetTimer(hwnd, 1, 1000, nullptr);
            return 0;
        }
        case WM_ERASEBKGND: {
            RECT rc;
            GetClientRect(hwnd, &rc);
            FillRect((HDC)wp, &rc, g_brush_bg);
            return 1;
        }
        case WM_CTLCOLORSTATIC:
            SetBkMode((HDC)wp, TRANSPARENT);
            SetTextColor((HDC)wp, g_pal.text);
            return (LRESULT)g_brush_bg;
        case WM_DRAWITEM: {
            DRAWITEMSTRUCT* di = (DRAWITEMSTRUCT*)lp;
            if (di && (di->CtlID == IDOK || di->CtlID == IDCANCEL)) {
                dibujar_boton_tema(di);
                return TRUE;
            }
            break;
        }
        case WM_TIMER:
            if (wp == 1) {
                g_apagado_restante--;
                if (g_apagado_restante <= 0) {
                    programar_apagado(10);
                    DestroyWindow(hwnd);
                } else {
                    wchar_t buf[64];
                    swprintf_s(buf, L"El equipo se apagara en %d s", g_apagado_restante);
                    SetWindowTextW(g_apagado_label, buf);
                }
            }
            return 0;
        case WM_COMMAND:
            if (LOWORD(wp) == IDOK) {
                programar_apagado(10);
                DestroyWindow(hwnd);
            } else if (LOWORD(wp) == IDCANCEL) {
                DestroyWindow(hwnd);
            }
            return 0;
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            KillTimer(hwnd, 1);
            g_apagado_hwnd = nullptr;
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static void mostrar_apagado() {
    static bool registrada = false;
    if (!registrada) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = ApagadoWndProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr;
        wc.lpszClassName = L"ConversorApagadoWnd";
        RegisterClassExW(&wc);
        registrada = true;
    }
    if (g_apagado_hwnd) return;
    RECT pr{};
    GetWindowRect(g_hwnd, &pr);
    int ancho = 380, alto = 160;
    int x = pr.left + ((pr.right - pr.left) - ancho) / 2;
    int y = pr.top + ((pr.bottom - pr.top) - alto) / 2;
    g_apagado_hwnd = CreateWindowExW(WS_EX_TOPMOST, L"ConversorApagadoWnd",
                                     L"Apagado automatico",
                                     WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
                                     x, y, ancho, alto, g_hwnd, nullptr,
                                     GetModuleHandleW(nullptr), nullptr);
    SetForegroundWindow(g_apagado_hwnd);
    aplicar_modo_oscuro_nativo(g_apagado_hwnd, g_oscuro);
    InvalidateRect(g_apagado_hwnd, nullptr, TRUE);
}

// ---------------------------------------------------------------------------
//  Vista previa antes / despues
// ---------------------------------------------------------------------------
static Bitmap g_prev_orig;
static Bitmap g_prev_webp;
static std::wstring g_prev_ruta;
static std::wstring g_prev_info_orig;
static std::wstring g_prev_info_webp;
static std::wstring g_prev_etiqueta_der = L"WebP resultante (vista reducida)";
static HWND g_prev_hwnd = nullptr;
static HWND g_prev_quality = nullptr;
static HWND g_prev_quality_lbl = nullptr;
static int g_prev_calidad = 85;
static int g_prev_metodo = 6;
static bool g_prev_sin_perdida = false;
static bool g_prev_avif = false;
#define ID_PREV_CALIDAD 5001

static void pintar_damas(HDC dc, RECT r) {
    const int lado = 12;
    HBRUSH claro = CreateSolidBrush(g_oscuro ? RGB(34, 45, 61) : RGB(250, 250, 250));
    HBRUSH oscuro = CreateSolidBrush(g_oscuro ? RGB(24, 33, 47) : RGB(225, 228, 232));
    for (int y = r.top; y < r.bottom; y += lado) {
        for (int x = r.left; x < r.right; x += lado) {
            RECT c{x, y, std::min<LONG>(x + lado, r.right), std::min<LONG>(y + lado, r.bottom)};
            FillRect(dc, &c, (((x - r.left) / lado + (y - r.top) / lado) & 1) ? oscuro : claro);
        }
    }
    DeleteObject(claro);
    DeleteObject(oscuro);
}

static void dibujar_bitmap_escalado(HDC dc, const Bitmap& b, RECT panel) {
    if (b.w <= 0 || b.h <= 0 || b.rgba.empty()) return;
    pintar_damas(dc, panel);

    double escala = std::min((double)(panel.right - panel.left) / b.w,
                             (double)(panel.bottom - panel.top) / b.h);
    int dw = std::max(1, (int)lround(b.w * escala));
    int dh = std::max(1, (int)lround(b.h * escala));
    int dx = panel.left + ((panel.right - panel.left) - dw) / 2;
    int dy = panel.top + ((panel.bottom - panel.top) - dh) / 2;

    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = b.w;
    bi.bmiHeader.biHeight = -(LONG)b.h;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HDC mem = CreateCompatibleDC(dc);
    HBITMAP hb = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!hb || !bits) {
        if (hb) DeleteObject(hb);
        DeleteDC(mem);
        return;
    }
    uint8_t* dst = (uint8_t*)bits;
    for (size_t i = 0; i < (size_t)b.w * b.h; i++) {
        dst[i * 4 + 0] = b.rgba[i * 4 + 2];
        dst[i * 4 + 1] = b.rgba[i * 4 + 1];
        dst[i * 4 + 2] = b.rgba[i * 4 + 0];
        dst[i * 4 + 3] = b.alfa ? b.rgba[i * 4 + 3] : 255;
    }
    HGDIOBJ old = SelectObject(mem, hb);
    SetStretchBltMode(dc, HALFTONE);
    SetBrushOrgEx(dc, 0, 0, nullptr);
    BLENDFUNCTION bf{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    AlphaBlend(dc, dx, dy, dw, dh, mem, 0, 0, b.w, b.h, bf);
    SelectObject(mem, old);
    DeleteObject(hb);
    DeleteDC(mem);
}

static void actualizar_preview_calidad(HWND hwnd) {
    if (!g_prev_quality || g_prev_orig.w <= 0) return;
    int q = (int)SendMessageW(g_prev_quality, TBM_GETPOS, 0, 0);
    g_prev_calidad = std::clamp(q, 1, 100);
    std::wstring error, aviso;
    auto datos = codificar_objetivo(g_prev_orig, g_prev_calidad, g_prev_metodo,
                                    g_prev_sin_perdida, g_prev_orig.alfa, 0,
                                    g_prev_avif, aviso, error);
    if (datos.empty()) return;
    Bitmap convertida;
    bool ok = false;
    if (g_prev_avif) {
        ok = decodificar_avif_datos(datos, convertida, error);
    } else {
        int w = 0, h = 0;
        uint8_t* rgba = WebPDecodeRGBA(datos.data(), datos.size(), &w, &h);
        if (rgba) {
            convertida.w = w;
            convertida.h = h;
            convertida.alfa = true;
            convertida.rgba.assign(rgba, rgba + (size_t)w * h * 4);
            WebPFree(rgba);
            ok = true;
        }
    }
    if (!ok) return;
    g_prev_webp = std::move(convertida);
    wchar_t kb[64];
    swprintf_s(kb, L"%.1f KB", (double)datos.size() / 1024.0);
    g_prev_info_webp = std::wstring(kb) + L"  (" + std::to_wstring(g_prev_calidad) +
                       L"%, esfuerzo " + std::to_wstring(g_prev_metodo) + L")" +
                       (aviso.empty() ? L"" : (L"\n" + aviso));
    InvalidateRect(hwnd, nullptr, TRUE);
}

static LRESULT CALLBACK PreviewWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CREATE:
            g_prev_hwnd = hwnd;
            CreateWindowExW(0, L"STATIC", L"Calidad de prueba", WS_CHILD | WS_VISIBLE,
                            20, 631, 100, 20, hwnd, nullptr, nullptr, nullptr);
            g_prev_quality = CreateWindowExW(
                0, TRACKBAR_CLASSW, L"", WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_AUTOTICKS,
                120, 626, 360, 32, hwnd, (HMENU)ID_PREV_CALIDAD, nullptr, nullptr);
            SendMessageW(g_prev_quality, TBM_SETRANGE, TRUE, MAKELPARAM(1, 100));
            SendMessageW(g_prev_quality, TBM_SETPOS, TRUE, g_prev_calidad);
            g_prev_quality_lbl = CreateWindowExW(
                0, L"STATIC", L"", WS_CHILD | WS_VISIBLE, 492, 631, 80, 20, hwnd,
                nullptr, nullptr, nullptr);
            {
                wchar_t q[8];
                swprintf_s(q, L"%d", g_prev_calidad);
                SetWindowTextW(g_prev_quality_lbl, q);
            }
            return 0;
        case WM_HSCROLL:
            if ((HWND)lp == g_prev_quality) {
                wchar_t q[8];
                swprintf_s(q, L"%d", (int)SendMessageW(g_prev_quality, TBM_GETPOS, 0, 0));
                SetWindowTextW(g_prev_quality_lbl, q);
                SetTimer(hwnd, 2, 250, nullptr);
            }
            return 0;
        case WM_TIMER:
            if (wp == 2) {
                KillTimer(hwnd, 2);
                actualizar_preview_calidad(hwnd);
            }
            return 0;
        case WM_CTLCOLORSTATIC:
            SetBkMode((HDC)wp, TRANSPARENT);
            SetTextColor((HDC)wp, g_pal.text);
            return (LRESULT)g_brush_bg;
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(hwnd, &ps);
            RECT rc;
            GetClientRect(hwnd, &rc);
            HBRUSH fondo = CreateSolidBrush(g_pal.bg);
            FillRect(dc, &rc, fondo);
            DeleteObject(fondo);
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, g_pal.text);
            SelectObject(dc, g_font_bold);

            RECT r1{20, 12, 470, 30};
            RECT r2{490, 12, 940, 30};
            DrawTextW(dc, L"Original", -1, &r1, DT_LEFT | DT_SINGLELINE);
            DrawTextW(dc, g_prev_etiqueta_der.c_str(), -1, &r2, DT_LEFT | DT_SINGLELINE);

            RECT p1{20, 36, 470, 540};
            RECT p2{490, 36, 940, 540};
            HBRUSH blanco = CreateSolidBrush(g_pal.input);
            FillRect(dc, &p1, blanco);
            FillRect(dc, &p2, blanco);
            DeleteObject(blanco);
            FrameRect(dc, &p1, g_brush_border);
            FrameRect(dc, &p2, g_brush_border);
            dibujar_bitmap_escalado(dc, g_prev_orig, p1);
            dibujar_bitmap_escalado(dc, g_prev_webp, p2);

            SelectObject(dc, g_font);
            RECT i1{20, 550, 470, 620};
            RECT i2{490, 550, 940, 620};
            DrawTextW(dc, g_prev_info_orig.c_str(), -1, &i1,
                      DT_LEFT | DT_WORDBREAK);
            DrawTextW(dc, g_prev_info_webp.c_str(), -1, &i2,
                      DT_LEFT | DT_WORDBREAK);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            KillTimer(hwnd, 2);
            g_prev_hwnd = nullptr;
            g_prev_quality = nullptr;
            g_prev_quality_lbl = nullptr;
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static std::wstring primera_imagen_de(const std::wstring& ruta) {
    std::error_code ec;
    fs::path p(ruta);
    if (fs::is_regular_file(p, ec) && es_imagen(p)) return p.wstring();
    if (fs::is_directory(p, ec)) {
        for (fs::recursive_directory_iterator it(p, fs::directory_options::skip_permission_denied, ec), fin;
             it != fin; it.increment(ec)) {
            if (ec) { ec.clear(); continue; }
            std::error_code ec2;
            if (it->is_regular_file(ec2) && es_imagen(it->path())) return it->path().wstring();
        }
    }
    return L"";
}

static void abrir_vista_previa() {
    if (g_origenes.empty()) {
        MessageBoxW(g_hwnd, L"Anade o selecciona una imagen para ver la vista previa.",
                    APP_NAME, MB_ICONINFORMATION);
        return;
    }
    int sel = ListView_GetNextItem(g_lista_origen, -1, LVNI_SELECTED);
    if (sel < 0 || sel >= (int)g_origenes.size()) sel = 0;
    std::wstring ruta = primera_imagen_de(g_origenes[(size_t)sel]);
    if (ruta.empty()) {
        MessageBoxW(g_hwnd, L"La seleccion no contiene ninguna imagen compatible.",
                    APP_NAME, MB_ICONINFORMATION);
        return;
    }

    SetWindowTextW(g_status, L"Generando vista previa...");
    UpdateWindow(g_hwnd);

    try {
    ComPtr<IWICImagingFactory> factory;
    CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                     IID_PPV_ARGS(&factory));
    if (!factory.Get()) {
        SetWindowTextW(g_status, L"Listo.");
        MessageBoxW(g_hwnd, L"No se pudo iniciar el decodificador de imagenes (WIC).",
                    APP_NAME, MB_ICONERROR);
        return;
    }

    Bitmap original;
    std::wstring error;
    if (!decodificar_imagen(factory.Get(), ruta, original, error)) {
        MessageBoxW(g_hwnd, (L"No se pudo abrir la imagen:\n" + error).c_str(),
                    APP_NAME, MB_ICONERROR);
        SetWindowTextW(g_status, L"Listo.");
        return;
    }

    Options opt = leer_opciones_controles();
    Bitmap reducida;
    if (!redimensionar_lanczos(original, 1600, reducida, error)) reducida = original;
    std::wstring aviso;
    auto datos = codificar_objetivo(reducida, opt.calidad, opt.metodo, opt.sin_perdida,
                                    reducida.alfa, 0, opt.formato == L"AVIF", aviso, error);
    if (datos.empty()) {
        MessageBoxW(g_hwnd, (L"No se pudo generar el WebP de vista previa:\n" + error).c_str(),
                    APP_NAME, MB_ICONERROR);
        SetWindowTextW(g_status, L"Listo.");
        return;
    }

    Bitmap convertida;
    bool decodificada = false;
    if (opt.formato == L"AVIF") {
        decodificada = decodificar_avif_datos(datos, convertida, error);
    } else {
        int w = 0, h = 0;
        uint8_t* rgba = WebPDecodeRGBA(datos.data(), datos.size(), &w, &h);
        if (rgba) {
            convertida.w = w;
            convertida.h = h;
            convertida.alfa = true;
            convertida.rgba.assign(rgba, rgba + (size_t)w * h * 4);
            WebPFree(rgba);
            decodificada = true;
        }
    }
    if (!decodificada) {
        MessageBoxW(g_hwnd, L"Archivo generado, pero no se pudo decodificar para la vista previa.",
                    APP_NAME, MB_ICONERROR);
        SetWindowTextW(g_status, L"Listo.");
        return;
    }

    g_prev_orig = reducida;
    g_prev_webp = std::move(convertida);
    g_prev_ruta = fs::path(ruta).filename().wstring();
    g_prev_calidad = opt.calidad;
    g_prev_metodo = opt.metodo;
    g_prev_sin_perdida = opt.sin_perdida;
    g_prev_avif = opt.formato == L"AVIF";
    g_prev_etiqueta_der = (opt.formato == L"AVIF" ? L"AVIF" : L"WebP") +
                          std::wstring(L" resultante (vista reducida)");

    std::error_code ec;
    double kb_orig = (double)fs::file_size(ruta, ec) / 1024.0;
    wchar_t kb1[32], kb2[32];
    swprintf_s(kb1, L"%.1f KB", kb_orig);
    swprintf_s(kb2, L"%.1f KB", (double)datos.size() / 1024.0);
    g_prev_info_orig = g_prev_ruta + L"\n" + std::to_wstring(original.w) + L" x " +
                       std::to_wstring(original.h) + L" px\n" + kb1;
    std::wstring nota = aviso.empty() ? L"" : (L"\n" + aviso);
    g_prev_info_webp = std::wstring(kb2) + L"  (" +
                       std::to_wstring(opt.calidad) + L"%, esfuerzo " +
                       std::to_wstring(opt.metodo) + L")" + nota;

    static bool registrada = false;
    if (!registrada) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = PreviewWndProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr;
        wc.lpszClassName = L"ConversorPreviewWnd";
        RegisterClassExW(&wc);
        registrada = true;
    }
    if (g_prev_hwnd) DestroyWindow(g_prev_hwnd);
    RECT pr{};
    GetWindowRect(g_hwnd, &pr);
    int ancho = 980, alto = 720;
    int x = pr.left + ((pr.right - pr.left) - ancho) / 2;
    int y = pr.top + ((pr.bottom - pr.top) - alto) / 2;
    g_prev_hwnd = CreateWindowExW(WS_EX_TOOLWINDOW, L"ConversorPreviewWnd",
                                  (L"Vista previa - " + g_prev_ruta).c_str(),
                                  WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
                                  x, y, ancho, alto, g_hwnd, nullptr,
                                  GetModuleHandleW(nullptr), nullptr);
    aplicar_modo_oscuro_nativo(g_prev_hwnd, g_oscuro);
    InvalidateRect(g_prev_hwnd, nullptr, TRUE);
    SetWindowTextW(g_status, L"Vista previa lista.");
    } catch (const std::exception& ex) {
        SetWindowTextW(g_status, L"Listo.");
        MessageBoxW(g_hwnd, (L"No se pudo generar la vista previa:\n" + a_wstring(ex.what())).c_str(),
                    APP_NAME, MB_ICONERROR);
    } catch (...) {
        SetWindowTextW(g_status, L"Listo.");
        MessageBoxW(g_hwnd, L"No se pudo generar la vista previa.", APP_NAME, MB_ICONERROR);
    }
}

static void cargar_config_a_controles() {
    if (g_edit_salida) SetWindowTextW(g_edit_salida, g_opt.salida.c_str());
    if (g_edit_zip) SetWindowTextW(g_edit_zip, g_opt.zip_nombre.c_str());
    if (g_track_calidad) {
        SendMessageW(g_track_calidad, TBM_SETPOS, TRUE, g_opt.calidad);
        wchar_t b[8];
        swprintf_s(b, L"%d", g_opt.calidad);
        SetWindowTextW(g_lbl_calidad, b);
    }
    if (g_combo_metodo) SendMessageW(g_combo_metodo, CB_SETCURSEL, g_opt.metodo, 0);
    if (g_combo_dim) {
        int idx = 0;
        switch (g_opt.max_dim) {
            case 2560: idx = 1; break;
            case 1920: idx = 2; break;
            case 1600: idx = 3; break;
            case 1200: idx = 4; break;
            case 800: idx = 5; break;
            case 512: idx = 6; break;
            default: idx = 0; break;
        }
        SendMessageW(g_combo_dim, CB_SETCURSEL, idx, 0);
    }
    if (g_check_lossless) SendMessageW(g_check_lossless, BM_SETCHECK, g_opt.sin_perdida ? BST_CHECKED : BST_UNCHECKED, 0);
    if (g_check_reencode) SendMessageW(g_check_reencode, BM_SETCHECK, g_opt.reencode ? BST_CHECKED : BST_UNCHECKED, 0);
    if (g_check_estructura) SendMessageW(g_check_estructura, BM_SETCHECK, g_opt.mantener ? BST_CHECKED : BST_UNCHECKED, 0);
    if (g_check_sobrescribir) SendMessageW(g_check_sobrescribir, BM_SETCHECK, g_opt.sobrescribir ? BST_CHECKED : BST_UNCHECKED, 0);
    if (g_check_zip) SendMessageW(g_check_zip, BM_SETCHECK, g_opt.zip ? BST_CHECKED : BST_UNCHECKED, 0);
    if (g_check_csv) SendMessageW(g_check_csv, BM_SETCHECK, g_opt.csv ? BST_CHECKED : BST_UNCHECKED, 0);
    g_ui_beep = g_opt.beep;
    if (g_check_beep) InvalidateRect(g_check_beep, nullptr, TRUE);
    if (g_edit_objetivo) {
        wchar_t kb[16];
        swprintf_s(kb, L"%d", g_opt.objetivo_kb > 0 ? g_opt.objetivo_kb : 150);
        SetWindowTextW(g_edit_objetivo, kb);
    }
    if (g_edit_patron) SetWindowTextW(g_edit_patron, g_opt.patron.c_str());
    if (g_check_dry) SendMessageW(g_check_dry, BM_SETCHECK, g_opt.dry_run ? BST_CHECKED : BST_UNCHECKED, 0);
    if (g_check_flatten) SendMessageW(g_check_flatten, BM_SETCHECK, g_opt.flatten ? BST_CHECKED : BST_UNCHECKED, 0);
    g_ui_apagar = g_opt.apagar;
    if (g_check_apagar) InvalidateRect(g_check_apagar, nullptr, TRUE);
    g_color = g_opt.color;
    if (g_btn_color) InvalidateRect(g_btn_color, nullptr, TRUE);
    if (g_btn_color) EnableWindow(g_btn_color, g_opt.flatten);
    if (g_track_calidad) EnableWindow(g_track_calidad, !g_opt.sin_perdida);
    if (g_combo_metodo) EnableWindow(g_combo_metodo, !g_opt.sin_perdida);
    if (g_edit_objetivo) EnableWindow(g_edit_objetivo, g_opt.objetivo_kb > 0);
    if (g_check_zip) EnableWindow(g_check_zip, !g_opt.dry_run);
    if (g_edit_zip) EnableWindow(g_edit_zip, g_opt.zip && !g_opt.dry_run);
    refrescar_combo_preset();
}

static LRESULT CALLBACK TipWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(hwnd, &ps);
            RECT rc;
            GetClientRect(hwnd, &rc);
            HBRUSH fondo = CreateSolidBrush(RGB(31, 41, 51));
            FillRect(dc, &rc, fondo);
            DeleteObject(fondo);
            FrameRect(dc, &rc, (HBRUSH)GetStockObject(BLACK_BRUSH));
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, RGB(248, 250, 252));
            HFONT vieja = (HFONT)SelectObject(dc, g_font_tip);
            RECT texto{9, 6, rc.right - 9, rc.bottom - 6};
            DrawTextW(dc, g_tip_texto_actual.c_str(), -1, &texto,
                      DT_LEFT | DT_WORDBREAK | DT_NOPREFIX);
            SelectObject(dc, vieja);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_NCHITTEST:
            return HTTRANSPARENT;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static void ocultar_tooltip() {
    if (g_tip_hwnd && IsWindowVisible(g_tip_hwnd)) ShowWindow(g_tip_hwnd, SW_HIDE);
}

static void mostrar_tooltip_rect(RECT rc, const std::wstring& texto) {
    if (!g_tip_hwnd) {
        g_tip_hwnd = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE,
                                     L"ConversorTipWnd", L"", WS_POPUP,
                                     0, 0, 10, 10, g_hwnd, nullptr,
                                     GetModuleHandleW(nullptr), nullptr);
        if (!g_tip_hwnd) return;
    }
    g_tip_texto_actual = texto;
    HDC dc = GetDC(g_tip_hwnd);
    HFONT vieja = (HFONT)SelectObject(dc, g_font_tip);
    RECT medida{0, 0, 360, 0};
    DrawTextW(dc, texto.c_str(), -1, &medida,
              DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
    SelectObject(dc, vieja);
    ReleaseDC(g_tip_hwnd, dc);
    int ancho = std::min(360, (int)medida.right) + 18;
    int alto = (int)medida.bottom + 12;
    int x = rc.left + 8;
    int y = rc.bottom + 10;
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    if (x + ancho > sw - 12) x = std::max(6, sw - ancho - 12);
    if (y + alto > sh - 12) y = std::max(6, (int)rc.top - alto - 8);
    SetWindowPos(g_tip_hwnd, HWND_TOPMOST, x, y, ancho, alto,
                 SWP_NOACTIVATE | SWP_SHOWWINDOW);
    InvalidateRect(g_tip_hwnd, nullptr, TRUE);
}

static void mostrar_tooltip(HWND control, const std::wstring& texto) {
    RECT rc{};
    GetWindowRect(control, &rc);
    mostrar_tooltip_rect(rc, texto);
}

static RECT zona_drop_actual(void);

static void actualizar_tooltip() {
    POINT p{};
    GetCursorPos(&p);
    POINT pc = p;
    ScreenToClient(g_hwnd, &pc);
    RECT zona_drop = zona_drop_actual();
    bool drop_hot = WindowFromPoint(p) == g_hwnd && PtInRect(&zona_drop, pc);
    if (drop_hot != g_drop_hot) {
        g_drop_hot = drop_hot;
        InvalidateRect(g_hwnd, &zona_drop, FALSE);
    }
    if (GetAsyncKeyState(VK_LBUTTON) & 0x8000) {
        ocultar_tooltip();
        g_tip_control = nullptr;
    }
    HWND w = WindowFromPoint(p);
    std::wstring texto;
    if (w == g_hwnd) {
        POINT c = p;
        ScreenToClient(g_hwnd, &c);
        RECT drop = zona_drop_actual();
        if (PtInRect(&drop, c)) texto = g_tip_drop_texto;
    } else {
        auto it = g_tips.find(w);
        if (it != g_tips.end()) texto = it->second;
    }
    if (texto.empty()) {
        ocultar_tooltip();
        g_tip_control = nullptr;
        return;
    }
    if (w != g_tip_control) {
        ocultar_tooltip();
        g_tip_control = w;
        g_tip_inicio = GetTickCount();
        return;
    }
    if (!IsWindowVisible(g_tip_hwnd) &&
        GetTickCount() - g_tip_inicio >= 450) {
        if (w == g_hwnd) {
            RECT r = zona_drop_actual();
            POINT pts[2] = {{r.left, r.top}, {r.right, r.bottom}};
            MapWindowPoints(g_hwnd, HWND_DESKTOP, pts, 2);
            RECT rs{pts[0].x, pts[0].y, pts[1].x, pts[1].y};
            mostrar_tooltip_rect(rs, texto);
        } else {
            mostrar_tooltip(w, texto);
        }
    }
}

static void crear_tooltips() {
    g_tip_drop_texto = L"Arrastra aquí imágenes, carpetas o comprimidos desde el Explorador.";
    auto add = [&](HWND control, const wchar_t* texto) {
        if (!control) return;
        g_tips[control] = texto;
    };

    add(GetDlgItem(g_hwnd, ID_ADD_FILES), L"Añade una o varias imágenes al lote.");
    add(GetDlgItem(g_hwnd, ID_ADD_FOLDER), L"Añade una carpeta completa, incluidas sus subcarpetas.");
    add(GetDlgItem(g_hwnd, ID_REMOVE), L"Quita de la lista las entradas seleccionadas (tecla Supr).");
    add(GetDlgItem(g_hwnd, ID_CLEAR), L"Vacía la lista de origen sin tocar tus archivos.");
    add(g_lista_origen, L"Orígenes añadidos. Selecciona varios con Ctrl o Mayús y pulsa Supr para quitarlos.");
    add(g_lbl_count, L"Resumen de carpetas, comprimidos e imágenes añadidos.");
    add(g_edit_salida, L"Carpeta donde se guardarán los archivos convertidos.");
    add(GetDlgItem(g_hwnd, ID_BROWSE), L"Elige la carpeta de salida.");
    add(g_btn_archive, L"Añade archivos .zip, .rar o .7z y convierte su contenido.");
    add(g_btn_pattern_help, L"Ver los marcadores disponibles para el nombre de salida.");
    add(GetDlgItem(g_hwnd, ID_TEMA), L"Cambia entre modo claro y modo oscuro.");
    add(g_lbl_extractor, L"Herramienta usada para abrir .rar y .7z (7-Zip o WinRAR).");
    add(g_check_estructura, L"Reproduce las subcarpetas del origen en la carpeta de salida.");
    add(g_check_sobrescribir, L"Activado reemplaza archivos existentes; desactivado los omite.");
    add(g_check_zip, L"Crea además un ZIP con todos los archivos generados.");
    add(g_edit_zip, L"Nombre del ZIP que se creará en la carpeta de salida.");
    add(g_combo_formato, L"Formato de salida: WebP (compatible) o AVIF (pesa menos).");
    add(g_check_csv, L"Guarda un CSV con el peso original, final y el ahorro de cada archivo.");
    add(g_edit_patron, L"Patrón de nombre. Vacío conserva el original. Ej.: {nombre}_{ancho}x{alto}.");
    add(g_check_dry, L"Calcula el peso estimado sin escribir ninguna imagen.");
    add(g_check_fecha, L"Conserva la fecha de modificación original en el archivo convertido.");
    add(g_combo_preset, L"Aplica una combinación guardada de calidad y tamaño.");
    add(g_btn_save_preset, L"Guarda los ajustes actuales como preset propio.");
    add(g_btn_del_preset, L"Borra el preset propio seleccionado.");
    add(g_track_calidad, L"Calidad de 1 a 100. Más calidad = mejor aspecto y más peso.");
    add(g_lbl_calidad, L"Valor de calidad actual.");
    add(g_combo_metodo, L"Esfuerzo 0-6: más esfuerzo comprime mejor pero tarda más.");
    add(g_check_lossless, L"Sin pérdida: fidelidad exacta, archivos más grandes.");
    add(g_check_reencode, L"Vuelve a convertir archivos que ya están en el formato de destino.");
    add(g_check_flatten, L"Rellena la transparencia con un color sólido.");
    add(g_btn_color, L"Elige el color de fondo al aplanar la transparencia.");
    add(g_combo_dim, L"Reduce el lado mayor a este tamaño; nunca amplía imágenes pequeñas.");
    add(GetDlgItem(g_hwnd, ID_TARGET), L"Ajusta la calidad automáticamente para no superar el peso indicado.");
    add(g_edit_objetivo, L"Peso máximo por imagen, en KB (mínimo 5).");
    add(g_btn_convertir, L"Inicia la conversión con los ajustes actuales (Ctrl+Enter o F5).");
    add(g_btn_cancelar, L"Detiene el proceso después del archivo en curso.");
    add(g_btn_pausa, L"Pausa o reanuda la cola sin cancelar el trabajo.");
    add(g_check_beep, L"Pitido al terminar: agudo si todo va bien, grave si hay errores.");
    add(g_check_apagar, L"Apaga el equipo al terminar una conversión correcta, con cuenta atrás cancelable.");
    add(GetDlgItem(g_hwnd, ID_OPEN), L"Abre el Explorador en la carpeta de salida.");
    add(g_lista_resultados, L"Resultados del último trabajo. Doble clic para abrir un archivo.");
    add(g_tab, L"Resultados en tabla y registro detallado del proceso.");
    add(g_edit_log, L"Registro detallado del proceso.");
}

static void dibujar_cards(HDC dc, RECT rc) {
    FillRect(dc, &rc, g_brush_bg);
    if (g_oscuro) {
        RECT barra{0, 0, rc.right, 18};
        FillRect(dc, &barra, g_brush_bg);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, g_pal.text);
        HFONT vieja = (HFONT)SelectObject(dc, g_font);
        RECT r1{16, 0, 110, 18};
        RECT r2{118, 0, 180, 18};
        RECT r3{188, 0, 240, 18};
        RECT zonas[3] = {r1, r2, r3};
        if (g_menu_hot >= 0 && g_menu_hot < 3) {
            HBRUSH resalte = CreateSolidBrush(g_pal.hover);
            FillRect(dc, &zonas[g_menu_hot], resalte);
            DeleteObject(resalte);
        }
        DrawTextW(dc, L"Herramientas", -1, &r1, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        DrawTextW(dc, L"Historial", -1, &r2, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        DrawTextW(dc, L"Ayuda", -1, &r3, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        SelectObject(dc, vieja);

        // Pestanas propias (la nativa no admite modo oscuro fiable)
        RECT rtab1{16, 728, 122, 751};
        RECT rtab2{126, 728, 208, 751};
        RECT rtab_zonas[2] = {rtab1, rtab2};
        const wchar_t* nombres[2] = {L"Resultados", L"Registro"};
        for (int i = 0; i < 2; i++) {
            HBRUSH fondo = CreateSolidBrush(i == g_tab_sel ? g_pal.card : g_pal.bg);
            FillRect(dc, &rtab_zonas[i], fondo);
            DeleteObject(fondo);
            SetTextColor(dc, i == g_tab_sel ? g_pal.accent_dark : g_pal.muted);
            HFONT fuente = (HFONT)SelectObject(dc, i == g_tab_sel ? g_font_bold : g_font);
            RECT texto = rtab_zonas[i];
            DrawTextW(dc, nombres[i], -1, &texto,
                      DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            SelectObject(dc, fuente);
        }
        RECT linea{16, 750, rc.right - 16, 752};
        FillRect(dc, &linea, g_brush_border);
        if (g_tab_sel >= 0 && g_tab_sel < 2) {
            RECT sub = rtab_zonas[g_tab_sel];
            sub.left += 10;
            sub.right -= 10;
            sub.top = 749;
            sub.bottom = 752;
            HBRUSH acento = CreateSolidBrush(g_pal.accent);
            FillRect(dc, &sub, acento);
            DeleteObject(acento);
        }
    }
    int leftR = 492 + (rc.right - 1000) / 2;
    RECT cards[] = {{16, 60, rc.right - 16, 285},
                    {16, 294, leftR, 618},
                    {leftR + 8, 294, rc.right - 16, 618}};
    for (auto& r : cards) {
        FillRect(dc, &r, g_brush_card);
    }
    // Zona de arrastre
    RECT zona_drop{28, 98, rc.right - 28, 161};
    HBRUSH relleno_drop =
        CreateSolidBrush(g_drop_hot ? g_pal.drop_hot : g_pal.drop);
    FillRect(dc, &zona_drop, relleno_drop);
    DeleteObject(relleno_drop);
    LOGBRUSH lb{BS_SOLID, g_drop_hot ? g_pal.accent : g_pal.border, 0};
    HPEN pen = ExtCreatePen(PS_GEOMETRIC | PS_DASH, g_drop_hot ? 2 : 1, &lb, 0, nullptr);
    HGDIOBJ old = SelectObject(dc, pen);
    HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(NULL_BRUSH));
    Rectangle(dc, zona_drop.left, zona_drop.top, zona_drop.right, zona_drop.bottom);
    SelectObject(dc, oldBrush);
    SelectObject(dc, old);
    DeleteObject(pen);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, g_drop_hot ? g_pal.text : g_pal.muted);
    RECT r1{zona_drop.left, 108, zona_drop.right, 132};
    RECT r2{zona_drop.left, 132, zona_drop.right, 152};
    HFONT fuente_vieja =
        (HFONT)SelectObject(dc, g_drop_hot ? g_font_drop_hot : g_font_drop);
    DrawTextW(dc, L"Arrastra aqui tus imagenes, carpetas o comprimidos .zip / .rar", -1,
              &r1, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
    SelectObject(dc, g_font_muted);
    SetTextColor(dc, g_pal.muted);
    DrawTextW(dc, L"Archivos, carpetas, .zip, .rar o .7z  ·  tambien puedes hacer clic aqui", -1,
              &r2, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
    SelectObject(dc, fuente_vieja);
}

static int g_slider_min = 1, g_slider_max = 100, g_slider_val = 85;

static LRESULT CALLBACK SliderWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(hwnd, &ps);
            RECT rc;
            GetClientRect(hwnd, &rc);
            HBRUSH fondo = CreateSolidBrush(g_pal.card);
            FillRect(dc, &rc, fondo);
            DeleteObject(fondo);
            int x0 = 8, x1 = rc.right - 8;
            int y = rc.bottom / 2;
            RECT canal{x0, y - 2, x1, y + 3};
            HBRUSH gris = CreateSolidBrush(g_pal.border);
            FillRect(dc, &canal, gris);
            DeleteObject(gris);
            double t = (double)(g_slider_val - g_slider_min) /
                       std::max(1, g_slider_max - g_slider_min);
            int x = x0 + (int)lround(t * (x1 - x0));
            RECT pulgar{x - 6, y - 10, x + 6, y + 11};
            HBRUSH pulgar_brush = CreateSolidBrush(g_pal.input);
            FillRect(dc, &pulgar, pulgar_brush);
            DeleteObject(pulgar_brush);
            FrameRect(dc, &pulgar, g_brush_border);
            if (GetFocus() == hwnd) DrawFocusRect(dc, &pulgar);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;
        case WM_LBUTTONDOWN: {
            SetFocus(hwnd);
            SetCapture(hwnd);
            RECT rc;
            GetClientRect(hwnd, &rc);
            int x0 = 8, x1 = rc.right - 8;
            int x = std::clamp((int)GET_X_LPARAM(lp), x0, x1);
            int nuevo = g_slider_min + (int)lround((double)(x - x0) / std::max(1, x1 - x0) *
                                                   (g_slider_max - g_slider_min));
            nuevo = std::clamp(nuevo, g_slider_min, g_slider_max);
            if (nuevo != g_slider_val) {
                g_slider_val = nuevo;
                InvalidateRect(hwnd, nullptr, FALSE);
                SendMessageW(GetParent(hwnd), WM_HSCROLL, SB_THUMBTRACK, (LPARAM)hwnd);
            }
            return 0;
        }
        case WM_MOUSEMOVE:
            if (GetCapture() == hwnd) {
                RECT rc;
                GetClientRect(hwnd, &rc);
                int x0 = 8, x1 = rc.right - 8;
                int x = std::clamp((int)GET_X_LPARAM(lp), x0, x1);
                int nuevo = g_slider_min + (int)lround((double)(x - x0) /
                                                       std::max(1, x1 - x0) *
                                                       (g_slider_max - g_slider_min));
                nuevo = std::clamp(nuevo, g_slider_min, g_slider_max);
                if (nuevo != g_slider_val) {
                    g_slider_val = nuevo;
                    InvalidateRect(hwnd, nullptr, FALSE);
                    SendMessageW(GetParent(hwnd), WM_HSCROLL, SB_THUMBTRACK, (LPARAM)hwnd);
                }
            }
            return 0;
        case WM_LBUTTONUP:
            if (GetCapture() == hwnd) ReleaseCapture();
            return 0;
        case WM_KEYDOWN: {
            int nuevo = g_slider_val;
            if (wp == VK_LEFT || wp == VK_DOWN) nuevo--;
            else if (wp == VK_RIGHT || wp == VK_UP) nuevo++;
            else if (wp == VK_PRIOR) nuevo += 10;
            else if (wp == VK_NEXT) nuevo -= 10;
            else if (wp == VK_HOME) nuevo = g_slider_min;
            else if (wp == VK_END) nuevo = g_slider_max;
            nuevo = std::clamp(nuevo, g_slider_min, g_slider_max);
            if (nuevo != g_slider_val) {
                g_slider_val = nuevo;
                InvalidateRect(hwnd, nullptr, FALSE);
                SendMessageW(GetParent(hwnd), WM_HSCROLL, SB_THUMBTRACK, (LPARAM)hwnd);
            }
            return 0;
        }
        case WM_GETDLGCODE:
            return DLGC_WANTARROWS;
        case WM_SETFOCUS:
        case WM_KILLFOCUS:
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        case TBM_SETRANGE:
            g_slider_min = (int)LOWORD(lp);
            g_slider_max = (int)HIWORD(lp);
            if (g_slider_max <= g_slider_min) g_slider_max = g_slider_min + 1;
            g_slider_val = std::clamp(g_slider_val, g_slider_min, g_slider_max);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        case TBM_SETPOS:
            g_slider_val = std::clamp((int)lp, g_slider_min, g_slider_max);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        case TBM_GETPOS:
            return g_slider_val;
        case TBM_GETRANGEMIN:
            return g_slider_min;
        case TBM_GETRANGEMAX:
            return g_slider_max;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static LRESULT CALLBACK ScrollWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    auto dibujar = [&]() {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        HBRUSH fondo = CreateSolidBrush(g_pal.card);
        FillRect(dc, &rc, fondo);
        DeleteObject(fondo);
        int w = rc.right;
        int alto = rc.bottom;
        int lado = std::min(16, alto / 3);
        HBRUSH gris = CreateSolidBrush(g_pal.muted);
        POINT arriba[3] = {{w / 4, lado - 5}, {3 * w / 4, lado - 5}, {w / 2, 5}};
        POINT abajo[3] = {{w / 4, alto - lado + 5}, {3 * w / 4, alto - lado + 5},
                          {w / 2, alto - 5}};
        HGDIOBJ viejo = SelectObject(dc, gris);
        Polygon(dc, arriba, 3);
        Polygon(dc, abajo, 3);
        SelectObject(dc, viejo);
        DeleteObject(gris);

        SCROLLINFO si{};
        si.cbSize = sizeof(si);
        si.fMask = SIF_ALL;
        GetScrollInfo(hwnd, SB_CTL, &si);
        int rango = std::max(1, si.nMax - si.nMin + 1);
        int pista = std::max(1, alto - lado * 2);
        int tam = std::max(18, (int)((double)pista * si.nPage / rango));
        if (tam > pista) tam = pista;
        double t = (double)(si.nPos - si.nMin) / std::max(1.0, (double)(rango - si.nPage));
        int y = lado + (int)lround(t * std::max(0, pista - tam));
        RECT pulgar{2, y, w - 2, y + tam};
        HBRUSH pulgar_brush = CreateSolidBrush(g_pal.hover);
        FillRect(dc, &pulgar, pulgar_brush);
        DeleteObject(pulgar_brush);
        FrameRect(dc, &pulgar, g_brush_border);
        EndPaint(hwnd, &ps);
    };
    switch (msg) {
        case WM_PAINT:
            dibujar();
            return 0;
        case WM_ERASEBKGND:
            return 1;
        case WM_LBUTTONDOWN: {
            RECT rc;
            GetClientRect(hwnd, &rc);
            int alto = rc.bottom;
            int lado = std::min(16, alto / 3);
            int y = GET_Y_LPARAM(lp);
            if (y < lado)
                SendMessageW(GetParent(hwnd), WM_VSCROLL, SB_LINEUP, (LPARAM)hwnd);
            else if (y > alto - lado)
                SendMessageW(GetParent(hwnd), WM_VSCROLL, SB_LINEDOWN, (LPARAM)hwnd);
            else {
                SCROLLINFO si{};
                si.cbSize = sizeof(si);
                si.fMask = SIF_ALL;
                GetScrollInfo(hwnd, SB_CTL, &si);
                int rango = std::max(1, si.nMax - si.nMin + 1);
                int pista = std::max(1, alto - lado * 2);
                int tam = std::max(18, (int)((double)pista * si.nPage / rango));
                if (tam > pista) tam = pista;
                double t = (double)(si.nPos - si.nMin) / std::max(1.0, (double)(rango - si.nPage));
                int pos_pulgar = lado + (int)lround(t * std::max(0, pista - tam));
                if (y < pos_pulgar)
                    SendMessageW(GetParent(hwnd), WM_VSCROLL, SB_PAGEUP, (LPARAM)hwnd);
                else if (y > pos_pulgar + tam)
                    SendMessageW(GetParent(hwnd), WM_VSCROLL, SB_PAGEDOWN, (LPARAM)hwnd);
            }
            return 0;
        }
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static void dibujar_check_plano(const DRAWITEMSTRUCT* di, bool marcado) {
    FillRect(di->hDC, &di->rcItem, g_brush_bg);
    int alto = di->rcItem.bottom - di->rcItem.top;
    RECT caja{di->rcItem.left + 2, di->rcItem.top + 2,
              di->rcItem.left + 18, di->rcItem.bottom - 2};
    int estado = marcado ? CBS_CHECKEDNORMAL : CBS_UNCHECKEDNORMAL;
    if (di->itemState & ODS_DISABLED)
        estado = marcado ? CBS_CHECKEDDISABLED : CBS_UNCHECKEDDISABLED;
    else if (di->itemState & ODS_SELECTED)
        estado = marcado ? CBS_CHECKEDPRESSED : CBS_UNCHECKEDPRESSED;
    bool pintado = false;
    HTHEME tema = OpenThemeData(di->hwndItem, L"Button");
    if (tema) {
        SIZE tam{};
        if (SUCCEEDED(GetThemePartSize(tema, di->hDC, BP_CHECKBOX, estado, nullptr,
                                       TS_TRUE, &tam)) && tam.cx > 0) {
            int lado = std::min((int)tam.cx, alto - 4);
            caja.left = di->rcItem.left + 2;
            caja.top = di->rcItem.top + (alto - lado) / 2;
            caja.right = caja.left + lado;
            caja.bottom = caja.top + lado;
        }
        pintado = SUCCEEDED(DrawThemeBackground(tema, di->hDC, BP_CHECKBOX, estado,
                                                &caja, nullptr));
        CloseThemeData(tema);
    }
    if (!pintado) {
        DrawFrameControl(di->hDC, &caja, DFC_BUTTON,
                         DFCS_BUTTONCHECK | (marcado ? DFCS_CHECKED : 0) |
                         ((di->itemState & ODS_DISABLED) ? DFCS_INACTIVE : 0));
    }
    SetBkMode(di->hDC, TRANSPARENT);
    SetTextColor(di->hDC, (di->itemState & ODS_DISABLED) ? g_pal.muted
                                                         : g_pal.text);
    HFONT vieja = (HFONT)SelectObject(di->hDC, g_font);
    wchar_t texto[128]{};
    GetWindowTextW(di->hwndItem, texto, 128);
    RECT zona = di->rcItem;
    zona.left = caja.right + 6;
    DrawTextW(di->hDC, texto, -1, &zona,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(di->hDC, vieja);
}

static RECT zona_drop_actual() {
    RECT rc{1000, 841, 0, 0};
    if (g_hwnd) GetClientRect(g_hwnd, &rc);
    if (rc.right < 1000) rc.right = 1000;
    return RECT{28, 98, rc.right - 28, 161};
}

static void reubicar_controles(int cw, int ch) {
    if (!g_ui_lista) return;
    if (cw < 1000) cw = 1000;
    if (ch < 841) ch = 841;
    int dx = cw - 1000;
    int leftR = 492 + dx / 2;
    int cardL = leftR + 8;
    int cardR = 984 + dx;
    auto ctl = [](int c) { return GetDlgItem(g_hwnd, c); };
    auto mover = [](HWND h, int x, int y, int w, int hh) {
        if (h) MoveWindow(h, x, y, w, hh, TRUE);
    };

    // Cabecera
    mover(ctl(1402), cw - 440, 23, 390, 20);
    mover(ctl(ID_TEMA), cw - 42, 20, 26, 26);

    // 1. Origen
    mover(ctl(ID_ADD_FILES), 534 + dx, 68, 74, 28);
    mover(ctl(ID_ADD_FOLDER), 616 + dx, 68, 63, 28);
    mover(g_btn_archive, 687 + dx, 68, 82, 28);
    mover(ctl(ID_REMOVE), 778 + dx, 68, 60, 28);
    mover(ctl(ID_CLEAR), 846 + dx, 68, 106, 28);
    mover(g_lista_origen, 28, 164, cw - 76, 72);
    if (g_lista_origen) ListView_SetColumnWidth(g_lista_origen, 0, cw - 90);
    mover(g_scroll_origen, cw - 48, 164, 16, 72);
    mover(ctl(ID_LBL_EXTRACTOR), cw - 300, 244, 268, 18);

    // 2. Destino
    mover(g_edit_salida, 28, 338, leftR - 132, 26);
    mover(ctl(ID_BROWSE), leftR - 96, 338, 76, 26);
    mover(g_edit_zip, 240, 433, leftR - 262, 24);
    mover(g_edit_patron, 124, 522, leftR - 172, 24);
    mover(g_btn_pattern_help, leftR - 42, 520, 30, 24);

    // 3. Calidad y opciones
    mover(ctl(1200), cardL + 13, 313, 300, 20);
    mover(ctl(1206), cardL + 13, 333, 50, 20);
    mover(g_combo_preset, cardL + 58, 331, cardR - cardL - 274, 240);
    mover(g_btn_save_preset, cardR - 210, 331, 70, 24);
    mover(g_btn_del_preset, cardR - 136, 331, 68, 24);
    mover(g_track_calidad, cardL + 13, 360, cardR - cardL - 175, 22);
    mover(ctl(1201), cardR - 152, 372, 32, 20);
    mover(ctl(1202), cardR - 126, 372, 58, 20);
    mover(g_combo_metodo, cardR - 66, 370, 52, 200);
    mover(g_check_lossless, cardL + 13, 390, 300, 20);
    mover(g_check_reencode, cardL + 13, 413, 300, 20);
    mover(g_check_flatten, cardL + 13, 434, 220, 20);
    mover(g_btn_color, cardL + 240, 434, 28, 20);
    mover(ctl(1203), cardL + 13, 462, 100, 20);
    mover(g_combo_dim, cardL + 119, 460, 118, 200);
    mover(ctl(1204), cardL + 243, 464, 160, 18);
    mover(ctl(ID_TARGET), cardL + 13, 489, 110, 20);
    mover(g_edit_objetivo, cardL + 126, 487, 51, 24);
    mover(ctl(1205), cardL + 184, 491, 120, 18);

    // Acciones
    int openX = cw - 146;
    mover(ctl(ID_OPEN), openX, 616, 124, 32);
    mover(g_check_beep, openX - 208, 621, 190, 20);
    mover(g_check_apagar, openX - 380, 621, 160, 20);

    // Progreso y estado
    mover(g_progress, 16, 680, cw - 32, 20);
    mover(g_status, 16, 708, cw - 32, 18);

    // Pestañas y resultados
    mover(g_tab, 16, 728, cw - 32, ch - 729);
    mover(g_lista_resultados, 24, 752, cw - 48, ch - 759);
    mover(g_edit_log, 24, 752, cw - 48, ch - 759);
    if (g_lista_resultados)
        ListView_SetColumnWidth(g_lista_resultados, 0, 556 + dx);

    InvalidateRect(g_hwnd, nullptr, TRUE);
}

// ---------------------------------------------------------------------------
//  Ventana "Acerca de" (mismo contenido que la version Python)
// ---------------------------------------------------------------------------
static HWND g_acerca = nullptr;
static HWND g_acerca_rich = nullptr;
static HWND g_acerca_titulo = nullptr;
static HWND g_acerca_sub = nullptr;
static HWND g_acerca_btn_manual = nullptr;
static HWND g_acerca_btn_cerrar = nullptr;
static const int ID_ACERCA_MANUAL = 5001;
static const int ID_ACERCA_CERRAR = 5002;

enum class EstiloAcerca { H1, H2, P, LI, CODE, MUTED };

struct BloqueAcerca {
    std::wstring texto;
    EstiloAcerca estilo;
};

static std::vector<BloqueAcerca> bloques_acerca() {
    std::vector<BloqueAcerca> b;
    auto h1 = [&](const wchar_t* t) { b.push_back({std::wstring(t) + L"\r", EstiloAcerca::H1}); };
    auto h2 = [&](const wchar_t* t) { b.push_back({std::wstring(t) + L"\r", EstiloAcerca::H2}); };
    auto p = [&](const wchar_t* t) { b.push_back({std::wstring(t) + L"\r", EstiloAcerca::P}); };
    auto li = [&](const std::wstring& t) {
        b.push_back({L"\x2022  " + t + L"\r", EstiloAcerca::LI});
    };
    auto code = [&](const wchar_t* t) { b.push_back({std::wstring(t) + L"\r", EstiloAcerca::CODE}); };

    h1(L"Qué hace");
    li(L"Convierte JPG, PNG, BMP, TIFF, GIF, ICO, WebP, AVIF y HEIC/HEIF a formato WebP.");
    li(L"Trabaja con imágenes sueltas, carpetas completas y comprimidos .zip / .rar / .7z.");
    li(L"Arrastra y suelta desde el Explorador de Windows en cualquier punto de la ventana.");
    li(L"Guarda el resultado en la carpeta que elijas y, si quieres, lo empaqueta en un .zip.");
    li(L"Todo el proceso es local: no se envía nada a internet. Los originales nunca se modifican.");

    h2(L"Opciones principales");
    li(L"Presets: Web, Catálogo, Iconos sin pérdida y Máxima calidad, o guarda los tuyos.");
    li(L"Calidad 1-100 (85 por defecto): más calidad, más peso. 85-92 es lo ideal para web.");
    li(L"Esfuerzo 0-6 (6 por defecto): cuanto más esfuerzo, archivos más pequeños.");
    li(L"Sin pérdida (lossless): fidelidad exacta al original, archivos más grandes.");
    li(L"Recodificar también los .webp / .avif: por defecto se saltan los del formato destino.");
    li(L"Aplanar transparencia sobre un color: rellena el fondo alfa de los PNG.");
    li(L"Mantener subcarpetas del origen: reproduce la estructura de carpetas original.");
    li(L"Sobrescribir archivos existentes: si se desactiva, los ya convertidos se omiten.");
    li(L"Empaquetar el resultado en .zip: crea un ZIP con todos los archivos generados.");
    li(L"Lado máximo: reduce a 2560, 1920, 1600, 1200, 800 o 512 px sin ampliar las pequeñas.");
    li(L"Peso máximo: ajusta la calidad automáticamente para no superar los KB indicados.");
    li(L"Formato de salida: WebP o AVIF (AVIF suele pesar menos con la misma calidad).");
    li(L"Informe CSV: deja en la carpeta de salida una tabla con peso original, final y ahorro.");
    li(L"Aviso sonoro al terminar: tono agudo si todo fue bien, grave si hubo errores.");
    li(L"Apagar al terminar: apaga el equipo tras una conversión correcta, con cuenta atrás cancelable.");

    h2(L"Historial y atajos");
    li(L"El menú Historial guarda los últimos 10 trabajos; al elegir uno se recuperan origen, destino y ajustes.");
    li(L"Ctrl+O añade archivos, Ctrl+Mayús+O añade una carpeta, Ctrl+Enter o F5 convierten.");
    li(L"Supr quita las entradas seleccionadas de la lista de origen (selección múltiple con Ctrl/Mayús).");
    li(L"F1 abre el manual de uso.");

    h2(L"Comprimidos");
    p(L"Los .zip se abren con el lector interno. Los .rar y .7z necesitan 7-Zip o WinRAR: el programa "
      L"los detecta e indica cuál usa. El contenido se extrae a una carpeta temporal que se borra al "
      L"terminar. Si añades varios comprimidos, cada uno se guarda en una subcarpeta con su nombre.");

    h2(L"Qué se conserva");
    li(L"La orientación de las fotos (etiqueta EXIF), para que no salgan giradas.");
    li(L"La transparencia de PNG, GIF y WebP, salvo que se active el aplanado.");
    li(L"El perfil de color ICC, si la imagen lo lleva incrustado.");
    li(L"Los GIF animados se convierten usando el primer fotograma (se avisa en el registro).");
    li(L"Los metadatos EXIF y la ubicación GPS no se copian, para que los archivos pesen menos.");

    h2(L"Nombres y duplicados");
    p(L"Se conserva el nombre y solo cambia la extensión: foto.jpg pasa a foto.webp. Si dos originales "
      L"darían el mismo nombre (foto.jpg y foto.png), el segundo recibe un sufijo: foto_png.webp. "
      L"Nunca se pisa un archivo distinto.");

    h2(L"Modo consola");
    code(L"ConversorWebPC.exe <origen...> -o <carpeta> [-q 85] [-m 6] [--zip salida.zip]");
    p(L"Opciones: --format avif, --max-dim 1920, --max-kb 150, --pattern {nombre}_{ancho}, "
      L"--dry-run, --flat --bg \"#ffffff\", --csv, --lossless, --no-recursive, --no-overwrite.");
    p(L"Sin -o, el programa abre la ventana con los archivos ya cargados (útil para 'Enviar a').");

    h2(L"Entorno de este equipo");
    li(L"Backend: WIC + libwebp + libavif + lcms2.");
    li(L"Arrastrar y soltar: disponible (Win32).");
    std::wstring extractor = buscar_extractor();
    li(extractor.empty() ? L"Extractor de comprimidos: ninguno (solo .zip)."
                         : L"Extractor de comprimidos: " +
                               fs::path(extractor).filename().wstring() + L".");

    h2(L"Consejos");
    li(L"Para un lote grande, deja esfuerzo 6 y vete a hacer otra cosa: puedes cancelar cuando quieras.");
    li(L"Si repites una conversión, desactiva 'Sobrescribir' para reanudar sin repetir trabajo.");
    li(L"El ZIP del resultado no sustituye a los WebP: ambos quedan en la carpeta de salida.");
    li(L"El manual completo, con todas las opciones explicadas, está en Ayuda - Manual de uso (F1).");

    b.push_back({L"\rConversor WebP " + std::wstring(APP_VERSION) +
                     L"  ·  Hecho para Windows  ·  Sin telemetría ni conexiones externas.\r",
                 EstiloAcerca::MUTED});
    return b;
}

static void rellenar_acerca(HWND rich) {
    auto bloques = bloques_acerca();
    std::wstring completo;
    std::vector<std::pair<CHARRANGE, EstiloAcerca>> rangos;
    for (auto& bl : bloques) {
        LONG ini = (LONG)completo.size();
        completo += bl.texto;
        CHARRANGE cr{ini, (LONG)completo.size()};
        rangos.emplace_back(cr, bl.estilo);
    }
    SendMessageW(rich, WM_SETREDRAW, FALSE, 0);
    SendMessageW(rich, WM_SETTEXT, 0, (LPARAM)completo.c_str());
    SendMessageW(rich, EM_SETBKGNDCOLOR, 0, (LPARAM)g_pal.card);
    SendMessageW(rich, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN,
                 MAKELPARAM(16, 12));

    for (auto& par : rangos) {
        CHARRANGE cr = par.first;
        EstiloAcerca estilo = par.second;
        SendMessageW(rich, EM_EXSETSEL, 0, (LPARAM)&cr);

        CHARFORMAT2W cf{};
        cf.cbSize = sizeof(cf);
        cf.dwMask = CFM_FACE | CFM_SIZE | CFM_COLOR | CFM_BOLD | CFM_BACKCOLOR;
        wcscpy_s(cf.szFaceName, L"Segoe UI");
        cf.yHeight = 180;
        cf.crTextColor = g_pal.text;
        cf.dwEffects = 0;
        cf.crBackColor = g_pal.card;
        if (estilo == EstiloAcerca::H1) {
            cf.yHeight = 260;
            cf.dwEffects = CFE_BOLD;
        } else if (estilo == EstiloAcerca::H2) {
            cf.yHeight = 200;
            cf.dwEffects = CFE_BOLD;
            cf.crTextColor = g_pal.accent_dark;
        } else if (estilo == EstiloAcerca::CODE) {
            wcscpy_s(cf.szFaceName, L"Consolas");
            cf.crTextColor = g_pal.text;
            cf.crBackColor = g_pal.input;
        } else if (estilo == EstiloAcerca::MUTED) {
            cf.yHeight = 160;
            cf.crTextColor = g_pal.muted;
        }
        SendMessageW(rich, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);

        PARAFORMAT2 pf{};
        pf.cbSize = sizeof(pf);
        pf.dwMask = PFM_STARTINDENT | PFM_OFFSET | PFM_SPACEBEFORE | PFM_SPACEAFTER;
        pf.dxStartIndent = 0;
        pf.dxOffset = 0;
        pf.dySpaceBefore = 0;
        pf.dySpaceAfter = 40;
        if (estilo == EstiloAcerca::H1) {
            pf.dySpaceBefore = 260;
            pf.dySpaceAfter = 60;
        } else if (estilo == EstiloAcerca::H2) {
            pf.dySpaceBefore = 240;
            pf.dySpaceAfter = 50;
        } else if (estilo == EstiloAcerca::LI) {
            pf.dxStartIndent = 280;
            pf.dxOffset = -280;
        }
        SendMessageW(rich, EM_SETPARAFORMAT, 0, (LPARAM)&pf);
    }
    SendMessageW(rich, EM_SETSEL, 0, 0);
    SendMessageW(rich, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(rich, nullptr, TRUE);
}

static void layout_acerca(HWND h) {
    RECT rc{};
    GetClientRect(h, &rc);
    int cw = rc.right;
    int ch = rc.bottom;
    int arriba = 80;
    int abajo = ch - 64;
    MoveWindow(g_acerca_titulo, 18, 14, 500, 30, TRUE);
    MoveWindow(g_acerca_sub, 18, 48, cw - 36, 18, TRUE);
    MoveWindow(g_acerca_rich, 19, arriba + 1, cw - 38, abajo - arriba - 2, TRUE);
    int y = ch - 46;
    MoveWindow(g_acerca_btn_cerrar, cw - 18 - 88, y, 88, 28, TRUE);
    MoveWindow(g_acerca_btn_manual, cw - 18 - 88 - 8 - 150, y, 150, 28, TRUE);
}

static LRESULT CALLBACK AcercaRichProc(HWND h, UINT m, WPARAM w, LPARAM l,
                                       UINT_PTR, DWORD_PTR) {
    if (m == WM_KEYDOWN && w == VK_ESCAPE) {
        DestroyWindow(GetParent(h));
        return 0;
    }
    return DefSubclassProc(h, m, w, l);
}

static LRESULT CALLBACK AcercaWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CREATE: {
            HINSTANCE inst = GetModuleHandleW(nullptr);
            g_acerca_titulo = CreateWindowExW(0, L"STATIC", APP_NAME,
                                              WS_CHILD | WS_VISIBLE, 18, 14, 500, 30,
                                              hwnd, nullptr, inst, nullptr);
            SendMessageW(g_acerca_titulo, WM_SETFONT, (WPARAM)g_font_title, TRUE);
            std::wstring sub = L"Versión " + std::wstring(APP_VERSION) +
                               L"  ·  Conversión local de imágenes a WebP  ·  Windows";
            g_acerca_sub = CreateWindowExW(0, L"STATIC", sub.c_str(),
                                           WS_CHILD | WS_VISIBLE, 18, 48, 500, 18,
                                           hwnd, nullptr, inst, nullptr);
            SendMessageW(g_acerca_sub, WM_SETFONT, (WPARAM)g_font_muted, TRUE);

            LoadLibraryW(L"Msftedit.dll");
            g_acerca_rich = CreateWindowExW(0, MSFTEDIT_CLASS, L"",
                                            WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE |
                                                ES_READONLY | ES_NOHIDESEL,
                                            19, 81, 700, 500, hwnd, nullptr, inst, nullptr);
            SendMessageW(g_acerca_rich, WM_SETFONT, (WPARAM)g_font, TRUE);
            SetWindowSubclass(g_acerca_rich, AcercaRichProc, 1, 0);

            g_acerca_btn_manual = CreateWindowExW(0, L"BUTTON", L"Abrir manual de uso",
                                                  WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                                                  500, 620, 150, 28, hwnd,
                                                  (HMENU)(INT_PTR)ID_ACERCA_MANUAL, inst, nullptr);
            g_acerca_btn_cerrar = CreateWindowExW(0, L"BUTTON", L"Cerrar",
                                                  WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                                                  660, 620, 88, 28, hwnd,
                                                  (HMENU)(INT_PTR)ID_ACERCA_CERRAR, inst, nullptr);
            SendMessageW(g_acerca_btn_manual, WM_SETFONT, (WPARAM)g_font, TRUE);
            SendMessageW(g_acerca_btn_cerrar, WM_SETFONT, (WPARAM)g_font, TRUE);

            rellenar_acerca(g_acerca_rich);
            layout_acerca(hwnd);
            return 0;
        }
        case WM_SIZE:
            layout_acerca(hwnd);
            return 0;
        case WM_GETMINMAXINFO: {
            MINMAXINFO* mm = (MINMAXINFO*)lp;
            RECT r{0, 0, 660, 480};
            AdjustWindowRectEx(&r, WS_OVERLAPPEDWINDOW, FALSE, 0);
            mm->ptMinTrackSize.x = r.right - r.left;
            mm->ptMinTrackSize.y = r.bottom - r.top;
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(hwnd, &ps);
            RECT rc{};
            GetClientRect(hwnd, &rc);
            FillRect(dc, &rc, g_brush_bg);
            RECT caja{18, 80, rc.right - 18, rc.bottom - 64};
            FillRect(dc, &caja, g_brush_card);
            FrameRect(dc, &caja, g_brush_border);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_CTLCOLORSTATIC: {
            SetBkMode((HDC)wp, TRANSPARENT);
            SetTextColor((HDC)wp, g_pal.text);
            return (LRESULT)g_brush_bg;
        }
        case WM_DRAWITEM: {
            DRAWITEMSTRUCT* di = (DRAWITEMSTRUCT*)lp;
            if (di && (di->CtlID == ID_ACERCA_MANUAL || di->CtlID == ID_ACERCA_CERRAR)) {
                dibujar_boton_tema(di);
                return TRUE;
            }
            break;
        }
        case WM_COMMAND:
            if (LOWORD(wp) == ID_ACERCA_MANUAL) {
                SendMessageW(g_hwnd, WM_COMMAND, MAKEWPARAM(ID_MENU_MANUAL, BN_CLICKED), 0);
                return 0;
            }
            if (LOWORD(wp) == ID_ACERCA_CERRAR) {
                DestroyWindow(hwnd);
                return 0;
            }
            break;
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            g_acerca = nullptr;
            g_acerca_rich = nullptr;
            g_acerca_titulo = nullptr;
            g_acerca_sub = nullptr;
            g_acerca_btn_manual = nullptr;
            g_acerca_btn_cerrar = nullptr;
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static void mostrar_acerca(HWND padre) {
    if (g_acerca && IsWindow(g_acerca)) {
        ShowWindow(g_acerca, SW_RESTORE);
        SetForegroundWindow(g_acerca);
        return;
    }
    HINSTANCE inst = GetModuleHandleW(nullptr);
    static bool registrada = false;
    if (!registrada) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = AcercaWndProc;
        wc.hInstance = inst;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hIcon = (HICON)LoadImageW(inst, MAKEINTRESOURCEW(101), IMAGE_ICON, 0, 0,
                                     LR_DEFAULTSIZE);
        wc.hIconSm = (HICON)LoadImageW(inst, MAKEINTRESOURCEW(101), IMAGE_ICON, 16, 16, 0);
        wc.lpszClassName = L"ConversorAcerca";
        RegisterClassExW(&wc);
        registrada = true;
    }
    RECT marco{0, 0, 780, 680};
    AdjustWindowRectEx(&marco, WS_OVERLAPPEDWINDOW, FALSE, 0);
    g_acerca = CreateWindowExW(0, L"ConversorAcerca", L"Acerca de Conversor WebP",
                               WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                               marco.right - marco.left, marco.bottom - marco.top,
                               padre, nullptr, inst, nullptr);
    if (g_acerca) {
        ShowWindow(g_acerca, SW_SHOW);
        UpdateWindow(g_acerca);
        SetForegroundWindow(g_acerca);
        aplicar_modo_oscuro_nativo(g_acerca, g_oscuro);
        InvalidateRect(g_acerca, nullptr, TRUE);
    }
}

static LRESULT dibujar_header_resultados(NMCUSTOMDRAW* cd) {
    if (cd->dwDrawStage == CDDS_PREPAINT) return CDRF_NOTIFYITEMDRAW;
    if (cd->dwDrawStage != CDDS_ITEMPREPAINT) return CDRF_DODEFAULT;
    FillRect(cd->hdc, &cd->rc, g_brush_bg);
    wchar_t texto[128]{};
    HDITEMW item{};
    item.mask = HDI_TEXT | HDI_FORMAT;
    item.pszText = texto;
    item.cchTextMax = 128;
    if (Header_GetItem(cd->hdr.hwndFrom, (int)cd->dwItemSpec, &item)) {
        SetBkMode(cd->hdc, TRANSPARENT);
        SetTextColor(cd->hdc, g_pal.text);
        HFONT vieja = (HFONT)SelectObject(cd->hdc, g_font);
        RECT r = cd->rc;
        r.left += 6;
        UINT formato = DT_SINGLELINE | DT_VCENTER | DT_LEFT;
        if (item.fmt & HDF_CENTER) formato = DT_SINGLELINE | DT_VCENTER | DT_CENTER;
        else if (item.fmt & HDF_RIGHT) formato = DT_SINGLELINE | DT_VCENTER | DT_RIGHT;
        DrawTextW(cd->hdc, texto, -1, &r, formato | DT_NOPREFIX);
        SelectObject(cd->hdc, vieja);
    }
    return CDRF_SKIPDEFAULT;
}

static LRESULT CALLBACK ResultadosSubclass(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp,
                                           UINT_PTR, DWORD_PTR) {
    if (msg == WM_NOTIFY) {
        NMHDR* nh = (NMHDR*)lp;
        if (g_oscuro && nh->code == NM_CUSTOMDRAW &&
            nh->hwndFrom == ListView_GetHeader(hwnd))
            return dibujar_header_resultados((NMCUSTOMDRAW*)lp);
    }
    return DefSubclassProc(hwnd, msg, wp, lp);
}

static LRESULT CALLBACK ProgressSubclass(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp,
                                        UINT_PTR, DWORD_PTR) {
    if (g_oscuro) {
        if (msg == WM_ERASEBKGND) return 1;
        if (msg == WM_PAINT) {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(hwnd, &ps);
            RECT rc;
            GetClientRect(hwnd, &rc);
            FillRect(dc, &rc, g_brush_border);
            PBRANGE rango{};
            SendMessageW(hwnd, PBM_GETRANGE, TRUE, (LPARAM)&rango);
            int valor = (int)SendMessageW(hwnd, PBM_GETPOS, 0, 0);
            int total = rango.iHigh - rango.iLow;
            if (total > 0 && valor > rango.iLow) {
                int ancho = (int)((double)(valor - rango.iLow) / total * rc.right);
                if (ancho > rc.right) ancho = rc.right;
                RECT lleno{rc.left, rc.top, rc.left + ancho, rc.bottom};
                HBRUSH br = CreateSolidBrush(g_pal.accent);
                FillRect(dc, &lleno, br);
                DeleteObject(br);
            }
            EndPaint(hwnd, &ps);
            return 0;
        }
    }
    return DefSubclassProc(hwnd, msg, wp, lp);
}

static int zona_menu_custom(POINT p) {
    if (!g_oscuro || p.y < 0 || p.y >= 18) return -1;
    if (p.x >= 12 && p.x < 114) return 0;
    if (p.x >= 114 && p.x < 184) return 1;
    if (p.x >= 184 && p.x < 244) return 2;
    return -1;
}

static int zona_tab_custom(POINT p) {
    if (!g_oscuro || p.y < 726 || p.y >= 754) return -1;
    if (p.x >= 16 && p.x < 122) return 0;
    if (p.x >= 126 && p.x < 208) return 1;
    return -1;
}

static void seleccionar_pestana(int sel) {
    g_tab_sel = sel == 1 ? 1 : 0;
    if (g_tab) TabCtrl_SetCurSel(g_tab, g_tab_sel);
    if (g_lista_resultados)
        ShowWindow(g_lista_resultados, g_tab_sel == 0 ? SW_SHOW : SW_HIDE);
    if (g_edit_log) ShowWindow(g_edit_log, g_tab_sel == 1 ? SW_SHOW : SW_HIDE);
    RECT barra{0, 724, 400, 756};
    InvalidateRect(g_hwnd, &barra, FALSE);
}

static void abrir_menu_custom(int cual, POINT pantalla) {
    HMENU menu = cual == 0 ? g_menu_herramientas
                           : (cual == 1 ? g_menu_hist : g_menu_ayuda);
    if (!menu) return;
    SetForegroundWindow(g_hwnd);
    int id = (int)TrackPopupMenu(menu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN,
                                 pantalla.x, pantalla.y, 0, g_hwnd, nullptr);
    if (id) PostMessageW(g_hwnd, WM_COMMAND, (WPARAM)id, 0);
    PostMessageW(g_hwnd, WM_NULL, 0, 0);
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CREATE: {
            g_hwnd = hwnd;
            aplicar_paleta_cpp(g_oscuro);

            WNDCLASSEXW wc_slider{};
            wc_slider.cbSize = sizeof(wc_slider);
            wc_slider.style = CS_HREDRAW | CS_VREDRAW;
            wc_slider.lpfnWndProc = SliderWndProc;
            wc_slider.hInstance = GetModuleHandleW(nullptr);
            wc_slider.hCursor = LoadCursorW(nullptr, IDC_ARROW);
            wc_slider.lpszClassName = L"ConversorSlider";
            RegisterClassExW(&wc_slider);

            WNDCLASSEXW wc_scroll{};
            wc_scroll.cbSize = sizeof(wc_scroll);
            wc_scroll.style = CS_HREDRAW | CS_VREDRAW;
            wc_scroll.lpfnWndProc = ScrollWndProc;
            wc_scroll.hInstance = GetModuleHandleW(nullptr);
            wc_scroll.hCursor = LoadCursorW(nullptr, IDC_ARROW);
            wc_scroll.lpszClassName = L"ConversorScroll";
            RegisterClassExW(&wc_scroll);

            WNDCLASSEXW wc_tip{};
            wc_tip.cbSize = sizeof(wc_tip);
            wc_tip.lpfnWndProc = TipWndProc;
            wc_tip.hInstance = GetModuleHandleW(nullptr);
            wc_tip.hCursor = LoadCursorW(nullptr, IDC_ARROW);
            wc_tip.lpszClassName = L"ConversorTipWnd";
            RegisterClassExW(&wc_tip);
            DragAcceptFiles(hwnd, TRUE);

            g_font = CreateFontW(-12, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
                                 OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                 DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            g_font_bold = CreateFontW(-12, 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET,
                                      OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                      DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            g_font_title = CreateFontW(-21, 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET,
                                       OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                       DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            g_font_section = CreateFontW(-13, 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET,
                                         OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                         DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            g_font_muted = CreateFontW(-11, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
                                       OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                       DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            g_font_start = CreateFontW(-14, 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET,
                                       OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                       DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            g_font_tip = CreateFontW(-11, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
                                     OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                     DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            g_font_drop = CreateFontW(-12, 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET,
                                      OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                      DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            g_font_drop_hot = CreateFontW(-13, 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET,
                                          OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                          DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            g_font_mono = CreateFontW(-12, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
                                      OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                      FIXED_PITCH | FF_MODERN, L"Consolas");

            // Cabecera
            etiqueta(L"Conversor WebP", 16, 17, 300, 30, 1400, g_font_title);
            etiqueta(L"v2.9.0", 200, 29, 120, 18, 1401, g_font_muted);
            etiqueta(L"JPG · PNG · BMP · TIFF · GIF · ICO  →  WebP / AVIF", 560, 23, 390, 20,
                     1402, g_font_muted, SS_RIGHT);
            boton(L"", 958, 20, 26, 26, ID_TEMA, BS_OWNERDRAW);

            // 1. Origen
            etiqueta(L"1. Origen", 28, 69, 200, 20, ID_SECTION_ORIGEN, g_font_section);
            boton(L"Archivos...", 534, 68, 74, 28, ID_ADD_FILES);
            boton(L"Carpeta...", 616, 68, 63, 28, ID_ADD_FOLDER);
            g_btn_archive = boton(L".zip / .rar...", 687, 68, 82, 28, ID_ADD_ARCHIVE);
            boton(L"Quitar", 778, 68, 60, 28, ID_REMOVE);
            boton(L"Quitar todo", 846, 68, 106, 28, ID_CLEAR);

            g_lista_origen = listview(28, 164, 924, 72, ID_RESULTS + 100);
            ListView_SetExtendedListViewStyle(g_lista_origen, LVS_EX_FULLROWSELECT);
            ListView_SetBkColor(g_lista_origen, g_pal.card);
            ListView_SetTextBkColor(g_lista_origen, g_pal.card);
            ListView_SetTextColor(g_lista_origen, g_pal.text);
            SendMessageW(g_lista_origen, WM_SETFONT, (WPARAM)g_font_mono, TRUE);
            ShowScrollBar(g_lista_origen, SB_VERT, FALSE);
            ShowScrollBar(g_lista_origen, SB_HORZ, FALSE);
            g_scroll_origen = crear(L"ConversorScroll", L"", WS_TABSTOP,
                                    952, 164, 16, 72, ID_SCROLL_ORIGEN);
            SCROLLINFO si_inicial{};
            si_inicial.cbSize = sizeof(si_inicial);
            si_inicial.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
            si_inicial.nMin = 0;
            si_inicial.nMax = 100;
            si_inicial.nPage = 20;
            SetScrollInfo(g_scroll_origen, SB_CTL, &si_inicial, TRUE);
            LVCOLUMNW col{};
            col.mask = LVCF_TEXT | LVCF_WIDTH;
            col.pszText = const_cast<wchar_t*>(L"Origen");
            col.cx = 910;
            ListView_InsertColumn(g_lista_origen, 0, &col);
            SetWindowLongW(g_lista_origen, GWL_STYLE,
                           GetWindowLongW(g_lista_origen, GWL_STYLE) | LVS_NOCOLUMNHEADER);
            g_lbl_count = etiqueta(L"Sin origen seleccionado", 28, 244, 500, 18, 1102);
            g_lbl_extractor = etiqueta(L"", 700, 244, 268, 18, ID_LBL_EXTRACTOR,
                                       g_font_muted, SS_RIGHT);

            // 2. Destino
            etiqueta(L"2. Carpeta de salida", 28, 313, 300, 20, 1100, g_font_section);
            g_edit_salida = edit(28, 338, 360, 26, ID_OUTPUT);
            boton(L"Examinar...", 396, 338, 76, 26, ID_BROWSE);
            g_check_estructura = check(L"Mantener subcarpetas del origen", 28, 378, 300, 20, ID_STRUCT, g_opt.mantener);
            g_check_sobrescribir = check(L"Sobrescribir archivos existentes", 28, 401, 300, 20, ID_OVERWRITE, g_opt.sobrescribir);
            g_check_zip = check(L"Empaquetar el resultado en .zip", 28, 435, 180, 20, ID_ZIP, g_opt.zip);
            g_edit_zip = edit(240, 433, 230, 24, ID_ZIPNAME, g_opt.zip_nombre.c_str());
            etiqueta(L"Formato de salida", 28, 463, 110, 20, 1101);
            g_combo_formato = combo(136, 461, 80, 200, ID_FORMAT);
            SendMessageW(g_combo_formato, CB_ADDSTRING, 0, (LPARAM)L"WebP");
            SendMessageW(g_combo_formato, CB_ADDSTRING, 0, (LPARAM)L"AVIF");
            SendMessageW(g_combo_formato, CB_SETCURSEL,
                         g_opt.formato == L"AVIF" ? 1 : 0, 0);
            etiqueta(L"AVIF pesa menos", 224, 465, 180, 18, 1104, g_font_muted);
            g_check_csv = check(L"Guardar informe CSV en la carpeta de salida", 28, 497, 430, 20,
                                ID_CSV, g_opt.csv);
            etiqueta(L"Nombre de salida", 28, 524, 100, 20, 1103);
            g_edit_patron = edit(124, 522, 320, 24, ID_PATRON, g_opt.patron.c_str());
            g_btn_pattern_help = boton(L"?", 450, 520, 30, 24, ID_PATTERN_HELP);
            g_check_dry = check(L"Solo analizar (no escribir archivos)", 28, 587, 300, 20,
                                ID_DRY, g_opt.dry_run);
            g_check_fecha = check(L"Conservar fecha original", 28, 564, 300, 20,
                                  ID_FECHA, g_opt.preservar_fecha);

            // 3. Calidad
            etiqueta(L"3. Calidad y opciones", 513, 313, 300, 20, 1200, g_font_section);
            etiqueta(L"Preset", 513, 333, 50, 20, 1206);
            g_combo_preset = combo(558, 331, 210, 240, ID_PRESET);
            g_btn_save_preset = boton(L"Guardar", 774, 331, 70, 24, ID_SAVE_PRESET);
            g_btn_del_preset = boton(L"Borrar", 848, 331, 68, 24, ID_DEL_PRESET);

            g_track_calidad = crear(L"ConversorSlider", L"", WS_TABSTOP,
                                    513, 360, 310, 22, ID_CALITY);
            SendMessageW(g_track_calidad, TBM_SETRANGE, TRUE, MAKELPARAM(1, 100));
            SendMessageW(g_track_calidad, TBM_SETPOS, TRUE, g_opt.calidad);
            g_lbl_calidad = etiqueta(L"85", 832, 372, 32, 20, 1201);
            etiqueta(L"Esfuerzo", 858, 372, 58, 20, 1202);
            g_combo_metodo = combo(918, 370, 52, 200, ID_METHOD);
            for (int i = 0; i <= 6; i++) {
                wchar_t b[8];
                swprintf_s(b, L"%d", i);
                SendMessageW(g_combo_metodo, CB_ADDSTRING, 0, (LPARAM)b);
            }
            SendMessageW(g_combo_metodo, CB_SETCURSEL, g_opt.metodo, 0);
            g_check_lossless = check(L"Sin perdida (lossless)", 513, 390, 300, 20,
                                     ID_LOSSLESS, g_opt.sin_perdida);
            g_check_reencode = check(L"Recodificar tambien los .webp / .avif", 513, 413, 300, 20,
                                     ID_REENCODE, g_opt.reencode);
            g_check_flatten = check(L"Aplanar transparencia sobre", 513, 434, 220, 20,
                                    ID_FLATTEN, g_opt.flatten);
            g_btn_color = boton(L"", 740, 434, 28, 20, ID_COLOR, BS_OWNERDRAW);
            etiqueta(L"Lado maximo", 513, 462, 100, 20, 1203);
            g_combo_dim = combo(619, 460, 118, 200, ID_DIM);
            for (auto t : {L"Sin cambio", L"2560", L"1920", L"1600", L"1200", L"800", L"512"})
                SendMessageW(g_combo_dim, CB_ADDSTRING, 0, (LPARAM)t);
            SendMessageW(g_combo_dim, CB_SETCURSEL, 0, 0);
            etiqueta(L"px  ·  nunca amplia", 743, 464, 160, 18, 1204, g_font_muted);
            check(L"Peso maximo", 513, 489, 110, 20, ID_TARGET, g_opt.objetivo_kb > 0);
            g_edit_objetivo = edit(626, 487, 51, 24, ID_TARGET_EDIT);
            wchar_t kb[16];
            swprintf_s(kb, L"%d", g_opt.objetivo_kb > 0 ? g_opt.objetivo_kb : 150);
            SetWindowTextW(g_edit_objetivo, kb);
            etiqueta(L"KB por imagen", 684, 491, 120, 18, 1205, g_font_muted);

            // Acciones
            g_btn_convertir = boton(L"Convertir a WebP", 16, 630, 160, 42, ID_START, BS_OWNERDRAW);
            SendMessageW(g_btn_convertir, WM_SETFONT, (WPARAM)g_font_start, TRUE);
            g_btn_cancelar = boton(L"Cancelar", 192, 619, 82, 32, ID_CANCEL);
            EnableWindow(g_btn_cancelar, FALSE);
            g_btn_pausa = boton(L"Pausar", 280, 619, 82, 32, ID_PAUSA);
            EnableWindow(g_btn_pausa, FALSE);
            g_ui_apagar = g_opt.apagar;
            g_ui_beep = g_opt.beep;
            g_check_apagar = crear(L"BUTTON", L"Apagar al terminar",
                                   BS_OWNERDRAW | WS_TABSTOP, 474, 621, 160, 20, ID_APAGAR);
            g_check_beep = crear(L"BUTTON", L"Aviso sonoro al terminar",
                                 BS_OWNERDRAW | WS_TABSTOP, 646, 621, 190, 20, ID_BEEP);
            boton(L"Abrir carpeta de salida", 854, 616, 124, 32, ID_OPEN);

            g_progress = crear(PROGRESS_CLASSW, L"", PBS_SMOOTH, 16, 680, 968, 20, 1300);
            SetWindowSubclass(g_progress, ProgressSubclass, 1, 0);
            SendMessageW(g_progress, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
            SendMessageW(g_progress, PBM_SETBARCOLOR, 0, (LPARAM)g_pal.accent);
            SendMessageW(g_progress, PBM_SETBKCOLOR, 0, (LPARAM)g_pal.border);
            g_status = etiqueta(L"Listo.", 16, 708, 800, 18, 1403, g_font_muted);

            // Pestañas
            g_tab = crear(WC_TABCONTROLW, L"", WS_TABSTOP, 16, 728, 968, 112, ID_TAB);
            TCITEMW tci{};
            tci.mask = TCIF_TEXT;
            tci.pszText = const_cast<wchar_t*>(L"Resultados");
            TabCtrl_InsertItem(g_tab, 0, &tci);
            tci.pszText = const_cast<wchar_t*>(L"Registro");
            TabCtrl_InsertItem(g_tab, 1, &tci);

            g_lista_resultados = listview(24, 752, 952, 82, ID_RESULTS);
            SetWindowSubclass(g_lista_resultados, ResultadosSubclass, 1, 0);
            const wchar_t* cols[] = {L"Archivo", L"Original (KB)", L"Final (KB)", L"Ahorro %", L"Estado"};
            int anchos[] = {556, 100, 90, 72, 114};
            for (int i = 0; i < 5; i++) {
                LVCOLUMNW c{};
                c.mask = LVCF_TEXT | LVCF_WIDTH;
                c.pszText = const_cast<wchar_t*>(cols[i]);
                c.cx = anchos[i];
                ListView_InsertColumn(g_lista_resultados, i, &c);
            }
            ListView_SetExtendedListViewStyle(g_lista_resultados, LVS_EX_FULLROWSELECT);
            ShowScrollBar(g_lista_resultados, SB_VERT, TRUE);
            ShowScrollBar(g_lista_resultados, SB_HORZ, FALSE);
            ListView_SetBkColor(g_lista_resultados, g_pal.card);
            ListView_SetTextBkColor(g_lista_resultados, g_pal.card);
            ListView_SetTextColor(g_lista_resultados, g_pal.text);

            g_edit_log = crear(L"EDIT", L"", ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL |
                               WS_VSCROLL, 24, 752, 952, 82, ID_LOG);
            SendMessageW(g_edit_log, WM_SETFONT, (WPARAM)g_font_mono, TRUE);
            ShowWindow(g_edit_log, SW_HIDE);

            cargar_config_a_controles();
            refrescar_origenes();
            crear_tooltips();
            g_timer = SetTimer(hwnd, 1, 100, nullptr);
            g_ui_lista = true;
            RECT rc_ini{};
            GetClientRect(hwnd, &rc_ini);
            reubicar_controles(rc_ini.right, rc_ini.bottom);
            return 0;
        }

        case WM_SIZE:
            if (wp != SIZE_MINIMIZED)
                reubicar_controles(LOWORD(lp), HIWORD(lp));
            return 0;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(hwnd, &ps);
            RECT rc;
            GetClientRect(hwnd, &rc);
            HDC mem = CreateCompatibleDC(dc);
            HBITMAP bmp = CreateCompatibleBitmap(dc, rc.right, rc.bottom);
            HGDIOBJ old = SelectObject(mem, bmp);
            dibujar_cards(mem, rc);
            BitBlt(dc, 0, 0, rc.right, rc.bottom, mem, 0, 0, SRCCOPY);
            SelectObject(mem, old);
            DeleteObject(bmp);
            DeleteDC(mem);
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_CTLCOLORSTATIC: {
            SetBkMode((HDC)wp, TRANSPARENT);
            int id = GetDlgCtrlID((HWND)lp);
            if (id == ID_LOG) {
                SetTextColor((HDC)wp, g_pal.log_fg);
                SetBkColor((HDC)wp, g_pal.log_bg);
                return (LRESULT)g_brush_log;
            }
            if (id == ID_SECTION_ORIGEN || id == 1100 || id == 1200) {
                SetTextColor((HDC)wp, g_pal.accent_dark);
                return (LRESULT)g_brush_card;
            }
            if (id == 1104 || id == 1204 || id == 1205 || id == 1401 ||
                id == 1402 || id == 1403 || id == ID_LBL_EXTRACTOR) {
                SetTextColor((HDC)wp, g_pal.muted);
            } else {
                SetTextColor((HDC)wp, g_pal.text);
            }
            if (id >= 1400) return (LRESULT)g_brush_bg;
            return (LRESULT)g_brush_card;
        }
        case WM_CTLCOLORBTN: {
            int id = GetDlgCtrlID((HWND)lp);
            if (id == ID_BEEP || id == ID_APAGAR) return (LRESULT)g_brush_bg;
            return (LRESULT)g_brush_card;
        }
        case WM_CTLCOLORLISTBOX:
            SetTextColor((HDC)wp, g_pal.text);
            SetBkColor((HDC)wp, g_pal.input);
            return (LRESULT)g_brush_input;
        case WM_CTLCOLOREDIT: {
            if (!IsWindowEnabled((HWND)lp)) {
                SetBkColor((HDC)wp, g_pal.disabled);
                SetTextColor((HDC)wp, g_pal.muted);
                return (LRESULT)g_brush_disabled;
            }
            SetBkColor((HDC)wp, g_pal.input);
            SetTextColor((HDC)wp, g_pal.text);
            return (LRESULT)g_brush_input;
        }

        case WM_MENUSELECT: {
            UINT id = LOWORD(wp);
            UINT flags = HIWORD(wp);
            if (id == 0xFFFF && flags == 0xFFFF) {
                SetWindowTextW(g_status, L"Listo.");
                return 0;
            }
            const wchar_t* desc = nullptr;
            switch (id) {
                case ID_MENU_MANUAL: desc = L"Abre el manual de uso en el navegador."; break;
                case ID_MENU_ABOUT: desc = L"Informacion de version y alcance de esta edicion C++."; break;
                case ID_MENU_CONTEXT_ON: desc = L"Anade 'Convertir a WebP' al menu contextual del Explorador (HKCU)."; break;
                case ID_MENU_CONTEXT_OFF: desc = L"Quita las entradas del menu contextual creadas por el programa."; break;
                case ID_MENU_SENDTO_ON: desc = L"Crea un acceso directo en la carpeta Enviar a."; break;
                case ID_MENU_SENDTO_OFF: desc = L"Borra el acceso directo de la carpeta Enviar a."; break;
                case ID_MENU_UPDATE: desc = L"Comprueba si hay una version mas reciente publicada."; break;
                case ID_MENU_WATCH: desc = L"Vigila una carpeta y convierte automaticamente las imagenes nuevas."; break;
                case ID_MENU_PREVIEW: desc = L"Compara el original con el WebP resultante antes de convertir."; break;
                case ID_HIST_CLEAR: desc = L"Borra la lista de los ultimos trabajos guardados."; break;
                default:
                    if (id >= ID_HIST_BASE && id < ID_HIST_BASE + 10)
                        desc = L"Recupera origen, destino y ajustes de ese trabajo.";
                    break;
            }
            if (desc) SetWindowTextW(g_status, desc);
            return 0;
        }

        case WM_MEASUREITEM: {
            MEASUREITEMSTRUCT* mi = (MEASUREITEMSTRUCT*)lp;
            if (mi && mi->CtlType == ODT_COMBOBOX) {
                mi->itemHeight = 22;
                return TRUE;
            }
            break;
        }

        case WM_DRAWITEM: {
            DRAWITEMSTRUCT* di = (DRAWITEMSTRUCT*)lp;
            if (di && di->CtlType == ODT_COMBOBOX) {
                dibujar_combo(di);
                return TRUE;
            }
            if (di && di->CtlID == ID_TEMA) {
                dibujar_icono_tema(di);
                return TRUE;
            }
            if (di && (di->CtlID == ID_APAGAR || di->CtlID == ID_BEEP)) {
                dibujar_check_plano(di, di->CtlID == ID_APAGAR ? g_ui_apagar : g_ui_beep);
                return TRUE;
            }
            if (di && di->CtlID == ID_START) {
                COLORREF fondo = g_pal.accent;
                if (di->itemState & ODS_DISABLED) fondo = g_pal.hover;
                else if (di->itemState & ODS_SELECTED) fondo = g_pal.accent_dark;
                HBRUSH br = CreateSolidBrush(fondo);
                FillRect(di->hDC, &di->rcItem, br);
                DeleteObject(br);
                SetBkMode(di->hDC, TRANSPARENT);
                SetTextColor(di->hDC, RGB(255, 255, 255));
                HFONT vieja = (HFONT)SelectObject(di->hDC, g_font_start);
                RECT r = di->rcItem;
                const wchar_t* texto_boton =
                    (g_combo_formato && SendMessageW(g_combo_formato, CB_GETCURSEL, 0, 0) == 1)
                        ? L"Convertir a AVIF" : L"Convertir a WebP";
                DrawTextW(di->hDC, texto_boton, -1, &r,
                          DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                SelectObject(di->hDC, vieja);
                if (di->itemState & ODS_FOCUS) DrawFocusRect(di->hDC, &di->rcItem);
                return TRUE;
            }
            if (di && di->CtlID == ID_COLOR) {
                HBRUSH fondo = CreateSolidBrush(g_color);
                FillRect(di->hDC, &di->rcItem, fondo);
                DeleteObject(fondo);
                FrameRect(di->hDC, &di->rcItem, g_brush_border);
                if (di->itemState & ODS_SELECTED)
                    DrawFocusRect(di->hDC, &di->rcItem);
                return TRUE;
            }
            if (di) {
                dibujar_boton_tema(di);
                return TRUE;
            }
            break;
        }

        case WM_VSCROLL: {
            if (g_scroll_origen && (HWND)lp == g_scroll_origen) {
                SCROLLINFO si{};
                si.cbSize = sizeof(si);
                si.fMask = SIF_ALL;
                GetScrollInfo(g_scroll_origen, SB_CTL, &si);
                int pos = si.nPos;
                switch (LOWORD(wp)) {
                    case SB_LINEUP: pos--; break;
                    case SB_LINEDOWN: pos++; break;
                    case SB_PAGEUP: pos -= 4; break;
                    case SB_PAGEDOWN: pos += 4; break;
                    case SB_THUMBTRACK:
                    case SB_THUMBPOSITION: pos = si.nTrackPos; break;
                }
                int maximo = std::max(0, (int)g_origenes.size() - 1);
                pos = std::clamp(pos, 0, maximo);
                si.fMask = SIF_POS;
                si.nPos = pos;
                SetScrollInfo(g_scroll_origen, SB_CTL, &si, TRUE);
                ListView_EnsureVisible(g_lista_origen, pos, FALSE);
                return 0;
            }
            return 0;
        }

        case WM_HSCROLL: {
            if ((HWND)lp == g_track_calidad) {
                int v = (int)SendMessageW(g_track_calidad, TBM_GETPOS, 0, 0);
                wchar_t b[8];
                swprintf_s(b, L"%d", v);
                SetWindowTextW(g_lbl_calidad, b);
                marcar_personalizado();
            }
            return 0;
        }

        case WM_COMMAND: {
            int id = LOWORD(wp);
            int codigo = HIWORD(wp);
            if (codigo == BN_CLICKED &&
                (id == ID_LOSSLESS || id == ID_REENCODE || id == ID_TARGET)) {
                marcar_personalizado();
            } else if (codigo == CBN_SELCHANGE && (id == ID_METHOD || id == ID_DIM || id == ID_FORMAT)) {
                marcar_personalizado();
                if (id == ID_FORMAT && g_btn_convertir)
                    InvalidateRect(g_btn_convertir, nullptr, TRUE);
            } else if (codigo == EN_CHANGE && id == ID_TARGET_EDIT) {
                marcar_personalizado();
            }
            switch (id) {
                case ID_ADD_FILES: {
                    IFileOpenDialog* dlg = nullptr;
                    if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                                   IID_PPV_ARGS(&dlg)))) {
                        DWORD opts;
                        dlg->GetOptions(&opts);
                        dlg->SetOptions(opts | FOS_ALLOWMULTISELECT | FOS_FILEMUSTEXIST);
                        COMDLG_FILTERSPEC filtros[] = {
                            {L"Imagenes", L"*.jpg;*.jpeg;*.jfif;*.png;*.bmp;*.gif;*.tif;*.tiff;*.ico;*.webp;*.avif;*.heic;*.heif;*.tga;*.pcx;*.dds;*.ppm;*.pgm;*.pbm"},
                            {L"Todos", L"*.*"}};
                        dlg->SetFileTypes(2, filtros);
                        if (SUCCEEDED(dlg->Show(hwnd))) {
                            IShellItemArray* items = nullptr;
                            if (SUCCEEDED(dlg->GetResults(&items))) {
                                DWORD n = 0;
                                items->GetCount(&n);
                                for (DWORD i = 0; i < n; i++) {
                                    IShellItem* it = nullptr;
                                    if (SUCCEEDED(items->GetItemAt(i, &it))) {
                                        PWSTR ruta = nullptr;
                                        if (SUCCEEDED(it->GetDisplayName(SIGDN_FILESYSPATH, &ruta))) {
                                            anadir_origen(ruta);
                                            CoTaskMemFree(ruta);
                                        }
                                        it->Release();
                                    }
                                }
                                items->Release();
                            }
                        }
                        dlg->Release();
                    }
                    return 0;
                }
                case ID_ADD_ARCHIVE: {
                    IFileOpenDialog* dlg = nullptr;
                    if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                                   IID_PPV_ARGS(&dlg)))) {
                        DWORD opts;
                        dlg->GetOptions(&opts);
                        dlg->SetOptions(opts | FOS_ALLOWMULTISELECT | FOS_FILEMUSTEXIST);
                        COMDLG_FILTERSPEC filtros[] = {
                            {L"Comprimidos", L"*.zip;*.rar;*.7z"},
                            {L"Todos", L"*.*"}};
                        dlg->SetFileTypes(2, filtros);
                        if (SUCCEEDED(dlg->Show(hwnd))) {
                            IShellItemArray* items = nullptr;
                            if (SUCCEEDED(dlg->GetResults(&items))) {
                                DWORD n = 0;
                                items->GetCount(&n);
                                for (DWORD i = 0; i < n; i++) {
                                    IShellItem* it = nullptr;
                                    if (SUCCEEDED(items->GetItemAt(i, &it))) {
                                        PWSTR ruta = nullptr;
                                        if (SUCCEEDED(it->GetDisplayName(SIGDN_FILESYSPATH, &ruta))) {
                                            anadir_origen(ruta);
                                            CoTaskMemFree(ruta);
                                        }
                                        it->Release();
                                    }
                                }
                                items->Release();
                            }
                        }
                        dlg->Release();
                    }
                    return 0;
                }
                case ID_ADD_FOLDER: {
                    IFileOpenDialog* dlg = nullptr;
                    if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                                   IID_PPV_ARGS(&dlg)))) {
                        DWORD opts;
                        dlg->GetOptions(&opts);
                        dlg->SetOptions(opts | FOS_PICKFOLDERS | FOS_PATHMUSTEXIST);
                        if (SUCCEEDED(dlg->Show(hwnd))) {
                            IShellItem* it = nullptr;
                            if (SUCCEEDED(dlg->GetResult(&it))) {
                                PWSTR ruta = nullptr;
                                if (SUCCEEDED(it->GetDisplayName(SIGDN_FILESYSPATH, &ruta))) {
                                    anadir_origen(ruta);
                                    CoTaskMemFree(ruta);
                                }
                                it->Release();
                            }
                        }
                        dlg->Release();
                    }
                    return 0;
                }
                case ID_REMOVE: {
                    std::set<int> seleccion;
                    for (int i = -1; (i = ListView_GetNextItem(g_lista_origen, i, LVNI_SELECTED)) != -1;)
                        seleccion.insert(i);
                    std::vector<std::wstring> nuevos;
                    for (size_t i = 0; i < g_origenes.size(); i++) {
                        if (!seleccion.count((int)i)) nuevos.push_back(g_origenes[i]);
                    }
                    g_origenes = nuevos;
                    refrescar_origenes();
                    std::error_code ec_cola;
                    if (fs::exists(fs::path(g_ini).parent_path() / L"queue.ini", ec_cola))
                        guardar_cola();
                    return 0;
                }
                case ID_CLEAR:
                    g_origenes.clear();
                    refrescar_origenes();
                    borrar_cola();
                    return 0;
                case ID_BROWSE: {
                    IFileOpenDialog* dlg = nullptr;
                    if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                                   IID_PPV_ARGS(&dlg)))) {
                        DWORD opts;
                        dlg->GetOptions(&opts);
                        dlg->SetOptions(opts | FOS_PICKFOLDERS | FOS_PATHMUSTEXIST);
                        if (SUCCEEDED(dlg->Show(hwnd))) {
                            IShellItem* it = nullptr;
                            if (SUCCEEDED(dlg->GetResult(&it))) {
                                PWSTR ruta = nullptr;
                                if (SUCCEEDED(it->GetDisplayName(SIGDN_FILESYSPATH, &ruta))) {
                                    SetWindowTextW(g_edit_salida, ruta);
                                    CoTaskMemFree(ruta);
                                }
                                it->Release();
                            }
                        }
                        dlg->Release();
                    }
                    return 0;
                }
                case ID_OPEN: {
                    std::wstring salida = texto_ventana(g_edit_salida);
                    if (!salida.empty()) ShellExecuteW(nullptr, L"open", salida.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                    return 0;
                }
                case ID_PRESET:
                    if (HIWORD(wp) == CBN_SELCHANGE) {
                        int sel = (int)SendMessageW(g_combo_preset, CB_GETCURSEL, 0, 0);
                        if (sel > 0) aplicar_preset(sel);
                    }
                    return 0;
                case ID_SAVE_PRESET:
                    if (HIWORD(wp) == BN_CLICKED) guardar_preset_ui();
                    return 0;
                case ID_DEL_PRESET:
                    if (HIWORD(wp) == BN_CLICKED) borrar_preset_ui();
                    return 0;
                case ID_COLOR:
                    if (HIWORD(wp) == BN_CLICKED) elegir_color();
                    return 0;
                case ID_PATRON:
                    if (HIWORD(wp) == EN_CHANGE) marcar_personalizado();
                    return 0;
                case ID_ZIP:
                    if (HIWORD(wp) == BN_CLICKED) {
                        bool activo = SendMessageW(g_check_zip, BM_GETCHECK, 0, 0) == BST_CHECKED;
                        bool analizar = SendMessageW(g_check_dry, BM_GETCHECK, 0, 0) == BST_CHECKED;
                        EnableWindow(g_edit_zip, activo && !analizar);
                    }
                    return 0;
                case ID_TARGET:
                    if (HIWORD(wp) == BN_CLICKED) {
                        bool activo = SendMessageW(GetDlgItem(hwnd, ID_TARGET), BM_GETCHECK, 0, 0) == BST_CHECKED;
                        EnableWindow(g_edit_objetivo, activo);

                    }
                    return 0;
                case ID_LOSSLESS:
                    if (HIWORD(wp) == BN_CLICKED) {
                        bool perdida = SendMessageW(g_check_lossless, BM_GETCHECK, 0, 0) == BST_CHECKED;
                        EnableWindow(g_track_calidad, !perdida);
                        EnableWindow(g_combo_metodo, !perdida);
                    }
                    return 0;
                case ID_APAGAR:
                    if (HIWORD(wp) == BN_CLICKED) {
                        g_ui_apagar = !g_ui_apagar;
                        if (g_ui_apagar) {
                            int respuesta = MessageBoxW(hwnd,
                                L"Al terminar una conversión correcta, el equipo se apagará tras "
                                L"una cuenta atrás de 30 segundos desde la propia aplicación.\n\n"
                                L"Podrás cancelarlo en esa ventana.\n\n"
                                L"¿Activar el apagado automático?",
                                APP_NAME, MB_ICONQUESTION | MB_YESNO);
                            if (respuesta != IDYES) g_ui_apagar = false;
                        }
                        g_opt.apagar = g_ui_apagar;
                        InvalidateRect(g_check_apagar, nullptr, TRUE);
                        marcar_personalizado();
                    }
                    return 0;
                case ID_BEEP:
                    if (HIWORD(wp) == BN_CLICKED) {
                        g_ui_beep = !g_ui_beep;
                        g_opt.beep = g_ui_beep;
                        InvalidateRect(g_check_beep, nullptr, TRUE);
                    }
                    return 0;
                case ID_DRY:
                case ID_FLATTEN:
                    if (HIWORD(wp) == BN_CLICKED) {
                        if (id == ID_FLATTEN && g_btn_color)
                            EnableWindow(g_btn_color,
                                         SendMessageW(g_check_flatten, BM_GETCHECK, 0, 0) == BST_CHECKED);
                        if (id == ID_DRY) {
                            bool analizar = SendMessageW(g_check_dry, BM_GETCHECK, 0, 0) == BST_CHECKED;
                            bool zip = SendMessageW(g_check_zip, BM_GETCHECK, 0, 0) == BST_CHECKED;
                            EnableWindow(g_check_zip, !analizar);
                            EnableWindow(g_edit_zip, zip && !analizar);
                        }
                        marcar_personalizado();
                    }
                    return 0;
                case ID_START: {
                    if (g_corriendo) return 0;
                    if (g_actualizando) {
                        MessageBoxW(hwnd, L"Espera a que termine la descarga de la actualizacion.",
                                    APP_NAME, MB_ICONINFORMATION);
                        return 0;
                    }
                    if (g_origenes.empty()) {
                        MessageBoxW(hwnd, L"Anade imagenes y elige una carpeta de salida.", APP_NAME, MB_ICONWARNING);
                        return 0;
                    }
                    g_opt = leer_opciones_controles();
                    if (g_opt.salida.empty() && !g_opt.dry_run) {
                        MessageBoxW(hwnd, L"Selecciona la carpeta de salida.", APP_NAME, MB_ICONWARNING);
                        return 0;
                    }
                    if (g_opt.salida.empty()) {
                        wchar_t temp[MAX_PATH];
                        GetTempPathW(MAX_PATH, temp);
                        g_opt.salida = (fs::path(temp) / L"conversor_webp_analisis").wstring();
                    }
                    if (!g_opt.dry_run) {
                        ULARGE_INTEGER libre{};
                        if (GetDiskFreeSpaceExW(g_opt.salida.c_str(), &libre, nullptr, nullptr) &&
                            libre.QuadPart < 100ull * 1024 * 1024) {
                            if (MessageBoxW(hwnd,
                                            L"Quedan menos de 100 MB libres en la carpeta de salida.\n\n"
                                            L"¿Continuar?",
                                            APP_NAME, MB_ICONWARNING | MB_YESNO) != IDYES)
                                return 0;
                        }
                    }

                    guardar_config();
                    guardar_cola();
                    EnableWindow(g_btn_convertir, FALSE);
                    EnableWindow(g_btn_cancelar, TRUE);
                    EnableWindow(g_btn_pausa, TRUE);
                    g_cancelar = false;
                    g_pausa = false;
                    g_inicio_trabajo = GetTickCount64();
                    SetWindowTextW(g_btn_pausa, L"Pausar");
                    g_corriendo = true;
                    {
                        std::lock_guard<std::mutex> lock(g_mutex_eventos);
                        g_eventos.clear();
                    }
                    ListView_DeleteAllItems(g_lista_resultados);
                    g_rutas_resultados.clear();
                    SetWindowTextW(g_edit_log, L"");
                    SendMessageW(g_progress, PBM_SETPOS, 0, 0);
                    SetWindowTextW(g_status, L"Preparando...");
                    if (g_hilo.joinable()) g_hilo.join();
                    g_hilo = std::thread(hilo_conversion_seguro, g_opt, g_origenes);
                    return 0;
                }
                case ID_CANCEL:
                    g_cancelar = true;
                    g_pausa = false;
                    EnableWindow(g_btn_pausa, FALSE);
                    SetWindowTextW(g_btn_pausa, L"Pausar");
                    SetWindowTextW(g_status, L"Cancelando...");
                    return 0;
                case ID_PAUSA:
                    if (g_corriendo) {
                        g_pausa = !g_pausa;
                        SetWindowTextW(g_btn_pausa, g_pausa ? L"Reanudar" : L"Pausar");
                        SetWindowTextW(g_status, g_pausa ? L"Pausado. Pulsa Reanudar."
                                                         : L"Reanudando...");
                    }
                    return 0;
                case ID_ORIGEN_UP:
                    mover_origen(-1);
                    return 0;
                case ID_ORIGEN_DOWN:
                    mover_origen(1);
                    return 0;
                case ID_RETRY:
                    reintentar_fallidos();
                    return 0;
                case ID_MENU_CONTEXT_ON:
                    if (instalar_menu_contextual(true))
                        SetWindowTextW(g_status, L"Menu contextual instalado (HKCU).");
                    else
                        MessageBoxW(hwnd, L"No se pudo escribir el menu contextual.",
                                    APP_NAME, MB_ICONERROR);
                    return 0;
                case ID_MENU_CONTEXT_OFF:
                    instalar_menu_contextual(false);
                    SetWindowTextW(g_status, L"Menu contextual eliminado.");
                    return 0;
                case ID_MENU_SENDTO_ON:
                    if (instalar_sendto(true))
                        SetWindowTextW(g_status, L"Acceso 'Enviar a' creado.");
                    else
                        MessageBoxW(hwnd, L"No se pudo crear el acceso en Enviar a.",
                                    APP_NAME, MB_ICONERROR);
                    return 0;
                case ID_MENU_SENDTO_OFF:
                    instalar_sendto(false);
                    SetWindowTextW(g_status, L"Acceso 'Enviar a' eliminado.");
                    return 0;
                case ID_MENU_WATCH:
                    iniciar_vigilante_ui();
                    return 0;
                case ID_MENU_PREVIEW:
                    abrir_vista_previa();
                    return 0;
                case ID_MENU_UPDATE:
                    if (g_corriendo) {
                        MessageBoxW(hwnd, L"Espera a que termine la conversion en curso.",
                                    APP_NAME, MB_ICONINFORMATION);
                        return 0;
                    }
                    if (g_actualizando) {
                        MessageBoxW(hwnd, L"Ya se esta descargando una actualizacion.",
                                    APP_NAME, MB_ICONINFORMATION);
                        return 0;
                    }
                    comprobar_actualizaciones(true);
                    return 0;
                case ID_TEMA:
                    aplicar_paleta_cpp(!g_oscuro);
                    guardar_config();
                    g_menu_hot = -1;
                    aplicar_modo_oscuro_nativo(hwnd, g_oscuro);
                    {
                        RECT rc{};
                        GetClientRect(hwnd, &rc);
                        reubicar_controles(rc.right, rc.bottom);
                    }
                    if (g_acerca_rich && IsWindow(g_acerca_rich)) {
                        rellenar_acerca(g_acerca_rich);
                    }
                    if (g_acerca && IsWindow(g_acerca)) {
                        aplicar_modo_oscuro_nativo(g_acerca, g_oscuro);
                        InvalidateRect(g_acerca, nullptr, TRUE);
                    }
                    if (g_prev_hwnd && IsWindow(g_prev_hwnd)) {
                        aplicar_modo_oscuro_nativo(g_prev_hwnd, g_oscuro);
                        InvalidateRect(g_prev_hwnd, nullptr, TRUE);
                    }
                    return 0;
                case ID_PATTERN_HELP:
                    MessageBoxW(hwnd,
                        L"Patron de nombre de salida\n\n"
                        L"Dejalo vacio para conservar el nombre original.\n\n"
                        L"Marcadores disponibles:\n"
                        L"  {nombre}   nombre original sin extension\n"
                        L"  {ancho}    ancho final en px (tras redimensionar)\n"
                        L"  {alto}     alto final en px\n"
                        L"  {hash}     SHA-1 corto del archivo original\n"
                        L"  {fecha}    AAAAMMDD-HHMMSS\n"
                        L"  {formato}  webp o avif\n"
                        L"  {calidad}  calidad configurada\n\n"
                        L"Ejemplos:\n"
                        L"  {nombre}_{ancho}x{alto}\n"
                        L"  {hash}\n"
                        L"  {fecha}_{nombre}",
                        APP_NAME, MB_ICONINFORMATION);
                    return 0;
                case ID_MENU_MANUAL: {
                    std::wstring ruta = ruta_ejecutable();
                    fs::path manual = ruta.empty()
                                          ? fs::path(L"MANUAL_DE_USO.html")
                                          : fs::path(ruta).parent_path() / L"MANUAL_DE_USO.html";
                    if (fs::exists(manual))
                        ShellExecuteW(nullptr, L"open", manual.wstring().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                    else
                        MessageBoxW(hwnd, L"No se encontro MANUAL_DE_USO.html junto al programa.", APP_NAME, MB_ICONINFORMATION);
                    return 0;
                }
                case ID_MENU_ABOUT:
                    mostrar_acerca(hwnd);
                    return 0;
                default:
                    if (id >= ID_HIST_BASE && id < ID_HIST_BASE + 10) {
                        restaurar_historial(id - ID_HIST_BASE);
                        return 0;
                    }
                    if (id == ID_HIST_CLEAR) {
                        vaciar_historial();
                        return 0;
                    }
                    break;
            }
            return 0;
        }

        case WM_NOTIFY: {
            NMHDR* nh = (NMHDR*)lp;
            if (g_oscuro && g_lista_resultados && nh->code == NM_CUSTOMDRAW &&
                nh->hwndFrom == ListView_GetHeader(g_lista_resultados)) {
                NMCUSTOMDRAW* cd = (NMCUSTOMDRAW*)lp;
                if (cd->dwDrawStage == CDDS_PREPAINT) return CDRF_NOTIFYITEMDRAW;
                if (cd->dwDrawStage == CDDS_ITEMPREPAINT) {
                    FillRect(cd->hdc, &cd->rc, g_brush_bg);
                    wchar_t texto[128]{};
                    HDITEMW item{};
                    item.mask = HDI_TEXT | HDI_FORMAT;
                    item.pszText = texto;
                    item.cchTextMax = 128;
                    if (Header_GetItem(nh->hwndFrom, (int)cd->dwItemSpec, &item)) {
                        SetBkMode(cd->hdc, TRANSPARENT);
                        SetTextColor(cd->hdc, g_pal.text);
                        HFONT vieja = (HFONT)SelectObject(cd->hdc, g_font);
                        RECT r = cd->rc;
                        r.left += 6;
                        UINT formato = DT_SINGLELINE | DT_VCENTER | DT_LEFT;
                        if (item.fmt & HDF_CENTER) formato = DT_SINGLELINE | DT_VCENTER | DT_CENTER;
                        else if (item.fmt & HDF_RIGHT) formato = DT_SINGLELINE | DT_VCENTER | DT_RIGHT;
                        DrawTextW(cd->hdc, texto, -1, &r, formato | DT_NOPREFIX);
                        SelectObject(cd->hdc, vieja);
                    }
                    return CDRF_SKIPDEFAULT;
                }
            }
            if (nh->idFrom == ID_RESULTS && nh->code == NM_CUSTOMDRAW) {
                NMLVCUSTOMDRAW* cd = (NMLVCUSTOMDRAW*)lp;
                if (cd->nmcd.dwDrawStage == CDDS_PREPAINT) return CDRF_NOTIFYITEMDRAW;
                if (cd->nmcd.dwDrawStage == CDDS_ITEMPREPAINT) {
                    wchar_t estado[64] = L"";
                    ListView_GetItemText(g_lista_resultados, (int)cd->nmcd.dwItemSpec,
                                         4, estado, 64);
                    if (_wcsicmp(estado, L"convertida") == 0)
                        cd->clrText = RGB(19, 115, 51);
                    else if (_wcsicmp(estado, L"analizada") == 0)
                        cd->clrText = RGB(21, 87, 176);
                    else if (_wcsicmp(estado, L"omitida") == 0)
                        cd->clrText = RGB(95, 107, 122);
                    else if (_wcsicmp(estado, L"fallo") == 0)
                        cd->clrText = RGB(197, 34, 31);
                    return CDRF_NEWFONT;
                }
            }
            if (nh->idFrom == ID_TAB && nh->code == TCN_SELCHANGE) {
                seleccionar_pestana(TabCtrl_GetCurSel(g_tab));
            }
            if (nh->idFrom == ID_RESULTS && nh->code == NM_DBLCLK) {
                int sel = ListView_GetNextItem(g_lista_resultados, -1, LVNI_SELECTED);
                if (sel >= 0 && sel < (int)g_rutas_resultados.size()) {
                    std::wstring ruta = g_rutas_resultados[sel];
                    if (fs::exists(ruta)) ShellExecuteW(nullptr, L"open", ruta.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                }
            }
            return 0;
        }

        case WM_LBUTTONDOWN: {
            POINT p{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
            int zona = zona_menu_custom(p);
            if (zona >= 0) {
                POINT pantalla = p;
                ClientToScreen(hwnd, &pantalla);
                abrir_menu_custom(zona, pantalla);
                return 0;
            }
            int pestana = zona_tab_custom(p);
            if (pestana >= 0) {
                seleccionar_pestana(pestana);
                return 0;
            }
            break;
        }

        case WM_MOUSEMOVE: {
            POINT p{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
            int zona = zona_menu_custom(p);
            if (zona != g_menu_hot) {
                g_menu_hot = zona;
                RECT barra{0, 0, 260, 18};
                InvalidateRect(hwnd, &barra, FALSE);
            }
            RECT zona_drop = zona_drop_actual();
            bool dentro = PtInRect(&zona_drop, p) != FALSE;
            if (dentro != g_drop_hot) {
                g_drop_hot = dentro;
                InvalidateRect(hwnd, &zona_drop, FALSE);
            }
            TRACKMOUSEEVENT tme{};
            tme.cbSize = sizeof(tme);
            tme.dwFlags = TME_LEAVE;
            tme.hwndTrack = hwnd;
            TrackMouseEvent(&tme);
            return 0;
        }

        case WM_MOUSELEAVE:
            if (g_menu_hot != -1) {
                g_menu_hot = -1;
                RECT barra{0, 0, 260, 18};
                InvalidateRect(hwnd, &barra, FALSE);
            }
            if (g_drop_hot) {
                g_drop_hot = false;
                RECT zona = zona_drop_actual();
                InvalidateRect(hwnd, &zona, FALSE);
            }
            return 0;

        case WM_SETCURSOR:
            if (LOWORD(lp) == HTCLIENT) {
                POINT p{};
                GetCursorPos(&p);
                ScreenToClient(hwnd, &p);
                if (zona_menu_custom(p) >= 0 || zona_tab_custom(p) >= 0) {
                    SetCursor(LoadCursorW(nullptr, IDC_HAND));
                    return TRUE;
                }
                RECT zona = zona_drop_actual();
                if (PtInRect(&zona, p)) {
                    SetCursor(LoadCursorW(nullptr, IDC_HAND));
                    return TRUE;
                }
            }
            break;

        case WM_LBUTTONUP: {
            POINT p{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
            RECT zona = zona_drop_actual();
            if (PtInRect(&zona, p))
                SendMessageW(hwnd, WM_COMMAND, MAKEWPARAM(ID_ADD_FILES, BN_CLICKED), 0);
            return 0;
        }

        case WM_PARENTNOTIFY:
            if (LOWORD(wp) == WM_LBUTTONDOWN || LOWORD(wp) == WM_RBUTTONDOWN ||
                LOWORD(wp) == WM_MOUSEWHEEL) {
                ocultar_tooltip();
                g_tip_control = nullptr;
            }
            return 0;

        case WM_DROPFILES: {
            HDROP drop = (HDROP)wp;
            UINT n = DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
            for (UINT i = 0; i < n; i++) {
                wchar_t ruta[MAX_PATH * 4];
                if (DragQueryFileW(drop, i, ruta, MAX_PATH * 4)) anadir_origen(ruta);
            }
            DragFinish(drop);
            return 0;
        }

        case WM_TIMER:
            if (wp == 1) {
                actualizar_tooltip();
                std::deque<Evento> pendientes;
                {
                    std::lock_guard<std::mutex> lock(g_mutex_eventos);
                    pendientes.swap(g_eventos);
                }
                for (auto& e : pendientes) {
                    if (e.tipo == Evento::LOG) log(e.texto);
                    else if (e.tipo == Evento::ESTADO) {
                        if (!g_pausa) SetWindowTextW(g_status, e.texto.c_str());
                    }
                    else if (e.tipo == Evento::PROGRESO) {
                        SendMessageW(g_progress, PBM_SETRANGE, 0, MAKELPARAM(0, std::max(1, e.b)));
                        SendMessageW(g_progress, PBM_SETPOS, e.a, 0);
                        if (!g_pausa) {
                            wchar_t b[128];
                            ULONGLONG transcurrido = g_inicio_trabajo
                                                         ? GetTickCount64() - g_inicio_trabajo
                                                         : 0;
                            ULONGLONG restante = (e.a > 0 && e.b > e.a)
                                                     ? transcurrido / (ULONGLONG)e.a *
                                                           (ULONGLONG)(e.b - e.a) / 1000
                                                     : 0;
                            if (restante >= 60)
                                swprintf_s(b, g_opt.dry_run
                                                   ? L"Analizando %d/%d  ·  quedan ~%llu min"
                                                   : L"Convirtiendo %d/%d  ·  quedan ~%llu min",
                                           e.a, e.b, restante / 60);
                            else
                                swprintf_s(b, g_opt.dry_run
                                                   ? L"Analizando %d/%d  ·  quedan ~%llu s"
                                                   : L"Convirtiendo %d/%d  ·  quedan ~%llu s",
                                           e.a, e.b, restante);
                            SetWindowTextW(g_status, b);
                        }
                    } else if (e.tipo == Evento::FILA) {
                        anadir_fila_resultado(e.fila);
                    } else if (e.tipo == Evento::ACTUALIZACION) {
                        g_update_en_curso = false;
                        if (e.b == 1) {
                            SetWindowTextW(g_status, L"Actualizacion disponible.");
                            if (MessageBoxW(hwnd, e.texto.c_str(), APP_NAME,
                                            MB_ICONINFORMATION | MB_YESNO) == IDYES) {
                                if (!e.extra.empty() && !e.extra3.empty() && e.modo >= 1)
                                    iniciar_descarga_actualizacion(e);
                                else
                                    MessageBoxW(hwnd, L"No se pudo preparar la actualizacion.",
                                                APP_NAME, MB_ICONERROR);
                            }
                        } else if (e.b == 2) {
                            if (e.a)
                                MessageBoxW(hwnd, e.texto.c_str(), APP_NAME, MB_ICONWARNING);
                            SetWindowTextW(g_status, L"Sin conexion para comprobar actualizaciones.");
                        } else {
                            if (e.a)
                                MessageBoxW(hwnd, e.texto.c_str(), APP_NAME, MB_ICONINFORMATION);
                            SetWindowTextW(g_status, L"Estas al dia.");
                        }
                    } else if (e.tipo == Evento::DESCARGA) {
                        if (e.a >= 0) {
                            SendMessageW(g_progress, PBM_SETPOS, e.a, 0);
                            wchar_t txt[80];
                            swprintf_s(txt, L"Descargando actualizacion... %d%%", e.a);
                            SetWindowTextW(g_status, txt);
                        } else {
                            SetWindowTextW(g_status, L"Descargando actualizacion...");
                        }
                    } else if (e.tipo == Evento::DESCARGA_LISTA) {
                        std::wstring error;
                        int firma = verificar_firma_authenticode(e.extra);
                        bool lanzado = false;
                        if (firma < 0) {
                            error = L"La firma digital del archivo no es valida.";
                        } else {
                            lanzado = lanzar_actualizador(e.extra, e.modo, error);
                        }
                        if (lanzado) {
                            SetWindowTextW(g_status, L"Reiniciando para actualizar...");
                            PostMessageW(hwnd, WM_CLOSE, 0, 0);
                        } else {
                            g_actualizando = false;
                            EnableWindow(g_btn_convertir, TRUE);
                            SendMessageW(g_progress, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
                            SendMessageW(g_progress, PBM_SETPOS, 0, 0);
                            SetWindowTextW(g_status, L"Listo.");
                            MessageBoxW(hwnd, (L"No se pudo iniciar el actualizador:\n" + error).c_str(),
                                        APP_NAME, MB_ICONERROR);
                        }
                    } else if (e.tipo == Evento::DESCARGA_ERROR) {
                        g_actualizando = false;
                        EnableWindow(g_btn_convertir, TRUE);
                        SendMessageW(g_progress, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
                        SendMessageW(g_progress, PBM_SETPOS, 0, 0);
                        SetWindowTextW(g_status, L"Listo.");
                        MessageBoxW(hwnd, (L"No se pudo descargar la actualizacion:\n" + e.texto).c_str(),
                                    APP_NAME, MB_ICONERROR);
                    } else if (e.tipo == Evento::FIN) {
                        g_corriendo = false;
                        g_pausa = false;
                        EnableWindow(g_btn_convertir, TRUE);
                        EnableWindow(g_btn_cancelar, FALSE);
                        EnableWindow(g_btn_pausa, FALSE);
                        SetWindowTextW(g_btn_pausa, L"Pausar");
                        SetWindowTextW(g_status, e.texto.c_str());
                        bool exito = e.b == 0 && !g_cancelar;
                        if (e.b > 0) {
                            g_fallidos_ultimos = g_origenes;
                            guardar_cola();
                        } else if (!g_cancelar) {
                            g_fallidos_ultimos.clear();
                            borrar_cola();
                        }
                        wchar_t extra[256];
                        swprintf_s(extra, L"%s\n\nTamano: %.2f MB -> %.2f MB",
                                   e.texto.c_str(), e.fila.kb_in / 1024.0, e.fila.kb_out / 1024.0);
                        MessageBoxW(hwnd, extra, APP_NAME,
                                    exito ? MB_ICONINFORMATION : MB_ICONWARNING);
                        if (g_opt.beep) {
                            std::thread([exito] {
                                if (exito) {
                                    Beep(880, 120);
                                    Beep(1320, 160);
                                } else {
                                    Beep(440, 200);
                                    Beep(330, 280);
                                }
                            }).detach();
                        }
                        if (!g_opt.dry_run && e.a > 0) registrar_historial(e.a);
                        if (g_opt.apagar && e.a > 0 && e.b == 0 && !g_opt.dry_run && !g_cancelar)
                            mostrar_apagado();
                        if (g_hilo.joinable()) g_hilo.join();
                    }
                }
            }
            return 0;

        case WM_GETMINMAXINFO: {
            MINMAXINFO* mmi = (MINMAXINFO*)lp;
            mmi->ptMinTrackSize.x = 1016;
            mmi->ptMinTrackSize.y = 900;
            return 0;
        }

        case WM_CLOSE:
            if (g_corriendo) {
                if (MessageBoxW(hwnd, L"Hay una conversion en curso. ¿Salir de todas formas?",
                                APP_NAME, MB_ICONQUESTION | MB_YESNO) != IDYES)
                    return 0;
                g_cancelar = true;
            }
            g_opt = leer_opciones_controles();
            guardar_config();
            DestroyWindow(hwnd);
            return 0;

        case WM_DESTROY:
            detener_vigilante(false);
            g_cancelar = true;
            g_pausa = false;
            if (g_hilo.joinable()) g_hilo.join();
            limpiar_temporales();
            if (g_tip_hwnd) DestroyWindow(g_tip_hwnd);
            if (g_timer) KillTimer(hwnd, 1);
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// ---------------------------------------------------------------------------
//  CLI
// ---------------------------------------------------------------------------
static int ejecutar_cli(const std::vector<std::wstring>& args) {
    Options opt;
    std::vector<std::wstring> origenes;
    int apagar_delay = 60;
    bool json = false;
    bool quiet = false;
    std::wstring preset;
    for (size_t i = 0; i < args.size(); i++) {
        std::wstring a = args[i];
        auto sig = [&](std::wstring& dst) {
            if (i + 1 < args.size()) dst = args[++i];
        };
        if (a == L"--version") { wprintf(L"ConversorWebP %s\n", APP_VERSION); return 0; }
        if (a == L"--help" || a == L"-h") {
            wprintf(L"Conversor WebP C++ %s\n\n", APP_VERSION);
            wprintf(L"Uso: ConversorWebPC.exe <origen...> -o <carpeta> [opciones]\n\n");
            wprintf(L"  -o, --output      carpeta de salida\n");
            wprintf(L"  -q, --quality     calidad 1-100 (85)\n");
            wprintf(L"  -m, --method      esfuerzo 0-6 (6)\n");
            wprintf(L"  --max-dim N       lado maximo (no amplia)\n");
            wprintf(L"  --max-kb N        peso maximo por imagen\n");
            wprintf(L"  --lossless        sin perdida\n");
            wprintf(L"  --pattern PATRON  patron de nombre ({nombre} {ancho} {alto} {fecha} {hash})\n");
            wprintf(L"  --format FORMATO  webp (defecto) o avif\n");
            wprintf(L"  --dry-run         analizar sin escribir archivos\n");
            wprintf(L"  --flat            aplanar transparencia\n");
            wprintf(L"  --bg #RRGGBB      color de fondo al aplanar (ffffff)\n");
            wprintf(L"  --zip NOMBRE      crear ZIP\n");
            wprintf(L"  --csv             guardar informe CSV\n");
            wprintf(L"  --no-overwrite    omitir existentes\n");
            wprintf(L"  --no-recursive    no mantener subcarpetas\n");
            wprintf(L"  --shutdown        apagar el equipo al terminar correctamente\n");
            wprintf(L"  --shutdown-delay N  segundos antes de apagar (60)\n");
            wprintf(L"  --json            salida final en JSON\n");
            wprintf(L"  --quiet           ocultar el progreso por archivo\n");
            wprintf(L"  --jobs N          maximo de conversiones en paralelo\n");
            wprintf(L"  --preset NOMBRE   aplicar un preset incluido\n");
            wprintf(L"  --keep-date       conservar la fecha original\n\n");
            return 0;
        }
        if (a == L"-o" || a == L"--output") sig(opt.salida);
        else if (a == L"-q" || a == L"--quality") { std::wstring v; sig(v); opt.calidad = _wtoi(v.c_str()); }
        else if (a == L"-m" || a == L"--method") { std::wstring v; sig(v); opt.metodo = _wtoi(v.c_str()); }
        else if (a == L"--max-dim") { std::wstring v; sig(v); opt.max_dim = _wtoi(v.c_str()); }
        else if (a == L"--max-kb") { std::wstring v; sig(v); opt.objetivo_kb = _wtoi(v.c_str()); }
        else if (a == L"--lossless") opt.sin_perdida = true;
        else if (a == L"--pattern") sig(opt.patron);
        else if (a == L"--format") {
            std::wstring v;
            sig(v);
            opt.formato = (_wcsicmp(v.c_str(), L"avif") == 0) ? L"AVIF" : L"WEBP";
        }
        else if (a == L"--dry-run") opt.dry_run = true;
        else if (a == L"--flat") opt.flatten = true;
        else if (a == L"--bg") {
            std::wstring v;
            sig(v);
            if (!v.empty() && v[0] == L'#') v.erase(0, 1);
            if (v.size() == 3) {
                std::wstring v6;
                for (wchar_t c : v) { v6 += c; v6 += c; }
                v = v6;
            }
            if (v.size() == 6) {
                auto hexval = [](wchar_t c) -> int {
                    if (c >= L'0' && c <= L'9') return c - L'0';
                    if (c >= L'a' && c <= L'f') return c - L'a' + 10;
                    if (c >= L'A' && c <= L'F') return c - L'A' + 10;
                    return 0;
                };
                int r = hexval(v[0]) * 16 + hexval(v[1]);
                int g = hexval(v[2]) * 16 + hexval(v[3]);
                int b = hexval(v[4]) * 16 + hexval(v[5]);
                opt.color = RGB(r, g, b);
            }
        }
        else if (a == L"--zip") { std::wstring v; sig(v); opt.zip = true; opt.zip_nombre = v; }
        else if (a == L"--csv") opt.csv = true;
        else if (a == L"--no-overwrite") opt.sobrescribir = false;
        else if (a == L"--no-recursive") opt.mantener = false;
        else if (a == L"--shutdown") opt.apagar = true;
        else if (a == L"--json") json = true;
        else if (a == L"--quiet") quiet = true;
        else if (a == L"--keep-date") opt.preservar_fecha = true;
        else if (a == L"--jobs") {
            std::wstring v;
            sig(v);
            opt.jobs = std::clamp(_wtoi(v.c_str()), 0, 32);
        }
        else if (a == L"--preset") sig(preset);
        else if (a == L"--shutdown-delay") {
            std::wstring v;
            sig(v);
            apagar_delay = std::clamp(_wtoi(v.c_str()), 10, 86400);
        }
        else if (!a.empty() && a[0] != L'-') origenes.push_back(a);
    }
    if (opt.salida.empty() || origenes.empty()) {
        wprintf(L"ERROR: faltan origen o -o. Usa --help.\n");
        return 2;
    }
    opt.calidad = std::clamp(opt.calidad, 1, 100);
    opt.metodo = std::clamp(opt.metodo, 0, 6);
    if (!preset.empty()) {
        inicializar_presets();
        auto it = std::find_if(g_presets.begin(), g_presets.end(),
                               [&](const Preset& p) { return p.nombre == preset; });
        if (it == g_presets.end()) {
            wprintf(L"ERROR: preset desconocido: %s\n", preset.c_str());
            return 2;
        }
        std::wstring salida = opt.salida;
        bool zip = opt.zip;
        std::wstring zip_nombre = opt.zip_nombre;
        bool csv = opt.csv;
        bool dry = opt.dry_run;
        bool fecha = opt.preservar_fecha;
        int jobs = opt.jobs;
        Options base = it->opt;
        base.salida = salida;
        base.zip = zip;
        base.zip_nombre = zip_nombre;
        base.csv = csv;
        base.dry_run = dry;
        base.preservar_fecha = fecha;
        base.jobs = jobs;
        opt = base;
    }
    g_opt = opt;
    g_origenes = origenes;
    g_cancelar = false;
    {
        std::lock_guard<std::mutex> lock(g_mutex_eventos);
        g_eventos.clear();
    }
    g_modo_cli = true;
    g_cli_quiet = quiet || json;
    hilo_conversion_seguro(g_opt, g_origenes);
    g_modo_cli = false;
    int fallidas = 0;
    int convertidas = 0;
    int omitidas = 0;
    {
        std::lock_guard<std::mutex> lock(g_mutex_eventos);
        for (auto& e : g_eventos) {
            if (e.tipo == Evento::LOG) {
                if (!g_cli_quiet) wprintf(L"%s\n", e.texto.c_str());
            } else if (e.tipo == Evento::FIN) {
                if (!json && !quiet) wprintf(L"\n%s\n", e.texto.c_str());
                convertidas = e.a;
                fallidas = e.b;
                omitidas = e.c;
            }
        }
    }
    bool apagado = false;
    if (opt.apagar && convertidas > 0 && fallidas == 0 && !opt.dry_run && !g_cancelar) {
        apagado = programar_apagado(apagar_delay, false);
        if (!json && !quiet) {
            if (apagado)
                wprintf(L"Apagado programado en %d s. Para cancelar: shutdown /a\n", apagar_delay);
            else
                wprintf(L"ERROR: no se pudo programar el apagado.\n");
        }
    }
    if (json) {
        wprintf(L"{\"version\":\"%s\",\"converted\":%d,\"skipped\":%d,\"failed\":%d,"
                L"\"cancelled\":%s,\"shutdown_scheduled\":%s}\n",
                APP_VERSION, convertidas, omitidas, fallidas,
                g_cancelar ? L"true" : L"false", apagado ? L"true" : L"false");
    } else {
        if (!quiet) wprintf(L"\n");
    }
    g_cli_quiet = false;
    return fallidas > 0 ? 1 : 0;
}

// ---------------------------------------------------------------------------
//  WinMain
// ---------------------------------------------------------------------------
int wmain(int argc, wchar_t** argv) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_UNAWARE_GDISCALED);
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    INITCOMMONCONTROLSEX icc{sizeof(icc), ICC_LISTVIEW_CLASSES | ICC_BAR_CLASSES |
                                               ICC_PROGRESS_CLASS | ICC_TAB_CLASSES |
                                               ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&icc);
    crc32_init();

    HINSTANCE inst = GetModuleHandleW(nullptr);
    std::vector<std::wstring> args;
    for (int i = 1; i < argc; i++) args.push_back(argv[i]);

    // El ejecutable es de subsistema Windows (sin consola propia). Si se lanza
    // desde una consola y no hay salida redirigida, se reutiliza la del padre.
    HANDLE salida_actual = GetStdHandle(STD_OUTPUT_HANDLE);
    bool sin_salida = !salida_actual || salida_actual == INVALID_HANDLE_VALUE;
    if (sin_salida && AttachConsole(ATTACH_PARENT_PROCESS)) {
        FILE* flujo = nullptr;
        freopen_s(&flujo, "CONOUT$", "w", stdout);
        freopen_s(&flujo, "CONOUT$", "w", stderr);
    }

    for (auto& a : args) {
        if (a == L"--install-context") {
            bool ok = instalar_menu_contextual(true);
            CoUninitialize();
            return ok ? 0 : 1;
        }
        if (a == L"--uninstall-context") {
            instalar_menu_contextual(false);
            CoUninitialize();
            return 0;
        }
        if (a == L"--install-sendto") {
            bool ok = instalar_sendto(true);
            CoUninitialize();
            return ok ? 0 : 1;
        }
        if (a == L"--uninstall-sendto") {
            instalar_sendto(false);
            CoUninitialize();
            return 0;
        }
    }

    bool modo_cli = false;
    for (auto& a : args) {
        if (a == L"-o" || a == L"--output" || a == L"--help" || a == L"-h" || a == L"--version")
            modo_cli = true;
    }
    if (modo_cli) {
        int rc = ejecutar_cli(args);
        CoUninitialize();
        return rc;
    }

    HANDLE instancia = CreateMutexW(nullptr, FALSE, L"Local\\ConversorWebPC_GUI");
    (void)instancia;
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        MessageBoxW(nullptr, L"Ya hay una instancia de Conversor WebP abierta.",
                    L"Conversor WebP", MB_ICONINFORMATION);
        return 0;
    }

    cargar_config();
    aplicar_modo_global_cpp(g_oscuro);
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = L"ConversorWebPC";
    HICON icono_grande = (HICON)LoadImageW(inst, MAKEINTRESOURCEW(101), IMAGE_ICON, 0, 0,
                                           LR_DEFAULTSIZE);
    HICON icono_pequeno = (HICON)LoadImageW(inst, MAKEINTRESOURCEW(101), IMAGE_ICON, 16, 16, 0);
    if (!icono_grande) {
        std::wstring exe = ruta_ejecutable();
        fs::path icono = (exe.empty() ? fs::current_path() : fs::path(exe).parent_path()) /
                         L"icon.ico";
        icono_grande = (HICON)LoadImageW(nullptr, icono.wstring().c_str(), IMAGE_ICON, 0, 0,
                                        LR_LOADFROMFILE);
    }
    if (!icono_pequeno) icono_pequeno = icono_grande;
    wc.hIcon = icono_grande;
    wc.hIconSm = icono_pequeno;
    RegisterClassExW(&wc);

    g_hwnd = CreateWindowExW(0, wc.lpszClassName, L"Conversor WebP 2.9.0",
                             WS_OVERLAPPEDWINDOW,
                             CW_USEDEFAULT, CW_USEDEFAULT, 1016, 900, nullptr, nullptr, inst, nullptr);
    if (!g_hwnd) return 1;

    aplicar_modo_oscuro_nativo(g_hwnd, g_oscuro);

    for (auto& a : args) {
        if (!a.empty() && a[0] != L'-') anadir_origen(a);
    }
    if (args.empty()) cargar_cola();

    HMENU menu = CreateMenu();
    g_menu_principal = menu;
    g_menu_hist = CreatePopupMenu();
    HMENU herramientas = CreatePopupMenu();
    g_menu_herramientas = herramientas;
    AppendMenuW(herramientas, MF_STRING, ID_MENU_CONTEXT_ON, L"Instalar en el menu contextual");
    AppendMenuW(herramientas, MF_STRING, ID_MENU_CONTEXT_OFF, L"Quitar del menu contextual");
    AppendMenuW(herramientas, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(herramientas, MF_STRING, ID_MENU_SENDTO_ON, L"Crear acceso en 'Enviar a'");
    AppendMenuW(herramientas, MF_STRING, ID_MENU_SENDTO_OFF, L"Quitar acceso de 'Enviar a'");
    AppendMenuW(herramientas, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(herramientas, MF_STRING, ID_MENU_WATCH, L"Vigilar una carpeta");
    AppendMenuW(herramientas, MF_STRING, ID_MENU_PREVIEW, L"Vista previa del seleccionado\tCtrl+P");
    AppendMenuW(herramientas, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(herramientas, MF_STRING, ID_ORIGEN_UP, L"Subir origen\tCtrl+Arriba");
    AppendMenuW(herramientas, MF_STRING, ID_ORIGEN_DOWN, L"Bajar origen\tCtrl+Abajo");
    AppendMenuW(herramientas, MF_STRING, ID_RETRY, L"Reintentar archivos fallidos");
    HMENU ayuda = CreatePopupMenu();
    g_menu_ayuda = ayuda;
    AppendMenuW(ayuda, MF_STRING, ID_MENU_MANUAL, L"Manual de uso\tF1");
    AppendMenuW(ayuda, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(ayuda, MF_STRING, ID_MENU_UPDATE, L"Buscar actualizaciones...");
    AppendMenuW(ayuda, MF_STRING, ID_MENU_ABOUT, L"Acerca de Conversor WebP (C++)");
    AppendMenuW(menu, MF_POPUP, (UINT_PTR)herramientas, L"Herramientas");
    AppendMenuW(menu, MF_POPUP, (UINT_PTR)g_menu_hist, L"Historial");
    AppendMenuW(menu, MF_POPUP, (UINT_PTR)ayuda, L"Ayuda");
    if (!g_oscuro) SetMenu(g_hwnd, menu);
    reconstruir_menu_historial();
    aplicar_modo_oscuro_nativo(g_hwnd, g_oscuro);
    DrawMenuBar(g_hwnd);

    ACCEL accels[] = {
        {FVIRTKEY | FCONTROL, 'O', ID_ADD_FILES},
        {FVIRTKEY | FCONTROL | FSHIFT, 'O', ID_ADD_FOLDER},
        {FVIRTKEY | FCONTROL, VK_RETURN, ID_START},
        {FVIRTKEY, VK_F5, ID_START},
        {FVIRTKEY, VK_DELETE, ID_REMOVE},
        {FVIRTKEY, VK_F1, ID_MENU_MANUAL},
        {FVIRTKEY | FCONTROL, 'P', ID_MENU_PREVIEW},
        {FVIRTKEY | FCONTROL, VK_UP, ID_ORIGEN_UP},
        {FVIRTKEY | FCONTROL, VK_DOWN, ID_ORIGEN_DOWN},
    };
    HACCEL accel = CreateAcceleratorTableW(accels, (int)(sizeof(accels) / sizeof(accels[0])));

    ShowWindow(g_hwnd, SW_SHOW);
    UpdateWindow(g_hwnd);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        if (!TranslateAcceleratorW(g_hwnd, accel, &msg) && !IsDialogMessageW(g_hwnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    if (accel) DestroyAcceleratorTable(accel);
    CoUninitialize();
    return 0;
}
