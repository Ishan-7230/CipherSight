FROM python:3.10-slim

WORKDIR /app

RUN apt-get update && apt-get install -y \
    libgl1-mesa-glx \
    libglib2.0-0 \
    && rm -rf /var/lib/apt/lists/*

RUN pip install --no-cache-dir qrcode[pil] pillow numpy opencv-python paho-mqtt opencv-contrib-python

COPY . .
RUN mkdir -p /app/outputs

CMD ["python", "core/shredder.py"]
