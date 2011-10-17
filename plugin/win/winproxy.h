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

#ifndef __WIN_WINPROXY_H__
#define __WIN_WINPROXY_H__
#include <windows.h>
#include <wininet.h>
#include "../npswitchproxy.h"
#include "../proxy_config.h"
#include "../proxy_base.h"

class WinProxy : public ProxyBase {
 public:
  WinProxy();
  ~WinProxy();
  virtual bool PlatformDependentStartup();
  virtual void PlatformDependentShutdown();
  virtual bool GetActiveConnectionName(const void** connection_name);
  virtual bool GetProxyConfig(ProxyConfig* config);
  virtual bool SetProxyConfig(const ProxyConfig& config);

private:
  char* WStrToUtf8(LPCWSTR str);  // Helper functions to convert widechar to utf8.
  LPWSTR Utf8ToWStr(const char * str);
  typedef bool (__stdcall* InternetQueryOptionFunc) (
    HINTERNET hInternet, DWORD dwOption,
    LPVOID lpBuffer, LPDWORD lpdwBufferLength);

  typedef bool (__stdcall* InternetSetOptionFunc) (
      HINTERNET hInternet, DWORD dwOption,
      LPVOID lpBuffer, DWORD dwBufferLength);

  typedef bool (__stdcall* InternetGetConnectedStateExFunc) (
      LPDWORD lpdwFlags, LPTSTR lpszConnectionName,
      DWORD dwNameLen, DWORD dwReserved);
  HINSTANCE hWinInetLib_;
  InternetQueryOptionFunc pInternetQueryOption_;
  InternetSetOptionFunc pInternetSetOption_;
  InternetGetConnectedStateExFunc pInternetGetConnectedStateEx_;
};
#endif  // __WIN_WINPROXY_H__
