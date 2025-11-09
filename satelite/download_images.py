import os
import numpy as np
import random
from PIL import Image
from datasets import load_dataset
from tqdm import tqdm
import json

# Where to save the images
output_dir = "sample_images"
num_images_to_download = 100  # Change this to download more images

os.makedirs(output_dir, exist_ok=True)

print(f"Loading multiple shards from satellogic/EarthView for more diversity...")

# Load images from multiple different shards for diversity
# Total shards is 7863 based on the original documentation
total_shards = 7863
num_shards_to_use = 5  # Use multiple shards for diversity

# Randomly select shard indices
selected_shard_indices = random.sample(range(total_shards), num_shards_to_use)
print(f"Selected shards: {selected_shard_indices}")

all_examples = []

# Load data from each selected shard
for shard_idx in selected_shard_indices:
    try:
        # Format the shard path
        shard_path = f"https://huggingface.co/datasets/satellogic/EarthView/resolve/main/satellogic/train-{shard_idx:05d}-of-{total_shards:05d}.parquet"
        print(f"Loading shard {shard_idx}...")
        
        # Load the shard
        shard_dataset = load_dataset(
            "parquet", 
            data_files={"train": shard_path},
            split="train"
        )
        
        print(f"Loaded shard with {len(shard_dataset)} examples")
        all_examples.append(shard_dataset)
        
    except Exception as e:
        print(f"Error loading shard {shard_idx}: {e}")
        continue

# If we failed to load any shards, try the original approach as fallback
if not all_examples:
    print("Failed to load random shards. Falling back to default shard...")
    try:
        dataset = load_dataset(
            "parquet", 
            data_files={"train": "https://huggingface.co/datasets/satellogic/EarthView/resolve/main/satellogic/train-00000-of-07863.parquet"},
            split="train"
        )
        all_examples.append(dataset)
    except Exception as e:
        print(f"Error loading fallback shard: {e}")
        import traceback
        traceback.print_exc()
        exit(1)

# Create a function to process one example
def process_example(example, idx, shard_id):
    if "rgb" not in example:
        return None
        
    rgb_data = example["rgb"]
    
    # Structure is [revisits][channels][height][width]
    if not (len(rgb_data) > 0 and isinstance(rgb_data[0], list) and len(rgb_data[0]) >= 3):
        return None
        
    # Extract R, G, B channels from first revisit
    channels = rgb_data[0][:3]
    
    # Check if channels are 2D arrays
    if not all(isinstance(ch, list) and len(ch) > 0 and all(isinstance(row, list) for row in ch) for ch in channels):
        return None
        
    # Convert channels to numpy arrays
    r = np.array(channels[0], dtype=np.uint8)
    g = np.array(channels[1], dtype=np.uint8)
    b = np.array(channels[2], dtype=np.uint8)
    
    # Check if all channels have the same shape
    if r.shape != g.shape or g.shape != b.shape:
        return None
        
    # Stack channels to create RGB image
    rgb_img = np.stack([r, g, b], axis=2)
    return Image.fromarray(rgb_img)

# Process and save RGB images
images_saved = 0
progress_bar = tqdm(total=num_images_to_download)

# For each shard, select some random examples
per_shard_samples = max(1, num_images_to_download // len(all_examples))

for shard_id, dataset in enumerate(all_examples):
    # Create a randomized index list for this shard
    shard_size = len(dataset)
    num_to_sample = min(shard_size, per_shard_samples * 2)  # Sample twice as many in case of failures
    
    if num_to_sample <= 0:
        continue
        
    random_indices = random.sample(range(shard_size), num_to_sample)
    
    # Process random examples from this shard
    for idx in random_indices:
        if images_saved >= num_images_to_download:
            break
            
        try:
            # Get the example at the random index
            example = dataset[idx]
            
            # Process the example
            img = process_example(example, idx, shard_id)
            
            if img is not None:
                # Save the image
                img.save(f"{output_dir}/rgb_shard{shard_id}_idx{idx:04d}.png")
                images_saved += 1
                progress_bar.update(1)
                print(f"Saved RGB image {images_saved}/{num_images_to_download} (shard {shard_id}, index {idx})")
                
                # Limit per-shard samples to ensure diversity
                if images_saved % per_shard_samples == 0 and images_saved < num_images_to_download:
                    print(f"Moving to next shard after saving {per_shard_samples} from shard {shard_id}")
                    break
        
        except Exception as e:
            print(f"Error processing item {idx} from shard {shard_id}: {e}")
            continue

progress_bar.close()
print(f"\nFinished downloading. Saved {images_saved} RGB images to {output_dir}/")

# Examine structure of the first example in more detail
if len(dataset) > 0:
    first_example = dataset[0]
    print(f"Available keys in dataset: {list(first_example.keys())}")
    
    # Let's use 'rgb' key as it's more likely to contain RGB data
    key_to_examine = 'rgb'
    if key_to_examine in first_example:
        rgb_data = first_example[key_to_examine]
        print(f"Detailed structure of '{key_to_examine}':")
        print(f"  - Type: {type(rgb_data)}, Length: {len(rgb_data)}")
        
        # First level - list of revisits
        if isinstance(rgb_data, list) and len(rgb_data) > 0:
            revisit = rgb_data[0]
            print(f"  - First revisit type: {type(revisit)}, Length: {len(revisit)}")
            
            # Second level - list of channels or bands
            if isinstance(revisit, list) and len(revisit) > 0:
                band = revisit[0]
                print(f"  - First band type: {type(band)}, Length: {len(band) if isinstance(band, list) else 'N/A'}")
                
                # Third level - possible image data
                if isinstance(band, list) and len(band) > 0:
                    data = band[0]
                    print(f"  - First data element type: {type(data)}")
                    if isinstance(data, list) and len(data) > 0:
                        print(f"  - Sample values: {data[:5]}")
    
    # Specifically check the shape and type of data in the 1m key
    if '1m' in first_example:
        m1_data = first_example['1m']
        print(f"\nDetailed structure of '1m' (likely 1-meter resolution):")
        print(f"  - Type: {type(m1_data)}, Length: {len(m1_data)}")
        
        if isinstance(m1_data, list) and len(m1_data) > 0:
            first_level = m1_data[0]
            print(f"  - First level type: {type(first_level)}, Length: {len(first_level) if isinstance(first_level, list) else 'N/A'}")
            
            if isinstance(first_level, list) and len(first_level) > 0:
                second_level = first_level[0]
                print(f"  - Second level type: {type(second_level)}, Length: {len(second_level) if isinstance(second_level, list) else 'N/A'}")
                
                if isinstance(second_level, list) and len(second_level) > 0:
                    # Analyze shapes if these are potentially arrays
                    try:
                        arr = np.array(second_level)
                        print(f"  - As numpy array shape: {arr.shape}, dtype: {arr.dtype}")
                        
                        # Get min/max to see if it looks like an image
                        if arr.dtype.kind in 'iuf':  # integer or float
                            min_val = np.min(arr)
                            max_val = np.max(arr)
                            print(f"  - Value range: {min_val} to {max_val}")
                    except Exception as e:
                        print(f"  - Could not convert to numpy array: {e}")

# Save raw data to inspect the structure
print("\nSaving raw data to 'raw_data/' directory for manual inspection...")
os.makedirs("raw_data", exist_ok=True)

# Save the first example with its structure
try:
    if len(dataset) > 0:
        first_example = dataset[0]
        
        # Save metadata
        if "metadata" in first_example:
            with open("raw_data/metadata.json", "w") as f:
                json.dump(first_example["metadata"], f, indent=2, default=str)
            print("Saved metadata.json")
        
        # Extract and save RGB data
        if "rgb" in first_example:
            rgb_data = first_example["rgb"]
            # Create a simplified structure to make it easier to understand
            with open("raw_data/rgb_structure.txt", "w") as f:
                f.write(f"RGB data structure:\n")
                f.write(f"- Type: {type(rgb_data)}, Length: {len(rgb_data)}\n")
                
                if isinstance(rgb_data, list) and len(rgb_data) > 0:
                    # Level 1
                    f.write(f"- Level 1 (revisits):\n")
                    for i, revisit in enumerate(rgb_data):
                        f.write(f"  - Revisit {i}: Type {type(revisit)}, Length: {len(revisit) if hasattr(revisit, '__len__') else 'N/A'}\n")
                        
                        # Level 2
                        if isinstance(revisit, list):
                            f.write(f"    - Level 2 (bands/channels):\n")
                            for j, band in enumerate(revisit):
                                f.write(f"      - Band {j}: Type {type(band)}, Length: {len(band) if hasattr(band, '__len__') else 'N/A'}\n")
                                
                                # Level 3
                                if isinstance(band, list) and len(band) > 0:
                                    data_sample = band[:5] if len(band) >= 5 else band
                                    f.write(f"        - Sample data: {data_sample}\n")
            print("Saved rgb_structure.txt")
            
            # Try to convert the first revisit's RGB data to an image
            try:
                # Assuming the structure is [revisits][channels][height][width]
                if len(rgb_data) > 0 and isinstance(rgb_data[0], list) and len(rgb_data[0]) >= 3:
                    # Extract R, G, B channels (assuming they are the first 3 channels)
                    channels = rgb_data[0][:3]
                    
                    # Check if the channels are 2D arrays
                    if all(isinstance(ch, list) and all(isinstance(row, list) for row in ch) for ch in channels):
                        # Convert each channel to a numpy array
                        r = np.array(channels[0], dtype=np.uint8)
                        g = np.array(channels[1], dtype=np.uint8)
                        b = np.array(channels[2], dtype=np.uint8)
                        
                        # Stack the channels to form an RGB image
                        if r.shape == g.shape == b.shape:
                            rgb_img = np.stack([r, g, b], axis=2)
                            img = Image.fromarray(rgb_img)
                            img.save("raw_data/first_image_attempt.png")
                            print("Successfully saved first_image_attempt.png")
                        else:
                            print(f"Channel shapes don't match: R {r.shape}, G {g.shape}, B {b.shape}")
                    else:
                        print("Channels are not 2D arrays")
            except Exception as e:
                print(f"Error creating image from RGB data: {e}")
        
        # Try saving the 1m data as an image as well
        if "1m" in first_example:
            try:
                m1_data = first_example["1m"]
                if len(m1_data) > 0 and isinstance(m1_data[0], list) and len(m1_data[0]) > 0:
                    # Assuming this might be a single-channel (grayscale) image
                    data = m1_data[0][0]  # First revisit, first channel
                    if isinstance(data, list) and all(isinstance(row, list) for row in data):
                        arr = np.array(data, dtype=np.uint8)
                        img = Image.fromarray(arr)
                        img.save("raw_data/first_1m_attempt.png")
                        print("Successfully saved first_1m_attempt.png")
            except Exception as e:
                print(f"Error creating image from 1m data: {e}")
            
except Exception as e:
    print(f"Error saving raw data: {e}")
    import traceback
    traceback.print_exc()

print(f"\nFinished inspection. Check the raw_data/ directory for the saved data structures.") 