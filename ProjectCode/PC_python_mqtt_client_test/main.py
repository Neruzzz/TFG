import paho.mqtt.client as mqtt
import ssl
import time
import logging
import json


# logging.basicConfig(level=logging.DEBUG)

# The callback for when the client receives a CONNACK response from the server.
def on_connect(client, userdata, flags, rc):
    print("Connected with result code " + str(rc))

    # Subscribing in on_connect() means that if we lose the connection and
    # reconnect then subscriptions will be renewed.
    client.subscribe("dev/uoc-1/notif/solParsed", qos=1)


def on_publish(client, userdata, mid):
    print("message published!")
    pass


# The callback for when a PUBLISH message is received from the server.
def on_message(client, userdata, msg):
    print(msg.topic + " " + str(msg.payload))
    # client.publish("dev/uoc-1/notif/notifData", "1", qos=1, retain=False)
    pass


def on_subscribe(client, userdata, mid, granted_qos):
    root = dict()
    root["name"] = "notifData"
    root["fields"] = dict()
    root["fields"]["macAddress"] = "00-17-0d-00-00-33-f8-c0"
    root["fields"]["data"] = [40,4,80,192,190,3,0,24,160,63,14,252,0,0,64,1,80,189,190,3,0,71,137,63,14,252,0,1,64,1,80,192,190,3,0,24,160,63,14,252,0,2,64,1,80,26,191,3,0,46,78,66,14,252,0,3,64,1]
    root["gateway"] = "uoc-1"
    root["timestamp"] = 1614334800#int(time.time())
    json_str = json.dumps(root)
    print(json_str)
    a = str()
    for i in root["fields"]["data"]:
        a += format(i,'x')
    print(a)
    client.publish("dev/uoc-1/notif/notifData", json_str, qos=1, retain=False)


client = mqtt.Client(client_id="uoc-1")

client.on_connect = on_connect
client.on_message = on_message
client.on_publish = on_publish
client.on_subscribe = on_subscribe

# logger = logging.getLogger(__name__)
# client.enable_logger(logger)

client.tls_set(ca_certs="ca.crt", certfile="uoc-1.crt", keyfile="uoc-1.key", cert_reqs=ssl.CERT_REQUIRED,
               tls_version=ssl.PROTOCOL_TLSv1_2, ciphers=None)

# client.tls_insecure_set(True)

ret = client.connect("a2yx4i4f6mu6nh-ats.iot.eu-central-1.amazonaws.com", port=8883)
print(ret)

# Blocking call that processes network traffic, dispatches callbacks and
# handles reconnecting.
# Other loop*() functions are available that give a threaded interface and a
# manual interface.
client.loop_forever(retry_first_connection=True)
