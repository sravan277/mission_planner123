# YOLOv8 Mango Detection Model

This model was trained on a mango dataset to detect mangoes in images.

## Model Details
- Base model: YOLOv8n
- Epochs trained: 50
- Input size: 640x640
- Training date: run

## Usage
python
from ultralytics import YOLO

# Load the model
model = YOLO('best.pt')

# Perform inference
results = model('path/to/image.jpg')

# Display results
results[0].show()

