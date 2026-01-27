#include "ExplorerHelper.h"
#include <windows.h>
#include <shlobj.h>
#include <exdisp.h>
#include <shlguid.h>
#include <QDebug>

QStringList ExplorerHelper::getSelectedPaths() {
    QStringList paths;

    // 获取当前前台窗口句柄
    HWND hwndForeground = GetForegroundWindow();
    if (!hwndForeground) return paths;

    // 初始化 COM (通常在 main.cpp 做，但这里确保安全)
    // CoInitialize(NULL);

    IShellWindows* psw = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_ShellWindows, nullptr, CLSCTX_ALL, IID_IShellWindows, (void**)&psw);

    if (SUCCEEDED(hr)) {
        long count = 0;
        psw->get_Count(&count);

        for (long i = 0; i < count; ++i) {
            VARIANT v;
            V_VT(&v) = VT_I4;
            V_I4(&v) = i;

            IDispatch* pdisp = nullptr;
            if (SUCCEEDED(psw->Item(v, &pdisp))) {
                IWebBrowserApp* pwba = nullptr;
                if (SUCCEEDED(pdisp->QueryInterface(IID_IWebBrowserApp, (void**)&pwba))) {
                    HWND hwndWba = 0;
                    pwba->get_HWND((LONG_PTR*)&hwndWba);

                    // 匹配前台窗口
                    if (hwndWba == hwndForeground) {
                        IServiceProvider* psp = nullptr;
                        if (SUCCEEDED(pwba->QueryInterface(IID_IServiceProvider, (void**)&psp))) {
                            IShellBrowser* psb = nullptr;
                            if (SUCCEEDED(psp->QueryService(SID_STopLevelBrowser, IID_IShellBrowser, (void**)&psb))) {
                                IShellView* psv = nullptr;
                                if (SUCCEEDED(psb->QueryActiveShellView(&psv))) {
                                    IFolderView* pfv = nullptr;
                                    if (SUCCEEDED(psv->QueryInterface(IID_IFolderView, (void**)&pfv))) {
                                        IShellItemArray* psia = nullptr;
                                        // 注意：IFolderView::GetSelection 可能在某些老版本 Windows 上不同，这里尝试通用的方法
                                        if (SUCCEEDED(pfv->Items(SVGIO_SELECTION, IID_IShellItemArray, (void**)&psia))) {
                                            DWORD dwItems = 0;
                                            psia->GetCount(&dwItems);
                                            for (DWORD j = 0; j < dwItems; ++j) {
                                                IShellItem* psi = nullptr;
                                                if (SUCCEEDED(psia->GetItemAt(j, &psi))) {
                                                    LPWSTR pszPath = nullptr;
                                                    if (SUCCEEDED(psi->GetDisplayName(SIGDN_FILESYSPATH, &pszPath))) {
                                                        paths << QString::fromWCharArray(pszPath);
                                                        CoTaskMemFree(pszPath);
                                                    }
                                                    psi->Release();
                                                }
                                            }
                                            psia->Release();
                                        }
                                        pfv->Release();
                                    }
                                    psv->Release();
                                }
                                psb->Release();
                            }
                            psp->Release();
                        }
                        // 找到匹配的窗口并处理完后可以提前退出
                        pwba->Release();
                        pdisp->Release();
                        break;
                    }
                    pwba->Release();
                }
                pdisp->Release();
            }
        }
        psw->Release();
    }

    // CoUninitialize();
    return paths;
}

bool ExplorerHelper::isForegroundExplorer() {
    HWND hwnd = GetForegroundWindow();
    if (!hwnd) return false;

    char className[MAX_PATH];
    if (GetClassNameA(hwnd, className, MAX_PATH)) {
        QString name = QString(className);
        return (name == "CabinetWClass" || name == "ExploreWClass");
    }
    return false;
}
