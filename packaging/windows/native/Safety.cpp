#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <msi.h>
#include <msiquery.h>
#include <tlhelp32.h>
#include <string>
namespace {
UINT reject(MSIHANDLE session,const wchar_t* text){const auto record=MsiCreateRecord(0);MsiRecordSetStringW(record,0,text);MsiProcessMessage(session,INSTALLMESSAGE_ERROR,record);MsiCloseHandle(record);return ERROR_INSTALL_FAILURE;}
}
extern "C" __declspec(dllexport) UINT __stdcall ValidatePlatformAndIdle(MSIHANDLE session){
    SYSTEM_INFO info{};GetNativeSystemInfo(&info);if(info.wProcessorArchitecture!=PROCESSOR_ARCHITECTURE_AMD64)return reject(session,L"Windows x64 e necessario.");
    WCHAR directory[32768];DWORD length=32768;if(MsiGetPropertyW(session,L"INSTALLDIR",directory,&length)!=ERROR_SUCCESS||length==0)return reject(session,L"Pasta de instalacao invalida.");std::wstring root(directory);while(!root.empty()&&root.back()==L'\\')root.pop_back();
    const auto snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);if(snapshot==INVALID_HANDLE_VALUE)return reject(session,L"Nao foi possivel verificar processos em uso.");PROCESSENTRY32W entry{};entry.dwSize=sizeof(entry);bool busy=false;if(Process32FirstW(snapshot,&entry))do{if(_wcsicmp(entry.szExeFile,L"NOVA DAW.exe")!=0&&_wcsicmp(entry.szExeFile,L"NovaPluginScanner.exe")!=0)continue;const auto process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,entry.th32ProcessID);if(!process){busy=true;break;}WCHAR path[32768];DWORD size=32768;const bool queried=QueryFullProcessImageNameW(process,0,path,&size)!=0;CloseHandle(process);if(!queried||_wcsicmp(path,(root+L"\\NOVA DAW.exe").c_str())==0||_wcsicmp(path,(root+L"\\NovaPluginScanner.exe").c_str())==0){busy=true;break;}}while(Process32NextW(snapshot,&entry));CloseHandle(snapshot);
    return busy?reject(session,L"Feche NOVA DAW Alpha e scanner antes de instalar, reparar ou remover. Nenhuma sessao foi encerrada."):ERROR_SUCCESS;
}
