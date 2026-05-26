import cv2
import numpy as np
import json
import time
import threading
import requests
from typing import List, Tuple
import os


# Точки для расчётов
pered = (0, 0)
zad = (0, 0)
centre = (0, 0)
user = (0, 0)
station = (0, 0)

# Переменные для контроля направления и расстояния
angle = 0.0
distance = 0.0
direction = 0  # 0 - влево, 1 - вправо

# Параметры движения робота
omega = 10  # угловая скорость
velocity = 52  # линейная скорость

# Минимальные значения для выполнения команд
min_angle = 5
min_dist = 10

# Цвета маркеров
COLOR_RED = 0
COLOR_GREEN = 1
COLOR_BLUE = 2
COLOR_PURPLE = 3

# Блокировка для потокобезопасности
lock = threading.Lock()

# URL сервера команд
SERVER_URL = "http://192.168.1.102:8081/command"


def calculate_centre():
    """Вычисление центра между передней и задней точками"""
    global centre, pered, zad
    centre = ((pered[0] + zad[0]) // 2, (pered[1] + zad[1]) // 2)


def calculate_angle():
    """Вычисление угла между направлением робота и целью"""
    global angle, direction, centre, pered, station
    
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
    direction = 1 if perpendicular > 0 else 0


def calculate_distance_value():
    """Вычисление расстояния до целевой точки"""
    global distance, centre, station
    
    b = np.array([station[0] - centre[0], station[1] - centre[1]], dtype=np.float32)
    distance = np.linalg.norm(b)


def calculate():
    """Полный расчёт параметров движения"""
    calculate_centre()
    calculate_angle()
    calculate_distance_value()
    print(f"Direction: {direction}")
    print(f"Angle: {angle:.2f}, Distance: {distance:.2f}")


def draw_lines(frame):
    """Рисование линий на кадре для визуализации"""
    cv2.line(frame, pered, zad, (255, 0, 0), 1)
    cv2.line(frame, centre, station, (255, 0, 0), 1)


def decode_qr_code(qrcode, src_points):
    """Декодирование QR кода и определение типа маркера"""
    global pered, zad, user, station
    
    detector = cv2.QRCodeDetector()
    data, points, _ = detector.detectAndDecode(qrcode)
    
    if data:
        print(f"QR Data: {data}")
        
        center_x = np.mean([p[0] for p in src_points])
        center_y = np.mean([p[1] for p in src_points])
        
        if data == "1":
            pered = (int(center_x), int(center_y))
        elif data == "2":
            zad = (int(center_x), int(center_y))
        elif data == "3":
            user = (int(center_x), int(center_y))
        elif data == "4":
            station = (int(center_x), int(center_y))
        
        calculate()


def correct_perspective(img, src_points):
    """Коррекция перспективы для QR кода"""
    src_points = np.array(src_points, dtype=np.float32)
    
    w = cv2.norm(src_points[0] - src_points[1])
    h = cv2.norm(src_points[1] - src_points[2])
    
    size = (int(w * 10), int(h * 10))
    dst_points = np.array([
        [0, 0],
        [w * 10, 0],
        [w * 10, h * 10],
        [0, h * 10]
    ], dtype=np.float32)
    
    M = cv2.getPerspectiveTransform(src_points, dst_points)
    warped = cv2.warpPerspective(img, M, size)
    
    decode_qr_code(warped, src_points)
    cv2.imshow("QR", warped)


def find_color(image, color_enum):
    """Поиск маркеров определённого цвета в изображении"""
    hsv = cv2.cvtColor(image, cv2.COLOR_BGR2HSV)
    
    if color_enum == COLOR_RED:
        # Красный цвет имеет два диапазона в HSV
        lower_red1 = np.array([0, 83, 95])
        upper_red1 = np.array([5, 133, 150])
        lower_red2 = np.array([165, 83, 95])
        upper_red2 = np.array([180, 133, 150])
        
        mask1 = cv2.inRange(hsv, lower_red1, upper_red1)
        mask2 = cv2.inRange(hsv, lower_red2, upper_red2)
        mask = cv2.bitwise_or(mask1, mask2)
    
    elif color_enum == COLOR_GREEN:
        lower_green = np.array([47, 34, 45])
        upper_green = np.array([67, 134, 155])
        mask = cv2.inRange(hsv, lower_green, upper_green)
    
    else:
        return []
    
    # Морфологические операции
    kernel = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (5, 5))
    mask = cv2.dilate(mask, kernel, iterations=5)
    mask = cv2.erode(mask, kernel, iterations=2)
    
    # Поиск контуров
    contours, _ = cv2.findContours(mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_NONE)
    
    output = []
    for contour in contours:
        if len(contour) < 3:
            continue
        
        rect = cv2.minAreaRect(contour)
        points = cv2.boxPoints(rect)
        points = np.float32(points)
        output.append(points)
    
    return output


def create_json_command(command, time_ms):
    """Создание JSON файла с командой для отправки на сервер"""
    data = {
        "command": command,
        "time": time_ms
    }
    
    with open("message.json", "w") as f:
        json.dump(data, f)


def calculate_direction_command():
    """Расчёт и отправка команды поворота"""
    time_ms = int(angle / omega * 200)
    if direction:
        create_json_command("right", time_ms)
    else:
        create_json_command("left", time_ms)


def calculate_distance_command():
    """Расчёт и отправка команды движения вперёд"""
    time_ms = int(distance / velocity * 200)
    create_json_command("forward", time_ms)


def read_json_response():
    """Чтение ответа от робота из JSON файла"""
    with lock:
        try:
            with open("answer.json", "r") as f:
                data = json.load(f)
                answer = data.get("answer", "")
                
                if answer == "received":
                    return "received"
                elif answer == "not received":
                    return "not received"
                elif answer == "done":
                    return "done"
        except (FileNotFoundError, json.JSONDecodeError):
            pass
    
    return ""


def send_command_to_server():
    """Отправка команды на сервер робота через HTTP POST"""
    try:
        with open("message.json", "r") as f:
            data = json.load(f)
        
        response = requests.post(SERVER_URL, json=data, timeout=5)
        
        if response.status_code == 200:
            print(f"Command sent successfully: {data}")
        else:
            print(f"Error: {response.status_code} - {response.text}")
    
    except Exception as e:
        print(f"Connection error: {e}")


def main():
    """Основной цикл захвата видео и обработки QR кодов"""
    global pered, zad, user, station
    
    cap = cv2.VideoCapture(0)
    
    if not cap.isOpened():
        print("Error: Cannot open camera")
        return -1
    
    cv2.namedWindow("QR Scanner", cv2.WINDOW_AUTOSIZE)
    
    iterations = 0
    fake_iterations = 0
    stickers = []
    
    while True:
        ret, frame = cap.read()
        if not ret:
            break
        
        fake_frame = frame.copy()
        iterations += 1
        result = read_json_response()
        
        # Поиск маркеров каждые 30 кадров
        if iterations == 30:
            green_stickers = find_color(frame, COLOR_GREEN)
            red_stickers = find_color(frame, COLOR_RED)
            stickers = green_stickers + red_stickers
            
            for sticker in stickers:
                correct_perspective(frame, sticker)
            
            iterations = 0
        
        # Рисование контуров найденных маркеров
        for sticker in stickers:
            for i in range(4):
                pt1 = tuple(map(int, sticker[i]))
                pt2 = tuple(map(int, sticker[(i + 1) % 4]))
                cv2.line(fake_frame, pt1, pt2, (255, 0, 0), 1)
        
        draw_lines(fake_frame)
        cv2.imshow("QR Scanner", fake_frame)
        
        if cv2.waitKey(1) >= 0:
            break
        
        fake_iterations += 1
        
        # Отправка команд робота
        if result in ["not received", "done", ""]:
            if fake_iterations > 300:
                if angle > min_angle:
                    calculate_direction_command()
                    send_command_to_server()
                elif distance > min_dist:
                    calculate_distance_command()
                    send_command_to_server()
                
                fake_iterations = 0
    
    cap.release()
    cv2.destroyAllWindows()
    return 0


if __name__ == "__main__":
    main()