const express = require("express");
const mqtt = require("mqtt");

const app = express();

// 🔹 Conecta no broker MQTT (HiveMQ público para teste)
const client = mqtt.connect("mqtt://broker.hivemq.com:1883");

client.on("connect", () => {
  console.log("Conectado ao MQTT");
});

// 🔹 Lista de totens (ID → Instagram fixo)
const clientes = {
  "1001": "https://www.instagram.com/printpixel_grafica/"
};

app.get("/totem/:id", (req, res) => {
  const id = req.params.id;

  if (!clientes[id]) {
    return res.status(404).send("Totem não encontrado");
  }

  const topic = `totem/${id}`;

  console.log("Publicando em:", topic);

  client.publish(topic, "play");

  res.redirect(clientes[id]);
});

const PORT = process.env.PORT || 3000;

app.listen(PORT, () => {
  console.log("Servidor rodando na porta", PORT);
});