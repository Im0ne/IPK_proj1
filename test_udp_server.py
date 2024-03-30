import socket
import struct

# Message types
CONFIRM = 0x00
REPLY = 0x01
AUTH = 0x02
JOIN = 0x03
MSG = 0x04
ERR = 0xFE
BYE = 0xFF

# Create a UDP socket
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

# Bind the socket to a port
server_address = ('localhost', 4567)
sock.bind(server_address)

while True:
    data, address = sock.recvfrom(4096)

    print(data)

    if data:
        # Parse the message type and ID from the received data
        message_type = data[0]
        message_id = struct.unpack('>H', data[1:3])[0]

        print('Message Type:', message_type)
        print('Message ID:', message_id)

        if message_type in [AUTH, JOIN]:
            # If the message is an AUTH or JOIN message, send a CONFIRM and REPLY
            confirm_message = struct.pack('>BH', CONFIRM, message_id)
            print('Sending CONFIRM:', confirm_message)
            sent = sock.sendto(confirm_message, address)

            # Construct the REPLY message
            result = 1  
            message_contents = 'Success'  
            reply_message = struct.pack('>BHB', REPLY, message_id, result) + message_id.to_bytes(2, 'big') + message_contents.encode() + b'\0'
            print('Sending REPLY:', reply_message)
            sent = sock.sendto(reply_message, address)
            message_id += 1

            # Construct the MSG message
            display_name = 'Server'
            message_contents = 'Hello, World!'
            msg_message = struct.pack('>BH', MSG, message_id) + display_name.encode() + b'\0' + message_contents.encode() + b'\0'
            print('Sending MSG:', msg_message)
            sent = sock.sendto(msg_message, address)
            message_id += 1
            
        elif message_type != CONFIRM:
            # For all other message types (except CONFIRM), send a CONFIRM
            confirm_message = struct.pack('>BH', CONFIRM, message_id)
            print('Sending CONFIRM:', confirm_message)
            sent = sock.sendto(confirm_message, address)
            message_id += 1