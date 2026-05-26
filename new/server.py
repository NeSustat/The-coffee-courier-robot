from http.server import HTTPServer, BaseHTTPRequestHandler
import json
import time
import signal
import sys
import threading

try:
    import RPi.GPIO as GPIO
    GPIO_AVAILABLE = True
except ImportError:
    GPIO_AVAILABLE = False
    print("Warning: RPi.GPIO not available. Running in simulation mode.")


# === CONFIGURATION ===
HOST = "0.0.0.0"
PORT = 8081
MAX_TIME_MS = 10000
# =====================

# Motor pins in BCM numbering
PINS = {
    "L_FWD": 12,   # Left forward
    "L_BWD": 13,   # Left backward
    "R_FWD": 21,   # Right forward
    "R_BWD": 20    # Right backward
}


class Robot:
    """Class for controlling robot motors via GPIO"""
    
    def __init__(self):
        """Initialize GPIO and set all pins to LOW"""
        if GPIO_AVAILABLE:
            GPIO.setmode(GPIO.BCM)
            GPIO.setwarnings(False)
            
            # Setup motor pins
            for pin in PINS.values():
                GPIO.setup(pin, GPIO.OUT)
                GPIO.output(pin, GPIO.LOW)
            
            # Setup enable pins
            GPIO.setup(6, GPIO.OUT)
            GPIO.output(6, GPIO.HIGH)
            GPIO.setup(26, GPIO.OUT)
            GPIO.output(26, GPIO.HIGH)
        
        print("Robot initialized")
    
    def _set_motors(self, l_fwd, l_bwd, r_fwd, r_bwd):
        """Set state of all four motor pins"""
        if GPIO_AVAILABLE:
            GPIO.output(PINS["L_FWD"], l_fwd)
            GPIO.output(PINS["L_BWD"], l_bwd)
            GPIO.output(PINS["R_FWD"], r_fwd)
            GPIO.output(PINS["R_BWD"], r_bwd)
        else:
            state = {
                "L_FWD": l_fwd,
                "L_BWD": l_bwd,
                "R_FWD": r_fwd,
                "R_BWD": r_bwd
            }
            print(f"Simulating motor state: {state}")
    
    def stop(self):
        """Stop all motors"""
        print("Stopping robots")
        self._set_motors(False, False, False, False)
    
    def forward(self, time_ms):
        """Move robot forward for specified time in milliseconds"""
        print(f"Moving forward for {time_ms} ms")
        self._set_motors(True, False, True, False)
        time.sleep(time_ms / 1000.0)
        self.stop()
    
    def left(self, time_ms):
        """Rotate robot left (in place) for specified time in milliseconds"""
        print(f"Rotating left for {time_ms} ms")
        # Left backward, right forward - left rotation
        self._set_motors(False, True, True, False)
        time.sleep(time_ms / 1000.0)
        self.stop()
    
    def right(self, time_ms):
        """Rotate robot right (in place) for specified time in milliseconds"""
        print(f"Rotating right for {time_ms} ms")
        # Left forward, right backward - right rotation
        self._set_motors(True, False, False, True)
        time.sleep(time_ms / 1000.0)
        self.stop()
    
    def cleanup(self):
        """Clean up GPIO resources"""
        self.stop()
        if GPIO_AVAILABLE:
            GPIO.cleanup()


# Create robot instance
robot = Robot()


def graceful_exit(sig, frame):
    """Handle signals for safe shutdown"""
    print("Shutting down...")
    robot.cleanup()
    sys.exit(0)


signal.signal(signal.SIGINT, graceful_exit)
signal.signal(signal.SIGTERM, graceful_exit)


class Handler(BaseHTTPRequestHandler):
    """HTTP request handler for robot commands"""
    
    def log_message(self, format, *args):
        """Disable default logging"""
        pass
    
    def do_POST(self):
        """Handle POST request with robot command"""
        if self.path == "/command":
            try:
                # Read request body
                content_length = int(self.headers.get("Content-Length", 0))
                body = self.rfile.read(content_length)
                data = json.loads(body)
                
                # Extract command and time
                command = data.get("command", "stop").upper()
                time_ms = max(0, min(data.get("time", 0), MAX_TIME_MS))
                
                print(f"[CMD] {command} {time_ms} ms")
                
                # Execute command in separate thread
                command_thread = threading.Thread(
                    target=self._execute_command,
                    args=(command, time_ms)
                )
                command_thread.daemon = True
                command_thread.start()
                
                # Send response
                self.send_response(200)
                self.send_header("Content-Type", "application/json")
                self.end_headers()
                response = json.dumps({"status": "received"})
                self.wfile.write(response.encode())
            
            except Exception as e:
                print(f"Error processing request: {e}")
                self.send_response(400)
                self.end_headers()
    
    def _execute_command(self, command, time_ms):
        """Execute robot command"""
        if command == "FORWARD":
            robot.forward(time_ms)
        elif command == "LEFT":
            robot.left(time_ms)
        elif command == "RIGHT":
            robot.right(time_ms)
        else:
            robot.stop()


def run_server():
    """Start HTTP server"""
    server = HTTPServer((HOST, PORT), Handler)
    print(f"Server started on {HOST}:{PORT}")
    print(f"Waiting for commands on http://{HOST}:{PORT}/command")
    
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("Server stopping...")
        graceful_exit(None, None)


if __name__ == "__main__":
    run_server()