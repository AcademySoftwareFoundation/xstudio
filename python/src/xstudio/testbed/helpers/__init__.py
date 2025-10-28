import os
import test_pb2
import __main__ as main
import asyncio
import uuid

reference = 0

def log(msg):
    print(f"{os.path.basename(main.__file__)}: {msg}")


class AuthenticatedClient():
    def __init__(self, token, display_name, connection, client_type=test_pb2.CT_BROWSER):
        self.token = token
        self.display_name = display_name
        self.client_type = client_type
        self.connection = connection

    def __str__(self):
        return f"display_name: {self.display_name} client_type: {self.client_type} token: {self.token} connected: {self.connection is not None}"

class Session():
    def __init__(self, display_name, owner_key=None, active=False, session_key=None, discovery_keys=None, host=None, port=0, scheduled=0):
        self.display_name = display_name

        if owner_key is None:
            self.owner_key = str(uuid.uuid4())
        else:
            self.owner_key = owner_key

        if session_key is None:
            self.session_key = str(uuid.uuid4())
        else:
            self.session_key = session_key

        self.discovery_keys = discovery_keys
        self.host = host
        self.port = port
        self.active = active
        self.scheduled = scheduled


def create_authenticate_msg(display_name, token=None, script_key=None):
    global reference
    reference += 1

    msg = test_pb2.TheMsg()
    msg.reference = reference
    msg.msg_type = test_pb2.TheMsg().MT_REQUEST_AUTHENTICATION_MSG
    amsg = test_pb2.RequestAuthenticationMsg()

    amsg.display_name = display_name
    if token is not None:
        amsg.client_token = token
    if script_key is not None:
        amsg.script_key = script_key

    msg.body.Pack(amsg)

    return msg

def create_available_sessions_msg(discovery_keys=None):
    global reference
    reference += 1

    msg = test_pb2.TheMsg()
    msg.reference = reference
    msg.msg_type = test_pb2.TheMsg().MT_REQUEST_AVAILABLE_SESSIONS_MSG
    amsg = test_pb2.RequestAvailableSessionsMsg()

    if discovery_keys is not None:
        for i in discovery_keys:
            amsg.discovery_keys.append(i)

    msg.body.Pack(amsg)

    return msg

def create_register_session_msg_base(display_name, hidden=False, active=False, host=None, port=None, discovery_keys=None):

    msg = test_pb2.RegisterSessionMsg()

    msg.detail.display_name = display_name
    msg.hidden = hidden
    msg.detail.active = active

    if host is not None:
        msg.detail.host = host
    if port is not None:
        msg.detail.port = port

    if discovery_keys is not None:
        for i in discovery_keys:
            msg.discovery_keys.append(i)

    return msg

def create_register_session_msg(display_name, hidden=False, active=False, host=None, port=None, discovery_keys=None):
    global reference
    reference += 1

    tmsg = test_pb2.TheMsg()
    tmsg.reference = reference
    tmsg.msg_type = test_pb2.TheMsg().MT_REGISTER_SESSION_MSG
    msg = create_register_session_msg_base(display_name, hidden, active, host, port, discovery_keys)
    tmsg.body.Pack(msg)

    return tmsg

def create_session_key_msg_base(owner_key, session_key):
    msg = test_pb2.SessionKeyMsg()

    msg.owner_key = owner_key
    msg.session_key = session_key

    return msg

def create_unregister_session_msg(owner_key, session_key):
    global reference
    reference += 1

    tmsg = test_pb2.TheMsg()
    tmsg.reference = reference
    tmsg.msg_type = test_pb2.TheMsg().MT_UNREGISTER_SESSION_MSG
    msg = create_session_key_msg_base(owner_key, session_key)

    tmsg.body.Pack(msg)

    return tmsg

def create_proxy_msg(session_key):
    global reference
    reference += 1

    tmsg = test_pb2.TheMsg()
    tmsg.reference = reference
    tmsg.msg_type = test_pb2.TheMsg().MT_PROXY_TO_SESSION_MSG
    msg = test_pb2.ProxyMsg()
    msg.session_key = session_key

    tmsg.body.Pack(msg)

    return tmsg

async def proxy_send(conn, session_key):
    msg = create_proxy_msg(session_key)
    await conn.send(msg.SerializeToString(), text=False)
    return msg.reference

def create_update_session_msg(owner_key, session_key, display_name, active=False, hidden=False, host=None, port=None, discovery_keys=None):
    global reference
    reference += 1

    tmsg = test_pb2.TheMsg()
    tmsg.reference = reference
    tmsg.msg_type = test_pb2.TheMsg().MT_UPDATE_SESSION_MSG
    msg = test_pb2.UpdateSessionMsg()

    msg.key.CopyFrom(create_session_key_msg_base(owner_key, session_key))
    msg.detail.CopyFrom(create_register_session_msg_base(display_name, hidden, active, host, port, discovery_keys))

    tmsg.body.Pack(msg)

    return tmsg

async def authenticate_send(conn, display_name, token, script_key=None):

    msg = create_authenticate_msg(display_name, token, script_key=script_key)
    await conn.send(msg.SerializeToString(), text=False)
    return msg.reference


async def wait_for_reply(messages, reply_id):
    reply = None
    done = False
    while not done:
        for ind, i in enumerate(messages):
            if i.reference == reply_id:
                reply = i
                messages.pop(ind)
                done = True
                break

        if not done:
            await asyncio.sleep(1)

    return reply

async def authenticate_reply(msg):
    if msg.msg_type == test_pb2.TheMsg().MT_AUTHENTICATED_MSG:
        reply = test_pb2.AuthenticatedMsg()
        msg.body.Unpack(reply)
        log(f"Authenticated: {reply.client_token} {reply.client_type}")
    elif msg.msg_type == test_pb2.TheMsg().MT_AUTHENTICATION_METHODS_MSG:
        reply = test_pb2.AuthenticationMethodsMsg()
        msg.body.Unpack(reply)
        for i in reply.supported_methods:
            log(f"Authentication method: {i}")
    elif msg.msg_type == test_pb2.TheMsg().MT_ERROR_MSG:
        reply = test_pb2.ErrorMsg()
        msg.body.Unpack(reply)
        log(f"ERROR Received: {reply.text}")
    else:
        log(f"Unexpected response: {msg.msg_type}")
        reply = msg.msg_type

    return reply

async def authenticate(conn, display_name, token, script_key=None):

    msg = create_authenticate_msg(display_name, token, script_key=script_key)
    await conn.send(msg.SerializeToString(), text=False)

    msg.ParseFromString(await conn.recv())

    if msg.msg_type == test_pb2.TheMsg().MT_AUTHENTICATED_MSG:
        reply = test_pb2.AuthenticatedMsg()
        msg.body.Unpack(reply)
        log(f"Authenticated: {reply.client_token} {reply.client_type}")
    elif msg.msg_type == test_pb2.TheMsg().MT_AUTHENTICATION_METHODS_MSG:
        reply = test_pb2.AuthenticationMethodsMsg()
        msg.body.Unpack(reply)
        for i in reply.supported_methods:
            log(f"Authentication method: {i}")
    elif msg.msg_type == test_pb2.TheMsg().MT_ERROR_MSG:
        reply = test_pb2.ErrorMsg()
        msg.body.Unpack(reply)
        log(f"ERROR Received: {reply.text}")
    else:
        log(f"Unexpected response: {msg.msg_type}")
        reply = msg.msg_type

    return reply


# async def get_available_sessions(conn, discovery_keys=None):
#     sessions = []

#     msg = create_available_sessions_msg(discovery_keys)
#     await conn.send(msg.SerializeToString(), text=False)

#     msg.ParseFromString(await conn.recv())

#     if msg.msg_type == test_pb2.TheMsg().MT_AVAILABLE_SESSIONS_MSG:
#         reply = test_pb2.AvailableSessionsMsg()
#         msg.body.Unpack(reply)
#         sessions = reply.sessions

#     elif msg.msg_type == test_pb2.TheMsg().MT_ERROR_MSG:
#         reply = test_pb2.ErrorMsg()
#         msg.body.Unpack(reply)
#         log(f"ERROR Received: {reply.text}")
#     else:
#         log(f"Unexpected response: {msg.msg_type}")

#     return sessions

async def get_available_sessions(conn, messages, discovery_keys=None):
    sessions = []

    msg = create_available_sessions_msg(discovery_keys)
    reply_id = msg.reference
    await conn.send(msg.SerializeToString(), text=False)
    msg = await wait_for_reply(messages, reply_id)

    if msg.msg_type == test_pb2.TheMsg().MT_AVAILABLE_SESSIONS_MSG:
        reply = test_pb2.AvailableSessionsMsg()
        msg.body.Unpack(reply)
        sessions = reply.sessions

    elif msg.msg_type == test_pb2.TheMsg().MT_ERROR_MSG:
        reply = test_pb2.ErrorMsg()
        msg.body.Unpack(reply)
        log(f"ERROR Received: {reply.text}")
    else:
        log(f"Unexpected response: {msg.msg_type}")

    return sessions
