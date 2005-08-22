/*
 * Copyright 2005 Jacek Caban
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "config.h"

#include <stdarg.h>

#define COBJMACROS

#include "windef.h"
#include "winbase.h"
#include "winuser.h"
#include "winreg.h"
#include "ole2.h"

#include "wine/debug.h"
#include "wine/unicode.h"

#include "mshtml_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(mshtml);

DEFINE_GUID(CLSID_StartupNotifier, 0x1f59b001,0x02c9,0x11d5,0xae,0x76,0xcc,0x92,0xf7,0xdb,0x9e,0x03);
DEFINE_GUID(CLSID_nsWebBrowser, 0xf1eac761,0x87e9,0x11d3,0xaf,0x80,0x00,0xa0,0x24,0xff,0xc0,0x8c);

#define NS_APPSTARTUPNOTIFIER_CONTRACTID "@mozilla.org/embedcomp/appstartup-notifier;1"
#define NS_WEBBROWSER_CONTRACTID "@mozilla.org/embedding/browser/nsWebBrowser;1"
#define NS_IOSERVICE_CONTRACTID "@mozilla.org/network/io-service;1"

#define APPSTARTUP_TOPIC "app-startup"

#define PR_UINT32_MAX 0xffffffff

typedef struct nsACString {
    void *d1;
    PRUint32 d2;
    void *d3;
} nsString;

static nsresult (*NS_InitXPCOM2)(nsIServiceManager**,void*,void*);
static nsresult (*NS_ShutdownXPCOM)(nsIServiceManager*);
static nsresult (*NS_StringContainerInit)(nsString*);
static nsresult (*NS_CStringContainerInit)(nsACString*);
static nsresult (*NS_StringContainerFinish)(nsString*);
static nsresult (*NS_CStringContainerFinish)(nsACString*);
static nsresult (*NS_StringSetData)(nsString*,const PRUnichar*,PRUint32);
static nsresult (*NS_CStringSetData)(nsString*,const char*,PRUint32);
static nsresult (*NS_NewLocalFile)(const nsString*,PRBool,nsIFile**);

static HINSTANCE hXPCOM = NULL;

static nsIServiceManager *pServMgr = NULL;
static nsIComponentManager *pCompMgr = NULL;
static nsIIOService *pIOService = NULL;

static const WCHAR wszNsContainer[] = {'N','s','C','o','n','t','a','i','n','e','r',0};

static ATOM nscontainer_class;

static LRESULT WINAPI nsembed_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    HTMLDocument *This;
    nsresult nsres;

    static const WCHAR wszTHIS[] = {'T','H','I','S',0};

    if(msg == WM_CREATE) {
        This = *(HTMLDocument**)lParam;
        SetPropW(hwnd, wszTHIS, This);
    }else {
        This = (HTMLDocument*)GetPropW(hwnd, wszTHIS);
    }

    switch(msg) {
        case WM_SIZE:
            TRACE("(%p)->(WM_SIZE)\n", This);

            nsres = nsIBaseWindow_SetSize(This->nscontainer->window,
                    LOWORD(lParam), HIWORD(lParam), TRUE);
            if(NS_FAILED(nsres))
                WARN("SetSize failed: %08lx\n", nsres);
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}


static void register_nscontainer_class(void)
{
    static WNDCLASSEXW wndclass = {
        sizeof(WNDCLASSEXW),
        CS_DBLCLKS,
        nsembed_proc,
        0, 0, NULL, NULL, NULL, NULL, NULL,
        wszNsContainer,
        NULL,
    };
    wndclass.hInstance = hInst;
    nscontainer_class = RegisterClassExW(&wndclass);
}

static BOOL get_mozilla_path(PRUnichar *gre_path)
{
    DWORD res, type, i, size = MAX_PATH;
    HKEY mozilla_key, hkey;
    WCHAR key_name[100];
    BOOL ret = FALSE;

    static const WCHAR wszGreKey[] =
        {'S','o','f','t','w','a','r','e','\\',
            'm','o','z','i','l','l','a','.','o','r','g','\\',
                'G','R','E',0};

    static const WCHAR wszGreHome[] = {'G','r','e','H','o','m','e',0};

    res = RegOpenKeyW(HKEY_LOCAL_MACHINE, wszGreKey, &mozilla_key);
    if(res != ERROR_SUCCESS) {
        TRACE("Could not open key %s\n", debugstr_w(wszGreKey));
        return FALSE;
    }

    for(i=0; !ret && RegEnumKeyW(mozilla_key, i, key_name, sizeof(key_name)/sizeof(WCHAR)) == ERROR_SUCCESS; i++) {
        RegOpenKeyW(mozilla_key, key_name, &hkey);
        res = RegQueryValueExW(hkey, wszGreHome, NULL, &type, (LPBYTE)gre_path, &size);
        if(res == ERROR_SUCCESS)
            ret = TRUE;
        RegCloseKey(hkey);
    }

    RegCloseKey(mozilla_key);
    return ret;
}

static BOOL get_mozctl_path(PRUnichar *gre_path)
{
    HKEY hkey;
    DWORD res, type, size = MAX_PATH;

    static const WCHAR wszMozCtlKey[] =
        {'S','o','f','t','w','a','r','e','\\','M','o','z','i','l','l','a',0};
    static const WCHAR wszBinDirectoryPath[] =
        {'B','i','n','D','i','r','e','c','t','o','r','y','P','a','t','h',0};

    res = RegOpenKeyW(HKEY_LOCAL_MACHINE, wszMozCtlKey, &hkey);
    if(res != ERROR_SUCCESS) {
        TRACE("Could not open key %s\n", debugstr_w(wszMozCtlKey));
        return FALSE;
    }

    res = RegQueryValueExW(hkey, wszBinDirectoryPath, NULL, &type, (LPBYTE)gre_path, &size);
    if(res != ERROR_SUCCESS) {
        ERR("Could not get value %s\n", debugstr_w(wszBinDirectoryPath));
        return FALSE;
    }

    return TRUE;
}

static BOOL load_gecko()
{
    nsresult nsres;
    nsIObserver *pStartNotif;
    nsString path;
    nsIFile *gre_dir;
    PRUnichar gre_path[MAX_PATH];
    WCHAR path_env[MAX_PATH];
    int len;

    static BOOL tried_load = FALSE;
    static const WCHAR wszPATH[] = {'P','A','T','H',0};
    static const WCHAR strXPCOM[] = {'x','p','c','o','m','.','d','l','l',0};

    TRACE("()\n");

    if(tried_load)
        return pCompMgr != NULL;
    tried_load = TRUE;

    if(!get_mozctl_path(gre_path) && !get_mozilla_path(gre_path)) {
        MESSAGE("Could not load Mozilla. HTML rendering will be disabled.\n");
        return FALSE;
    }

    TRACE("found path %s\n", debugstr_w(gre_path));

    /* We have to modify PATH as XPCOM loads other DLLs from this directory. */
    GetEnvironmentVariableW(wszPATH, path_env, sizeof(path_env)/sizeof(WCHAR));
    len = strlenW(path_env);
    path_env[len++] = ';';
    strcpyW(path_env+len, gre_path);
    SetEnvironmentVariableW(wszPATH, path_env);

    hXPCOM = LoadLibraryW(strXPCOM);
    if(!hXPCOM) {
        ERR("Could not load XPCOM: %ld\n", GetLastError());
        return FALSE;
    }

#define NS_DLSYM(func) \
    func = (typeof(func))GetProcAddress(hXPCOM, #func); \
    if(!func) \
        ERR("Could not GetProcAddress(" #func ") failed\n")

    NS_DLSYM(NS_InitXPCOM2);
    NS_DLSYM(NS_ShutdownXPCOM);
    NS_DLSYM(NS_StringContainerInit);
    NS_DLSYM(NS_CStringContainerInit);
    NS_DLSYM(NS_StringContainerFinish);
    NS_DLSYM(NS_CStringContainerFinish);
    NS_DLSYM(NS_StringSetData);
    NS_DLSYM(NS_CStringSetData);
    NS_DLSYM(NS_NewLocalFile);

#undef NS_DLSYM

    NS_StringContainerInit(&path);
    NS_StringSetData(&path, gre_path, PR_UINT32_MAX);
    nsres = NS_NewLocalFile(&path, FALSE, &gre_dir);
    NS_StringContainerFinish(&path);
    if(NS_FAILED(nsres)) {
        ERR("NS_NewLocalFile failed: %08lx\n", nsres);
        FreeLibrary(hXPCOM);
        return FALSE;
    }

    nsres = NS_InitXPCOM2(&pServMgr, gre_dir, NULL);
    if(NS_FAILED(nsres)) {
        ERR("NS_InitXPCOM2 failed: %08lx\n", nsres);
        FreeLibrary(hXPCOM);
        return FALSE;
    }

    nsres = nsIServiceManager_QueryInterface(pServMgr, &IID_nsIComponentManager, (void**)&pCompMgr);
    if(NS_FAILED(nsres))
        ERR("Could not get nsIComponentManager: %08lx\n", nsres);

    nsres = nsIComponentManager_CreateInstanceByContractID(pCompMgr, NS_APPSTARTUPNOTIFIER_CONTRACTID,
            NULL, &IID_nsIObserver, (void**)&pStartNotif);
    if(NS_SUCCEEDED(nsres)) {
        nsres = nsIObserver_Observe(pStartNotif, NULL, APPSTARTUP_TOPIC, NULL);
        if(NS_FAILED(nsres))
            ERR("Observe failed: %08lx\n", nsres);

        nsIObserver_Release(pStartNotif);
    }else {
        ERR("could not get appstartup-notifier: %08lx\n", nsres);
    }

    return TRUE;
}

nsACString *nsACString_Create(void)
{
    nsACString *ret;
    ret = HeapAlloc(GetProcessHeap(), 0, sizeof(nsACString));
    NS_CStringContainerInit(ret);
    return ret;
}

void nsACString_SetData(nsACString *str, const char *data)
{
    NS_CStringSetData(str, data, PR_UINT32_MAX);
}

void nsACString_Destroy(nsACString *str)
{
    NS_CStringContainerFinish(str);
    HeapFree(GetProcessHeap(), 0, str);
}

void close_gecko()
{
    TRACE("()\n");

    if(pCompMgr)
        nsIComponentManager_Release(pCompMgr);

    if(pServMgr)
        nsIServiceManager_Release(pServMgr);

    if(hXPCOM)
        FreeLibrary(hXPCOM);
}

nsIURI *get_nsIURI(LPCWSTR url)
{
    nsIURI *ret;
    nsACString *acstr;
    nsresult nsres;
    char *urla;
    int len;

    if(!pIOService) {
        nsres = nsIServiceManager_GetServiceByContactID(pServMgr, NS_IOSERVICE_CONTRACTID,
                &IID_nsIIOService, (void**)&pIOService);
        if(NS_FAILED(nsres))
            ERR("Failed to create nsIOService: %08lx\n", nsres);
    }

    len = WideCharToMultiByte(CP_ACP, 0, url, -1, NULL, -1, NULL, NULL);
    urla = HeapAlloc(GetProcessHeap(), 0, len);
    WideCharToMultiByte(CP_ACP, 0, url, -1, urla, -1, NULL, NULL);

    acstr = nsACString_Create();
    nsACString_SetData(acstr, urla);

    nsres = nsIIOService_NewURI(pIOService, acstr, NULL, NULL, &ret);
    if(NS_FAILED(nsres))
        FIXME("NewURI failed: %08lx\n", nsres);

    nsACString_Destroy(acstr);
    HeapFree(GetProcessHeap(), 0, urla);

    return ret;
}

void HTMLDocument_NSContainer_Init(HTMLDocument *This)
{
    nsIWebBrowserSetup *wbsetup;
    nsresult nsres;

    This->nscontainer = NULL;

    if(!load_gecko())
        return;

    This->nscontainer = HeapAlloc(GetProcessHeap(), 0, sizeof(NSContainer));

    nsres = nsIComponentManager_CreateInstanceByContractID(pCompMgr, NS_WEBBROWSER_CONTRACTID,
            NULL, &IID_nsIWebBrowser, (void**)&This->nscontainer->webbrowser);
    if(NS_FAILED(nsres))
        ERR("Creating WebBrowser failed: %08lx\n", nsres);

    nsres = nsIWebBrowser_QueryInterface(This->nscontainer->webbrowser, &IID_nsIBaseWindow,
            (void**)&This->nscontainer->window);
    if(NS_FAILED(nsres))
        ERR("Could not get nsIBaseWindow interface: %08lx\n", nsres);

    nsres = nsIWebBrowser_QueryInterface(This->nscontainer->webbrowser,
            &IID_nsIWebBrowserSetup, (void**)&wbsetup);
    if(NS_SUCCEEDED(nsres)) {
        nsres = nsIWebBrowserSetup_SetProperty(wbsetup, SETUP_IS_CHROME_WRAPPER, TRUE);
        if(NS_FAILED(nsres))
            ERR("SetProperty failed: %08lx\n", nsres);
        nsIWebBrowserSetup_Release(wbsetup);
    }else {
        ERR("Could not get nsIWebBrowserSetup interface\n");
    }

    nsres = nsIWebBrowser_QueryInterface(This->nscontainer->webbrowser, &IID_nsIWebNavigation,
            (void**)&This->nscontainer->navigation);
    if(NS_FAILED(nsres))
        ERR("Could not get nsIWebNavigation interface: %08lx\n", nsres);

    nsres = nsIWebBrowserStream_QueryInterface(This->nscontainer->webbrowser, &IID_nsIWebBrowserStream,
            (void**)&This->nscontainer->stream);
    if(NS_FAILED(nsres))
        ERR("Could not get nsIWebBrowserStream interface: %08lx\n", nsres);

    if(!nscontainer_class)
        register_nscontainer_class();

    This->nscontainer->hwnd = CreateWindowExW(0, wszNsContainer, NULL,
            WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, 0, 0, 100, 100,
            GetDesktopWindow(), NULL, hInst, This);

    nsres = nsIBaseWindow_InitWindow(This->nscontainer->window, This->nscontainer->hwnd, NULL,
            0, 0, 100, 100);
    if(NS_SUCCEEDED(nsres)) {
        nsres = nsIBaseWindow_Create(This->nscontainer->window);
        if(NS_FAILED(nsres))
            WARN("Creating window failed: %08lx\n", nsres);

        nsIBaseWindow_SetVisibility(This->nscontainer->window, FALSE);
        nsIBaseWindow_SetEnabled(This->nscontainer->window, FALSE);
    }else {
        ERR("InitWindow failed: %08lx\n", nsres);
    }
}

void HTMLDocument_NSContainer_Destroy(HTMLDocument *This)
{
    TRACE("(%p)\n", This);

    nsIWebBrowser_Release(This->nscontainer->webbrowser);
    nsIWebNavigation_Release(This->nscontainer->navigation);
    nsIBaseWindow_Release(This->nscontainer->window);

    if(This->nscontainer->stream)
        nsIWebBrowserStream_Release(This->nscontainer->stream);

    HeapFree(GetProcessHeap(), 0, This->nscontainer);
}
