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

#ifndef __PROXY_CONFIG_H__
#define __PROXY_CONFIG_H__
#include <npapi.h>
#include <npupp.h>
#include <npruntime.h>

struct ProxyConfig {
  ProxyConfig() {
    auto_detect = false;
    auto_config = false;
    use_proxy = false;
    auto_config_url = NULL;
    proxy_server = NULL;
    bypass_list = NULL;
  }

  ~ProxyConfig() {
    delete auto_config_url;
    delete proxy_server;
    delete bypass_list;
  }

  bool auto_detect;
  bool auto_config;
  bool use_proxy;
  char* auto_config_url;
  char* proxy_server;
  char* bypass_list;
};

struct ProxyConfigObj : NPObject {
  ProxyConfig config;
};

ProxyConfigObj* CreateProxyConfigObj(NPP instance);
#endif  // __PROXY_CONFIG_H__