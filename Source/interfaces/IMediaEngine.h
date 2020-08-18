/**
* If not stated otherwise in this file or this component's LICENSE
* file the following copyright and licenses apply:
*
* Copyright 2020 RDK Management
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
**/

#pragma once

#include "Module.h"

namespace WPEFramework {
namespace Exchange {

struct IMediaEngine : virtual public Core::IUnknown {

    enum { ID = ID_MEDIAENGINE };

    // events callback
    struct ICallback : virtual public Core::IUnknown {

        enum { ID = ID_MEDIAENGINE_NOTIFICATION };

        virtual ~ICallback() {}

        virtual void Event(const string& id, const string& eventName, const string& parametersJson) = 0;
    };

    virtual ~IMediaEngine() {}

    // generic methods
    virtual uint32_t Create(const string& id) = 0;
    virtual uint32_t Destroy(const string& id) = 0;
    virtual uint32_t Load(const string& id, const string& url, bool autoPlay) = 0;
    virtual uint32_t Play(const string& id) = 0;
    virtual uint32_t Pause(const string& id) = 0;
    virtual uint32_t SeekTo(const string& id, int32_t positionSec) = 0;
    virtual uint32_t Stop(const string& id) = 0;
    virtual uint32_t InitConfig(const string& id, const string& configurationJson) = 0;
    virtual uint32_t InitDRMConfig(const string& id, const string& configurationJson) = 0;
    virtual void RegisterCallback(IMediaEngine::ICallback* callback) = 0;
};

}
}
