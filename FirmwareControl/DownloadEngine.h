/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
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
 */

#pragma once

#include "Module.h"

namespace WPEFramework {

struct INotifier {
    virtual ~INotifier() {}
    virtual void NotifyStatus(const uint32_t status) = 0;
    virtual void NotifyProgress(const uint8_t percentage) = 0;
};

namespace PluginHost {

    class DownloadEngine : public Web::ClientTransferType<Core::SocketStream, Web::SignedFileBodyType<Crypto::SHA256>> {

    private:
        typedef Web::ClientTransferType<Core::SocketStream, Web::SignedFileBodyType<Crypto::SHA256>> BaseClass;

        DownloadEngine() = delete;
        DownloadEngine(const DownloadEngine&) = delete;
        DownloadEngine& operator=(const DownloadEngine&) = delete;

    public:
        DownloadEngine(INotifier* notifier, const string& downloadStorage)
            : BaseClass(false, Core::NodeId(_T("0.0.0.0")), Core::NodeId(), 1024, ((64 * 1024) - 1))
            , _notifier(notifier)
            , _storage(downloadStorage.c_str(), false)
            , _interval(0)
            , _fileSize(0)
            , _activity(*this)
            , _destination(nullptr)
        {
        }
        virtual ~DownloadEngine()
        {
            _adminLock.Lock();
            _activity.Revoke();
            _adminLock.Unlock();
        }

    public:
        uint32_t Start(const string& locator, const string& destination, const string& hash)
        {
            Core::URL url(locator);
            uint32_t result = (url.IsValid() == true ? Core::ERROR_INPROGRESS : Core::ERROR_INCORRECT_URL);

            if (result == Core::ERROR_INPROGRESS) {

                _adminLock.Lock();

                CleanupStorage();

                if (_storage.IsOpen() == false) {

                    result = Core::ERROR_OPENING_FAILED;

                    if (_storage.Create() == true) {

                        _hash = hash;

                        result = BaseClass::Download(url, _storage);
                    }
                }

                _adminLock.Unlock();
            }

            return (result);
        }

        void StartProgressNotifier(const uint16_t interval)
        {
            _adminLock.Lock();
            _interval = interval;
            _activity.Revoke();

            if (interval) {
                _activity.Schedule(Core::Time::Now().Add(_interval * 1000));
            }

            _adminLock.Unlock();
        }

        void NotifyProgress()
        {
            uint8_t percentage = 0;

            _adminLock.Lock();
            if (_notifier != nullptr) {
                _storage.LoadFileInfo();
                if (_storage.Exists() && (_storage.Size() > 0) && (_fileSize > 0)) {
                    percentage = static_cast<uint8_t>(static_cast<float>(((_storage.Size() * 100)/static_cast<uint64_t>(_fileSize))));
                    if (percentage) {
                        _notifier->NotifyProgress(percentage);
                    }
                }
                if (percentage < 100) {
                    _activity.Schedule(Core::Time::Now().Add(_interval * 1000));
                }

            }
            _adminLock.Unlock();

            return;
        }
        uint32_t CheckHMAC()
        {
            uint32_t status = Core::ERROR_NONE;
            uint8_t hashHex[Crypto::HASH_SHA256];
            if (HashStringToBytes(_hash, hashHex) == true) {

                if (_destination) {
                    const uint8_t* downloadedHash = _destination->SerializedHashValue();
                    if (downloadedHash != nullptr) {
                        for (uint16_t i = 0; i < Crypto::HASH_SHA256; i++) {
                            if (downloadedHash[i] != hashHex[i]) {
                                status = Core::ERROR_INCORRECT_HASH;
                                break;
                            }
                        }
                    }
                }
            }
            return status;
        }

        inline void CleanupStorage()
        {
            if (_storage.Exists()) {
                _storage.Destroy();
            }
        }
    private:
        virtual void Started(const uint32_t size) override
        {
            _adminLock.Lock();
            _fileSize = size;
            _adminLock.Unlock();
        }

        virtual void Transfered(const uint32_t result, const Web::SignedFileBodyType<Crypto::SHA256>& destination) override
        {
            _adminLock.Lock();
            if (result == Core::ERROR_NONE) {
                if (_hash.empty() != true) {
                    _destination = &destination;
                }
            }
            if (_notifier != nullptr) {
                _notifier->NotifyStatus(result);
            }

            _storage.Close();
            _adminLock.Unlock();
        }

        virtual bool Setup(const Core::URL& remote) override
        {
            bool result = false;

            if (remote.Host().IsSet() == true) {
                uint16_t portNumber(remote.Port().IsSet() ? remote.Port().Value() : 80);

                BaseClass::Link().RemoteNode(Core::NodeId(remote.Host().Value().c_str(), portNumber));

                result = true;
            }
            return (result);
        }

        inline bool HashStringToBytes(const std::string& hash, uint8_t (&hashHex)[Crypto::HASH_SHA256])
        {
            bool status = true;

            for (uint8_t i = 0; i < Crypto::HASH_SHA256; i++) {
                char highNibble = hash.c_str()[i * 2];
                char lowNibble = hash.c_str()[(i * 2) + 1];
                if (isxdigit(highNibble) && isxdigit(lowNibble)) {
                    std::string byteStr = hash.substr(i * 2, 2);
                    hashHex[i] = static_cast<uint8_t>(strtol(byteStr.c_str(), nullptr, 16));
                }
                else {
                    status = false;
                    break;
                }
            }
	    return status;
        }

        friend Core::ThreadPool::JobType<DownloadEngine&>;
        void Dispatch()
        {
           NotifyProgress();
        }

    private:
        string _hash;
        Core::CriticalSection _adminLock;
        INotifier* _notifier;
        Core::File _storage;
        uint16_t _interval;
        uint32_t _fileSize;
        Core::WorkerPool::JobType<DownloadEngine&> _activity;
        const Web::SignedFileBodyType<Crypto::SHA256>* _destination;
    };
}
}
