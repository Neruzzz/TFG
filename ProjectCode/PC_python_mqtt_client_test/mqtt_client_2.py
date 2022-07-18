import paho.mqtt.client as mqtt
import time
import ssl

client = mqtt.Client(client_id="PC2")

client.tls_set(ca_certs="ca.crt", certfile="uoc-1.crt", keyfile="uoc-1.key", cert_reqs=ssl.CERT_REQUIRED,
        tls_version=ssl.PROTOCOL_TLS, ciphers=None)
ret = client.connect("a2yx4i4f6mu6nh-ats.iot.eu-central-1.amazonaws.com", port=443)
print(ret)
client.loop_start()

client.subscribe("dev/uoc-1/notif/notifData",qos=1)
i = 0
while True:
    time.sleep(2)
    client.publish("dev/uoc-1/notif/notifData", "prueba", qos=1, retain=False)
    i+=1