import os
import requests
from PIL import Image
from io import BytesIO

# Create a directory for the sample image
os.makedirs("sample_single", exist_ok=True)

# Test different types of URLs that might work
# These are common patterns for dataset samples, demos, and previews
test_urls = [
    # Try direct URLs to the EarthView Viewer space
    "https://huggingface.co/spaces/satellogic/EarthView-Viewer/resolve/main/earthview.py",
    "https://huggingface.co/spaces/satellogic/EarthView-Viewer/resolve/main/app.py",
    
    # Try sample images with different names
    "https://huggingface.co/spaces/satellogic/EarthView-Viewer/resolve/main/sample.png",
    "https://huggingface.co/spaces/satellogic/EarthView-Viewer/resolve/main/samples/sample.png",
    "https://huggingface.co/spaces/satellogic/EarthView-Viewer/resolve/main/static/sample.png",
    "https://huggingface.co/spaces/satellogic/EarthView-Viewer/resolve/main/assets/sample.png",
    
    # Try dataset samples
    "https://huggingface.co/datasets/satellogic/EarthView/resolve/main/sample.png",
    "https://huggingface.co/datasets/satellogic/EarthView/resolve/main/preview.png",
    
    # Try direct from the blog content images
    "https://satellogic.com/wp-content/uploads/2024/05/Sample-Satellogic-data.jpg",
    "https://satellogic.com/wp-content/uploads/2024/05/earthview-sample.jpg"
]

# Try each URL and see what works
for url in test_urls:
    print(f"\nTrying URL: {url}")
    try:
        response = requests.get(url, timeout=10)
        print(f"Status code: {response.status_code}")
        
        if response.status_code == 200:
            print(f"SUCCESS: Got content type: {response.headers.get('content-type', 'unknown')}")
            
            # If it looks like an image, save it
            if 'image' in response.headers.get('content-type', ''):
                try:
                    img = Image.open(BytesIO(response.content))
                    filename = os.path.join("sample_single", os.path.basename(url))
                    img.save(filename)
                    print(f"Saved image to {filename}")
                except Exception as e:
                    print(f"Error saving image: {e}")
            # If it's the Python file, save it anyway
            elif url.endswith('.py'):
                filename = os.path.join("sample_single", os.path.basename(url))
                with open(filename, 'wb') as f:
                    f.write(response.content)
                print(f"Saved Python file to {filename}")
                
                # If it's earthview.py, print first 10 lines
                if 'earthview.py' in url:
                    print("\nFirst 10 lines of earthview.py:")
                    content = response.content.decode('utf-8')
                    for i, line in enumerate(content.split('\n')[:10]):
                        print(f"{i+1}: {line}")
    except Exception as e:
        print(f"Error with URL {url}: {e}")

print("\nFinished testing URLs. Check sample_single/ directory for any downloaded files.") 