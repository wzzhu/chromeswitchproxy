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

bool PlatformDependentStartup();
void PlatformDependentShutdown();
bool GetActiveConnectionName(LPWSTR* connection_name);
bool GetProxyConfig(ProxyConfig* config);
bool SetProxyConfig(const ProxyConfig& config);
char* WStrToUtf8(LPCWSTR str);  // Helper functions to convert widechar to utf8.
#endif  // __WIN_WINPROXY_H__
