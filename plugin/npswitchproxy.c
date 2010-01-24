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

#include "npswitchproxy.h"

#include <stdio.h>
#include <string.h>
#include <npapi.h>
#include <npupp.h>
#include <npruntime.h>

#include "proxy_config.h"
#ifdef WIN32
#include "win/winproxy.h"
#endif

static NPObject* so = NULL;
NPNetscapeFuncs* npnfuncs = NULL;

const char* kGetProxyStateMethod = "getProxyState";
const char* kToggleProxyStateMethod = "toggleProxyState";
const char* kGetProxyConfigMethod = "getProxyConfig";
const char* kSetProxyConfigMethod = "setProxyConfig";
const char* kGetConnectionNameProperty = "connectionName";

void DebugLog(const char* msg) {
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

// Begin the implementation of scriptable plugin NPObject.

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
  PluginObj* plugin = (PluginObj*)obj;
  ProxyConfigObj* proxy = CreateProxyConfigObj(plugin->npp);
  if (!GetProxyConfig(&proxy->config)) {
    return false;
  }
  OBJECT_TO_NPVARIANT((NPObject*)proxy, *result);
  return true;
}

// We accept calls using the following forms:
// plugin.SetProxyConfig();  No proxy
// plugin.SetProxyConfig(use_proxy, proxy_server);
// plugin.SetProxyConfig(use_proxy, proxy_server, bypass_list);
// plugin.SetProxyConfig(use_proxy, proxy_server, bypass_list, auto_detect);
// plugin.SetProxyConfig(use_proxy, proxy_server, bypass_list, auto_detect,
// auto_config, auto_config_url);
static bool InvokeSetProxyConfig(NPObject* obj, const NPVariant* args,
                                 uint32_t argCount, NPVariant* result) {
  ProxyConfig config;
  NPString str[3];
  if (!GetProxyConfig(&config)) {
    return false;
  }
  if (argCount == 0) {
    config.use_proxy = false;
  } else if (argCount == 2 &&
             NPVARIANT_IS_BOOLEAN(args[0]) &&
             NPVARIANT_IS_STRING(args[1])) {
    config.use_proxy = NPVARIANT_TO_BOOLEAN(args[0]);
    str[0] = NPVARIANT_TO_STRING(args[1]);
    config.proxy_server = str[0].utf8characters;
  } else if (argCount == 3 &&
             NPVARIANT_IS_BOOLEAN(args[0]) &&
             NPVARIANT_IS_STRING(args[1]) &&
             NPVARIANT_IS_STRING(args[2])) {
    config.use_proxy = NPVARIANT_TO_BOOLEAN(args[0]);
    str[0] = NPVARIANT_TO_STRING(args[1]);
    config.proxy_server = str[0].utf8characters;
    str[1] = NPVARIANT_TO_STRING(args[2]);
    config.bypass_list = str[1].utf8characters;
  } else if (argCount == 4 &&
             NPVARIANT_IS_BOOLEAN(args[0]) &&
             NPVARIANT_IS_STRING(args[1]) &&
             NPVARIANT_IS_STRING(args[2]) &&
             NPVARIANT_IS_BOOLEAN(args[3])) {
    config.use_proxy = NPVARIANT_TO_BOOLEAN(args[0]);
    str[0] = NPVARIANT_TO_STRING(args[1]);
    config.proxy_server = str[0].utf8characters;
    str[1] = NPVARIANT_TO_STRING(args[2]);
    config.bypass_list = str[1].utf8characters;
    config.auto_detect = NPVARIANT_TO_BOOLEAN(args[3]);
  } else if (argCount == 6 &&
             NPVARIANT_IS_BOOLEAN(args[0]) &&
             NPVARIANT_IS_STRING(args[1]) &&
             NPVARIANT_IS_STRING(args[2]) &&
             NPVARIANT_IS_BOOLEAN(args[3]) &&
             NPVARIANT_IS_BOOLEAN(args[4]) &&
             NPVARIANT_IS_STRING(args[5])) {
    config.use_proxy = NPVARIANT_TO_BOOLEAN(args[0]);
    str[0] = NPVARIANT_TO_STRING(args[1]);
    config.proxy_server = str[0].utf8characters;
    str[1] = NPVARIANT_TO_STRING(args[2]);
    config.bypass_list = str[1].utf8characters;
    config.auto_detect = NPVARIANT_TO_BOOLEAN(args[3]);
    config.auto_config = NPVARIANT_TO_BOOLEAN(args[4]);
    str[2] = NPVARIANT_TO_STRING(args[5]);
    config.auto_config_url = str[2].utf8characters;
  }
  return SetProxyConfig(config);
}

static bool GetConnectionName(NPObject* obj, NPVariant* result) {
  DebugLog("npswitchproxy: GetConnectionName\n");  
  GetCurrentConnectionName();
  char* utf8_name = npnfuncs->utf8fromidentifier(
      npnfuncs->getstringidentifier(utf8_connection_name));
  STRINGZ_TO_NPVARIANT(utf8_name, *result);
  return true;
}

static NPObject* Allocate(NPP instance, NPClass* npclass) {
  return (NPObject*)new PluginObj;
}

static void Deallocate(NPObject* obj) {
  delete((PluginObj*) obj);
}

static bool HasMethod(NPObject* obj, NPIdentifier methodName) {
  DebugLog("npswitchproxy: HasMethod\n");
  return true;
}

static bool InvokeDefault(NPObject* obj, const NPVariant* args,
                          uint32_t argCount, NPVariant* result) {
  DebugLog("npswitchproxy: InvokeDefault\n");	
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
   // Aim exception handling. 
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
  if (name && !strncmp((const char*)name, kGetConnectionNameProperty,
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

static NPClass plugin_ref_obj = {
  NP_CLASS_STRUCT_VERSION,
  Allocate,
  Deallocate,
  NULL,
  HasMethod,
  Invoke,
  InvokeDefault,
  HasProperty,
  GetProperty,
  NULL,
  NULL,
};

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
    if(!so) {
      so = npnfuncs->createobject(instance, &plugin_ref_obj);
    }
    // Retain the object since we keep it in plugin code
    // so that it won't be freed by browser.
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
// End of the implementation of scriptable plugin NPObject.

// Expected by Safari on Darwin.
static NPError HandleEvent(NPP instance, void* ev) {
  DebugLog("npswitchproxy: HandleEvent\n");
  return NPERR_NO_ERROR;
}

// Expected by Opera.
static NPError SetWindow(NPP instance, NPWindow* pNPWindow) {
  DebugLog("npswitchproxy: SetWindow\n");
  return NPERR_NO_ERROR;
}

// EXPORT
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

// Needs to be present for WebKit based browsers.
NPError OSCALL NP_GetValue(void* npp, NPPVariable variable, void* value) {
  return GetValue((NPP)npp, variable, value);
}

#ifdef __cplusplus
}
#endif
