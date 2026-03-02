#!/usr/bin/env python

import test_pb2
import asyncio
import uuid
import sys
from helpers import log, AuthenticatedClient, Session, authenticate, create_register_session_msg,create_unregister_session_msg
from helpers import create_update_session_msg
from websockets.asyncio.server import serve
from websockets.asyncio.client import connect
from websockets.asyncio.server import broadcast

clients = {}
banned_clients = set()

browser_authentication_methods = [test_pb2.AT_SCRIPT_KEY]
client_authentication_methods = [test_pb2.AT_SCRIPT_KEY]

# use sets for speed..
# but not for demo.
# browser_authentication_script_keys = {
#     "LET ME IN": test_pb2.CT_OBSERVER
# }
browser_authentication_script_keys = [
    "LET ME IN"
]

current_session = None

shutdown = False
client_server = None
auto_admit = False

async def unauthenticate_client(websocket):
    client_type = test_pb2.CT_UNAUTHENTICATED
    client_token = None

    if websocket.remote_address[0] in banned_clients:
        await websocket.close(reason="Client Banned")
    else:
        async for message in websocket:
            tmsg = test_pb2.TheMsg()
            tmsg.ParseFromString(message)

            if tmsg.msg_type == test_pb2.TheMsg().MT_REQUEST_AUTHENTICATION_MSG:
                ramsg = test_pb2.RequestAuthenticationMsg()
                tmsg.body.Unpack(ramsg)

                # already authenticated. (probably reconnecting)
                if ramsg.client_token in clients:
                    client_type = test_pb2.CT_BROWSER
                    client_token = ramsg.client_token

                    amsg = test_pb2.AuthenticatedMsg()
                    amsg.client_token = client_token
                    amsg.client_type = client_type
                    tmsg.msg_type = test_pb2.TheMsg().MT_AUTHENTICATED_MSG
                    tmsg.body.Pack(amsg)
                    await websocket.send(tmsg.SerializeToString(), text=False)

                    # update display_name
                    clients[ramsg.client_token].display_name = ramsg.display_name
                    clients[ramsg.client_token].client_type = client_type
                    clients[ramsg.client_token].connection = websocket
                    log(f"Client reconnected..: {ramsg.display_name}")

                # no authentication required.
                elif test_pb2.AT_NO_AUTHENTICATION in browser_authentication_methods:
                    client_type = test_pb2.CT_BROWSER
                    client_token = str(uuid.uuid4())

                    amsg = test_pb2.AuthenticatedMsg()
                    amsg.client_token = client_token
                    amsg.client_type = client_type

                    tmsg.msg_type = test_pb2.TheMsg().MT_AUTHENTICATED_MSG
                    tmsg.body.Pack(amsg)
                    clients[amsg.client_token] = AuthenticatedClient(amsg.client_token, ramsg.display_name, websocket, amsg.client_type)
                    await websocket.send(tmsg.SerializeToString(), text=False)
                    log(f"Client connected..: {ramsg.display_name}")

                elif test_pb2.AT_SCRIPT_KEY in browser_authentication_methods and ramsg.script_key:
                    if ramsg.script_key in browser_authentication_script_keys:
                        client_type = test_pb2.CT_BROWSER
                        client_token = str(uuid.uuid4())

                        amsg = test_pb2.AuthenticatedMsg()
                        amsg.client_token = client_token
                        amsg.client_type = client_type

                        tmsg.msg_type = test_pb2.TheMsg().MT_AUTHENTICATED_MSG
                        tmsg.body.Pack(amsg)
                        clients[amsg.client_token] = AuthenticatedClient(amsg.client_token, ramsg.display_name, websocket, amsg.client_type)
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
            await authenticated_browser(websocket, client_token)
        elif client_token:
            clients[client_token].client_type = test_pb2.CT_UNSPECIFIED
            clients[client_token].connection = None

async def authenticated_browser(websocket, client_token):
    global current_session
    async for message in websocket:
        tmsg = test_pb2.TheMsg()
        tmsg.ParseFromString(message)

        if tmsg.msg_type == test_pb2.TheMsg().MT_REQUEST_AVAILABLE_SESSIONS_MSG:
            msg = test_pb2.RequestAvailableSessionsMsg()
            tmsg.body.Unpack(msg)

            tmsg.msg_type = test_pb2.TheMsg().MT_AVAILABLE_SESSIONS_MSG
            reply = test_pb2.AvailableSessionsMsg()

            if current_session is not None:
                log(current_session.display_name)
                session = test_pb2.SessionMsg()
                session.display_name = current_session.display_name
                session.session_key = current_session.session_key
                session.active = current_session.active
                reply.sessions.append(session)

            tmsg.body.Pack(reply)
            await websocket.send(tmsg.SerializeToString(), text=False)
        else:
            log(f"Unexpected request: {tmsg.msg_type}")
            if tmsg.reference:
                tmsg.msg_type = test_pb2.TheMsg().MT_ERROR_MSG
                reply = test_pb2.ErrorMsg()
                reply.code = 1
                reply.text = "Unexpected request"
                tmsg.body.Pack(reply)
                await websocket.send(tmsg.SerializeToString(), text=False)

    clients[client_token].client_type = test_pb2.CT_UNSPECIFIED
    clients[client_token].connection = None


async def admit_client(connection, client_type):
    tmsg = test_pb2.TheMsg()
    tmsg.msg_type = test_pb2.TheMsg().MT_SESSION_JOINED_MSG
    msg = test_pb2.SessionJoinedMsg()
    msg.client_type = client_type
    tmsg.body.Pack(msg)
    await connection.send(tmsg.SerializeToString(), text=False)

async def leave_client(connection, client_type):
    tmsg = test_pb2.TheMsg()
    tmsg.msg_type = test_pb2.TheMsg().MT_SESSION_LEFT_MSG
    msg = test_pb2.SessionLeftMsg()
    msg.client_type = client_type
    tmsg.body.Pack(msg)
    await connection.send(tmsg.SerializeToString(), text=False)


async def client_main(myport):
    # set this future to exit the server
    log("launching client server")
    global client_server
    client_server = asyncio.get_running_loop().create_future()

    async with serve(unauthenticate_client, "localhost", myport):
        try:
            await client_server
        except:
            pass

async def server_main(server_port):
    log("launching register to server")
    token = None
    client_type = None
    test_reconnect = False
    global current_session

    async for websocket in connect(f"ws://localhost:{server_port}"):
        log("hmm")
        try:
            reply = await authenticate(websocket, "The Session", token, "IM A SESSION")

            # connected, register our session..
            if isinstance(reply, test_pb2.AuthenticatedMsg):
                token = reply.client_token
                client_type = reply.client_type

                tmsg = create_register_session_msg(
                    display_name=current_session.display_name,
                    hidden=False,
                    active=False,
                    host=current_session.host,
                    port=current_session.port,
                    discovery_keys=current_session.discovery_keys
                )
                await websocket.send(tmsg.SerializeToString(), text=False)

                # get response..
                tmsg.ParseFromString(await websocket.recv())
                if tmsg.msg_type == test_pb2.TheMsg().MT_SESSION_REGISTERED_MSG:
                    msg = test_pb2.SessionKeyMsg()
                    tmsg.body.Unpack(msg)
                    current_session.session_key = msg.session_key
                    current_session.owner_key = msg.owner_key
                    log(f"Session Created {current_session.session_key} {current_session.owner_key}")
                else:
                    log(f"Unexpected reply: {tmsg.msg_type}")

                # await asyncio.sleep(5)

                # current_session.display_name = "Changed Name"
                # tmsg = create_update_session_msg(
                #     current_session.owner_key, current_session.session_key,
                #     display_name=current_session.display_name,
                #     hidden=False,
                #     active=current_session.active,
                #     host=current_session.host,
                #     port=current_session.port,
                #     discovery_keys=current_session.discovery_keys
                # )
                # await websocket.send(tmsg.SerializeToString(), text=False)

                # await asyncio.sleep(5)

                # tmsg = create_unregister_session_msg(current_session.owner_key, current_session.session_key)
                # await websocket.send(tmsg.SerializeToString(), text=False)
                # # current_session = None
                # log("remove session from server")
            break

            if test_reconnect:
                test_reconnect = False
                continue

        except websockets.exceptions.ConnectionClosed:
            continue

    log("exit server")

async def ainput(prompt: str) -> str:
     return await asyncio.to_thread(input, f'{prompt} ')

async def session_main():
    log("launching session")
    global shutdown
    global client_server
    global current_session
    global clients

    while True:
        try:
            line = await ainput("command: ")
            line = line.strip()
        except KeyboardInterrupt:
            break

        if not line:
            break

        if line == "l":
            for i in clients:
                print(clients[i])

        elif line == "b":
            # start session
            if not current_session.active:
                current_session.active = True
                # auto admit clients..
                admitted_clients = set()

                for i in clients:
                    if clients[i].connection and clients[i].client_type == test_pb2.CT_BROWSER:
                        clients[i].client_type = test_pb2.CT_OBSERVER
                        await admit_client(clients[i].connection, clients[i].client_type)
                        admitted_clients.add(clients[i].connection)

                tmsg = test_pb2.TheMsg()
                tmsg.msg_type = test_pb2.TheMsg().MT_CHAT_MSG
                msg = test_pb2.MessageMsg()
                msg.text = "1"
                tmsg.body.Pack(msg)
                broadcast(admitted_clients, tmsg.SerializeToString())

        elif line[0] == "p":
            spam_clients = set()
            for i in clients:
                if clients[i].connection:
                    spam_clients.add(clients[i].connection)

            tmsg = test_pb2.TheMsg()
            tmsg.msg_type = test_pb2.TheMsg().MT_CHAT_MSG
            msg = test_pb2.MessageMsg()
            msg.text = line[2:]
            tmsg.body.Pack(msg)
            broadcast(spam_clients, tmsg.SerializeToString())

        elif line == "e":
            # end session
            if current_session.active:
                current_session.active = False
                # auto leave clients..
                for i in clients:
                    if clients[i].client_type in [test_pb2.CT_OBSERVER, test_pb2.CT_CONTRIBUTOR]:
                        clients[i].client_type = test_pb2.CT_BROWSER
                        if clients[i].connection:
                            await leave_client(clients[i].connection, clients[i].client_type)

        elif line[0] == "B":
            _,key = line.split()
            if key in clients and clients[key].connection:
                banned_clients.add(clients[key].connection.remote_address[0])
                await clients[key].connection.close()
                del clients[key]

        elif line[0] == "k":
            _,key = line.split()
            if key in clients and clients[key].connection:
                await clients[key].connection.close()
                del clients[key]

        elif line[0] == "a" and current_session.active:
            _,key = line.split()
            if key in clients and clients[key].connection and clients[key].client_type == test_pb2.CT_BROWSER:
                 clients[key].client_type = test_pb2.CT_OBSERVER
                 await admit_client(clients[key].connection, clients[key].client_type)

    shutdown = True
    client_server.cancel()
    log("exit session")

async def main(my_port, server_port):
    global current_session

    log("enter main")

    current_session = Session(
        "My Session",
        owner_key=None, session_key=None, active=False, discovery_keys=[],
        host="localhost", port = my_port
    )

    await asyncio.gather(
        client_main(my_port),
        server_main(server_port),
        session_main()
    )

    log("exit main")

asyncio.run(main(my_port=8766, server_port=8765))
