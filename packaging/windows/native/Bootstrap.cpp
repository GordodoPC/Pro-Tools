#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <msi.h>
#include <tlhelp32.h>
#include <string>
#include <vector>
namespace {
constexpr auto title=L"NOVA DAW Alpha 0.7 - Public Test";
constexpr auto product=L"{32C8E968-D103-4A70-9665-EE04BA06A707}";
std::wstring quote(const std::wstring& value){return L"\""+value+L"\"";}
int fail(const std::wstring& text,bool quiet,int code){if(!quiet)MessageBoxW(nullptr,text.c_str(),title,MB_OK|MB_ICONERROR);return code;}
bool running(const std::wstring& root){const auto snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);if(snapshot==INVALID_HANDLE_VALUE)return true;PROCESSENTRY32W entry{};entry.dwSize=sizeof(entry);bool found=false;if(Process32FirstW(snapshot,&entry))do{if(_wcsicmp(entry.szExeFile,L"NOVA DAW.exe")!=0&&_wcsicmp(entry.szExeFile,L"NovaPluginScanner.exe")!=0)continue;const auto process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,entry.th32ProcessID);if(!process){found=true;break;}WCHAR path[32768];DWORD length=32768;const bool queried=QueryFullProcessImageNameW(process,0,path,&length)!=0;CloseHandle(process);if(!queried||_wcsicmp(path,(root+L"\\NOVA DAW.exe").c_str())==0||_wcsicmp(path,(root+L"\\NovaPluginScanner.exe").c_str())==0){found=true;break;}}while(Process32NextW(snapshot,&entry));CloseHandle(snapshot);return found;}
}
int WINAPI wWinMain(HINSTANCE module,HINSTANCE,PWSTR,int){int count=0;auto** argv=CommandLineToArgvW(GetCommandLineW(),&count);bool quiet=false;std::wstring log,extract;for(int i=1;i<count;++i){const std::wstring arg=argv[i];if(arg==L"/quiet")quiet=true;else if(arg==L"/log"&&i+1<count)log=argv[++i];else if(arg==L"/extract"&&i+1<count)extract=argv[++i];else{LocalFree(argv);return fail(L"Argumento desconhecido. Opcoes: /quiet /log caminho /extract pasta.",quiet,87);}}LocalFree(argv);
    WCHAR local[MAX_PATH];if(FAILED(SHGetFolderPathW(nullptr,CSIDL_LOCAL_APPDATA|CSIDL_FLAG_CREATE,nullptr,SHGFP_TYPE_CURRENT,local)))return fail(L"Pasta de dados do usuario indisponivel.",quiet,3);
    const std::wstring logRoot=std::wstring(local)+L"\\NOVA DAW\\Alpha 0.7\\Diagnostics";SHCreateDirectoryExW(nullptr,logRoot.c_str(),nullptr);
    if(extract.empty()&&running(std::wstring(local)+L"\\Programs\\NOVA DAW Alpha 0.7"))return fail(L"Feche a NOVA DAW Alpha e seu scanner antes de instalar, reparar ou remover. Nenhum processo foi encerrado.",quiet,1618);
#ifdef NOVA_UNINSTALL
    if(!extract.empty())return fail(L"Extracao nao se aplica ao desinstalador.",quiet,87);
    if(log.empty())log=logRoot+L"\\uninstall-"+std::to_wstring(GetTickCount64())+L".log";
    if(!quiet&&MessageBoxW(nullptr,L"Remover NOVA DAW Alpha 0.7 deste usuario?\nSessoes, fontes, preferencias e logs serao preservados.",title,MB_YESNO|MB_ICONQUESTION)!=IDYES)return 1602;
    std::wstring arguments=L" /x "+std::wstring(product);
#else
    using VersionFunction=LONG (WINAPI*)(OSVERSIONINFOW*);const auto versionFunction=reinterpret_cast<VersionFunction>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"),"RtlGetVersion"));OSVERSIONINFOW version{};version.dwOSVersionInfoSize=sizeof(version);SYSTEM_INFO info{};GetNativeSystemInfo(&info);if(!versionFunction||versionFunction(&version)!=0||version.dwMajorVersion<10||info.wProcessorArchitecture!=PROCESSOR_ARCHITECTURE_AMD64)return fail(L"Windows 10/11 x64 e necessario.",quiet,1633);
    if(!quiet&&extract.empty()&&MessageBoxW(nullptr,L"Instalar candidato para REVISAO LOCAL?\nLicenca publica pendente. Estabilidade de audio ainda em validacao.\nInstalacao por usuario, sem drivers ou plugins globais.",title,MB_YESNO|MB_ICONQUESTION)!=IDYES)return 1602;
    if(log.empty())log=logRoot+L"\\install-"+std::to_wstring(GetTickCount64())+L".log";
    GUID id{};if(FAILED(CoCreateGuid(&id)))return fail(L"Falha ao criar pasta temporaria.",quiet,3);WCHAR uuid[40],temp[MAX_PATH];StringFromGUID2(id,uuid,40);GetTempPathW(MAX_PATH,temp);
    const std::wstring folder=extract.empty()?std::wstring(temp)+L"NOVA-Setup-"+uuid:extract;
    if(!CreateDirectoryW(folder.c_str(),nullptr))return fail(L"Pasta de extracao deve ser nova e gravavel.",quiet,3);
    const auto payload=FindResourceW(module,MAKEINTRESOURCEW(100),RT_RCDATA);const auto resource=payload?LoadResource(module,payload):nullptr;const DWORD size=payload?SizeofResource(module,payload):0;const auto* bytes=resource?LockResource(resource):nullptr;
    const auto msi=folder+L"\\NOVA-DAW-Alpha-0.7-x64.msi";const auto file=CreateFileW(msi.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);DWORD written=0;const bool saved=file!=INVALID_HANDLE_VALUE&&bytes&&size&&WriteFile(file,bytes,size,&written,nullptr)&&written==size;if(file!=INVALID_HANDLE_VALUE)CloseHandle(file);if(!saved){DeleteFileW(msi.c_str());RemoveDirectoryW(folder.c_str());return fail(L"Pacote incorporado invalido ou erro de disco.",quiet,3);}if(!extract.empty())return 0;
    const bool repair=MsiQueryProductStateW(product)==INSTALLSTATE_DEFAULT;std::wstring arguments=(repair?L" /fvamus ":L" /i ")+quote(msi)+L" NOVAWIN10=1";
#endif
    WCHAR system[MAX_PATH];GetSystemDirectoryW(system,MAX_PATH);const auto executable=std::wstring(system)+L"\\msiexec.exe";std::wstring command=quote(executable)+arguments+(quiet?L" /qn":L" /passive")+L" /norestart /L*v "+quote(log);
    STARTUPINFOW startup{};startup.cb=sizeof(startup);PROCESS_INFORMATION process{};if(!CreateProcessW(executable.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&startup,&process))return fail(L"Nao foi possivel iniciar Windows Installer.",quiet,3);CloseHandle(process.hThread);
#ifdef NOVA_UNINSTALL
    // Exit before MSI removes this executable. Windows Installer owns progress/error UI.
    CloseHandle(process.hProcess);return 0;
#else
    WaitForSingleObject(process.hProcess,INFINITE);DWORD result=1;GetExitCodeProcess(process.hProcess,&result);CloseHandle(process.hProcess);DeleteFileW(msi.c_str());RemoveDirectoryW(folder.c_str());if(result!=0&&result!=3010)return fail(L"Instalacao nao concluida. Consulte o log:\n"+log,quiet,int(result));if(!quiet)MessageBoxW(nullptr,L"NOVA DAW Alpha 0.7 instalada. Use os atalhos no menu Iniciar ou area de trabalho.\nCandidato local: nao publicar antes dos aceites de licenca e estabilidade.",title,MB_OK|MB_ICONINFORMATION);return int(result);
#endif
}
