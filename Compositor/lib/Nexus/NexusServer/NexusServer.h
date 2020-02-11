#ifndef PLATFORMIMPLEMENTATION_H
#define PLATFORMIMPLEMENTATION_H

#include <core/core.h>
#include <nexus_config.h>
#include <nxserverlib.h>

#include <interfaces/IComposition.h>

namespace WPEFramework {
namespace Broadcom {

    class Platform {
    private:
        Platform() = delete;
        Platform(const Platform&) = delete;
        Platform& operator=(const Platform&) = delete;

    public:
        class Client : public Exchange::IComposition::IClient {
        private:
            Client() = delete;
            Client(const Client&) = delete;
            Client& operator=(const Client&) = delete;

        public:
            Client(nxclient_t client, const NxClient_JoinSettings* settings)
                : _client(client)
                , _settings(*settings)
            {
                TRACE_L1("Created client named: %s", _settings.name);
            }

            static Client* Create(nxclient_t client, const NxClient_JoinSettings* settings)
            {
                Client* result = Core::Service<Client>::Create<Client>(client, settings);

                return (result);
            }
            virtual ~Client()
            {
                TRACE_L1("Destructing client named: %s", _settings.name);
            }

        public:
            inline bool IsActive() const
            {
                return (_client != nullptr);
            }
            inline nxclient_t Handle() const
            {
                return _client;
            }
            inline const char* Id() const
            {
                return (_settings.name);
            }

            virtual string Name() const override;
            virtual void Kill() override;
            virtual void Opacity(const uint32_t value) override;

            BEGIN_INTERFACE_MAP(Entry)
            INTERFACE_ENTRY(Exchange::IComposition::IClient)
            END_INTERFACE_MAP

        private:
            // note: following methods are for callback, do not call on the interface to influence the Client, see Compostion interface to do this
            virtual void ChangedGeometry(const Exchange::IComposition::Rectangle& rectangle) override;
            virtual void ChangedZOrder(const uint8_t zorder) override;

        private:
            nxclient_t _client;
            NxClient_JoinSettings _settings;
        };

        enum server_state {
            FAILURE = 0x00,
            UNITIALIZED = 0x01,
            INITIALIZING = 0x02,
            OPERATIONAL = 0x03,
            DEINITIALIZING = 0x04,
        };

        struct IClient {
            virtual ~IClient() {}

            virtual void Attached(Exchange::IComposition::IClient*) = 0;

            virtual void Detached(const char name[]) = 0;
        };

        struct IStateChange {

            virtual ~IStateChange() {}

            // Signal changes on the subscribed namespace..
            virtual void StateChange(server_state state) = 0;
        };

    public:
        Platform(IStateChange* stateChanges, IClient* clientChanges, const string& configuration, const NEXUS_VideoFormat& format);
        virtual ~Platform();

    public:
        inline server_state State() const
        {
            return _state;
        }

    private:
        void Add(nxclient_t client, const NxClient_JoinSettings* joinSettings);
        void Remove(const char clientName[]);
        void StateChange(server_state state);
        static void CloseDown();
        template <typename... T>
        static int ClientConnect(nxclient_t client, const NxClient_JoinSettings* pJoinSettings, NEXUS_ClientSettings* pClientSettings, T... Targs);
        static void ClientDisconnect(nxclient_t client, const NxClient_JoinSettings* pJoinSettings);

    private:
        BKNI_MutexHandle _lock;
        nxserver_t _instance;
        nxserver_settings _serverSettings;
        NEXUS_PlatformSettings _platformSettings;
        NEXUS_PlatformCapabilities _platformCapabilities;
        server_state _state;
        IClient* _clientHandler;
        IStateChange* _stateHandler;
        bool _joined;
        static Platform* _implementation;
    };
}
}
#endif //PLATFORMIMPLEMENTATION_H
