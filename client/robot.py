import httpx
import time

server_url = "http://127.0.0.1:8080/poll"
poll_interval = 1.0

class Robot:
    def forward(self, time_ms):
        print(f"Едем вперед {time_ms} мс")
    def left(self, time_ms):
        print(f"Едем влево {time_ms} мс")
    def right(self, time_ms):
        print(f"Едем вправо {time_ms} мс")
    def stop(self):
        print("Остановка")

robot_nasil = Robot()

with httpx.Client() as client:
    while True:
        try:
            r = client.get(server_url, timeout=2)
            if r.status_code == 200:
                data = r.json()
                action = data["action"]
                t = int(data["time"])
                if action == "forward":
                    robot_nasil.forward(t)
                elif action == "left":
                    robot_nasil.left(t)
                elif action == "right":
                    robot_nasil.right(t)
                else:
                    robot_nasil.stop()
        except Exception as e:
            print(f"error: {e}")
        time.sleep(poll_interval)
