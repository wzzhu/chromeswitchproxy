/* ***** BEGIN LICENSE BLOCK *****
* Copyright 2009-2010. Wenzhang Zhu (wzzhu@cs.hku.hk)
* Version: MPL 1.1/GPL 2.0/LGPL 2.1
*
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

#ifndef __NPSWITCHPROXY_H__
#define __NPSWITCHPROXY_H__

#include <npapi.h>
#include <npupp.h>
#include <npruntime.h>

struct PluginObj : NPObject {
  NPP npp;
};
extern NPNetscapeFuncs* npnfuncs;
void DebugLog(const char* msg);
extern const char* kGetProxyConfigMethod;
extern const char* kSetProxyConfigMethod;
extern const char* kGetConnectionNameProperty;

#endif  // __NPSWITCHPROXY_H__
