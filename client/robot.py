import httpx
import time
from gpiozero import Robot as GpioZeroRobot
from gpiozero.pins.pigpio import PiGPIOFactory

server_url = "http://127.0.0.1:8080/poll"
poll_interval = 1.0

# 1. Настройка удаленного подключения к Raspberry Pi
# Замените на реальный IP-адрес вашей платы в локальной сети
RPI_IP = "192.168.1.100" 
factory = PiGPIOFactory(host=RPI_IP)

class Robot:
    def __init__(self):
        # 2. Инициализация моторов через gpiozero
        # Параметры: left=(вперед, назад), right=(вперед, назад)
        # Укажите ваши GPIO пины, к которым подключен драйвер моторов (например, L298N)
        self.device = GpioZeroRobot(left=(12, 13), right=(20, 21), pin_factory=factory)
        print("Робот успешно подключен к Raspberry Pi!")

    def forward(self, time_ms):
        print(f"Едем вперед {time_ms} мс")
        self.device.forward()
        time.sleep(time_ms / 1000.0)  # Переводим миллисекунды в секунды
        self.device.stop()

    def left(self, time_ms):
        print(f"Едем влево {time_ms} мс")
        self.device.left()
        time.sleep(time_ms / 1000.0)
        self.device.stop()

    def right(self, time_ms):
        print(f"Едем вправо {time_ms} мс")
        self.device.right()
        time.sleep(time_ms / 1000.0)
        self.device.stop()

    def stop(self):
        print("Остановка")
        self.device.stop()

# Создаем экземпляр нашего робота
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
