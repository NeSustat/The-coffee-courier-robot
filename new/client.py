import cv2
import numpy as np
import json
import time
import threading
import requests
from typing import Dict, Tuple


# Координаты робота и целей
robotA_pos = None  # Передняя часть робота (переди)
robotB_pos = None  # Задняя часть робота (зад)
coffee_pos = None  # Целевая точка (куда надо приехать)

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

# Блокировка для потокобезопасности
lock = threading.Lock()

# URL сервера команд
SERVER_URL = "http://192.168.1.101:8081/command"

# Словарь обнаруженных QR кодов
detected_qr_codes = {}


def detect_qr_codes(frame):
    """Обнаружение QR кодов в кадре"""
    global robotA_pos, robotB_pos, coffee_pos, detected_qr_codes
    
    detector = cv2.QRCodeDetector()
    
    # Поиск всех QR кодов в кадре
    retval, decoded_info, points, _ = detector.detectMulti(frame)
    
    if retval:
        for i in range(len(decoded_info)):
            data = decoded_info[i]
            qr_points = points[i]
            
            if data:
                print(f"QR Code found: {data}")
                
                # Вычисление центра QR кода
                center = np.mean(qr_points, axis=0)
                center_x = int(center[0])
                center_y = int(center[1])
                
                # Сохранение координат в зависимости от содержимого QR кода
                if data == " robotA ":
                    robotA_pos = (center_x, center_y)
                    detected_qr_codes[" robotA "] = (center_x, center_y)
                    print(f"  robotA (переди) at {robotA_pos}")
                
                elif data == " robotB ":
                    robotB_pos = (center_x, center_y)
                    detected_qr_codes[" robotB "] = (center_x, center_y)
                    print(f"  robotB (зад) at {robotB_pos}")
                
                elif data == " coffee ":
                    coffee_pos = (center_x, center_y)
                    detected_qr_codes[" coffee "] = (center_x, center_y)
                    print(f"  coffee (цель) at {coffee_pos}")
                
                # Рисование контура QR кода на экране
                draw_qr_contour(frame, qr_points, data)


def draw_qr_contour(frame, points, label):
    """Рисование контура QR кода и подписи"""
    points = np.int32(points)
    
    # Цвет в зависимости от типа QR кода
    if label == " robotA ":
        color = (0, 255, 0)  # Зелёный
    elif label == " robotB ":
        color = (255, 0, 0)  # Красный
    elif label == " coffee ":
        color = (0, 0, 255)  # Синий
    else:
        color = (255, 255, 255)  # Белый
    
    # Рисование контура
    for i in range(len(points)):
        pt1 = tuple(points[i])
        pt2 = tuple(points[(i + 1) % len(points)])
        cv2.line(frame, pt1, pt2, color, 2)
    
    # Рисование текста с названием
    center = np.mean(points, axis=0)
    cv2.putText(frame, label, tuple(map(int, center)), 
                cv2.FONT_HERSHEY_SIMPLEX, 0.5, color, 2)


def calculate_robot_parameters():
    """Вычисление угла и расстояния до цели"""
    global angle, distance, direction, robotA_pos, robotB_pos, coffee_pos
    
    if robotA_pos is None or robotB_pos is None or coffee_pos is None:
        print("Ожидание обнаружения всех QR кодов...")
        return False
    
    # Вычисление центра робота
    centre = (
        (robotA_pos[0] + robotB_pos[0]) // 2,
        (robotA_pos[1] + robotB_pos[1]) // 2
    )
    
    # Вектор направления робота (от зада к передней части)
    robot_direction = np.array([
        robotA_pos[0] - centre[0],
        robotA_pos[1] - centre[1]
    ], dtype=np.float32)
    
    # Вектор до цели (от центра робота к coffee)
    to_target = np.array([
        coffee_pos[0] - centre[0],
        coffee_pos[1] - centre[1]
    ], dtype=np.float32)
    
    robot_dir_length = np.linalg.norm(robot_direction)
    target_length = np.linalg.norm(to_target)
    
    if robot_dir_length == 0 or target_length == 0:
        return False
    
    # Вычисление угла между направлением робота и целью
    cos_angle = np.dot(robot_direction, to_target) / (robot_dir_length * target_length)
    cos_angle = np.clip(cos_angle, -1, 1)
    angle = np.arccos(cos_angle) * 180 / np.pi
    
    # Определение направления поворота (влево или вправо)
    # В OpenCV ось Y направлена вниз, поэтому знак cross product инвертирован
    perpendicular = robot_direction[0] * to_target[1] - robot_direction[1] * to_target[0]
    direction = 0 if perpendicular > 0 else 1
    
    # Вычисление расстояния до цели
    distance = target_length
    
    print(f"Угол: {angle:.2f}°, Расстояние: {distance:.2f} пиксели, Направление: {'вправо' if direction else 'влево'}")
    return True


def draw_info_on_frame(frame):
    """Рисование информации на кадре"""
    if robotA_pos and robotB_pos:
        centre = (
            (robotA_pos[0] + robotB_pos[0]) // 2,
            (robotA_pos[1] + robotB_pos[1]) // 2
        )
        
        # Рисование линии направления робота
        cv2.line(frame, centre, robotA_pos, (100, 100, 100), 2)
    
    if coffee_pos and robotA_pos and robotB_pos:
        centre = (
            (robotA_pos[0] + robotB_pos[0]) // 2,
            (robotA_pos[1] + robotB_pos[1]) // 2
        )
        
        # Рисование линии к цели
        cv2.line(frame, centre, coffee_pos, (255, 255, 0), 2)
    
    # Вывод информации на экран
    y_offset = 30
    if robotA_pos:
        cv2.putText(frame, f"robotA: {robotA_pos}", (10, y_offset),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 255, 0), 1)
        y_offset += 25
    
    if robotB_pos:
        cv2.putText(frame, f"robotB: {robotB_pos}", (10, y_offset),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255, 0, 0), 1)
        y_offset += 25
    
    if coffee_pos:
        cv2.putText(frame, f"coffee: {coffee_pos}", (10, y_offset),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 0, 255), 1)
        y_offset += 25
    
    if angle > 0:
        cv2.putText(frame, f"Angle: {angle:.2f}°", (10, y_offset),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.5, (200, 200, 200), 1)
        y_offset += 25
    
    if distance > 0:
        cv2.putText(frame, f"Distance: {distance:.2f}", (10, y_offset),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.5, (200, 200, 200), 1)


def create_json_command(command, time_ms):
    """Создание JSON файла с командой для отправки на сервер"""
    data = {
        "command": command,
        "time": time_ms
    }
    
    with open("message.json", "w") as f:
        json.dump(data, f)


def calculate_and_send_direction_command():
    """Расчёт и отправка команды поворота"""
    time_ms = int(angle / omega * 200)
    if direction:
        create_json_command("right", time_ms)
        print(f"Отправка команды поворота: вправо на {time_ms} мс")
    else:
        create_json_command("left", time_ms)
        print(f"Отправка команды поворота: влево на {time_ms} мс")
    
    send_command_to_server()


def calculate_and_send_distance_command():
    """Расчёт и отправка команды движения вперёд"""
    time_ms = int(distance / velocity * 200)
    create_json_command("forward", time_ms)
    print(f"Отправка команды движения: вперёд на {time_ms} мс")
    send_command_to_server()


def send_command_to_server():
    """Отправка команды на сервер робота через HTTP POST"""
    try:
        with open("message.json", "r") as f:
            data = json.load(f)
        
        response = requests.post(SERVER_URL, json=data, timeout=5)
        
        if response.status_code == 200:
            print(f"Команда отправлена успешно: {data}")
        else:
            print(f"Ошибка: {response.status_code} - {response.text}")
    
    except Exception as e:
        print(f"Ошибка подключения: {e}")


def main():
    """Основной цикл захвата видео и обработки QR кодов"""
    global robotA_pos, robotB_pos, coffee_pos
    
    # Попытка открыть камеру
    cap = cv2.VideoCapture(0)  # Попробуйте 0, 1, 2 если не работает
    
    if not cap.isOpened():
        print("Ошибка: Не удалось открыть камеру")
        print("Попробуйте указать другой номер камеры (0, 1, 2)")
        return -1
    
    print("Камера открыта. Начинаем поиск QR кодов...")
    print("Нажмите 'q' или ESC для выхода")
    
    cv2.namedWindow("QR Scanner", cv2.WINDOW_AUTOSIZE)
    
    frame_count = 0
    command_send_interval = 0
    
    while True:
        ret, frame = cap.read()
        if not ret:
            print("Ошибка чтения кадра")
            break
        
        # Обнаружение QR кодов в каждом кадре
        detect_qr_codes(frame)
        
        # Рисование информации на кадре
        draw_info_on_frame(frame)
        
        # Вывод статуса на экран
        status_text = "Поиск QR кодов..."
        if robotA_pos and robotB_pos and coffee_pos:
            status_text = "Все маркеры найдены!"
        
        cv2.putText(frame, status_text, (10, frame.shape[0] - 10),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 0), 2)
        
        cv2.imshow("QR Scanner", frame)
        
        # Обработка клавиатуры
        key = cv2.waitKey(1) & 0xFF
        if key == ord('q') or key == 27:  # 'q' или ESC
            break
        
        frame_count += 1
        command_send_interval += 1
        
        # Отправка команд каждые 30 кадров при наличии всех маркеров
        if command_send_interval > 30:
            if robotA_pos and robotB_pos and coffee_pos:
                if calculate_robot_parameters():
                    if angle > min_angle:
                        print(f"Поворот необходим. Угол: {angle:.2f}°")
                        calculate_and_send_direction_command()
                        # Ждём выполнения команды перед следующей
                        time_ms = int(angle / omega * 200)
                        time.sleep(time_ms / 1000.0 + 0.2)
                        # Сбрасываем позиции для повторного обнаружения
                        robotA_pos = None
                        robotB_pos = None
                    elif distance > min_dist:
                        print(f"Движение необходимо. Расстояние: {distance:.2f}")
                        calculate_and_send_distance_command()
                        time_ms = int(distance / velocity * 200)
                        time.sleep(time_ms / 1000.0 + 0.2)
                        robotA_pos = None
                        robotB_pos = None
                    else:
                        print("Цель достигнута!")
            
            command_send_interval = 0
    
    cap.release()
    cv2.destroyAllWindows()
    return 0


if __name__ == "__main__":
    main()