
import urllib.request
import urllib.error
import json
import time
import signal
import sys
import RPi.GPIO as GPIO

# === КОНФИГУРАЦИЯ ===
SERVER_URL = "http://192.168.1.50:8080/poll"   #  IP вашего сервера в локальной сети
POLL_INTERVAL = 0.5                             # Опрос каждые 0.5 сек
MAX_TIME_MS = 10000                             # Макс. длительность команды (мс)
TIMEOUT_SEC = 3                                 # Таймаут запроса
# ===================

# Пины в нумерации BCM (стандарт для AlphaBot / L298N)
PINS = {
    "L_FWD": 12,  # IN1
    "L_BWD": 13,  # IN2
    "R_FWD": 20,  # IN3
    "R_BWD": 21   # IN4
}

class Robot:
    def __init__(self):
        GPIO.setmode(GPIO.BCM)
        GPIO.setwarnings(False)
        for pin in PINS.values():
            GPIO.setup(pin, GPIO.OUT)
            GPIO.output(pin, GPIO.LOW)
        print("🤖 Робот инициализирован (RPi.GPIO + urllib)")

    def _set_motors(self, l_fwd, l_bwd, r_fwd, r_bwd):
        GPIO.output(PINS["L_FWD"], l_fwd)
        GPIO.output(PINS["L_BWD"], l_bwd)
        GPIO.output(PINS["R_FWD"], r_fwd)
        GPIO.output(PINS["R_BWD"], r_bwd)

    def forward(self, time_ms):
        print(f"⬆️ Вперёд {time_ms} мс")
        self._set_motors(True, False, True, False)
        time.sleep(time_ms / 1000.0)
        self.stop()

    def left(self, time_ms):
        print(f"⬅️ Влево {time_ms} мс")
        # Разворот на месте: левый назад, правый вперёд
        self._set_motors(False, True, True, False)
        time.sleep(time_ms / 1000.0)
        self.stop()

    def right(self, time_ms):
        print(f"➡️ Вправо {time_ms} мс")
        # Разворот на месте: левый вперёд, правый назад
        self._set_motors(True, False, False, True)
        time.sleep(time_ms / 1000.0)
        self.stop()

    def stop(self):
        print("⏹️ Остановка")
        self._set_motors(False, False, False, False)

    def cleanup(self):
        self.stop()
        GPIO.cleanup()

# Создаём экземпляр
robot = Robot()

#  Обработчик сигналов для безопасного завершения
def graceful_exit(sig, frame):
    print("\n Получен сигнал завершения. Остановка...")
    robot.cleanup()
    sys.exit(0)

signal.signal(signal.SIGINT, graceful_exit)
signal.signal(signal.SIGTERM, graceful_exit)

#  Основной цикл опроса сервера
def poll_server():
    try:
        # urllib.request.urlopen поддерживает таймаут
        with urllib.request.urlopen(SERVER_URL, timeout=TIMEOUT_SEC) as response:
            # Читаем и декодируем ответ
            body = response.read().decode('utf-8')
            # Парсим JSON
            data = json.loads(body)
            
            action = data.get("action", "stop")
            t_ms = max(0, min(int(data.get("time", 0)), MAX_TIME_MS))
            
            if action == "forward":
                robot.forward(t_ms)
            elif action == "left":
                robot.left(t_ms)
            elif action == "right":
                robot.right(t_ms)
            else:
                robot.stop()
                
    except urllib.error.HTTPError as e:
        print(f"[HTTP] Ошибка {e.code}: {e.reason}")
    except urllib.error.URLError as e:
        print(f"[NET] Нет связи с сервером: {e.reason}")
    except json.JSONDecodeError as e:
        print(f"[JSON] Неверный формат ответа: {e}")
    except (KeyError, ValueError) as e:
        print(f"[DATA] Ошибка в данных: {e}")
    except Exception as e:
        print(f"[ERR] Неожиданная ошибка: {e}")

#  Главный цикл
try:
    print(f"[OK] Подключение к {SERVER_URL}...")
    while True:
        poll_server()
        time.sleep(POLL_INTERVAL)
        
except KeyboardInterrupt:
    graceful_exit(None, None)
finally:
    robot.cleanup()