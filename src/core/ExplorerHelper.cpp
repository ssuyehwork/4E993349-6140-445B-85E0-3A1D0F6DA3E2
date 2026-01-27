#include "ExplorerHelper.h"
#include <windows.h>
#include <shlobj.h>
#include <exdisp.h>
#include <shlguid.h>
#include <QDebug>

QStringList ExplorerHelper::getSelectedPaths() {
    QStringList paths;

    // 获取当前前台窗口
    HWND hwndForeground = GetForegroundWindow();
    if (!hwndForeground) return paths;

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

                    // 匹配逻辑：检查前台窗口是否属于该 Shell 窗口
                    if (hwndWba != NULL && (hwndWba == hwndForeground || IsChild(hwndWba, hwndForeground))) {
                        IServiceProvider* psp = nullptr;
                        if (SUCCEEDED(pwba->QueryInterface(IID_IServiceProvider, (void**)&psp))) {
                            IShellBrowser* psb = nullptr;
                            if (SUCCEEDED(psp->QueryService(SID_STopLevelBrowser, IID_IShellBrowser, (void**)&psb))) {
                                IShellView* psv = nullptr;
                                if (SUCCEEDED(psb->QueryActiveShellView(&psv))) {
                                    HWND hwndView = NULL;
                                    if (SUCCEEDED(psv->GetWindow(&hwndView))) {
                                        // 针对 Windows 11 标签页：
                                        // 只有当前可见的标签页视图才是活动的。
                                        // 另外，某些情况下 hwndForeground 可能是视图本身或其子窗口。
                                        if (IsWindowVisible(hwndView)) {
                                            IFolderView* pfv = nullptr;
                                            if (SUCCEEDED(psv->QueryInterface(IID_IFolderView, (void**)&pfv))) {
                                                IShellItemArray* psia = nullptr;
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

                                            // 既然找到了可见的 View 且它是前台窗口的父级，
                                            // 那么无论有没有选中项，这都是我们要找的那个“活动”标签页。
                                            psv->Release();
                                            psb->Release();
                                            psp->Release();
                                            pwba->Release();
                                            pdisp->Release();
                                            psw->Release();
                                            return paths;
                                        }
                                    }
                                    psv->Release();
                                }
                                psb->Release();
                            }
                            psp->Release();
                        }
                    }
                    pwba->Release();
                }
                pdisp->Release();
            }
        }
        psw->Release();
    }

    return paths;
}

bool ExplorerHelper::isForegroundExplorer() {
    HWND hwnd = GetForegroundWindow();
    if (!hwnd) return false;

    // 向上查找根窗口
    HWND root = GetAncestor(hwnd, GA_ROOTOWNER);

    char className[MAX_PATH];
    if (GetClassNameA(root, className, MAX_PATH)) {
        QString name = QString(className);
        if (name == "CabinetWClass" || name == "ExploreWClass" ||
            name == "Progman" || name == "WorkerW") {
            return true;
        }
    }
    return false;
}
