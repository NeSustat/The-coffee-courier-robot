import requests
import json
import time


# Configuration
SERVER_URL = "http://192.168.1.102:8081/command"
MESSAGE_FILE = "message.json"


def send_command():
    """Send command to robot server from JSON file"""
    try:
        # Read command from JSON file
        with open(MESSAGE_FILE, "r") as f:
            data = json.load(f)
        
        print(f"Sending command: {data}")
        
        # Send POST request to server
        response = requests.post(SERVER_URL, json=data, timeout=10)
        
        if response.status_code == 200:
            print("Request successful!")
            print(f"Response: {response.json()}")
        else:
            print(f"Request failed with status code {response.status_code}")
            print(f"Response text: {response.text}")
    
    except FileNotFoundError:
        print(f"Error: {MESSAGE_FILE} not found")
    
    except json.JSONDecodeError:
        print(f"Error: Invalid JSON in {MESSAGE_FILE}")
    
    except requests.exceptions.RequestException as e:
        print(f"Connection error: {e}")
    
    except Exception as e:
        print(f"Error: {e}")


if __name__ == "__main__":
    send_command()