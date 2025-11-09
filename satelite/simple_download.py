import os
import requests
from PIL import Image
from io import BytesIO
import time

# Where to save the images
output_dir = "sample_rgb"
num_images = 100

os.makedirs(output_dir, exist_ok=True)

# Earth View dataset viewer sample API
print(f"Attempting to download {num_images} sample RGB images to {output_dir}/")

# This is a simple method that tries to download sample images from Earth View viewer demos
# Instead of using the complex streaming dataset API
for i in range(num_images):
    try:
        # Add a slight delay to avoid overwhelming the server
        if i > 0 and i % 10 == 0:
            print(f"Downloaded {i} images so far, sleeping for 2 seconds...")
            time.sleep(2)
            
        # Try to get a sample image - using various index formats
        # These URLs are constructed based on common patterns for dataset viewers/demos
        # We're trying multiple possible formats
        possible_urls = [
            f"https://huggingface.co/spaces/satellogic/EarthView-Viewer/resolve/main/samples/sample_{i:04d}.png",
            f"https://huggingface.co/spaces/satellogic/EarthView-Viewer/resolve/main/samples/sample_{i}.png",
            f"https://huggingface.co/spaces/satellogic/EarthView-Viewer/resolve/main/samples/satellogic_{i:04d}.png",
            f"https://huggingface.co/datasets/satellogic/EarthView/resolve/main/samples/rgb_{i:04d}.png"
        ]
        
        for url in possible_urls:
            print(f"Attempting to download from {url}")
            response = requests.get(url, timeout=10)
            
            if response.status_code == 200:
                print(f"Success! Downloaded from {url}")
                img = Image.open(BytesIO(response.content))
                img.save(f"{output_dir}/image_{i:04d}.png")
                break
        else:
            print(f"Failed to download image {i} - tried all URL patterns without success")
            
    except Exception as e:
        print(f"Error downloading image {i}: {e}")

print(f"\nFinished attempting to download sample images to {output_dir}/")
print("Note: If this approach didn't work, we may need to explore the EarthView-Viewer code directly") 