/* ***** BEGIN LICENSE BLOCK *****
* Copyright 2009 Wenzhang Zhu (wzzhu@cs.hku.hk)
* Version: MPL 1.1/GPL 2.0/LGPL 2.1
* This code was based on the npsimple.c sample code in Gecko-sdk.
*
* The contents of this file are subject to the Mozilla Public License Version
* 1.1 (the "License"); you may not use this file except in compliance with
* the License. You may obtain a copy of the License at
* http://www.mozilla.org/MPL/
*
* Software distributed under the License is distributed on an "AS IS" basis,
* WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
* for the specific language governing rights and limitations under the
* License.
* ***** END LICENSE BLOCK ***** */

#include <stdio.h>
#include <string.h>
#include <npapi.h>
#include <npupp.h>
#include <npruntime.h>
#ifdef WIN32
#include <windows.h>
#include <wininet.h>
#endif

static NPObject* so              = NULL;
static NPNetscapeFuncs* npnfuncs = NULL;
static const char* kGetProxyStateMethod = "getProxyState";
static const char* kToggleProxyStateMethod = "toggleProxyState";
static const char* kGetProxyConfigMethod = "getProxyConfig";
static const char* kSetProxyConfigMethod = "setProxyConfig";
static const char* kGetConnectionNameProperty = "connectionName";

typedef enum proxyState {
  Proxy_Unknown = 0,
  Proxy_On,
  Proxy_Off
} ProxyState;

typedef struct proxyConfig {
  bool auto_detect;
  char auto_config;
  bool use_proxy;
  char* auto_config_url;
  char* proxy_server;
  char* bypass_list;
} ProxyConfig;

static void DebugLog(const char* msg) {
#ifdef DEBUG
#ifndef _WINDOWS
  fputs(msg, stderr);
#else
  FILE* out = fopen("\\npswitchproxy.log", "a");
  fputs(msg, out);
  fclose(out);
#endif
#endif
}

#ifdef WIN32
typedef bool (__stdcall* InternetQueryOptionFunc) (
  HINTERNET hInternet, DWORD dwOption,
  LPVOID lpBuffer, LPDWORD lpdwBufferLength);

typedef bool (__stdcall* InternetSetOptionFunc) (
  HINTERNET hInternet, DWORD dwOption,
  LPVOID lpBuffer, DWORD dwBufferLength);

typedef bool (__stdcall* InternetGetConnectedStateExFunc) (
              LPDWORD lpdwFlags,
              LPTSTR lpszConnectionName,
              DWORD dwNameLen,
              DWORD dwReserved);

HINSTANCE hWinInetLib;
InternetQueryOptionFunc pInternetQueryOption;
InternetSetOptionFunc pInternetSetOption;
InternetGetConnectedStateExFunc pInternetGetConnectedStateEx;

/* The global buffer to hold the current Internet connection name */
const int kMaxLengthOfConnectionName = 1024;
wchar_t connection_name[kMaxLengthOfConnectionName];
char utf8_connection_name[kMaxLengthOfConnectionName];

/* Convert the wide null-terminated string to utf8 null-terminated string.
   Note that the return value should be freed after use.
*/
char* WStrToUtf8(LPCWSTR str) {
  int bufferSize = WideCharToMultiByte(CP_UTF8, 0, (LPCWSTR)str, -1, NULL, 0, NULL, NULL);
	char* m = (char*)malloc(bufferSize);
	WideCharToMultiByte(CP_UTF8, 0, (LPCWSTR)str, -1, m, bufferSize, NULL, NULL);
	return m;
}

static bool LoadWinInetDll() {
  hWinInetLib = LoadLibrary(TEXT("wininet.dll"));
  if (hWinInetLib == NULL) {
    return false;
  }
  pInternetQueryOption = (InternetQueryOptionFunc)GetProcAddress(
    HMODULE(hWinInetLib), "InternetQueryOptionW");
  if (pInternetQueryOption == NULL) {
    pInternetQueryOption = (InternetQueryOptionFunc)GetProcAddress(
      HMODULE(hWinInetLib), "InternetQueryOptionA");
  }
  if (pInternetQueryOption == NULL) {
    FreeLibrary(hWinInetLib);
    return false;
  }
  pInternetSetOption = (InternetSetOptionFunc)GetProcAddress(
    HMODULE(hWinInetLib), "InternetSetOptionW");

  if (pInternetSetOption == NULL) {
    pInternetSetOption = (InternetSetOptionFunc)GetProcAddress(
      HMODULE(hWinInetLib), "InternetSetOptionA");
  }
  if (pInternetSetOption == NULL) {
    FreeLibrary(hWinInetLib);
    return false;
  }

  if (pInternetGetConnectedStateEx == NULL) {
    pInternetGetConnectedStateEx =
      (InternetGetConnectedStateExFunc)GetProcAddress(
      HMODULE(hWinInetLib), "InternetGetConnectedStateExW");
  }
  if (pInternetGetConnectedStateEx == NULL) {
    pInternetGetConnectedStateEx = (InternetGetConnectedStateExFunc)GetProcAddress(
      HMODULE(hWinInetLib), "InternetGetConnectedStateExA");
  }
  if (pInternetGetConnectedStateEx == NULL) {
    FreeLibrary(hWinInetLib);
    return false;
  }
  return true;
}

static void UnloadWinInetDll() {
  if (hWinInetLib != NULL) {
    FreeLibrary(hWinInetLib);
  }
}

static LPWSTR GetCurrentConnectionName() {  
  DWORD flags;  
  memset(utf8_connection_name, 0, kMaxLengthOfConnectionName);
  if (pInternetGetConnectedStateEx(&flags,(LPTSTR)connection_name,
                                   kMaxLengthOfConnectionName, NULL)) {
    WideCharToMultiByte(
        CP_UTF8, 0, (LPCWSTR)connection_name,
        -1, utf8_connection_name, kMaxLengthOfConnectionName,
        NULL, NULL);

    if ((flags & INTERNET_CONNECTION_LAN) == 0) {
      DebugLog("npswitchproxy: Got non-Lan connection.\n");
      return (LPWSTR) connection_name;
    }
  }  
  return NULL;
}
  
static ProxyState GetProxyState() {  
  INTERNET_PER_CONN_OPTION options[1];
  options[0].dwOption = INTERNET_PER_CONN_FLAGS;
  INTERNET_PER_CONN_OPTION_LIST list;	
  unsigned long nSize = sizeof(INTERNET_PER_CONN_OPTION_LIST);   
  list.dwSize = nSize;  
  list.pszConnection = GetCurrentConnectionName();  
  list.pOptions = options;
  list.dwOptionCount = 1;
  list.dwOptionError = 0;	
  if (pInternetQueryOption(
    NULL,
    INTERNET_OPTION_PER_CONNECTION_OPTION,
    &list,
    &nSize)) {
      DWORD proxyState = options[0].Value.dwValue;		
      if (proxyState & PROXY_TYPE_PROXY) {
        return Proxy_On;
      } else {
        return Proxy_Off;
      }
  }
  return Proxy_Unknown;
}

static ProxyState ToggleProxyState() {
  INTERNET_PER_CONN_OPTION options[1];
  options[0].dwOption = INTERNET_PER_CONN_FLAGS;
  INTERNET_PER_CONN_OPTION_LIST list;	
  unsigned long nSize = sizeof(INTERNET_PER_CONN_OPTION_LIST);	
  list.dwSize = nSize;
  list.pszConnection = GetCurrentConnectionName();
  list.pOptions = options;
  list.dwOptionCount = 1;
  list.dwOptionError = 0;	
  if (pInternetQueryOption(
    NULL,
    INTERNET_OPTION_PER_CONNECTION_OPTION,
    &list,
    &nSize)) {
      DWORD dwProxyState = options[0].Value.dwValue;
      ProxyState state;
      if (dwProxyState & PROXY_TYPE_PROXY) {
        dwProxyState &= ~PROXY_TYPE_PROXY;
        state = Proxy_Off;
      } else {
        dwProxyState |= PROXY_TYPE_PROXY;
        state = Proxy_On;
      }
      options[0].Value.dwValue = dwProxyState;
      if (pInternetSetOption(
        NULL,
        INTERNET_OPTION_PER_CONNECTION_OPTION,
        &list,
        nSize)) {
          return state;
      }
  }
  return Proxy_Unknown;
}

static bool GetProxyConfig(ProxyConfig* config) {
  INTERNET_PER_CONN_OPTION options[] = {    
    {INTERNET_PER_CONN_FLAGS, 0},
    {INTERNET_PER_CONN_AUTOCONFIG_URL, 0},    
    {INTERNET_PER_CONN_PROXY_SERVER, 0},
    {INTERNET_PER_CONN_PROXY_BYPASS, 0},    
  };
  INTERNET_PER_CONN_OPTION_LIST list;     
  unsigned long nSize = sizeof INTERNET_PER_CONN_OPTION_LIST;

  list.dwSize = nSize;
  list.pszConnection = GetCurrentConnectionName();
  list.pOptions = options;
  list.dwOptionCount = sizeof options / sizeof INTERNET_PER_CONN_OPTION;
  list.dwOptionError = 0;
  if (InternetQueryOption(
	        NULL,
	        INTERNET_OPTION_PER_CONNECTION_OPTION,
	        &list,
	        &nSize)) {
    memset(config, 0, sizeof(ProxyConfig));
    DWORD conn_flag = options[0].Value.dwValue;
    config->auto_detect = (conn_flag & PROXY_TYPE_AUTO_DETECT) ==
                          PROXY_TYPE_AUTO_DETECT;
    config->auto_config = (conn_flag & PROXY_TYPE_AUTO_PROXY_URL) ==
                          PROXY_TYPE_AUTO_PROXY_URL;
    config->use_proxy = (conn_flag & PROXY_TYPE_PROXY) == PROXY_TYPE_PROXY;

    if (options[1].Value.pszValue != NULL) {
      config->auto_config_url = WStrToUtf8(options[1].Value.pszValue);
      GlobalFree(options[1].Value.pszValue);
    }
    if (options[2].Value.pszValue != NULL) {
      config->proxy_server = WStrToUtf8(options[2].Value.pszValue);
      GlobalFree(options[2].Value.pszValue);
    }
    if (options[3].Value.pszValue != NULL) {
      config->bypass_list = WStrToUtf8(options[3].Value.pszValue);
      GlobalFree(options[3].Value.pszValue);
    }
    DebugLog("npswitchproxy: InternetGetOption succeeded.\n");
    return true;
  }
  DebugLog("npswitchproxy: InternetGetOption failed.\n");
  return false;
}

/* Maintain C style instead of C++. 
   Otherwise use const ProxyConfig& config. */
static bool SetProxyConfig(const ProxyConfig* config) {
  INTERNET_PER_CONN_OPTION options[] = {    
    {INTERNET_PER_CONN_FLAGS, 0},
    {INTERNET_PER_CONN_AUTOCONFIG_URL, 0},    
    {INTERNET_PER_CONN_PROXY_SERVER, 0},
    {INTERNET_PER_CONN_PROXY_BYPASS, 0},    
  };
  INTERNET_PER_CONN_OPTION_LIST list;     
  unsigned long nSize = sizeof INTERNET_PER_CONN_OPTION_LIST;

  list.dwSize = nSize;
  list.pszConnection = GetCurrentConnectionName();
  list.pOptions = options;
  list.dwOptionCount = sizeof options / sizeof INTERNET_PER_CONN_OPTION;
  list.dwOptionError = 0;
  options[0].Value.dwValue = PROXY_TYPE_DIRECT;
  if (config->auto_config) {
    options[0].Value.dwValue |= PROXY_TYPE_AUTO_PROXY_URL;
  }
  if (config->auto_detect) {
    options[0].Value.dwValue |= PROXY_TYPE_AUTO_DETECT;
  }
  if (config->use_proxy) {
    options[0].Value.dwValue |= PROXY_TYPE_PROXY;
  }
  if (config->auto_config_url != NULL) {
    options[1].Value.pszValue = (LPWSTR) config->auto_config_url;
  }
  if (config->proxy_server != NULL) {
    options[2].Value.pszValue = (LPWSTR) config->proxy_server;
  }
  if (config->bypass_list != NULL) {
    options[3].Value.pszValue = (LPWSTR) config->bypass_list;
  }
  if (InternetSetOption(NULL, INTERNET_OPTION_PER_CONNECTION_OPTION,
                        &list, nSize)) {
    DebugLog("npswitchproxy: InternetSetOption succeeded.\n");
    return true;
  }
  DebugLog("npswitchproxy: InternetSetOption failed.\n");
  return false;
}

#endif /* WIN32 */

/* NPN */

static bool HasMethod(NPObject* obj, NPIdentifier methodName) {
  DebugLog("npswitchproxy: HasMethod\n");
  return true;
}

static bool InvokeDefault(NPObject* obj, const NPVariant* args,
                          uint32_t argCount, NPVariant* result) {
  DebugLog("npswitchproxy: InvokeDefault\n");	
  return true;
}

static bool InvokeGetProxyState(NPObject* obj, const NPVariant* args,
                                uint32_t argCount, NPVariant* result) {
  DebugLog("npswitchproxy: InvokeGetProxyState\n");
  ProxyState state = GetProxyState();
  result->type = NPVariantType_Int32;	
  if (state == Proxy_On) {
    result->value.intValue = 1;
  } else if (state == Proxy_Off) {
    result->value.intValue = 0;
  } else {
    result->value.intValue = -1;
  }
  return true;
}

static bool InvokeToggleProxyState(NPObject* obj, const NPVariant* args,
                                   uint32_t argCount, NPVariant* result) {
 DebugLog("npswitchproxy: InvokeToggleProxyState\n");
 ProxyState state = ToggleProxyState();
 result->type = NPVariantType_Int32;	
 if (state == Proxy_On) {
   result->value.intValue = 1;
 } else if (state == Proxy_Off) {
   result->value.intValue = 0;
 } else {
   result->value.intValue = -1;
 }
 return true;
}

static bool InvokeGetProxyConfig(NPObject* obj, const NPVariant* args,
                                 uint32_t argCount, NPVariant* result) {  
  return true;
}

static bool InvokeSetProxyConfig(NPObject* obj, const NPVariant* args,
                                 uint32_t argCount, NPVariant* result) {
  return true;
}

static bool GetConnectionName(NPObject* obj, NPVariant* result) {
  DebugLog("npswitchproxy: GetConnectionName\n");  
  NPString str;
  GetCurrentConnectionName();
  str.utf8characters = npnfuncs->utf8fromidentifier(
    npnfuncs->getstringidentifier(utf8_connection_name));
  str.utf8length = strlen(utf8_connection_name);
  result->type = NPVariantType_String;
  result->value.stringValue = str;  
  return true;
}

static bool Invoke(NPObject* obj, NPIdentifier methodName,
                   const NPVariant* args, uint32_t argCount,
                   NPVariant* result) {
 DebugLog("npswitchproxy: Invoke\n");
 char* name = npnfuncs->utf8fromidentifier(methodName);
 bool ret_val = false;
 if (!name) {
   return false;
 }
 if(!strncmp((const char*)name, kGetProxyStateMethod,
             strlen(kGetProxyStateMethod))) {
   ret_val = InvokeGetProxyState(obj, args, argCount, result);
 } else if (!strncmp((const char*)name, kToggleProxyStateMethod,
                     strlen(kToggleProxyStateMethod))) {
   ret_val = InvokeToggleProxyState(obj, args, argCount, result);
 } else if (!strncmp((const char*)name, kGetProxyConfigMethod,
                     strlen(kGetProxyConfigMethod))) {
   ret_val = InvokeGetProxyConfig(obj, args, argCount, result);
 } else if (!strncmp((const char*)name, kSetProxyConfigMethod,
                     strlen(kSetProxyConfigMethod))) {
   ret_val = InvokeSetProxyConfig(obj, args, argCount, result);
 } else {
   /* Aim exception handling */
   npnfuncs->setexception(obj, "exception during invocation");
   return false;
 }
 if (name) {
   npnfuncs->memfree(name);
 }
 return ret_val;
}

static bool HasProperty(NPObject* obj, NPIdentifier propertyName) {
  DebugLog("npswitchproxy: HasProperty\n");
  char* name = npnfuncs->utf8fromidentifier(propertyName);
  bool ret_val = false;
  if (name && 
      !strncmp((const char*)name, kGetConnectionNameProperty,
               strlen(kGetConnectionNameProperty))) {
    ret_val = true;
  }

  if (name) {
    npnfuncs->memfree(name);
  }
  return ret_val;
}

static bool GetProperty(NPObject* obj, NPIdentifier propertyName,
                        NPVariant* result) {
  DebugLog("npswitchproxy: GetProperty\n");
  char* name = npnfuncs->utf8fromidentifier(propertyName);
  bool ret_val = false;
  if (name && !strncmp((const char*)name, kGetConnectionNameProperty,
                       strlen(kGetConnectionNameProperty))) {    
    ret_val = GetConnectionName(obj, result);
  }
  if (name) {
    npnfuncs->memfree(name);
  }
  return ret_val;
}

static NPClass npcRefObject = {
  NP_CLASS_STRUCT_VERSION,
  NULL,
  NULL,
  NULL,
  HasMethod,
  Invoke,
  InvokeDefault,
  HasProperty,
  GetProperty,
  NULL,
  NULL,
};

/* NPP */

static NPError NewNPInstance(NPMIMEType pluginType, NPP instance,
                             uint16 mode, int16 argc, char* argn[],
                             char* argv[], NPSavedData* saved) {
  DebugLog("npswitchproxy: new\n");
  return NPERR_NO_ERROR;
}

static NPError DestroyNPInstance(NPP instance, NPSavedData** save) {
  if(so) {
    npnfuncs->releaseobject(so);
  }
  so = NULL;
  DebugLog("npswitchproxy: DestroyNPInstance\n");
  return NPERR_NO_ERROR;
}

static NPError GetValue(NPP instance, NPPVariable variable, void* value) {
  switch(variable) {
  default:
    DebugLog("npswitchproxy: GetValue - default\n");
    return NPERR_GENERIC_ERROR;
  case NPPVpluginNameString:
    DebugLog("npswitchproxy: GetValue - name string\n");
    *((char **)value) = "SwitchProxyPlugin";
    break;
  case NPPVpluginDescriptionString:
    DebugLog("npswitchproxy: GetValue - description string\n");
    *((char **)value) = "SwitchProxyPlugin plugin.";
    break;
  case NPPVpluginScriptableNPObject:
    DebugLog("npswitchproxy: GetValue - scriptable object\n");
    if(!so)
      so = npnfuncs->createobject(instance, &npcRefObject);
    npnfuncs->retainobject(so);
    *(NPObject **)value = so;
    break;
#if defined(XULRUNNER_SDK)
  case NPPVpluginNeedsXEmbed:
    DebugLog("npswitchproxy: GetValue - xembed\n");
    *((PRBool *)value) = PR_FALSE;
    break;
#endif
  }
  return NPERR_NO_ERROR;
}

/* expected by Safari on Darwin */
static NPError HandleEvent(NPP instance, void* ev) {
  DebugLog("npswitchproxy: HandleEvent\n");
  return NPERR_NO_ERROR;
}

/* expected by Opera */
static NPError SetWindow(NPP instance, NPWindow* pNPWindow) {
  DebugLog("npswitchproxy: SetWindow\n");
  return NPERR_NO_ERROR;
}

/* EXPORT */
#ifdef __cplusplus
extern "C" {
#endif

  NPError OSCALL NP_GetEntryPoints(NPPluginFuncs* nppfuncs) {
    DebugLog("npswitchproxy: NP_GetEntryPoints\n");
    nppfuncs->version = (NP_VERSION_MAJOR << 8) | NP_VERSION_MINOR;
    nppfuncs->newp = NewNPInstance;
    nppfuncs->destroy = DestroyNPInstance;
    nppfuncs->getvalue = GetValue;
    nppfuncs->event = HandleEvent;
    nppfuncs->setwindow = SetWindow;
    return NPERR_NO_ERROR;
  }

#ifndef HIBYTE
#define HIBYTE(x) ((((uint32)(x)) & 0xff00) >> 8)
#endif

  NPError OSCALL NP_Initialize(NPNetscapeFuncs* npnf
#if !defined(_WINDOWS) && !defined(WEBKIT_DARWIN_SDK)
    , NPPluginFuncs *nppfuncs) {
#else
    ) {
#endif	
      DebugLog("npswitchproxy: NP_Initialize\n");
      if(npnf == NULL) {
        return NPERR_INVALID_FUNCTABLE_ERROR;
      }
      if(HIBYTE(npnf->version) > NP_VERSION_MAJOR) {
        return NPERR_INCOMPATIBLE_VERSION_ERROR;
      }
      npnfuncs = npnf;
#if !defined(_WINDOWS) && !defined(WEBKIT_DARWIN_SDK)
      NP_GetEntryPoints(nppfuncs);
#endif
#ifdef WIN32
      if (!LoadWinInetDll()) {
        return NPERR_MODULE_LOAD_FAILED_ERROR;
      }
#endif
      return NPERR_NO_ERROR;
  }

  NPError	OSCALL NP_Shutdown() {
    DebugLog("npswitchproxy: NP_Shutdown\n");
#ifdef WIN32
    UnloadWinInetDll();
#endif
    return NPERR_NO_ERROR;
  }

  char* NP_GetMIMEDescription(void) {
    DebugLog("npswitchproxy: NP_GetMIMEDescription\n");
    return "application/x-switch-wininet-proxy::wzzhu@cs.hku.hk";
  }

  /* needs to be present for WebKit based browsers */
  NPError OSCALL NP_GetValue(void* npp, NPPVariable variable, void* value) {
    return GetValue((NPP)npp, variable, value);
  }

#ifdef __cplusplus
}
#endif