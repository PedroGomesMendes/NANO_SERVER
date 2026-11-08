import socket
import threading
import numpy as np
import time


from PyQt6.QtWidgets import (
    QApplication,
    QLabel,
    QWidget,
    QVBoxLayout,
    QHBoxLayout,
    QSlider,
    QComboBox,
    QFormLayout,
)
from PyQt6.QtGui import QImage, QPixmap
from PyQt6.QtCore import pyqtSignal, QObject, QTimer, Qt

IMG_W = 328
IMG_H = 320
RAW12_BYTES_PER_LINE = 492
UDP_PACKET_SIZE = RAW12_BYTES_PER_LINE + 2
UDP_TX_IP = "192.168.4.1"
UDP_TX_PORT = 5002




def update_preview(img: np.ndarray) -> None:
    qimg = QImage(
        img.data,
        IMG_W,
        IMG_H,
        IMG_W * 2,
        QImage.Format.Format_Grayscale16,
    )
    image_label.setPixmap(QPixmap.fromImage(qimg))
    fps()


def _send_udp_register_bytes(self, frame0: int, frame1: int) -> None:
    packet = bytes([0x00]) + frame0.to_bytes(3, byteorder="big") + bytes([0x01]) + frame1.to_bytes(3, byteorder="big")
    try:
        tx_sock.sendto(packet, (UDP_TX_IP, UDP_TX_PORT))
    except Exception as exc:
        print(f"Erro ao enviar UDP de registos: {exc}")



def send_command():
    reg0_rows_in_reset = reg0_rows.value()
    reg0_vrst_pix = int(reg0_vrst.currentText())
    reg0_ramp_gain = int(reg0_gain.currentText())
    reg0_offset_ramp = int(reg0_offset.currentText())
    reg0_output_curr = int(reg0_curr.currentText())

    reg1_rows_delay = reg1_rows.value()
    reg1_bias_curr_increase = int(reg1_bias.currentText())
    reg1_cds_gain = int(reg1_cds.currentText())
    reg1_output_mode = int(reg1_output.currentText())
    reg1_mclk_mode = int(reg1_mclk.currentText())
    reg1_vref_value = int(reg1_vref.currentText())
    reg1_cvc_curr = int(reg1_cvc.currentText())
    reg1_idle_mode = int(reg1_idle.currentText())
    reg1_high_speed = int(reg1_high.currentText())

    cabecalho = bytearray([reg0_rows_in_reset, 127,reg0_ramp_gain, 0, 0, 0, 0, 0, 0, 0, 0, 0])
    tx_sock.sendto(cabecalho, (UDP_TX_IP, UDP_TX_PORT))


def unpack_raw12_to_uint16(raw_line: bytes, width: int) -> np.ndarray:
    pixels = np.zeros(width, dtype=np.uint16)
    j = 0

    for i in range(0, width, 2):
        b0 = raw_line[j]
        b1 = raw_line[j + 1]
        b2 = raw_line[j + 2]

        p0 = ((b0 << 3) & 0x3F8) | (b1 >> 5)
        p1 = ((b1 << 7) & 0x380) | (b2 >> 1)

        pixels[i] = p0 << 6
        pixels[i + 1] = p1 << 6
        j += 3

    return pixels


class ImageReceiver(QObject):
    new_frame = pyqtSignal(np.ndarray)
    prev_line_index = 0
    def __init__(self, port=5001):
        super().__init__()
        self.port = port
        self.running = True
        self.thread = threading.Thread(target=self.run, daemon=True)
        self.thread.start()

    def run(self):
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.bind(("0.0.0.0", self.port))
        print(f"Servidor UDP escutando na porta {self.port}")
        img = np.zeros((IMG_H, IMG_W), dtype=np.uint16)
        while self.running:

            data, _ = sock.recvfrom(494)
            line_index = (data[493] << 8) | data[492]
            if (line_index - self.prev_line_index > 1):
                print(f"Perda de linha: {self.prev_line_index} -> {line_index}")
                self.prev_line_index = line_index

            img[line_index] = unpack_raw12_to_uint16(data[:492], IMG_W)
            if line_index == 319:
                update_preview(img.copy())




def fps():
    global old_time
    actual_time = time.time()
    print (1/(actual_time - old_time))
    old_time   =   actual_time

if __name__ == "__main__":
    import sys

    old_time = time.time()


    app = QApplication(sys.argv)


    image_label = QLabel()
    image_label.setMinimumSize(IMG_W, IMG_H)
    image_label.setStyleSheet("border: 20px solid #555; background: black;")
    image_label.setAlignment(Qt.AlignmentFlag.AlignCenter)

    fps_label = QLabel()
    fps_label.setText("0 fps")

    # ============================================================
    # REG0
    # ============================================================

    reg0_rows = QSlider(Qt.Orientation.Horizontal)
    reg0_rows.setRange(0, 255)
    reg0_rows.setValue(2)

    reg0_vrst = QComboBox()
    reg0_vrst.addItems(["0", "1", "2", "3"])
    reg0_vrst.setCurrentIndex(2)

    # ramp_gain
    reg0_gain = QComboBox()
    reg0_gain.addItems(["0", "1", "2", "3"])
    reg0_gain.setCurrentIndex(0)

    reg0_offset = QComboBox()
    reg0_offset.addItems(["0", "1", "2", "3"])
    reg0_offset.setCurrentIndex(3)

    reg0_curr = QComboBox()
    reg0_curr.addItems(["0", "1", "2", "3"])
    reg0_curr.setCurrentIndex(3)


    # ============================================================
    # REG1
    # ============================================================

    reg1_rows = QSlider(Qt.Orientation.Horizontal)
    reg1_rows.setRange(0, 31)
    reg1_rows.setValue(0)

    reg1_bias = QComboBox()
    reg1_bias.addItems(["0", "1"])
    reg1_bias.setCurrentIndex(0)

    reg1_cds = QComboBox()
    reg1_cds.addItems(["0", "1",])
    reg1_cds.setCurrentIndex(0)

    reg1_output = QComboBox()
    reg1_output.addItems(["0", "1"])
    reg1_output.setCurrentIndex(0)

    reg1_mclk = QComboBox()
    reg1_mclk.addItems(["0", "1", "2", "3"])
    reg1_mclk.setCurrentIndex(2)

    reg1_vref = QComboBox()
    reg1_vref.addItems(["0", "1", "2", "3"])
    reg1_vref.setCurrentIndex(2)

    reg1_cvc = QComboBox()
    reg1_cvc.addItems(["0", "1", "2", "3"])
    reg1_cvc.setCurrentIndex(1)

    reg1_idle = QComboBox()
    reg1_idle.addItems(["0", "1"])
    reg1_idle.setCurrentIndex(0)

    reg1_high = QComboBox()
    reg1_high.addItems(["0", "1"])
    reg1_high.setCurrentIndex(1)   


    form = QFormLayout()

    # REG0
    form.addRow("Reg0 rows_in_reset", reg0_rows)
    form.addRow("Reg0 vrst_pix", reg0_vrst)
    form.addRow("Reg0 ramp_gain", reg0_gain)
    form.addRow("Reg0 offset_ramp", reg0_offset)
    form.addRow("Reg0 output_curr", reg0_curr)

    # REG1
    form.addRow("Reg1 rows_delay", reg1_rows)
    form.addRow("Reg1 bias_curr_increase", reg1_bias)
    form.addRow("Reg1 cds_gain", reg1_cds)
    form.addRow("Reg1 output_mode", reg1_output)
    form.addRow("Reg1 mclk_mode", reg1_mclk)
    form.addRow("Reg1 vref", reg1_vref)
    form.addRow("Reg1 cvc_curr", reg1_cvc)
    form.addRow("Reg1 idle_mode", reg1_idle)
    form.addRow("Reg1 high_speed", reg1_high)

    preview_layout = QVBoxLayout()
    preview_layout.addWidget(image_label)

    content_layout = QHBoxLayout()
    content_layout.addLayout(preview_layout)
    content_layout.addLayout(form)

    main_layout = QVBoxLayout()
    main_layout.addLayout(content_layout)

    receiver = ImageReceiver(port=5001)

    window = QWidget()
    window.setWindowTitle("NANO SERVER VIEWER")
    window.setLayout(main_layout)
    window.resize(850, 400)
    window.show()


    tx_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    timer = QTimer()
    timer.timeout.connect(send_command)
    timer.start(1000)

    sys.exit(app.exec())
