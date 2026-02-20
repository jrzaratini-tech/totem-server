const express = require("express");
const mqtt = require("mqtt");

const app = express();

// 🔹 CONFIGURE AQUI
const DEVICE_ID = "123";
const INSTAGRAM_URL = "https://instagram.com/seu_post_aqui";

// 🔹 Conectar no broker
const client = mqtt.connect("mqtt://broker.hivemq.com:1883");

client.on("connect", () => {
  console.log("Conectado ao MQTT");
});

app.get("/totem/:id", (req, res) => {
  const id = req.params.id;

  const topic = `totem/${id}`;

  client.publish(topic, "play");

  console.log("Publicado play em:", topic);

  res.redirect(INSTAGRAM_URL);
});

const PORT = process.env.PORT || 3000;

app.listen(PORT, () => {
  console.log("Servidor rodando na porta", PORT);
});