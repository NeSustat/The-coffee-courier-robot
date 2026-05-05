import httpx

server_url = "http://127.0.0.1:8080/poll"
poll_interval = 0.2


class Robot:

    def forward(time_ms):
        print(f"Едем вперед {time_ms} секунд")
    
    def left(time_ms):
        print(f"Едем влево {time_ms} секунд")
    
    def right(time_ms):
        print(f"Едем вправо {time_ms} секунд")
    
    def stop():
        print("Остановка")


robot_nasil = Robot()

with httpx.Client() as client:
    r = client.get(server_url)
    data = r.json()
    if data["action"] == "forward":
        robot_nasil.forward(int(data["time"]))
    elif data["action"] == "left":
        robot_nasil.left(int(data["time"]))
    elif data["action"] == "right":
        robot_nasil.right(int(data["time"]))
    else:
        robot_nasil.stop()
    