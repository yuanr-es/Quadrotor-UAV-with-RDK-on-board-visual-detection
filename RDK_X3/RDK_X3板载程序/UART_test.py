import signal
import time
import serial

signal.signal(signal.SIGINT, signal.SIG_DFL)

UART_DEV = "/dev/ttyS3"  # 串口设备
BAUD_RATE = 115200  # 波特率

ser = serial.Serial(UART_DEV, BAUD_RATE, timeout=0.01)


def send_data(data):
    ser.write(data)

def recv_data():
    buffer = ser.read(1024)
    if len(buffer) < 5:
        return None
    for i in range(len(buffer) - 4):
        if buffer[i] == 0xAA and buffer[i + 1] == 0x55:
            # 3. 找帧尾 0xFF 0xFF
            for j in range(i + 2, len(buffer) - 1):
                if buffer[j] == 0xFF and buffer[j + 1] == 0xFF:
                    # 4. 提取中间有效数据
                    data = buffer[i + 2: j]
                    return data
    return None


def make_packet(data_bytes):
    header = bytes([0xAA, 0x55])
    tail = bytes([0xFF, 0xFF])
    return header + data_bytes + tail


def Test():
    test_data = "1234"
    data_bytes = test_data.encode('utf-8')
    send_buff = make_packet(data_bytes)
    send_data(send_buff)
    received_data = recv_data()

    data_decimal = list(received_data)
    print("Receive: " + str(data_decimal))

def close():
    ser.close()
