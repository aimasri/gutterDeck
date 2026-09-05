import os
import json

CONFIG_DIR = os.path.expanduser("~/.config/gutter-deck")
CONFIG_FILE = os.path.join(CONFIG_DIR, "config.json")

DEFAULT_CONFIG = {
    "gutters": [
        {
            "id": "gutter_1",
            "name": "Dev",
            "color": "#80FF5555",  # ARGB format for transparency (50% red)
            "command": "google-chrome --new-window"
        },
        {
            "id": "gutter_2",
            "name": "Preview",
            "color": "#805555FF",  # ARGB format (50% blue)
            "command": "google-chrome --profile-directory='Profile 3' --new-window"
        }
    ],
    "settings": {
        "gutter_width": 18,
        "screen_width": 1080,
        "screen_height": 1920,
        "first_launch": True
    }
}

def load_config():
    if not os.path.exists(CONFIG_FILE):
        save_config(DEFAULT_CONFIG)
        return DEFAULT_CONFIG
    try:
        with open(CONFIG_FILE, 'r') as f:
            return json.load(f)
    except Exception:
        return DEFAULT_CONFIG

def save_config(config_data):
    if not os.path.exists(CONFIG_DIR):
        os.makedirs(CONFIG_DIR)
    with open(CONFIG_FILE, 'w') as f:
        json.dump(config_data, f, indent=4)
