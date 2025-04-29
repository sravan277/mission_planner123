# Mango Ripeness Classifier

This Keras model uses MobileNetV2 + custom head to classify mango ripeness.

## Usage
```python
from tensorflow.keras.models import load_model
model = load_model('best_mango_classifier.keras')
preds = model.predict(your_image_batch)  # one-hot ripeness vector
```

## Details
- Backbone: MobileNetV2 (ImageNet pre-trained)
- Input size: 224×224
- Classes (3): OverRipe, Ripe, UnRipe
- Trained epochs: 20
