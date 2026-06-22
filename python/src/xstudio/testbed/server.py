#!/usr/bin/env python

import test_pb2
import asyncio
import uuid
from helpers import log, AuthenticatedClient, Session
from websockets.asyncio.server import serve
from websockets.asyncio.client import connect


authenticated_clients = {}

authentication_methods = [test_pb2.AT_SCRIPT_KEY]

# use sets for speed..
# but not for demo.
authentication_script_keys = {
    "LET ME IN": test_pb2.CT_BROWSER,
    "IM A SESSION": test_pb2.CT_SESSION,
}

available_sessions = [
    Session("Hidden", discovery_keys=""),
    Session("Open", discovery_keys=[]),
    Session("Restricted", discovery_keys=["mykey"])
]

async def authenticate(websocket):
    authenticated = False
    client_type = test_pb2.CT_UNAUTHENTICATED
    client_token = None
    async for message in websocket:
        tmsg = test_pb2.TheMsg()
        tmsg.ParseFromString(message)

        if tmsg.msg_type == test_pb2.TheMsg().MT_REQUEST_AUTHENTICATION_MSG:
            ramsg = test_pb2.RequestAuthenticationMsg()
            tmsg.body.Unpack(ramsg)

            # already authenticated. (probably reconnecting)
            if ramsg.client_token in authenticated_clients:
                client_type = authenticated_clients[ramsg.client_token].client_type
                client_token = ramsg.client_token

                amsg = test_pb2.AuthenticatedMsg()
                amsg.client_token = ramsg.client_token
                amsg.client_type = client_type
                tmsg.msg_type = test_pb2.TheMsg().MT_AUTHENTICATED_MSG
                tmsg.body.Pack(amsg)
                await websocket.send(tmsg.SerializeToString(), text=False)

                # update display_name
                authenticated_clients[ramsg.client_token].display_name = ramsg.display_name
                authenticated_clients[ramsg.client_token].connection = websocket

                log(f"Client reconnected..: {ramsg.display_name}")
            # no authentication required.
            elif test_pb2.AT_SCRIPT_KEY in authentication_methods and ramsg.script_key:
                if ramsg.script_key in authentication_script_keys:
                    client_type = authentication_script_keys[ramsg.script_key]
                    client_token = str(uuid.uuid4())

                    amsg = test_pb2.AuthenticatedMsg()
                    amsg.client_token = client_token
                    amsg.client_type = client_type

                    tmsg.msg_type = test_pb2.TheMsg().MT_AUTHENTICATED_MSG
                    tmsg.body.Pack(amsg)
                    authenticated_clients[amsg.client_token] = AuthenticatedClient(amsg.client_token, ramsg.display_name, websocket, client_type)
                    await websocket.send(tmsg.SerializeToString(), text=False)
                    log(f"Client connected..: {ramsg.display_name}")
                else:
                    log(f"Invalid script key..: {ramsg.script_key}")
                    errmsg = test_pb2.ErrorMsg()
                    errmsg.code = 1
                    errmsg.text = f"Invalid script key {ramsg.script_key}"
                    tmsg.msg_type = test_pb2.TheMsg().MT_ERROR_MSG
                    tmsg.body.Pack(errmsg)
                    await websocket.send(tmsg.SerializeToString(), text=False)

            elif test_pb2.AT_NO_AUTHENTICATION in authentication_methods:
                client_type = test_pb2.CT_BROWSER
                client_token = str(uuid.uuid4())

                amsg = test_pb2.AuthenticatedMsg()
                amsg.client_token = client_token
                amsg.client_type = client_type

                tmsg.msg_type = test_pb2.TheMsg().MT_AUTHENTICATED_MSG
                tmsg.body.Pack(amsg)
                authenticated_clients[amsg.client_token] = AuthenticatedClient(amsg.client_token, ramsg.display_name, websocket, client_type)
                await websocket.send(tmsg.SerializeToString(), text=False)
                log(f"Client connected..: {ramsg.display_name}")

            else:
                # send authentication requirement detail..
                amsg = test_pb2.AuthenticationMethodsMsg()
                for i in authentication_methods:
                    amsg.supported_methods.append(i)
                tmsg.msg_type = test_pb2.TheMsg().MT_AUTHENTICATION_METHODS_MSG
                tmsg.body.Pack(amsg)
                await websocket.send(tmsg.SerializeToString(), text=False)
                log(f"Send connection details to ..: {ramsg.display_name}")
        else:
            log(f"Must authenticate..: {tmsg.msg_type}")
            errmsg = test_pb2.ErrorMsg()
            errmsg.code = 1
            errmsg.text = "You must first authenticate."
            tmsg.msg_type = test_pb2.TheMsg().MT_ERROR_MSG
            tmsg.body.Pack(errmsg)
            await websocket.send(tmsg.SerializeToString(), text=False)

        if client_type != test_pb2.CT_UNAUTHENTICATED:
            break
        else:
            # stop HAMMERING from bad clients.., more mitigations might be wanted here..
            await asyncio.sleep(1)

    if client_type != test_pb2.CT_UNAUTHENTICATED:
        if client_type == test_pb2.CT_SESSION:
            await authenticated_session(websocket, client_token)
        elif client_type == test_pb2.CT_BROWSER:
            await authenticated_browser(websocket, client_token)

async def authenticated_browser(websocket, client_token):

    async for message in websocket:
        tmsg = test_pb2.TheMsg()
        tmsg.ParseFromString(message)

        if tmsg.msg_type == test_pb2.TheMsg().MT_REQUEST_AVAILABLE_SESSIONS_MSG:
            msg = test_pb2.RequestAvailableSessionsMsg()
            tmsg.body.Unpack(msg)

            tmsg.msg_type = test_pb2.TheMsg().MT_AVAILABLE_SESSIONS_MSG
            reply = test_pb2.AvailableSessionsMsg()

            for i in available_sessions:
                if i.discovery_keys is None:
                    continue
                elif not i.discovery_keys or (set(i.discovery_keys) & set(msg.discovery_keys)):
                    log(i.display_name)
                    session = test_pb2.SessionMsg()
                    session.display_name = i.display_name
                    session.session_key = i.session_key
                    session.active = i.active
                    if i.host is not None:
                        session.host = i.host
                    if i.port != 0:
                        session.port = i.port
                    reply.sessions.append(session)

            tmsg.body.Pack(reply)
            await websocket.send(tmsg.SerializeToString(), text=False)
        elif tmsg.msg_type == test_pb2.TheMsg().MT_PROXY_TO_SESSION_MSG:
            msg = test_pb2.ProxyMsg()
            tmsg.body.Unpack(msg)
            for i in available_sessions:
                if msg.session_key == i.session_key:
                    if i.host and i.port:
                        await proxy_client_to_session(websocket, i.host, i.port)
                    break
        else:
            log(f"Unexpected request: {tmsg.msg_type}")
            # give reply or they'll wait forever..
            if tmsg.reference:
                tmsg.msg_type = test_pb2.TheMsg().MT_ERROR_MSG
                reply = test_pb2.ErrorMsg()
                reply.code = 1
                reply.text = "Unexpected request"
                tmsg.body.Pack(reply)
                await websocket.send(tmsg.SerializeToString(), text=False)


async def proxy_client_to_session(websocket, session_host, session_port):
    # ack they can't ban me... WATCH OUT FOR THIS!!!!
    # create connection to session..
    async for rwebsocket in connect("ws://"+session_host+":"+str(session_port)):
        # we now need to bidrectionally pass mesages...
        # spawn prcoess to read from rwebsocket and send to message
        await asyncio.gather(
            send_from_to(websocket, rwebsocket),
            send_from_to(rwebsocket, websocket),
        )


async def send_from_to(source , destination):
    async for message in source:
        await destination.send(message, text=False)

async def authenticated_session(websocket, client_token):

    async for message in websocket:
        tmsg = test_pb2.TheMsg()
        tmsg.ParseFromString(message)

        if tmsg.msg_type == test_pb2.TheMsg().MT_REGISTER_SESSION_MSG:
            msg = test_pb2.RegisterSessionMsg()
            tmsg.body.Unpack(msg)

            discovery_keys = None
            if not msg.hidden:
                discovery_keys = []
                for i in msg.discovery_keys:
                    discovery_keys.append(i)

            # add session..
            new_session = Session(
                msg.detail.display_name,
                active=msg.detail.active,
                discovery_keys=discovery_keys,
                host=msg.detail.host,
                port=msg.detail.port,
                scheduled=msg.detail.scheduled
            )

            available_sessions.append(
                new_session
            )

            log(f"New session registered '{new_session.display_name}'")
            # send reply with session_key and owner_key
            reply = test_pb2.SessionKeyMsg()
            reply.owner_key = new_session.owner_key
            reply.session_key = new_session.session_key

            tmsg.msg_type = test_pb2.TheMsg().MT_SESSION_REGISTERED_MSG
            tmsg.body.Pack(reply)
            await websocket.send(tmsg.SerializeToString(), text=False)

        elif tmsg.msg_type == test_pb2.TheMsg().MT_UNREGISTER_SESSION_MSG:
            msg = test_pb2.SessionKeyMsg()
            tmsg.body.Unpack(msg)

            for index, item in enumerate(available_sessions):
                if item.owner_key == msg.owner_key and item.session_key == msg.session_key:
                    log(f"Session removed '{item.display_name}'")
                    available_sessions.pop(index)
                    break

        elif tmsg.msg_type == test_pb2.TheMsg().MT_UPDATE_SESSION_MSG:
            msg = test_pb2.UpdateSessionMsg()
            tmsg.body.Unpack(msg)

            for index, item in enumerate(available_sessions):
                if item.owner_key == msg.key.owner_key and item.session_key == msg.key.session_key:
                    log(f"Session Updating '{item.display_name}'")
                    item.display_name = msg.detail.detail.display_name
                    item.host = msg.detail.detail.host
                    item.port = msg.detail.detail.port
                    item.active = msg.detail.detail.active
                    item.scheduled = msg.detail.detail.scheduled

                    discovery_keys = None
                    if not msg.detail.hidden:
                        discovery_keys = []
                        for i in msg.detail.discovery_keys:
                            discovery_keys.append(i)

                    item.discovery_keys = discovery_keys

                    log(f"Session Updated '{item.display_name}'")
                    break

            # should we respond ?
        else:
            log(f"Unexpected request: {tmsg.msg_type}")
            if tmsg.reference:
                tmsg.msg_type = test_pb2.TheMsg().MT_ERROR_MSG
                reply = test_pb2.ErrorMsg()
                reply.code = 1
                reply.text = "Unexpected request"
                tmsg.body.Pack(reply)
                await websocket.send(tmsg.SerializeToString(), text=False)


async def main():
    log("launching")
    async with serve(authenticate, "localhost", 8765) as server:
        await server.serve_forever()

asyncio.run(main())