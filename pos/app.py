import os
import random
import base64
import io
import paho.mqtt.client as mqtt
from flask import Flask, render_template, request, jsonify
from PIL import Image
import numpy as np
import sys

# Ensure core modules can be imported
sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__), '..')))
sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__), '..', 'core')))
from core.shredder import CipherShredder
from core.orchestrator import CipherOrchestrator

app = Flask(__name__)
app.secret_key = "pos_secret_key_8828"

BROKER = os.environ.get("MQTT_BROKER", "broker.hivemq.com")
PORT = 1883
TOPIC = "phantasm/iot/display"

shredder = CipherShredder(output_dir="pos_outputs")
orch = CipherOrchestrator()

# Store active transactions: {tx_id: {amount, otp, share_b_base64, status}}
active_txs = {}


def matrix_to_base64(matrix):
    img = Image.fromarray((matrix * 255).astype(np.uint8), mode='L')
    buffered = io.BytesIO()
    img.save(buffered, format="PNG")
    return base64.b64encode(buffered.getvalue()).decode('utf-8')


def mqtt_publish(payload, qos=1):
    client = mqtt.Client()
    try:
        client.connect(BROKER, PORT, 60)
        client.publish(TOPIC, payload, qos=qos)
        client.disconnect()
    except Exception as e:
        print(f"[MQTT Error] {e}")


@app.route('/')
def index():
    return "CipherSight POS System. Use /merchant or /customer."


@app.route('/merchant')
def merchant_dashboard():
    return render_template('merchant.html')


@app.route('/customer')
def customer_portal():
    return render_template('customer.html')


@app.route('/api/create-transaction', methods=['POST'])
def create_transaction():
    data = request.json
    amount = data.get('amount')
    tx_id = f"TX{random.randint(1000, 9999)}"

    # 1. Generate Transaction Secret (encode payment details into a QR)
    secret_data = f"PAY:ID={tx_id};AMT={amount}"
    share_a, share_b = shredder.shred(secret_data)

    # 2. Generate Hardware Challenge OTP
    otp = str(random.randint(1000, 9999))

    # 3. Store Transaction State server-side (shares live here securely)
    active_txs[tx_id] = {
        "amount": amount,
        "otp": otp,
        "share_a": share_a,
        "share_b": share_b,
        "status": "pending"
    }

    # 4. Publish Share A bitmap to Merchant OLED via MQTT
    payload = orch.prepare_payload(share_a)
    mqtt_publish(payload)

    print(f"[POS] Transaction {tx_id} created — Amount: ${amount} — OTP: {otp}")

    # Return only tx_id (not the share_b — customer fetches it separately)
    return jsonify({
        "success": True,
        "tx_id": tx_id,
        "amount": amount
    })


@app.route('/api/transaction/<tx_id>', methods=['GET'])
def get_transaction(tx_id):
    """Customer fetches tx details using only the tx_id."""
    if tx_id not in active_txs:
        return jsonify({"success": False, "message": "Transaction not found"}), 404
    tx = active_txs[tx_id]
    return jsonify({
        "success": True,
        "tx_id": tx_id,
        "amount": tx["amount"],
        "status": tx["status"]
    })


@app.route('/api/display-pin/<tx_id>', methods=['POST'])
def display_pin(tx_id):
    """Merchant triggers the 4-digit PIN to be sent to OLED."""
    if tx_id not in active_txs:
        return jsonify({"success": False, "message": "Transaction not found"}), 404
    otp = active_txs[tx_id]["otp"]
    mqtt_publish(otp)
    print(f"[POS] PIN for {tx_id} sent to OLED: {otp}")
    return jsonify({"success": True, "message": "PIN sent to hardware"})


@app.route('/api/verify-payment', methods=['POST'])
def verify_payment():
    data = request.json
    tx_id = data.get('tx_id')
    user_otp = data.get('otp')

    if tx_id not in active_txs:
        return jsonify({"success": False, "message": "Transaction not found"}), 404

    if active_txs[tx_id]["status"] == "completed":
        return jsonify({"success": False, "message": "Transaction already completed"}), 400

    if active_txs[tx_id]["otp"] == user_otp:
        active_txs[tx_id]["status"] = "completed"
        
        # Perform Server-Side Reconstruction
        share_a = active_txs[tx_id]["share_a"]
        share_b = active_txs[tx_id]["share_b"]
        reconstructed = shredder.reconstruct(share_a, share_b)
        reconstructed_b64 = matrix_to_base64(reconstructed)
        
        print(f"[POS] Transaction {tx_id} COMPLETED ✅")
        return jsonify({
            "success": True, 
            "message": "Payment Successful!", 
            "amount": active_txs[tx_id]["amount"],
            "reconstructed_qr": reconstructed_b64
        })
    else:
        print(f"[POS] Wrong OTP attempt on {tx_id}: {user_otp}")
        return jsonify({"success": False, "message": "Invalid challenge code. Check the OLED screen."}), 401


if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000, debug=False)
