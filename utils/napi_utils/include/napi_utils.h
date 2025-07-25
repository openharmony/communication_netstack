/*
 * Copyright (c) 2021-2022 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef COMMUNICATIONNETSTACK_NETSTACK_NAPI_UTILS_H
#define COMMUNICATIONNETSTACK_NETSTACK_NAPI_UTILS_H

#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <vector>
#include <functional>
#include <queue>
#include <mutex>

#include "initializer_list"
#include "napi/native_api.h"
#include "uv.h"

#include "securec.h"

namespace OHOS::NetStack::NapiUtils {
static constexpr int NETSTACK_NAPI_INTERNAL_ERROR = 2300002;
using UvHandler = std::function<void()>;
struct UvHandlerQueue : public std::queue<UvHandler> {
    UvHandler Pop();
    void Push(const UvHandler &handler);
private:
    std::mutex mutex;
};

struct SecureData : public std::string {
    ~SecureData()
    {
        // Clear Data, to keep the memory safe
        (void)memset_s(data(), size(), 0, size());
    }
};

napi_valuetype GetValueType(napi_env env, napi_value value);

bool IsInstanceOf(napi_env env, napi_value object, const std::string &name);

/* named property */
bool HasNamedProperty(napi_env env, napi_value object, const std::string &propertyName);

napi_value GetNamedProperty(napi_env env, napi_value object, const std::string &propertyName);

void SetNamedProperty(napi_env env, napi_value object, const std::string &name, napi_value value);

std::vector<std::string> GetPropertyNames(napi_env env, napi_value object);

/* UINT32 */
napi_value CreateUint32(napi_env env, uint32_t code);

napi_value CreateUint64(napi_env env, uint64_t code);

int64_t GetInt64FromValue(napi_env env, napi_value value);

int64_t GetInt64Property(napi_env env, napi_value object, const std::string &propertyName);

uint32_t GetUint32FromValue(napi_env env, napi_value value);

uint32_t GetUint32Property(napi_env env, napi_value object, const std::string &propertyName);

void SetUint32Property(napi_env env, napi_value object, const std::string &name, uint32_t value);

void SetUint64Property(napi_env env, napi_value object, const std::string &name, uint64_t value);

/* INT32 */
napi_value CreateInt32(napi_env env, int32_t code);

int32_t GetInt32FromValue(napi_env env, napi_value value);

int32_t GetInt32Property(napi_env env, napi_value object, const std::string &propertyName);

void SetInt32Property(napi_env env, napi_value object, const std::string &name, int32_t value);

void SetDoubleProperty(napi_env env, napi_value object, const std::string &name, double value);

/* String UTF8 */
napi_value CreateStringUtf8(napi_env env, const std::string &str);

std::string GetStringFromValueUtf8(napi_env env, napi_value value);

std::string GetStringPropertyUtf8(napi_env env, napi_value object, const std::string &propertyName);

void GetSecureDataFromValueUtf8(napi_env env, napi_value value, SecureData &data);

void GetSecureDataPropertyUtf8(
    napi_env env, napi_value object, const std::string &propertyName, SecureData &data);

std::string NapiValueToString(napi_env env, napi_value value);

void SetStringPropertyUtf8(napi_env env, napi_value object, const std::string &name, const std::string &value);

std::vector<std::string> GetDnsServers(napi_env env, napi_value object, const std::string &propertyName);

/* array buffer */
bool ValueIsArrayBuffer(napi_env env, napi_value value);

void *GetInfoFromArrayBufferValue(napi_env env, napi_value value, size_t *length);

napi_value CreateArrayBuffer(napi_env env, size_t length, void **data);

/* object */
napi_value CreateObject(napi_env env);

/* undefined */
napi_value GetUndefined(napi_env env);

/* function */
napi_value CallFunction(napi_env env, napi_value recv, napi_value func, size_t argc, const napi_value *argv);

napi_value CreateFunction(napi_env env, const std::string &name, napi_callback func, void *arg);

/* reference */
napi_ref CreateReference(napi_env env, napi_value callback);

napi_value GetReference(napi_env env, napi_ref callbackRef);

void DeleteReference(napi_env env, napi_ref callbackRef);

/* boolean */
bool GetBooleanProperty(napi_env env, napi_value object, const std::string &propertyName);

void SetBooleanProperty(napi_env env, napi_value object, const std::string &name, bool value);

napi_value GetBoolean(napi_env env, bool value);

bool GetBooleanFromValue(napi_env env, napi_value value);

/* define properties */
void DefineProperties(napi_env env, napi_value object,
                      const std::initializer_list<napi_property_descriptor> &properties);

/* array */
napi_value CreateArray(napi_env env, size_t length);
void SetArrayElement(napi_env env, napi_value array, uint32_t index, napi_value value);
bool IsArray(napi_env env, napi_value value);
void SetArrayProperty(napi_env env, napi_value object, const std::string &name, napi_value value);
uint32_t GetArrayLength(napi_env env, napi_value arr);
napi_value GetArrayElement(napi_env env, napi_value arr, uint32_t index);

/* JSON */
napi_value JsonStringify(napi_env env, napi_value object);

napi_value JsonParse(napi_env env, const std::string &inStr);

/* libuv */
void CreateUvQueueWork(napi_env env, void *data, void(handler)(uv_work_t *, int status));

void CreateUvQueueWorkEnhanced(napi_env env, void *data, void (*handler)(napi_env env, napi_status status, void *data));

/* scope */
napi_handle_scope OpenScope(napi_env env);

void CloseScope(napi_env env, napi_handle_scope scope);

/* error */
napi_value CreateErrorMessage(napi_env env, int32_t errorCodeconst, const std::string &errorMessage);

napi_value GetGlobal(napi_env env);

napi_value GetValueFromGlobal(napi_env env, const std::string &className);

void CreateUvQueueWorkByModuleId(napi_env env, const UvHandler &handler, uint64_t id);

uint64_t CreateUvHandlerQueue(napi_env env);

void HookForEnvCleanup(void *data);
void SetEnvValid(napi_env env);
bool IsEnvValid(napi_env env);
 
bool IsDebugMode();

std::string RemoveUrlParameters(const std::string& url);
} // namespace OHOS::NetStack::NapiUtils

#endif /* COMMUNICATIONNETSTACK_NETSTACK_NAPI_UTILS_H */
