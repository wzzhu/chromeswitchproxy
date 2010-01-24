/* ***** BEGIN LICENSE BLOCK *****
* Copyright 2009 Wenzhang Zhu (wzzhu@cs.hku.hk)
* Version: MPL 1.1/GPL 2.0/LGPL 2.1
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

#include "winproxy.h"

#include <windows.h>
#include <wininet.h>

#include "../npswitchproxy.h"
#include "../proxy_config.h"

typedef bool (__stdcall* InternetQueryOptionFunc) (
    HINTERNET hInternet, DWORD dwOption,
    LPVOID lpBuffer, LPDWORD lpdwBufferLength);

typedef bool (__stdcall* InternetSetOptionFunc) (
    HINTERNET hInternet, DWORD dwOption,
    LPVOID lpBuffer, DWORD dwBufferLength);

typedef bool (__stdcall* InternetGetConnectedStateExFunc) (
    LPDWORD lpdwFlags, LPTSTR lpszConnectionName,
    DWORD dwNameLen, DWORD dwReserved);

HINSTANCE hWinInetLib;
InternetQueryOptionFunc pInternetQueryOption;
InternetSetOptionFunc pInternetSetOption;
InternetGetConnectedStateExFunc pInternetGetConnectedStateEx;

// The global buffer to hold the current Internet connection name.
const int kMaxLengthOfConnectionName = 1024;
wchar_t connection_name[kMaxLengthOfConnectionName];
char utf8_connection_name[kMaxLengthOfConnectionName];

// Convert the wide null-terminated string to utf8 null-terminated string.
// Note that the return value should be freed after use.
static char* WStrToUtf8(LPCWSTR str) {
  int bufferSize = WideCharToMultiByte(CP_UTF8, 0, (LPCWSTR)str, -1, NULL, 0, NULL, NULL);
	char* m = new char[bufferSize];
	WideCharToMultiByte(CP_UTF8, 0, (LPCWSTR)str, -1, m, bufferSize, NULL, NULL);
	return m;
}

bool LoadWinInetDll() {
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
    pInternetGetConnectedStateEx =
        (InternetGetConnectedStateExFunc)GetProcAddress(
            HMODULE(hWinInetLib), "InternetGetConnectedStateExA");
  }
  if (pInternetGetConnectedStateEx == NULL) {
    FreeLibrary(hWinInetLib);
    return false;
  }
  return true;
}

void UnloadWinInetDll() {
  if (hWinInetLib != NULL) {
    FreeLibrary(hWinInetLib);
  }
}

LPWSTR GetCurrentConnectionName() {  
  DWORD flags;  
  memset(utf8_connection_name, 0, kMaxLengthOfConnectionName);
  if (pInternetGetConnectedStateEx(
          &flags,(LPTSTR)connection_name,
          kMaxLengthOfConnectionName, NULL)) {
    WideCharToMultiByte(CP_UTF8, 0, (LPCWSTR)connection_name,
                        -1, utf8_connection_name, kMaxLengthOfConnectionName,
                        NULL, NULL);
    if ((flags & INTERNET_CONNECTION_LAN) == 0) {
      DebugLog("npswitchproxy: Got non-Lan connection.\n");
      return (LPWSTR) connection_name;
    }
  }  
  return NULL;
}
  
ProxyState GetProxyState() {  
  INTERNET_PER_CONN_OPTION options[1];
  options[0].dwOption = INTERNET_PER_CONN_FLAGS;
  INTERNET_PER_CONN_OPTION_LIST list;	
  unsigned long nSize = sizeof(INTERNET_PER_CONN_OPTION_LIST);   
  list.dwSize = nSize;  
  list.pszConnection = GetCurrentConnectionName();  
  list.pOptions = options;
  list.dwOptionCount = 1;
  list.dwOptionError = 0;	
  if (pInternetQueryOption(NULL, INTERNET_OPTION_PER_CONNECTION_OPTION,
                           &list, &nSize)) {
      DWORD proxyState = options[0].Value.dwValue;		
      if (proxyState & PROXY_TYPE_PROXY) {
        return Proxy_On;
      } else {
        return Proxy_Off;
      }
  }
  return Proxy_Unknown;
}

ProxyState ToggleProxyState() {
  INTERNET_PER_CONN_OPTION options[1];
  options[0].dwOption = INTERNET_PER_CONN_FLAGS;
  INTERNET_PER_CONN_OPTION_LIST list;	
  unsigned long nSize = sizeof(INTERNET_PER_CONN_OPTION_LIST);	
  list.dwSize = nSize;
  list.pszConnection = GetCurrentConnectionName();
  list.pOptions = options;
  list.dwOptionCount = 1;
  list.dwOptionError = 0;	
  if (pInternetQueryOption(NULL, INTERNET_OPTION_PER_CONNECTION_OPTION,
                           &list, &nSize)) {
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
      if (pInternetSetOption(NULL, INTERNET_OPTION_PER_CONNECTION_OPTION,
                             &list, nSize)) {
          return state;
      }
  }
  return Proxy_Unknown;
}

bool GetProxyConfig(ProxyConfig* config) {
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
  if (pInternetQueryOption(NULL, INTERNET_OPTION_PER_CONNECTION_OPTION,
                           &list, &nSize)) {
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

bool SetProxyConfig(const ProxyConfig& config) {
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
  if (config.auto_config) {
    options[0].Value.dwValue |= PROXY_TYPE_AUTO_PROXY_URL;
  }
  if (config.auto_detect) {
    options[0].Value.dwValue |= PROXY_TYPE_AUTO_DETECT;
  }
  if (config.use_proxy) {
    options[0].Value.dwValue |= PROXY_TYPE_PROXY;
  }
  if (config.auto_config_url != NULL) {
    options[1].Value.pszValue = (LPWSTR) config.auto_config_url;
  }
  if (config.proxy_server != NULL) {
    options[2].Value.pszValue = (LPWSTR) config.proxy_server;
  }
  if (config.bypass_list != NULL) {
    options[3].Value.pszValue = (LPWSTR) config.bypass_list;
  }
  if (pInternetSetOption(NULL, INTERNET_OPTION_PER_CONNECTION_OPTION,
                         &list, nSize)) {
    DebugLog("npswitchproxy: InternetSetOption succeeded.\n");
    return true;
  }
  DebugLog("npswitchproxy: InternetSetOption failed.\n");
  return false;
}
