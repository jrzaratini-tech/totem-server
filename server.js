const express = require("express");
const mqtt = require("mqtt");

const app = express();

// 🔹 Conecta no broker MQTT (mesmo que o ESP usa)
const client = mqtt.connect("mqtt://broker.hivemq.com:1883");

client.on("connect", () => {
  console.log("Conectado ao MQTT");
});

client.on("error", (err) => {
  console.error("Erro MQTT:", err);
});

// 🔹 Lista de totens (ID → Instagram fixo)
const clientes = {
  "123": "https://www.instagram.com/printpixel_grafica/"
};

app.get("/totem/:id", (req, res) => {
  const id = req.params.id;

  if (!clientes[id]) {
    return res.status(404).send("Totem não encontrado");
  }

  const topic = `totem/${id}`;

  console.log("Publicando play em:", topic);

  client.publish(topic, "play");

  res.redirect(clientes[id]);
});

const PORT = process.env.PORT || 3000;

app.listen(PORT, () => {
  console.log("Servidor rodando na porta", PORT);
});