from flask import Flask, render_template, request, jsonify
import paho.mqtt.client as mqtt
import csv
import os
from datetime import datetime

app = Flask(__name__)

# MQTT broker details
mqtt_broker = "broker.hivemq.com"
mqtt_port = 1883

# Global variables
diversity_count = 0
selected_diversity = None  # Diversity is selected after Tool Change
overall_count = 0  # Track count within a cycle

# CSV file to store logs
csv_file = "log_data.csv"

# Create CSV file if it doesn't exist
# Create CSV file if it doesn't exist
if not os.path.exists(csv_file):
    with open(csv_file, mode="w", newline="") as file:
        writer = csv.writer(file)
        writer.writerow(["Timestamp", "Diversity No", "Diversity Count"])  # Headers

# MQTT client setup
mqtt_client = mqtt.Client()
mqtt_client.connect(mqtt_broker, mqtt_port, 60)

# MQTT message handling
def on_message(client, userdata, message):
    global diversity_count
    topic = message.topic
    payload = message.payload.decode("utf-8")

    if topic == "diversity/count":
        diversity_count = int(payload)

# Set up MQTT client
mqtt_client.on_message = on_message
mqtt_client.subscribe("diversity/count")
mqtt_client.loop_start()

def log_tool_change():
    """Logs final count before reset to CSV."""
    global diversity_count, selected_diversity

    if selected_diversity is not None:  # Ensure a valid diversity selection
        with open(csv_file, mode="a", newline="") as file:
            writer = csv.writer(file)
            writer.writerow([datetime.now().strftime("%Y-%m-%d %H:%M:%S"), selected_diversity, diversity_count])

@app.route("/", methods=["GET", "POST"])
def index():
    global selected_diversity

    if request.method == "POST":
        if "tool_change" in request.form:
            log_tool_change()  # Log previous count
            selected_diversity = None  # Reset selection
            mqtt_client.publish("tool/change", "reset")
            print("Tool Change - Count Reset")

        if "diversity_select" in request.form:
            selected_diversity = int(request.form["diversity_select"])
            mqtt_client.publish("diversity/select", str(selected_diversity))
            print(f"Diversity Selected: {selected_diversity}")

    # Read log data to display
    log_entries = []
    with open(csv_file, mode="r") as file:
        reader = csv.reader(file)
        try:
            header = next(reader)  # Skip header
            log_entries = list(reader)  # Read log entries
        except StopIteration:
            pass  # File is empty except for the header

    return render_template(
        "index.html",
        diversity_count=diversity_count,
        selected_diversity=selected_diversity,
        log_entries=log_entries,
    )

@app.route("/get_diversity_count")
def get_diversity_count():
    """API to get diversity count."""
    return jsonify({"diversity_count": diversity_count})
if __name__ == "__main__":
    app.run(debug=True)
