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

#include "proxy_config.h"

#include <npapi.h>
#include <npfunctions.h>
#include <npruntime.h>

#include "npswitchproxy.h"

const char* kAutoDetectProperty = "autoDetect";
const char* kAutoConfigProperty = "autoConfig";
const char* kUseProxyProperty = "useProxy";
const char* kAutoConfigUrlProperty = "autoConfigUrl";
const char* kProxyServerProperty = "proxyServer";
const char* kBypassListProperty = "bypassList";

const int kMaxPropertyNameLen = 20;
static const char* properties[] = {
  kAutoDetectProperty,
  kAutoConfigProperty,
  kUseProxyProperty,
  kAutoConfigUrlProperty,
  kProxyServerProperty,
  kBypassListProperty,
};

inline bool SafeStrCmp(const char* src, const char* target) {
  return strncmp(src, target, kMaxPropertyNameLen) == 0;
}

static NPObject* Allocate(NPP instance, NPClass* npclass) {
  return (NPObject*) new ProxyConfigObj;
}

static void Deallocate(NPObject* obj) {
  delete((ProxyConfigObj*) obj);
}

static bool HasProperty(NPObject* obj, NPIdentifier propertyName) {
  DebugLog("npswitchproxy: ProxyConfigHasProperty\n");
  NPUTF8* name = npnfuncs->utf8fromidentifier(propertyName);
  if (!name) {
    return false;
  }
  bool ret_val = false;
  for (int i = 0; i < sizeof(properties) / sizeof(const char*); ++i) {
    if (SafeStrCmp((const char*)name, properties[i])) {
      ret_val = true;
      break;
    }
  }
  npnfuncs->memfree(name);
  return ret_val;
}

static bool GetProperty(NPObject* obj, NPIdentifier propertyName,
                        NPVariant* result) {
  DebugLog("npswitchproxy: ProxyConfigGetProperty\n");
  NPUTF8* name = npnfuncs->utf8fromidentifier(propertyName);
  if (!name) {
    return false;
  }
  bool rc = true;
  const ProxyConfig& config = ((ProxyConfigObj*) obj)->config;
  if (SafeStrCmp(name, kAutoDetectProperty)) {
    BOOLEAN_TO_NPVARIANT(config.auto_detect, *result);
  } else if (SafeStrCmp(name, kAutoConfigProperty)) {
    BOOLEAN_TO_NPVARIANT(config.auto_config, *result);
  } else if (SafeStrCmp(name, kUseProxyProperty)) {
    BOOLEAN_TO_NPVARIANT(config.use_proxy, *result);
  } else if (SafeStrCmp(name, kAutoConfigUrlProperty)) {
    if (config.auto_config_url) {
      char* utf8_str = npnfuncs->utf8fromidentifier(
          npnfuncs->getstringidentifier(config.auto_config_url));
      STRINGZ_TO_NPVARIANT(utf8_str, *result);
    }
  } else if (SafeStrCmp(name, kProxyServerProperty)) {
    if (config.proxy_server) {
      char* utf8_str = npnfuncs->utf8fromidentifier(
          npnfuncs->getstringidentifier(config.proxy_server));
      STRINGZ_TO_NPVARIANT(utf8_str, *result);      
    }
  } else if (SafeStrCmp(name, kBypassListProperty)) {
    if (config.bypass_list) {
      char* utf8_str = npnfuncs->utf8fromidentifier(
          npnfuncs->getstringidentifier(config.bypass_list));
      STRINGZ_TO_NPVARIANT(utf8_str, *result);      
    }
  } else {
    rc = false;
  }
  npnfuncs->memfree(name);
  return rc;
}

static NPClass proxy_config_ref_obj = {
  NP_CLASS_STRUCT_VERSION,
  Allocate,
  Deallocate,
  NULL,
  NULL,
  NULL,
  NULL,
  HasProperty,
  GetProperty,
  NULL,
  NULL,
};

ProxyConfigObj* CreateProxyConfigObj(NPP instance) {
  ProxyConfigObj* obj = (ProxyConfigObj*) npnfuncs->createobject(
      instance, &proxy_config_ref_obj);
  return obj;
}