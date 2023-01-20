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

#include "MemcrImplementation.h"

/*
    TODO: 
        - add tracing instead of printfs
        - monitor hibernated plugins to remove them from processed list if they disappear
        - prevent hibernation of own PID (local plugin)
        - replace FindPid with Core method
        - at least the wakeup as blocking method, specially if we have sequence of hibernate->wakeup calls
*/

namespace WPEFramework {

namespace PluginsHibernate {

    IPluginsHibernateAdministrator& PluginsHibernate::IPluginsHibernateAdministrator::Instance()
    {
        static MemcrAdministrator& memcrAdministrator = Core::SingletonType<MemcrAdministrator>::Instance();

        return memcrAdministrator;
    }

    static void ProcessName(const uint32_t pid, TCHAR buffer[], const uint32_t maxLength)
    {
        ASSERT(maxLength > 0);

        if (maxLength > 0) {
            char procpath[48];
            int fd;

            snprintf(procpath, sizeof(procpath), "/proc/%u/comm", pid);

            if ((fd = open(procpath, O_RDONLY)) > 0) {
                ssize_t size;
                if ((size = read(fd, buffer, maxLength - 1)) > 0) {
                    if (buffer[size - 1] == '\n') {
                        buffer[size - 1] = '\0';
                    } else {
                        buffer[size] = '\0';
                    }
                } else {
                    buffer[0] = '\0';
                }

                close(fd);
            } else {
                buffer[0] = '\0';
            }
        }
    }

    static pid_t FindPid(const string& name)
    {
        DIR* dp;
        struct dirent* ep;

        pid_t retPid = -1;

        dp = opendir("/proc");
        if (dp != nullptr) {
            while (nullptr != (ep = readdir(dp))) {
                pid_t pid;
                char* endptr;

                pid = strtol(ep->d_name, &endptr, 10);

                if ('\0' == endptr[0]) {
                    // We have a valid PID, Find, the parent of this process..
                    TCHAR buffer[512];
                    ProcessName(pid, buffer, sizeof(buffer));
                    if (name == buffer) {
                        retPid = pid;
                        break;
                    }
                }
            }

            (void)closedir(dp);
        }

        return retPid;
    }

    bool MemcrAdministrator::hibernate(const std::string& callSign, uint32_t timeoutMs)
    {
        bool isCheckpointAllowed = false;

        MemcrAdministrator::Lock lock(mProcessedAppsLock);
        if (mProcessedApps.find(callSign) == mProcessedApps.end()) {
            // App not processed
            isCheckpointAllowed = true;
        } else if (mProcessedApps[callSign] == MemcrAdministrator::RESTORING) {
            // App being processeed, then shedule checkpoint only if restore is ongoing
            isCheckpointAllowed = true;
        }
        //TODO: check if it is not own PID

        if (isCheckpointAllowed) {
            mProcessedApps[callSign] = MemcrAdministrator::CHECKPOINTING;
        }

        lock.unlock();

        if (isCheckpointAllowed) {
            printf("MemcrAdministrator: Launching Checkpoint Thread for %s\n", callSign.c_str());fflush(stdout);
            launchRequestThread(MemcrAdministrator::MEMCR_CHECKPOINT, callSign, timeoutMs);
            return true;
        }

        printf("MemcrAdministrator: Checkpoint not allowed for %s\n", callSign.c_str());fflush(stdout);
        return false;
    }

    bool MemcrAdministrator::wakeup(const std::string& callSign, uint32_t timeoutMs)
    {
        bool isRestoreAllowed = false;

        MemcrAdministrator::Lock lock(mProcessedAppsLock);
        if (mProcessedApps.find(callSign) != mProcessedApps.end()
            && (mProcessedApps[callSign] == MemcrAdministrator::CHECKPOINTING
                || mProcessedApps[callSign] == MemcrAdministrator::CHECKPOINTED)) {
            // Apps checkpoint ongoing or already checkpointed
            isRestoreAllowed = true;
            mProcessedApps[callSign] = MemcrAdministrator::RESTORING;
        }

        lock.unlock();

        if (isRestoreAllowed) {
            printf("MemcrAdministrator: Launching Restore Thread for %s\n", callSign.c_str());fflush(stdout);
             launchRequestThread(MemcrAdministrator::MEMCR_RESTORE, callSign, timeoutMs);
            return true;
        }

        printf("MemcrAdministrator: Restore not allowed for %s\n", callSign.c_str());fflush(stdout);
        return false;
    }

    bool MemcrAdministrator::isHibernated(const std::string& callSign)
    {
        bool isProcessed = false;
        MemcrAdministrator::Lock lock(mProcessedAppsLock);
        isProcessed = (mProcessedApps.find(callSign) != mProcessedApps.end());

        return isProcessed;
    }

    void MemcrAdministrator::removeFromProcessed(const std::string& callSign)
    {
        MemcrAdministrator::Lock lock(mProcessedAppsLock);
        if (mProcessedApps.find(callSign) != mProcessedApps.end()) {
            mProcessedApps.erase(callSign);
        }
    }

    void MemcrAdministrator::launchRequestThread(
        MemcrAdministrator::ServerRequestCode cmd,
        const std::string& callSign,
        uint32_t timeoutMs)
    {
        std::thread requestsThread = std::thread([=]() {
            bool success = false;
            std::string errorMsg;

            std::list<uint32_t> processPids;
            pid_t procPid = FindPid(callSign);

            if (procPid > 0) {
                printf("MemcrAdministrator: Pid found for %s: %d\n", callSign.c_str(), procPid);fflush(stdout);

                MemcrAdministrator::ServerRequest req = {
                    .reqCode = cmd,
                    .pid = procPid,
                    .timeout = timeoutMs
                };

                MemcrAdministrator::ServerResponse resp;

                success = sendRcvCmd(req, resp, timeoutMs);
                if (!success) {
                    errorMsg = std::to_string(static_cast<int>(resp.respCode));
                }
            } else {
                printf("MemcrAdministrator: Pid not found for %s\n", callSign.c_str());fflush(stdout);
                errorMsg = "Process not found";
                success = false;
            }

            if (!success) {
                removeFromProcessed(callSign);
            }

            if (cmd == MemcrAdministrator::MEMCR_CHECKPOINT) {
                MemcrAdministrator::Lock lock(mProcessedAppsLock);
                if (success && mProcessedApps.find(callSign) != mProcessedApps.end()
                    && mProcessedApps[callSign] == MemcrAdministrator::CHECKPOINTING) {
                    mProcessedApps[callSign] = MemcrAdministrator::CHECKPOINTED;
                }

            } else if (cmd == MemcrAdministrator::MEMCR_RESTORE) {
                MemcrAdministrator::Lock lock(mProcessedAppsLock);
                if (success && mProcessedApps.find(callSign) != mProcessedApps.end()
                    && mProcessedApps[callSign] == MemcrAdministrator::RESTORING) {
                    mProcessedApps.erase(callSign);
                }
            }
        });
        requestsThread.detach();
    }

    bool MemcrAdministrator::sendRcvCmd(
        MemcrAdministrator::ServerRequest& cmd,
        MemcrAdministrator::ServerResponse& resp,
        uint32_t timeoutMs)
    {
        int cd;
        int ret;
        struct sockaddr_un addr = { 0 };
        resp.respCode = MemcrAdministrator::ServerResponseCode::MEMCR_ERROR;

        cd = socket(PF_UNIX, SOCK_STREAM, 0);
        if (cd < 0) {
            printf("MemcrAdministrator: Socket create failed: %d", cd);fflush(stdout);
            return false;
        }

        struct timeval rcvTimeout;
        rcvTimeout.tv_sec = timeoutMs / 1000;
        rcvTimeout.tv_usec = (timeoutMs % 1000) * 1000;

        setsockopt(cd, SOL_SOCKET, SO_RCVTIMEO, &rcvTimeout, sizeof(rcvTimeout));

        addr.sun_family = PF_UNIX;
        snprintf(addr.sun_path, sizeof(addr.sun_path),
            MemcrAdministrator::MEMCR_SERVER_SOCKET.c_str());

        ret = connect(cd, (struct sockaddr*)&addr, sizeof(struct sockaddr_un));
        if (ret < 0) {
            printf("MemcrAdministrator: Socket connect failed: %d with %s", ret,
                MemcrAdministrator::MEMCR_SERVER_SOCKET.c_str());fflush(stdout);
            close(cd);
            return false;
        }

        ret = write(cd, &cmd, sizeof(cmd));
        if (ret != sizeof(cmd)) {
            printf("MemcrAdministrator: Socket write failed: ret %d", ret);fflush(stdout);
            close(cd);
            return false;
        }

        ret = read(cd, &resp, sizeof(resp));
        if (ret != sizeof(resp)) {
            printf("MemcrAdministrator: Socket read failed: ret %d", ret);fflush(stdout);
            close(cd);
            return false;
        }

        close(cd);

        return (resp.respCode == MemcrAdministrator::ServerResponseCode::MEMCR_OK);
    }

} // namespace ProcessContainers

} // namespace WPEFramework
