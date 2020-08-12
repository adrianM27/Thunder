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

        virtual void Event(const uint32_t id, const string& eventName, const string& parameters) = 0;
    };

    virtual ~IMediaEngine() {}

    // generic methods
    virtual uint32_t Create(uint32_t id) = 0;
    virtual uint32_t Load(uint32_t id, const string& configuration) = 0;
    virtual uint32_t Play(uint32_t id) = 0;
    virtual uint32_t Pause(uint32_t id) = 0;
    virtual uint32_t SetPosition(uint32_t id, int32_t positionSec) = 0;
    virtual uint32_t Stop(uint32_t id) = 0;
    virtual void RegisterCallback(IMediaEngine::ICallback* callback) = 0;
};

}
}
