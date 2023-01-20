/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2023 Metrological
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
 */

#pragma once

#include "pluginshibernate/PluginsHibernate.h"
#include <condition_variable>

namespace WPEFramework {
namespace PluginsHibernate {

        class MemcrAdministrator: public IPluginsHibernateAdministrator
        {
        public:

            friend class Core::SingletonType<MemcrAdministrator>;

            MemcrAdministrator() {};
            MemcrAdministrator(const MemcrAdministrator&) = delete;
            MemcrAdministrator& operator=(const MemcrAdministrator&) = delete;

            enum ProcessedAppState
            {
                CHECKPOINTING,
                CHECKPOINTED,
                RESTORING
            };

            bool hibernate(const std::string &callSign, uint32_t timeouteMs) override;
            bool restore(const std::string &callSign, uint32_t timeouteMs) override;
            bool getState(const std::string &callSign, ProcessedAppState &state);
            bool isProcessed(const std::string &callSign) override;
            void removeFromProcessed(const std::string &callSign) override;
            bool isRemovedFromProcessed(const std::string &callSign, uint32_t waitTimeMs) override;

        private /*types and const*/:
            enum ServerRequestCode
            {
                MEMCR_CHECKPOINT = 100,
                MEMCR_RESTORE
            };

            enum ServerResponseCode
            {
                MEMCR_OK = 0,
                MEMCR_ERROR = -1
            };

            struct ServerRequest
            {
                ServerRequestCode reqCode;
                pid_t pid;
                int timeout;
            } __attribute__((packed));

            struct ServerResponse
            {
                ServerResponseCode respCode;
            } __attribute__((packed));

            const string MEMCR_SERVER_SOCKET = "/tmp/memcrservice";
            const int WATCHDOG_TIMEOUT_SEC = 10;

            typedef std::unique_lock<std::mutex> Lock;

        private /*methods*/:
            void launchRequestThread(ServerRequestCode request, const std::string &callSign, uint32_t timeouteMs);
            bool sendRcvCmd(ServerRequest &cmd, ServerResponse &resp, uint32_t timeouteMs);

        private /*members*/:
            std::mutex mProcessedAppsLock;
            std::map<std::string, ProcessedAppState> mProcessedApps;
            std::condition_variable mProcessedAppsChanged;
        };
  
}
}
