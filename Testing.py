# 1. Install necessary libraries (if you haven't already on your local machine)


import cv2
from ultralytics import YOLO
import numpy as np # Still useful for general image handling if needed

# 2. Load your TRAINED YOLOv8 model
# --- IMPORTANT: Replace with the ACTUAL path to your downloaded best.pt file ---
model_path =r'C:\Users\mukun\OneDrive\Desktop\best.pt'
try:
    model = YOLO(model_path)
    print(f"Successfully loaded model from {model_path}")
    # You can optionally set device here if needed, e.g., model = YOLO(model_path).to('cuda')
except Exception as e:
    print(f"Error loading model: {e}")
    print("Make sure the path is correct and you have the .pt file.")
    exit() # Stop if model loading fails

# 3. Initialize Webcam
cap = cv2.VideoCapture(0) # 0 is usually the default webcam

if not cap.isOpened():
    print("Error: Could not open webcam.")
    exit()

print("Webcam opened successfully. Press 'q' to quit.")

# 4. Processing Loop
while True:
    # Read frame from webcam
    ret, frame = cap.read()
    if not ret:
        print("Error: Failed to grab frame.")
        break

    # 5. Perform Inference using Ultralytics
    # model.predict automatically handles resizing (to imgsz used during training),
    # normalization, and returns results.
    # You can adjust confidence threshold here.
    results = model.predict(source=frame, conf=0.6, verbose=False) # Set verbose=False to reduce console output

    # 6. Process and Visualize Results
    # results is a list (usually one element for a single image)
    if results and len(results) > 0:
        result = results[0] # Get the results for the first (and only) image

        # Use the built-in plot() method to draw boxes, labels, and confidence scores
        annotated_frame = result.plot(conf=True) # conf=True shows confidence scores on boxes

        # If you need manual access to boxes (e.g., for custom logic):
        # num_detections = len(result.boxes)
        # print(f"Detected {num_detections} objects.")
        # for box in result.boxes:
        #     xyxy = box.xyxy[0].cpu().numpy().astype(int) # Get coordinates [x1, y1, x2, y2]
        #     conf = box.conf[0].cpu().numpy()            # Get confidence score
        #     cls_id = int(box.cls[0].cpu().numpy())      # Get class ID
        #     label = model.names[cls_id]                 # Get class name
        #     print(f"  Box: {xyxy}, Conf: {conf:.2f}, Class: {label} ({cls_id})")
            # You could draw manually here if needed, but result.plot() is easier
            # cv2.rectangle(frame, (xyxy[0], xyxy[1]), (xyxy[2], xyxy[3]), (0, 255, 0), 2)
            # cv2.putText(frame, f"{label} {conf:.2f}", (xyxy[0], xyxy[1]-10), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0,255,0), 2)
    else:
        # If no results object is returned (shouldn't usually happen for predict)
        annotated_frame = frame # Show the original frame

    # 7. Display the frame
    cv2.imshow('YOLOv8 Webcam Detection', annotated_frame)

    # 8. Exit condition
    if cv2.waitKey(1) & 0xFF == ord('q'):
        print("Exiting...")
        break

# 9. Release resources
cap.release()
cv2.destroyAllWindows()
