import paho.mqtt.client as mqtt

print("Conectando ao broker MQTT...")
client = mqtt.Client()
client.connect("broker.hivemq.com", 1883, 60)
print("Conectado!")

print("Enviando comando 'play'...")
client.publish("totem/printpixel/trigger", "play")
print("✓ Comando enviado para totem/printpixel/trigger")

client.disconnect()
print("Desconectado. Verifique os logs do ESP32!")
