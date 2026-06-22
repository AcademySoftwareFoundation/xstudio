#!/usr/bin/env python

import asyncio
import websockets
from websockets.asyncio.client import connect
import test_pb2
import sys

from helpers import log, create_authenticate_msg, create_available_sessions_msg, authenticate, get_available_sessions, authenticate_send
from helpers import authenticate_reply, wait_for_reply, proxy_send

shutdown = False

server_state = {
    "websocket" : None,
    "messages" : [],
    "client_type" : test_pb2.CT_UNSPECIFIED,
    "sessions": []
}

session_state = {
    "websocket" : None,
    "messages" : [],
    "client_type" : test_pb2.CT_UNSPECIFIED,
    "sessions": []
}


async def ainput(prompt: str) -> str:
     return await asyncio.to_thread(input, f'{prompt} ')

async def monitor_messages(host, port, state):

    log(f"launching messages {host} {port}")

    async for websocket in connect("ws://"+host+":"+str(port)):
        try:
            state["websocket"] = websocket
            state["client_type"] = test_pb2.CT_UNAUTHENTICATED

            async for message in websocket:
                tmsg = test_pb2.TheMsg()
                tmsg.ParseFromString(message)

                if tmsg.reference:
                    state["messages"].append(tmsg)
                else:
                    # not a reply message
                    # process here..
                    if tmsg.msg_type == test_pb2.TheMsg().MT_ERROR_MSG:
                        msg = test_pb2.ErrorMsg()
                        tmsg.body.Unpack(msg)
                        log(f"ERROR Received: {msg.text}")
                    elif tmsg.msg_type == test_pb2.TheMsg().MT_SESSION_JOINED_MSG:
                        msg = test_pb2.SessionJoinedMsg()
                        tmsg.body.Unpack(msg)
                        state["client_type"] = msg.client_type
                        log(f"Joined session: {state['client_type']}")
                    elif tmsg.msg_type == test_pb2.TheMsg().MT_SESSION_LEFT_MSG:
                        msg = test_pb2.SessionLeftMsg()
                        tmsg.body.Unpack(msg)
                        state["client_type"] = msg.client_type
                        log(f"Left session: {state['client_type']}")
                    elif tmsg.msg_type == test_pb2.TheMsg().MT_CHAT_MSG:
                        msg = test_pb2.MessageMsg()
                        tmsg.body.Unpack(msg)
                        log(f"Message: {msg.text}")
                    else:
                        log(f"Unexpected response: {tmsg.msg_type}")



        except websockets.exceptions.ConnectionClosed as err:
            log(f"dead {str(err)}")
            # except
            if not shutdown:
                continue

        break

    log(f"Connection closed, {websocket.close_reason}")
    state["client_type"] = test_pb2.CT_UNSPECIFIED
    state["websocket"] = None


async def client_main(client_name):
    log("launching client_main")
    global shutdown
    global server_state
    global session_state

    session_token = None
    server_token = None

    session_task = None

    while True:
        try:
            line = await ainput("command: ")
            line = line.strip()
        except KeyboardInterrupt:
            break

        if not line:
            break

        if line == "a" and session_state["websocket"]:
            try:
                reply = await authenticate_reply(
                    await wait_for_reply(session_state["messages"],
                        await authenticate_send(session_state["websocket"], client_name, session_token, "LET ME IN")
                    )
                )

                if isinstance(reply, test_pb2.AuthenticatedMsg):
                    session_token = reply.client_token
                    session_state["client_type"] = reply.client_type
            except websockets.exceptions.ConnectionClosed as err:
                log(f"dead {str(err)}")


        if line == "l" and session_state["websocket"]:
            reply = await get_available_sessions(session_state["websocket"], session_state["messages"], ["mykey"])
            session_state["sessions"] = reply
            for i in reply:
                log(i)

        if line == "A" and server_state["websocket"]:
            reply = await authenticate_reply(
                await wait_for_reply(server_state["messages"],
                    await authenticate_send(server_state["websocket"], client_name, server_token, "LET ME IN")
                )
            )

            if isinstance(reply, test_pb2.AuthenticatedMsg):
                server_token = reply.client_token
                server_state["client_type"] = reply.client_type

        if line == "L" and server_state["websocket"]:
            reply = await get_available_sessions(server_state["websocket"], server_state["messages"], ["mykey"])
            server_state["sessions"] = reply
            for i in reply:
                log(f"{i.display_name}, {i.active}, {i.session_key}, {i.host}, {i.port}")

        if line[0] == "c" and server_state["websocket"]:
            _,key = line.split()
            for i in server_state["sessions"]:
                if i.session_key == key:
                    session_task = asyncio.create_task(monitor_messages(i.host, i.port, session_state))
                    break

        if line[0] == "C" and server_state["websocket"]:
            _,key = line.split()
            await proxy_send(server_state["websocket"], key)

    shutdown = True

    if session_task:
        if session_state["websocket"]:
            await session_state["websocket"].close()
        await session_task

    await server_state["websocket"].close()

    log("exit client_main")

async def main(client_name):
    log("enter main")

    await asyncio.gather(
        client_main(client_name),
        # monitor_messages("localhost", 8766, session_state),
        monitor_messages("localhost", 8765, server_state)
    )

    log("exit main")

if __name__ == "__main__":
    args = sys.argv[1:]
    asyncio.run(main(args[0]))

