import cv2
import numpy as np
import json
import requests
import time
import threading
import argparse
from enum import Enum
from typing import Optional, Tuple

# Глобальные переменные для координат
pered = None  # Передняя часть робота (QR код "1")
zad = None    # Задняя часть робота (QR код "2")
centre = None # Центр робота
user = None   # Пользователь (QR код "3")
station = None # Станция (QR код "4")

# Параметры движения
angle = 0.0
length = 0.0
direction = ""
omega = 23 * 10  # Угловая скорость
velocity = 2.6 * 20  # Линейная скорость

# Константы
coef = 11.5
min_angle = 15
min_length = 10

# URL сервера робота
SERVER_URL = "http://192.168.1.101:8081/command"

# Мьютекс для потокобезопасности
lock = threading.Lock()


def calculating_centre():
    """Вычисление центра робота"""
    global centre, pered, zad
    if pered is not None and zad is not None:
        centre = (
            (pered[0] + zad[0]) // 2,
            (pered[1] + zad[1]) // 2
        )


def calculating_angle():
    """Вычисление угла поворота до станции"""
    global angle, direction, pered, centre, station
    
    with lock:
        if pered is None or centre is None or station is None:
            return
        
        a = np.array([pered[0] - centre[0], pered[1] - centre[1]], dtype=np.float32)
        b = np.array([station[0] - centre[0], station[1] - centre[1]], dtype=np.float32)
        
        b_length = np.linalg.norm(b)
        a_length = np.linalg.norm(a)
        
        if a_length == 0 or b_length == 0:
            return
        
        cos_angle = np.dot(a, b) / (a_length * b_length)
        cos_angle = np.clip(cos_angle, -1, 1)
        angle = np.arccos(cos_angle) * 180 / np.pi
        
        perpendicular = a[0] * b[1] - a[1] * b[0]
        direction = "RIGHT" if perpendicular > 0 else "LEFT"


def calculating_length():
    """Вычисление расстояния до станции"""
    global length, centre, station
    
    with lock:
        if centre is None or station is None:
            return
        
        b = np.array([station[0] - centre[0], station[1] - centre[1]], dtype=np.float32)
        length = np.linalg.norm(b)


def send_command_to_server(command, time_sec):
    """Отправка команды на сервер робота"""
    data = {
        "command": command,
        "time": float(time_sec)  # Преобразуем в стандартный Python float
    }
    
    try:
        response = requests.post(SERVER_URL, json=data, timeout=5)
        
        if response.status_code == 200:
            print(f"✓ Команда отправлена: {command} на {time_sec:.2f} сек")
            return True
        else:
            print(f"✗ Ошибка: {response.status_code} - {response.text}")
            return False
    except Exception as e:
        print(f"✗ Ошибка подключения: {e}")
        return False


def rotate():
    """Поворот робота"""
    global pered, zad
    
    time_sec = float(angle / omega)  # Преобразуем в стандартный Python float
    send_command_to_server(direction, time_sec)
    
    # Сброс позиций
    pered = None
    zad = None


def move():
    """Движение робота вперёд"""
    global pered, zad
    
    if pered is None or zad is None:
        return
    
    robot_length = float(np.sqrt((pered[0] - zad[0])**2 + (pered[1] - zad[1])**2))
    if robot_length == 0:
        return
    
    time_sec = float(length * coef / robot_length / velocity)  # Преобразуем в стандартный Python float
    send_command_to_server("FORWARD", time_sec)
    
    # Сброс позиций
    pered = None
    zad = None


class State(Enum):
    """Состояния конечного автомата"""
    WAITING = 1
    CALCULATING_ANGLE = 2
    ROTATING = 3
    CALCULATING_DISTANCE = 4
    MOVING = 5


class Connection:
    """Конечный автомат для управления роботом"""
    
    def __init__(self):
        self.state = State.WAITING
    
    def process_znak_sverhu(self):
        """Обработка события: все маркеры обнаружены"""
        print("→ ZNAK_SVERHU")
        
        if self.state == State.WAITING:
            calculating_angle()
            self.state = State.CALCULATING_ANGLE
            self.process_angle_calculated()
    
    def process_angle_calculated(self):
        """Обработка события: угол вычислен"""
        print(f"→ ANGLE_CALCULATED: {angle:.2f}° ({direction})")
        
        if self.state == State.CALCULATING_ANGLE:
            if angle > min_angle:
                rotate()
                time.sleep(5)  # Ждём выполнения команды
                self.state = State.ROTATING
                self.process_rotated()
            else:
                calculating_length()
                self.state = State.CALCULATING_DISTANCE
                self.process_distance_calculated()
    
    def process_rotated(self):
        """Обработка события: поворот выполнен"""
        print("→ ROTATED")
        
        if self.state == State.ROTATING:
            calculating_angle()
            self.state = State.CALCULATING_ANGLE
            self.process_angle_calculated()
    
    def process_distance_calculated(self):
        """Обработка события: расстояние вычислено"""
        print(f"→ DISTANCE_CALCULATED: {length:.2f}")
        
        if self.state == State.CALCULATING_DISTANCE:
            if length < min_length:
                print("★ Цель достигнута!")
                self.state = State.WAITING
            else:
                move()
                time.sleep(5)  # Ждём выполнения команды
                self.state = State.MOVING
                self.process_moved()
    
    def process_moved(self):
        """Обработка события: движение выполнено"""
        print("→ MOVED")
        
        if self.state == State.MOVING:
            calculating_length()
            if length < min_length:
                print("★ Цель достигнута!")
                self.state = State.WAITING
            else:
                calculating_angle()
                self.state = State.CALCULATING_ANGLE
                self.process_angle_calculated()


def draw(frame):
    """Рисование линий на кадре"""
    if pered is not None and zad is not None:
        cv2.line(frame, pered, zad, (255, 0, 0), 2)
    
    if centre is not None and station is not None:
        cv2.line(frame, centre, station, (255, 0, 0), 2)


def decode_qr_code(frame):
    """Декодирование QR кодов"""
    global pered, zad, user, station
    
    with lock:
        detector = cv2.QRCodeDetector()
        retval, decoded_info, points, _ = detector.detectAndDecodeMulti(frame)
        
        if retval:
            for i in range(len(decoded_info)):
                data = decoded_info[i]
                if data:
                    qr_points = points[i]
                    
                    # Вычисление центра QR кода
                    center = np.mean(qr_points, axis=0)
                    center_x = int(center[0])
                    center_y = int(center[1])
                    
                    if data == " robotA ":
                        pered = (center_x, center_y)
                    elif data == " robotB ":
                        zad = (center_x, center_y)
                    elif data == "3":
                        user = (center_x, center_y)
                    elif data == " coffee ":
                        station = (center_x, center_y)
            
            calculating_centre()


def draw_info_on_frame(frame):
    """Отображение информации на кадре"""
    y_offset = 30
    
    if pered:
        cv2.putText(frame, f"Pered (1): {pered}", (10, y_offset),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 255, 0), 2)
        y_offset += 25
    
    if zad:
        cv2.putText(frame, f"Zad (2): {zad}", (10, y_offset),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255, 0, 0), 2)
        y_offset += 25
    
    if station:
        cv2.putText(frame, f"Station (4): {station}", (10, y_offset),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 0, 255), 2)
        y_offset += 25
    
    if angle > 0:
        cv2.putText(frame, f"Angle: {angle:.2f} ({direction})", (10, y_offset),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.5, (200, 200, 200), 2)
        y_offset += 25
    
    if length > 0:
        cv2.putText(frame, f"Length: {length:.2f}", (10, y_offset),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.5, (200, 200, 200), 2)


def cam(camera_source):
    """Поток для захвата видео и обработки QR кодов"""
    cap = cv2.VideoCapture(camera_source)
    if not cap.isOpened():
        print("Ошибка: не удалось открыть камеру")
        return
    
    cv2.namedWindow("QR Scanner", cv2.WINDOW_AUTOSIZE)
    
    print("Камера открыта. Начинаем сканирование...")
    print("Нажмите 'q' для выхода")
    
    while True:
        ret, frame = cap.read()
        if not ret:
            print("Ошибка чтения кадра")
            break
        
        decode_qr_code(frame)
        draw(frame)
        draw_info_on_frame(frame)
        
        # Статус
        status_text = "Поиск QR кодов..."
        if pered and zad and station:
            status_text = "Все маркеры найдены!"
        
        cv2.putText(frame, status_text, (10, frame.shape[0] - 10),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 0), 2)
        
        cv2.imshow("QR Scanner", frame)
        
        if cv2.waitKey(1) & 0xFF == ord('q'):
            break
    
    cap.release()
    cv2.destroyAllWindows()


def main(camera_source=0):
    """Основной цикл программы"""
    print(f"Используется источник камеры: {camera_source}")
    print(f"URL сервера: {SERVER_URL}")
    
    connection = Connection()
    
    # Запуск потока камеры
    camera_thread = threading.Thread(target=cam, args=(camera_source,), daemon=True)
    camera_thread.start()
    
    print("\nОжидание обнаружения всех маркеров...")
    
    # Основной цикл конечного автомата
    while True:
        try:
            if pered is not None and zad is not None and station is not None:
                if pered != (0, 0) and zad != (0, 0) and station != (0, 0):
                    connection.process_znak_sverhu()
                    # Ждём перед следующей проверкой
                    time.sleep(1)
        except KeyboardInterrupt:
            print("\nПрограмма остановлена пользователем")
            break
    
    return 0


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='QR код сканер с конечным автоматом для управления роботом')
    parser.add_argument('--camera', type=str, default='0', 
                       help='Источник камеры: номер (0,1,2) или URL IP-камеры')
    parser.add_argument('--server', type=str, default=SERVER_URL,
                       help=f'URL сервера робота (по умолчанию: {SERVER_URL})')
    
    args = parser.parse_args()
    
    # Обновляем URL сервера если указан
    if args.server != SERVER_URL:
        SERVER_URL = args.server
    
    # Преобразуем camera аргумент
    camera_source = args.camera
    if camera_source.isdigit():
        camera_source = int(camera_source)
    
    main(camera_source)
