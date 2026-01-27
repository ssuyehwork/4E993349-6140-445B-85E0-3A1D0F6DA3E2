#include "ExplorerHelper.h"
#include <windows.h>
#include <shlobj.h>
#include <exdisp.h>
#include <shlguid.h>
#include <QDebug>

QStringList ExplorerHelper::getSelectedPaths() {
    QStringList paths;

    // 获取当前前台窗口及所属的顶层窗口
    HWND hwndForeground = GetForegroundWindow();
    if (!hwndForeground) return paths;

    // 使用 GA_ROOTOWNER 确保获取到的是顶层拥有者窗口（如 CabinetWClass）
    HWND hwndRoot = GetAncestor(hwndForeground, GA_ROOTOWNER);

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

                    // 匹配逻辑：IWebBrowserApp 的 HWND 应该是我们前台窗口的根窗口
                    if (hwndWba == hwndRoot) {
                        IServiceProvider* psp = nullptr;
                        if (SUCCEEDED(pwba->QueryInterface(IID_IServiceProvider, (void**)&psp))) {
                            IShellBrowser* psb = nullptr;
                            if (SUCCEEDED(psp->QueryService(SID_STopLevelBrowser, IID_IShellBrowser, (void**)&psb))) {
                                IShellView* psv = nullptr;
                                if (SUCCEEDED(psb->QueryActiveShellView(&psv))) {

                                    // 关键：检查 View 窗口是否可见且是前台窗口的祖先或本身
                                    // 这能有效解决 Windows 11 多标签页下所有标签共享同一个 Root HWND 的问题
                                    HWND hwndView = NULL;
                                    if (SUCCEEDED(psv->GetWindow(&hwndView))) {
                                        bool isActiveView = (hwndView == hwndForeground || IsChild(hwndView, hwndForeground));

                                        // 如果不是直接子孙，尝试检查可见性（回退方案）
                                        if (!isActiveView) {
                                            isActiveView = IsWindowVisible(hwndView);
                                        }

                                        if (isActiveView) {
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
                                        }
                                    }
                                    psv->Release();
                                }
                                psb->Release();
                            }
                            psp->Release();
                        }

                        // 如果找到了路径，说明这就是激活的窗口/标签，可以退出循环
                        if (!paths.isEmpty()) {
                            pwba->Release();
                            pdisp->Release();
                            break;
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

    // 获取根窗口
    HWND root = GetAncestor(hwnd, GA_ROOTOWNER);

    char className[MAX_PATH];
    if (GetClassNameA(root, className, MAX_PATH)) {
        QString name = QString(className);
        // 支持标准资源管理器窗口和桌面
        return (name == "CabinetWClass" || name == "ExploreWClass" ||
                name == "Progman" || name == "WorkerW");
    }
    return false;
}
